# TODO-03A7C：Charged/Sprint Melee Motion-Warp Adoption v1

## 计划状态与摘要

- **状态**：实现、用户验证与 Main 的 defect-first fresh review 已完成；本次文档收口完成后等待用户明确批准再 staging/commit。
- **唯一阶段目标**：在已完成的 Player Light Combo Motion-Warp 合同之上，分别审查并采用 `UChargedAttackAbility` 与 `USprintAttackAbility` 的近战 Root Motion 接触辅助。每个 Ability 独立决定 `adopted` 或 evidence-backed `no-adoption`；不把 Motion Warping 扩展成“主角所有近战攻击”的全局系统。
- **核心玩家问题**：Charged 释放和 Sprint Attack 在有锁定目标时可能停在目标前、穿过目标或出现明显空挥；本阶段只改善 Root Motion 接触表现，不改变攻击伤害、削韧、资源、取消或 Sprint 所有权。
- **当前仓库基线**：`main @ ae0139be10c3273cc8311c19d0210bb1d2e0cbc4`（`ae0139b`，TODO-03A7B 最终提交）。`c91fbfd`、`c8b72c6` 与 `90ad381` 只保留在各自历史任务视角中，不改写成当前基线。
- **执行路线**：`manual/out-of-band Gemini`；Main 拥有架构、合同、计划、验证解释、复核、文档收口与提交门；用户拥有 Editor authoring、手动编译、Editor readback、PIE/视觉验证与最终 commit approval。

## 当前工作区快照（2026-08-31；实施后、文档收口前）

本计划建立时工作区不是 clean checkout，必须保留所有既有 WIP：

- 分支：`main`。
- `HEAD`：`ae0139be10c3273cc8311c19d0210bb1d2e0cbc4`。
- `git status --short`：330 项；其中 289 项 tracked 修改、41 项 untracked。
- `Content/**`：318 项（279 项 tracked 修改、39 项 untracked），均为用户资产/资源 WIP；不得清理、回滚、导入、重定向、移动、复制或纳入本阶段提交。
- `Config/Automation/Presets/1.json`：1 项 tracked 用户 WIP；不得修改或纳入本阶段提交。
- `Source/**`：9 项本阶段批准变更（7 项 tracked 修改、2 个 untracked 共享 helper）；不得把其他 Source WIP 混入。
- 本快照时 `plan.md` 与 `ROADMAP-archive.md` 已是文档 WIP；`AGENTS.md`、`ARCHITECTURE.md`、`ROADMAP.md`、`README.md` 在本次收口前尚未加入本阶段变更，随后只按本文档收口边界更新。
- 已提交的 `ae0139b` 包含 TODO-03A7B 的六个 Source/test 文件、阶段文档与规则同步；本阶段从该提交的真实源码继续，不把旧 WIP 当作待重做功能。

## 已确认的源码与架构事实

### GAS、所有权与既有 Motion-Warp 边界

- `ABaseCharacter` 拥有单机 ASC 与 `UCharacterAttributeSet`；Charged/Sprint 都是 `UStaminaActionAbility` 的 `InstancedPerActor`、`ServerOnly` 子类。不能新增第二个 ASC、AttributeSet 或动作状态真值。
- `APlayerCharacter` 已独占一个 `UMotionWarpingComponent`，在构造函数中把 `bSearchForWindowsInAnimsWithinMontages` 设为 `false`；`SetMeleeMotionWarpTarget()` 与 `ClearMeleeMotionWarpTargets()` 是现有 C++ 窄桥。
- Light 已有受保护的几何合同：有限坐标、水平距离、停距、最大修正距离、夹角和双方接地检查；目标以 Actor center 的静态 Transform 写入，不 Tick 跟随、不重选。TODO-03A7B 的死亡/销毁、清锁、离地、UnPossess、Montage/task 早退清理也已提交。
- 既有唯一近战伤害链不可改变：`UAbilityTask_MeleeTraceWindow -> FMeleeHitResolver -> Damage GameplayEffect`。Motion Warping 只改变接触位移，不直接施加伤害、不读写 Health/Poise、不改变 SetByCaller 数据。

### Charged 当前调用边界

- `UChargedAttackAbility::ActivateAbility()` 建立 Montage、HoldReady、Trace、输入、取消和 Rate Window Tasks；HoldReady 会暂停主动 Montage。
- `BeginRelease()` 的当前顺序是 `SetCharging(false)`、`CheckCost`、`CommitAbility`、确认主动 Montage、计算 Damage/Poise，再恢复暂停 Montage；Hold 阶段本身不应捕获 Motion-Warp 目标。
- `EndAbility()` 已统一清理 Charging/取消标签、Montage、Tasks、Trace 和 Rate；本阶段只在其边界增加 Warp 清理/快照重置，不改变成本与恢复延迟语义。

### Sprint 当前调用边界

- `USprintAttackAbility::ActivateAbility()` 要求真实 Sprint 状态、接地、移动输入、有效 Montage/effects/tags；先建立 Tasks 并 `CommitAbility()`，再写动作标签、一次性锁定朝向、激活 Montage。
- Montage Task/主动 Montage 成功确认后，现有代码才取消 Guard 与 Sprint；本阶段的 Warp 捕获必须发生在这两个取消动作之前，以保留当前 Sprint ownership 顺序。
- `EndAbility()` 统一清理动作标签、Trace、Rate、Montage delegate/task 与 Montage；本阶段只增加同一条清理路径的 Warp 清理/快照重置。

### 资产现状

当前 `Content/**` 是用户本地 WIP。只读文件名可提供候选入口，例如：

- Charged：`/Game/_Abilities/Weapon/LightSword/Charged/GA_Sword_ChargedAttack`、`/Game/_Abilities/Weapon/HeavySword/GA_HeavySword_ChargedAttack`、`/Game/_Abilities/Weapon/Unarmed/Charged/GA_Unarmed_ChargedAttack`，以及对应的 `AM_Sword_ChargAttack`、`AM_LongSword_ChargedAttack`、`AM_Unarmed_Charged`。
- Sprint：`/Game/_Abilities/Weapon/LightSword/SprintAttack/GA_Sword_SprintAttack`、`/Game/_Abilities/Weapon/HeavySword/GA_HeavySword_SprintAttack`、`/Game/_Abilities/Weapon/Unarmed/Sprint/GA_Unarmed_SprintAttack`，以及对应的 `AM_Sword_SprintAttack`、`AM_LongSword_SprintAttack`、`AM_Unarmed_SprintAttack`。

这些路径只是 Editor readback 的候选，不能凭文件名、字节串或旧 Test 项目推断 GA 类、Montage 引用、Root Motion、Notify、Modifier、目标名或时间窗已经正确。

## 阶段范围与垂直切片

本阶段拆成两个依赖有序、可独立关闭的切片：

1. **C1 — Charged release adoption**：只在成功 `CommitAbility`、主动 Montage 仍有效且即将恢复播放时，做一次静态目标捕获/写入。
2. **C2 — Sprint attack adoption**：只在 Montage Task 已激活、主动 Montage 已确认播放且 Sprint 尚未被取消时，做一次静态目标捕获/写入。

C1 完成或以 no-adoption 关闭后才进入 C2；C1/C2 都不要求另一 Ability 的配置或资产可用。任一 Ability 的候选 Montage 不适用时，保持该 Ability 的 `bUseMotionWarping=false` 并记录证据，不为完成 TODO 强行打开资产。

## 冻结的运行时合同

### 1. 窄共享 helper，不建立全局系统

允许新增以下两个源文件，以避免 Charged/Sprint 复制 Light 的几何公式：

- `E:\GameDevelop\PolyQuest\Source\PolyQuest\Public\Combat\Melee\MeleeMotionWarping.h`
- `E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\Combat\Melee\MeleeMotionWarping.cpp`

helper 只能是窄的、无全局状态的 C++ 值/纯函数边界；完整的 Lock-On/ASC/Controller/World/Player bridge 生命周期编排仍由各 Ability 自己拥有：

- `FMeleeMotionWarpConfig` 包含 `bUseMotionWarping`、`WarpTargetName`、`WarpStopDistance`、`MaxWarpDistance`、`MaxWarpAngleDegrees`；默认关闭，生产默认数值沿用 `190cm / 110cm / 60°`。
- `FMeleeMotionWarpSnapshot` 是非反射、由每个 Ability 实例拥有的状态，至少包含 `bAttemptedCapture`、`TWeakObjectPtr<AEnemyCharacter>`、有限的缓存 Actor-center 位置和捕获时目标接地布尔值；提供幂等 `Reset()`。
- `FMeleeMotionWarpingLifecycle`（若保留该命名，或等价的同文件值 helper）最多负责配置合法性、快照值操作和静态 evaluator；不得接收/持有 UObject 生命周期、调用 Lock-On/ASC/Controller/World、写 Player bridge、输出 Debug、拥有 AbilityTask、Tick、Timer、Delegate、全局 singleton、自动重选或跨 Ability 共享快照。Charged/Sprint 的一次性捕获顺序、上下文检查、失败清理和时序必须留在各自 Ability 的私有函数中，即使两处代码相似也不为消除少量重复而引入回调/模板/通用服务。
- 几何公式从当前 `ULightAttackAbility::EvaluateMeleeMotionWarpTransform()` 提取时必须保持行为等价。`ULightAttackAbility` 保留现有 public static 签名，先从 `FComboChainEntry` 构造局部 `FMeleeMotionWarpConfig`，再委托同一数学实现；共享 helper 不依赖 `ComboChainDataAsset.h`，现有 Light 测试边界不能漂移。
- Light 原私有嵌套 `FMeleeMotionWarpSnapshot` 迁移为共享的非反射值类型；保留 `bAttemptedCapture`、`CapturedTarget`、`CapturedTargetLocation`、`bCapturedTargetOnGround` 字段语义，以及既有测试 Getter 名称/结果，不能借迁移改变 entry `0..2` 或生命周期行为。
- helper 不拥有 AbilityTask、GameplayTag、ASC grant、Damage GE、Trace Window 或 Player action state；调用者仍拥有自己的生命周期和失败处理。

### Gemini 审阅意见处理（2026-08-31）

- **采纳配置解耦**：`FComboChainEntry` 只在 Light 兼容 wrapper 边界转换为 `FMeleeMotionWarpConfig`；Charged/Sprint 直接使用自己的配置，禁止修改 `ComboChainDataAsset.h`。
- **采纳快照迁移**：Light、Charged、Sprint 共用同一非反射快照定义，但每个 Ability 实例独立持有并在激活/结束边界重置；不得建立共享可变状态。
- **采纳 Sprint seam**：在 `WITH_DEV_AUTOMATION_TESTS` 下补齐与 Charged/Light 同等的 ActorInfo、AnimInstance/Montage、最终 Montage-active 测试控制和快照观察入口；Task 创建、Task active/finished、上下文校验、evaluator 与清理不得旁路。具体命名遵循现有项目风格，避免额外生产 API。
- **不采纳完整生命周期 helper**：目标读取、一次性机会消耗、ASC/Controller/World/弱引用验证、Player bridge 写入和失败清理保留在各 Ability 的私有编排中。原因是 Charged 的 Release 与 Sprint 的 Montage-start 生命周期不同，把它们压成一个 UObject/回调驱动 helper 会模糊所有权并增加重入风险；少量重复代码是本阶段可接受成本。

### 2. Charged/Sprint 的 authored 配置

在 `UChargedAttackAbility` 与 `USprintAttackAbility` 各自增加同名、直接挂在 GA 上的 `EditDefaultsOnly/BlueprintReadOnly` Motion-Warp 配置字段（或一个等价的可读配置映射）：

- `bUseMotionWarping=false`；
- `WarpTargetName=MeleeContact`；
- `WarpStopDistance=190.0f`；
- `MaxWarpDistance=110.0f`；
- `MaxWarpAngleDegrees=60.0f`。

不修改 `ComboChainDataAsset`，不新增通用 DataAsset、Gameplay Tag、Input、Config 键或 Blueprint dispatcher。字段非法、空名称、非有限值、负距离或角度不在 `[0,180]` 时 fail-closed，并且不读取 Lock-On、不消耗捕获机会。

### 3. 一次性目标快照与 fail-closed

- 每次 Ability 激活开始先 reset 自己的快照，并在 Player 有效时清掉现存 melee Warp targets；状态不得跨激活泄漏。
- HoldReady/Charging 阶段绝不捕获或重选目标。首个合法 opt-in 的捕获时机由下方 C1/C2 固定，后续事件不得重复应用。
- 在任何 Player、World、Controller、ASC 或 Lock-On 查询之前，首个合法 opt-in 必须先将 `bAttemptedCapture=true`；缺失上下文也会消耗本 Ability 本次捕获机会。
- 捕获要求：Player 未销毁且有 World；当前 Controller 未销毁；ActorInfo 的 ASC 有效、Owner 是该 Player 且等于 `PlayerCharacter->GetAbilitySystemComponent()`；原始 `GetLockedTarget()` 有效、未销毁、存活且同 World；`ResolveValidLockedTarget()` 必须返回同一个目标，死亡交接/自动替换一律 fail-closed。
- 捕获成功只保存弱引用、有限的目标 Actor-center 位置和捕获时目标接地状态。后续 apply 不读取当前 Lock-On、`ResolveValidLockedTarget()`、目标当前位置或目标当前接地状态，也不因锁定变化重选。
- 后续 apply 只验证缓存目标仍有效、未销毁、存活且同 World，再用缓存位置/接地状态、当前 Player 位置/水平朝向/接地状态和当前 Ability 自身配置调用同一 evaluator。
- evaluator 失败、Player bridge `SetMeleeMotionWarpTarget()` 返回 false、目标失效、ASC/Controller/World 失效时清除 Player Warp targets 并返回 false；不能把失败当成成功，也不能重置一个已经成功捕获的快照（除非 Ability 激活边界或 EndAbility）。
- 每个 Ability 的成功/失败都不影响其原有攻击继续执行；Warp 失败不是 Charged/Sprint 攻击取消条件。

### 4. C1 Charged release 时序

只允许在 `BeginRelease()` 的以下状态成立后调用一次窄 apply：

1. `bEndAbilityRequested == false` 且仍有有效 `CurrentActorInfo`/Player；
2. `CheckCost()` 与 `CommitAbility()` 已按现有顺序成功；
3. `BoundAnimInstance`、`ActiveMontage` 有效且 `Montage_IsActive(ActiveMontage)` 为真；
4. 计算并写入现有 Damage/Poise SetByCaller 值、设置 `bReleaseStarted=true`；
5. 在 `Montage_Resume()` 之前调用一次 Motion-Warp apply，然后保留原有恢复暂停 Montage 的逻辑。

Cost/Commit/Montage 任一失败时不得捕获；`EndFromMontage()`/`EndAbility()` 清理残留。没有暂停的合法释放也必须只调用一次 apply，不得以重复事件再次捕获。Apply 失败继续既有 Trace/Resolver/Damage、取消窗口和 Montage 流程。

### 5. C2 Sprint activation 时序

保持现有 preflight、Task 创建、Cost commit、Action Tags、锁定朝向、Delegate 绑定、Task 激活的顺序，只在成功启动检查后插入一次 apply：

1. `MontageTask->ReadyForActivation()` 返回后，先确认没有同步 `EndAbility`，Task 仍是当前 Task、`IsActive()` 且未 `IsFinished()`；
2. `BoundAnimInstance`/`ActiveMontage` 有效且 `Montage_IsActive()` 为真；
3. 在 `CancelActiveGuardAfterConfirmedAction(true)` 与 `CancelSprintAbility()` 之前调用一次 Motion-Warp apply；
4. 无论 apply 成功与否，都继续现有 Guard/Sprint 取消和攻击生命周期。

若 Task/Montage 启动失败，沿现有 `EndAbility()` 早退，不捕获、不留下 Warp target。Apply 不得改变 Sprint tag、体力消耗、Dodge cancel window、Trace 或 Damage 行为。

### 6. 统一清理与重入

- Charged/Sprint 的 `EndAbility()` 首次实际清理路径必须清 Player Warp targets，再 reset 自己的快照；重复 EndAbility 必须幂等。
- 目标死亡/销毁、`ClearLockedTarget()`、Player 离地、UnPossess、Player EndPlay 和 Ability 取消沿用既有 Player/Enemy 窄桥；不新增第二套监听或全局广播。已有 Player 清理桥不得被改成只服务 Light。
- 所有异步 Montage delegate、GameplayEvent、Task completion 和同步 `ReadyForActivation()` 返回路径都先检查当前 Ability、Player、Task、Montage 与 World，再访问对象或写入 Warp。
- 不改变 `APlayerCharacter` 左上角实时距离 Debug HUD 的 debug-only 宏、key `1002` 或输出语义；不把它当 Motion-Warp/Lock-On 真值。

## 批准的实现路径与职责

### 允许 Gemini 修改的路径

1. `E:\GameDevelop\PolyQuest\Source\PolyQuest\Public\Combat\Melee\MeleeMotionWarping.h`
2. `E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\Combat\Melee\MeleeMotionWarping.cpp`
3. `E:\GameDevelop\PolyQuest\Source\PolyQuest\Public\AbilitySystem\Abilities\ChargedAttackAbility.h`
4. `E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\AbilitySystem\Abilities\ChargedAttackAbility.cpp`
5. `E:\GameDevelop\PolyQuest\Source\PolyQuest\Public\AbilitySystem\Abilities\SprintAttackAbility.h`
6. `E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\AbilitySystem\Abilities\SprintAttackAbility.cpp`
7. `E:\GameDevelop\PolyQuest\Source\PolyQuest\Public\AbilitySystem\Abilities\LightAttackAbility.h`
8. `E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\AbilitySystem\Abilities\LightAttackAbility.cpp`
9. `E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\Tests\PlayerMeleeMotionWarpingAutomationTests.cpp`

`Contract owner: Main`；`implementation writer: Gemini`。职责冻结如下：

- **MeleeMotionWarping helper**：只抽取配置/快照值操作/纯 evaluator；不得承接 UObject 生命周期、Lock-On/ASC/Controller 查询、Player bridge 写入、Debug 输出、全局服务、Player 组件或第二条目标写入路径。
- **Charged header/cpp**：只增加配置、Ability-owned snapshot/helper seam，并在 `ActivateAbility`、`BeginRelease`、`EndAbility` 接入时序；不得重写 Charge、Cost、Poise、Trace、Rate、Dodge cancel 或 GE 逻辑。
- **Sprint header/cpp**：只增加配置、Ability-owned snapshot/helper seam，并在 `ActivateAbility`、`EndAbility` 接入时序；不得改变 Sprint activation predicate、Cost、Action Tags、Guard/Dodge cancellation、Trace、Rate 或 Sprint ownership。
- **Light header/cpp**：只允许迁移共享快照类型并为共享 evaluator 做无行为变化的 wrapper/适配；不得改变已经通过验证的 entry `0..2` 门控、快照时序、生命周期桥、测试 bypass 或伤害链。
- **Automation test**：只扩展现有 `PolyQuest.Combat.PlayerMeleeMotionWarping`；不复制几何公式、不直接写 MotionWarpingComponent 内部数组、不创建第二套 fixture/测试套件。

禁止修改：`PlayerCharacter.*`、`EnemyCharacter.*`、`BaseCharacter.*`、`ComboChainDataAsset.h`、`PolyQuest.Build.cs`、`PolyQuest.uproject`、`Config/**`、`Content/**`、Gameplay Tags、Input、Trace/Resolver、Projectile/Bow、Guard/Parry、Enemy AI/StateTree、任何项目文档。现有 `MotionWarping` 私有模块依赖已经存在，不为本阶段改 Build.cs。

### 测试 seam 约束

- 所有新增 seam、字段访问器和测试标志必须严格放在 `#if WITH_DEV_AUTOMATION_TESTS`，Shipping/正式运行时不可见。
- seam 可以设置测试 ActorInfo、Bound Anim/Montage、配置和读取快照，或调用真实的窄 apply 入口；不得直接注入“已捕获目标”结果。
- `USprintAttackAbility` 必须补齐与现有 Charged/Light 风格一致的最小 seam（至少能设置测试 ActorInfo、Bound AnimInstance/ActiveMontage、读取捕获尝试/快照，并在必要时只旁路最终 `Montage_IsActive()` 布尔结果）；测试应能观察 Task active/finished 门禁，而不是伪造其通过。
- 如无头环境只会让最终 `Montage_IsActive()` 结果不可证明，可使用明确命名的 test flag 仅旁路该最终布尔结果；不得旁路 Task 创建/激活、Task active/finished 检查、ASC/Controller/World/目标验证、evaluator 或清理。Task 本身无法建立时停止并回报，不扩大 bypass。
- 测试标志在 Ability 激活与结束边界复位；测试调用不能改变生产默认配置或 Montage 播放行为。

## Automation 计划

继续使用 `PolyQuest.Combat.PlayerMeleeMotionWarping`，保留已有 Light evaluator/entry/lifecycle 矩阵，追加以下 Charged/Sprint 组：

### 共享配置与数学

- 默认配置关闭；空名称、负值、NaN/Inf、角度越界、非地面、过近、超最大修正、零水平方向均 fail-closed。
- Shared helper 与 Light wrapper 在既有 `100/60/60` 测试边界上结果一致；`100/60/60` 只属于测试夹具，不改变生产 `190/110/60` 默认值。该测试还要确认 wrapper 的 `FComboChainEntry -> FMeleeMotionWarpConfig` 转换不改变字段语义。
- 不同 Target Name 与不同 entry/Ability 参数独立使用；一次失败不重选、不污染下一次激活。

### Charged

- HoldReady/Charging 阶段不读取 Lock-On、不消耗 `bAttemptedCapture`、不写 Warp。
- 成本失败、Commit 失败、主动 Montage 无效或未播放时不捕获且无残留。
- 成功 Release 只捕获一次；重复 Released/GameplayEvent 不重复写入或重选。
- 首次无锁、死亡/销毁、ASC/Controller/World 无效均 fail-closed；后续锁定变化不能补捕获。
- 捕获后的目标位置/接地状态静态复用；移动目标、切锁、目标接地状态变化不会改写缓存。
- evaluator/bridge 失败不取消 Charged，不改变 Damage/Poise/Trace/Cost；EndAbility、取消、离地、死亡、UnPossess、EndPlay 无残留。

### Sprint

- 未进入真实 Sprint、无移动/未接地、Task 创建失败或 Montage 未启动时不捕获。
- Task active/unfinished 与 Montage active 两级门禁通过后只捕获一次，并且发生在现有 `CancelSprintAbility()` 之前。
- 测试 seam 必须分别覆盖 Task active/finished 门禁与最终 Montage-active 检查；最终 Montage 布尔结果可在无头环境受控旁路，但不得把 Task 伪造为 active 或跳过真实 Task 创建。
- Apply 失败仍按原逻辑取消 Guard/Sprint、保留攻击 Trace/Damage；不得出现半个 Sprint tag 或额外 Cost。
- 首次无锁、死亡/销毁、ASC/Controller/World 无效、后续切锁/目标移动/接地变化均按静态快照 fail-closed。
- EndAbility、Dodge/Guard 取消、离地、UnPossess、EndPlay 和同步 Montage 结束无残留。

### 回归

- 现有 Light entry `0..2`、entry replacement、静态快照、死亡/销毁 bridge、清锁、UnPossess、Task rollback 全部保持通过。
- Charged 的伤害倍率/削韧 SetByCaller、HoldReady pause/resume、Rate Window、Dodge cancel 与 Stamina regen delay 不回归。
- Sprint 的 grounded predicate、Sprint tag/Drain、Guard cancel、Dodge cancel、Rate Window、Trace/Resolver/Damage 与 Sprint ownership 不回归。
- Lock-On acquisition/cycle/retention、Bow/Projectile、Guard/Parry、Enemy AI/Poise/Death 不被 Motion-Warp 接入改变。

无法由无头 Automation 证明真实 Notify、Modifier、Root Motion、窗口时序或视觉接触的部分，必须标记为 seam/static coverage，不能冒充 Editor readback 或 PIE 证据。

## 用户资产与验证门槛

用户拥有资产作者化与运行验证；本阶段不由 Gemini 或 Main 手工改 `.uasset`/`.umap`，不导入/移动/复制/重定向资产，也不提交 Content WIP。

用户在 Editor readback 中逐项确认每个要采用的 Charged/Sprint GA+Montage：

1. GA 的实际父类分别是 `UChargedAttackAbility` / `USprintAttackAbility`，且 Montage 引用来自同一真实可达输入/Loadout 路径；
2. `bUseMotionWarping` 与五项配置明确，目标名与 Montage 中直接存在的 `AnimNotifyState_MotionWarping` 窗口严格一致；
3. Motion-Warp Modifier 的平移/旋转设置、窗口起止时序适配本阶段 capture 时机；Charged 窗口在 release/resume 后仍有效，Sprint 窗口覆盖 Montage 启动后的 apply 时点；
4. Montage 使用可由 `ABP_Player_Dungeon` 驱动的 Root Motion，Slot/Sequence/Notify 没有只存在于不可搜索的错误层级；
5. 同一 Player 只有一个 `UMotionWarpingComponent`，且 `bSearchForWindowsInAnimsWithinMontages=false` 的直接组件设置保持成立；
6. 原有 `AttackTraceWindow`、HoldReady、Dodge/Rate Window Notify 仍在正确 Montage 上，未被 Motion-Warp 调整破坏。

缺少 readback 只能记录“尚未证明 adoption”，不能自动写成 no-adoption；只有实际资产不适用或 PIE 失败，才记录 evidence-backed `no-adoption`。

## 证据分层与验证顺序

### Gemini 可做的静态门

- 读取最终 diff 与一跳 callers/callees；若 `.codegraph/` 存在，用定点查询确认 `BeginRelease/ActivateAbility -> Motion-Warp apply -> EndAbility`，以及 Player bridge/evaluator 调用；`.code-review-graph/` 仅作补充影响证据。
- 对九个批准路径运行 Rider lint/problem（若可用）与 `git diff --check`；不运行 UBT、Build.bat、Editor、Automation、PIE、打包或提交。
- 报告 changed paths、静态结果、self-review findings、未运行门禁与任何停止条件。

### 用户门

1. VS2022 手动编译 `PolyQuestEditor` Development Editor；
2. Editor readback 完成上述 GA/Montage/Notify/Modifier/Root Motion/组件设置清单；
3. 运行 focused `PolyQuest.Combat.PlayerMeleeMotionWarping`，并按实际结果记录 Charged/Sprint、Light、Trace/Resolver、Lock-On、Projectile/Bow、Guard/Parry 回归套件数量；
4. 在 `/Game/Maps/Scene01` PIE 验证 Charged Hold→Release、Sprint Attack、无锁/近距/超距/超角/空中、目标移动/切锁/死亡/销毁、取消、离地、UnPossess/重新 Possess、地图 teardown，以及伤害/削韧/成本/Guard/Dodge/Sprint 回归；
5. 视觉上确认 Root Motion 接触改善且没有穿模/贴脸/持续追踪；确认 Warp 失败时普通攻击仍按原链路完成。

所有证据必须注明类型：静态、编译、Automation、Editor readback、PIE/视觉；不得把 CodeGraph、Rider、`git diff --check` 或测试 seam 当作运行时证明。

## 成功标准与关闭条件

本阶段的“源码实现收口”以以下条件为准；其中手动编译与作者化 readback 是后续 clean authored baseline 的额外门槛，不被当前用户 Automation/PIE 结果替代：

- C1 Charged 与 C2 Sprint 的源码生命周期切片均已实现；每个 authored Ability 的最终 `adopted`/evidence-backed `no-adoption` 结论仍以用户 readback/PIE 证据为准；
- 对任何实际采用的 Ability，必须证明一次性静态快照、目标验证、几何门控、Task/Montage 时序与所有取消/销毁/离地/EndAbility 清理；
- Charged Hold 不捕获、Release 成功后只捕获一次；Sprint 只在有效攻击 Montage 启动后捕获且不破坏 Sprint ownership；
- 现有 Light entry `0..2` 与唯一 `Trace -> Resolver -> Damage GE` 路径无回归；Charged/Sprint 的 Cost、Poise、Tags、Rate、Dodge/Guard、Trace 和 Damage 无回归；
- focused Automation 与 Scene01 PIE 有独立用户记录；手动编译、Editor readback 等未运行或缺失门禁必须明确列为债务；
- Main 完成一次当前批准范围内的 defect-first fresh review；若发现 P0/P1 或共享 API/异步生命周期风险，只做最小证据扩展并先返还合同决策；
- Main 完成文档收口后，等待用户明确批准再精确 staging/commit。不得把 Config/Content WIP 或其他 Source/document WIP 纳入。

如果某个 Ability 没有合适的作者化 Motion-Warp Notify/Root Motion，保持默认关闭并以 no-adoption 关闭该切片仍是成功结果；不能为追求“两个都采用”而放宽几何、生命周期或资产证据门槛。

## 非目标与明确排除

- 不实现 Light 之外的通用连招状态、不修改 Light 已验证的 entry `0..2` 行为。
- 不接入 Melee Skill、Bow/Projectile、Enemy Melee、Hit Reaction、Guard/Parry、Locomotion 或远程敌人；这些属于后续独立阶段。
- 不新增全局 Targeting/Lock-On 服务、自动重选、持续追踪、Tick、Timer、Dispatcher、GameplayCue、第二套 Damage/Trace 路径或通用 Ability Framework。
- 不移动 `UMotionWarpingComponent` 到 `ABaseCharacter`，不新增 Player 组件，不改变 ASC/AttributeSet/GameplayTag/Input/Config/Build.cs/uproject 所有权。
- 不改变 Charged/Sprint 的数值伤害、削韧、Cost、Stamina 恢复、Action Tags、Dodge/Guard 取消、Sprint 结束顺序或动画 Rate Window。
- 不修改、清理、回滚、重置、提交任何 `Content/**`、Config WIP、旧 Test 项目或用户资产。

## 风险、债务与停止条件

当前没有阻塞本阶段源代码收口的债务。TODO-03A7B 缺少独立手动编译与 entry 1/2 资产 readback 的问题仍是作者化基线验证债务，不得被本阶段的静态/Automation 结果覆盖，也不阻止本阶段收口。

非阻塞风险：

- 候选 GA/Montage 可能没有直接 Motion-Warp Notify、Root Motion 或正确时间窗；关闭条件是用户真实 Editor readback/PIE，而不是文件名推断。
- Montage Task 或 Notify 可能在 `ReadyForActivation()` 中同步结束；关闭条件是生产门禁与清理测试，不得扩大 test bypass。
- Charged 的暂停/恢复与 Sprint 的 Sprint-cancel 时序是两个独立生命周期；任何需要共享可变状态、跨 Ability 锁定或额外 Player/Enemy 文件的方案都必须回 Main 决策。
- 共享 helper 若被实现为接收 UObject、回调或 Ability 引用来统一完整捕获流程，属于超出本计划的架构变化；应退回各 Ability 私有编排，而不是为消除重复继续扩张。
- 本阶段 Charged/Sprint 的源码接入、focused Automation 与 Scene01 PIE 已有证据；没有单独记录的 `PolyQuestEditor` 手动编译或逐项 GA/Montage/Notify/Modifier/Root Motion Editor readback，因此不能宣称 clean authored baseline。关闭触发器是用户完成对应编译/readback，或对不适用 Ability 给出真实 no-adoption 证据；这是非阻塞验证债务，不阻止后续制定 `TODO-03A7D`。
- `ReadyForActivation()` 后真实生产 Task/Montage 联动的无头端到端证据仍有限；现有 seam 覆盖 Task/Montage 门禁与清理约束，但没有把 bypass 当作生产运行时证明。只有能在不削弱真实门禁的前提下建立确定性夹具时才补测，不作为当前源代码 blocker。

Gemini 必须立即停止并回报证据，不得自行绕过或扩大范围，如果：

- 需要第十个路径、`PlayerCharacter.*`/`EnemyCharacter.*`/Build.cs/Config/Content/额外 Tag/Input/资产修改；
- 需要改变 `SetMeleeMotionWarpTarget`/`ClearMeleeMotionWarpTargets` 的生产合同、ASC/Ability 所有权、伤害链或现有 Light 行为；
- 无法在不旁路真实 Task/evaluator/验证/清理的情况下建立测试 seam；
- 共享 helper 需要 UObject/回调驱动的生命周期接口，或 Light 快照迁移会改变现有字段/Getter/行为；
- 发现当前 Engine API、实际源码或 Editor readback 与本计划冻结合同冲突。

## 依赖顺序与后续路线

本阶段的技术依赖为：

`TODO-03A7B（当前 ae0139b） → TODO-03A7C-C1 Charged → TODO-03A7C-C2 Sprint → TODO-03A7D Melee Skill → TODO-05A → TODO-05B → TODO-07B5 → TODO-07B6 → TODO-03C`

`TODO-03A7E Enemy Melee Motion-Warp` 仍在首个远程敌人之后，属于可选阶段；装备/拾取、AI/StateTree、Poise、Persistence 和 Bow 不是本阶段前置。`ROADMAP.md` 继续作为轻量路线/依赖指针，详细基线、快照、证据与 closeout 只放本 `plan.md`，历史 closeout 进入 `ROADMAP-archive.md`。

## 执行交接记录

- 交接方式：本阶段完整 handoff 由 Main 在对话中直接发送给 Gemini，不重复嵌入 `plan.md`；Gemini 必须把本计划作为唯一冻结合同。
- 执行路线：`manual/out-of-band Gemini`；`Contract owner: Main`；`implementation writer: Gemini`。Gemini 已完成 9 个批准 Source/test 路径；Main 仅在同一批准路径内对 Charged/Sprint 的 Montage/AnimInstance 有效性门禁做了窄 `IsValid()` 收紧，没有改变时序、公开 API、所有权或资产边界。Main 保留范围/架构、验证解释、fresh review、文档、staging 与 commit 所有权。
- 交接词必须明确绝对仓库路径、当前基线、批准文件、非目标、停止条件、用户验证门和“禁止编译/Editor/Automation/PIE/stage/commit/清理 WIP”；若交接词与本计划冲突，以本计划和 Main 的后续范围决定为准。

## 实施、验证与复核收口记录（2026-08-31）

### 实施范围

- Gemini 完成以下 9 个批准 Source/test 路径：共享 `MeleeMotionWarping` config/snapshot/evaluator、`LightAttackAbility` 兼容 wrapper、`ChargedAttackAbility`、`SprintAttackAbility` 及现有 `PlayerMeleeMotionWarpingAutomationTests` 扩展。
- Charged 只在成功 Cost/Commit、主动 Montage 仍活跃、写入 Damage/Poise 与 `bReleaseStarted` 后、`Montage_Resume()` 前尝试一次；HoldReady/Charging 不捕获。Sprint 只在 Montage Task active/unfinished 与主动 Montage active 两级门禁通过后、取消 Guard/Sprint 前尝试一次。
- 两个 Ability 各自拥有非反射快照，复用共享纯 evaluator 和既有 Player bridge；失败、取消、目标失效、EndAbility 与同步 Task/Montage 早退均 fail-closed 清理。Light 的 entry `0..2`、Trace/Resolver/Damage、ASC/Tag/Input/Cost/资源与 Sprint ownership 未改写。
- Main 的窄修复仅将 Charged/Sprint 的 Montage/AnimInstance 门禁补为 `IsValid()` 安全检查；它属于已批准文件内的可逆局部缺陷修复，不是新架构或范围扩张。

### 证据分层

- **用户运行证据**：用户确认最终修复后的 focused `PolyQuest.Combat.PlayerMeleeMotionWarping` Automation 成功，并确认 Scene01 PIE 通过；该证据只覆盖实际运行场景，不扩展为全量套件或编译证明。
- **静态证据**：Gemini 报告 9 个批准文件 Rider error-level 检查无 error、`git diff --check` 通过；Main 对窄修复文件完成定点 lint，并以当前 diff-first、一跳 CodeGraph 和补充 code-review-graph 做了独立 defect-first fresh review，未发现需要返工的阻塞缺陷。Code-review-graph 的风险分数/测试缺口只作影响提示，不作运行时结论。
- **未提供/未重复运行**：没有独立记录的 VS2022 `PolyQuestEditor` Development Editor 编译，也没有逐项 Charged/Sprint GA/Montage/Notify/Modifier/Root Motion 的 Editor readback；本轮未重新启动 Editor、Automation 或 PIE。真实生产 Montage/Task 的所有异步时序仍以用户 PIE 和未来可安全建立的端到端夹具为边界，不以 test bypass 冒充证明。

### 文档与提交边界

- 本次收口更新 `plan.md`、`ROADMAP.md`、`ROADMAP-archive.md`、`ARCHITECTURE.md`、`README.md`，并在项目 `AGENTS.md` 增加按变更性质而非 P1/P2/P3 级别授权的 Main 窄修复例外；不修改全局 `ue-stage-workflow`。
- `ROADMAP.md` 只保留当前/下一阶段指针与 canonical validation debt；本详细基线、工作区快照、实现/验证/复核收据继续由 `plan.md` 保存；本阶段历史 closeout 追加到 `ROADMAP-archive.md`，归档不是运行时权威。
- 本阶段候选提交只包含 9 个批准 Source/test 路径和明确的文档收口路径；`Content/**`、`Config/Automation/Presets/1.json`、其他 Source/文档 WIP、`PolyQuest.uproject`、`Build.cs`、Gameplay Tags、Input 与用户资产全部排除。当前尚未 stage/commit，等待用户明确批准。

### 阶段结论

- TODO-03A7C 的源码接入与已确认的用户验证已完成；Charged/Sprint 的作者化 opt-in、Notify/Modifier/Root Motion 与逐项资产 readback 仍是非阻塞验证债务。没有证据时不把候选资产写成已采用，也不把缺失 readback 写成 no-adoption。
- 下一阶段唯一指针为 `TODO-03A7D：Melee Skill Motion-Warp Adoption v1`；在其计划获用户确认前不开始实现。
- 文档收口后当前 `git status --short` 为 334 项（293 项 tracked、41 项 untracked）；其中 `Content/**` 仍为 318 项（279 tracked、39 untracked），`Config/Automation/Presets/1.json` 为 1 项，`Source/**` 为 9 项（7 tracked、2 untracked），其余为本阶段文档变更。该快照仍不是 clean checkout。
- 本轮没有修改 `Content/**`、Config WIP 或 Editor 状态，没有编译、打包、stage、commit、reset、删除或清理 WIP。
