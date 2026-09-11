# TODO-07B13-RET: Player Legacy Launch Retirement v1

## 1. 阶段定位、基线与路由

- Target Objective：删除 Player Launch Reaction 中已不被当前作者化 CDO 使用的 Physics Launch 兼容路径及已失去分支选择意义的 bUseGroundedRootMotionKnockdown，使 UPlayerLaunchReactionAbility 成为单一 grounded Root Motion knockdown/recovery 生命周期。Root candidate 无效或 Root Montage 启动失败时一律 fail-closed，不得恢复 Physics fallback。
- 阶段关系：TODO-07B13 已归档至 ROADMAP-archive.md，Archive Preflight 为 PASS。本阶段只退休 Player；TODO-07B11-RET Enemy Legacy Launch Retirement 保持独立，后续另立 active plan。
- 基线：main @ 9a01d3ceb8ad074b4e9a478f2aa2bac5f0601d63（2026-09-11），外加用户保留的非干净工作树和 Main 未提交的 ROADMAP.md 排期更新。
- 用户前置证据：GA_PlayerLaunchReaction 已是 Root Motion-only，RootMotionKnockdownMontage = AM_TakeOff、Legacy Takeoff/LandingRecovery 字段为空；原 bUseGroundedRootMotionKnockdown 为 true，现由本切片一并退休。07B13 的编译、Automation 和 Scene01 PIE 是前置事实，不等同于本退休切片已经验证。
- 工作树约束：所有 Content、Config、Blueprint、AnimBP、Montage、地图、导入资源、tmp 及其他 WIP 均保留；不得回滚、清理、格式化、批量暂存或归因。
- 技能路由：
  - Outer: ue-stage-workflow
  - Primary: ue5-cpp-gameplay
  - Support: ue5-debug-validation
  - Route reason: GAS Ability 生命周期窄删除，必须保留 Root Motion、Dodge window、CMC/ledge 和 EndAbility 清理契约。
- 执行路线：manual/out-of-band Gemini。Implementation executors: 1（Gemini）；in-app delegation: 0。Main 保留架构、计划、验证解释、Fresh Review、文档、暂存和提交所有权；Gemini 不得提交。

## 2. 目标、完成标准与刻意非目标

### Target Objective

1. 删除 Player Takeoff -> TurningToLaunch -> AwaitingAirborne -> Airborne -> LandingRecovery Physics Launch 分支，以及只服务该分支的 reflected fields、Task、回调、测试 seam、正向兼容测试和已失去分支选择意义的 bUseGroundedRootMotionKnockdown。
2. 保留唯一 RootMotionKnockdown phase：Root Montage、持续 Dodge cancel-window listeners、一次性 facing、StopMovementImmediately、CMC Walking、ledge 保护与幂等 EndAbility。
3. activation 门禁改为 common validation 加 IsRootMotionKnockdownCandidate；没有 Root candidate 时拒绝激活，不读取旧 Montage 或速度参数。
4. 将 PlayerActionWindow 的旧 LandingRecovery cancel-window fixture 迁移为 RootMotionKnockdown fixture，保留 payload identity、重复 Begin/End 幂等和 EndAbility 清理。

### Completion Criteria

- UPlayerLaunchReactionAbility 不再声明、复制、校验、播放或停止 Player TakeoffMontage / LandingRecoveryMontage，不再调用 LaunchCharacter、创建 CommitEventTask / FacingTurnTask / FallValidationTask，也不再保留旧 phase。
- Player 不再请求或验证 Event.Reaction.Launch.Commit；全局 Tag、UAnimNotify_ReactionLaunchCommit 和 Enemy 消费路径完整保留。
- Root branch 仍不创建 Commit/Facing/Falling task；Commit gameplay event 对活跃 Player Root Motion Ability 无 Player 生命周期副作用。
- Montage 为空/无 Root Motion/无 Slot/长度非法或非 MOVE_Walking 时 fail-closed，不存在 Legacy fallback。
- Player 不再声明或读取 bUseGroundedRootMotionKnockdown；Root Motion、Slot、播放长度与 Walking 是唯一 Root candidate 合法性门禁。Enemy 同名开关仍属 TODO-07B11-RET 前的独立 Enemy 兼容面，不在本阶段修改。
- Player scoped Legacy Source/Test 引用为零；Enemy Legacy、共享 FLaunchFacingSmoothingState 和全局 Commit Notify/Tag 是允许残留。
- 用户确认 Development Editor 编译、focused Automation 与 Scene01 PIE；Main Fresh Review 无 P0-P2 blocker。

### Deliberate Non-goals

- 不修改 EnemyLaunchReactionAbility、Enemy 测试、Enemy CDO、AIController、State.Block.Facing、Enemy StanceBreak handoff 或 TODO-07B11-RET。
- 不删除或修改 LaunchFacingSmoothingState、AbilityTask_TurnToFacing、UAnimNotify_ReactionLaunchCommit、Event.Reaction.Launch.Commit 或 Config Tag；它们仍由 Enemy 路径使用。
- 不修改 DodgeAbility、PlayerGuardBreakAbility、PlayerCharacter、HitReactionImpactResolver、Build.cs、uproject、Config、网络/预测或死亡架构。
- 不编辑或自动迁移任何 uasset / umap、Blueprint、AnimBP、Montage 或地图；不批量 resave 用户资产。
- 不引入 Motion Warping、空中保护、Tech-Roll、State.Block.Movement、通用 reaction framework 或新 Gameplay Tag。
- 不合并 Player 和 Enemy retirement，也不做共享重构。

## 3. Approved Paths 与所有权

### Gemini 可修改

1. Source/PolyQuest/Public/AbilitySystem/Abilities/PlayerLaunchReactionAbility.h
   - 删除 Player Legacy data、phase、Task、callback/helper、test seam 与 bUseGroundedRootMotionKnockdown reflected field/test getter/setter；类注释和测试 seam 收敛为 Root-only。
2. Source/PolyQuest/Private/AbilitySystem/Abilities/PlayerLaunchReactionAbility.cpp
   - 删除 Legacy activation、commit/facing/falling/landing 生命周期与其 cleanup，以及 Root candidate 的 bUseGroundedRootMotionKnockdown predicate；保留既有 Root branch 运行顺序与 EndAbility 所有权。
3. Source/PolyQuest/Private/Tests/PlayerLaunchReactionRootMotionAutomationTests.cpp
   - 删除 Legacy candidate/fallback 正向测试与 Player flag-specific test setup/assertions，改为 Root-only invalid-config fail-closed、task-isolation 与无 Commit 消费覆盖。
4. Source/PolyQuest/Private/Tests/PlayerActionWindowAutomationTests.cpp
   - 仅将 Section 10 从 LandingRecovery Dodge-window fixture 迁移为 RootMotionKnockdown fixture。
5. Source/PolyQuest/Private/Tests/HitReactionAutomationTests.cpp
   - 仅删除 Player Launch 已退休的 Physics Launch CDO 默认 horizontal/vertical speed 与 facing turn-rate 四条断言；不新增重复 Root CDO 断言，不改 Enemy Launch 断言或该套件其他逻辑。

### Main 批准的 Unity Build hygiene 例外

6. Source/PolyQuest/Private/Tests/EnemyLaunchReactionRootMotionAutomationTests.cpp
   - 仅将匿名命名空间的 TagTeardownOnUnpossess 定义移入 FEnemyLaunchReactionRootMotionAutomationTest::RunTest() 局部作用域，与既有局部 TagBlockFacing 并列；保留变量名、Tag 值和全部断言语义，不修改 EnemyStanceBreakRateWindowAutomationTests.cpp 或任何 Enemy 运行时逻辑。

### Main-only

- plan.md、ARCHITECTURE.md、ROADMAP.md、ROADMAP-archive.md。
- 仅在用户门禁与 Fresh Review 完成后可归档、同步文档、暂存或提交。

### Explicitly Excluded

- Source/PolyQuest/Public/AbilitySystem/Abilities/LaunchFacingSmoothingState.h
- Source/PolyQuest/Private/AbilitySystem/Abilities/LaunchFacingSmoothingState.cpp
- Source/PolyQuest/Public/Animation/Combat/AnimNotify_ReactionLaunchCommit.h
- Source/PolyQuest/Private/Animation/Combat/AnimNotify_ReactionLaunchCommit.cpp
- Source/PolyQuest/Public/AbilitySystem/Abilities/EnemyLaunchReactionAbility.h
- Source/PolyQuest/Private/AbilitySystem/Abilities/EnemyLaunchReactionAbility.cpp
- Source/PolyQuest/Private/Tests/LaunchFacingSmoothingAutomationTests.cpp
- Content/**、Config/**、Build.cs、uproject 和全部二进制资产。

若编译或测试暴露清单外文件的直接依赖，Gemini 必须停止，报告完整错误、最小候选路径及原因，不得自行扩大白名单。

## 4. 冻结的运行时与 API 契约

### Root-only activation

- ELaunchPhase 仅保留 None 和 RootMotionKnockdown。
- ValidateActivationSetup 只接受 common validation 加 Root candidate；不得保留 OR Legacy candidate。
- Root candidate 条件：RootMotionKnockdownMontage 非空、HasRootMotion、存在 Slot track、有限正播放长度、CMC 精确为 MOVE_Walking。
- 无效 Root 配置在任何副作用之前 fail-closed，不创建 Task、不 Commit、不改 ledge、不回退 Physics Launch。

### 删除的 Player Legacy surface

- 删除 TakeoffMontage、LandingRecoveryMontage、LaunchHorizontalSpeed、LaunchVerticalSpeed、FacingTurnRateDegreesPerSecond、bUseGroundedRootMotionKnockdown 及其 Player-only test getter/setter。
- 删除 CommitEventTask、FacingTurnTask、FallValidationTask、LaunchCommitEventTag、FLaunchFacingSmoothingState 成员、bCommitHandled 与 Legacy phase。
- 删除 OnLaunchCommitEventReceived、OnFacingTurnCompleted、OnFacingTurnFailed、CommitFrozenLaunch、OnFallValidationFinished、IsLegacyLaunchCandidate、IsEventFromTakeoffMontage、IsEventFromLandingRecoveryMontage 及其 Player-only seams。
- 删除 Player 对 UAbilityTask_TurnToFacing / UAbilityTask_WaitDelay 的依赖；不删除共享类型或 Enemy 使用者。

### 保留的 Root lifecycle

- 保留 RootMotionKnockdownMontage、MontageTask、CancelBeginTask、CancelEndTask、BoundAnimInstance、BoundPlayerCharacter、impact snapshots、MovementMode delegate、ledge 保存/恢复和 bEndAbilityRequested。
- 仍遵循：创建 Root Montage / persistent cancel listeners -> CommitAbility -> 取消竞争 Ability -> 残留 Root Motion 拒绝 -> StopMovementImmediately -> 一次性 Yaw -> ledge guard -> delegate -> ReadyForActivation。
- 每个 ReadyForActivation 后检查 bEndAbilityRequested。Montage 启动失败、方向无效、非 Walking、取消、死亡、Destroy、UnPossess、自然结束和中断统一经过 EndAbility。
- EndAbility 仅清理仍存在的 Root task/listener、Montage、Dodge tag、delegate、ledge 值、snapshots 和 phase；不得因删除 Legacy 破坏 Root teardown。

### Dodge 与 Commit event

- WaitGameplayEvent(CancelWindowTag, nullptr, false, true) 继续持续监听，非法 payload 不消费 listener，重复 Begin/End 幂等。
- Cancel window 只接受当前 Root Montage 或其 Slot 中 SequenceBase payload，并校验 Avatar、Instigator、Target。
- Player 不再监听 Event.Reaction.Launch.Commit。该事件不能暂停 Player Montage、发射速度或改 phase；Notify/Tag 留给 Enemy。
- bTestBypassAnimInstanceActiveCheck 与 bTestBypassMontageActiveCheck 仍是 Root 自动化的窄 seam，保留且只绕过既有 active assertion。

## 5. 实施顺序与测试设计

1. Gemini 阅读本 plan、两份 Player Ability、两个批准测试、DodgeAbility 的 Player Launch cancel consumer，以及 Enemy/shared/Notify 的只读边界。
2. Header 先移除 Legacy API/phase/fields/tasks/helpers/test seams 及 Player bUseGroundedRootMotionKnockdown reflected field/test getter/setter；必须以最窄的 SetTestCurrentPhaseToRootMotionKnockdown 替换 SetTestCurrentPhaseToLandingRecovery，且只保留 Root-specific test seam。
3. CPP 删除 Legacy activation block、旧 callbacks、旧 MovementMode/Montage-end 分支、只服务 Legacy 的 cleanup/validation，以及 Root candidate 对 bUseGroundedRootMotionKnockdown 的判断。不得重排或重构 Root path。
4. PlayerLaunchReactionRootMotionAutomationTests 删除 CreateSyntheticLegacyMontage、Legacy candidate 与 7.5/7.6 fallback 用例，并删除 Player CDO flag default、flag-disabled candidate/activation 断言及其机械 SetTestUseGroundedRootMotionKnockdown 调用；保留 Montage/Slot/长度/Walking invalid-config fail-closed。验证 Commit ignore 时必须通过真实 ASC 的 HandleGameplayEvent(Event.Reaction.Launch.Commit, Payload) 派发，并断言 Root phase 不变；不得保留已删 handler 专用入口。
5. PlayerActionWindowAutomationTests Section 10 改用 Root Montage/inner Sequence/root phase，保留 malformed payload、nested sequence、duplicate Begin/End 与 EndAbility cleanup。
6. Gemini 做批准路径静态自审和一次干净只读实施自审；Main 再做 diff-first Fresh Review。

## 6. 验证矩阵

### 静态与零引用

- git diff --check。
- 在批准 Player Header/CPP/Tests 中不再出现 TakeoffMontage、LandingRecoveryMontage、LaunchCharacter、TurningToLaunch、AwaitingAirborne、Airborne、CommitEventTask、FacingTurnTask、FallValidationTask、IsLegacyLaunchCandidate、OnLaunchCommitEventReceived、bUseGroundedRootMotionKnockdown 或 Player Legacy seam。
- 全局允许 Enemy、共享 smoothing 和 Commit Notify/Tag 保留同类概念；不得把它们误判为 Player retirement 失败。
- Player 仍保留 Ability.Reaction.Player.Launch、Ability.Action.Teardown.OnUnpossess、State.Action.HitReacting、State.Input.Block.Movement、State.Input.Block.Jump、BlockAbilitiesWithTag.Num() == 10、AbilitiesToCancel.Num() == 11。

### Focused Automation

- PolyQuest.Combat.PlayerLaunchReactionRootMotion：
  - valid Root candidate；
  - null、non-root、no-slot、非法长度、non-Walking 均 fail-closed；
  - Root-only activation、task isolation、Commit event 无 Player 生命周期副作用；
  - natural/interrupted completion、ledge、external cancel、Destroy、UnPossess 清理。
- PolyQuest.Player.ActionWindows：
  - Root Montage / inner Sequence cancel payload identity；
  - malformed avatar/montage/sequence 拒绝；
  - duplicate Begin/End 幂等；
  - EndAbility 清除 State.Action.CanCancel.Dodge。
- PolyQuest.Combat.HitReaction：Player Launch CDO Tag/Input 合同回归。
- PolyQuest.Combat.EnemyLaunchReactionRootMotion：共享 Commit Notify/Tag 与 Enemy 路径回归。

### User-owned Editor Readback

- 打开 GA_PlayerLaunchReaction：类正确、Root Montage 正确，旧 Takeoff/Landing/速度/平滑转向/bUseGroundedRootMotionKnockdown 字段均消失，无 Missing Property / Missing Class 警告。
- 打开 Root Montage：Root Motion、Slot 与 Dodge cancel-window 仍存在。若 Player Montage 含旧 Reaction Launch Commit Notify，只记录 readback；本 C++ 切片不删除共享 Notify 或二进制资产。
- 对 Player Launch GA 与已知派生/使用入口做 Reference Viewer/readback，确认不需要资产迁移。用户 WIP 不构成 clean-checkout asset baseline。

### User-owned Compile 与 PIE

- 手动编译 PolyQuestEditor (Development Editor)。
- Scene01 PIE：Player grounded Root Motion、自然结束、Dodge window、取消/打断、边缘失败恢复、普通 locomotion；确认不存在 Physics airborne fallback。
- 复测一项 Enemy Launch，确认 Player retirement 未影响 Enemy Commit/Launch 链路。
- 编译、Automation、Editor readback、PIE、视觉和包装证据必须分开记录。

## 7. Gemini Handoff、停止条件与收口

- 工作目录：E:\GameDevelop\PolyQuest。
- Gemini 只能修改第 3 节五个 Player Source/Test 路径和唯一批准的 EnemyLaunchReactionRootMotionAutomationTests.cpp Unity Build hygiene 例外，不得修改 docs、Content、Config、Build.cs、uproject、EnemyStanceBreakRateWindowAutomationTests.cpp 或其他 Enemy 文件。
- 首次实现交付前，Gemini 必须派发一个干净只读子代理做实施自审；后续窄修复回路不再派发子代理。
- 交付必须附：批准路径 diff、git diff --check、Player scoped zero-reference、Automation 结果、未运行的用户门禁、以及 ReadyForActivation、Dodge listener、ledge、external cancel/Destroy/UnPossess 自审。
- 出现清单外 Source/Test 依赖、资产迁移、Enemy 修改、Config/Tag 改动、Motion Warping、网络或死亡架构需求时立即停止报告。
- 每个根因最多一次有证据修复和一次定向重跑；重复失败交由 Main/用户裁定。
- Gemini 不得提交。Main 在用户门禁与 Fresh Review 后更新文档；仅用户明确批准时按路径暂存/提交。

## 8. 当前计划状态与已知债务

- 本文件记录的 TODO-07B13-RET 已完成：07B13 收口已在 ROADMAP-archive.md，Archive Preflight PASS；本阶段的 Player Physics Launch 路径和 bUseGroundedRootMotionKnockdown 均已退休。
- 用户确认 Development Editor 编译、五项 focused Automation、GA_PlayerLaunchReaction 无 Missing Property/Missing Class 的 Editor readback 与 Scene01 PIE 通过；Main Fresh Review 未发现 P0/P1/P2 blocker。上述证据不扩写为 clean authored asset baseline、网络或包装证明。
- 本计划作为最近完成切片的交接记录保留，直至下一阶段通过 Archive Preflight 后正式替换。下一推荐执行切片为独立的 TODO-07B11-RET；届时必须重新建立 Approved Paths、资产 readback、Automation、compile、PIE、Fresh Review 和提交门禁。
