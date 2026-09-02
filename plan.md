# TODO-07B8-A：Enemy Stance Break RateWindow Adoption v1

## 阶段状态与基线

- 状态：已实施，专项验证、Main Fresh Review 与文档收口已完成；当前等待用户提交批准。
- 日期：2026-09-02。
- 仓库：E:\GameDevelop\PolyQuest。
- 基线：HEAD 049507c。工作树不干净，包含用户-owned Content/**、资产差异、Config/Automation/Presets/1.json 及其他未跟踪 WIP；这些内容不属于本阶段。
- 上一阶段：TODO-05B Backstab v1 已完成并在 ROADMAP-archive.md 留有历史收口记录。本文件承接为当前阶段的详细收口记录。
- 路线：TODO-07B8-A -> TODO-05A1-A -> TODO-03C。

本阶段实际已产生七个批准路径的源码/测试变更；用户-owned Content、资产差异、`Config/Automation/Presets/1.json`、`AGENTS.md` 及其他 WIP 均不属于本阶段。

## 阶段目标

本阶段只回答一个运行时问题：Enemy Stance Break 的活动 Montage 能否消费现有 Montage Rate Window 事件，在局部窗口内调整播放速率，并在自然结束、取消、中断、销毁和 UnPossess 等所有退出路径可靠恢复原始速率，同时不改变 Stunned、移动锁定、Poise 恢复或 GAS 伤害合同。

预期结果：

- UEnemyStanceBreakAbility 对现有 Event.Action.RateWindow.Begin/End 事件显式 opt-in。
- 速率窗口只影响当前 Stance Break Montage，不使用全局 Time Dilation，不监听所有角色 Montage。
- 作者可在 Stance Break Montage 上用 MontageRateWindow 延长可处决时段；约一秒窗口需要接近三秒时可从 RateMultiplier 约 0.33 开始，由用户在 Editor/PIE 调整。
- 任何异常事件或生命周期竞态都 fail-closed，且不会留下 Stunned、移动禁用、AbilityTask、委托或非基线播放速率。

## 工具路线与责任

- Outer：ue-stage-workflow。
- Primary：ue5-cpp-gameplay。
- Support：ue5-debug-validation、ue5-blueprint-workflow（仅用于用户资产验证清单，不修改资产）。
- Route reason：这是一个 Native GAS Ability 生命周期切片，复用已有 AnimNotifyState 事件并需要在 Montage/Ability 终止边界恢复状态；不需要全局动画系统或新的 Editor 自动化框架。
- Execution route：manual/out-of-band Gemini。
- Contract owner：Main/Codex。
- Implementation writer：Gemini，只能写下列批准的源码和测试路径。
- Main 负责计划、架构与合同、范围决定、验证解释、Fresh Review、文档、暂存和提交；用户负责 Editor 作者化、手动编译、Automation、PIE 和最终提交批准。
- Gemini 不得修改 plan.md、ROADMAP.md、ROADMAP-archive.md、ARCHITECTURE.md 或 README.md，不得暂存或提交。

## 当前源码事实

- UAnimNotifyState_MontageRateWindow 已发送 Event.Action.RateWindow.Begin 和 Event.Action.RateWindow.End。
- Begin 的 EventMagnitude 来自 RateMultiplier，OptionalObject 是 UAnimSequenceBase* Animation；该对象只能用于 Montage/Sequence 归属校验，不能当作 NotifyState 实例 token。
- UEnemyStanceBreakAbility 已拥有 InstancedPerActor、ServerOnly、Stunned ActivationOwnedTags、MontageTask、活动 Montage、移动锁定、OnMontageEnded 和 Poise 恢复清理。
- APlayerCharacter::UnPossessed 已使用 Ability.Action.Teardown.OnUnpossess 取消带该标签的 Ability；Enemy 端需要对齐这一 teardown 入口。
- 现有 Player RateWindow 消费者允许作为行为参考，但本阶段不批量重写它们。

## 批准变更路径

以下是唯一批准的实现路径：

    Source/PolyQuest/Public/AbilitySystem/Abilities/MontageRateWindowLifecycle.h
    Source/PolyQuest/Private/AbilitySystem/Abilities/MontageRateWindowLifecycle.cpp
    Source/PolyQuest/Public/AbilitySystem/Abilities/EnemyStanceBreakAbility.h
    Source/PolyQuest/Private/AbilitySystem/Abilities/EnemyStanceBreakAbility.cpp
    Source/PolyQuest/Public/Character/Enemy/EnemyCharacter.h
    Source/PolyQuest/Private/Character/Enemy/EnemyCharacter.cpp
    Source/PolyQuest/Private/Tests/EnemyStanceBreakRateWindowAutomationTests.cpp

不在批准范围内的路径即使看起来方便也必须停止并返回 Main，不得自行扩展。

## 冻结运行时合同

### RateWindow 生命周期 helper

- 新增窄的 Ability 侧 helper FAbilityMontageRateWindowLifecycle。它是可复用的生命周期协议，不是 ABaseCharacter 全局监听器，也不自动接管其他 Ability。
- Helper 只在拥有它的 Ability 明确 BindAndCapture 后工作；Ability 自己创建和销毁 WaitGameplayEvent tasks，并把 Begin/End payload 转发给 helper。
- BindAndCapture 必须发生在 MontageTask->ReadyForActivation() 之后，并且确认 BoundAnimInstance->Montage_IsActive(ActiveMontage) 成功之后。
- 捕获当前实际 Montage_GetPlayRate() 作为 baseline。若值非 finite 或小于等于 KINDA_SMALL_NUMBER，记录一次 Warning 并使用 1.0f 作为保底 baseline。
- Begin 必须同时满足：Ability 仍 active、Avatar/AnimInstance/Montage 有效、事件 Instigator 与 Target 都是 Avatar、Payload.OptionalObject 属于当前 ActiveMontage 或其 SlotAnimTrack 中的源 Sequence、EventMagnitude finite 且大于零、当前 Montage 仍 active。任一条件失败都不得修改栈或播放速率。
- 每个有效 Begin 先读取并压入当前实际播放速率，再将 EventMagnitude 作为新的 Montage play rate。使用纯 TArray<float> LIFO 栈，允许同一 Montage 的重叠/嵌套窗口。
- Begin 读取到非 finite 或过小的当前播放速率时拒绝该事件，不压栈、不写入新的速率；不得用异常值污染恢复栈。
- 每个有效 End 只接受当前 Montage/Sequence 和 Avatar 的事件；有栈时弹出并恢复上一层速率，栈空时恢复 baseline。额外 End、错误归属或无效状态不改变速率。
- RestoreAndClear 必须在 Montage_Stop 之前执行；恢复时再次检查弱引用、AnimInstance、Montage 有效且 Montage active，任何对象失效都安全跳过。清理后 baseline、栈、绑定弱引用和 applied 状态必须归零/失效。
- 每次 Ability 激活都要使上一轮 task/delegate/context 失效；Begin、End、Montage 结束和 teardown 回调必须检查当前 active 状态、当前 Montage 以及可证明的当前激活代际/任务身份，迟到旧回调不得结束或修改新一轮 Ability。
- 不新增 NotifyState token，不使用 OptionalObject2，不修改 AnimNotifyState_ActionWindows.cpp。

### Enemy Stance Break 接入

- 在 UEnemyStanceBreakAbility 中缓存 RateWindow Begin/End tags，创建对应 WaitGameplayEvent tasks，并对 Tag、Task、Context 和 ReadyForActivation 后的同步终态做 fail-closed 检查。
- 实施顺序固定为：重置本轮瞬态状态并完成二次前置校验 -> 创建并判空所有 tasks -> CommitAbility -> 绑定事件/结束委托 -> 依次 ReadyForActivation，每次调用后检查 Ability 与 Task 终态 -> 确认 Montage active -> BindAndCapture baseline。任何一步失败都走统一 EndAbility。
- Montage 未实际启动、Task 创建失败或 Task 在启动阶段失效时，统一走现有 EndAbility 清理路径，不播放一个没有可靠窗口监听的 Stance Break。
- Montage 成功 active 后，绑定并捕获 helper baseline；Begin/End handler 只处理当前 Ability 的事件，不改变 Stunned 的所有权。
- EndAbility 的顺序固定为：先使当前激活/回调失效并调用 RestoreAndClear -> 停止 Montage -> 结束 RateWindow tasks 与 MontageTask、解绑 Montage 委托 -> 恢复既有移动状态和 Poise -> 调用 Super::EndAbility。所有自然结束、BlendOut/中断、外部取消、Enemy 销毁和 UnPossess 都汇入这一条幂等路径。
- 保留现有 Stunned ActivationOwnedTags、AbilitiesToCancel、移动锁定、HitReaction 协作和 RestorePoiseToMax 语义；RateWindow 不延长或替换 Stance Break Ability 的逻辑生命周期。
- 在构造函数中将有效的 Ability.Action.Teardown.OnUnpossess 加入 AbilityTags，使 Enemy UnPossessed 能通过 GAS 选择器取消该 Ability。

### Enemy teardown

- AEnemyCharacter 增加 UnPossessed override，沿用 Player 的模式：查找有效 Ability.Action.Teardown.OnUnpossess，调用 ASC->CancelAbilities，再调用 Super::UnPossessed。
- 只取消带 teardown tag 的 Ability；不调用 CancelAllAbilities，不改变死亡、AI、Poise 或其他角色的 teardown 合同。

## 明确非目标

- 不接入 Enemy HitReaction、Small HitReaction、Launch 或 Enemy Melee；它们作为后续独立 adoption slice。
- 不批量重写 Player RateWindow 消费者，不抽取全局动画监听器，不修改 NotifyState、Build.cs、uproject 或 Gameplay Tag 配置。
- 不引入 TODO-05A1 的双人处决锁定、BeingExecuted/Victim Ability、无敌、外部伤害阻断、死亡延迟、Recovery、Launch/Knockback、Camera、HitStop、Audio、Motion Warp 或第二条伤害路径。
- 不把三秒 Stance Break 时长当作处决锁定正确性的依赖；双人时序仍由 TODO-05A1 负责。
- 不手动编辑、移动、导入、删除或重命名任何 uasset/umap/Blueprint/Montage。

## Automation 计划

测试文件：Source/PolyQuest/Private/Tests/EnemyStanceBreakRateWindowAutomationTests.cpp

专项测试名：PolyQuest.Combat.EnemyStanceBreakRateWindow。

至少覆盖：

1. CDO 合同：InstancedPerActor、ServerOnly、Stunned owned tag、既有取消集合、OnUnpossess teardown tag，以及 RateWindow 事件 tag 有效。
2. 非 1.0 baseline 的单层 Begin/End：Begin 保存实际 baseline，End 恢复实际 baseline，而不是硬编码 1.0。
3. 同一 Montage/源 Sequence 的两层嵌套窗口按 LIFO 恢复；额外 End 不破坏当前速率。
4. baseline 非 finite/过小、倍率 NaN/Inf/零/负数、错误 Montage、错误 Actor、缺失 Tag、缺失 OptionalObject 的 fail-closed 行为。
5. Bind 时机和 Montage active 门禁；WaitGameplayEvent 或 MontageTask 创建/激活失败时 Ability 立即终止且不留下 ActivationOwnedTags。
6. Montage 自然结束、取消、中断、Enemy 销毁、UnPossess 后，速率栈、tasks、delegates、移动锁定和 Stunned 都被清理。
7. 既有 Stance Break -> Poise 恢复、HitReaction/Launch 协作和 Front Execution 入口的回归，不改变现有伤害/Reaction 路径。

测试可在 WITH_DEV_AUTOMATION_TESTS 下增加最小 seam，但不得新增 Shipping API、模拟全局动画系统或依赖未批准的 Content 资产。异步回调必须验证 Ability 当前 active、对象有效和当前 Montage/代际归属。

## 用户验证矩阵（阶段门禁）

Gemini 完成静态检查后，停止等待用户门禁，不自行启动 UBT 或 Editor。

- 静态：批准 C++ 文件执行 Rider error-level lint/get_file_problems；执行 git diff --check。
- 编译：用户在 Visual Studio 2022 编译 PolyQuestEditor (Development Editor)。
- Automation：用户运行 PolyQuest.Combat.EnemyStanceBreakRateWindow，并回归 PolyQuest.Combat.HitReaction 与 PolyQuest.Combat.FrontExecution。
- Editor readback：用户确认 /Game/_Abilities/Enemy/GA_EnemyStanceBreak 仍指向正确 Ability/Montage；在 /Game/BP/Montages/AM_StanceBreak 的实际 Stance Break 段放置 MontageRateWindow，RateMultiplier 可从约 0.33 起调；确认没有产生无关资产差异。
- PIE：用户在 /Game/Maps/Scene01 验证 Stance Break 局部变速、处决窗口体感、自然结束、取消/中断/销毁/解绑后的速率和移动恢复。该验证只证明本阶段 RateWindow 表现，不证明 TODO-05A1 的双人锁定或死亡 Recovery。

## 失败与停止条件

- 发现需要新增 Tag、修改 Notify、修改 Build.cs、修改公共 Shipping API、触碰 Content/Blueprint/资产、改变 Stunned/Poise/伤害所有权，立即停止并把证据返回 Main。
- 发现现有事件无法提供当前 Montage/Sequence 归属、LIFO 语义或安全 teardown 证据时，先报告设计缺口，不用全局监听器或 loose tag 绕过。
- 静态检查或专项测试失败时保留首个失败证据；同一根因最多一次修复和一次定向重跑，不开启无边界循环。
- 未经 Main 明确批准，不得修改文档、暂存、提交、重置或删除用户文件。

## 文档与提交边界

- Gemini 只提交实现证据，不编辑项目文档。
- 用户验证和 Main Fresh Review 通过后，Main 才更新 ROADMAP.md 的阶段状态/下一指针，并把本阶段详细验证与工作树快照保留在 plan.md；替换下一阶段计划前将必要历史收口追加到 ROADMAP-archive.md。
- ARCHITECTURE.md 只记录已经稳定且验证通过的 RateWindow 所有权/生命周期合同，不记录临时调参步骤。
- 阶段提交只允许包含七个批准 Source/test 路径，以及 Main 收口所需的文档路径；所有 Content、Blueprint、Montage、地图、资产和其他 WIP 排除。

## 实施、验证与复核收口

- **实际结果**：新增 `FAbilityMontageRateWindowLifecycle` 作为 Ability 侧局部协议；`UEnemyStanceBreakAbility` 显式创建并消费 RateWindow Begin/End 事件，按当前 Montage/源 Sequence 校验归属，捕获实际 baseline、用 LIFO 栈处理嵌套窗口，并在自然结束、取消、中断、销毁和 UnPossess 前恢复速率。`AEnemyCharacter::UnPossessed()` 只取消带 `Ability.Action.Teardown.OnUnpossess` 的 Ability，既有 Stunned、移动锁定、Poise 恢复和伤害路径保持不变。
- **实际变更路径**：`Source/PolyQuest/Public/AbilitySystem/Abilities/MontageRateWindowLifecycle.h`、`Source/PolyQuest/Private/AbilitySystem/Abilities/MontageRateWindowLifecycle.cpp`、`Source/PolyQuest/Public/AbilitySystem/Abilities/EnemyStanceBreakAbility.h`、`Source/PolyQuest/Private/AbilitySystem/Abilities/EnemyStanceBreakAbility.cpp`、`Source/PolyQuest/Public/Character/Enemy/EnemyCharacter.h`、`Source/PolyQuest/Private/Character/Enemy/EnemyCharacter.cpp`、`Source/PolyQuest/Private/Tests/EnemyStanceBreakRateWindowAutomationTests.cpp`。
- **复核修复**：Main 的首轮 Fresh Review 发现测试通过临时 CDO 写入污染后续测试，以及测试 Montage-active 旁路可能跨激活残留；修复限定在批准的 Ability/测试路径内，改为保存并立即恢复 CDO 原值，并在 Ability 激活/结束边界复位测试旁路。最终窄复核未发现 P0/P1/P2 blocker。
- **已确认验证**：用户确认 `PolyQuest.Combat.EnemyStanceBreakRateWindow` Automation 为 `Success`，并确认 `/Game/Maps/Scene01` PIE 通过。Main 已执行批准 C++ 路径的 Rider error-level 检查（0 errors）和 `git diff --check`（通过）。Test Run 3 中的 Montage 播放失败、baseline `0.0` 回退到 `1.0` 以及 resolver/Poise 负路径日志属于无头测试夹具的 fail-closed 覆盖，不改变成功结论。
- **尚未形成的独立收据**：当前没有本阶段单独归档的 Visual Studio `PolyQuestEditor (Development Editor)` 编译记录，也没有 `GA_EnemyStanceBreak` / `AM_StanceBreak` 的明确 Editor readback 记录；这些是非阻塞的 authored-validation debt。关闭条件是用户完成对应编译与资产读回，或对不适用项提供真实的 evidence-backed no-adoption；不能把本阶段源码/Automation/PIE 证据表述为干净 authored baseline。
- **阶段边界与下一步**：`TODO-07B8-A` 只完成 Enemy Stance Break 的 RateWindow adoption。Enemy 其他 Montage、双人处决锁定、目标侧 Victim Ability、无敌/外部伤害阻断、致死 Recovery、Launch/Knockback、执行反馈和通用执行基类仍不属于本阶段；下一开放实施切片为 `TODO-05A1-A`。
