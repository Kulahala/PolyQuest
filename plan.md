# TODO-07B5：Small Hit Reaction Retrigger v1

## 阶段状态与执行路由

- **状态**：实现、用户 Automation/PIE 验证与 Main Fresh Review 已完成；本文件现记录阶段 closeout、剩余验证债务与提交边界。
- **基线**：`main @ 18b4695851cd`。
- **工作区边界**：保留现有用户 `Content/**` WIP、删除项、未跟踪资源、`Config/Automation/Presets/1.json` 以及其他 Source/文档改动；不得清理、回滚或顺手纳入本阶段。
- **Outer**：`ue-stage-workflow`
- **Primary**：`ue5-cpp-gameplay`
- **Support**：`ue5-debug-validation`
- **Route reason**：这是两个 Native GAS Reaction Ability 的有限重激活与 Montage/AbilityTask 生命周期调整，配套一个聚焦 Hit Reaction Automation 矩阵和用户 PIE 门禁；不涉及资产迁移或新战斗系统。
- **Execution route**：`manual/out-of-band Gemini`。
- **Ownership**：Main 负责合同、计划、范围、验证解释、fresh review、文档、staging 和 commit；Gemini 只实现下列冻结的 Source/test 路径；用户负责手动编译、Editor readback、PIE 和最终提交批准。
- **Contract owner**：Main。Gemini 是 implementation writer，不得改变公开合同、ASC 所有权、Tag/Input 语义或范围。

## 目标与冻结合同

本阶段只让 Player/Enemy Small Hit Reaction 在当前同一个 `InstancedPerActor` Ability 尚未自然结束时接受新的 Small hit。GAS 必须先结束旧轮次，再以最新事件 payload 重新解析方向并从第 0 帧开始最新的四向 Montage；`State.Action.SmallHitReacting` 仍由 `ActivationOwnedTags` 持有。

冻结行为：

- 两个构造函数都设置 `bRetriggerInstancedAbility = true`。
- 从两个 Ability 的 `ActivationBlockedTags` 中仅移除 `State.Action.SmallHitReacting`；该 Tag 保留在 `ActivationOwnedTags`。
- `State.Status.Dead`、`State.Status.Stunned`、`ServerOnly`、`InstancedPerActor`、四向 Montage 完整性、现有方向解析、Big/Launch 取消关系和 EndAbility 清理全部保持。
- 新一轮必须读取本轮 `TriggerEventData`，不得沿用上一轮方向、Montage 或运行时快照。
- 同方向连续命中可能得到同一个 `UAnimMontage*`，因此不能把“Montage 指针相同/不同”作为唯一的旧回调隔离依据。

## 关键技术决策：按 Task 代际隔离，而不是孤立 Activation ID

当前 Small Ability 直接绑定 `UAnimInstance::OnMontageEnded`。该全局动态回调只有 `Montage*` 和 `bInterrupted`，在同一 Montage 重触发时无法携带 activation 代际；单独增加 `uint32 CurrentActivationID` 但不随回调传递并不能修复竞态。

唯一允许的 v1 方案是复用项目 `UDodgeAbility` 已落地的 per-Montage-instance 模式：

- 移除两个 Small Ability 对 `AnimInstance::OnMontageEnded` 的绑定，以及 `OnActiveMontageEnded(UAnimMontage*, bool)` 作为结束入口。
- 将每轮新建的 `UAbilityTask_PlayMontageAndWait` 作为该轮唯一结束信号源，绑定其 `OnCompleted`、`OnInterrupted`、`OnCancelled` 到 Ability 的无参回调。
- 旧轮次在 `EndAbility()` 中解除 Task 的三个 Ability 回调并 `EndTask()`；旧 Task 即使有排队的 Montage 事件，也不得再向 Ability 广播。新轮次只接受新 Task 的回调。
- `ActiveMontage` 只用于当前轮次启动校验和诊断，不能再被当作跨轮次身份证明。
- 若实际 UE 5.8 Task 行为不能证明 `EndTask()` 后旧 Task 回调被抑制，必须停止并把证据交回 Main；不得新增引擎改动、通用代理类或无法携带代际的假 ID。

## 实现合同

### Player / Enemy Small Ability

- 在两个 Header 中声明 `OnMontageCompleted()`、`OnMontageInterrupted()`、`OnMontageCancelled()`，并删除不再使用的全局 Montage ended 入口。
- `ActivateAbility()` 入口先复位 `bEndAbilityRequested`、`BoundAnimInstance`、`ActiveMontage` 和 `MontageTask`。若发现残留旧 Task，必须先完成其 delegate 解绑/Task teardown 后再覆写成员，不能静默丢弃仍有效的 Task。
- 保持现有 ASC、Avatar、AnimInstance、死亡状态和四向 Montage 门禁；任一门禁失败沿已有 canonical `EndAbility()` fail-closed。
- 创建局部 `CreatedMontageTask` 后写入成员。`CommitAbility()` 返回后、绑定回调前，以及 `ReadyForActivation()` 返回后，都检查 Ability 终态、Task 身份、`IsValid`、`!IsFinished()` 和 `IsActive()`；同步结束时不得继续读取或覆写已清理的成员。
- 先设置本轮 `BoundAnimInstance`/`ActiveMontage`，再绑定 Task 三个回调，最后调用 `ReadyForActivation()`。任何回调同步重入都必须短路后续逻辑。
- 三个回调只在 Ability 仍有效时调用 `EndFromMontage(false/true)`；不得重新解析事件、重置新轮次或触碰已清理的旧指针。
- `EndAbility()` 必须幂等：先标记终态，解除 `MontageTask` 的 Ability 回调，再在对象有效时停止当前 Montage，随后 `EndTask()`、清空全部瞬态成员，最后只调用一次 `Super::EndAbility()`。如果保留任何 AnimInstance delegate 清理，解绑必须发生在 `Montage_Stop()` 之前。
- 不改变 Root Motion、移动、Damage/Poise、StateTree、输入或跨 tier interruption 语义。

### 测试 Seam

在两个 Header 的 `#if WITH_DEV_AUTOMATION_TESTS` 区域保留既有三个 getter：

- `GetTestActivationOwnedTags()`
- `GetTestActivationBlockedTags()`
- `GetTestAbilityTriggers()`

只增加验证本阶段所需的最小 seam，命名与项目既有风格一致：`GetTestRetriggerInstancedAbility()`、当前 `ActiveMontage`/`MontageTask` 的只读 getter，以及设置测试 ActorInfo 和直接驱动 Task completion/interruption/cancellation 的测试入口（若实际矩阵需要）。所有 seam 必须留在 `WITH_DEV_AUTOMATION_TESTS` 下，不得提供绕过生产 ASC、AnimInstance、Montage 或生命周期门禁的开关。

## 批准修改路径

Gemini 只能修改以下五个路径：

```text
Source/PolyQuest/Public/AbilitySystem/Abilities/PlayerSmallHitReactionAbility.h
Source/PolyQuest/Private/AbilitySystem/Abilities/PlayerSmallHitReactionAbility.cpp
Source/PolyQuest/Public/AbilitySystem/Abilities/EnemySmallHitReactionAbility.h
Source/PolyQuest/Private/AbilitySystem/Abilities/EnemySmallHitReactionAbility.cpp
Source/PolyQuest/Private/Tests/HitReactionAutomationTests.cpp
```

任何新增文件、其他 Header/Source、Config/Gameplay Tag、CombatAutomationFixture、Build.cs、Blueprint、Montage、AnimBP、Input、地图或其他资产需求，都必须停止并返回证据，由 Main 决定是否扩展；不得提交 Git。

## 执行顺序与 Automation 矩阵

1. 先在两个 Ability CDO/构造函数完成 retrigger 与 self-block 合同，再实现 Task-owned Montage callback 生命周期。
2. 收敛入口/Commit/ReadyForActivation 后的同步重入门禁，确认旧 Task 不会被新轮次覆盖。
3. 收敛 EndAbility 的 delegate、Montage、Task 和指针清理顺序。
4. 在 `HitReactionAutomationTests.cpp` 更新既有 CDO 断言并增加以下矩阵：
   - Player/Enemy 的 `bRetriggerInstancedAbility`、Owned/Blocked Tag、Event trigger 和四向配置合同；
   - 同一 Montage 指针连续两轮与不同方向 Montage 两轮：旧 Task 的 completion/interruption/cancellation 不得结束新轮次，当前 Task 必须能结束当前轮次；
   - Player 与 Enemy 真实 ASC Small event 的不同方向、同方向高频连续触发，最新 payload 胜出，active-state 只保留当前轮次；
   - Dead、Stunned、无效 Montage/AnimInstance、Commit/Task 创建失败、取消、销毁和重复 EndAbility 的 fail-closed/清理；
   - Big/Launch Reaction 既有行为回归。
5. 真实 AnimInstance/Montage 跨帧能力若在无头世界不可用，不得用 bypass 或手动改状态冒充 E2E：保留可证明的 CDO、事件路由和 Task teardown 测试，并在交付报告中记录 `Debt-07B5-RealASCRetriggerE2E`，关闭条件为有效 Montage fixture 或真实 PIE/integration 覆盖。

## 用户验证门槛

- **静态**：Gemini 对五个批准文件执行 Rider error-level 检查和 `git diff --check`，报告真实结果。
- **编译**：用户手动编译 `PolyQuestEditor` Development Editor。
- **Editor readback**：确认 `GA_PlayerSmallHitReaction`、`GA_EnemySmallHitReaction` 的 Retrigger 开启；Owned 含 `State.Action.SmallHitReacting`；Blocked 含 Dead/Stunned 且不含 SmallHitReacting；四向 Montage 完整。
- **Scene01 PIE**：
  - 不同方向连续受击选择最新方向；
  - 同方向高频轻攻击每次从第 0 帧重新播放并重置硬直，不出现抽搐后提前回 Idle；
  - 旧轮次的迟到完成事件不能结束新轮次；最后一轮自然结束后才恢复 Idle；
  - Big、Launch、Dead、Stunned、取消、销毁和 teardown 无回归。

## 非目标、风险与停止条件

- 不实现 Big/Launch retrigger、跨 tier interruption、Damage/Poise、StateTree、移动/输入 arbitration、通用 retrigger framework、网络/权限模型或新 Tag/Input。
- 不迁移或调参任何资产，不修改 Config、Build.cs、`.uproject` 或测试夹具文件。
- 不把 `ActivationID` 作为没有实际回调载体的装饰字段；不以 Montage 指针相等性单独证明回调归属。
- 若需要修改批准路径之外的公共 API、ASC 所有权、Task/引擎行为或资产合同，立即停止并交回 Main。

## Main 收尾

用户编译、Editor readback、Automation/PIE 和 Main fresh review 均有证据后，Main 才更新本文件 closeout、`ROADMAP.md` 当前/下一阶段指针及必要的 `ARCHITECTURE.md` 稳定合同，并追加 `ROADMAP-archive.md` 历史记录。提交只包含本阶段批准的 Source/test 与文档路径，排除全部用户 Content WIP、`Config/Automation/Presets/1.json` 和其他未批准变更。

## Main Fresh Review / Repair Addendum（2026-09-01）

- **状态**：Fresh Review 已完成；P2 收口与用户复验已完成，现进入阶段文档 closeout、staging 与 commit。
- **P2-1（已完成的批准窄修复）**：Player/Enemy 两处 `UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy` 调用使用了 `bAllowInterruptAfterBlendOut=false` 的默认值。若 Montage 已开始 BlendOut 后被覆盖，Task 可能只结束自身而不回调 Ability，遗留 `State.Action.SmallHitReacting`。Main 已在现有两个批准的 `.cpp` 路径内显式开启该参数，并保留原有 Task/EndAbility 时序；这是不改变公开合同的可逆窄修复。
- **P2-2（验证债务）**：Section 7 的 7.1/7.2 是手动创建 Task 与广播的 synthetic seam，7.3 覆盖真实 ASC Spec/Tag/Trigger 合同，7.4 覆盖生产方向 Resolver/Selector，但仍未证明完整的 ASC Gameplay Event → Ability retrigger → 最新 `TriggerEventData` → 跨帧 Montage Task 生产链路。不得把该测试描述为真实 E2E；若现有测试文件无法建立有效 Montage fixture，则保留为明确债务，关闭条件为集成 fixture 或专用 PIE/Automation 覆盖。
- **本轮非目标**：不新增测试夹具文件、不改资产/Config/Build.cs、不扩展批准路径、不借机重构 Retrigger 架构。
- **复验门槛**：Main 已执行 `git diff --check` 与两处修复 C++ 文件的 Rider error-level 检查，均无问题；用户随后确认 `PolyQuest.Combat.HitReaction` Automation 与 Scene01 PIE 通过，包含连续受击/后摇中断路径。未形成独立手动 `PolyQuestEditor` 编译或 Editor readback 收据。

## 阶段 Closeout（2026-09-01）

- **结果**：TODO-07B5 的 Player/Enemy Small Hit Reaction retrigger 实现已完成。两个 `InstancedPerActor`、`ServerOnly` Ability 开启 `bRetriggerInstancedAbility`，移除自身 `State.Action.SmallHitReacting` 阻断但继续由 `ActivationOwnedTags` 持有该状态；每轮由独立 `UAbilityTask_PlayMontageAndWait` 的完成/中断/取消回调驱动结束，旧 Task 先解绑再 teardown，所有终态汇入幂等 `EndAbility()`。
- **实际变更路径（仅 5 个 Source/test 文件）**：
  - `Source/PolyQuest/Public/AbilitySystem/Abilities/PlayerSmallHitReactionAbility.h`
  - `Source/PolyQuest/Private/AbilitySystem/Abilities/PlayerSmallHitReactionAbility.cpp`
  - `Source/PolyQuest/Public/AbilitySystem/Abilities/EnemySmallHitReactionAbility.h`
  - `Source/PolyQuest/Private/AbilitySystem/Abilities/EnemySmallHitReactionAbility.cpp`
  - `Source/PolyQuest/Private/Tests/HitReactionAutomationTests.cpp`
- **Main 窄修复**：在 Player/Enemy 已批准 cpp 路径的 Montage Task 创建调用中显式启用 `bAllowInterruptAfterBlendOut=true`，避免 BlendOut 后覆盖时 Task 只结束自身而遗留 Small 状态；未改变公开 API、ASC 所有权或阶段范围。
- **验证证据**：用户确认 `PolyQuest.Combat.HitReaction` Automation 通过并完成 Scene01 PIE；Gemini 报告五个批准文件 Rider error-level 检查无错误且 `git diff --check` 通过；Main 对最终差异完成 defect-first Fresh Review，P0/P1/P2 均为 0。上述证据不扩展为全量套件、独立手动编译或 Editor readback。
- **验证债务**：`Debt-07B5-RealASCRetriggerE2E` 仍开放。无头 Automation 的 7.1/7.2 是 synthetic Task seam，7.3/7.4 是真实 ASC Spec/Tag 与方向解析验证，尚未证明完整的 `HandleGameplayEvent → GAS retrigger → 最新 TriggerEventData → 跨帧 Montage Task` 链路。关闭条件是有效 AnimInstance/Montage 集成 fixture 或专用 PIE/Automation 场景；当前用户 PIE 覆盖运行时表现，但不替代该债务的独立记录。
- **范围与提交边界**：未修改或纳入 `Content/**`、`Config/Automation/Presets/1.json`、其他 Source/Config WIP、Build.cs、`.uproject`、Input、Gameplay Tag 或资产；不声称独立手动 `PolyQuestEditor` 编译或 Editor readback。下一开放阶段为 `TODO-07B6：Combat Feedback DataAsset Consolidation v1`。
