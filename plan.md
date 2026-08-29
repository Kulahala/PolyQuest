# TODO-03A7：Selected Melee Motion-Warp Contact Assist v1

> 本阶段只为玩家首段轻攻击建立一个受锁定目标约束、一次性快照、有限距离/角度的 Motion Warping 接触辅助。它是可选的动画表现层，不改变 Lock-On、GAS、Trace/Resolver、Projectile 或伤害权威。实现由 `manual/out-of-band Gemini` 执行；Main 保留契约、范围、验证解释、Fresh Review、文档和提交所有权。

## Plan State And Route

- **状态**：实现与文档收口已完成；Gemini 已完成严格 self-review，用户已确认 focused Automation 与 Scene01 PIE 通过，Main 已完成一轮独立 defect-first fresh review；用户已明确批准按本记录边界提交。
- **仓库/父基线**：`E:\GameDevelop\PolyQuest`，`main @ 90ad3818afce10e8edae26157664573c1394fcd6`（`TODO-07B4` 提交）。
- **工作区**：当前工作区有既存 `ROADMAP.md`、`Config/**`、大量 `Content/**` 以及 `Source/PolyQuest/Private/Tests/WeaponEquipmentComponentAutomationTests.cpp` WIP；它们不是本阶段基线，也不得清理、回滚或纳入实现提交。工作区不是 clean checkout。
- **Outer**：`ue-stage-workflow`。
- **Primary**：`ue5-cpp-gameplay`。
- **Support**：`ue5-debug-validation`。
- **Route reason**：这是一个局部 GAS Ability + Player 组件生命周期扩展，涉及 UE 5.8 MotionWarping 模块和动画资产采用门禁；不需要通用 targeting、相机或动画框架重构。
- **Execution route**：`manual/out-of-band Gemini`。
- **Plan explorers**：`0`（Main 完成范围内定点探索；不派发 in-app explorer）。
- **Implementation executors**：`1`（Gemini）。
- **Main parallel work**：`none`。
- **Contract owner**：Main；**implementation writer**：Gemini。
- **探索预算**：只查看批准文件、受影响符号的一跳 callers/callees、MotionWarping 直接 API，以及本阶段点名的三个资产接口；出现未列文件或跨模块契约冲突立即停工回报。

本节“Confirmed Baseline Facts”记录的是实现前基线；实现后的事实、验证类别和剩余债务以本计划末尾的 Closeout Record 为准。

## Objective And Player Value

固定斜视角下，玩家已锁定一个近距离敌人并开始首段轻攻击时，现有 Root Motion 可能把玩家带到敌人前方不稳定的位置。本阶段要回答一个问题：**在当前有效 Lock-On、目标与玩家满足有限几何条件、且首段 Montage 具备已读回的 Root Motion/Motion-Warping 窗口时，能否把玩家一次性贴近目标的 authored contact point；其余情况是否完全保持旧攻击行为。**

唯一生产路径：

```text
PrimaryAttack input
  -> GAS grants/activates ULightAttackAbility
  -> ULightAttackAbility::ActivateAbility
  -> StartComboEntry(0)
  -> (valid opt-in entry only) APlayerCharacter narrow warp-target bridge
  -> UMotionWarpingComponent + montage AnimNotifyState_MotionWarping
  -> existing Root Motion playback
```

`UAbilityTask_MeleeTraceWindow -> FMeleeHitResolver -> Damage GameplayEffect` 仍是唯一近战伤害路径；Motion Warping 不得从 Trace、Resolver、Projectile 或 Tick 另起任何路径。

## Confirmed Baseline Facts

- `FComboChainEntry` 当前只有 `Montage` 字段。
- `ULightAttackAbility` 为 `InstancedPerActor`、`ServerOnly`；`ActivateAbility()` 先执行既有 `ApplyLockAwareActionFacing()`，`StartComboEntry()` 是首段和连段段落的唯一播放切换入口，已有 Montage identity、Trace/Combo/Rate Window 和统一 `EndAbility()` 清理。
- `APlayerCharacter` 当前没有 `UMotionWarpingComponent`；`ABaseCharacter` 只拥有共享 ASC、AttributeSet、Melee Trace/Trail 等组件，不能把本功能放入共享基类。
- `PolyQuest.uproject` 当前启用 `AnimationWarping`，未启用 `MotionWarping`；`PolyQuest.Build.cs` 当前没有 `MotionWarping` 模块依赖。
- UE 5.8 引擎插件位于 `D:\UE\UE_5.8\Engine\Plugins\Animation\MotionWarping`，本计划使用的直接 API 是 `UMotionWarpingComponent::AddOrUpdateWarpTargetFromTransform`、`RemoveWarpTarget`、`RemoveAllWarpTargets`、`FindWarpTarget`，以及 `UAnimNotifyState_MotionWarping` / `URootMotionModifier_SkewWarp`。
- 现有作者化入口是本地 WIP：
  - `E:\GameDevelop\PolyQuest\Content\_DataAssets\Player_Combat\DA_Combo_StraightSword.uasset`
  - `E:\GameDevelop\PolyQuest\Content\BP\Montages\LightSword\AM_Sword_LightAttack01.uasset`
  - `E:\GameDevelop\PolyQuest\Content\BP\Characters\Player\Animations\ABP_Player_Dungeon.uasset`
  静态二进制字符串只能确认引用关系，不能证明 Root Motion、Notify、Modifier 或时间窗口已正确配置；这些必须由用户在 Editor readback 门禁中确认。

## Frozen Runtime Contract

### 1. Plugin And Module Boundary

- 在 `PolyQuest.uproject` 保留现有 `AnimationWarping`，仅按编译需要增加 `MotionWarping` Runtime 插件条目；不得修改其他插件、Target、地图或 Config。
- 在 `PolyQuest.Build.cs` 的 `PrivateDependencyModuleNames` 仅增加 `"MotionWarping"`；`PlayerCharacter.h` 只做 `UMotionWarpingComponent` 前置声明，插件头文件只在 `.cpp` 引入。若 UE 5.8 的 UHT/编译证据确实要求 Public 依赖，必须停工回报证据，不得无理由扩大公开模块边界。
- 若 Engine 5.8 实际 API/模块名与上述静态核对不符，Gemini 必须停止并返回证据；不得改 Engine 源码、复制插件或用自制替代组件绕过。

### 2. Player Ownership And Narrow Bridge

- 只有 `APlayerCharacter` 创建并拥有一个 `UMotionWarpingComponent`；不得把组件加入 `ABaseCharacter`、Enemy 或全局 Subsystem。
- 组件使用 `TObjectPtr` 和现有 Components 风格，设置 `bSearchForWindowsInAnimsWithinMontages = false`，要求 Motion-Warping Notify 直接作者在选定 Montage 上；不得依赖嵌套 Sequence 的隐式搜索。
- 在 `APlayerCharacter` 提供仅供 C++ 的窄桥（不加 `UFUNCTION`、Delegate、Tag、Input、RPC）：
  - `bool SetMeleeMotionWarpTarget(FName WarpTargetName, const FTransform& TargetTransform)`：检查组件、有效名称和有限 Transform 后调用 `AddOrUpdateWarpTargetFromTransform`。
  - `void ClearMeleeMotionWarpTargets()`：清除本功能在 Player-owned 组件上建立的目标；v1 该组件没有其他生产使用者，不得用它建立通用 Warp dispatcher。
- Player 在 `UnPossessed()`、`EndPlay()`，以及明确发生的离地/攻击 teardown 路径清除本功能目标；清理必须在现有 `Super`/ASC 取消顺序中安全执行。不得清理其他角色或创建第二个 Motion Warping 生命周期。
- Ability 为 `ServerOnly`，目标写入遵循现有单机服务器权威；不新增复制/RPC/客户端预测契约。
- `APlayerCharacter::SetMeleeMotionWarpTarget` 和 `ClearMeleeMotionWarpTargets` 是唯一组件写入/清理桥；不得让测试或 Ability 直接持有并操作组件内部数组。

### 3. Authored Combo Entry Data

在 `FComboChainEntry` 增加以下窄配置，默认关闭且不迁移现有资产值：

```cpp
bool bUseMotionWarping = false;
FName WarpTargetName = FName(TEXT("MeleeContact"));
float WarpStopDistance = 190.0f;
float MaxWarpDistance = 110.0f;
float MaxWarpAngleDegrees = 60.0f;
```

契约：

- 配置是每个 Combo entry 的 authored opt-in；v1 只计划在 `DA_Combo_StraightSword` 的 entry 0（`AM_Sword_LightAttack01`）开启。entry 1/2、Charged、Sprint Attack、Melee Skill、Bow 均保持关闭/不接入。
- `WarpStopDistance` 是目标前方的期望停距；`MaxWarpDistance` 是**玩家当前位置到最终 WarpLocation 的最大水平修正距离**，不是目标最大距离；`MaxWarpAngleDegrees` 是玩家当前水平前向与目标方向的最大夹角。
- 缺失名称、非有限值、负距离、角度不在 `[0, 180]` 或其他非法配置均 fail-closed：只禁用 Warp，普通轻攻击仍按旧路径启动。不得用硬编码资产路径或隐式默认目标掩盖错误。
- 字段只提供数据，不成为新的输入、Tag、伤害或状态真值源。

### 4. One-Shot Target Snapshot And Geometry

`ULightAttackAbility` 拥有目标验证、快照、写入和清理；Player 只负责组件桥接。首段 `StartComboEntry(0)` 在 `NewMontageTask->ReadyForActivation()` **之前**尝试写入目标。

快照与判定固定如下：

1. 先读取当前 `APlayerCharacter::GetLockedTarget()`；无锁、空指针、死亡或销毁的原始目标直接禁用 Warp，不调用会触发死亡交接的验证路径。Player 无有效 ASC/Controller 时同样不写目标。
2. 对仍存活的原始目标最多调用一次现有 `ResolveValidLockedTarget()` 做当前资格验证；若验证失败、发生死亡交接或返回的目标不是最初读取的目标，本次 Warp 必须禁用。不得因本阶段自动扫描、重选或偷换目标。
3. Player 与目标都必须处于可用的地面状态（使用现有 `CharacterMovement` 地面判定）；空中、正在 teardown 或非法世界状态只禁用 Warp。
4. 使用目标 `GetActorLocation()` 作为 Actor center，不使用 Bow 的上躯干瞄准点：

   ```text
   PlayerLocation = Player->GetActorLocation()
   TargetLocation = LockedTarget->GetActorLocation()
   ToTarget2D = Normalize2D(TargetLocation - PlayerLocation)
   WarpLocation = TargetLocation - ToTarget2D * WarpStopDistance
   WarpLocation.Z = PlayerLocation.Z
   WarpYaw = ToTarget2D 的水平 Yaw
   WarpTransform = (WarpYaw, WarpLocation)
   ```

5. 先对 Player/Target 位置、前向和配置的每个分量执行 `FMath::IsFinite`；`ToTarget2D.SizeSquared2D()` 必须大于 `KINDA_SMALL_NUMBER` 且归一化、修正距离和最终 Yaw 仍有限。目标水平距离小于等于 `WarpStopDistance`、`PlayerLocation -> WarpLocation` 的水平修正距离大于 `MaxWarpDistance`、或前向夹角大于 `MaxWarpAngleDegrees` 时不写目标；边界采用有限值下的明确 `<=` 接受、`>` 拒绝语义。
6. 目标 Transform 是静态快照：调用 `AddOrUpdateWarpTargetFromTransform`，不使用 `AddOrUpdateWarpTargetFromComponent`，`bFollowComponent` 不参与；目标移动、Lock-On 切换或后续视角变化不得更新本次 Warp。
7. Combo continuation 不重新选择或重新验证目标。entry 切换前清除上一段目标；只有下一段也显式 opt-in 时才可复用本次已经保存的快照。v1 entry 1/2 关闭，因此不会产生新的目标。

几何数学必须集中在一个无副作用的生产评估器（建议命名 `EvaluateMeleeMotionWarpTransform`）中，生产路径和测试 seam 共用该入口。评估器只接收已快照的 Player/Target 位置、水平前向、地面布尔值和 `FComboChainEntry`，输出 `FTransform`；不得查询世界、Lock-On、ASC 或修改组件。Lock/死亡/Controller/权限等状态验证留在 Ability 外层包装，`WITH_DEV_AUTOMATION_TESTS` seam 只能转调同一评估器，禁止复制第二套边界数学。

### 5. Montage And Lifecycle Safety

- 选定 entry 的目标写入、旧目标清理、Montage identity 更新和 `ReadyForActivation()` 的顺序必须避免同步结束/启动失败留下 stale target。
- `EndAbility()` 必须显式取得当前 Player 并调用 `ClearMeleeMotionWarpTargets()`；Montage 创建失败、`ReadyForActivation()` 同步结束、`Montage_IsActive()` 失败、取消、自然结束、entry 替换、Player Dead、UnPossess 和 EndPlay 都必须经统一清理路径移除 Warp Target，重复调用须安全。
- 若无法启用 Warp，必须继续执行原有 `CommitAbility`、Montage、Trace Window、Combo Window、Rate Window 和 `EndAbility()` 语义；不因 Warp 失败取消普通攻击，不重复扣费，不重复触发伤害。
- 不在 Tick 中追踪目标、不添加 Timer、Tick 委托、LOS 查询、自动朝向覆盖或全局相机变换。既有 `ApplyLockAwareActionFacing()` 保持原顺序和所有权。

## Approved Change Paths

Gemini 只可修改以下八个绝对路径：

1. `E:\GameDevelop\PolyQuest\PolyQuest.uproject`
2. `E:\GameDevelop\PolyQuest\Source\PolyQuest\PolyQuest.Build.cs`
3. `E:\GameDevelop\PolyQuest\Source\PolyQuest\Public\Combat\ComboChainDataAsset.h`
4. `E:\GameDevelop\PolyQuest\Source\PolyQuest\Public\AbilitySystem\Abilities\LightAttackAbility.h`
5. `E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\AbilitySystem\Abilities\LightAttackAbility.cpp`
6. `E:\GameDevelop\PolyQuest\Source\PolyQuest\Public\Character\Player\PlayerCharacter.h`
7. `E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\Character\Player\PlayerCharacter.cpp`
8. `E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\Tests\PlayerMeleeMotionWarpingAutomationTests.cpp`

文件级职责冻结：

- `uproject`：仅 MotionWarping 插件启用条目。
- `Build.cs`：仅 MotionWarping 模块依赖。
- `ComboChainDataAsset.h`：仅 `FComboChainEntry` 的五项 opt-in 数据及必要的最小访问接口。
- `LightAttackAbility.h/.cpp`：仅快照/几何判定、StartComboEntry 前写入、entry 切换/失败/EndAbility 清理和测试 seam；不得改 Damage GE、Trace Task、输入或其他 Ability。
- `PlayerCharacter.h/.cpp`：仅 Player-owned component、窄桥和现有 Possess/Movement/EndPlay 清理；不得改变 Lock-On/Bow/装备/Hit Reaction 行为。
- 新测试文件：仅本阶段独立套件，不改动现有测试文件或生产 Resolver。

所有共享契约文件：`Contract owner: Main; implementation writer: Gemini`。如需 `EnemyCharacter.*`、其他 Ability、Trace/Resolver/Projectile、Config、Gameplay Tags、Input、Content、`.uasset/.umap` 或任何第九个路径，必须停止并把证据交回 Main。

## Automation Contract

新增独立套件：

```text
PolyQuest.Combat.PlayerMeleeMotionWarping
```

使用 transient Player/Enemy/Controller 与最小 test seam；测试必须调用与生产相同的快照/几何/清理代码，不复制一套数学。至少覆盖：

- entry 默认关闭时不写 Warp Target；显式开启且有有效锁定时通过生产评估器生成正确的静态 Location、水平 Yaw 和目标名称。
- `WarpStopDistance`、`MaxWarpDistance`、`MaxWarpAngleDegrees` 的接受/拒绝边界及逐分量有限值检查；NaN/Inf、零向量、空名称、负值和非法角度 fail-closed。
- 无锁、空目标、目标死亡/销毁、Player 或目标空中、过近、超过修正距离、超过角度、缺失组件/Controller/ASC 时不写 Warp，但普通攻击启动结果不被改写。
- 目标移动、Lock-On 切换或视角变化后，已写入的 Transform 不变；Combo continuation 不重新选目标。
- Montage 启动失败、同步结束、取消、自然结束、entry 替换、Player Dead、UnPossess、EndPlay 后无 stale target；重复清理安全且不崩溃。
- 现有 `StartComboEntry` 的 Montage identity、Trace Window/伤害入口、Combo continuation、Lock-aware facing 与 Bow/Projectile/Lock-On 相关回归不被改动。测试数量按实际运行记录，不硬编码历史套件总数。

测试若无法在无头环境证明真正的 Root Motion/Notify 执行，必须明确标为 seam/static coverage，并把真实 Motion-Warping adoption 留给用户 Editor/PIE 门禁；不得用测试 seam 冒充视觉证明。

## Execution Order And Static Gate

1. Gemini 先读取最新 `AGENTS.md`、本 `plan.md`、当前 `git status`，用 CodeGraph 定点核对 `ULightAttackAbility::ActivateAbility -> StartComboEntry -> EndAbility` 及 Player Lock-On/生命周期一跳调用；不做全仓库漫游。
2. 读取 UE 5.8 MotionWarping 头文件确认 include、模块和 `FindWarpTarget`/Remove API；若与计划冲突立即停工。
3. 先加入 uproject/Build.cs 最小模块边界，再加入 `FComboChainEntry` 五项数据；保持现有资产默认关闭。
4. 在 Player 增加 component、窄桥和 teardown 清理；不把组件放入 `ABaseCharacter`，不新增通用 dispatcher。
5. 在 `ULightAttackAbility` 实现一次性快照、有限几何判定、写入时序和统一清理；entry 1/2 不得隐式重查目标。
6. 新建独立 Automation suite，优先覆盖 fail-closed、边界、快照不跟随和生命周期，再覆盖轻攻击/Trace 回归。
7. 读取最终 diff，运行 `git diff --check`；端点可用时仅对八个批准路径中的 C++ 文件运行 Rider `lint_files`/`get_file_problems`，并确认生产与测试 seam 都调用同一评估器；完成一次严格实现者 self-review。CodeGraph/code-review-graph 只作静态/影响补充，不是编译或运行时证明。
8. Gemini 不得编译、启动 Editor、运行 Automation/PIE、打包、修改任何资产/Config/文档、stage、commit、reset、删除或清理 WIP；交接后停止。

## User-Owned Validation Gates

实现交接后由用户负责以下门禁，结果必须按证据类别记录：

1. 手动编译 `PolyQuestEditor`（VS2022，Development Editor）。
2. Editor readback：确认 `MotionWarping` 插件启用、Player 只有一个 `UMotionWarpingComponent`、`bSearchForWindowsInAnimsWithinMontages=false`；确认 `ABP_Player_Dungeon` 的 Root Motion 模式可驱动 Montage Root Motion；确认 `AM_Sword_LightAttack01` **直接**包含 `AnimNotifyState_MotionWarping`，其 Modifier 为 `URootMotionModifier_SkewWarp`，目标名为 `MeleeContact`，平移开启、忽略 Z、旋转/朝向设置和 Notify 时间窗覆盖首段接触；确认 `DA_Combo_StraightSword` entry 0 开启五项配置，entry 1/2 仍关闭。该门禁用于确认作者化 adoption 与 clean-checkout baseline；缺少独立 readback 时，不宣称该 baseline，并登记验证债务。只有 readback 或 PIE 证据显示配置不适用/失败时，才以 evidence-backed `no-adoption` 结束。
3. 运行 focused `PolyQuest.Combat.PlayerMeleeMotionWarping` 及现有 Light Attack、Melee Trace/Resolver、Lock-On、Projectile/Bow、Guard/Parry 回归套件；套件数量按实际记录。
4. 在 `/Game/Maps/Scene01` PIE 验证：采用的 entry 0 调参为 `WarpStopDistance=190cm`、`MaxWarpDistance=110cm`、`MaxWarpAngleDegrees=60°`；无锁、过近、修正超限、角度超限、空中时普通攻击照常且不吸附；目标在攻击开始后移动不被追踪；连段、取消、死亡、UnPossess、重新 Possess 和地图 teardown 后无残留目标；Trace/伤害、Lock-On、Bow 行为不回归。Automation 中的 `100/60/60` 仅是独立边界矩阵夹具，不代表生产资产调参。

源码静态检查、CodeGraph、Automation seam 和 Gemini self-review 不能替代用户编译、Editor readback 或 PIE/视觉证据。

## Known Debt And Blocker Decision

- 用户已确认 Scene01 PIE 通过；首段贴近以及取消/离地/teardown 的具体子场景来自 Gemini 交接报告，Main 本轮未再次运行这些场景。本轮没有独立的手动 `PolyQuestEditor` 编译记录，也没有逐项 Editor readback 记录，因此不能把本阶段描述成可由 clean checkout 重现的作者化资产基线。该债务的 closure trigger 是在后续 adoption slice 前补齐编译与 `ABP_Player_Dungeon`、`AM_Sword_LightAttack01`、`DA_Combo_StraightSword` 的定点 readback；不是当前源码 blocker。
- focused Automation `PolyQuest.Combat.PlayerMeleeMotionWarping` 已由用户确认通过，但主要覆盖共享 evaluator、Player bridge 和清理 seam，没有完整驱动 `StartComboEntry(0) -> TryApplyMeleeMotionWarpTarget` 的生产入口。closure trigger 是 `TODO-03A7B` 的首段/后续 entry adoption 矩阵或一个明确的生产入口测试 seam；不是当前运行时 blocker。
- MotionWarping 是 UE 5.8 插件 API；本轮已完成引擎头文件/模块静态核对，不能以此替代用户编译证据，也不复制插件或修改 Engine。
- Actor center 与具体动画 contact point 的美术调校可在后续 adoption slice 中处理；不得用扩大边界或动态追踪掩盖体验问题。
- 当前 `Content/**`、Config 和其他 Source WIP 继续保留；它们不是本阶段 blocker，也不是提交候选。

## Documentation And Review Closeout

- `ARCHITECTURE.md`：同步 Player-owned MotionWarping、entry 0 一次性静态快照、有限几何与统一清理契约；不写未来调参清单。
- `ROADMAP.md`：将 `TODO-03A7` 从开放阶段移出，加入玩家后续 Motion-Warp adoption 拆片、敌人后置阶段和上述两项验证债务；不伪造新的提交 SHA。
- `ROADMAP-archive.md`：本轮不追加；只有下一阶段获准替换本 `plan.md` 时，才按 AGENTS.md 归档本阶段详细 closeout。
- `README.md`：本阶段没有新增可独立公开的稳定基线；不把缺少编译/readback 的源码能力写成 clean-checkout 资产事实。
- `plan.md`：保留本阶段 closeout，直到下一阶段获准替换。

Main 已对批准实现文件完成**一轮**独立 defect-first fresh review，范围为批准文件及受影响符号一跳 callers/callees。未发现 P0/P1/P2 blocker；未追加第二轮 adversarial review，也未派独立 Reviewer。

## Commit Boundary

阶段提交候选仅为八个批准路径与必要文档收尾；明确排除全部 `Content/**`、`Config/**`、其他 Source WIP、测试 preset、地图、Blueprint、Montage、AnimBP、Niagara、音频、Gameplay Tags、Input、旧 Test 项目和 Engine 源码。提交必须等待用户明确批准，并以实际 staged diff 为准。

## Non-Goals

- 不为 Enemy、Charged/Sprint/Skill/Bow、Hit Reaction、处决、Locomotion 或全局 Root Motion 增加 Motion Warping。
- 不改变 Lock-On acquisition/retention/cycle、Bow 6% Target Assist、装备/拾取、Poise/Stance Break、AI/StateTree、Projectile 或任何 Damage GameplayEffect。
- 不新增自动扫描、临时选敌、LOS/相机逻辑、Tick/Timer 跟踪、通用 targeting/warp/feedback framework、GameplayCue、Tag、Input、RPC、复制或多人支持。
- 不手工编辑、导入、移动、复制、重定向或删除 `.uasset/.umap`；不修改用户作者化字段作为代码实现的隐式前置条件。
- 不清理、回滚、覆盖、stage 或提交既有 WIP。

## Gemini Handoff Prompt

以下提示在 Main 确认本计划后交给 Gemini；它冻结执行边界，不授予额外架构权限：

> 你是 TODO-03A7 的实现执行者。工作目录必须是 `E:\GameDevelop\PolyQuest`，父基线为 `90ad3818afce10e8edae26157664573c1394fcd6`；先读取最新 `AGENTS.md` 和当前 `plan.md`。执行路线是 `manual/out-of-band Gemini`，Outer 为 `ue-stage-workflow`，Primary Skill 为 `ue5-cpp-gameplay`，Support Skill 为 `ue5-debug-validation`。`Contract owner: Main; implementation writer: Gemini`。
>
> 只允许修改 plan 中列出的八个绝对路径：`PolyQuest.uproject`（仅启用 MotionWarping）、`PolyQuest.Build.cs`（仅在 PrivateDependencyModuleNames 增加 MotionWarping）、`ComboChainDataAsset.h`（仅增加五项 entry 数据）、`LightAttackAbility.h/.cpp`（仅一次性快照/纯几何评估器/StartComboEntry 写入/统一清理）、`PlayerCharacter.h/.cpp`（仅 Player-owned UMotionWarpingComponent、窄 C++ bridge、Possess/Movement/EndPlay 清理）和新建 `PlayerMeleeMotionWarpingAutomationTests.cpp`（仅独立 suite `PolyQuest.Combat.PlayerMeleeMotionWarping`）。不得修改 `ABaseCharacter`、Enemy、Trace/Resolver、Projectile/Bow、其他 Ability、Config、Gameplay Tags、Input、Content、Blueprint、Montage、AnimBP、地图、Engine 或文档。
>
> 冻结行为：组件只在 Player；`bSearchForWindowsInAnimsWithinMontages=false`；entry 默认 `bUseMotionWarping=false`，v1 只允许首段 `AM_Sword_LightAttack01` 的 entry 0 opt-in。先读取原始 `GetLockedTarget()`，无锁/死亡/销毁时直接 fail-closed；对仍存活的原始目标最多调用一次 `ResolveValidLockedTarget()`，若发生死亡交接或返回不同目标也 fail-closed，不得接受自动 retarget。Player/目标空中、非法 ASC 或 Controller 同样不写目标。使用 Actor center 和水平公式 `WarpLocation = TargetLocation - Normalize2D(TargetLocation-PlayerLocation)*WarpStopDistance`，将 Z 固定为 Player Z，朝向为水平目标 Yaw；生产采用的 `WarpStopDistance=190cm`、`MaxWarpDistance=110cm`（玩家到 WarpLocation 的水平修正上限）、`MaxWarpAngleDegrees=60°`，有限边界 `<=` 接受、`>` 拒绝；Automation 可用 `100/60/60` 独立夹具测试边界。调用 `AddOrUpdateWarpTargetFromTransform` 写静态快照，绝不 Tick/follow/重查目标；连段不重新选择目标。启动失败、同步结束、取消、替换、死亡、UnPossess、EndPlay 必须清理，Warp 失败时普通轻攻击照常运行，伤害路径完全不变。
>
> 先用 CodeGraph 定点核对 `ActivateAbility -> StartComboEntry -> EndAbility` 和 Player 生命周期，再确认 UE 5.8 插件头文件/API。几何数学必须由一个无副作用生产评估器提供，测试 seam 只能转调它；不得在测试中复制边界公式。若需要未列文件、修改公共契约、添加 Tag/Input/Config/资产、改变 Lock-On/Trace/Damage 或无法确认模块/API，立即停止并把证据交回 Main；不得绕过边界。不得编译、启动 Editor、运行 Automation/PIE、打包、stage、commit、reset、删除或清理 WIP。
>
> 测试必须走生产快照/几何/清理代码，覆盖默认关闭、正确静态 Transform、停距/修正距离/角度边界、NaN/Inf、无锁/死亡/销毁/空中/过近、目标移动不跟随、连段不重选、启动失败/取消/自然结束/替换/死亡/UnPossess/EndPlay 清理，以及 Light Attack/Trace/Lock-On/Bow 回归。读取最终 diff，运行 `git diff --check`，端点可用时对批准 C++ 文件运行 Rider lint/problem 检查；完成严格 self-review，列出 changed paths、静态证据、未运行的用户门禁和 remaining risks。self-review 不得称为 Main fresh review。

## Acceptance Contract

本阶段实现/采用判定沿用以下门禁；未单独提供的证据必须保留为验证债务，不得推断：

1. 批准源码与模块边界通过用户 `PolyQuestEditor` Development Editor 编译。
2. Editor readback 用于证明 Player component、首段 Montage Notify/SkewWarp、AnimBP Root Motion 和 entry 0 配置均真实存在且匹配契约，并支持 clean-checkout authored-baseline 声明；缺少独立 readback 时保留验证债务，不把它与已确认的 PIE 结果混同。只有证据显示配置不适用或运行失败时，才明确记录 no-adoption。
3. focused Automation 与相关回归通过，且测试覆盖不依赖固定历史套件数量。
4. Scene01 PIE 证明有限贴近、无锁/超限 fail-closed、静态快照不追踪、连段/取消/teardown 清理及既有 Trace/伤害行为无回归。
5. Main 完成一轮受控 defect-first fresh review，随后同步文档并等待用户明确提交批准。

## Closeout Record (2026-08-30; implementation closeout based on parent HEAD `90ad381`; pre-commit working-tree snapshot was not clean)

- **Implementation**：八个批准路径已完成 Player entry 0 Motion-Warp contact assist；默认关闭、静态 Lock-On 快照、有限距离/角度/地面判定、`Forward2D.Normalize()` fail-closed、防止死亡交接偷换目标，以及 entry/取消/同步失败/离地/UnPossess/EndPlay 清理均已核对。既有 Trace Window、`FMeleeHitResolver`、Damage GameplayEffect、Lock-On 和 Bow 路径未改动。
- **User evidence**：用户确认 `PolyQuest.Combat.PlayerMeleeMotionWarping` Automation 通过，并确认 Scene01 PIE 通过；Gemini 报告 `git diff --check` 与 Rider errors-only 检查无诊断。这里不把这些结果表述为独立编译或 Editor readback 证据。
- **Main fresh review**：一轮、受控、一跳范围的 defect-first review 完成；未发现 P0/P1/P2 blocker。重点核对 `StartComboEntry(0) -> TryApplyMeleeMotionWarpTarget -> EvaluateMeleeMotionWarpTransform`、静态目标不跟随、清理路径、有限值防御和普通伤害回归。
- **Validation debt**：无独立手动 `PolyQuestEditor` 编译/readback 记录；Automation 尚未完整驱动生产 `StartComboEntry` 入口。两项 closure trigger 已登记到 `ROADMAP.md`，不阻塞下一玩家 source slice 的计划制定。
- **Scope**：本阶段提交候选为八个批准路径与阶段文档；`AGENTS.md`、`Config/**`、全部 `Content/**`、其他 Source WIP、测试 preset、资产作者化和 Engine 源码明确排除。文档收口轮中 Main 未自行编译、启动 Editor 或运行 Automation/PIE；用户/Gemini 的验证证据已在上方按类别记录，提交仅在用户批准后按此边界执行。
