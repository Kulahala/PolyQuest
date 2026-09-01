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

# TODO-07B6：Combat Feedback DataAsset Consolidation v1

## 阶段状态与执行路由

- **状态**：实现、用户 Editor 配置、Automation/PIE 验证与 Main Fresh Review 已完成；本节现记录阶段 closeout、验证债务、Loadout 清理前置证据与提交边界。
- **基线**：`E:\GameDevelop\PolyQuest`，实施前父提交 `HEAD 8fc3d71`（`TODO-07B5` closeout）。
- **工作区边界**：当前工作区包含用户拥有的 `Content/**` WIP、删除项、未跟踪资源和 `Config/Automation/Presets/1.json`；全部保留，不清理、不回滚、不顺手纳入本阶段。
- **Outer**：`ue-stage-workflow`
- **Primary**：`ue5-cpp-gameplay`
- **Support**：`ue5-debug-validation`
- **Editor 迁移支持**：用户按 `ue5-blueprint-workflow` 完成资产创建、属性迁移和 readback；Gemini 不写入 `.uasset`、`.umap` 或 live Editor 状态。
- **Route reason**：这是一个跨 BaseCharacter、Player、Enemy、Guard/Parry Ability 的 Native C++/GAS authored-feedback 数据收拢阶段，包含 DataAsset 公共合同和现有 Automation 夹具迁移，但不引入新的战斗权威或网络模型。
- **Execution route**：`manual/out-of-band Gemini`。
- **Ownership**：Main 是合同 owner，负责计划、范围、验证解释、Fresh Review、文档、staging 和 commit；Gemini 只实现下列冻结 Source/test 路径；用户负责手动编译、Editor readback、Scene01 PIE 和最终提交批准。

## 目标与冻结决策

将当前分散在 `ABaseCharacter`、`APlayerCharacter`、`AEnemyCharacter`、`UPlayerGuardAbility` 和 `UPlayerParryAbility` 中的反馈引用与调参收拢到统一的 `UCombatFeedbackDataAsset` 类型，同时保留每个运行时 owner 的生命周期和 GAS/Damage 合同。

“一个 Combat Feedback DataAsset”解释为一个统一的 DataAsset 类型与数据合同；Player 与 Enemy 各挂一个角色级 profile，而不是共用全局 singleton。这样可以集中字段，同时允许不同角色拥有不同的声音、血液、Overlay 和镜头反馈。

迁移策略已冻结为一次性直接迁移：

- 删除旧的反馈 UPROPERTY 和旧读取路径；
- 不保留 legacy fallback，也不同时维护两套真相；
- 缺失 profile、单个资源或非法数值只关闭对应反馈通道，不阻断 Damage、Poise、Guard、Parry、死亡或 Ability 生命周期；
- 旧/新等价性通过固定基线 transient fixture 与行为断言验证，不在运行时保留旧字段作对照。

## DataAsset 公共合同

新增：

- `Source/PolyQuest/Public/Combat/Feedback/CombatFeedbackDataAsset.h`
- `Source/PolyQuest/Private/Combat/Feedback/CombatFeedbackDataAsset.cpp`

头文件合同：

- `FCombatFeedbackTierSettings` 与 `FCombatFeedbackDefenseSettings` 必须是 `USTRUCT(BlueprintType)`；
- 直接 include `Combat/Reaction/HitReactionClassifier.h`，以使用非反射 `EHitReactionTier`；
- `UCameraShakeBase`、`USoundBase`、`UNiagaraSystem`、`UMaterialInterface` 使用前置声明；
- `*.generated.h` 保持最后一个 include。

`FCombatFeedbackTierSettings` 字段固定为：

- `ReceivedHitCameraShakeClass`；
- `AttackerImpactCameraShakeClass`；
- `ImpactHitStopDurationSeconds`；
- `ImpactHitStopTimeDilation`。

`FCombatFeedbackDefenseSettings` 字段固定为：

- `GuardSuccessSound`；
- `ParrySuccessSound`；
- `ParrySuccessHitStopDurationSeconds`；
- `ParrySuccessHitStopTimeDilation`。

`UCombatFeedbackDataAsset` 字段固定为：

- `HitFeedbackOverlayMaterial`；
- `HitFeedbackOverlayDurationSeconds`；
- `ReceivedHitSound`；
- `ImpactSound`；
- `ImpactBloodSystem`；
- `SmallTier`、`BigTier`、`LaunchTier`；
- `Defense`。

提供只读 `GetTierSettings(EHitReactionTier Tier)`；`None` 和 `Invalid` 返回空结果。DataAsset 只保存 authored data，不保存 Timer、活动 Shake、World、ASC、GameplayEffect、句柄或其他运行时状态。

默认数值保持当前已验证合同：

- Overlay：`0.10s`；
- Small Hit-Stop：`0.03s / 0.1`；
- Big Hit-Stop：`0.05s / 0.03`；
- Launch Hit-Stop：`0.05s / 0.05`；
- Parry：`0.05s / 0.03`。

数值元数据固定为：Duration `ClampMin = "0.0"`、`Units = "Seconds"`；Time Dilation `ClampMin = "0.001"`、`ClampMax = "1.0"`；Overlay Duration `ClampMin = "0.0"`、`Units = "Seconds"`。

## 运行时接入与所有权不变式

### `ABaseCharacter`

- 增加角色级 `CombatFeedbackData` `UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)` 和只读 `GetCombatFeedbackData()`；
- `TriggerHitFeedbackOverlay()` 改为读取 profile；Overlay 缓存、Timer、外部 Overlay 保留和 `EndPlay()` 恢复顺序全部保持；
- 在 `WITH_DEV_AUTOMATION_TESTS` 下提供 `SetTestCombatFeedbackData(UCombatFeedbackDataAsset*)` 与 `GetTestCombatFeedbackData()`；
- 用统一 profile 注入替代旧的 Overlay 配置 seam。

### `APlayerCharacter`

删除旧的六个 Camera Shake 字段和 `ReceivedHitSound` 字段，改为从 profile 读取：

- received-hit Small/Big/Launch Shake；
- attacker-impact Small/Big/Launch Shake；
- received-hit sound。

保留 PlayerCameraManager 本地 Shake owner、活动 Manager/Shake/Class 弱引用、同类重启、tier 切换停止旧实例、`UnPossessed()`/`EndPlay()` 清理、`None/Invalid` no-op、Health/Team 过滤和 GE Spec 去重。

移除依赖旧配置字段的测试 setter；测试直接修改 transient profile。播放次数、位置和音频 bypass 仅作为 `WITH_DEV_AUTOMATION_TESTS` 观察 seam 保留，不新增生产 bypass。

### `AEnemyCharacter`

删除旧 Impact Sound、Blood 和六个 Hit-Stop 字段，改为从 profile 读取：

- `ImpactSound`；
- `ImpactBloodSystem`；
- Small/Big/Launch Hit-Stop 参数。

保留 Enemy 的 Team.Player 边界、Health modifier 去重、ImpactPoint/ActorLocation fallback、ImpactNormal 校验、血液旋转和目标侧 sound/blood owner；`APolyQuestPlayerController` 仍是全局 Hit-Stop 唯一 owner。

保持既有 tier 行为：Enemy 的 `None/Invalid` 仍使用 Small Hit-Stop；Player 的 `None/Invalid` Shake 仍 no-op。

### Guard / Parry

- `UPlayerGuardAbility` 删除 `GuardSuccessSound`，从 Player profile 的 `Defense.GuardSuccessSound` 读取；
- `UPlayerParryAbility` 删除 Parry sound 与两项 Hit-Stop 字段，从 `Defense` 读取；
- Parry 成功镜头继续调用 Player 的 Big received-hit Shake，不新增独立 Parry Camera Shake 字段；
- 删除旧反馈配置 setter，但保留 Guard/Parry 其他 GAS、窗口、消耗、取消和测试观察 seam。

### 缺失数据和诊断规则

- `CombatFeedbackData == nullptr`：对应 owner 首次触发反馈时记录一次 Warning，之后静默 fail-closed；
- profile 内单个 Sound、Niagara、Material 或 Camera Shake 为空：视为可选 no-op，不输出持续 Warning；
- Duration 非有限或不大于零，或 Dilation 非有限/不在 `(0, 1]`：仅跳过该次 Hit-Stop；
- 无效 World、位置或 HitResult：沿用当前 fallback/跳过规则；
- 任何反馈缺失不得改变 Damage、Poise、死亡、Guard/Parry 消费、GE 去重或 Ability 结束结果。

## 批准修改路径与执行顺序

Gemini 只能修改以下路径：

```text
Source/PolyQuest/Public/Combat/Feedback/CombatFeedbackDataAsset.h
Source/PolyQuest/Private/Combat/Feedback/CombatFeedbackDataAsset.cpp
Source/PolyQuest/Public/Character/BaseCharacter.h
Source/PolyQuest/Private/Character/BaseCharacter.cpp
Source/PolyQuest/Public/Character/Player/PlayerCharacter.h
Source/PolyQuest/Private/Character/Player/PlayerCharacter.cpp
Source/PolyQuest/Public/Character/Enemy/EnemyCharacter.h
Source/PolyQuest/Private/Character/Enemy/EnemyCharacter.cpp
Source/PolyQuest/Public/AbilitySystem/Abilities/PlayerGuardAbility.h
Source/PolyQuest/Private/AbilitySystem/Abilities/PlayerGuardAbility.cpp
Source/PolyQuest/Public/AbilitySystem/Abilities/PlayerParryAbility.h
Source/PolyQuest/Private/AbilitySystem/Abilities/PlayerParryAbility.cpp
Source/PolyQuest/Private/Tests/CombatAutomationFixture.cpp
Source/PolyQuest/Private/Tests/CombatHitFeedbackAutomationTests.cpp
Source/PolyQuest/Private/Tests/PlayerDefenseAudioAutomationTests.cpp
Source/PolyQuest/Private/Tests/ParrySuccessImpactFeedbackAutomationTests.cpp
```

执行顺序：

1. 新增 DataAsset、两个反射结构、默认值和只读 `GetTierSettings()`。
2. 接入 `ABaseCharacter` profile 与 Overlay。
3. 迁移 Player/Enemy runtime reads，保持各自生命周期 owner。
4. 迁移 Guard/Parry 防御反馈，移除旧反馈配置字段和 setter。
5. 在 `CombatAutomationFixture.cpp` 的 `SpawnPlayer()` 与 `SpawnPassiveEnemy()` 中创建 transient DataAsset，并注入与当前行为等价的默认数据。
6. 更新三组反馈测试，直接通过 `SetTestCombatFeedbackData()`/`GetTestCombatFeedbackData()` 修改 transient profile。
7. 执行 Rider error-level 检查和 `git diff --check`，然后交还 Main；不得自行编译、修改文档、暂存或提交。

不预期修改 `Build.cs`；现有 Engine/Niagara 依赖应已足够。若发现未列出的消费者、公共 API、Build.cs、Config、Gameplay Tag、Input、资产或生命周期合同需求，立即停止并返回证据，不得自行扩展。

### 共享合同文件的精确函数边界

Gemini 只可在下列函数、构造/字段声明及其直接 include 范围内完成迁移；不得借机改动同文件中的输入、装备、锁定、移动、Damage、Poise、死亡或 Ability 仲裁逻辑：

- `CombatFeedbackDataAsset.h/.cpp`：`UCombatFeedbackDataAsset` 构造函数（如需要）、`GetTierSettings()`，以及本阶段新增的两个 `USTRUCT`/DataAsset 字段声明。
- `BaseCharacter.h/.cpp`：`ABaseCharacter` 的反馈 profile 声明、构造函数、`ConfigureTestHitFeedbackOverlay()`（改为 profile 注入兼容 seam 或移除旧 seam）、`TriggerHitFeedbackOverlay()`、`ClearHitFeedbackOverlay()`、`EndPlay()` 及对应 `WITH_DEV_AUTOMATION_TESTS` getter/setter。
- `PlayerCharacter.h/.cpp`：反馈字段声明、构造函数、`BeginPlay()`/`EndPlay()` 中与反馈 profile 初始化或清理直接相关的语句，`TriggerParrySuccessCameraShake()`、`TriggerAttackerImpactCameraShake()`、`TriggerHitFeedbackCameraShake()`、`TriggerReceivedHitSound()`、`ResolveHitFeedbackCameraShakeClass()`、`ResolveAttackerImpactCameraShakeClass()`、`StartHitFeedbackCameraShakeInstance()`、`ClearActiveHitFeedbackCameraShake()`，以及现有反馈观察 seam 的迁移；不得改动输入、锁定、装备、移动或耐力逻辑。
- `EnemyCharacter.h/.cpp`：反馈字段声明、构造函数、`EndPlay()` 中的反馈清理（如现有路径需要）和 `HandleCombatImpactFeedback()`，以及现有反馈观察 seam；不得改动敌人生命、削韧、死亡、AI 或击飞时序。
- `PlayerGuardAbility.h/.cpp`：反馈字段/旧 setter 声明移除或迁移、构造函数，以及 `TriggerGuardSuccessFeedback()`；`CanActivateAbility()`、窗口、消耗、取消和 Montage 生命周期只允许因读取 profile 而作必要的局部改动。
- `PlayerParryAbility.h/.cpp`：反馈字段/旧 setter 声明移除或迁移、构造函数，以及 `TriggerParrySuccessFeedback()`；`CanActivateAbility()`、窗口、消耗、取消和 Montage 生命周期只允许因读取 profile 而作必要的局部改动。
- `CombatAutomationFixture.cpp`：仅 `SpawnPlayer()` 与 `SpawnPassiveEnemy()` 的 transient `UCombatFeedbackDataAsset` 创建、默认值填充和注入。
- 三个反馈测试 `.cpp`：仅各自 `RunTest()` 中与 profile 创建/注入、反馈断言和旧 setter 替换直接相关的段落；不得删减原有行为覆盖或把 synthetic seam 说成 PIE/E2E 证据。

Contract owner: Main；implementation writer: manual/out-of-band Gemini。上述函数边界是冻结合同，不是建议列表。

## Automation 矩阵与用户门槛

Automation 必须覆盖：

- 两个 USTRUCT、tier/Defense 映射和默认值；
- Player received-hit / attacker-impact 三档 Shake 与 received sound；
- Enemy 三档 Hit-Stop、Impact sound、Blood 以及 `None/Invalid -> Small` 回归；
- Overlay Timer、重复刷新、外部 Overlay 保留和 EndPlay 清理；
- Guard/Parry 成功反馈及 ImpactPoint/ActorLocation fallback；
- profile 为空、单项为空、非法数值时 fail-closed；
- 缺失反馈不阻断 Damage、Poise、Guard、Parry、Dead 或 GE 去重；
- UnPossessed、Destroyed、Controller teardown 无残留；
- 不引入 GameplayCue、第二条 Damage 路径或新的反馈 dispatcher。

目标入口：

- `PolyQuest.Combat.HitFeedback`
- `PolyQuest.Combat.AttackerImpactCameraShake`
- `PolyQuest.Combat.DefenseAudio`
- `PolyQuest.Combat.ParrySuccessFeedback`

用户门槛：

1. 用户手动编译 `PolyQuestEditor`（Development Editor）。
2. 用户在 Editor 创建并配置两个产品 profile：
   - `Content/_DataAssets/Player/FeedBack/DA_CombatFeedback_Player.uasset`
   - `Content/_DataAssets/Enemy/FeedBack/DA_CombatFeedback_Enemy.uasset`
3. 分别挂到当前实际使用的 `BP_Player` 与 Enemy Blueprint，并从旧字段迁移 Camera Shake、Sound、Blood、Overlay、Hit-Stop、Guard/Parry 数值；这些 `.uasset` 由用户维护，Gemini 不得写入。
4. 使用 Details/Reference Viewer 核对当前实际授予的 Guard/Parry GA 不再承担反馈配置；候选资产包括：
   - `Content/_Abilities/Player/Parry/GA_PlayerParry.uasset`
   - `Content/_Abilities/Player/Parry/GA_PlayerShieldParry.uasset`
   - `Content/_Abilities/Weapon/Shield/GA_PlayerShieldGuard.uasset`
   - `Content/_Abilities/Weapon/LightSword/Guard/GA_Guard_Sowrd.uasset`
5. Scene01 PIE 验证 Player 受击、Player 攻击 Enemy、Guard、Parry、Guard Break、缺失资源、死亡、UnPossess、销毁和 teardown。

## 非目标、债务与停止条件

非目标：

- 不引入 GameplayCue、全局 Feedback Dispatcher、通用反馈框架、网络模型或第二条 Damage 路径；
- 不改变 ASC、Health/Poise、Damage、Guard/Parry、CameraManager、Controller、Enemy、BaseCharacter 的所有权和顺序；
- 不迁移武器、输入、Motion Warping、Montage、AnimBP 或无关 Content；
- 不修改 `.uasset`、`.umap`、Config、Build.cs 或 `.uproject`；
- 不为测试增加生产绕过开关。

在用户手动编译、Editor readback 和 PIE 完成前，分别记录 `TODO-07B6` 验证债务及具体关闭条件。旧阶段债务不在本阶段重新打开，除非本次变更产生直接回归证据。

若实现需要修改批准路径之外的文件、改变 DataAsset 合同、增加每武器覆盖、引入旧字段回退、改变反馈 owner 或触碰第二条 Damage 路径，必须停止并交回 Main 做范围决策。

## Main 收尾

用户编译、Editor readback、Automation/PIE 和 Main defect-first Fresh Review 均有证据后，Main 才：

- 更新本文件 closeout；
- 将稳定的 DataAsset 字段与 owner 合同同步到 `ARCHITECTURE.md`；
- 将 `ROADMAP.md` 的当前指针推进到 `TODO-03I4`，并登记后续已接受的 `TODO-07B7` 与未完成验证债务；
- 追加 `ROADMAP-archive.md` 历史记录；
- 仅暂存批准的 Source/test 与文档路径，排除全部 Content WIP、`Config/Automation/Presets/1.json` 和其他未批准变更。

## Main Fresh Review（2026-09-01）

- **审查范围**：以 07B6 的 16 个批准 Source/test 路径为主，沿 `UCombatFeedbackDataAsset`、Overlay、Player Camera Shake/Received Sound、Enemy Impact/Hit-Stop、Guard/Parry 反馈入口检查一跳调用边界、空安全、World/Actor 生命周期和 GAS/Damage 所有权；未扩大到无关输入、装备、Motion-Warp 或资产内容。
- **结论**：未发现 P0、P1 或 P2 缺陷；不需要在本阶段追加源码修复。用户确认相关 Automation 与 Scene01 PIE 通过；Gemini 报告批准文件 Rider error-level 检查无错误、`git diff --check` 通过。没有独立的 Main 手动 `PolyQuestEditor` 编译收据，故不把报告中的静态/编译描述替代为 Main 编译证据。
- **P3 / 验证债务**：
  - `CombatFeedbackData == nullptr` 的主流程 Automation 没有对每个 owner 分别形成显式断言；运行时仍 fail-closed，不影响 Damage、Poise、Guard、Parry、死亡或 Ability 生命周期。
  - 缺失 profile 的一次性 Warning 合同在 Received Sound、Guard/Parry 音效路径上不完全对称；资源单项为空仍按可选 no-op 处理。
  - `AEnemyCharacter::HandleCombatImpactFeedback()` 的音效/Niagara 调用点未新增独立 `GetWorld()` 门禁；当前 Health 回调入口未提供已复现崩溃证据，保留为低风险生命周期观察。
- **Loadout 前置证据**：用户明确确认已先通过 Editor Reference Viewer 核实旧 Loadout 在项目中为 0 引用，再置空产品引用并删除旧 Loadout 资产。当前 C++ 的 `UCombatLoadoutDefinition`、`InitialCombatLoadout`、`ActiveCombatLoadout` 与 `AssociatedLoadout` 仍是 I1 兼容镜像残留，不能把“资产零引用”误写成“源码已清退”；下一阶段单独处理源码/测试清理。
- **派生 DataAsset 决策（07B6 Fresh Review 当时）**：不在 07B6 增加 Base/Player/Enemy 派生类；当时将其保留为条件性建议。随后用户确认现有 Player/Enemy 面板已经产生明确的职责混杂与误配风险，新的接受决策见文末 `TODO-07B7` 记录。

## 阶段 Closeout（2026-09-01）

- **实现结果**：新增统一 `UCombatFeedbackDataAsset`、tier/Defense 结构和只读 tier 映射；Overlay、Player 本地 Camera Shake/受击音效、Enemy Impact Sound/Blood/Hit-Stop、Guard/Parry 防御反馈均改为读取 profile，同时保留原有运行时 owner、GAS/Damage 顺序、去重和清理语义。没有引入 GameplayCue、全局 dispatcher、第二条伤害路径或新的运行时状态持有者。
- **用户证据**：用户完成两个产品 profile 的 Editor 配置并确认 Automation 与 Scene01 PIE 通过。实际 profile 路径为 `Content/_DataAssets/Player/FeedBack/DA_CombatFeedback_Player.uasset` 与 `Content/_DataAssets/Enemy/FeedBack/DA_CombatFeedback_Enemy.uasset`；这些用户-owned `.uasset` 保留在工作区，不纳入本阶段源码/文档提交。
- **实际 Source/test 变更**：共 16 个批准路径（14 个已跟踪 Source/test 文件与新增 `CombatFeedbackDataAsset.h/.cpp`），未修改其他 Source、Config、Content、Build.cs 或 `.uproject`。本阶段工作树仍包含用户 WIP，不能作为干净 authored baseline。
- **验证边界**：用户 Automation/PIE 证据覆盖已运行的反馈入口和 Scene01 表现；profile-null 各 owner 的完整矩阵、独立手动 `PolyQuestEditor` 编译收据和更深的异步/销毁覆盖仍是非阻塞债务，按上面的 closure trigger 处理。
- **后续路线**：下一阶段安排为 `TODO-03I4：Legacy Combat Loadout Compatibility Removal v1`，随后执行已接受的 `TODO-07B7：Combat Feedback Profile Taxonomy Split v1`，再进入 `TODO-05A`；I4 只清除已无产品资产引用的 C++ 兼容镜像与测试/类残留，不改变 I1 direct route。07B7 不属于 I4 范围。
- **提交边界**：本次已获用户明确提交授权；仅暂存本阶段 16 个批准 Source/test 路径与 `plan.md`、`ARCHITECTURE.md`、`ROADMAP.md`、`ROADMAP-archive.md`，排除全部 `Content/**`（含两个用户 profile）、`Config/Automation/Presets/1.json`、旧 Loadout 资产删除差异及其他未批准 WIP。

## Post-closeout Decision：TODO-07B7 Accepted（2026-09-01）

- **触发依据**：用户确认当前统一 `UCombatFeedbackDataAsset` 的编辑器面板已将 Player 专属与 Enemy 专属字段混在一起，造成明确的配置冗余、职责辨识成本和误配风险。该 authoring 证据足以启动拆分，不再要求等待第二个敌人。
- **阶段决策**：将原先的条件性派生建议提升为正式后续阶段 `TODO-07B7：Combat Feedback Profile Taxonomy Split v1`。不回写或扩大已完成的 07B6 实现；07B6 closeout 中“当时条件性保留”的记录仍是历史事实。
- **推荐顺序**：`TODO-03I4 → TODO-07B7 → TODO-05A → TODO-05B`。I4 先完成已满足零引用前置条件的 C++/测试 Loadout 兼容层清理；07B7 随后完成反馈 Profile 类型与产品资产迁移，再进入处决阶段。
- **冻结方向**：保留 `ABaseCharacter` 的共同反馈 owner；共同基类只保存真正共享的 Overlay 等字段；Player/Enemy 派生 Profile 分别保存各自消费的音效、震屏、Defense、Impact/Blood/Hit-Stop 字段；类型不匹配、空 Profile 和空资源继续 fail-closed。不得引入全局 dispatcher、GameplayCue、第二条 Damage 路径、按敌人数量自动派生或无证据的模块拆分。
- **迁移门槛**：07B7 必须由独立计划冻结公共反射 API、资产迁移/Reference Viewer readback、transient Automation fixture、编译与 Scene01 PIE 门禁；`.uasset`/`.umap` 仍由用户在 Editor 中维护，旧统一 Profile 只能在零引用证据后删除。
