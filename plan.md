# TODO-07B13: Player Grounded Launch Reaction Root-Motion Alignment v1

## 1. 阶段定位、基线与路由

- **Target Objective**：在 Player 的 grounded 受击反应中增加 CMC Walking 下的作者化 Root Motion knockdown/slide/recovery 分支；当前 authored CDO 采用 Root Motion-only，Root candidate 失败时 fail-closed，并保证朝向、输入、Dodge cancel、恢复与 teardown 生命周期收敛。
- **阶段关系**：ROADMAP.md 已排定 TODO-07B11 -> TODO-07B12 -> TODO-07B13 -> 条件性 TODO-07B14。07B11 的 Enemy Root Motion 只作为生命周期参考；本阶段不改 Enemy 运行时逻辑，仅记录下方唯一获批的 Unity Build hygiene 例外。Player Legacy 源码兼容分支的退休另立下一独立切片。
- **基线**：main @ d2968a08675610aa698255f60f5fa2c35b561283（2026-09-11）。
- **Archive Preflight**：PASS（只读核对）。ROADMAP-archive.md 当前存在且仅存在一个 TODO-07B12 收口章节（第 1470 行）；旧 active plan 已完成归档后才替换。
- **当前工作树快照**：非 clean，共 323 条既有变更（266 删除、14 修改、43 未跟踪）。Content、Blueprint、AnimBP、Montage、地图、Config 和其他 WIP 均属于用户，必须保留、排除，不得回滚、清理、格式化或批量暂存。
- **初始证据边界**：已取得当前 Source、ROADMAP/ARCHITECTURE、定向 CodeGraph 和资产文件名静态证据；Rider 资产属性查询为空。本计划不把静态结果写成编译、Editor、Automation、PIE 或视觉证据。
- **技能路由**：
  - Outer: ue-stage-workflow
  - Primary: ue5-cpp-gameplay
  - Support: ue5-debug-validation
  - Route reason: native GAS Ability lifecycle plus grounded CMC/Root Motion branch and bounded validation gates。
- **执行路线**：manual/out-of-band Gemini。Implementation executors: 1（Gemini）；in-app delegation: 0。Main 保留架构、计划、验证解释、Fresh Review、文档、暂存和提交所有权；Gemini 不得提交。

## 2. 目标、完成标准与刻意非目标

### 目标

- 新增 Player-specific Root Motion Montage 配置；当配置有效且角色精确处于 MOVE_Walking 时优先执行连续 Root Motion。
- Root Motion Montage 承载起身、滑行/击倒和恢复；CMC 继续独占胶囊碰撞、地面、台阶、墙和边缘。
- 在 Montage 启动前冻结 target-local Target -> Attacker 方向并只写一次 Actor Yaw；现有 HitReacting、HasAnyRootMotion、Lock-On 和 locomotion gate 继续有效。
- 当前 authored Player CDO 不配置 Physics Launch fallback；Root candidate 失败、Root Montage 启动失败或开关关闭时 fail-closed。旧 Physics Launch/FacingTask/Commit Notify/Falling watchdog/LandingRecovery 源码仅作为下一独立退休切片前的 dormant compatibility 保留。
- 让 movement/jump block、作者化 Dodge cancel、自然结束、取消、外部死亡取消、销毁和 UnPossess 均通过单一幂等 EndAbility 清理。
- 完成标准：批准路径静态检查、focused Automation、用户 Development Editor 编译、真实资产 readback、Scene01 Player/Enemy PIE 回归，以及 Main Fresh Review 均有独立证据。

### Deliberate Non-goals

- 不使用 MOVE_None、直接写 Actor Location 或承诺真正 Falling/空中/大间隙飞行。
- 不新增 State.Block.Facing owner、State.Block.Movement、平行动作状态机、网络/预测/回滚或通用反应框架。
- 不新增全局 Motion Warping；没有真实 Warp Window 与动态目标/距离消费者时记录 no-adoption。若条件成立，另立 TODO-07B14 或新批准切片。
- 不实现 Tech-Roll、Player 空中 Dodge、Enemy 改写、伤害/Poise/StateTree/Execution Lock 或 Camera feedback 新通道。
- 不修改 PlayerCharacter、DodgeAbility、Config、Build.cs、uproject、Blueprint、AnimBP、Montage、地图或任何二进制资产。
- 不因本阶段便利而修改现有 HitReactionAutomationTests.cpp；新契约由专项测试覆盖，既有套件只作回归。
- 不在本阶段删除 Legacy Launch C++ 分支或其兼容测试；删除必须由下一独立切片完成零引用、回归和用户批准门禁。

## 3. Approved Paths 与所有权边界

### Gemini 可修改

- Source/PolyQuest/Public/AbilitySystem/Abilities/PlayerLaunchReactionAbility.h
- Source/PolyQuest/Private/AbilitySystem/Abilities/PlayerLaunchReactionAbility.cpp
- Source/PolyQuest/Private/Tests/PlayerLaunchReactionRootMotionAutomationTests.cpp（新增）

### Main 批准的 Unity Build hygiene 例外

- Source/PolyQuest/Private/Tests/EnemyLaunchReactionRootMotionAutomationTests.cpp
  - 仅将 `TagBlockFacing` 从匿名命名空间移入 `RunTest()` 局部作用域，以消除 Unity Build 下与另一测试文件的 C4459 名称遮蔽。
  - 该例外不改变 Enemy 运行时逻辑、GameplayTag 契约或测试断言语义；除上述单点外不得扩展修改。

### Main-only

- plan.md
- ARCHITECTURE.md
- ROADMAP.md
- ROADMAP-archive.md

### 绝对排除

- Source/PolyQuest/Private/Character/Player/PlayerCharacter.cpp 及其 Header
- Source/PolyQuest/Private/AbilitySystem/Abilities/DodgeAbility.cpp/.h
- Enemy Ability、Enemy Controller、HitReactionAutomationTests.cpp、PlayerActionWindowAutomationTests.cpp 的源码修改（不含上方唯一 Unity Build hygiene 例外）
- Config/Tags/PolyQuestGameplayTags.ini（所需 Tag 已存在）
- 所有 Content/**、.uasset、.umap、外部导入资源和用户 WIP
- 任何其他清单外 Source、Tag、Input、Config、共享资产迁移或格式化输出

## 4. 冻结运行时契约与接口

### Native/反射接口

- 在 UPlayerLaunchReactionAbility 私有区域增加：
  - RootMotionKnockdownMontage：EditDefaultsOnly、BlueprintReadOnly、默认 nullptr。
  - bUseGroundedRootMotionKnockdown：EditDefaultsOnly、BlueprintReadOnly、默认 true。
  - ELaunchPhase::RootMotionKnockdown。
  - TeardownOnUnpossessTag 成员，并将已有 Ability.Action.Teardown.OnUnpossess 加入 AbilityTags。
- 保留现有 ActivationOwnedTags：State.Action.HitReacting、State.Input.Block.Movement、State.Input.Block.Jump；不手动移除 GAS 自动拥有的标签。
- 保留 BlockAbilitiesWithTag.Num() == 10 和 AbilitiesToCancel.Num() == 11；teardown selector 不得进入 AbilitiesToCancel。
- 增加私有候选、朝向和 payload helper：
  - IsRootMotionKnockdownCandidate
  - IsLegacyLaunchCandidate
  - TryResolveRootMotionFacingYaw
  - IsEventFromMontage(Payload, ExpectedMontage)
  - Root/Takeoff/Landing wrapper 只补各自 active-Montage 条件，不复制 SlotAnimTracks 遍历。
- WITH_DEV_AUTOMATION_TESTS 下增加 Root 属性、phase、candidate、朝向、回调和 task 查询 seams，以及 bTestBypassMontageActiveCheck。该 bypass 只用于测试中的 Montage_IsActive 启动断言，EndAbility 时重置；现有 bTestBypassAnimInstanceActiveCheck 继续只服务 payload active 检查。

### Candidate 与分支选择

- Common validation 必须确认 ASC、存活且未销毁的 Player、AnimInstance、CMC、必要 Tag 和 grounded 状态。
- Root candidate 必须同时满足：flag 开启、Montage 非空、HasRootMotion() 为真、SlotAnimTracks 非空、播放长度有限且大于零、CMC->MovementMode == MOVE_Walking。Root candidate 不依赖旧 Montage 或速度参数。
- Legacy candidate 必须满足旧 Takeoff/LandingRecovery Montage、IsMovingOnGround()、LaunchHorizontalSpeed、LaunchVerticalSpeed、FacingTurnRateDegreesPerSecond 均有限且为正；该候选仅保留给 dormant compatibility 和后续退休测试，不是当前 authored CDO 的验收路径。
- CanActivateAbility 保留 Super 结果加 common validation 和两个 candidate 的 OR，以维持源码兼容边界；当前 authored CDO 的 Legacy Montage 字段故意为空，因此真实 Root 失败路径只能 fail-closed。Root 优先，分支在任何副作用前冻结。
- Root 分支一旦选定，任务创建、冲击方向解析、Montage 启动或同步重入失败都只进入 EndAbility，不得切换到 Physics fallback。

### Root Motion 启动与朝向

- Root 分支先创建 Commit 以外的 Montage/Cancel listeners，CommitAbility 成功后绑定 Avatar、AnimInstance、ActiveMontage 和 RootMotionKnockdown phase。
- 按固定顺序停止残留移动：停止竞争动作后检查 HasAnyRootMotion；随后调用 MovementComponent->StopMovementImmediately()，再解析并一次性写入 Yaw。
- 由 FHitReactionImpactResolver 冻结 target-local Target -> Attacker 平面方向和参考 Yaw；零、近零、NaN、Inf、缺少有效上下文时不写变换并结束 Ability。
- 保存 bCanWalkOffLedges、设置为 false、绑定 Montage/MovementMode 委托，再调用 ReadyForActivation()。一旦 bLedgeSettingModified 为 true，任何出口都必须调用 EndAbility 或在同步 EndAbility 后立即返回，不得裸 return。
- ReadyForActivation() 后检查 bEndAbilityRequested 和 Montage_IsActive；测试只可通过 bTestBypassMontageActiveCheck 绕过后一项。Root Motion 阶段任何非 MOVE_Walking 变化都调用统一清理。
- Root Montage 自然结束或被中断都结束 Ability；不拆出独立 LandingRecovery phase，不暂停 Montage，不创建 Falling watchdog。

### Legacy compatibility（deferred retirement）

- 旧 Takeoff -> TurningToLaunch -> AwaitingAirborne -> Airborne -> LandingRecovery 的提交、FacingTask、LaunchCharacter、watchdog 和恢复 Montage 源码语义暂保持不变，但不属于当前 authored Player CDO 的运行时验收路径。
- 当前 `GA_PlayerLaunchReaction` 故意将 `Takeoff Montage` 与 `Landing Recovery Montage` 置空；Root candidate 失败时 Ability fail-closed，不再承诺 Physics fallback。
- Legacy 分支和对应测试仅作为下一独立退休切片前的兼容残留；退休前不得重新把它当作本阶段 Root Motion 的成功/失败对照路径。
- Root 分支不得触碰或消费 Event.Reaction.Launch.Commit；Legacy 分支不得误用 Root Motion phase。

### Dodge cancel 与事件任务

- CancelBeginTask 和 CancelEndTask 必须调用 WaitGameplayEvent(..., false, true)：OnlyTriggerOnce=false、OnlyMatchExact=true，保持持续监听。
- 错误 Instigator、Target、Montage 或 Sequence 只被忽略，不得销毁 listener；重复 Begin/End 由现有 SetDodgeCancelable 的幂等保护处理。
- IsEventFromMontage 统一验证 Avatar、Instigator/Target、OptionalObject 等于 ExpectedMontage 或其 Slot 内 Sequence；LandingRecovery 与 Root wrapper 另行确认对应 Montage 当前 active，测试 bypass 只放宽该检查。
- Root Motion phase 中合法 Begin/End 才添加/移除 State.Action.CanCancel.Dodge；EndAbility 无条件清除本 Ability 的残余 cancel 状态。
- 现有 UDodgeAbility 已在该 Tag 存在时取消 Ability.Reaction.Player.Launch；不修改 Dodge 代码，不实现 Tech-Roll。

### EndAbility/teardown

- EndAbility 以 bEndAbilityRequested 防重入，结束所有 AbilityTask，解绑 Montage/MovementMode 委托，停止属于本 Ability 的 Active Montage，清空快照和 phase。
- 仅在 Player 有效且 bLedgeSettingModified 时恢复保存的 bCanWalkOffLedges；不清除不属于本 Ability 的 Root Motion source。
- PlayerCharacter::UnPossessed() 已按 Ability.Action.Teardown.OnUnpossess 调用 ASC->CancelAbilities；新增 AbilityTags 后由现有路径触发清理。
- Player 真实终端死亡 owner 仍由既有 TODO-03D 路线负责；本阶段保证死亡导致的外部取消到达时清理完整，不新增死亡监听或死亡架构。

## 5. 实施顺序

1. Main 在本 plan 生效后保持 main 分支，刷新工作树快照；Gemini 先阅读本文件、目标 Header/CPP、PlayerCharacter UnPossessed、DodgeAbility、Enemy 07B11 Root Motion 和既有 Automation。
2. Gemini 先完成 Header 的属性、phase、Tag、helper 与测试 seams，再改 Constructor/validation。
3. 在 ActivateAbility 中先做 candidate 判定，再按 Root Motion 固定顺序实现任务、Commit、竞争取消、残留 Root Motion 检查、StopMovementImmediately、一次性 Yaw、ledge 保护、委托和 Montage 启动。
4. 扩展 Montage end、MovementMode、payload 和 EndAbility；每个 ReadyForActivation() 后立即处理同步 EndAbility 重入。
5. 新增专项 Automation，先覆盖 CDO/candidate/身份边界，再覆盖 Root/Legacy 生命周期和所有清理出口。
6. Gemini 做批准范围内静态自审并交付 diff；Main 再做 diff-first Fresh Review、用户门禁解释和文档收口。

## 6. User-owned Editor Readback

由用户在 Unreal Editor 中完成；Codex/Gemini 不直接编辑二进制资产。

- 读取实际 GA_PlayerLaunchReaction 资产路径、类类型和 CDO：RootMotionKnockdownMontage、bUseGroundedRootMotionKnockdown、Ability.Reaction.Player.Launch、Ability.Action.Teardown.OnUnpossess、State.Action.HitReacting、State.Input.Block.Movement、State.Input.Block.Jump、Legacy Montage 字段的有意置空状态和 cancellation 集合。
- 读取实际 Root Motion Montage：Root Motion enabled、有效 Slot/Slot Group、有限正长度、连续起身/滑行/击倒/恢复段，以及 UAnimNotifyState_ActionDodgeCancelWindow Begin/End。
- 读取实际 Player Mesh AnimBP 的 Root Motion Mode、Montage Slot 兼容性和 CMC grounded Walking/胶囊碰撞设置。
- 不要求为本阶段配置旧 Takeoff/LandingRecovery fallback；记录其有意置空的 no-adoption 决策，并将 Legacy C++ 退休归入下一独立切片。
- 用户提供的 GA CDO、Root Motion Montage/Sequence readback 加上用户亲自确认的 PIE，可关闭本阶段 AuthoredReadback 债务；该证据仍不扩写为 clean-checkout authored baseline 或 packaging 证明。
- 若 Editor readback 发现真实 Warp Window 且存在动态目标/距离需求，停止本切片，不在本批准路径内临时加入 Motion Warping。

## 7. Automation 与用户验证矩阵

### 新专项套件

新增 PolyQuest.Combat.PlayerLaunchReactionRootMotion，使用 transient UAnimSequence/UAnimMontage/Slot fixture，不依赖 Content 资产，至少覆盖：

- CDO 默认值、teardown Tag、既有 owned/blocked Tag、BlockAbilitiesWithTag 和 AbilitiesToCancel 数量。
- Root candidate 优先、Root-only authored CDO、flag 关闭/Root 无效时 fail-closed、空 Montage、无 Root Motion、无 Slot、零/非有限长度、非 Walking，以及 Legacy-only dormant compatibility 和双无效矩阵。
- Root 分支不创建 Commit/Facing/Falling task、不写 Physics velocity、不调用 LaunchCharacter；Commit event 在 Root phase 被忽略。
- 有效、零、NaN、Inf target-local 方向与一次性 Yaw；Yaw 不被 Lock-On/locomotion Tick 覆盖。
- StopMovementImmediately 清除已有速度；残留 Root Motion、Montage 启动失败和非 Walking 均 fail-closed。
- WaitGameplayEvent(false, true) 持续监听：错误 payload 不消费 listener，合法 Montage/Sequence payload 可用，重复窗口保持幂等。
- Root 自然结束、打断、显式取消、模拟死亡外部取消、Destroy、UnPossess 的 Montage、task、Dodge tag、ledge 值、快照和 phase 清理；Legacy-only 行仅保留兼容测试，不作为 authored PIE 验收。
- bTestBypassMontageActiveCheck 开启/关闭两条启动断言路径。

### 既有回归套件

- PolyQuest.Combat.HitReaction
- PolyQuest.Player.ActionWindows
- PolyQuest.Combat.EnemyLaunchReactionRootMotion
- PolyQuest.Enemy.RootMotionFacing
- PolyQuest.Player.LockOn

### 用户门禁

- 手动编译 PolyQuestEditor (Development Editor)。
- 在 /Game/Maps/Scene01 覆盖 Player 平地、轻坡、台阶、墙/角落、边缘、摄像机跟随与既有 Launch Shake/FOV、移动/跳跃阻断、作者 Dodge cancel、自然恢复和普通 locomotion。
- 覆盖 Root flag 关闭或 Root Montage 无效时 authored CDO 的 fail-closed，以及取消、死亡、销毁、UnPossess；Legacy branch 不作为本阶段 PIE fallback 验收。
- 复跑 Enemy 07B11 地面/边缘/中断回归，确认 Player 改动无跨角色影响。
- 编译、Editor readback、Automation、PIE、视觉和网络证据严格分开记录；不得互相替代。

## 8. Gemini Handoff、停止条件与收口

- Gemini 只能修改本 plan 的 Approved Paths；仅可按上方限定执行 Unity Build hygiene 例外，不能修改 plan.md、ROADMAP.md、ARCHITECTURE.md、ROADMAP-archive.md、Content、Config、Build.cs 或任何资产。
- 交付必须包含：批准路径 diff、git diff --check 结果、Automation 结果和新增覆盖、未运行的用户门禁、资产 no-adoption/readback 状态、以及对五项生命周期检查的自审说明。
- 首次交付前完成一次干净只读实施自审，逐项核验 persistent WaitGameplayEvent、bTestBypassMontageActiveCheck、通用 payload helper、StopMovementImmediately、ledge fail-closed 恢复、ReadyForActivation 重入和单一 EndAbility。
- 发现清单外 Source/Tag/Input/Config、资产迁移、Motion Warping 需求、墙体专属新表现、网络逻辑或死亡架构要求时立即停止并报告，不自行扩展。
- 每个静态、编译、Automation 或运行时根因最多允许一次有证据修复和一次定向重跑；保留首个失败证据。
- Main 在用户门禁和 Fresh Review 完成后，才更新 ARCHITECTURE.md、ROADMAP.md、ROADMAP-archive.md；只有用户明确批准才暂存/提交，不得 git add -A。

## 9. 当前计划状态与已知债务

- 本轮已完成 07B12 archive preflight 并将本文件切换为 07B13 active handoff。实现交付包含三份 Player 批准路径，以及一份仅用于 Unity Build hygiene 的 Enemy 测试例外；未修改资产、Config、Build.cs、Player/Enemy 运行时逻辑或其他文档。
- 当前 authored `GA_PlayerLaunchReaction` 明确采用 Root Motion-only：`RootMotionKnockdownMontage = AM_TakeOff`、开关开启、Legacy `Takeoff/LandingRecovery` 字段有意置空。Legacy C++ 分支暂留，下一独立切片退休。
- 用户提供 GA CDO、`AM_TakeOff` Montage/Slot/Notify、Root Motion Sequence readback，并亲自确认 `PolyQuestEditor (Development Editor)` 编译及 Scene01 PIE；该证据归属于用户，不由本次静态审查重跑。
- Main Fresh Review：在批准 diff 内未发现可定级的 P0-P2 运行时缺陷；`code-review-graph` 的风险分数和未覆盖提示仅作静态导航，不替代编译、Automation、Editor readback 或 PIE 证据。
- `Debt-07B13-AuthoredReadback` 已由上述用户 Editor readback 与 PIE 证据关闭；不因此扩写为 clean-checkout authored baseline、包装或发布证明。
- 07B14 的 Dodge Motion Warping 与 Player/Enemy asymmetric air-reaction policy，以及 Legacy C++ 退休，均不属于本阶段，必须由新的证据和批准范围触发。
