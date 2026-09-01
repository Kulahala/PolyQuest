# TODO-05A：Stagger Front Execution v1

## 阶段状态与执行路由

- **状态**：已完成。源码实现、聚焦 Automation、用户编译、修复后 Editor readback、修复后 Scene01 PIE 与 Main Fresh Review 均已通过；当前等待提交已获用户明确批准。
- **基线**：`main @ 83d1d17`（`[Refactor] 拆分战斗反馈角色 Profile (Split Combat Feedback Profiles)`）。
- **工作区边界**：工作区已有用户拥有的 `Content/**` 修改、删除和未跟踪 WIP，以及 `Config/Automation/Presets/1.json`；全部保留，不清理、不回滚、不暂存、不提交。
- **Outer**：`ue-stage-workflow`
- **Primary**：`ue5-cpp-gameplay`
- **Support**：`ue5-debug-validation`
- **Execution route**：`manual/out-of-band Gemini`
- **Contract owner**：Main/Codex。
- **Implementation writer**：Gemini 仅写入下列批准的 Source/Config/test 路径；不得改变合同、范围、所有权或验证结论。
- **用户门禁**：用户负责 Unreal Editor 资产配置、手动 `PolyQuestEditor (Development Editor)` 编译、Automation、Editor readback、PIE 和最终提交批准。

### 实施与复核收口（2026-09-02）

- **实际修改路径**：
  - `Config/Tags/PolyQuestGameplayTags.ini`
  - `Source/PolyQuest/Public/AbilitySystem/Abilities/PlayerFrontExecutionAbility.h`
  - `Source/PolyQuest/Private/AbilitySystem/Abilities/PlayerFrontExecutionAbility.cpp`
  - `Source/PolyQuest/Public/Animation/Combat/AnimNotify_PlayerFrontExecutionHit.h`
  - `Source/PolyQuest/Private/Animation/Combat/AnimNotify_PlayerFrontExecutionHit.cpp`
  - `Source/PolyQuest/Public/Character/Player/PlayerCharacter.h`
  - `Source/PolyQuest/Private/Character/Player/PlayerCharacter.cpp`
  - `Source/PolyQuest/Private/Tests/FrontExecutionAutomationTests.cpp`
- **执行与修复**：Gemini 在批准路径内完成首轮实现与两轮问题收敛，补齐 Task 启动/同步重入门禁、错误 Target 过滤、Exactly-Once 伤害断言、finite 几何合同、玩家状态前置校验和 resolver 失败后的终止清理。Main 在最终窄复核中仅在 `PlayerFrontExecutionAbility.cpp` 补齐有限 `FHitResult::Normal` 写入与非法法线 fail-closed，未改变公共 API、ASC/Tag/Input 所有权或资产范围。
- **当前证据**：用户确认 `PolyQuest.Combat.FrontExecution` Automation 为 `Success`；Test Run 3 中 resolver 失败与 Poise 恢复日志属于覆盖的 fail-closed 负路径，不构成测试失败。Gemini 报告 Rider error-level 检查无错误、`git diff --check` 通过；Main 完成一次 diff-first、一跳边界的 defect-first Fresh Review，未发现 P0/P1/P2 blocker，未进行第二轮 adversarial review。
- **用户门禁**：用户确认本阶段当前验证通过，包括手动 `PolyQuestEditor (Development Editor)` 编译、`GA_PlayerFrontExecution` 的 `Max Execution Distance = 250 cm` Editor readback，以及修复后 `/Game/Maps/Scene01` PIE。用户同时明确批准提交。
- **提交边界**：只暂存上述 8 个 Source/Config 路径、本计划、`ROADMAP.md`、`ROADMAP-archive.md` 与稳定合同更新后的 `ARCHITECTURE.md`；所有 `Content/**`、`Config/Automation/Presets/1.json` 和其他用户 WIP 保持原样排除。

## 目标与冻结决策

复用现有 `Input.PrimaryAttack`，为玩家增加魂系式正面处决入口：玩家锁定的敌人必须正处于真实 `EnemyStanceBreakAbility` 造成的 `State.Status.Stunned`，且 Poise 已归零；满足正面和距离条件时优先尝试处决，否则回退现有主手 Primary。

- Sprint Attack 保持最高优先级；1~4 技能槽、Bow Primary 和物理输入不变。
- 不监听 Parry 成功事件；Parry 或普通攻击只能削韧，只有实际 Stance Break 才能开启处决。
- 只保存一个存活目标的弱引用 reservation，不自动换目标、不重新追踪、不引入自由瞄准。
- 命中只调用现有 `FMeleeHitResolver` 和一个已配置的 Damage GameplayEffect；不得直接写 Health 或创建第二条伤害路径。
- v1 沿用普通攻击的可取消/可被 Reaction 打断语义；不新增霸体、无敌或新的 Stunned/Finisher 状态。
- 处决无新增体力/冷却合同；距离、角度、Montage 和 Damage GE 由用户资产配置，C++ 默认配置必须 fail-closed。

## GAS 与数据合同

新增 `UPlayerFrontExecutionAbility : UGameplayAbility`：

- `InstancingPolicy = InstancedPerActor`，`NetExecutionPolicy = ServerOnly`。
- `AbilityTags`：
  - `Ability.Action.Execution.Front`
  - `Ability.Action.CancelableBy.Dodge`
  - `Ability.Action.CancelableBy.Defense`
  - `Ability.Action.CancelableBy.Reaction`
  - `Ability.Action.Teardown.OnUnpossess`
- `ActivationOwnedTags`：`State.Action.Attacking`、`State.Input.Block.Movement`、`State.Input.Block.Jump`。
- `ActivationBlockedTags` 复用现有 `State.Action.Attacking`、`State.Action.Dodging`、`State.Action.Parrying`、`State.Action.Charging`、`State.Movement.Sprinting`、`State.Status.Dead`、`State.Status.Exhausted`、`State.Status.Stunned`。
- 必需字段：`ExecutionMontage`、`DamageGameplayEffectClass`、`MinExecutionDistance`、`MaxExecutionDistance`、`MaxFrontAngleDegrees`。
- 可选字段：`bUseMotionWarping`、`WarpTargetName`、`MinTriggerDistance`、`WarpStopDistance`、`MaxTriggerDistance`、`MaxWarpAngleDegrees`。
- 所有数值必须 finite；执行距离满足 `0 <= Min < Max`，正面角度为 `[0, 90]`；默认 `Max <= Min` 以拒绝未配置资产。

新增 `UAnimNotify_PlayerFrontExecutionHit`：

- 只发送 `Event.Action.Execution.Front.Hit`，设置 `Instigator = Owner`、`Target = Owner`、`OptionalObject = Animation`。
- 不应用 GameplayEffect、不修改 Health/Poise、不操作目标状态。

Gameplay Tag 配置只新增：

- `Ability.Action.Execution.Front`
- `Event.Action.Execution.Front.Hit`

## 输入路由

在 `APlayerCharacter::RequestAbilityForInputIntent()` 中保持以下不可变顺序：

1. 1~4 槽继续使用现有精确 Spec Handle。
2. `Input.PrimaryAttack` 且 `ShouldRequestSprintAttack()` 为真时，先尝试现有 Sprint 路由。
3. Sprint 条件成立但 Sprint Tag 缺失或激活失败时，继续原有普通 Primary fallback；本次输入不得尝试处决。
4. 仅在 `!ShouldRequestSprintAttack()` 且当前主手为 `UMeleeWeaponDefinition` 时，按 `Ability.Action.Execution.Front` 尝试激活。
5. GAS 接受处决激活时结束本次路由；未接受时继续 `WeaponEquipmentComponent::TryResolveInputIntent()` 的 direct Primary。
6. Bow、无主手、非法 direct route 或无锁定目标保持现有行为。

新增辅助函数和测试入口必须为私有或 `WITH_DEV_AUTOMATION_TESTS`，不得修改 `WeaponEquipmentComponent` 的 direct route 或新增 Blueprint/Shipping 输入入口。

## 激活、目标与几何

`CanActivateAbility()` 与 `ActivateAbility()` 均 fail-closed 检查：

- Player Avatar、ASC、主手近战定义、Execution Montage 和 Damage GE 有效；玩家未 Dead、Exhausted、Stunned，且没有 active Sprint。
- 使用 `GetLockedTarget()` 取得当前锁定目标的原始弱引用；不得调用会自动 retarget 的路径。
- 目标存活、未销毁、同一 World、ASC 有效且非 Invulnerable。
- 目标 ASC 拥有 `State.Status.Stunned`，`AEnemyCharacter::IsPoiseBroken()` 为真，且其 `UEnemyStanceBreakAbility` Spec 当前 `IsActive()`；只有 loose Stunned 不得通过。
- 执行距离和角度配置合法；零向量、NaN、Inf、错误 Actor/World 全部拒绝。

激活后保存一个 `TWeakObjectPtr<AEnemyCharacter>` reservation：

- 初始位置/朝向用于一次性几何和对齐；不随 Lock-On 变化重新选目标。
- `Distance2D` 使用包含边界的 `[MinExecutionDistance, MaxExecutionDistance]`。
- 目标正面由 `TargetForward2D` 与 `TargetToPlayer2D` 的夹角判定；不增加视线检测或背后规则。
- 命中帧使用目标当前 Transform 再验证 Stunned、Poise、active Stance Break、距离和正面几何。
- 处决开始后 Stance Break 在命中帧前自然结束，按预期 fail-closed 结束，不强行延长 Stunned 或补发伤害。

## 朝向、Motion-Warp 与命中

- 若 Motion-Warp 开启且现有 `FMeleeMotionWarpingLifecycle::EvaluateMeleeMotionWarpTransform()` 与 `SetMeleeMotionWarpTarget()` 均成功，以 evaluator 返回的 Transform 作为本次唯一对齐结果，不再叠加独立 Yaw 修正。
- Motion-Warp 未启用或失败时，清理 Warp Target，并直接将玩家 Yaw 一次性朝向目标；失败只降级为原地执行。
- 使用 `UAbilityTask_PlayMontageAndWait` 播放执行 Montage，使用 `UAbilityTask_WaitGameplayEvent` 接收命中事件。
- `ReadyForActivation()` 后立即检查 Ability 终态、Task UObject 和 Montage 状态，处理同步结束/取消重入。
- 命中事件必须验证 Event Tag、Avatar 身份、`OptionalObject == ActiveMontage`、reservation 有效且尚未命中过。
- 构造 `FMeleeHitRequest`：Source 为玩家及其 ASC，Damage GE 为配置值，SourceObject 为 Ability 或 Avatar；`HitResult.Actor` 为 reservation 目标，`ImpactPoint` 为目标 ActorLocation，`Normal` 为玩家指向目标的有限二维法线。
- v1 不添加 SetByCaller 执行倍率字段；伤害数值由选定 GE 本身提供。
- `bDamageEventConsumed` 保证一次执行至多尝试一次伤害；resolver 成功后继续 Montage，失败则结束且不重试。

## 生命周期与清理

绑定目标 ASC 的 Stunned/Dead Tag 变化和目标 `OnDestroyed`；任一目标失效立即结束 Ability。`EndAbility()` 必须幂等，顺序固定为：

1. 设置终态标志。
2. 注销目标 Tag/Destroyed 委托。
3. 解除并结束 Event/Montage Tasks，必要时停止 Montage。
4. 清理 Motion-Warp Target、reservation、快照和命中标志。
5. 最后调用 `Super::EndAbility()`。

目标状态恢复、目标死亡、目标销毁、玩家取消/死亡/UnPossess、Montage Completed/Interrupted/Cancelled 都走同一清理路径。不得修改目标 Poise、Stunned、移动或 Stance Break Ability。

## 批准路径与只读边界

允许新增或修改：

```text
Source/PolyQuest/Public/AbilitySystem/Abilities/PlayerFrontExecutionAbility.h
Source/PolyQuest/Private/AbilitySystem/Abilities/PlayerFrontExecutionAbility.cpp
Source/PolyQuest/Public/Animation/Combat/AnimNotify_PlayerFrontExecutionHit.h
Source/PolyQuest/Private/Animation/Combat/AnimNotify_PlayerFrontExecutionHit.cpp
Source/PolyQuest/Public/Character/Player/PlayerCharacter.h
Source/PolyQuest/Private/Character/Player/PlayerCharacter.cpp
Config/Tags/PolyQuestGameplayTags.ini
Source/PolyQuest/Private/Tests/FrontExecutionAutomationTests.cpp
```

只读参考：`EnemyStanceBreakAbility`、`EnemyCharacter`、`MeleeHitResolver`、`MeleeMotionWarping`、`WeaponEquipmentComponent` 及现有 AbilityTask/Animation Notify/近战 Ability。不得修改 `Build.cs`、现有 Stance Break、伤害解析器、Bow/Sprint Ability、`Content/**`、地图、Blueprint 或未列路径。发现额外依赖必须停止并交还 Main 决策。

## Automation 与用户验证

`FrontExecutionAutomationTests.cpp` 至少覆盖：

- 新 Tag、CDO、Instancing、NetExecutionPolicy、Owned/Blocked/Cancelable/Teardown 合同和非法默认配置。
- 正面/背面、角度边界、距离边界、零向量、NaN/Inf。
- loose Stunned、Poise 未归零、Stance Break Spec 非 active、Dead/Destroyed/Invulnerable/错误目标拒绝。
- 近战主手处决尝试、Bow 隔离、Sprint 优先、普通 Primary fallback、1~4 不变。
- 错误 Notify 身份、错误 Montage、重复事件、命中前目标失效不造成伤害；正确事件经 resolver 只造成一次 GE 效果。
- Stunned 移除、Dead、Destroyed、Montage 三种结束结果、玩家取消/死亡/UnPossess 和 Warp 清理；`ReadyForActivation()` 同步终止不恢复失效 Task。
- 无头 Task/回调 seam 明确标注 synthetic，不冒充真实动画播放。

用户在 Editor 中创建 `GA_PlayerFrontExecution`，加入 `BP_Player.StartupAbilities`，不加入武器 `BaseGrantedActions` 或 Prepared Slots；配置 authored Montage、现有 Damage GE、距离/角度和命中 Notify。启用 Motion-Warp 时配置匹配 TargetName。用户手动编译、运行 Automation、Editor readback，并在 `/Game/Maps/Scene01` PIE 验证：合法正面处决、非法条件回退普攻、Bow、Sprint、单次伤害、目标恢复/死亡/离开、Montage 中断和取消。不验证 Backstab。

## 非目标、停止条件与收口

- 不做 Backstab、通用 Finisher、敌方处决 Montage、Boss 逻辑、Parry 事件直连、第二伤害系统、多人复制或客户端权威。
- 不新增 Stunned/HyperArmor 状态，不改变现有目标状态所有权。
- 若需要修改批准路径外的公共 API、ASC/Tag/Input 所有权、伤害解析器、资产路由或 Build 依赖，立即停止并报告证据。
- Gemini 完成后只报告修改路径、静态检查、未运行的用户门禁、self-review 发现和剩余风险；不得修改文档、暂存或提交。
- Main 已完成一次独立 defect-first Fresh Review，并已同步本计划、`ROADMAP.md`、`ARCHITECTURE.md` 与历史归档；用户门禁和提交审批均已完成，不再扩大本阶段审查范围。

## 05A 后续建议与阶段拆分（不属于本阶段实施范围）

五项建议不回填到 05A v1；当前 v1 冻结的“沿用普通攻击可取消/可被 Reaction 打断”语义保持不变。

1. **05A 用户门禁已完成**：当前 v1 处决生命周期合同保持不变，后续只根据新的 PIE/产品证据决定是否改变。
2. **条件阶段 `TODO-05A1：Execution Lock-in And Recovery`**：只有 PIE 明确证明致死时后摇被截断，或产品明确要求双向锁定，才启动。该阶段单独定义玩家/目标的锁定、无敌/可取消策略、敌人死亡延迟和 Recovery 完成顺序；不在 05A 中偷偷加入新状态或霸体。
3. **`TODO-05B` 设计门**：在 Backstab 实现前先审计 `MaxExecutionDistance`、触发距离和 Warp 停止距离的真实语义；只有语义相同才合并为单一数据源，不能为减少字段而抹掉不同边界。通用 `PlayerExecutionAbilityBase` 也只在 05B 证明几何、reservation、命中与清理确实重复后提取已验证的共同生命周期，不预建通用 Finisher 框架。
4. **条件阶段 `TODO-07B8：Enemy Reaction RateWindow`**：独立接入 `EnemyStanceBreakAbility` 及需要它的受击 Montage，复用现有 `RateWindow` NotifyState/生命周期；须有真实敌人 Montage、Editor readback 和 PIE 证据，不改变 Poise、Stunned、伤害或 AI 所有权。建议在 05A/05B 稳定后、首个远程敌人前安排。
5. **条件阶段 `TODO-07B9：Execution Impact Feedback`**：在锁定/后摇语义稳定后，复用 07B7 typed feedback profile、现有 Hit-Stop/Camera Shake/音频链路，仅在执行 Notify 命中瞬间增加专属反馈；反馈失败必须是 no-op，不得影响 resolver、伤害或 `EndAbility()`。

**建议顺序**：`05A 已完成 →（若触发）05A1 → 05B（含距离设计门和按需抽象）→ 07B8/07B9 → 03C`。`07B8` 与 `07B9` 是表现支线，尚未作为本阶段或 `03C` 的硬依赖接受。
