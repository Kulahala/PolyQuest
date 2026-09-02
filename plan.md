# TODO-05A1-A：Execution Lock-in Contact Window And Paired Lock

## 阶段状态与基线

- 状态：已实施并提交；用户编译、聚焦 Automation、Scene01 PIE、Main Fresh Review 与文档收口均已完成。
- 日期：2026-09-03。
- 仓库：`E:\GameDevelop\PolyQuest`。
- 基线：`main @ 407d205853bba754cc8e2da9cfce57acb83d54c7`。
- 上一阶段：`TODO-07B8-A` 已完成，详细收口已保存在 `ROADMAP-archive.md`；其 Enemy Stance Break RateWindow 是本阶段的支撑能力，不改变本阶段边界。
- 路线：`TODO-05A1-A -> TODO-05A1-B -> TODO-05A1-C -> TODO-03C`。
- 工作树：不干净，包含用户-owned `Content/**`、资产删除/修改、`Config/Automation/Presets/1.json` 和其他未跟踪 WIP；这些内容必须保留并排除，不作为本阶段基线或提交内容。

## 阶段目标

本阶段只回答一个运行时问题：Front Execution 和 Backstab 能否在同一套 GAS 会话合同下，可靠地把玩家与目标锁定在执行接触窗口内，并在外部干扰或任意终止路径后完整恢复。

预期结果：

- 两个玩家执行 Ability 的激活距离与 Motion Warp 触发范围使用同一份 `MinExecutionDistance/MaxExecutionDistance` 配置，保留独立且必须位于窗口内的 `WarpStopDistance`。
- 玩家通过同步 Gameplay Event 请求目标侧 `EnemyVictimExecutionAbility`；目标确认后，双方持有同一个 transient execution context。
- 会话期间玩家和目标分别持有 GAS 锁定/无敌状态；目标的 CharacterMovement、导航和 StateTree 不再驱动移动或攻击。
- 普通 Melee/Projectile 伤害和普通反应不能打断或伤害会话双方；唯一允许的执行命中仍经过现有 `FMeleeHitResolver`。
- 目标侧锁定状态由目标 Ability 的 `ActivationOwnedTags` 持有，禁止玩家跨 Actor 写 Loose Tag。
- 取消、Montage/Task 失败、目标失效、死亡、Destroyed、UnPossess、AI 控制器切换和旧代际回调都 fail-closed，并释放全部委托、Task、Tag、移动锁、AI 锁、reservation、Warp 和 session。

## 工具路线与责任

- Outer：`ue-stage-workflow`。
- Primary：`ue5-cpp-gameplay`。
- Support：`ue5-state-tree-ai`。
- Route reason：这是原生 GAS Ability、共享近战命中授权和 StateTree AI 锁定组成的窄垂直切片；不需要新的全局动画系统或平行 FSM。
- Execution route：`manual/out-of-band Gemini`。
- Contract owner：Main/Codex。
- Implementation writer：Gemini，只能写下列批准路径中的冻结实现。
- Main 负责架构与合同、计划、范围决定、验证解释、Fresh Review、文档、暂存和提交；用户负责 Visual Studio 编译、Editor readback、Automation、PIE/视觉验证和最终提交批准。
- Gemini 不得修改 `plan.md`、`ROADMAP.md`、`ROADMAP-archive.md`、`ARCHITECTURE.md`、`README.md`，不得暂存或提交。

## 已确认的源码边界

- `UPlayerFrontExecutionAbility` 与 `UPlayerBackstabExecutionAbility` 都是 `InstancedPerActor`、`ServerOnly`，当前都分别保存执行距离和旧 Motion Warp 触发距离字段；本阶段只收敛这两个 Ability，不修改其他攻击消费者或共享 `FMeleeMotionWarpConfig` API。
- `FMeleeMotionWarpingLifecycle::IsConfigValid()` 仍要求 `MinTriggerDistance <= WarpStopDistance <= MaxTriggerDistance`；Front/Backstab 只把自己的执行距离映射到旧结构的触发字段。
- `UEnemyStanceBreakAbility` 拥有 `State.Status.Stunned`、Stance Break Montage、RateWindow、移动锁和 Poise 恢复；Front 交接必须先取得 Victim Lock，再取消 Stance Break，避免提前恢复移动或 Poise。
- `AEnemyCharacter::HandleDeath()` 目前会立即设置 Dead、取消全部 Ability 并启动 Ragdoll；本阶段不改变该流程，致死 Recovery 属于 `TODO-05A1-B`。
- `AEnemyAIController` 由 `UStateTreeAIComponent` 驱动，当前没有执行锁定 API；本阶段增加窄 API，不引入 Behavior Tree、Blackboard 或新的 AI 状态机。
- `FMeleeHitResolver` 当前对目标 `State.Status.Invulnerable` 一律拒绝；本阶段只增加当前 execution session 的窄授权穿透，Projectile resolver 保持不变。

## 批准变更路径

原始实施计划批准 16 个路径。实施期间经 Main 明确授权，另增加 3 个 Unity Build 回归测试修复路径与 3 个 Lock-On Retention 回归路径；本阶段实际批准范围共 22 个路径。所有用户-owned `Content/**`、`Config/Automation/Presets/1.json`、`AGENTS.md` 与其他 WIP 均排除。

1. `Source/PolyQuest/Public/Combat/Execution/ExecutionLockContext.h`
2. `Source/PolyQuest/Private/Combat/Execution/ExecutionLockContext.cpp`
3. `Source/PolyQuest/Public/AbilitySystem/Abilities/EnemyVictimExecutionAbility.h`
4. `Source/PolyQuest/Private/AbilitySystem/Abilities/EnemyVictimExecutionAbility.cpp`
5. `Source/PolyQuest/Public/AbilitySystem/Abilities/PlayerFrontExecutionAbility.h`
6. `Source/PolyQuest/Private/AbilitySystem/Abilities/PlayerFrontExecutionAbility.cpp`
7. `Source/PolyQuest/Public/AbilitySystem/Abilities/PlayerBackstabExecutionAbility.h`
8. `Source/PolyQuest/Private/AbilitySystem/Abilities/PlayerBackstabExecutionAbility.cpp`
9. `Source/PolyQuest/Public/Combat/Melee/MeleeHitResolver.h`
10. `Source/PolyQuest/Private/Combat/Melee/MeleeHitResolver.cpp`
11. `Source/PolyQuest/Public/AbilitySystem/Abilities/EnemyStanceBreakAbility.h`
12. `Source/PolyQuest/Private/AbilitySystem/Abilities/EnemyStanceBreakAbility.cpp`
13. `Source/PolyQuest/Public/AI/EnemyAIController.h`
14. `Source/PolyQuest/Private/AI/EnemyAIController.cpp`
15. `Config/Tags/PolyQuestGameplayTags.ini`
16. `Source/PolyQuest/Private/Tests/ExecutionLockInAutomationTests.cpp`

实施期间 Main 授权的回归修复路径：

17. `Source/PolyQuest/Private/Tests/FrontExecutionAutomationTests.cpp`（Unity Build 匿名命名空间符号冲突）
18. `Source/PolyQuest/Private/Tests/BackstabExecutionAutomationTests.cpp`（Unity Build 匿名命名空间符号冲突）
19. `Source/PolyQuest/Private/Tests/EnemyStanceBreakRateWindowAutomationTests.cpp`（Unity Build 匿名命名空间符号冲突）
20. `Source/PolyQuest/Public/Character/Player/PlayerCharacter.h`（仅 `WITH_DEV_AUTOMATION_TESTS` 的 Lock-On 测试 seam）
21. `Source/PolyQuest/Private/Character/Player/PlayerCharacter.cpp`（执行态 Lock-On Retention 豁免）
22. `Source/PolyQuest/Private/Tests/PlayerLockOnAutomationTests.cpp`（成对锁定与 Invulnerable 门禁回归）

需要修改未列路径、Enemy Blueprint StartupAbilities、任何其他 Tag/Input/Config、Build.cs、资产或文档时，Gemini 必须停止并返回 Main 做范围决策，不得绕过；上述 17-22 项仅由 Main 在审查/修复环节授权。

## 实施合同

### 1. Shared execution context

新增 transient `UExecutionLockContext`。对象由发起方 Player Ability 以自身为 Outer 创建；Front、Backstab 和 Victim Ability 都以 `UPROPERTY(Transient) TObjectPtr<UExecutionLockContext> ActiveExecutionContext` 强持有同一对象，避免会话期间被 GC。context 内部只保存 source/victim Actor、Ability/ASC 的弱引用、request tag、source activation token，以及 accepted/active/release 状态。

必须提供并实现以下语义：

- 初始化当前 source Player Ability、source Actor/ASC、目标 Actor；
- `AcceptVictim(...)` 只允许匹配当前 source、target、request 和 token 的目标 Ability 接受一次；
- `IsCurrent(...)` 验证 session 仍 active、双方对象有效且代际匹配；
- `IsHitAuthorized(...)` 同时验证 context 指针、source Ability/`SourceObject`、source Actor、source ASC、target Actor、target ASC、active Victim Ability 和 token；
- `InvalidateSession()` 幂等地关闭会话，使所有迟到 Hit/Release/Delegate fail-closed。

不得让 Player 直接向 Enemy 写 Loose Tag。任何一方结束 Ability 都必须使 context 失效；另一方的清理不得重新激活旧 context。

### 2. Enemy victim Ability

新增 `UEnemyVictimExecutionAbility`，固定 GAS 合同：

- Ability tag：`Ability.Action.Execution.Victim`；
- Gameplay Event triggers：`Event.Action.Execution.Request.Front`、`Event.Action.Execution.Request.Backstab`；
- Release listener：`Event.Action.Execution.Release`；
- `ActivationOwnedTags`：`State.Action.Execution.VictimLocked`、`State.Status.Invulnerable`、`State.Status.Stunned`、`State.Input.Block.Movement`、`State.Input.Block.Jump`；
- `Ability.Action.Teardown.OnUnpossess` 必须属于 Ability 的 teardown selector；
- `ActivationBlockedTags` 至少包含 `State.Status.Dead` 和 `State.Action.Execution.VictimLocked`，不能用 `State.Status.Stunned` 阻断自身 Front 请求。

请求校验必须验证 EventTag、Instigator、Target、World、context 类型、会话唯一性、目标存活及当前状态：

- Front 请求要求目标有真实 active `EnemyStanceBreakAbility` 且 Poise 为零；
- Backstab 请求要求目标存活、非 `Stunned`，并沿用现有 Backstab 激活时后方几何快照合同；
- 目标已有其他 Victim Lock、Dead、Destroyed、跨 World 或 context 不匹配时拒绝。

接受顺序固定为：取得 Victim Lock → 调用目标 `CharacterMovementComponent->StopMovementImmediately()` 并禁用移动 → 调用 `AEnemyAIController::BeginExecutionLock()`（无 Controller 时允许继续但不能伪造 AI accepted）→ 取消允许交接的敌人攻击/反应 Ability → 标记 context accepted。任何失败都不得向 Player 返回 accepted。

本阶段目标侧采用 lock-only fallback，不播放新受害 Montage。Victim `EndAbility()` 必须幂等地处理 Release、取消、Destroyed、Dead、UnPossess 和 context 失效，清理移动/AI 锁、Owned Tags、Tasks 与 context；死亡或 UnPossess 后不得恢复 AI。Front 交接中若 Stance Break 已被取消，Victim 在最终 Release 且目标仍存活时只调用一次 `RestorePoiseToMax()`。

### 3. Enemy AI execution lock

在 `AEnemyAIController` 增加窄 API：

- `BeginExecutionLock()`；
- `EndExecutionLock()`；
- `IsExecutionLocked()`。

Begin 必须保存锁前是否运行 StateTree、同一控制 Pawn、当前焦点和必要的移动状态，然后停止 StateTree、路径请求、移动和焦点驱动。锁定期间，以下入口不得重新驱动 AI 或发起 MoveTo：`Tick`、`UpdateControlRotation`、感知/目标重验证、`TryRequestCooldownReposition`、`TryRequestApproach`、`PreparePendingAttackProfile`、`TryRequestMeleeAttack`、`BeginAlert`、`OnMoveCompleted` 及其直接移动调用链。清理入口、死亡处理和 `OnUnPossess` 仍必须可执行。

End 只在同一 Pawn 仍存活且仍由该 Controller 控制时恢复；只有锁前 StateTree 确实运行时才重新启动。死亡或 UnPossess 不得重启 AI。所有访问 Pawn、StateTree、Movement 和焦点的路径都必须先判空/判有效。

### 4. Front / Backstab integration

Front 与 Backstab 都采用以下配置合同：

- `MinExecutionDistance = 0.0f`；
- `MaxExecutionDistance = 250.0f`；
- `WarpStopDistance = 190.0f`；
- 旧 `MinTriggerDistance/MaxTriggerDistance` 不再作为这两个 Ability 的反射配置来源；Motion Warp 构造临时旧结构时分别映射为 `MinExecutionDistance/MaxExecutionDistance`；
- `WarpStopDistance` 必须满足 `MinExecutionDistance <= WarpStopDistance <= MaxExecutionDistance`，无效配置或 Warp 设置失败继续使用已有 Yaw fallback；
- 不修改 Light、Charged、Sprint、Melee Skill、Dodge 或共享 Motion Warp 结构。

两者都增加 `State.Action.Execution.PlayerLocked` 与 `State.Status.Invulnerable` 的 Ability-owned 生命周期、当前 execution context、activation token、目标锁定委托和完整异步代际检查。Front 新增与 Backstab 对称的 per-activation callback proxy；所有 Montage、Hit、Target、Tag 回调先检查 token、context、`IsActive()` 和终态标志。移除普通 `CancelableBy.Dodge/Defense/Reaction` 标记，保留 `Teardown.OnUnpossess`；Montage 确认播放后继续调用既有 `CancelActiveGuardAfterConfirmedAction(true)`。

激活顺序固定为：

1. 现有自身、武器、目标、几何和配置前置校验；目标不能已被 Victim Lock 或 Invulnerable。
2. 保留 Front/Backstab 当前 Commit 与输入路由合同，不新增 Cost 或新的输入路径。
3. 创建并强持有 context，递增当前 activation token。
4. 向目标 ASC 发送对应 Request Gameplay Event。
5. 必须在同一调用栈内确认 Victim accepted；失败立即 EndAbility 并让输入回退既有 direct Primary。
6. 设置 Motion Warp；失败时清除 Warp 并执行既有一次性水平 Yaw 对齐。
7. 创建并绑定 Hit/Montage Tasks。
8. 调用 `ReadyForActivation()` 后立即检查 Ability、context、Task 有效性和终态；确认 Montage active。
9. Montage 确认后取消 Guard。

命中阶段继续复验存活、距离、Payload、当前 Montage、source/target 和 Exactly-Once；不重新判定 Backstab 的目标朝向快照。命中请求必须填写当前 `ExecutionContext`，resolver 成功后不得重试。

`EndAbility()` 顺序固定为：标记终态 → 向仍有效的目标发送带 context 的 Release → 立即失效 callback proxy 并解绑目标委托 → 结束 Tasks、停止自有 Montage、清理 Motion Warp/reservation → `InvalidateSession()` 并复位瞬态字段 → `Super::EndAbility()`。任何分支都不能在 `Super::EndAbility()` 后继续访问旧 Task、Actor 或 context。

### 5. Melee resolver authorization

在 `FMeleeHitRequest` 增加可选字段：

```cpp
const UExecutionLockContext* ExecutionContext = nullptr;
```

`FMeleeHitResolver::TryResolveHit()` 保持现有自伤、队伍、Dead、Effect、Guard/Parry 和单一伤害入口合同，只收窄 Invulnerable 分支：

- 普通 Melee、Projectile 和错误执行请求仍拒绝目标 `State.Status.Invulnerable`；
- 只有 `ExecutionContext->IsHitAuthorized(...)` 成功时，当前执行命中才可穿透目标 Victim 的 Invulnerable；
- 不修改 `FCombatProjectileHitResolver`，不直接写 Health/Poise，不增加第二条伤害路径。

### 6. Stance Break handoff

在 `UEnemyStanceBreakAbility` 增加 `State.Action.Execution.VictimLocked` 读取和激活阻断。其现有退出顺序仍为先恢复 RateWindow、再停止 Montage、再清理 Task；若 Victim Lock 仍存在，退出时不得恢复移动或 Poise，交由 Victim Ability 在最终 Release 负责。

### 7. Gameplay Tags

仅在 `Config/Tags/PolyQuestGameplayTags.ini` 注册：

- `Ability.Action.Execution.Victim`
- `State.Action.Execution.PlayerLocked`
- `State.Action.Execution.VictimLocked`
- `Event.Action.Execution.Request.Front`
- `Event.Action.Execution.Request.Backstab`
- `Event.Action.Execution.Release`

不得新增跨 Actor Loose Tag 写入、全局动画监听器、全局 Time Dilation 或第二套状态机。

## Automation 与用户验证

新增 `Source/PolyQuest/Private/Tests/ExecutionLockInAutomationTests.cpp`。测试必须使用真实 GAS 授予/激活和 `HandleGameplayEvent` 路径；测试 seam 只能置于 `WITH_DEV_AUTOMATION_TESTS`，不得绕过 session 授权、锁定或清理。

至少覆盖：

- Victim Ability CDO、触发器、Owned/Blocked/Teardown Tag 合同；
- Front/Backstab 共用执行距离，默认 Max `250`，WarpStop 边界与 Warp 失败 Yaw fallback；
- Front StanceBreak → Victim Lock 的同步交接，StanceBreak 不提前恢复移动/Poise；
- Backstab 非 Stunned 目标的同步交接；
- 双方 `PlayerLocked/VictimLocked/Invulnerable`、目标速度清零、移动禁用、AI/StateTree 暂停；
- 普通 Melee/Projectile 对双方的外部伤害阻断；
- 当前 context 执行命中可通过，错误/旧 context、错误 source/target、Dead 目标被拒绝；
- Task/Montage 启动失败、取消、中断、目标 Tag 丢失、Destroyed、Dead、Player/Enemy UnPossess；
- session A 结束后 session B 激活，A 的迟到 Hit/Release/Delegate 不得影响 B；
- Release 后无 Tag、移动、AI、Poise、Task、委托、reservation 或 Warp 泄漏；
- 回归运行 `PolyQuest.Combat.FrontExecution`、`PolyQuest.Combat.Backstab`、`PolyQuest.Combat.EnemyStanceBreakRateWindow`、`PolyQuest.Combat.HitReaction`。

用户门禁：

1. 在 Visual Studio 2022 手动编译 `PolyQuestEditor (Development Editor)`。
2. 在 Editor readback 中把 `UEnemyVictimExecutionAbility` 加入敌人 Blueprint 的 `StartupAbilities`；确认 Front/Backstab 的 Max Execution Distance 为 `250`、WarpStop 为 `190`，且旧 Trigger 距离不再作为来源。
3. 在 `/Game/Maps/Scene01` PIE 验证 Front/Backstab 均能吸附并锁住敌人；敌人不移动、不重新进入 AI，双方不受外部 Melee/Projectile 伤害，Release 后恢复移动/AI。
4. 不把本阶段结果描述为 `DeathPending`、致死 Recovery、延迟死亡/Ragdoll、Launch/Knockback 或双人受击 Montage 已完成。

## 阶段实施与验证收口

- **实际结果**：实现 `UExecutionLockContext`、`UEnemyVictimExecutionAbility` 与 `AEnemyAIController` 执行锁；Front/Backstab 通过同步 Gameplay Event 完成成对握手，双方的 `PlayerLocked`/`VictimLocked`/`Invulnerable` 和移动/AI 锁由各自 Ability/Controller 所有；执行命中仍唯一经过 `FMeleeHitResolver`，仅当前 session 获得无敌穿透授权。Front 的 Stance Break 交接、目标速度清零、Release 清理和 Lock-On Retention 均已接入。
- **实际变更路径**：上方 22 个批准路径全部有对应实现或回归修复；未纳入任何 `Content/**`、产品 `.uasset/.umap`、`Config/Automation/Presets/1.json`、`AGENTS.md` 或其他用户 WIP。
- **用户验证**：用户确认 `PolyQuestEditor (Development Editor)` 编译通过，并确认 `PolyQuest.Combat.ExecutionLockIn`、`PolyQuest.Combat.FrontExecution`、`PolyQuest.Combat.Backstab`、`PolyQuest.Player.LockOn` Automation 均为 `Success`；用户确认 `/Game/Maps/Scene01` PIE 中 Front/Backstab 吸附、成对锁定、外部伤害阻断及 Release 恢复符合预期。Main 未重复启动 Editor、PIE 或 Automation。
- **静态检查**：Main 对本阶段新增/修改 C++ 执行 Rider error-level 检查（0 errors），并执行 `git diff --check`（通过）。Gemini 的实现报告另记录了批准路径静态检查通过。
- **Fresh Review**：Main 按 `ue-strict-review` 完成 diff-first 常规 Fresh Review；先使用 `code-review-graph` 读取基线一致的变更/影响半径，再针对 Lock-On/候选过滤调用一次定向 CodeGraph，随后核对一跳调用与 Front/Backstab 任务启动终态。未发现 P0、P1 或 P2；未执行额外对抗性轮次。
- **日志观察项**：测试输出中的 `State.Status.Stunned` replication-state 提示与 CDO `CanActivateAbility` invalid Handle 日志没有形成可复现的 P0-P2 行为证据；暂记为观察项，不扩大本阶段范围。
- **后续债务**：当前终端死亡仍沿用 `Dead -> CancelAllAbilities -> Ragdoll`，尚未提供 `DeathPending`、延迟死亡、处决后 Recovery 或非致死 Launch/Knockback；下一阶段 `TODO-05A1-B` 必须先冻结这些契约，并把执行期间的 DeathPending Lock-On 保持与死亡交接一并验证。Enemy Victim Ability 的产品资产作者化仍依赖用户维护的 Content 基线，不作为本次源码提交的干净检出复现承诺。
- **提交边界**：提交候选仅为上述 22 个 Source/Config/test 路径及 Main 收口文档 `plan.md`、`ROADMAP.md`、`ROADMAP-archive.md`、`ARCHITECTURE.md`、`README.md`；明确排除所有用户资产和其他 WIP。

## 非目标、风险与文档影响

本阶段不做：`DeathPending`、致死后玩家 Recovery、延迟 `Dead -> CancelAllAbilities -> Ragdoll`、Launch/Knockback、受害 Montage、Camera/HitStop/Audio、通用 `PlayerExecutionAbilityBase`、Player RateWindow 重构、全局动画监听、多人复制/预测、Build.cs 变更或手动编辑 `.uasset/.umap`。

若敌人 Blueprint 未授予 Victim Ability，Request 必须失败并回退既有普通攻击；不得从 C++ 隐式授予。当前即时死亡流程仍可能在命中后立即取消双方，这是 `TODO-05A1-B` 的明确后续合同。

收口完成后的文档记录：

- Main 已将稳定的 execution context、Victim Ability、AI 锁定、resolver 授权和 Lock-On retention 所有权写入 `ARCHITECTURE.md`；
- 已将 `TODO-05A1-A` 标为完成、`TODO-05A1-B` 设为下一阶段；
- 已把当前终端死亡与 DeathPending/Release 后表现缺口写入 `ROADMAP.md` 的验证债务条目；
- 本阶段详细证据保留在本文件，历史收口已追加到 `ROADMAP-archive.md`。

## Gemini Handoff Prompt

以下代码块保留实施前的原始 handoff；实际实施与修复范围以本文件上方记录的 22 个批准路径和收口证据为准。

```text
你是 PolyQuest 的 Implementation Executor（Gemini）。

cwd: E:\GameDevelop\PolyQuest
baseline: main @ 407d205853bba754cc8e2da9cfce57acb83d54c7
active plan: E:\GameDevelop\PolyQuest\plan.md

请先读取 AGENTS.md、plan.md，并按 ue-stage-workflow、ue5-cpp-gameplay、
ue5-state-tree-ai 执行。Contract owner: Main；implementation writer: Gemini。

只允许修改 plan.md 中列出的 16 个批准路径。不得修改 PlayerCharacter 输入路由、
其他 Motion-Warp 消费者、Projectile resolver、EnemyCharacter 死亡/Ragdoll、
RateWindow helper、Build.cs、资产文件、ROADMAP/ARCHITECTURE/README 或提交。
任何未列出的文件、Tag、Input、Config、公共 API、资产授予或生命周期规则需求，
必须停止并返回 Main，不得绕过范围。

必须保持：GAS ServerOnly/InstancedPerActor；单一 FMeleeHitResolver 伤害入口；
目标 Ability 持有锁定 Tag；无跨 Actor Loose Tag；既有 Front/Backstab 几何快照、
输入顺序、队伍和 Cost 合同；普通命中继续被 Invulnerable 拒绝，只有当前 execution
context 授权的执行命中可穿透。

UExecutionLockContext 由 Player Ability 创建并以自身为 Outer；Front、Backstab、
Victim 都必须以 UPROPERTY(Transient) TObjectPtr 强持有同一对象。任意一方
EndAbility 都要先发送合法 Release（目标仍有效时），使 callback/context 失效，
再清理 Task、Montage、Warp、reservation、移动/AI 锁，最后调用 Super::EndAbility。
Victim 接受前必须 StopMovementImmediately，并完成 BeginExecutionLock；死亡或
UnPossess 后不得恢复 AI。Resolver 只有 IsHitAuthorized 的当前会话可穿透 Invulnerable。

执行顺序：ExecutionLockContext/Tags -> Victim Ability/AI lock -> Front/Backstab
对称 callback 与距离映射 -> resolver 授权/StanceBreak 交接 -> Automation。
测试必须走真实 GAS 授予/激活与 HandleGameplayEvent；测试旁路只能放在
WITH_DEV_AUTOMATION_TESTS，不能绕过锁定、授权或清理。

完成后只做批准范围内的源码静态检查与 git diff --check；不要运行 UBT、Editor、
PIE 或提交。完成报告必须列出实际改动路径、关键生命周期顺序、静态/Automation
结果、未运行的用户门禁、剩余风险，并进行严格实施自查；自查不能替代 Main 的
独立 Fresh Review。
```
