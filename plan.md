# TODO-03A7B：Light Combo Motion-Warp Adoption v1

## 摘要

唯一推荐阶段是：把已完成的玩家轻攻击 Combo entry 0 Motion Warping 合同，扩展到明确选中的零基 entry 1/2；不扩展为“主角所有近战攻击”的通用系统。

提交基线是 `E:\GameDevelop\PolyQuest` 的 `main @ c91fbfdaab49e933922e902b92565859506f9351`。`c8b72c6` 是上一阶段 TODO-03A7 的 entry 0 实现，`c91fbfd` 是最近的文档/装备测试路径收口。`90ad381` 继续作为 TODO-03A7 当时的历史父基线，不改写成当前仓库基线；它反映的是当时最新提交尚未落地的任务视角。

实施前工作树不是 clean checkout：2026-08-30 的 `git status --short` 共 325 项（286 个 tracked、39 个 untracked），其中 318 个 `Content/**`（279 个 tracked、39 个 untracked）、1 个 `Config/**`、4 个 `Source/**`，另有 `plan.md` 与 `ROADMAP-archive.md` 文档 WIP。现有 Source WIP 是 `LightAttackAbility.h/.cpp`、`PlayerCharacter.cpp` 和 `PlayerMeleeMotionWarpingAutomationTests.cpp`；它们是本阶段当前工作树起点，Gemini 必须在其上增量修改，不得重置、覆盖、清理或把未批准的 `Config/**`、`Content/**`、其他文档/Source WIP 纳入本阶段。

## 当前收口状态（2026-08-31）

- 六个批准 Source/test 文件的 entry 0..2 adoption 与生命周期修复已完成；用户已确认 focused `PolyQuest.Combat.PlayerMeleeMotionWarping` Automation 和 `/Game/Maps/Scene01` PIE 通过，Main 已完成批准范围内的 defect-first fresh review，未发现 P0/P1/P2 blocker。
- Gemini 报告 Rider errors-only 检查和 `git diff --check` 通过。本记录没有独立的手动 `PolyQuestEditor` Development Editor 编译或 Motion-Warp Editor readback，因此不宣称 clean authored baseline；该验证债务的关闭触发器保留在 `ROADMAP.md`。
- 当前阶段状态是“实现完成、作者化验证债务待收口”，不是完整 authored closure；后续计划指针为 `TODO-03A7C`，不得据此跳过后续用户门禁。

## 目标与玩家体验问题

以提交基线 `c91fbfd` 的生产行为看，`ULightAttackAbility::StartComboEntry()` 只在 `EntryIndex == 0` 时尝试 Motion Warping；后续轻攻击段落即使有合适的 Root Motion，也可能在锁定目标前停止或出现明显空挥。当前工作树已有 Gemini 的 entry 1/2 adoption WIP，本阶段 handoff 是在该 WIP 上完成并修正合同，不得按 clean checkout 重做或覆盖已有实现。

本阶段解决：

- entry 0 保持现有行为并做回归；
- entry 1、entry 2 各自独立 opt-in；
- 只有通过资产 readback 和 PIE 证明适用的段落才开启；
- 不适用的段落保持 `bUseMotionWarping=false`，普通连段继续工作。

`PolyQuest.uproject`、`PolyQuest.Build.cs`、`FComboChainEntry` 的 Motion-Warp 字段、Player-owned `UMotionWarpingComponent` 和现有 evaluator 已存在，不需要重复建设。

## 冻结的运行时契约

1. **显式索引门控**

   只允许零基 entry `0..2` 进入 Light Combo Motion-Warp 路径。即使未来 entry 3 或更高段落错误地把 `bUseMotionWarping` 打开，也必须 fail-closed；不能简单删除 `EntryIndex == 0` 判断而隐式采用未来资产。

2. **每段独立 opt-in**

   每个 entry 使用自己的 `bUseMotionWarping`、`WarpTargetName`、`WarpStopDistance`、`MaxWarpDistance` 和 `MaxWarpAngleDegrees`。禁用段落必须先清掉上一段的 Warp Target，再继续普通 Montage/Trace/Combo 逻辑。

3. **一次性 Combo 目标快照**

   默认采用“首个合法 opt-in 段落懒捕获”：

   - “合法 opt-in”同时要求 entry 索引在 `0..2`、`bUseMotionWarping=true`，且 Warp Target Name、有限值、非负距离和 `[0,180]` 角度等基础配置通过；禁用或配置非法的 entry 不读取 Lock-On，也不消耗本次捕获机会；
   - 第一个合法 opt-in entry 在任何目标查询前先把 `bTargetCaptureAttempted` 置为 `true`，然后只读取一次 `GetLockedTarget()`；缺失有效 Player、ASC 或 Controller 也算本次捕获失败；
   - 原目标必须先通过 UObject 有效性/销毁检查，再检查存活，并且 `ResolveValidLockedTarget()` 必须返回同一个目标；死亡交接、自动替换或验证失败均 fail-closed；
   - 捕获成功后保存目标的弱引用、有限的 Actor-center 位置和**捕获当时**的目标接地布尔值；即使该 entry 的几何评估或 Player bridge 写入失败，也保留这份快照供后续合法 entry 复用；
   - entry 0 关闭时，entry 1/2 可以成为首次捕获者；
   - 一旦首次捕获尝试失败，本次 Ability 生命周期内不得因锁定变化而再次选敌；
   - 后续 entry 先检查缓存弱引用仍然有效且目标未销毁，再检查存活；失败时清理 Player Warp Target 并 fail-closed；不得因失败重新捕获；
   - 后续 entry 不再读取当前 Lock-On、不调用 `ResolveValidLockedTarget()`、不读取目标当前位置或当前接地状态，只使用缓存位置和缓存接地布尔值；
   - 每个后续 entry 仍可用自己的参数、当前 Player 位置/朝向和当前 Player 接地状态重新调用同一个纯 evaluator，因此能独立通过或拒绝，但不会跟随目标移动。

   Ability 必须拥有非反射、Ability-instance-owned 的快照状态（至少包含捕获尝试标志、`TWeakObjectPtr<AEnemyCharacter>`、缓存位置和缓存目标接地状态）。`ResetMeleeMotionWarpState()` 必须幂等：在 `ActivateAbility()` 开始处，以及激活准备失败和 `EndAbility()` 首次实际清理路径执行，清空弱引用、位置、接地状态和捕获尝试标志；它只重置 Ability 内部状态，不替代 Player bridge 的目标清理。**单个 entry 的几何/bridge 失败不得重置已成功捕获的快照**，否则后续 entry 无法复用；状态不能跨 Ability 激活泄漏。

4. **写入与清理顺序**

   `StartComboEntry()` 在新 Montage 真正激活前：

   - 清理上一段全部 Player melee Warp Target（包括禁用、非法、超出索引和评估失败的 entry）；
   - 依据批准索引和 entry opt-in 决定是否尝试应用；
   - 通过 `APlayerCharacter::SetMeleeMotionWarpTarget()` 写入静态 Transform，并检查其 `bool` 返回值；bridge 拒绝时不得假报成功或重选目标；
   - 启动失败、同步结束、Montage identity/active 检查失败、entry 替换、取消、自然结束、目标死亡、离地、UnPossess 和 EndPlay 后不得留下 stale target。

   不新增 Player 组件、不把组件移到 `ABaseCharacter`，不新增通用 Warp dispatcher、Tick、Timer、追踪或自动重选。

5. **最小生命周期窄桥（本轮扩大边界）**

   - `APlayerCharacter::ClearLockedTarget()` 每次调用都必须清理 `ClearMeleeMotionWarpTargets()`，即使当前弱引用已经无效；不得改变 Lock-On acquisition/cycle 或死亡重选的既有语义。
   - 在 `APlayerCharacter` 增加仅供 C++ 的窄桥 `ClearMeleeMotionWarpTargetsForInvalidatedTarget(const AEnemyCharacter* InvalidatedTarget)`（不加 `UFUNCTION`、Delegate、Tag、Input 或 RPC）。它只在 `LockedTarget` 仍指向该目标时清理 Warp Target，不得清除 `LockedTarget` 或 `LastValidLockedTargetCandidate`；现有 `ValidateCurrentLockedTarget() -> TryRetargetAfterLockedTargetDeath()` 仍是唯一一次性死亡 retarget 路径。
   - `AEnemyCharacter::HandleDeath()` 在死亡 teardown 开始处、`AEnemyCharacter::EndPlay()` 在 `Super::EndPlay()` 前，通过当前单机 Player 的一对一直接查找调用上述桥；桥接必须在目标进入不可比较状态前按弱引用/指针身份核对、检查 Player/World、幂等清理。`EndPlay` 覆盖 Destroyed teardown；不得新增全局 Dispatcher、广播 Delegate、Tick、Timer、持续跟随或自动重选。
   - `APlayerCharacter::UnPossessed()` 保持现有 Warp 清理先行，然后在 `Super::UnPossessed()` 前通过 ASC 和精确的 `Ability.Attack.Light` Gameplay Tag 只取消 Light Combo Ability，再继续既有委托/交互清理；不得用 `CancelAllAbilities()`，不得连带取消 Guard、Parry、Bow、Dodge 或其他能力，并保证取消回调重入安全。
   - `StartComboEntry()` 必须在所有早退路径（无效 Entry/Bound Anim/Montage、Montage Task 创建失败、同步 `ReadyForActivation()` 结束、Montage identity/active 检查失败等）之前尽可能取得当前 Player 并清掉旧 Warp Target；Player 无效时只安全返回，不得解引用。`TryApplyMeleeMotionWarpTarget()` 每次使用都检查当前 Player、Controller、ASC、World，以及缓存目标的 UObject 有效性、`IsActorBeingDestroyed()`、World 一致性和死亡状态；任何失败都清理并 fail-closed。
   - `bTestBypassMontageActiveCheck` 必须在 Ability 激活和结束边界重置；测试标志最多绕过最终 `Montage_IsActive` 结果，不得绕过真实 `UAbilityTask_PlayMontageAndWait` 创建/激活、目标验证、清理或 evaluator。
   - `PlayerCharacter.cpp` 现有左上角实时距离 Debug HUD（`#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)`、稳定 debug message key `1002`）必须保留其 debug-only 性质和输出语义；生命周期修复不得把它变成 gameplay 真值、不得移入 Shipping，也不得借机重构 Tick。若需防止销毁目标解引用，只加等价的有效性保护。

6. **GAS 与伤害边界**

   `ULightAttackAbility` 继续保持现有 `InstancedPerActor`、`ServerOnly`、成本、状态标签、Combo Window、Rate Window 和 `EndAbility()` 生命周期。

   唯一近战伤害路径保持：

   `UAbilityTask_MeleeTraceWindow -> FMeleeHitResolver -> Damage GameplayEffect`

 Motion Warping 只影响 Root Motion 接触表现，不改 ASC、AttributeSet、GameplayEffect、Trace、Resolver、Lock-On、Bow/Projectile、Guard/Parry 或 Enemy AI。

## Gemini 审阅反馈的决策

- **采纳**：Ability 私有快照的显式生命周期重置、弱引用与死亡/销毁防御、缓存目标接地状态、entry 上限具名常量，以及全部测试 seam 的 `WITH_DEV_AUTOMATION_TESTS` 隔离。
- **澄清而非照搬**：本计划继续使用“首个**合法** opt-in”定义。`bUseMotionWarping=true` 但名称/数值配置非法的 entry 不读取 Lock-On、也不消耗捕获机会；只有配置合法后真正开始捕获，失败才锁定本次 Ability。否则 entry 0 的坏配置会意外阻断 entry 1/2 的合法采用。
- **保留快照**：首个合法 entry 已捕获目标但当前几何或 bridge 写入失败时，不重置快照；后续合法 entry 仍可用自己的参数重评估。只有 Ability 激活边界或终止清理才重置快照。
- **收窄 seam**：无头测试最多绕过最终 `Montage_IsActive` 结果门槛，不能跳过真实 Montage task 创建/激活、目标验证、清理或 evaluator；task 本身无法建立时停工回报。
- **本轮生命周期复核**：当前 WIP 已覆盖 entry adoption 和 Ability 内部快照，但仍有跨对象清理缺口：`ClearLockedTarget()` 不保证同步清 Warp、Enemy 死亡/销毁可能等到下一帧才清理、`UnPossessed()` 未精确取消 Light Combo、`StartComboEntry()` 的最前置早退可能遗留目标，且测试 bypass 标志没有完整的 Ability 生命周期复位。只通过 Player/Enemy 一对一桥接和取消路径补齐这些缺口，不改变 Lock-On retarget、伤害链或资产范围。
- **调试信息决策**：现有 `PlayerCharacter.cpp` 左上角实时距离 HUD 是用户明确要求保留的 debug instrumentation；它不属于本阶段 gameplay 合同，不能因生命周期修复被删除、迁移或升级为运行时真值。

## 批准的实现路径

实现阶段只允许修改：

- `E:\GameDevelop\PolyQuest\Source\PolyQuest\Public\AbilitySystem\Abilities\LightAttackAbility.h`
- `E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\AbilitySystem\Abilities\LightAttackAbility.cpp`
- `E:\GameDevelop\PolyQuest\Source\PolyQuest\Public\Character\Player\PlayerCharacter.h`
- `E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\Character\Player\PlayerCharacter.cpp`
- `E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\Character\Enemy\EnemyCharacter.cpp`
- `E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\Tests\PlayerMeleeMotionWarpingAutomationTests.cpp`

文件级职责与约束：

- `LightAttackAbility.h/.cpp`：保持 `EvaluateMeleeMotionWarpTransform()` 唯一生产数学入口；维护非反射、Ability-instance-owned 快照、具名 entry 上限门控、捕获/复用/清理、所有 `StartComboEntry()` 早退清理，以及测试 seam 生命周期。不得改 Damage GE、Trace Task、Combo Window、成本、输入、其他 Ability 或生产 public/Blueprint API。
- `PlayerCharacter.h/.cpp`：只增加 `ClearMeleeMotionWarpTargetsForInvalidatedTarget(const AEnemyCharacter* InvalidatedTarget)` 这一 C++ 窄桥，并在 `ClearLockedTarget()`、`UnPossessed()` 接入清理/精确 Light Ability 取消；UnPossessed 保持 Warp 清理先行、Light-only cancel、再执行既有委托/交互清理和 `Super`。不得改变 Lock-On acquisition/cycle、死亡 retarget、Bow、装备、Guard/Parry、Movement 或现有 Tick 语义。左上角实时距离 Debug HUD 必须保留原 debug-only 宏、message key `1002` 和输出语义。
- `EnemyCharacter.cpp`：只在 `HandleDeath()` 与 `EndPlay()` 通过当前单机 Player 的一对一直接查找调用上述 invalidation bridge；不得修改 Enemy AI/StateTree/Poise/Death 语义、Enemy header、全局事件或新增目标系统。
- `PlayerMeleeMotionWarpingAutomationTests.cpp`：只扩展现有 `PolyQuest.Combat.PlayerMeleeMotionWarping`，通过真实生产入口和现有 fixture 验证生命周期；不得复制 evaluator 数学、直接操作组件内部数组或创建第二套目标写入路径。
- 生产 public/Blueprint API 不增加新的 Gameplay Tag、Input、Config 或 DataAsset 字段；不改 `ComboChainDataAsset.h`、`PolyQuest.uproject`、`Build.cs`、Config、Content、其他 Ability、Trace/Resolver、Projectile/Bow、Enemy header。

为覆盖生产入口，允许在 `WITH_DEV_AUTOMATION_TESTS` 下增加极窄 seam：

- 设置测试用 `ComboDefinition`、`CurrentActorInfo` 和 `BoundAnimInstance`；
- `TestStartComboEntry(int32)` 直接调用真实 `StartComboEntry()`；如无头夹具无法让 transient Montage 通过最终播放状态检查，可用一个明确命名的测试标志**仅绕过 `Montage_IsActive` 结果门槛**，仍必须创建/激活真实 `UAbilityTask_PlayMontageAndWait` 并执行 entry 门控、清理、快照、evaluator 和 Player bridge；task 创建或 `ReadyForActivation()` 本身若无法建立，立即停止回报，不扩大 bypass；
- seam 的声明、定义和所有调用点都必须由 `#if WITH_DEV_AUTOMATION_TESTS` 包裹，测试标志默认关闭并在 Ability 激活/结束生命周期重置，不进入 Shipping/正式运行时 API；
- 测试可以使用现有 Player fixture/test hook 安排真实 Lock-On、Controller 和接地前置，但不得直接注入 Ability 的“已捕获目标”结果，也不得复制几何公式来代替真实 `StartComboEntry()` 路径。

如果该 seam 需要额外文件、Engine 修改或改变生产 Montage 行为，立即停止并返回证据，不扩大范围。

## Automation 计划

继续使用现有套件：

`PolyQuest.Combat.PlayerMeleeMotionWarping`

重点补齐：

- entry 0/1/2 opt-in 成功路径；
- entry 0/1/2 disabled 时不写目标；
- entry 3+ 即使错误 opt-in 也不采用；
- 禁用/非法 opt-in 不消耗捕获机会，随后合法 entry 可以首次捕获；合法 entry 首次无锁或验证失败后，后续新锁定不得重试；
- entry 替换先清旧目标，新的 opt-in 才写新目标；
- entry 0/1/2 捕获后改变 Lock-On、移动目标或目标接地状态，后续 entry 仍使用原始静态位置/接地快照；
- 捕获成功但首段几何/bridge 失败时，后续合法 entry 可复用同一快照；目标死亡/销毁后不得复用；
- Ability 结束后再次激活不会继承旧快照或失败锁定标志；
- 不同 entry 参数和 Warp Target Name 的独立使用；
- 过近、修正距离、角度、接地、NaN/Inf、空名称和非法配置的 fail-closed；
- `ClearLockedTarget()` 立即清理 Warp Target；Enemy `HandleDeath()`/`EndPlay()` 的一对一 invalidation bridge 立即清理当前锁定目标的 Warp，但保留既有一次性死亡 retarget 所需的锁定候选；Destroy/销毁后不得复用；
- `UnPossessed()` 只取消 `Ability.Attack.Light`，并与既有清理顺序安全重入；
- `StartComboEntry()` 的无效 Entry、缺失 Anim/Montage、Montage Task 创建失败、同步结束和 inactive identity/active 检查等早退路径均无残留；
- 启动失败、同步结束、取消、自然结束、目标死亡、UnPossess、Falling、EndPlay 后无残留；
- `bTestBypassMontageActiveCheck` 在 Ability 生命周期边界复位且不能绕过真实 Montage Task 创建/激活；
- 既有 entry 0 行为、Montage identity、Combo continuation 和普通 Trace/伤害入口不回归。

测试中的 `100/60/60` 仍只是边界夹具；生产默认值保持 `190cm / 110cm / 60°`。

无法在无头夹具中证明真实 Notify、Root Motion 或视觉接触的部分，只能标为 seam/static coverage，不得冒充 Editor 或 PIE 证据。

## 用户资产与验证门槛

用户拥有 `.uasset`/`.umap` 作者化和运行验证。本阶段不手工编辑、导入、移动、复制、重定向或提交资产。

必须由用户在 Editor readback 中逐项确认：

- `DA_Combo_StraightSword` 的 entry 0/1/2 当前 opt-in 状态；
- 每个启用 entry 对应的 `AM_Sword_LightAttack01/02/03` 直接包含 `AnimNotifyState_MotionWarping`；
- Modifier、目标名、平移/旋转设置和 Notify 时间窗一致；
- 每个采用 Montage 的 Root Motion 可由 `ABP_Player_Dungeon` 驱动；
- Player 只有一个 `UMotionWarpingComponent`，且 `bSearchForWindowsInAnimsWithinMontages=false`；
- 不得凭文件名推断 Root Motion、Notify、Modifier 或目标名已经正确。

证据分层：

- **静态**：最终 diff、CodeGraph 定点调用链、必要的 code-review-graph 影响提示、Rider lint/problem、`git diff --check`。图结果不是运行时证明；当前 code-review-graph 索引落后时只作补充导航。
- **编译**：用户在 VS2022 手动编译 `PolyQuestEditor`（Development Editor）。
- **Automation**：用户运行上述 focused suite，以及 Light Attack、Melee Trace/Resolver、Lock-On、Projectile/Bow、Guard/Parry 回归套件；数量按实际记录。
- **Editor readback**：逐项确认 entry 1/2 的作者化 adoption 条件。
- **PIE**：用户在 `/Game/Maps/Scene01` 验证 entry 0/1/2 连段、无锁、过近、超距离、超角度、空中、目标移动、目标切换、取消、死亡/销毁、`ClearLockedTarget()`、UnPossess、重新 Possess、地图 teardown，以及 Trace/伤害、Lock-On、Bow 无回归；额外确认死亡时 Warp 立即清除但既有一次性 retarget 仍只发生一次，UnPossess 只终止 Light Combo。

（实施前记录）TODO-03A7 已有的 focused Automation 和 Scene01 PIE 结果只能作为 entry 0 历史证据；当时的 TODO-03A7B Automation/PIE 结果不自动覆盖后来新增的生命周期桥。实施后的生产入口覆盖、用户证据和剩余债务以本计划“当前收口状态”及末尾“实施与文档收口记录”为准。缺少 readback 不是 no-adoption；只有实际配置不适用或 PIE 失败才可记录 evidence-backed no-adoption。当前工作树的左上角距离 HUD 仅是用户要求保留的 debug instrumentation；静态存在或显示本身不构成 gameplay/PIE 证明。

## 成功标准与关闭条件

本阶段只有在以下条件全部满足后才能标记完成：

- entry 0 回归通过；
- entry 1 和 entry 2 各自得到明确结果：`adopted` 或基于真实 readback/PIE 的 `no-adoption`；
- `StartComboEntry` 的替换、快照、不重选和清理矩阵通过；
- 快照在 Ability 激活/结束边界正确重置，且首次失败锁定与后续弱引用死亡防御有生产入口覆盖；
- `ClearLockedTarget()`、Enemy death/EndPlay invalidation bridge、UnPossess Light-only cancel 和 `StartComboEntry()` 全部早退路径的生命周期矩阵通过；
- 左上角实时距离 HUD 保持 debug-only、无空悬引用/无 Shipping 影响，且不成为 Motion-Warp 或 Lock-On 的状态真值；
- 既有 Trace/Resolver/Damage、Lock-On、Bow、Guard/Parry 无行为回归；
- 用户手动编译、Editor readback、focused Automation、Scene01 PIE 均有独立记录；
- Main 完成一次批准范围内的 defect-first fresh review；
- 文档完成收口后，等待用户明确批准才 staging/commit（本次批准已收到）。

如果两个候选 Montage 都不适用，保持 entry 1/2 关闭也可以关闭本阶段；不能为了“完成 TODO”强行打开资产字段。

## 文档、执行与依赖

执行路线：

- Outer：`ue-stage-workflow`
- Primary：`ue5-cpp-gameplay`
- Support：`ue5-debug-validation`
- Executor：`manual/out-of-band Gemini`
- `Contract owner: Main`
- 本阶段 `Implementation executors: 1（Gemini，已完成实现与 self-review）`；执行路线为 manual/out-of-band，不通过 in-app orchestration 派发。

Main 负责架构、`plan.md`、文档、验证解释、fresh review、staging 和提交；用户负责 Editor authoring、手动编译、Editor readback、PIE/视觉验证和最终 commit approval。TODO-03A7 closeout 已在本轮替换前归档；`ROADMAP.md` 的 `90ad381` 父基线继续保持当时视角，本轮不改写它，也不把它当当前 HEAD；`ROADMAP.md` 只保留轻量里程碑、依赖和债务指针，`ARCHITECTURE.md` 只记录验证后的稳定合同。

依赖顺序固定为：

`TODO-03A7B → TODO-03A7C → TODO-03A7D → TODO-05A → TODO-05B → TODO-07B5 → TODO-07B6 → TODO-03C`

`TODO-03A7E` 仍放在首个远程敌人之后，属于可选 Enemy Motion-Warp 阶段；装备、拾取、AI/StateTree、Poise、Persistence 和其他近战家族不是本阶段前置。

## Gemini 执行提示词（已执行记录）

```text
你是 PolyQuest TODO-03A7B 的实现执行者。工作目录固定为 E:\GameDevelop\PolyQuest；提交基线为 main @ c91fbfdaab49e933922e902b92565859506f9351。当前工作树不是 clean checkout：已有 LightAttackAbility.h/.cpp、PlayerCharacter.cpp、PlayerMeleeMotionWarpingAutomationTests.cpp 的 TODO-03A7B WIP，以及大量 Config/Content WIP；先读取最新 AGENTS.md、当前 plan.md 和 git status，保留这些变更，绝不 reset、checkout、清理或覆盖。以 plan.md 的 TODO-03A7B 条款为唯一执行合同。执行路线是 manual/out-of-band Gemini，不通过 in-app orchestration。Outer Skill 为 ue-stage-workflow，Primary Skill 为 ue5-cpp-gameplay，Support Skill 为 ue5-debug-validation。

【所有权与允许路径】
Contract owner: Main；implementation writer: Gemini。只允许修改以下六个绝对路径：
1. E:\GameDevelop\PolyQuest\Source\PolyQuest\Public\AbilitySystem\Abilities\LightAttackAbility.h
2. E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\AbilitySystem\Abilities\LightAttackAbility.cpp
3. E:\GameDevelop\PolyQuest\Source\PolyQuest\Public\Character\Player\PlayerCharacter.h
4. E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\Character\Player\PlayerCharacter.cpp
5. E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\Character\Enemy\EnemyCharacter.cpp
6. E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\Tests\PlayerMeleeMotionWarpingAutomationTests.cpp
只读参考路径为 `E:\GameDevelop\PolyQuest\AGENTS.md`、`E:\GameDevelop\PolyQuest\plan.md`，以及 Unreal 资产 `/Game/_DataAssets/Player_Combat/DA_Combo_StraightSword`、`/Game/BP/Montages/LightSword/AM_Sword_LightAttack01`、`/Game/BP/Montages/LightSword/AM_Sword_LightAttack02`、`/Game/BP/Montages/LightSword/AM_Sword_LightAttack03`、`/Game/BP/Characters/Player/Animations/ABP_Player_Dungeon`；只能核对，不能写入、导入、移动、复制、重定向或提交。`ROADMAP.md`、`ARCHITECTURE.md`、`README.md`、其他 Source（列出的六个除外）和全部 Config/Content WIP 均不得修改。

共享契约文件的函数边界：
- `LightAttackAbility.h`：只维护非反射的 Ability 私有快照状态/私有 helper，以及 `WITH_DEV_AUTOMATION_TESTS` 下的最小 seam；不得改变现有生产 Blueprint/reflected API 或 `EvaluateMeleeMotionWarpTransform` 签名。
- `LightAttackAbility.cpp`：只调整 `ActivateAbility`、`EndAbility`、`StartComboEntry`、`TryApplyMeleeMotionWarpTarget` 及为本合同所需的同文件私有 helper/具名常量；不得重写其他攻击、Trace、Combo、成本或事件逻辑。
- `PlayerCharacter.h/.cpp`：只增加非反射 C++ 窄桥 `ClearMeleeMotionWarpTargetsForInvalidatedTarget(const AEnemyCharacter* InvalidatedTarget)`，并在 `ClearLockedTarget`、`UnPossessed` 接入本计划的清理/取消；不得改动 Lock-On acquisition/cycle、死亡 retarget、Bow、装备、Guard/Parry 或 Movement 语义。`PlayerCharacter.cpp` 现有左上角实时距离 Debug HUD（`#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)`、message key `1002`）必须原样保留其 debug-only 输出；不得重构 Tick，最多只能添加等价销毁目标保护。
- `EnemyCharacter.cpp`：只在 `HandleDeath` 和 `EndPlay` 调用 Player invalidation bridge；不得修改 Enemy header、AI/StateTree、Poise、Death 业务语义或新增全局事件。
- `PlayerMeleeMotionWarpingAutomationTests.cpp`：只扩展现有 `PolyQuest.Combat.PlayerMeleeMotionWarping` 套件，调用真实 `StartComboEntry` 和现有 Player/Enemy fixture；不得复制 evaluator 数学、直接操作组件内部数组或创建第二套目标写入路径。

【冻结运行时合同】
- Light Ability 继续 `InstancedPerActor`、`ServerOnly`；ASC、AttributeSet、GameplayEffect、GameplayTag、Input、Config、DataAsset 字段和唯一伤害链 `UAbilityTask_MeleeTraceWindow -> FMeleeHitResolver -> Damage GameplayEffect` 均不变。
- 只有零基 entry 0、1、2 允许进入 Motion-Warp 路径；使用 `.cpp` 私有具名常量（值为 2）统一门控，entry 3+ 即使 opt-in 也 fail-closed，不影响普通连段。每次 entry 替换先清 `ClearMeleeMotionWarpTargets`，并检查 `SetMeleeMotionWarpTarget` 的 bool 结果。
- “合法 opt-in”同时要求索引 0..2、`bUseMotionWarping=true`、非空目标名、有限且非负距离、角度在 [0,180]。禁用/非法 entry 不读取 Lock-On、不消耗捕获机会；首个合法 entry 才能把捕获尝试标志置 true，并只读取一次 `GetLockedTarget()`。原目标须先通过 UObject 有效性、`IsActorBeingDestroyed` 和死亡检查，再由 `ResolveValidLockedTarget()` 返回同一指针；失败即本 Ability 生命周期 fail-closed，不因锁定变化重选。
- 成功捕获只保存目标弱引用、有限 Actor-center 位置和捕获时目标接地状态；后续 entry 不再读取 Lock-On、Resolve、目标当前位置或当前接地状态，只用缓存与当前 Player 状态调用同一 evaluator。单个 evaluator/bridge 失败不得清掉成功快照，但必须清理 Player Warp Target。
- `ResetMeleeMotionWarpState` 必须幂等，并在 `ActivateAbility` 开始、激活准备失败和 `EndAbility` 首次实际清理路径执行；状态不得跨 Ability 激活泄漏。`bTestBypassMontageActiveCheck` 同样在 Ability 激活/结束边界复位。

【本轮生命周期窄桥合同】
- `APlayerCharacter::ClearLockedTarget()` 每次调用都清 `ClearMeleeMotionWarpTargets()`，即使当前锁定弱引用无效；不得在此新增 acquisition/cycle 或 retarget。
- `ClearMeleeMotionWarpTargetsForInvalidatedTarget` 只在 `LockedTarget` 仍等于传入 Enemy 时清 Warp，不清 `LockedTarget` 或 `LastValidLockedTargetCandidate`；在 Enemy 进入不可比较状态前按弱引用/指针身份核对。`AEnemyCharacter::HandleDeath()` 在死亡 teardown 开始处、`EndPlay()` 在 `Super::EndPlay()` 前通过当前单机 Player 的一对一直接查找调用它；桥接必须检查 World/对象有效性、幂等，并保留现有 `ValidateCurrentLockedTarget() -> TryRetargetAfterLockedTargetDeath()` 的一次性死亡 retarget。不得使用全局 Dispatcher、广播 Delegate、Tick、Timer 或自动重选。
- `APlayerCharacter::UnPossessed()` 保持 Warp 清理先行，在 `Super::UnPossessed()` 前用 ASC 和精确 `Ability.Attack.Light` Tag 只取消 Light Combo Ability，再执行既有交互/委托解绑；不得 `CancelAllAbilities()`，不得连带取消 Guard/Parry/Bow/Dodge/其他能力，并处理取消回调重入。
- `StartComboEntry()` 在无效 Entry/Bound Anim/Montage、Montage Task 创建失败、`ReadyForActivation()` 同步结束、Montage identity/active 失败等每条早退前尽可能取得当前 Player 并清旧 Warp；Player 无效时安全返回，不得解引用。每次 Motion-Warp 尝试都检查当前 Player/Controller/ASC/World、缓存目标有效性/销毁状态/World 一致性/死亡状态；失败清理并 fail-closed。
- 不改现有左上角距离 HUD 的 debug-only 语义，不把它当作 gameplay 真值或 Motion-Warp 状态来源。

【测试 seam】
- 所有 seam 声明、定义、测试标志和调用点严格放在 `#if WITH_DEV_AUTOMATION_TESTS`；Shipping/正式运行时不可见。`TestStartComboEntry(int32)` 必须直接调用真实 `StartComboEntry`。
- 如无头夹具只在最终 `Montage_IsActive` 结果处失败，可仅绕过该结果门槛；仍必须创建/激活真实 `UAbilityTask_PlayMontageAndWait` 并执行门控、清理、快照、evaluator、Player bridge。task 创建/激活失败就停止回报，不扩大 bypass，不注入“已捕获目标”，不复制边界公式。
- Automation 至少覆盖 entry 0/1/2、entry 3+、禁用/非法不消耗捕获、首次无锁失败锁定、快照静态位置/接地复用、entry 替换、Ability 重置、目标死亡与 Destroy/EndPlay、`ClearLockedTarget` 立即清理、UnPossessed 只取消 Light、`StartComboEntry` 早退清理，以及既有 entry 0/evaluator/bridge 回归。优先复用现有 fixture；需要新增测试 seam 时仍不得改变生产 API。

【执行顺序】
1. 读取 AGENTS.md、plan.md、当前 git status；若 `.codegraph` 存在，用一次定点查询核对 `ActivateAbility -> StartComboEntry -> TryApplyMeleeMotionWarpTarget -> EndAbility` 及 Player/Enemy bridge；code-review-graph 只能作补充影响证据。
2. 在六个批准文件内做最小增量实现；保留现有 Source WIP 和距离 HUD，不改生产 evaluator、伤害链、资产或配置。
3. 扩展现有 Automation suite，记录生命周期覆盖与任何无法在无头环境证明的缺口。
4. 做一次实现者 self-review：读取最终 diff，检查异步回调/弱引用/空值/销毁/重入/Tag 精确性，运行 `git diff --check`；若 Rider lint/problem 可用，只检查六个批准 C++ 文件。报告 changed paths、静态证据、self-review findings 和未运行门禁。

【用户门禁与证据】
你不得编译、运行 UBT/Build.bat、启动或写入 Unreal Editor、运行 Automation/PIE、打包、stage、commit、reset、删除、清理 WIP 或修改资产。用户随后负责 VS2022 Development Editor 编译、Editor readback、focused Automation 和 Scene01 PIE；报告必须把静态/seam 证据与这些未运行的用户门禁分开，不能声称运行时或视觉通过。Editor readback 至少核对 `DA_Combo_StraightSword` entry 0/1/2 的 opt-in、采用 Montage 的直接 Motion-Warp Notify/Modifier/目标名/时间窗、Root Motion 与 `ABP_Player_Dungeon`、以及 Player 单一 `UMotionWarpingComponent` 的窗口搜索设置。

【停止条件】
如果需要第七个路径、Enemy header、额外 public/reflected API、Player/Combo/DataAsset/Tag/Input/Config/资产改动，或发现 UE API/现有生命周期与本合同冲突，立即停止并把证据交回 Main；不要绕过、猜测、扩大范围或提交。完成后只返回交接报告，不继续做下一阶段。
```

## 实施与文档收口记录（2026-08-31）

- **实现范围**：`LightAttackAbility.h/.cpp` 完成零基 entry `0..2` 的显式门控、每段 opt-in、首个合法 entry 的一次性 Lock-On 快照、静态位置/接地复用、task/montage 两级激活门禁与安全 rollback；目标无效、死亡/销毁、离地、取消、UnPossess、EndPlay 和各类 `StartComboEntry()` 早退均 fail-closed 清理。`PlayerCharacter`/`EnemyCharacter` 只增加窄桥和精确 `Ability.Attack.Light` 取消，未改变 Lock-On retarget 或 `UAbilityTask_MeleeTraceWindow -> FMeleeHitResolver -> Damage GameplayEffect` 唯一路径。左上角距离 HUD 仍保持 `#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)` 与 debug message key `1002` 的 debug-only 语义。
- **修改路径**：六个批准 Source/test 文件，以及 Main 为同步生命周期规则直接补充的 `AGENTS.md` 条款；本次文档收口另更新 `plan.md`、`ROADMAP.md`、`ARCHITECTURE.md`、`README.md` 和本归档文件。
- **用户证据**：用户确认修复后的 `PolyQuest.Combat.PlayerMeleeMotionWarping` focused Automation 成功，且确认 Scene01 PIE 通过。该证据覆盖用户实际运行的 focused 场景，不扩展为未运行的全量回归、编译或资产读回结论。
- **静态/复核证据**：Gemini 报告 Rider errors-only 无 error、`git diff --check` 通过；Main 使用定点 CodeGraph、补充 code-review-graph 和一跳 diff-first fresh review，未发现 P0/P1/P2 blocker。没有进行第二轮 adversarial review，也没有重新运行用户的 Editor/Automation/PIE 门禁。
- **未解决验证债务**：没有单独记录的手动 `PolyQuestEditor` Development Editor 编译，也没有逐项 `DA_Combo_StraightSword`/entry 1/2 Motion-Warp Notify、Modifier、Root Motion 和窗口设置的 Editor readback；因此不宣称 clean authored baseline。现有 3.14 测试证明终止前置门禁与清理，但没有独立构造 `ReadyForActivation()` 返回后同步 `EndTask()` 的无头重入夹具；生产顺序已静态复核，这不是本次提交 blocker，关闭触发器已登记在 `ROADMAP.md`。
- **范围与提交边界**：明确排除全部 `Content/**`、`Config/Automation/Presets/1.json`、其他 Source/文档 WIP、Blueprint/Montage/AnimBP/DataAsset/地图、`PolyQuest.uproject`、`Build.cs`、Gameplay Tags、Engine/旧 Test 项目；不清理、不回滚、不修改资产。提交前父 HEAD 为 `c91fbfdaab49e933922e902b92565859506f9351`，`90ad381` 继续只表示上次 TODO-03A7 的历史任务视角。
