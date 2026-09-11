# TODO-07B11-RET: Enemy Legacy Launch Retirement v1

## 1. 阶段定位、基线与路由

- Target Objective：删除 UEnemyLaunchReactionAbility 中已被 grounded Root Motion 路径取代的 Physics Launch 兼容分支及其 Enemy-only reflected/test surface，使 Enemy Launch Reaction 只接受有效 grounded Root Motion knockdown。Root candidate 无效或 Montage 启动失败时一律 fail-closed，不得恢复 Physics fallback。
- 阶段关系：TODO-07B11 已在 ROADMAP-archive.md 收口；TODO-07B13-RET 的历史收口已追加归档，Archive Preflight 为 PASS。本切片只退休 Enemy Legacy Launch；不回溯 Player，也不启动条件性 TODO-07B14。
- 基线：main @ aab8ef630ec49575fff3d82f0e0fd845bf7d44f1（2026-09-11）。工作树仍含用户保留的 Content/**、Config/**、Blueprint、AnimBP、Montage、地图、导入资源与其他 WIP；它们不属于本阶段，必须保留。
- 已有源码证据：Enemy Ability 当前同时拥有 RootMotionKnockdown 与 Takeoff -> TurningToLaunch -> AwaitingAirborne -> Airborne -> LandingRecovery Physics 分支。后者独占 LaunchCharacter、Commit listener、FLaunchFacingSmoothingState、UAbilityTask_TurnToFacing、fall watchdog、Takeoff/Landing Recovery Montage 及速度/转向字段；Root 分支已拥有 AI stop、一次性 facing、StopMovementImmediately()、CMC ledge guard、State.Block.Facing、stance-break deferral 和幂等 EndAbility()。
- 已有路线图证据：用户此前 readback 的 Enemy CDO 已按 Root Motion-only 作者化，但该事实不足以替代本阶段对所有实际 Enemy Launch 资产与派生类的独立 readback。
- 技能路由：
  - Outer: ue-stage-workflow
  - Primary: ue5-cpp-gameplay
  - Support: none
  - Route reason: 这是 GAS UGameplayAbility 的窄生命周期退休，必须精确保留 ASC-owned tags、CMC/ledge、AI stop、Poise/stance-break handoff 与异步任务收敛。
- 执行路线：manual/out-of-band Gemini。Implementation executors: 1（Gemini）；in-app delegation: 0。Main（Codex）拥有架构、计划、用户验证解释、Fresh Review、文档、暂存和提交；Gemini 不得提交。首次实施交付前，Gemini 必须安排一名干净只读子代理完成实施自审；后续窄修复回路不得再派发子代理。

## 2. 目标、完成标准与刻意非目标

### Target Objective

1. 将 Enemy Ability 生命周期收敛为 None -> RootMotionKnockdown -> EndAbility，删除 dormant Physics Launch、paused Takeoff、真实 Falling、Landing Recovery 和平滑转向链路。
2. 删除已失去分支选择意义的 bUseGroundedRootMotionKnockdown。Root Motion、Slot、有效有限播放长度和精确 MOVE_Walking 是唯一合法 candidate；该开关不再替代动画资产的 Root Motion 事实来源。
3. 保留 Enemy Root Motion 的既有行为顺序与所有权：AI navigation stop、速度清零、竞争 Ability cancel、残余 Root Motion fail-closed、一次性 target-local facing、ledge 保存/恢复、MovementMode/Montage delegate、ReadyForActivation() 重入保护、State.Block.Facing 生命周期和 Stance Break deferral。
4. 将 Enemy Launch 自动化从 Legacy fallback 正向用例改为 Root-only invalid-config fail-closed、Commit 无副作用、自然/打断/取消/Destroy/UnPossess 收敛和 deferral 回归。

### Completion Criteria

- ELaunchPhase 仅保留 None 与 RootMotionKnockdown；没有 Takeoff、Turning、AwaitingAirborne、Airborne 或 LandingRecovery phase。
- Enemy Ability 不再声明、读取、复制、验证、播放或停止 TakeoffMontage、LandingRecoveryMontage、LaunchHorizontalSpeed、LaunchVerticalSpeed、FacingTurnRateDegreesPerSecond 或 bUseGroundedRootMotionKnockdown。
- Enemy Ability 不再创建或清理 CommitEventTask、FacingTurnTask、FallValidationTask，不再调用 LaunchCharacter()、Montage_Pause() 或 UAbilityTask_WaitDelay。
- Enemy Ability 不再请求、校验或监听 LaunchCommitEventTag，不再拥有 FLaunchFacingSmoothingState、bCommitHandled、OnLaunchCommitEventReceived、OnFacingTurnCompleted、OnFacingTurnFailed、CommitFrozenLaunch、OnFallValidationFinished、IsLegacyLaunchCandidate 或 IsEventFromTakeoffMontage。
- Root candidate 必须同时具备：非空 RootMotionKnockdownMontage、HasRootMotion()、至少一个 Slot track、有限且大于零的播放长度，以及 CMC 精确 MOVE_Walking。任一条件失败时，在创建 Task、Commit、停止 AI、修改 velocity/rotation/ledge 或绑定 delegate 前拒绝激活。
- Root Montage 自然结束将 bRootMotionKnockdownCompletedNaturally 置真并走 CompleteLaunchStanceBreakDeferral()；非自然结束、取消、Falling、Destroy、死亡或 UnPossess 走 AbortLaunchStanceBreakDeferral()。旧 bLandingRecoveryCompletedNaturally 及其 GetTestLandingRecoveryCompletedNaturally / SetTestLandingRecoveryCompletedNaturally seam 必须分别改为 bRootMotionKnockdownCompletedNaturally、GetTestRootMotionKnockdownCompletedNaturally 与 SetTestRootMotionKnockdownCompletedNaturally，不能残留为 Root 语义。
- ActivationOwnedTags 中的 State.Action.HitReacting 与 State.Block.Facing、Ability.Action.Teardown.OnUnpossess、既有 blocked tags、AbilitiesToCancel 的两个 Enemy action tag 与其数量契约均不变。
- 用户确认 Development Editor 编译、指定 Automation、Editor readback 和 Enemy Scene01 PIE；Main Fresh Review 无 P0/P1/P2 blocker。

### Deliberate Non-goals

- 不修改 Player Launch、Dodge、Player Lock-On、Player/Enemy Air-Reaction policy、Motion Warping、Tech-Roll、网络/预测、死亡架构或 TODO-07B14。
- 不修改 AEnemyAIController、AEnemyCharacter、UEnemyStanceBreakAbility、UEnemyMeleeAbility、UEnemyHitReactionAbility、State.Block.Facing 的生产所有权或 AbilitiesToCancel.Num() == 2/4 的既有契约。
- 不删除或修改全局 Event.Reaction.Launch.Commit Tag、UAnimNotify_ReactionLaunchCommit、FLaunchFacingSmoothingState、UAbilityTask_TurnToFacing、LaunchFacingSmoothingAutomationTests 或 test-only host ability。它们可能仍被用户资产或独立测试引用；本切片只移除 Enemy Ability 对它们的消费，不假设全局零引用。
- 不编辑、迁移、重存或删除任何 .uasset、.umap、Blueprint、AnimBP、Montage、地图、Config、Build.cs 或 uproject；不自动移除旧 Commit Notify。
- 不引入新 Gameplay Tag、State.Block.Movement、平行动作状态机、通用 Reaction framework 或新的测试框架。

## 3. Approved Paths 与所有权

### Gemini 可修改

1. Source/PolyQuest/Public/AbilitySystem/Abilities/EnemyLaunchReactionAbility.h
   - 删除 Enemy Legacy UPROPERTY、Legacy enum phase、Legacy tasks/tags/state/callback/helper 和只服务它们的 test seams/getter/setter；删除 bUseGroundedRootMotionKnockdown。
   - 将自然完成的私有状态/test seam 从 Landing Recovery 语义收敛为 Root Motion knockdown 语义；保留 Root-only test seams、State.Block.Facing、BoundEnemyCharacter、ledge 与 stance-break 所需状态。
2. Source/PolyQuest/Private/AbilitySystem/Abilities/EnemyLaunchReactionAbility.cpp
   - 删除 Legacy activation block、Commit/Facing/Falling/Landing 生命周期、Physics cleanup 和 OR Legacy validation；将既有 Root branch 平铺为唯一分支，但不得改变其既有副作用顺序或 EndAbility() 单一收敛出口。
   - 删除已不再需要的 AbilityTask_WaitGameplayEvent、AbilityTask_WaitDelay、AbilityTask_TurnToFacing、Animation/AnimSequenceBase.h 和 smoothing include；其中 AnimSequenceBase 只服务于已退休的 IsEventFromTakeoffMontage。保留 Root Montage、AI/CMC、impact resolver、Montage/MovementMode delegate 和 EndAbility 的防御检查。
3. Source/PolyQuest/Private/Tests/EnemyLaunchReactionRootMotionAutomationTests.cpp
   - 删除 Legacy fallback、switch-disabled、Takeoff/Landing fixture 和直接 Commit callback 测试；新增/保留 Root-only candidate、激活、Commit event 无副作用、phase/ledge/facing/stance-break、自然/中断/取消/Falling/Destroy/UnPossess 覆盖。
   - 不在匿名命名空间重新声明会污染 Unity blob 的 gameplay tag；现有局部 TagBlockFacing 与 TagTeardownOnUnpossess 作用域保持局部。
4. Source/PolyQuest/Private/Tests/HitReactionAutomationTests.cpp
   - 仅删除 Enemy Launch 已退休的 horizontal speed、vertical speed 和 facing turn-rate CDO 断言。
   - 保留全局 Event.Reaction.Launch.Commit Tag 与 UAnimNotify_ReactionLaunchCommit CDO 测试，因为它们不在本切片删除范围。

### Main-only

- plan.md、ARCHITECTURE.md、ROADMAP.md、ROADMAP-archive.md。
- 用户门禁完成且 Fresh Review 通过后，Main 才能处理文档、暂存和提交。

### Explicitly Excluded

- Source/PolyQuest/Public/AbilitySystem/Abilities/LaunchFacingSmoothingState.h
- Source/PolyQuest/Private/AbilitySystem/Abilities/LaunchFacingSmoothingState.cpp
- Source/PolyQuest/Public/AbilitySystem/Tasks/AbilityTask_TurnToFacing.h
- Source/PolyQuest/Private/AbilitySystem/Tasks/AbilityTask_TurnToFacing.cpp
- Source/PolyQuest/Public/Animation/Combat/AnimNotify_ReactionLaunchCommit.h
- Source/PolyQuest/Private/Animation/Combat/AnimNotify_ReactionLaunchCommit.cpp
- Source/PolyQuest/Private/Tests/LaunchFacingSmoothingAutomationTests.cpp
- Source/PolyQuest/Private/Tests/TestLaunchFacingSmoothingAbility.h
- Source/PolyQuest/Private/Tests/TestLaunchFacingSmoothingAbility.cpp
- Source/PolyQuest/Private/Tests/EnemyRootMotionFacingAutomationTests.cpp
- Source/PolyQuest/Private/Tests/EnemyStanceBreakRateWindowAutomationTests.cpp
- Source/PolyQuest/Private/Tests/CombatHitFeedbackAutomationTests.cpp
- Source/PolyQuest/Private/Character/Enemy/EnemyCharacter.cpp、Source/PolyQuest/Private/AI/EnemyAIController.cpp、所有其他 Source、Content/**、Config/**、Build.cs、uproject 和全部二进制资产。

若编译、Automation 或 Unity Build 暴露清单外文件，Gemini 必须停止，保留首个错误并报告完整触发条件、最小候选修复路径与影响；不得创建 hygiene 例外、修改清单外文件或扩大白名单。

## 4. 冻结的运行时与 API 契约

### Root-only Enemy activation

- CanActivateAbility() 继续调用 Super 和 ValidateActivationSetup()；ValidateActivationSetup() 从 common Enemy/ASC/AnimInstance/CMC/tag/cancel-count 验证加唯一 Root candidate 构成，绝不保留 Root || Legacy。
- RootMotionKnockdownMontage 仍是唯一作者化 UPROPERTY。动画资产上的 Root Motion 与 Slot 是运动表现的唯一事实来源，不以 bool policy 开关覆盖。
- 既有 Root 活化顺序必须保持：创建 Montage Task -> CommitAbility() -> 绑定 avatar/AnimInstance/montage/phase -> 停 AI navigation 与速度 -> cancel Enemy melee/small hit -> 检查残余 Root Motion -> 解析并一次性写 yaw -> 保存/关闭 ledge walk-off -> 绑定 Montage/MovementMode -> ReadyForActivation() -> 立即检查 bEndAbilityRequested 和 montage active。
- ReadyForActivation() 是同步重入边界。任何同步 EndAbility() 后只允许立即返回；一旦 bLedgeSettingModified 为真，后续所有失败出口必须通过 EndAbility() 恢复 CMC 值。
- Root phase 任何非 MOVE_Walking 变化均 fail-closed；CMC 继续拥有碰撞、墙、台阶、坡面、边缘和 Falling 的物理权威，不承诺 Root Motion 绕过胶囊。

### GAS、AI、Stance Break 与 cleanup

- Enemy Ability 仍为 InstancedPerActor / ServerOnly，由 Ability.Reaction.Enemy.Launch 的 Event.Reaction.Enemy.Launch trigger 启动；不改变 ASC 归属。
- State.Block.Facing 仍由该 Ability 的 ActivationOwnedTags 覆盖整个 Root lifecycle；AEnemyAIController 仍消费该 GAS tag，而不是旧的 Launch-specific active-spec 查询。
- Ability.Action.Teardown.OnUnpossess 继续位于 AbilityTags，且 UnPossess 通过既有 ASC cancellation 收敛；不改变 UEnemyStanceBreakAbility 对 Enemy Launch tag 的 cancel relationship。
- AEnemyCharacter::BeginLaunchStanceBreakDeferral()、CompleteLaunchStanceBreakDeferral() 和 AbortLaunchStanceBreakDeferral() 的所有权不移动。只把 Enemy Ability 的自然完成标记改为与 Root Montage 名称一致，确保状态完成/中断仍选择原有 handoff。
- EndAbility() 保留 bEndAbilityRequested 幂等保护，解绑 delegate、停止本 Ability 的 Root Montage、结束 Montage Task、恢复 ledge、清空 snapshot/phase/weak owner，并只在有效未销毁 Enemy 上调用 deferral complete/abort。Destroy、死亡、取消与 UnPossess 后的迟到回调不得解引用无效 owner。

### Commit 与共享 Legacy facilities

- Enemy Root Ability 不再创建 UAbilityTask_WaitGameplayEvent 来消费 Event.Reaction.Launch.Commit；实际 ASC->HandleGameplayEvent() 的同名事件在 Root phase 不得暂停 Montage、发射速度、切 phase 或结束 Ability。
- Event.Reaction.Launch.Commit、Notify class、tag 配置、smoothing struct 和 turn task 保持不变。Montage 仍含 Commit Notify 时，这是可记录但非阻塞的 no-op authored residue；不得在本切片删除 Notify 或资产。
- 本阶段的 zero-reference 只要求 Enemy Ability Header/CPP 与批准的 Enemy Launch/HitReaction tests 中不存在 retired Enemy symbols。全局 shared/test-only references 是刻意允许残留，不能误判为本切片失败。

## 5. 用户拥有的 Editor readback（Source 实施前硬门禁）

Gemini 开始 Source 修改前，用户在 Unreal Editor 中完成只读检查并反馈结果；Codex/Gemini 不直接编辑二进制资产。

1. 定位实际使用 UEnemyLaunchReactionAbility 的 Enemy Launch Gameplay Ability 及所有派生/覆写资产，记录实际 package path 与父类链。
2. 对每个实际 CDO/覆写检查：RootMotionKnockdownMontage 已配置且可用；bUseGroundedRootMotionKnockdown 为 true；TakeoffMontage、LandingRecoveryMontage、Launch speed 与 facing turn rate 没有需要保留的非默认作者化值。
3. 打开实际 Root Montage/Sequence，确认 Root Motion、可播放 Slot 和有限正长度；记录是否仍含 Reaction Launch Commit Notify，但不删除它。
4. 通过 Reference Viewer/资产引用检查确认没有需要手动迁移的 Enemy Launch 派生资产或 Blueprint default override。不得把“主 CDO 看起来为空”替代为全体派生类检查。

通过条件：所有已发现的 Enemy Launch consumer 均可接受 Root-only 合同，且没有需迁移的 Legacy property 值。失败条件：发现非空 Legacy value、派生类 override、缺失 Root Montage 或不明确消费者时，立即停止 Source 实施，报告资产/属性/引用；用户决定手动迁移或放弃退休，本计划不授权自动资产修改。

实施后复做 readback：旧反射字段和 toggle 消失，所有已发现 GA/派生资产可打开且无 Missing Property/Missing Class 警告；这不是 clean-checkout asset baseline 或 packaging 证明。

## 6. 实施顺序

1. Main 确认第 5 节用户 readback 的通过证据，保留该证据类型边界与现有脏工作树。
2. Gemini 先改 Enemy Header：收敛 enum、UPROPERTY、tags/tasks/state、callback/helper 与 test seams；将自然完成状态重命名为 Root Motion 语义。不得修改 shared facilities。
3. Gemini 改 Enemy CPP：删除 Legacy branch，并将原 Root branch 直接保留为唯一 activation flow；删除 Legacy callbacks/cleanup/validation、Physics includes 和 CDO fallback copy。不得重排 Root side-effect order。
4. Gemini 更新专项 Automation：删除 Legacy positive fallback，新增 invalid Root fail-closed 与真实 ASC Commit event no-op；保留/扩展 Root lifecycle、facing block、ledge、stance-break、external cancel、Destroy 和 UnPossess。
5. Gemini 对 HitReactionAutomationTests.cpp 做四条 CDO legacy speed 断言的窄删除；Tag/Notify shared-contract 断言不动。
6. Gemini 运行批准范围静态自审、一次干净只读实施自审和 git diff --check，交付限定 diff、zero-reference、Automation 结果与未运行的用户门禁。Main 后续执行 ue-strict-review、文档归档与提交门禁。

## 7. 验证矩阵

### 静态与范围

- git diff --check。
- git diff --name-status 只能包含第 3 节四个 Gemini paths；无 Source 外溢、无 Content/Config/asset 写入、无 Unity hygiene 例外。
- Enemy Ability 与批准测试 scoped zero-reference：TakeoffMontage、LandingRecoveryMontage、LaunchHorizontalSpeed、LaunchVerticalSpeed、FacingTurnRateDegreesPerSecond、bUseGroundedRootMotionKnockdown、CommitEventTask、FacingTurnTask、FallValidationTask、LaunchCommitEventTag、FLaunchFacingSmoothingState、bCommitHandled、Legacy phase 和相关 callback/helper 均为零。
- 验证保留：Enemy launch/trigger tags、State.Action.HitReacting、State.Block.Facing、Ability.Action.Teardown.OnUnpossess、AbilitiesToCancel.Num() == 2、Root candidate 与 Enemy Character deferral API 均保持可用。
- Rider 诊断可用时，四个批准路径必须无错误；不可用时记录 coverage fallback，不把静态检查说成编译或 PIE。

### Focused Automation

- PolyQuest.Combat.EnemyLaunchReactionRootMotion：
  - CDO tag/cancel contract 与 Root-only reflection surface；
  - valid Root candidate；null、no-root-motion、no-slot、zero/NaN/Inf length、non-Walking 均 fail-closed。每个 invalid activation 必须回到 None / inactive，保持 velocity、State.Block.Facing 和 bCanWalkOffLedges 无副作用；不得留下 MontageTask 或其他 AbilityTask；
  - Root activation 的 AI/velocity/facing/State.Block.Facing 行为、真实 ASC Commit event no-op、无 Physics launch；
  - natural/interrupted montage、external ASC cancel、Falling、ledge、stance-break complete/abort、UnPossess、Destroy/迟到 callback 安全收敛。
- PolyQuest.Combat.HitReaction：Enemy Launch CDO 更新后仍保留 global Commit Tag/Notify 与其余 Hit Reaction contract。
- 回归：PolyQuest.Combat.EnemyStanceBreakRateWindow、PolyQuest.Enemy.RootMotionFacing，以及现有与 AEnemyCharacter stance-break/feedback handoff 直接相关的 Automation。未修改的 shared PolyQuest.Combat.LaunchFacingSmoothing 只作为可选回归，不把其存在当作 Enemy Legacy 尚未退休的失败。

### 用户拥有的 Compile 与 PIE

- PolyQuestEditor (Development Editor) 手动编译。
- 第 5 节实施后 Editor readback。
- Scene01 PIE：Enemy 在平地、轻坡/台阶、墙角和边缘触发 Root Motion knockdown；确认无 LaunchCharacter 弹跳/滞空 fallback，Root animation 与 capsule 行为自然。
- Scene01 PIE：Montage 自然完成、外部取消/打断、Falling/ledge fail-closed、UnPossess/敌人销毁后均清理 State.Block.Facing、ledge 与 stance-break deferral；AI 可按既有所有权恢复。
- 编译、Automation、Editor readback、PIE、视觉、网络与包装证据必须分别记录，不能互相替代。

## 8. 收口记录与当前状态

- 工作目录：E:\GameDevelop\PolyQuest；基线：aab8ef630ec49575fff3d82f0e0fd845bf7d44f1。
- Gemini 仅可修改第 3 节四个 Approved Paths。不得修改本文档、其他文档、任何资产/Config/Build.cs、shared Commit/Facing facilities、EnemyStanceBreakRateWindowAutomationTests.cpp、EnemyRootMotionFacingAutomationTests.cpp 或任何清单外文件。
- 第 5 节 readback 未通过或不存在时，不得开始 C++ 修改。发现共享 consumer、资产迁移、Motion Warping、网络、死亡架构、墙体新表现或清单外依赖时立即停止并报告。
- 每个静态、编译、Automation 或运行时根因最多一次有证据修复加一次定向重跑；重复根因或两轮连续修复失败必须交回 Main/用户。
- Gemini 不得提交。Main 仅在用户 readback、编译、Automation、PIE 与 Fresh Review 有证据后更新 ARCHITECTURE.md、ROADMAP.md、ROADMAP-archive.md 并等待用户明确提交批准。
- 当前状态：已完成。Enemy Launch 仅保留 `None -> RootMotionKnockdown -> EndAbility`；Physics Launch、旧反射字段、Commit/Facing/Falling 生命周期、Legacy 正向测试和 `bUseGroundedRootMotionKnockdown` 已退休。下一候选为条件性 `TODO-07B14`；`TODO-03C` 不被本切片阻塞。
- 用户确认的门禁：Development Editor 编译、Enemy Launch/HitReaction/StanceBreak/Facing Focused Automation、Enemy Launch Editor readback 与 Scene01 PIE 通过。Main Fresh Review 及其一次定向修复后的 delta review 未发现 P0/P1/P2 blocker；静态、编译、Automation、readback 与 PIE 证据不互相替代。
- 定向修复收据：Destroy 路径无条件清理 Ability 内部 delegate/ledge latch，而仅在有效未销毁 Enemy 上访问 CMC；专项 Automation 覆盖负值、NaN、Infinity 长度、自然/中断 deferral 区分和 Destroy 后迟到 Montage 回调。
- 本计划作为最近完成切片记录保留，直至下一阶段通过 Archive Preflight 后正式替换。Gemini 未提交；Main 仅在用户明确批准时按批准 Source/Test 路径及本阶段文档暂存和提交。
