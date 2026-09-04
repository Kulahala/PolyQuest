# TODO-05A1-D2C：StanceBreak Backstab Compatibility v1（实施计划）

## 阶段状态与基线

- 状态：D2C 实现已完成；用户已确认相关 Focused Automation 与 Scene01 PIE 通过；Main 最终 Fresh Review 未发现批准范围内 P0-P2 blocker。手动编译与 Editor readback 仅按执行者报告记录，未形成 Main 独立收据。
- 日期：2026-09-04。
- 仓库：E:\GameDevelop\PolyQuest；分支：main；父 HEAD：3254c10（D2B“武器专属处决 Montage 选择”已提交），本阶段提交前相对 origin/main ahead 6。
- 工作树：约 321 条用户-owned Content、Config、AGENTS.md 和其他 WIP；这些变更不是本阶段批准集合，必须原样保留。
- Archive preflight：PASS。D2B 收口已存在于 ROADMAP-archive.md 的日期化条目；替换本文件不会丢失 D2B 历史证据。
- 本阶段触发条件已满足且 D2C 已收口；TODO-05A1-E、D2D 不纳入本阶段，也不因本计划被标记为完成。

## 目标与成功标准

本阶段只回答一个运行时问题：目标处于由单一 active UEnemyStanceBreakAbility 拥有的 Stunned 状态时，Backstab 是否能安全接管 StanceBreak 的恢复责任，同时仍使用 Backstab 的受害者 Montage。

成功标准：

1. 普通非 Stunned 目标继续允许 Backstab；普通/外部 Stunned 目标继续拒绝。
2. 仅当目标有一个 active StanceBreak 且 State.Status.Stunned 总 contribution 恰好为一份时允许 Backstab。
3. StanceBreak 加额外 Stunned contribution、多个 active StanceBreak、StanceBreak 与 Stunned 计数不一致时 fail-closed。
4. Victim 在取消 StanceBreak 前捕获 handoff；VictimLocked 继续是 StanceBreak 跳过自身移动/Poise 恢复的既有闸门。
5. handoff 只表示恢复所有权；Backstab 请求始终选择 BackstabExecutionVictimMontage，Front 请求始终选择 Front Montage。
6. 存活目标在 Release、取消或中断后只由 Victim 恢复移动、Poise 与 AI 锁一次；死亡/销毁路径不新增恢复。
7. D2A Snap、D2B 激活 Montage 快照、统一 Hit Notify、唯一 FMeleeHitResolver、VictimStart -> Hit -> Release、Exactly-Once 和 Front 优先级保持不变。

## 证据基线与状态合同

- Player 目标校验当前直接拒绝 State.Status.Stunned：PlayerBackstabExecutionAbility.cpp:321。
- Victim 当前在 Backstab 分支拒绝 active StanceBreak 和额外 Stunned contribution：EnemyVictimExecutionAbility.cpp:198。
- Victim 当前用 bHandoffFromStanceBreak 同时决定方向 Montage 与 Poise 恢复：EnemyVictimExecutionAbility.cpp:265。
- Victim 的 Backstab Montage 选择位于 EnemyVictimExecutionAbility.cpp:332；Poise 恢复位于 EnemyVictimExecutionAbility.cpp:694。
- StanceBreak 在 VictimLocked 存在时跳过自身恢复：EnemyStanceBreakAbility.cpp:271。
- UE 5.8 GAS PreActivate 会在进入 ActivateAbility 前添加 ActivationOwnedTags；Victim 的 ActivationOwnedTags 明确包含一份 State.Status.Stunned。因此 Victim 校验必须先扣除自身一份，再应用同一状态表。

定义 S = active UEnemyStanceBreakAbility 数量，C = Victim 激活前 State.Status.Stunned 总 contribution 数量：

| S | C | 结果 |
|---:|---:|---|
| 0 | 0 | 普通 Backstab，handoff=false |
| 1 | 1 | 允许 Backstab，handoff=true |
| 0 | >=1 | 拒绝 |
| 1 | !=1 | 拒绝 |
| >=2 | 任意 | 拒绝 |

## 冻结实现合同

### 同构私有判定

- 在 PlayerBackstabExecutionAbility.cpp 和 EnemyVictimExecutionAbility.cpp 的匿名命名空间中各实现同名、同逻辑的纯函数：

~~~cpp
bool EvaluateStanceBreakCompatibility(
    int32 ActiveStanceBreakCount,
    int32 PreActivationStunnedContribution,
    bool& bOutHandoff);
~~~

## Main Review Repair Loop（2026-09-04）

- Review finding：新增 Backstab 正例曾通过手动设置 StanceBreak spec 的 ActiveCount 和 loose Stunned tag 构造，不能证明真实 StanceBreak-owned tag 生命周期。
- Main/Codex 依据 AGENTS.md Main narrow-fix 例外直接修复，仅修改已批准的 E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\Tests\BackstabExecutionAutomationTests.cpp；生产状态判定、公开 API、所有权和资产未改动。
- 修复内容：新增 ActivateTestStanceBreak()，使用真实 StanceBreak 激活模式；Section 5.2b 与 10.2b 的 S=1,C=1 正例改用该夹具；S=1,C=0、S=1,C=2、S=2,C=1 继续作为明确标注的合成 fail-closed 计数边界。
- Main 已执行该文件和本计划的 git diff --check；随后用户确认修复后的 focused Automation 与 Scene01 PIE 通过。Main 未自行重跑用户-owned 编译、Editor readback、Automation 或 PIE。
- 当前剩余验证说明：真实 GAS owned-tag 归属由实际 StanceBreak 正例覆盖；按计数构造的异常行不作为真实 Ability 生命周期证明，也不扩大为新的生产测试接口。

- 函数先清零输出，负数拒绝，只接受上表两种成功组合；不引入共享公共 Helper、Runtime Service 或新的 reflected API。
- Player 传入目标 ASC 的原始 Stunned count。
- Victim 先确认自身 Stunned ActivationOwnedTags 合同和总数至少一，再使用 TotalStunnedCount - 1 作为 C；自身 contribution 缺失或计数下溢直接拒绝。
- active StanceBreak 数量通过目标 ASC 的 GetActivatableAbilities() 统计 Spec.Ability->IsA<UEnemyStanceBreakAbility>() && Spec.IsActive()。

### Player 侧

- 仅在 ValidateTargetPrerequisites() 放宽目标 Stunned 判断，CanActivateAbility() 与 ActivateAbility() 继续共用该校验。
- Player 自身 ActivationBlockedTags 中的 State.Status.Stunned 不变。
- 失败必须发生在 Commit/握手/Snap 之前；不改变 CommitAbility()、D2A Snap 或 D2B Montage resolver。

### Victim 侧

- 将私有 ValidateExecutionRequest() 增加 bool& bOutHandoffFromStanceBreak 输出，并在函数入口初始化为 false。
- Front 分支保留现有 Poise-zero 和 active-StanceBreak 要求，并直接输出 handoff=true；不对 Front 引入新的计数门禁。
- Backstab 分支使用同构判定；校验成功后在 CancelAbilities() 前写入 bHandoffFromStanceBreak。
- PendingVictimMontage 只由请求 Event Tag 决定，不能再读取 handoff 标志决定方向。
- EndAbility() 继续是存活目标恢复的唯一所有者，并保持现有幂等清理顺序；不得让 StanceBreak 和 Victim 同时恢复 Poise/移动。
- 现有 HasTestHandoffFromStanceBreak() 已存在于测试宏区，直接复用，不重复添加测试暴露接口。

## 范围、所有权与路线

- Outer：ue-stage-workflow
- Primary：ue5-cpp-gameplay
- Support：ue5-debug-validation
- Execution route：manual/out-of-band Gemini
- Contract owner：Main/Codex；implementation writer：Gemini；Codex 子代理委派数：0。
- 允许修改的唯一 Source/Test 路径：
  - E:\GameDevelop\PolyQuest\Source\PolyQuest\Public\AbilitySystem\Abilities\PlayerBackstabExecutionAbility.h
  - E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\AbilitySystem\Abilities\PlayerBackstabExecutionAbility.cpp
  - E:\GameDevelop\PolyQuest\Source\PolyQuest\Public\AbilitySystem\Abilities\EnemyVictimExecutionAbility.h
  - E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\AbilitySystem\Abilities\EnemyVictimExecutionAbility.cpp
  - E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\Tests\BackstabExecutionAutomationTests.cpp
  - E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\Tests\ExecutionVictimPresentationAutomationTests.cpp
- EnemyStanceBreakAbility.*、Front Ability、FrontExecutionAutomationTests.cpp、ExecutionLockInAutomationTests.cpp 默认只读/回归运行；需要修改时必须停止并返回 Main 决策。
- 明确排除：Gameplay Tags、Input、Config、Build.cs、PlayerCharacter.*、装备组件、ExecutionLockContext.*、Blueprint、AnimBP、Montage、地图、全部 Content、D2A/D2B/E/D2D 逻辑和任何通用处决框架。

## 执行顺序与停止条件

1. 用户先在 Editor 读取真实 StanceBreak Montage、Backstab Victim Montage、骨架、AnimBP、Slot 和统一 Hit Notify 组合；不自动保存、不迁移、不创建新跪地/倒地资产。组合不兼容时停止。
2. 计划接受后 Main 发送下方 Gemini handoff；Gemini 只修改冻结路径中的 Source/Test。
3. Gemini 完成实现和首次切片严格自审；按项目规则优先派发一个无历史污染的只读自审代理，后续修复由 Gemini 单兵复核。
4. Main 读取最终 diff，运行静态门禁、git diff --check 和 Rider C++ inspection；不调用 UBT 或 Editor 编译。
5. 用户手动编译 PolyQuestEditor (Development Editor)，运行 Focused Automation、Editor readback 和 Scene01 PIE。
6. Main 运行一次限定范围 ue-strict-review；通过后再同步 Roadmap/阶段收口，等待用户显式 commit approval。

任何实现需要未列路径、公共 API、Tag/Input/Config、资产写入或改变 StanceBreak 所有权时立即停止；同一根因最多一次证据驱动修复和一次针对性复验。

## Automation 与用户验收

- BackstabExecutionAutomationTests 保留现有外部 Stunned 负例（S=0,C=1，当前 10.2），并新增 active StanceBreak 正例（S=1,C=1，建议 10.3）：Backstab 激活、Victim 接受、无 Primary fallback。
- 覆盖 StanceBreak 加额外 Stunned、StanceBreak/Tag 计数不一致和多个 active StanceBreak 的拒绝；若现有夹具无法构造某一行，不得新增生产接口，需报告并由 Main 决定 test-only fixture。
- ExecutionVictimPresentationAutomationTests 覆盖 Backstab handoff=true 但 Pending/Active Montage 为 Backstab Montage；Front handoff 与 Front Montage 保持原行为。
- 正常 Release、取消、Montage 中断后，存活目标 Poise 为 Max、移动为 MOVE_Walking、AI 锁解除，且恢复只执行一次；死亡/销毁路径不恢复。
- 保持 VictimStart -> Hit -> Release、唯一伤害、重复事件、目标销毁、UnPossess/Teardown 和已有 Front/Backstab 回归断言。
- 用户必须确认真实资产组合、Development Editor 编译、相关 Execution Automation、Scene01 PIE 的 Backstab-after-StanceBreak、Front 回归、命中、Release、清理和换装阻断。
- 静态检查、CodeGraph、Gemini 自审和 Automation 不替代编译、Editor readback 或 PIE 证据。

## 静态门禁、债务与提交边界

- Main 检查两份 helper 同构、Victim 自身 contribution 扣除、handoff 捕获早于 CancelAbilities()、请求方向与恢复语义分离，以及无第二伤害路径。
- 对批准 C++ 文件运行 Rider lint_files/get_file_problems（不可用则如实记录），执行 git diff --check；本次 code-review-graph 基于 `3254c10` 且与审查基线匹配，仅作影响导航。
- D2A/D2B 独立编译/readback、D2A-VERTICAL-SURFACE、Debt-REC-05A1-03-RET-Teardown 继续保留为非阻塞债务。
- 本阶段不提前修改 ROADMAP.md 或 ARCHITECTURE.md；通过用户门禁和 Main Review 后，由 Main 更新 D2C 状态、债务闭环和历史归档。
- 最终提交只允许冻结的 Source/Test 与经 Main 收口批准的文档；所有用户-owned WIP 原样排除，等待显式 commit approval。

## Gemini Handoff Prompt

~~~text
你是 PolyQuest 的 TODO-05A1-D2C Implementation Executor。

仓库：
E:\GameDevelop\PolyQuest

基线：
main @ 3254c10
当前活动计划：
E:\GameDevelop\PolyQuest\plan.md

路线：
Outer=ue-stage-workflow
Primary=ue5-cpp-gameplay
Support=ue5-debug-validation
Execution route=manual/out-of-band Gemini
Contract owner=Main/Codex
Implementation writer=Gemini

在开始写 Source 前必须等待 Main/user 提供真实 Editor readback：
- 当前 StanceBreak Montage
- Backstab Victim Montage
- 骨架、AnimBP、Slot
- 统一 Player Execution Hit Notify
本次不创建、迁移、保存或修改任何 .uasset/.umap；组合不兼容、资产路径不明确或需要新增资产时立即停止并回报。

只允许修改以下 6 个文件：
- E:\GameDevelop\PolyQuest\Source\PolyQuest\Public\AbilitySystem\Abilities\PlayerBackstabExecutionAbility.h
- E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\AbilitySystem\Abilities\PlayerBackstabExecutionAbility.cpp
- E:\GameDevelop\PolyQuest\Source\PolyQuest\Public\AbilitySystem\Abilities\EnemyVictimExecutionAbility.h
- E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\AbilitySystem\Abilities\EnemyVictimExecutionAbility.cpp
- E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\Tests\BackstabExecutionAutomationTests.cpp
- E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\Tests\ExecutionVictimPresentationAutomationTests.cpp

允许的函数/区域：
- Player：匿名命名空间状态 helper、ValidateTargetPrerequisites() 及其 CanActivateAbility()/ActivateAbility() 调用点。
- Victim：私有 ValidateExecutionRequest() 输出参数、ActivateAbility() 的 handoff 捕获和请求方向 Montage 选择、EndAbility() 的既有恢复分支。
- Tests：Backstab 的 Stunned/active-StanceBreak 矩阵与 Presentation 的 Montage/Poise/Movement/AI 生命周期断言。
- 现有 HasTestHandoffFromStanceBreak() 已存在，直接使用，不新增重复 accessor。

冻结状态合同：
S = active UEnemyStanceBreakAbility spec 数量；
C = Victim 激活前 State.Status.Stunned 总 contribution 数量。
只允许：
  S=0,C=0 -> 普通 Backstab，handoff=false
  S=1,C=1 -> 允许 Backstab，handoff=true
其他组合全部拒绝。

在 PlayerBackstabExecutionAbility.cpp 和 EnemyVictimExecutionAbility.cpp 的匿名命名空间中各实现同构纯函数：
bool EvaluateStanceBreakCompatibility(
    int32 ActiveStanceBreakCount,
    int32 PreActivationStunnedContribution,
    bool& bOutHandoff);
先清零输出并拒绝负数。Player 传入原始目标计数；Victim 在确认自身 ActivationOwnedTags 含恰好一份 Stunned 且总数至少一后，先减一再调用 helper。

Front 合同不变：
- Front 仍要求 Poise=0 且存在 active StanceBreak。
- Front 输出 handoff=true。
- 不给 Front 引入新的 Stunned 计数门禁，不修改 PlayerFrontExecutionAbility 或 FrontExecutionAutomationTests。

时序要求：
- Victim ValidateExecutionRequest() 得到 handoff 输出后，必须在 CharacterASC->CancelAbilities(...) 之前保存 bHandoffFromStanceBreak。
- VictimLocked 继续让 StanceBreak 跳过自身移动/Poise 恢复。
- PendingVictimMontage 必须按请求 Event Tag 选择：Front -> FrontExecutionVictimMontage，Backstab -> BackstabExecutionVictimMontage；handoff 不能参与方向选择。
- Victim EndAbility() 是存活目标恢复移动、Poise、AI 锁的唯一所有者，并保证 exactly-once。

绝对非目标：
- 不改 D2A Snap 几何、D2B Montage resolver/快照、统一 Hit Notify、FMeleeHitResolver、VictimStart/Hit/Release、Front 优先级或普通攻击。
- 不改 EnemyStanceBreakAbility.*、Gameplay Tags、Input、Config、Build.cs、装备组件、ExecutionLockContext、Blueprint、AnimBP、Montage、地图、Content 或任何未列文件。
- 不新增公共运行时 API、通用处决基类、第二条伤害路径或生产测试 seam。

必须更新/保留的测试：
- 保留现有外部 Stunned 负例 S=0,C=1。
- 新增 active StanceBreak 单贡献正例 S=1,C=1，断言 Backstab 激活且不回退 Primary。
- 覆盖 active StanceBreak + 额外 Stunned、状态计数不一致和可构造的多 active StanceBreak 负例。
- Presentation 测试断言 Backstab handoff 使用 Backstab Victim Montage，Release/取消/中断后 Poise=Max、Movement=Walking、AI unlock 且只恢复一次。
- 保持 VictimStart -> Hit -> Release、Exactly-Once、Front 回归和清理断言。

停止条件：
- 需要未列文件、资产保存、Tag/Input/Config、公共 API、StanceBreak 所有权改变或新的生命周期规则。
- 现有夹具无法构造所需状态且必须新增生产接口。
- 发现 PreActivate/ActivationOwnedTags、VictimLocked 或现有 Release/EndAbility 合同与计划冲突。
遇到任一条件只回报证据，不绕过范围。

交付前自审：
- 按 PolyQuest 规则，首次切片优先派发一个无历史污染的只读严格自审代理；后续修复由你单兵复核。
- 运行适用的静态检查和 git diff --check；不要运行 UBT、Build.bat、Editor 写入、打包或提交。
- 回交必须列出：改动路径、函数、状态表覆盖、静态检查结果、未运行的用户-owned 编译/Editor/Automation/PIE 门禁、自审 findings、剩余风险。
- 不得修改 plan.md、ROADMAP.md、README.md、ARCHITECTURE.md，不得 staging 或 commit。
~~~

## Main Closeout（2026-09-04）

- 阶段结果：D2C 已按冻结的 `S/C` 状态表落地；普通/外部 Stunned、额外 contribution、多个 active StanceBreak 和计数不一致均 fail-closed；单一 active `UEnemyStanceBreakAbility` 的单一 Stunned contribution 可进入 Backstab。
- 用户证据：用户确认 `PolyQuest.Combat.Backstab`、`PolyQuest.Combat.FrontExecution`、`PolyQuest.Combat.ExecutionVictimPresentation` 通过，并确认 Scene01 PIE 的 StanceBreak-after-Backstab、Backstab Victim Montage、Release/Cancel 状态恢复与 Front 回归通过。
- Main 静态/复核证据：批准路径 `git diff --check` 退出码为 0；code-review-graph 基线匹配；一次定向 CodeGraph 核对 handoff、Montage 方向和 StanceBreak/Victim 恢复所有权；最终 Fresh Review 无 P0/P1/P2 blocker。
- 执行者报告：Gemini 报告 Rider 零 Error、手动 `PolyQuestEditor` 编译和资产 readback；这些结果保留为执行者证据，不改写为 Main 独立收据。
- Main 修复：仅在 `BackstabExecutionAutomationTests.cpp` 增加真实 StanceBreak 激活正例夹具；异常计数行明确标注为合成 fail-closed 边界，不宣称覆盖所有真实 contribution 生命周期。
- 文档影响：本阶段同步 `ARCHITECTURE.md`、`ROADMAP.md` 与 `ROADMAP-archive.md`；`plan.md` 保留为当前最近阶段记录。
- 非阻塞债务：D2C 独立 Main/user 编译与执行资产 Editor readback 收据仍未归档；合成异常计数矩阵、D2A 垂直面限制、D2A/D2B compile/readback 与 `Debt-REC-05A1-03-RET-Teardown` 继续按各自关闭条件保留。
- 提交边界：仅包含本阶段五个 Source/Test 改动及四份 Main 文档；所有 `Content/**`、用户 Config/Blueprint/地图和其他 WIP 排除。下一执行切片为 `TODO-05A1-E`；D2D 仍为独立条件分支。
