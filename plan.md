# TODO-05A1-D1：Execution Victim Presentation Timing And Launch Fallback v1

## 阶段状态与基线

- 状态：已实施、用户 PIE/Automation 验证通过、Main Fresh Review 通过；等待明确提交批准；本文件保留当前阶段收口详情。
- 日期：2026-09-03。
- 仓库：`E:\GameDevelop\PolyQuest`。
- 基线：`main @ 5922c1f01844d4f26c56348359dbc41d5cadbe43`。
- 前置阶段：`TODO-05A1-A/B/C` 已完成成对锁定、授权命中/DeathPending、Release 结果分流；C 的详细收口已在 `ROADMAP-archive.md` 归档。
- 工作树：存在用户-owned `Content/**`、Config、`AGENTS.md` 及其他 WIP；本阶段只处理下列批准路径，绝不回滚或吞并这些变动。

## 实施收口记录

- 实际实施严格落在 11 个批准路径：`Config/Tags/PolyQuestGameplayTags.ini`；`AnimNotify_PlayerExecutionVictimStart` 的 Public/Private 文件；Player Front/Backstab Execution Ability 的 Public/Private 文件；`EnemyVictimExecutionAbility` 的 Public/Private 文件；`ExecutionVictimPresentationAutomationTests.cpp`；以及 `ExecutionReleaseOutcomesAutomationTests.cpp` 的兼容性修订。未修改未批准的 `ExecutionLockContext.*`、`EnemyCharacter.*`、`MeleeHitResolver.*`、`PlayerCharacter.*`、Launch Ability、旧 Hit Notify、Build.cs、Blueprint 或二进制资产。
- D1 已落地 `VictimStart -> Hit -> Release` 三语义点：握手只建立锁定和监听，受害者 Montage 由经过 Player/Context/Token/Actor/ASC/动画身份校验的 `VictimStart` 同步转发后才启动；Hit 仍是唯一伤害点，Release 仍是唯一结果提交点；无表现、缺失通知、播放失败和提前结束均不改变 gameplay 收尾。
- Gemini 报告：手动 Visual Studio `PolyQuestEditor (Development Editor)` 编译、Rider 静态检查、`git diff --check`、9 场景 `PolyQuest.Combat.ExecutionVictimPresentation` 及既有回归套件均通过，并完成相应 Editor readback。用户在本轮明确确认 PIE 与 Automation 通过；这些证据分别只覆盖其报告或确认的门禁，不互相扩展。
- Main 已完成一轮独立、diff-first、缺陷优先 `ue-strict-review`：未发现当前批准范围内可证实的 P0/P1/P2 缺陷。`code-review-graph` 索引早于本阶段审查基线，仅作为 stale-coverage 提示；结论以批准 diff、定向源码核对和已有验证证据为准。
- 作者化 Front/Backstab Montage、Victim Montage、Notify 放置、Launch/AnimBP/Blueprint 关系仍属于用户 `Content/**` WIP，不纳入本阶段提交，也不宣称干净检出即可复现完整 authored fixture。Gemini 报告的编译/readback与用户确认的 PIE/Automation需保持证据来源区分。
- 阶段候选提交仅包含上述批准实现路径及 Main 收口文档 `plan.md`、`ROADMAP.md`、`ROADMAP-archive.md`、`ARCHITECTURE.md`、`README.md`；不包含 `AGENTS.md`、全部 `Content/**`、其他 Config WIP 或生成目录。必须在文档检查后另行取得用户明确提交批准。

## 阶段目标与成功标准

本阶段只解决一个运行时问题：受害者配合 Montage 必须由主角 Montage 的明确接触通知启动，而不是在握手瞬间提前播放；缺少表现或表现失败时，既有 Hit/Release/Launch/Death 结果仍须安全收尾。

稳定不变量：

- Player Montage 是唯一 gameplay 时钟。
- `VictimStart` 只启动受害者表现；`Hit` 是唯一伤害结算点；`Release` 是唯一解除锁定和提交结果点。
- Victim Montage 完成、BlendOut、播放失败或缺失通知都不能结束执行会话、改变伤害或提前 Release。
- 保留 `bLaunchNonLethalOnRelease` 作为唯一特殊物理结果开关；Small/Big/倒地等差异由 Victim Montage 作者化，不增加结果枚举。
- 正常非致死 Release 仍先恢复受害者控制状态，再按既有 Launch 或站立降级路径收尾。

## 工具路线与责任

- Outer：`ue-stage-workflow`。
- Primary：`ue5-cpp-gameplay`。
- Support：none。
- Route reason：这是既有 GAS 成对执行会话的窄表现时序切片，不引入新的动画同步框架、伤害路径或通用 Ability。
- Execution route：`manual/out-of-band Gemini`。
- Contract owner：Main/Codex。
- Implementation writer：Gemini，仅能修改批准路径中的冻结实现。
- Main 负责架构、范围、验证解释、`ue-strict-review`、文档、暂存和提交；用户负责 Visual Studio 编译、Editor readback、Automation、PIE/视觉验证和最终提交批准。
- Gemini 不得修改 `plan.md`、`ROADMAP.md`、`ROADMAP-archive.md`、`ARCHITECTURE.md` 或 `README.md`，不得暂存或提交。

## 批准变更路径

1. `Config/Tags/PolyQuestGameplayTags.ini`
2. `Source/PolyQuest/Public/Animation/Combat/AnimNotify_PlayerExecutionVictimStart.h`
3. `Source/PolyQuest/Private/Animation/Combat/AnimNotify_PlayerExecutionVictimStart.cpp`
4. `Source/PolyQuest/Public/AbilitySystem/Abilities/PlayerFrontExecutionAbility.h`
5. `Source/PolyQuest/Private/AbilitySystem/Abilities/PlayerFrontExecutionAbility.cpp`
6. `Source/PolyQuest/Public/AbilitySystem/Abilities/PlayerBackstabExecutionAbility.h`
7. `Source/PolyQuest/Private/AbilitySystem/Abilities/PlayerBackstabExecutionAbility.cpp`
8. `Source/PolyQuest/Public/AbilitySystem/Abilities/EnemyVictimExecutionAbility.h`
9. `Source/PolyQuest/Private/AbilitySystem/Abilities/EnemyVictimExecutionAbility.cpp`
10. `Source/PolyQuest/Private/Tests/ExecutionVictimPresentationAutomationTests.cpp`
11. `Source/PolyQuest/Private/Tests/ExecutionReleaseOutcomesAutomationTests.cpp`（仅测试夹具兼容性修订）

`ExecutionReleaseOutcomesAutomationTests.cpp` 的允许修订仅用于：结果测试默认不配置 Victim Montage，或在专门的表现用例中显式发送 VictimStart，避免新契约产生无意义 Warning；不得改变 C 阶段的结果断言。

未批准路径：`ExecutionLockContext.*`、`EnemyCharacter.*`、`MeleeHitResolver.*`、`PlayerCharacter.*`、`EnemyLaunchReactionAbility.*`、旧 Front/Backstab Hit Notify、Build.cs、Blueprint、地图和所有 `.uasset/.umap`。如确实需要其中任一路径，Gemini 必须停止并返回证据，由 Main 重新定范围。

## 冻结事件与 Payload 契约

### VictimStart

新增唯一 Tag：`Event.Action.Execution.Request.VictimStart`。Notify 与 Player 转发使用同一个 Tag，不增加对称的第二个 Tag。

`UAnimNotify_PlayerExecutionVictimStart`：

- 只向 Notify 所属 Player 的 ASC 发送事件。
- Payload 固定为 `Instigator=Owner`、`Target=Owner`、`OptionalObject=Animation`。
- 不查找 Victim、不直接激活目标 Ability、不修改 Health/Poise/Movement。

Player Front/Backstab：

- 各自新增 `VictimStart` WaitGameplayEvent Task、代际回调和一次性消费状态。
- Task 必须在 Player Montage Task 之前创建并 `ReadyForActivation()`；每次 Ready 后立即检查 Ability、Context、Token、Task UObject 和 `IsActive()`，同步重入时直接统一 `EndAbility()`。
- Player 端验证当前 Ability、activation token、Context、Player/ReservedTarget/双方 ASC、事件 Actor 以及现有 Front/Backstab Montage/Sequence identity helper。
- 验证通过后同步向 Victim ASC 转发同一个 Tag：`Instigator=Player`、`Target=Enemy`、`OptionalObject=ActiveExecutionContext`、`OptionalObject2=原始 Animation`。
- 重复、迟到、旧 Context、错误 Actor/ASC/动画和已 Release 会话全部 fail-closed；每个会话最多成功转发一次。

Victim Ability：

- `AbilityTriggers` 只能保留 Front/Backstab Request；严禁加入 `VictimStart`，避免重新激活 Ability。
- 握手时只建立锁定、Release 监听和 VictimStart 监听，不立即播放 Front/Backstab Victim Montage。
- `OptionalObject` 必须是当前活动 Context；Actor、Source/Target 和 ASC 必须匹配当前会话。
- `OptionalObject2` 只做防御性检查：非空且为 `UAnimMontage` 或 `UAnimSequenceBase`。Victim 不反向比较 Player Montage 资产；Player 负责精确动画归属认证。
- 有效 VictimStart 每会话只消费一次；无 Victim Montage 是合法无表现降级，不创建播放 Task。

`VictimStart` 与 `Hit` 可同帧或任意先后。`Release` 仍必须等待唯一一次合法 Hit 解决；既有 Hit/Release latch、Exactly-Once 和 Context 生命周期不改变。

## Victim Montage 生命周期与 Launch

- 使用 `UAbilityTask_PlayMontageAndWait` 时将 `bStopWhenAbilityEnds=false`，由 Victim Ability 自己拥有停止顺序。
- 自然 `OnCompleted`/`OnBlendOut` 只解绑本 Ability 委托、结束自身 Task、清空表现引用，不再次调用 `Montage_Stop`，保留引擎自然融合。
- `OnInterrupted`、`OnCancelled`、正式 Release 和异常 teardown 先解绑委托；仅当 Montage 仍 active 且 `!Montage_GetIsStopped()` 时执行一次 `Montage_Stop(0.2f, Montage)`，随后结束 Task 并清空引用。任何 GAS Task `OnDestroy()` 清理不得二次零时长覆盖。
- `ReadyForActivation()` 期间若同步进入 End/Cancel，不得恢复旧 Task、旧 Context 或旧 Montage identity。
- 只有“已配置 Victim Montage、正常正式 Release、Hit 已为 NonLethal/DeathPending、从未收到有效 VictimStart”时输出一次缺失通知 Warning。无 Montage、取消、中断、无命中、销毁、错误请求和异常 teardown 不报警。
- 非致死正常 Release 继续在 `Super::EndAbility()` 前恢复 AI/Movement/Poise，之后发送带 `Instigator=Player`、`Target=Enemy` 的既有 Launch 事件；Launch 不可激活时不重试，保留站立恢复降级。取消、无命中和致死不得发送 Launch。

## 实施顺序

1. 注册 Tag，新增固定 Payload 的 VictimStart Notify。
2. 对称扩展 Front/Backstab 的 callback context、监听 Task、校验、同步转发、一次性状态和测试 seam。
3. 修改 Victim Ability 的延迟播放、VictimStart 消费、自然/异常停止和缺失通知诊断；保持现有 Release/Death/Launch 所有权。
4. 新增表现专项 Automation，并调整 C 阶段结果测试夹具以符合新 Warning 契约。
5. 对批准 C++ 路径执行 Rider error-level 检查、`git diff --check` 和有界静态复核；不运行 UBT/Build.bat/Rider build。

## 自动化测试矩阵

新增 `PolyQuest.Combat.ExecutionVictimPresentation`，覆盖：

- Tag/Notify Payload、VictimStart 不在 AbilityTriggers。
- Front/Backstab 有效同步转发、同帧/错序 Hit、重复/迟到/错误 Context/Actor/ASC/动画拒绝。
- 有 Victim Montage 延迟到 VictimStart 才启动；无 Montage 和缺失 VictimStart 安全降级。
- 自然完成/BlendOut 不结束会话；异常停止最多一次 BlendOut；Task Ready 同步失效不泄漏。
- 正常 NonLethal Launch、关闭 Launch 的站立结果，以及取消、无命中、致死、Malformed Release 均不 Launch。

回归运行：`PolyQuest.Combat.ExecutionLockIn`、`PolyQuest.Combat.FrontExecution`、`PolyQuest.Combat.Backstab`、`PolyQuest.Combat.ExecutionReleaseOutcomes`。测试证据只代表 Automation，不替代编译或 PIE。

## 用户门禁与收口证据

- Gemini 报告手动编译、Editor readback、专项 `PolyQuest.Combat.ExecutionVictimPresentation`（9 个场景）及回归 Automation 均通过；本轮用户明确确认 PIE 与 Automation 通过。
- Scene01 PIE 的实际确认覆盖握手后不提前播放、VictimStart 驱动表现、Hit/Release 收尾、非致死 Launch/站立降级及取消清理；不把该确认扩展为未运行的全量或网络验证。
- Main Fresh Review 已完成；静态检查、图工具和 Automation 不替代编译、Editor readback 或 PIE 证据，报告中各来源保持分列。
- authored 资产仍由用户维护并排除在提交之外；若后续需要干净 authored baseline 或 packaging 声明，须另行提供可追溯的资产基线和读回记录。

## 非目标、文档与提交边界

- 不做统一 Front/Backstab Hit Notify（`REC-05A1-03`）、武器专属 Montage（`TODO-05A1-D2`）、震屏/出血反馈（`TODO-05A1-E`）、Contextual Animation（`REC-05A1-02`）、Small/Big 枚举、Boss 投技或通用 Ability 基类。
- Gemini 只交付实现证据和剩余风险；Main 在验证/Review 通过后维护 `ROADMAP.md`、必要的 `ARCHITECTURE.md` 和历史归档。本阶段 D1 已完成，下一开放切片为 `TODO-05A1-D2`。
- 最终提交只允许包含批准源码/Config/test 路径及 Main 收口文档；排除全部用户 `Content/**`、`AGENTS.md`、其他 Config WIP 和生成目录。提交前必须显式获得用户批准，按路径暂存并检查 staged diff。
