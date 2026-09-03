# REC-05A1-03：Unified Execution Hit Notify v1

## 阶段状态与基线

- 状态：Native Contract Gate 已完成；Gemini 独立自审 0 缺陷，Main Fresh Review 通过，用户已确认编译、Editor readback、Automation 和 Scene01 PIE 门禁。Authored Migration / Legacy Retirement 仍是后续独立门。
- 日期：2026-09-04。
- 仓库：`E:\GameDevelop\PolyQuest`。
- 当前基线：`main @ 8f1357af39ec520c94f93e9b93c8ccf781170ff0`（TODO-05A1-D1 已提交；D2A 尚未实施）。
- 前置阶段：`TODO-05A1-A/B/C/D1` 已完成并有收口记录；本阶段先于 D2A/D2B，避免新武器专属处决 Montage 继续产生方向专用 Hit Notify。
- 当前工作树：存在用户-owned `Content/**` 增删改和未跟踪内容、`Config/DefaultEngine.ini` WIP、`Config/Automation/Presets/1.json` 删除、`ROADMAP.md`/本文件 WIP 及其他配置改动。不得回滚、清理或顺带纳入这些变更。

## 目标与运行时问题

正面处决与背刺处决当前各有一套 Native Hit Notify 和事件 Tag。两套 Ability 的伤害、锁定、动画身份和生命周期逻辑本来就是独立的，但动画制作只需要表达同一个语义：在这一帧结算处决命中。

本阶段建立一个新的 Native 入口：

1. 新增 `UAnimNotify_PlayerExecutionHit`，发送统一的 `Event.Action.Execution.Hit` Gameplay Event。
2. Front/Backstab Ability 的 active instance 同时支持统一入口和各自旧入口，旧资产在迁移前继续工作。
3. 两个方向仍分别保留自己的 Ability 身份、几何校验、`ExecutionLockContext`、Activation Token、动画归属校验、`FMeleeHitResolver` 唯一伤害路径和 exactly-once 门禁。
4. 本阶段只收口 Native contract；不自动迁移或修改任何 `.uasset`/Montage。新武器从本阶段起只使用统一 Notify。

## 工具路线与责任

- Outer：`ue-stage-workflow`。
- Primary：`ue5-cpp-gameplay`。
- Support：none。
- Route reason：这是现有 GAS 处决能力的窄事件契约收口，需要 Native AnimNotify、Gameplay Tag、AbilityTask 监听和 focused Automation；不需要 Blueprint、资产编辑或通用战斗重构。
- Execution route：`manual/out-of-band Gemini`。
- Contract owner：Main/Codex。
- Implementation writer：Gemini，仅能修改下列批准路径；不得改变契约、所有权或范围。
- Main 负责架构与计划、范围决策、用户验证解释、Fresh Review、文档、暂存和提交；用户负责 Visual Studio 编译、Editor readback、Automation、PIE/视觉验证和最终提交批准。
- Gemini 不得修改 `plan.md`、`ROADMAP.md`、`ROADMAP-archive.md`、`ARCHITECTURE.md`、`README.md`，不得暂存或提交。
- Code navigation：需要确认源码调用链或 UE/GAS 声明时，使用一次有目标的 CodeGraph 查询；禁止全库漫游。实现完成后的影响审查按 `ue-strict-review` 的 diff-first 规则使用 code-review-graph，图输出不是运行时证据。

## 批准修改路径

1. `E:\GameDevelop\PolyQuest\Config\Tags\PolyQuestGameplayTags.ini`
2. `E:\GameDevelop\PolyQuest\Source\PolyQuest\Public\Animation\Combat\AnimNotify_PlayerExecutionHit.h`（新增）
3. `E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\Animation\Combat\AnimNotify_PlayerExecutionHit.cpp`（新增）
4. `E:\GameDevelop\PolyQuest\Source\PolyQuest\Public\AbilitySystem\Abilities\PlayerFrontExecutionAbility.h`
5. `E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\AbilitySystem\Abilities\PlayerFrontExecutionAbility.cpp`
6. `E:\GameDevelop\PolyQuest\Source\PolyQuest\Public\AbilitySystem\Abilities\PlayerBackstabExecutionAbility.h`
7. `E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\AbilitySystem\Abilities\PlayerBackstabExecutionAbility.cpp`
8. `E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\Tests\ExecutionHitNotifyAutomationTests.cpp`（新增）

允许的现有 Ability 修改仅限：统一 Hit Tag 的 Task/回调接入、各自 Legacy Hit Task、Task 的失效测试 seam、相关 EndAbility 清理和精确 Payload 校验。不得改写伤害计算、锁定状态机或其他处决事件。

### 明确未批准路径

- `AnimNotify_PlayerFrontExecutionHit.*`、`AnimNotify_PlayerBackstabExecutionHit.*`（本阶段保持原样）
- 既有测试文件（包括 `ExecutionReleaseOutcomesAutomationTests.cpp`）
- `ExecutionLockContext.*`、`MeleeHitResolver.*`、`EnemyVictimExecutionAbility.*`、`EnemyCharacter.*`、`PlayerCharacter.*`
- `Build.cs`、其他 Tag/Input/Config 文件、Blueprint、地图以及全部 `Content/**`/`.uasset`/`.umap`
- D2A/D2B 的 Snap 对齐、武器 Montage 选择、VictimStart/Release、Launch/反馈表现和普通攻击 Motion Warping

若实现确实需要未批准路径，必须停止并返回证据，由 Main 重新定范围；不得通过复制逻辑或临时文件绕过边界。

## Canonical Native 合同

### 统一 Notify

- 类：`UAnimNotify_PlayerExecutionHit`。
- `Notify` 只从 `MeshComp->GetOwner()` 获取 Owner/ASC，失败或 ASC 不存在时 fail-closed 并返回。
- 事件 Tag：`Event.Action.Execution.Hit`，必须从配置注册且无效时 fail-closed。
- Payload 固定为：`Instigator = Owner`、`Target = Owner`、`OptionalObject = Animation`。
- Notify 只发送 Gameplay Event；不选择处决目标、不直接施加 GE、不绕过 Ability 或 `FMeleeHitResolver`。
- 显示名：`Player Execution Hit`。

### Tag 与触发器

- 在 `PolyQuestGameplayTags.ini` 注册 `Event.Action.Execution.Hit`。
- 保留 `Event.Action.Execution.Front.Hit` 和 `Event.Action.Execution.Backstab.Hit` 及旧 Notify，直到 Authored Migration / Retirement Gate 完成。
- 统一 Hit Tag 只由 active Front/Backstab instance 内的 `UAbilityTask_WaitGameplayEvent` 消费；不得把统一 Tag 或 Legacy Hit Tag 添加到 `UEnemyVictimExecutionAbility` 或处决 Ability 的 `AbilityTriggers`，不得用 Hit 事件重新激活 Ability。
- 所有 Hit Task 使用 `OnlyMatchExact = true`。禁止监听 `Event.Action.Execution` 父 Tag 或其他宽匹配。

### 双方向精确白名单

共享 `HandleHitEventReceived` 回调内部必须再次做精确校验，因为测试 seam 可以绕过 AbilityTask 直接调用回调：

- Front 只接受 `Event.Action.Execution.Hit` 或 `Event.Action.Execution.Front.Hit`。
- Backstab 只接受 `Event.Action.Execution.Hit` 或 `Event.Action.Execution.Backstab.Hit`。
- 跨方向 Tag、Release/VictimStart、父 Tag、任意无关 Tag 一律拒绝。
- 统一入口与对应 Legacy 入口混合到达时，只允许第一个完整合法事件进入现有命中路径；`bDamageEventConsumed` 仍是唯一 exactly-once 门禁，第二个事件完全忽略。
- 保留现有 Player/Target 身份、Activation Token、Context current/active、动画 Montage/Sequence identity、几何/距离、目标存活和 ASC 有效性校验；任何校验失败不得结算伤害，也不得消耗事件。
- 成功命中仍只构造现有 `FMeleeHitRequest` 并调用 `FMeleeHitResolver::TryResolveHit`；不得新增第二条伤害入口。

## Legacy 兼容实现

- Front 新增一个可选 `WaitLegacyHitEventTask`，精确监听 Front Legacy Tag；Backstab 对称监听 Backstab Legacy Tag。
- 统一 Task 与 Legacy Task 共用现有 Hit callback、Token 检查和 exactly-once 门禁，不复制命中逻辑。
- Legacy Tag 请求无效时只跳过对应可选 Task；统一 Tag/统一 Task 无效时按 canonical contract fail-closed，不得静默改回宽匹配。
- Legacy Task 与现有事件 Task 一样声明为 `UPROPERTY(Transient)`，在正常结束、取消、目标销毁和失效路径中清理。
- 在 `WITH_DEV_AUTOMATION_TESTS` 下增加 `SetTestInvalidateWaitLegacyHitEventTaskAfterReady(bool)`，并在 Task `ReadyForActivation()` 后按现有重入规则检查 Ability/Context/Task 有效性；若同步重入已结束 Ability，不得恢复旧 Task 或 active identity。
- 不修改旧 Notify 的 `GetNotifyName_Implementation()` 为 `[DEPRECATED]`；这属于 Editor hygiene/Authored Migration，不进入本 Native Gate，也不修改既有测试文件。

## 实施顺序

1. 以当前 HEAD 重新确认基线、批准路径和工作树 WIP；不沿用旧 SHA，不修改无关文件。
2. 核对现有 Front/Backstab Hit Task、回调和测试 seam；需要补充调用链时进行目标明确的 CodeGraph 查询。
3. 注册统一 Tag，新增 `UAnimNotify_PlayerExecutionHit` 头/源文件。
4. 对称更新 Front/Backstab：添加统一 exact Task、可选 Legacy Task、任务成员/测试注入、回调白名单和 EndAbility 清理；保持现有命中与生命周期代码不变。
5. 新增 focused Automation，覆盖统一/Legacy/跨方向/畸形 Payload/重复事件/失效重入。
6. 运行 Rider `get_file_problems`（所有改动 C++ 文件）和 `git diff --check`；不运行 UBT、`Build.bat`、UAT 或 Rider build。
7. 提交静态证据和未运行门禁给 Main；由用户执行手动编译、Editor readback、Automation 和 Scene01 PIE。
8. 用户门禁通过后，Main 执行一次独立 `ue-strict-review`；发现具体缺陷时只按 review 规定做一次有证据的修复和一次定向复跑。

## Automation 测试矩阵

新增套件：`PolyQuest.Combat.ExecutionHitNotify`，至少覆盖：

1. Notify 无 Owner、无 ASC、无 Animation 和无效 Tag 时静默 fail-closed；统一 Tag 已注册；显示名正确。
2. Front 收到统一 Hit 时正确进入现有命中路径且只结算一次。
3. Front 收到 Front Legacy Hit 时保持兼容且只结算一次。
4. Backstab 收到统一 Hit 时正确进入现有命中路径且只结算一次。
5. Backstab 收到 Backstab Legacy Hit 时保持兼容且只结算一次。
6. Front 收到 Backstab Legacy、Backstab 收到 Front Legacy，以及任意无关/父/Release/VictimStart Tag 时拒绝，不扣血、不消耗事件。
7. 统一与对应 Legacy 混合、重复派发时只允许第一个合法命中；`bDamageEventConsumed` 和 Context 状态保持正确。
8. 错误 Instigator、Target、Animation、Token、Context、死亡目标或无效 ASC 时 fail-closed。
9. 每个事件 Task 和 Legacy Task 在 `ReadyForActivation()` 同步失效、取消、目标销毁、正常 EndAbility 后都不残留；Legacy task 的测试失效注入与统一 task 对称。

回归运行：`PolyQuest.Combat.ExecutionLockIn`、`PolyQuest.Combat.FrontExecution`、`PolyQuest.Combat.Backstab`、`PolyQuest.Combat.ExecutionLethalRecovery`、`PolyQuest.Combat.ExecutionReleaseOutcomes`、`PolyQuest.Combat.ExecutionVictimPresentation`。不得把既有 GAS 日志警告或静态检查结果描述为 PIE 证据。

## 用户验证门禁

- Compile：用户在 Visual Studio 2022 手动构建 `PolyQuestEditor (Development Editor)`，报告真实结果和首个错误（如失败）。
- Editor readback：用户确认 `Event.Action.Execution.Hit` 可在项目 Tag 配置中读取、`Player Execution Hit` Native Notify 可在编辑器类列表中出现；不修改资产，不要求本阶段完成 Montage 迁移。
- Automation：专项套件与上述回归套件通过；专项测试通过不等于资产或 PIE 已验证。
- Scene01 PIE：使用现有 Front/Backstab 资产验证旧 Legacy Notify 仍可完成处决且命中不重复；若用户已在测试 Montage 中手动放置统一 Notify，再分别验证统一入口。记录输入、命中、锁定释放和回归行为。
- Main 未收到适用门禁证据前，不得标记 Native Gate 完成、更新退役结论或提交。

## 两道关闭门与后续迁移

### Native Contract Gate（本阶段）

满足以下条件后才算完成：新 Notify/Tag 编译可用；Front/Backstab 双通道 exact listener、Legacy 兼容、Payload 防御和 focused Automation 通过；旧路径未被破坏；用户编译/Automation/必要 PIE 证据齐全；Main Fresh Review 无未解决 P0-P2。

### Authored Migration / Retirement Gate（后续独立门）

用户在 Unreal Editor 中将实际处决 Montage 的旧 Notify 替换为统一 Notify，并通过 Editor/Reference Viewer/readback 证明旧 Notify 类无资产引用；随后再进行用户编译、Automation、Scene01 PIE 和 Main 审查。只有该门通过，才另立阶段移除 Legacy listener、旧 Notify 类和旧 Tag。禁止文件系统自动迁移、手改 `.uasset` 或提前删除兼容路径。

## 非目标、债务与提交边界

- 不做武器专属处决 Montage 选择（D2B）、握手 Snap 对齐（D2A）、VictimStart/Release 或受害者表现、Launch/震屏/出血、Contextual Animation、Boss 投技和普通攻击 Motion-Warp 重构。
- 不修改旧 Notify 显示名，不引入 `Small/Big/Launch` 反应枚举，不新增共享处决 Ability 基类。
- 不修改 `MeleeHitResolver`、`ExecutionLockContext`、Enemy/Victim 能力或任何资产/Blueprint。
- 阶段候选源码提交只允许本计划 8 个路径及 Main 的文档变更；排除全部 `Content/**`、`AGENTS.md`、其他 Config WIP、生成目录和临时文件。提交前 Main 必须按显式路径暂存、检查 staged diff 和 `git diff --cached --check`，并取得用户明确批准。
- 已知债务：旧资产迁移、旧 Notify/Tag 零引用证明和 Editor deprecated hygiene 仍由 Authored Migration / Retirement Gate 负责；在该门通过前，Legacy 兼容代码是有意保留的运行时边界，不是遗漏。
- 任意新增公共 API、Tag/Input/Config 契约、资产字段、生命周期所有权或未列文件需求都必须停止并返回 Main 重新定范围。

## Handoff 完成凭证要求

Gemini 回交时必须列出：实际修改路径、每个路径对应的函数/类、统一与 Legacy Task 的 exact 监听证据、Rider 静态检查结果、`git diff --check` 结果、focused/regression Automation 是否运行、未运行的用户门禁、严格实施自审 findings、剩余风险；明确未暂存、未提交且未修改批准范围外文件。

## 阶段收口记录（2026-09-04）

- **实现证据（Executor）**：Gemini 已完成统一 `UAnimNotify_PlayerExecutionHit`、`Event.Action.Execution.Hit` 注册、Front/Backstab 双方向 exact listener、Legacy Hit 兼容 task、Payload 白名单和 focused Automation；Gemini 报告独立实施自审为 0 缺陷。该证据属于实现交付和静态/测试报告，不替代用户编译、Editor 或 PIE 证据。
- **实际变更范围**：当前批准的 8 个路径均为本阶段候选范围：1 个 Tag 配置、1 对统一 Notify 源文件、Front/Backstab Ability 各 1 对文件、1 个专项测试文件。旧 Notify、既有测试、资产、Blueprint、Build.cs 和其他用户 WIP 未获本阶段授权。
- **Main Fresh Review**：基线 `8f1357af39ec520c94f93e9b93c8ccf781170ff0` 与 code-review-graph 构建 SHA 一致；按两批有界流程检查批准 diff、相关事件/生命周期一跳调用链和 exact Tag 合同，未发现可证实的 P0/P1/P2 缺陷。图谱提示的结构性测试覆盖缺口保留为残余验证风险，不作为缺陷结论。
- **已确认验证**：用户确认 Visual Studio `PolyQuestEditor (Development Editor)` 编译通过（0 错误）、`PolyQuest.Combat.ExecutionHitNotify`、`PolyQuest.Combat.FrontExecution`、`PolyQuest.Combat.Backstab` Automation 成功、Editor Notify 列表可读回 `Player Execution Hit`，且 Scene01 PIE 无 Ensure/崩溃；`git diff --check` 退出码为 0（仅有行尾转换提示）。
- **当前结论**：Native Contract Gate 已完成；本阶段提交不移除 Legacy 路径，也不建立旧资产引用为零的结论。完成态历史记录追加到 `ROADMAP-archive.md`，后续迁移门单独管理。
- **下一门禁节奏**：下一阶段为 Authored Migration / Legacy Retirement Gate：先建立实际 Montage/Notify 清单，再在 Unreal Editor 逐资产替换旧 Notify，读回时序和引用，运行旧/新入口回归并证明旧类/Tag 零引用，最后另立退役提交。D2A、D2B、E 不依赖旧路径退役，但新武器只能使用统一 Native Notify。
