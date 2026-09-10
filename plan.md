# TODO-07B11: Enemy Launch Reaction Root-Motion Knockdown v1

## 1. 阶段定位、基线与路由

- **Target Objective**：为 UEnemyLaunchReactionAbility 增加 Enemy 专属、固定距离的 grounded Root Motion 击倒路径。位移与倒地、滑行、翻滚、恢复表现由作者化 Montage 提供；胶囊碰撞、地面、台阶和边缘仍由 Character Movement Component（CMC）唯一负责。
- **本阶段范围**：只实施 Slice A。Slice B（Enemy-local Motion Warping）和 Slice C（阻挡命中后的专属水平表现）只作为后续证据触发项，不在本阶段预铺实现。
- **基线**：main @ 319feb6f8ebf6df9b3fbf70831eeebfc7778bfc0（2026-09-11 当前 HEAD；07B11 只读规划基线）。
- **当前工作树**：非 clean。现有 Content/**、Config、Blueprint、AnimBP、Montage、地图、插件和其他 WIP 均为用户所有；本阶段必须保留、排除，不得回滚、清理或批量暂存。当前 `ROADMAP.md` 与 `Source/PolyQuest/Public/Combat/Projectile/CombatProjectile.h` 等仍有用户未提交变动；`ARCHITECTURE.md` 的稳定契约更新仅由 Main 在验证收口时完成。
- **Archive Preflight**：PASS。上一阶段 TODO-07B10 已在 `ROADMAP-archive.md` 保留唯一收口摘要；其被替换的完整旧 `plan.md` 由 Git 历史（`319feb6:plan.md`，同源于 `c708d65`）保留。本次替换不重复归档、不改写历史记录。
- **技能路由**：
  - Outer: ue-stage-workflow
  - Primary: ue5-cpp-gameplay
  - Support: ue5-debug-validation
  - Route reason: 这是现有 GAS Ability 内的局部双路径生命周期改造，需要保留旧物理回退并增加 Root Motion/CMC 状态边界与确定性 Automation。
- **执行路线**：manual/out-of-band Gemini。Implementation executors: 1（Gemini）；in-app delegation: 0。Main 保留架构、范围、验证解释、Fresh Review、文档、暂存和提交所有权；Gemini 不得提交。

## 2. 目标、刻意非目标与所有权

### 目标

- 在有效 Enemy 专属 Root Motion Montage 已配置且角色处于 MOVE_Walking 时，完整击倒表现由一个连续 Montage 驱动。
- 首帧前完成面向攻击者的 Yaw 对齐，Root Motion 使用本地 -X 固定位移表达远离攻击者的后退/倒地。
- Root Motion 期间不产生第二套物理 Launch 状态；异常移动模式、取消、死亡、销毁和同步重入均安全收敛到单一 EndAbility() 清理出口。
- 保留并可显式使用现有 LaunchCharacter() + Falling/LandingRecovery 流程作为兼容 fallback。

### Deliberate Non-goals

- 不改 PlayerLaunchReactionAbility、FLaunchFacingSmoothingState、AEnemyCharacter、AEnemyAIController、FHitReactionImpactResolver 或 UAbilityTask_TurnToFacing。
- 不改任何 GameplayTag、Input、Config、Build.cs、GAS 权属、伤害/Poise 路径或既有 Enemy cancellation taxonomy。
- 不修改或迁移共享的 AM_LaunchTakeoff / AM_LaunchLandingRecovery，不编辑 .uasset、.umap、Blueprint、AnimBP 或导入资源。
- 不加入全局 Motion Warping、Enemy 通用移动框架、Ragdoll/物理击飞重写、网络预测/回滚、空中/大间隙飞行支持。
- 不在本阶段解决墙体命中后的自定义停顿、滑行转场或骨骼表现问题；若 PIE 证明需要，进入 Slice C。

## 3. Approved Paths 与最小接口

Gemini 只可修改：

- Source/PolyQuest/Public/AbilitySystem/Abilities/EnemyLaunchReactionAbility.h
- Source/PolyQuest/Private/AbilitySystem/Abilities/EnemyLaunchReactionAbility.cpp
- Source/PolyQuest/Private/Tests/EnemyLaunchReactionRootMotionAutomationTests.cpp（新增）

允许的接口/状态变化：

- 增加 EditDefaultsOnly、BlueprintReadOnly 的 RootMotionKnockdownMontage，默认 nullptr。
- 增加 EditDefaultsOnly、BlueprintReadOnly 的 bUseGroundedRootMotionKnockdown，CDO 默认 true；显式设为 false 时锁定 legacy 物理流程。
- 在私有 ELaunchPhase 增加 RootMotionKnockdown，与旧 Takeoff、TurningToLaunch、AwaitingAirborne、Airborne、LandingRecovery 隔离。
- 在 CPP 增加私有 TryResolveRootMotionFacingYaw(...)，只做平面方向归一化、有限值校验和 Yaw 变换；不得改变 Resolver 公共接口或旧速度算法。
- WITH_DEV_AUTOMATION_TESTS 下可增加只读 getter、测试 setter 和回调触发 seam，用于验证分支、Task、阶段、ledge 恢复和清理；不得把测试 seam 暴露到 Shipping。
- 现有 CommitEventTask、Launch 速度字段和旧资产字段保留，仅由 legacy 分支使用。

## 4. 冻结运行时契约

### 4.1 分支选择与激活校验

- 公共校验继续要求有效 ASC、存活 Enemy、AnimInstance、所需既有 Tag、有效移动组件和可接受的地面状态。
- Root Motion 候选必须同时满足：开关为真、Montage 有效、HasRootMotion() 为真、Slot 配置有效、播放长度为有限正值，且移动模式精确为 MOVE_Walking。
- Legacy 候选必须同时满足：TakeoffMontage 与 LandingRecoveryMontage 有效；LaunchHorizontalSpeed、LaunchVerticalSpeed、FacingTurnRateDegreesPerSecond 为有限正值；保持现有 grounded 约束。
- CanActivateAbility 只在 Root Motion 候选或 Legacy 候选至少一套有效时通过。Root Motion 候选有效时不再强制要求旧 Takeoff/Landing 资产。
- Root Motion 开关或资产在启动前无效且 Legacy 候选有效时，选择 Legacy fallback；两套均无效才拒绝激活。
- Root Motion 已选定后若受击方向、对象、委托或移动状态失败，只能 fail-closed 结束，禁止动态切换到 LaunchCharacter()。

### 4.2 Root Motion 分支时序

1. 创建 Root Motion Montage Task；不创建 CommitEventTask。
2. CommitAbility 成功后设置绑定 Enemy、ActiveMontage 和 CurrentPhase = RootMotionKnockdown。
3. 停止 AI 导航和当前速度。
4. 在新 Montage ReadyForActivation() 之前调用 CharacterASC->CancelAbilities(&AbilitiesToCancel, nullptr, this)，取消 Enemy Melee 与 Small Hit Reaction。
5. 取消后立即检查 EnemyCharacter->HasAnyRootMotion()；仍有残留时 fail-closed，禁止两个 Root Motion 来源在首帧叠加。
6. 从现有受击上下文读取本地攻击者方向，通过 TryResolveRootMotionFacingYaw 计算面向攻击者的 Yaw。方向缺失、零向量、非有限或无法归一化时，在播放前结束 Ability。
7. 在 MontageTask->ReadyForActivation() 之前一次性 SetActorRotation；不得调用 UAbilityTask_TurnToFacing，不得依赖旧水平/垂直速度参数。
8. 在 ReadyForActivation() 之前保存原始 bCanWalkOffLedges，设置为 false 并标记 bLedgeSettingModified；绑定 Montage End 与 MovementModeChanged 委托。
9. 调用 ReadyForActivation() 后检查 bEndAbilityRequested、Ability Active 状态和 Montage 身份/活动状态。同步重入已经结束时不得恢复旧 Task、Montage 或阶段。
10. Montage 确认活动后再调用既有 BeginLaunchStanceBreakDeferral()；若此后自然完成则完成 deferral，否则由统一清理出口中止。

### 4.3 Notify、移动模式和自然结束

- Root Motion 分支不创建或依赖 CommitEventTask。ReactionLaunchCommit Notify 对该分支是可选观察点，不暂停 Montage、不触发 Launch、不创建 Falling watchdog；缺失或延迟不妨碍自然收尾。
- RootMotionKnockdown 期间仅接受 MovementMode == MOVE_Walking。任何离开该模式的变化，包括 MOVE_Falling、MOVE_NavWalking 或其他模式，立即 EndFromMontage(true)。
- OnActiveMontageEnded 只在 Montage 身份匹配时处理：Root Motion Montage 非中断结束标记自然完成并结束 Ability；中断结束按异常路径处理。旧 Takeoff/Landing 的处理保持 legacy 语义。
- Root Motion Montage 必须自身包含起飞、后退/滑行/翻滚和恢复段；不另行启动 LandingRecovery Montage。

### 4.4 单一清理出口与 legacy 保全

- EndAbility() 必须幂等，统一解绑所有委托、结束并置空 Task、停止活动 Montage、恢复 bCanWalkOffLedges、清空绑定指针/阶段，并处理 Stance Break deferral。
- 自然 Root Motion 结束调用 CompleteLaunchStanceBreakDeferral()；中断、Falling、死亡、销毁、UnPossess、播放失败和同步重入调用 AbortLaunchStanceBreakDeferral()。两者均不得产生第二条状态恢复路径。
- Legacy 分支保留当前 CommitEventTask -> Takeoff pause -> FacingTask -> LaunchCharacter -> Falling/LandingRecovery 生命周期；只将资产校验改为分支兼容，不顺手重构旧路径。
- 不调用 DisableMovement()，不直接写 Actor Location，不清理或覆盖其他 Ability 的 Root Motion；残留 Root Motion 只允许触发 fail-closed。

## 5. 用户拥有的 Editor 资产门禁

用户在 Unreal Editor 中创建或确认一个 Enemy-exclusive Knockdown Montage。Codex/Gemini 不得编辑二进制资产。

Editor readback 必须确认：

- Montage 使用 Enemy Skeleton 和有效 Slot，AnimBP 的 Root Motion 模式能让 CMC 消费该 Montage。
- 实际 Sequence 已启用 Root Motion，Montage HasRootMotion() 为真，播放长度有效。
- 根骨骼位移是本地 -X 的固定后退距离；敌人先面向攻击者，再向后倒地/滑行/翻滚。
- 起飞、位移、恢复在同一 Montage 内完成，并在可行地面状态下自然结束。
- ReactionLaunchCommit 可存在也可缺失；它不是 Root Motion 生命周期硬门禁。
- 只在 GA_EnemyLaunchReaction 上赋值新 Montage；旧 Takeoff/Landing 资产仍保留为 fallback。不得修改共享 Player/Enemy Montage。

## 6. Focused Automation

新增测试文件并注册：

PolyQuest.Combat.EnemyLaunchReactionRootMotion

至少覆盖以下确定性场景：

1. SelectsRootMotionBranchWhenConfigured：有效 Root Motion Montage + 开关开启 + Walking 选择新分支；不创建 CommitEventTask、Facing Task 或 Falling watchdog。
2. FallsBackToPhysicsLaunchWhenDisabledOrInvalid：开关关闭、Root Motion Montage 缺失/无 Root Motion/Slot 无效时，在 Legacy 配置完整的前提下选择旧流程；两套均无效时 fail-closed。
3. InstantFacingAppliedBeforeMontageActivation：使用有限平面受击方向，在 Montage Task ReadyForActivation() 前完成面向攻击者的 Yaw；验证不读取旧速度作为 Root Motion 前置。
4. LedgeProtectionRestoredOnAllExitPaths：自然完成、被动中断、同步重入和对象销毁后均恢复原始 bCanWalkOffLedges，且清理只发生一次。
5. PrematureFallingAbortsReactionAndStanceBreak：Root Motion 阶段进入 MOVE_Falling 或其他非 Walking 模式时立即异常结束并调用 Abort deferral。
6. CommitNotifyIsNonBlocking：Root Motion 分支不监听 Commit Task；有 Notify、无 Notify、延迟 Notify 都不阻止 Montage 自然结束。

测试使用 transient 对象和 WITH_DEV_AUTOMATION_TESTS seam；Headless/NullRHI 结果只证明 Native 分支和生命周期，不证明真实动画位移、碰撞或视觉表现。

## 7. 验证矩阵与收口门禁

| 门禁 | 所有者 | 需要的证据 |
| --- | --- | --- |
| 最终静态回查 | Gemini / Main | 仅批准路径 diff、Rider 错误级检查（可用时）、git diff --check、实施自审；不得称为 PIE 或视觉证据 |
| 手动编译 | 用户 | PolyQuestEditor (Development Editor) 实际成功结果 |
| Focused Automation | 用户 | 新专项全部 Success，并回归 PolyQuest.Combat.HitReaction、PolyQuest.Combat.LaunchFacingSmoothing、PolyQuest.Enemy.RootMotionFacing |
| Editor readback | 用户 | Enemy GA 新字段、Montage Root Motion/Slot/Skeleton/AnimBP 模式、固定 -X 位移与旧 fallback 配置 |
| Scene01 PIE | 用户 | 平地、缓坡、台阶、墙/墙角、边缘、Montage 中断、死亡/销毁和 Legacy fallback |
| Main Fresh Review | Main | 依 ue-strict-review 做批准范围内的一跳影响审查，重点检查取消/销毁/重入/null/委托/移动模式/Tag 回归 |
| Commit gate | 用户 + Main | 用户明确批准后才可按批准路径暂存和提交；本计划阶段不自动提交 |

Slice A 的 PIE 成功标准：

- 平地固定距离与作者化 Root Motion 一致。
- 缓坡和台阶保持 CMC 地面跟随，不穿透、不进入空中 Launch。
- 墙体/墙角由 CMC 阻挡胶囊，不出现穿透；专属阻挡过渡不在本阶段承诺。
- 边缘不会被伪装成空中击飞；离开 Walking 会异常收敛。
- 所有退出路径恢复 ledge 设置、Montage/Task/委托和 GAS/Poise 生命周期。

## 8. Gemini 交付要求与停止条件

- 先阅读本 plan.md、目标 Header/CPP、直接调用者和现有相关 Automation；只在批准路径内实现。
- 首轮交付前完成一次干净、只读的实施自审；明确列出 ReadyForActivation() 同步重入、残留 Root Motion、Montage 身份、MovementMode、销毁/UnPossess、ledge 恢复和单一 EndAbility() 清理检查。
- 交付内容必须包含批准路径 diff、静态检查结果、Automation 结果/新增覆盖、未运行的用户门禁，以及明确的 fallback 与非目标说明。
- 不得修改 plan.md、ROADMAP.md、ARCHITECTURE.md、全部 Content/**、Config、Blueprint、AnimBP、Montage、地图或 Build.cs；不得 git add -A、提交、回滚、删除或导入资产。
- 任何额外 Source 文件、Tag、Input、Config、Build.cs、共享 Montage 迁移、Motion Warping 组件、网络逻辑或墙体专属表现需求出现时立即停止并报告，不自行扩大范围。
- 同一根因只允许一轮有证据的修复及一次目标重跑；根因重复或连续两轮失败后停止并保留首个失败证据。
- 没有真实 Warp 窗口及动态距离/目标消费者时，不实现 Slice B；没有墙体 PIE 证据时，不实现 Slice C。

## 9. Main 收口记录与后续排期（2026-09-11）

- **实现范围**：执行者报告确认改动封闭在本计划批准的 Enemy Header/CPP 与专项 Automation 路径；Player Launch 路径未改动，符合本阶段非目标。
- **用户证据**：用户确认 `PolyQuest.Combat.EnemyLaunchReactionRootMotion`、既有 HitReaction/LaunchFacingSmoothing/RootMotionFacing 回归 Automation、`PolyQuestEditor (Development Editor)` 编译以及 Scene01 PIE 通过；该记录不把执行者静态结果包装成运行时证据。
- **Main Fresh Review**：批准范围内未发现可由当前 diff/source 证据定级的 P0/P1/P2 缺陷。影响雷达的结构性未覆盖提示保留为残余风险，不触发范围扩张或 Player 修复。
- **门禁状态**：用户已补充确认 `PolyQuestEditor (Development Editor)` 编译通过，`Debt-07B11-CompileReadback` 关闭；07B11 的实现、静态、编译、Automation、Editor/资产 readback、PIE 与 Main Fresh Review 门禁均已有对应证据，可以进入提交门。
- **Player 对齐决定**：不把 Player 重构并入 TODO-07B11。未来只在独立、证据触发的 `TODO-07B12` 中评估通用 `State.Block.Facing` 契约及 Player 专属 grounded launch；本阶段不新增该 Tag、不改 AIController/PlayerLaunchReactionAbility，也不把提案写入 `ARCHITECTURE.md`。
- **后续触发条件**：只有真实 In-Place 动作转向复现、统一 Tag 所有权与清理边界、Player 相机/输入/Tech Roll 契约均冻结后，才进入 TODO-07B12；Motion Warping 仍需真实 Montage Warp Window 与动态目标/距离消费者。
