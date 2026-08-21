# TODO-03B：Player Bow And Projectile v1

## 状态与目标

- **Plan state:** COMPLETE - `03B-1`（2026-08-21）；用户确认当前 `PolyQuestEditor` 编译/加载、五套 Automation 和手动验证均通过且无明显问题。`03B-2` 仍未开始，不能据此提前进入目标辅助、追踪或 `TODO-03C`。
- **Baseline:** `3872a8e`（TODO-03H1 武器与战斗架构健康审查收尾）。
- **Main pre-integration working baseline:** `Source/PolyQuest/Public/Combat/Equipment/WeaponEquipmentComponent.h`、对应 `.cpp`、`Source/PolyQuest/Private/Tests/WeaponEquipmentComponentAutomationTests.cpp` 与 `Config/Tags/PolyQuestGameplayTags.ini` 的未提交改动属于 Main 前置交付；Gemini 必须保留，不得重置、回滚或改写。
- **Prerequisites:** `TODO-03A1`、`TODO-03A2`、`TODO-03A3`、`TODO-03A4`、`TODO-03A5B`、`TODO-03AI`、`TODO-03AI1`、`TODO-03AI2`、`TODO-03H1` 已完成。
- **Stage split:** `03B-1` 已证明一支直线箭的完整运行时生命周期；后续 `03B-2` 才能处理指针朝向、目标辅助和有限追踪。
- **Primary question:** 在不污染近战链、不引入玩家/敌人投射物继承树的前提下，能否让玩家的 TwoHanded Bow 通过 GAS 完成蓄力、释放、投射物命中，并留下 `TODO-03C` 可复用的窄投射物契约。

~~~
Outer: ue-stage-workflow
Primary: ue5-cpp-gameplay
Support: ue5-architecture, ue5-debug-validation
Route reason: 本阶段同时涉及 DataAsset、GAS Ability、运行时 Projectile Actor、碰撞生命周期和俯视角目标快照；必须先建立窄的共享运行时契约，再分别接入玩家瞄准与未来敌人瞄准。
~~~

## 锁定架构决策

1. `UWeaponDefinition` 保持玩家装备抽象基类；Bow 锁定为窄的具体 `UBowWeaponDefinition`，其 `HandSlot` 为 `MainHandTwoHanded`，只增加 Bow 默认 Projectile Definition 与发射 Socket 引用，不新增 Ranged 插槽或抽象 `URangedWeaponDefinition`，也不污染 Sword/Shield。其定义校验必须要求有效的 `AssociatedLoadout`，且该 Loadout 明确把 `Input.PrimaryAttack` 映射为 `Ability.Attack.Primary`；当前组合还必须有且仅有一个组件-owned Primary grant，其 Ability CDO 携带该 Primary tag。
2. 新增不可变的 `UProjectileDefinition`（名称可按项目命名规范微调）保存投射物作者化数据。`03B-1` 只包含显示网格、初速度、寿命、碰撞半径和伤害 GameplayEffect；有限追踪参数、目标点和目标引用属于 `03B-2`，不得在当前切片启用。它不得保存目标、飞行状态、冷却、当前拥有者或运行时句柄。
3. 只建立一个具体的 travelling-projectile runtime Actor（暂称 `ACombatProjectile`）。它以 `USphereComponent` 为 Root/UpdatedComponent，使用 `UProjectileMovementComponent` 处理有 Sweep 的移动；`03B-1` 强制 `ProjectileGravityScale = 0.0f`、不反弹、不开启 Homing，不手写 Tick 位移。`03B-1` 只拥有移动、碰撞、寿命、单次命中保护和失效清理；可选制导与目标失效回退接口留给 `03B-2`，且它不读取输入、不查询 AI、不搜索目标。成功 Spawn 后 Projectile 独立拥有飞行生命周期；Bow Ability 结束时只解除委托和引用，不因 Ability 结束而销毁已发出的 Projectile。
4. Projectile 碰撞是独立的 QueryOnly 运行时边界：Sphere 显式忽略 `ECC_Camera`、自身和发射来源；不会阻挡 SpringArm，也不产生物理碰撞伤害。对有效敌对角色的首次接触只进入 Projectile Resolver；对世界阻挡物的接触只结束 Projectile；所有其他通道都由最小、显式的 Response Matrix 拒绝。`03B-2` 即使使用 `UProjectileMovementComponent` 的 Homing API，也不得改变这一碰撞/伤害所有权。
5. 玩家 Bow Ability 与未来敌人 Ranged Ability 分开。玩家 Bow 通过现有 `Input.PrimaryAttack -> Ability.Attack.Primary` 路由激活，但装备 Bow 时只授予 `UBowDrawFireAbility`，不与 `UPrimaryAttackAbility` 并存；`03B-1` 的一次性 `FProjectileLaunchRequest`（或等价窄结构）只传入来源 ASC、生成变换和初始方向，目标点快照/可选目标引用留给 `03B-2`。
6. Quick Tap 锁定为一次合法的最小蓄力释放，不是静默取消：正常物理输入路径先登记 Held、同步激活 Bow Ability，Ability 必须先激活 `Released`/`Canceled` 等待任务再启动 Draw Montage。若 `Released` 早于 Bow Draw-ready 语义事件，Ability 记录一次待释放和原始 HeldDuration，并在第一个有效 Draw-ready 事件开始最小蓄力 Release；最终仍只有当前 Bow Montage 的 Release 语义事件能 Spawn Projectile。`03B-1` 的“最小蓄力”只定义时序，不引入伤害、速度、弹种或 DataAsset 的蓄力倍率。`Canceled`、Dodge、Guard Break、现有角色 teardown 或 Montage 失败优先清理且绝不发射。玩家 Death 终结链尚未建立，按收尾 P3 交给 `TODO-03D`。若 Ability 由非正常输入路径激活且初始时未 Held，则 fail-closed；不为此向 `PlayerCharacter` 增加第二套输入缓存或状态机。
7. 不创建 `APlayerProjectile` / `AEnemyProjectile` 派生树，也不提前创建通用 Projectile Framework。只有第二种投射物出现真正不同的生命周期后，才重新评估派生 Actor。
8. Projectile 命中建立独立的 Projectile GAS 投递路径；复用 ASC、队伍、Dead、Invulnerable 和防御契约，但不把 Projectile 碰撞接入 `UAbilityTask_MeleeTraceWindow` 或 `FMeleeHitResolver`。不直接写 Health/Poise 属性，所有伤害仍通过 GameplayEffect。
9. 玩家目标策略与敌人目标策略分离：
   - 玩家在 `03B-2` 由 Bow Ability 解析指针相对的平面朝向，可选地选择一个有效敌对目标并传入 3D 目标点；追踪只在 Projectile Definition/LaunchRequest 明确允许时启用。
   - 敌人在 `TODO-03C` 使用 AI Controller 的 `CurrentTarget`，在发射时取得一次方向/目标快照，默认不追踪，不引入全局 Lock-on。
10. Bow 的 RMB/Q 防御沿用当前 Defense Profile 和 Guard/Parry Ability 生命周期；`03B-1` 不修改 Guard、Parry、ASC、AttributeSet 或近战防御逻辑。Projectile 对玩家防御的专门交互留给 `TODO-03C` 或单独防御切片。

## 当前执行切片：03B-1（已完成）

### 目标

完成一支默认玩家箭的最小垂直闭环：Bow 装备 -> LMB 蓄力 -> 动画释放事件 -> 生成直线 Projectile -> 命中敌人一次 -> 应用 Damage GameplayEffect -> Projectile 销毁或寿命结束。

### Main 前置集成（执行者开始前）

- Main 先确认并实现（若当前源码尚无）`bool TryGetEquippedMainHandDisplaySocketTransform(FName SocketName, FTransform& OutTransform) const`。接口先将 `OutTransform` 置为确定的 Identity，再验证当前主手、有效的主手显示网格、Socket 存在及取得的世界变换有限；Bow 类型由 Bow Ability 自己验证。该前置改动不授予 Ability、不改变装备事务、不暴露瞬时组件，也不允许 Gemini 代改。
- Main 还负责在 `Config/Tags/PolyQuestGameplayTags.ini` 增加两个窄的动画语义 Tag：`Event.Attack.Bow.DrawReady` 与 `Event.Attack.Bow.Release`。它们不改变物理输入、`Ability.Attack.Primary` 路由或任何全局 Lock-on；Gemini 只能读取并使用已存在的 Tag，不能编辑 Config。
- Main 已完成源码读回与静态检查：查询覆盖无 MainHand、有效 Socket 和缺失 Socket 的 Identity-reset 行为；`TransactionMatrix` 还会断言两个 Tag 已注册且无重复。用户手动编译 `PolyQuestEditor` 及运行该定向 Automation 通过前，这些仅是静态证据，不得视为可运行验证。
- **用户确认的前置验证（2026-08-19）：** `PolyQuest.Equipment.TransactionMatrix` 为 `Success`。输出中的 TwoHanded/OffHand 冲突、重合 Socket、应用或掉落阶段回滚、战斗中换装拒绝，均是该矩阵主动覆盖的负面分支；未报告新增 Socket 查询或 Bow Event Tag 断言失败。用户尚未单独报告 `PolyQuestEditor` 编译结果。
- 用户已明确放行 Gemini 开始 03B-1。查询契约及两个 Bow 动画语义 Tag 已读回，`TransactionMatrix` 已通过；前置没有单独记录完整编译日志，因而 Gemini 必须保留该事实并在实现完成后的完整用户编译/Automation/Editor/PIE 门禁中重新验证，不得将当前 Automation 说成独立编译证明。

### 允许实现

- `UProjectileDefinition` 的最小有效性校验和作者化字段。
- 一个具体 travelling Projectile Actor 及其 paired `.h/.cpp`：
  - `USphereComponent` Root、`UProjectileMovementComponent` 和显式 QueryOnly Response Matrix；`ECC_Camera`、来源 Actor/组件和无关通道必须 Ignore，世界阻挡物只销毁 Projectile；
  - 由 `UProjectileMovementComponent` 以 LaunchRequest 中的初始方向/速度驱动 Sweep，`03B-1` 明确关闭重力、反弹与 Homing；
  - 忽略自身、无效队友和已死亡/无敌目标；
  - 只向一个目标成功投递一次；
  - 有限寿命、销毁和 EndPlay 清理；
  - 目标/来源失效时 fail-closed，不访问悬挂对象。
- 独立 `ProjectileHitResolver`（可为窄 `F` helper），只负责 Projectile 命中检查、Effect Spec 构造和一次性应用。
- 具体 `UBowWeaponDefinition` 玩家装备定义及其 `MainHandTwoHanded`、有效 `AssociatedLoadout` 的 Primary 路由、Projectile Definition 引用、发射 Socket 和显示配置校验。发射 Socket 必须在 `WeaponMesh` 上存在；缺失时装备预检或 Ability 发射均 fail-closed。
- 独立 `UBowDrawFireAbility`（名称可按项目规范微调）：
  - `ActivateAbility` 在确认 `Input.PrimaryAttack` 仍处于 Held 后，先激活 `Released`/`Canceled` 等待任务再开始 Draw；角色的 `Event.Input.Pressed` 发生在 Ability 激活之前，不能依赖该事件重放来启动 Draw；
  - LMB Completed/Released 只在合法状态下请求释放。Quick Tap 若发生在 Draw-ready 前，保存一次待释放和原始 HeldDuration；第一个身份有效的 `Event.Attack.Bow.DrawReady` 将其转换为最小蓄力 Release，不能遗漏、重复释放或在输入回调中直接 Spawn；
  - LMB Canceled、Dodge、Guard Break、现有角色 teardown 和 Montage 失败都走统一清理；Dodge 继续通过现有 `Ability.Attack.Primary` 取消链中断 Draw，Guard/Parry 只在 Bow Ability 显式拥有现有 `State.Action.CanCancel.Defense` 时中断，不新增无条件防御取消规则；成功释放后的 Projectile 不属于 Ability 的结束清理对象。玩家 Death 终结链不在本切片实现；
  - 不复用近战 `UChargedAttackAbility` 的 Trace/伤害状态机；
  - 在一个动画释放事件确认后才 Spawn Projectile，不在输入回调中直接制造命中。
- 两个 Bow 专用 Notify/Event（沿用项目的 Gameplay Event + `OptionalObject` 身份校验模式）：`Event.Attack.Bow.DrawReady` 只打开/推进 Draw 的 Release 资格，`Event.Attack.Bow.Release` 才由 Ability 在验证当前 Bow Montage、Release 已开始且尚未 Spawn 后生成 Projectile。不得复用名字或语义属于近战 Charged Attack 的 Notify。
- 03B-1 固定使用 Bow Definition 上的一个默认 Projectile Definition，不增加装备组件的 Projectile slot API；`1-4` 的兼容 Projectile Definition 选择完整留在 `03B-2`，不得把运行时选择写回任何 DataAsset。
- 发射位置必须来自当前 Bow 显示网格的作者化 Socket；若现有装备组件缺少该读取能力，由 Main 在执行前增补一个只读的 `TryGetEquippedMainHandDisplaySocketTransform(FName, FTransform&)`（或等价窄 API），只返回世界变换，不暴露瞬时组件、不改变装备事务。Bow Ability 仍必须先验证当前定义类型和 Socket 名称。

### 03B-1 明确不做

- 不做指针目标候选查询、自动目标选择、3D 高低差修正和追踪；03B-1 复用现有 `APlayerCharacter::GetActionWorldDirection()` 的确定平面方向（无移动输入时使用角色前向），不新增鼠标/指针查询。
- 不新增物理 Input、Primary 路由或通用 Gameplay Tag；仅 Main 前置集成的 `Event.Attack.Bow.DrawReady` / `Event.Attack.Bow.Release` 是本切片允许的窄动画语义 Tag。
- 不做远程敌人、敌人 StateTree/AI、敌人 Projectile Profile 或敌人瞄准。
- 不改 `UChargedAttackAbility`、`UAbilityTask_MeleeTraceWindow`、`FMeleeHitResolver`、Guard/Parry、Melee AttackSet、近战 Trace Geometry 或现有装备回滚语义。

### 03B-1 成功标准

- 无目标时玩家可完成一次合法 Draw/Release，Projectile 沿确定的平面方向飞行。
- Bow 的发射 Socket 缺失、显示网格失效或当前主手不是 Bow 时，Ability/组件 fail-closed，不在猜测位置生成 Projectile。
- Bow 装备后 `Input.PrimaryAttack -> Ability.Attack.Primary` 只解析到 `UBowDrawFireAbility`；`UPrimaryAttackAbility` 不得同时存在。Dodge 继续通过现有 Primary 取消链中断 Draw；Guard/Parry 仅在 Bow 显式开放既有 `State.Action.CanCancel.Defense` 窗口时中断。
- Projectile 命中敌人时最多生成一次 Damage GE；重复碰撞、同帧重复回调、目标死亡或来源销毁不会重复伤害或访问失效对象。
- Quick Tap（Draw-ready 前 Released）在首个有效 Draw-ready 后只进入一次最小蓄力 Release，随后只由当前 Montage 的 Release 事件 Spawn 一支箭；Canceled 不发射，外部非 Held 激活 fail-closed。
- Draw/Release/Canceled/已有取消链/角色 teardown/Montage 播放失败都能清理 Ability 标签、Montage 委托、临时箭显示和 Projectile 引用；玩家 Death 中断由 `TODO-03D` 统一建立。
- Bow 仍是 TwoHanded，Shield 互斥；当前 Sword、Unarmed、Shield、敌人近战和拾取事务没有行为回归。

## 后续切片：03B-2（不得提前执行）

`03B-1` 的用户编译/Automation/手动验证已通过；`03B-2` 仍需在新的计划中重新定义其独立的 Editor Readback、Automation 和 PIE 门禁。

- Bow Draw 期间使用 Bow 专用的指针到地面/平面方向解析，使角色朝向指针；不改全局摄像机、自由俯仰、普通近战朝向或全局 Lock-on。
- 在有限半径和前向角内筛选有效敌对目标；使用稳定的距离/角度 Tie-break，必要时做可读的 Visibility 检查；不把候选目标写入 `WeaponDefinition`。
- 释放时保存有限的 3D 目标点快照和可选弱目标引用；Projectile 只在配置允许时制导，目标失效后沿最近有效速度继续飞行，不重新搜索目标。
- 扩展 Bow 的组件-owned `1-4` Projectile Definition 选择，保留现有近战 Ability slot 的精确 Handle 契约，不把 Projectile Definition 伪装成 GameplayAbility。
- 完成一支箭的 prepared-arrow 显示、Bow Montage/AnimBP 读回和目标辅助/追踪 PIE 验证。

## 所有权与数据流

~~~
Player Input.PrimaryAttack
        -> active MainHand resolver
        -> UBowDrawFireAbility
        -> Bow release Gameplay Event
        -> FProjectileLaunchRequest (一次性快照)
        -> ACombatProjectile
        -> ProjectileHitResolver
        -> Damage GameplayEffect on target ASC
~~~

- `UWeaponEquipmentComponent` 继续只服务 `APlayerCharacter`，负责 Bow 的装备组合、AbilitySpec 和组件-owned Projectile selection；敌人不走该事务。
- `UProjectileDefinition` 和 `UBowWeaponDefinition` 只保存作者化数据；Projectile、Ability、Controller 不把运行时状态回写资产。
- `ACombatProjectile` 不拥有输入、AI、装备、目标搜索或全局锁定；来源 Ability 在 Spawn 时传入全部必要快照。
- Projectile 的视觉网格和碰撞组件不能成为额外伤害路径，也不能阻挡 Camera；伤害只能由 ProjectileHitResolver 创建并应用 GameplayEffect。
- 本项目仍是单机；不添加复制、预测、RPC、服务器权威改造或网络 Projectile。

## 允许修改路径

### Gemini 可执行的受限原生路径

- `Source/PolyQuest/Public/Combat/Projectile/**`
- `Source/PolyQuest/Private/Combat/Projectile/**`
- `Source/PolyQuest/Public/Combat/Equipment/ProjectileDefinition.h`
- `Source/PolyQuest/Private/Combat/Equipment/ProjectileDefinition.cpp`
- `Source/PolyQuest/Public/Combat/Equipment/BowWeaponDefinition.h`
- `Source/PolyQuest/Private/Combat/Equipment/BowWeaponDefinition.cpp`
- `Source/PolyQuest/Public/AbilitySystem/Abilities/BowDrawFireAbility.h`
- `Source/PolyQuest/Private/AbilitySystem/Abilities/BowDrawFireAbility.cpp`
- 配套的 Bow 释放 Notify/Event 类型及 `Source/PolyQuest/Private/Tests/**` 自动化测试。

### Main 所有的集成边界

- `PlayerCharacter` 的公开指针朝向 API、`WeaponEquipmentComponent` 的公共 Projectile slot/输入集成、Gameplay Tag 配置、Build.cs 或其他公共跨系统契约，必须先由 Main 审核；Gemini 不得自行扩大这些接口。03B-1 应优先复用现有 Primary 路由和取消标签；若无法在不改公共契约的情况下完成 Bow 激活/打断，Gemini 必须停止并报告最小接口需求。
- 若需要从装备的运行时显示网格读取 Bow 发射 Socket，`Source/PolyQuest/Public/Combat/Equipment/WeaponEquipmentComponent.h` 与对应 `.cpp` 只允许 Main 增加一个 fail-closed 的只读世界变换查询；该查询不是 Projectile/Weapon 基类，也不授予 Ability、不改变事务所有权。Gemini 只能调用已批准的 API，不能编辑这两个文件。
- Main 可在既有 `WeaponEquipmentComponentAutomationTests.cpp` 增加仅覆盖该查询契约的断言；Gemini 不得移除、弱化或重写 Main 的前置测试。
- `Content/**` 的 Bow、Projectile、Montage、AnimBP、Blueprint、Input、DataAsset 和 Map 作者化由用户在 Unreal Editor 完成；禁止手工写 `.uasset`/`.umap`。

## 禁止与非目标

- 禁止抽取 `WeaponComponent`、`ProjectileComponent` 或 `PlayerProjectile/EnemyProjectile` 抽象继承层。
- 禁止实现远程敌人 AI、敌人 StateTree、敌人追踪、Boss、Staff 持续施法、Beam、Area Effect、投射物物理弹道或弹药持久化。
- 禁止建立全局 Lock-on、自由垂直鼠标瞄准、相机重构、Motion Warping 或普通移动朝向重构。
- 禁止把 Projectile 命中接到近战 Sweep/Resolver，禁止直接写 Health/Poise，禁止新增第二套 DamageEffect 规则。
- 禁止修改现有 Sword/Unarmed/Shield 装备事务、近战 Trace、敌人近战冷却/Approach、SaveGame、多人网络或无关文档/资产。
- 禁止提交 Git；阶段收尾由 Main 负责。

## 验证矩阵

### 静态验证

- 读取新增 `.h/.cpp` 与直接调用者，确认所有 Timer、Hit/Overlap、Montage Delegate、AbilityTask 和 Projectile EndPlay 路径都检查对象有效性和当前 Ability 身份。
- CodeGraph 优先查询 Projectile Actor、Bow Ability、Projectile Resolver 的 callers/callees；`.code-review-graph` 仅作为补充并标注覆盖限制。
- `git diff --check`；检查没有错误的 Gameplay Tag 名、未使用 Include、公开头循环依赖或隐式运行时 DataAsset 写入。
- 不运行 UBT/Editor build；编译由用户执行。

### Automation（03B-1）

新增一个聚焦套件（建议 `PolyQuest.Projectile.Lifecycle`，最终命名由 Main 审核）：

- 有效 Definition 可生成、移动并在寿命到期清理；缺失/非有限/非正参数 fail-closed。
- Projectile 由 `UProjectileMovementComponent` 执行无重力、无反弹、无 Homing 的 Sweep 移动；Sphere 的 `ECC_Camera` 响应为 Ignore，命中世界阻挡物不应用 Damage GE。
- Bow 发射 Socket、当前显示组件和当前 Bow 定义的有效/缺失组合均有 fail-closed 覆盖；不能以角色原点、固定组件名或旧近战 Marker 静默回退。
- 命中异队目标应用一次 Damage GE；重复 Hit、同帧 Overlap、Self、同队、Dead、Invulnerable 均不产生额外伤害。
- 发射时保存来源 ASC/Instigator 的弱引用快照；Projectile/目标失效时安全退出。Bow Ability 在箭已成功生成后结束，不应使箭因 Ability 身份失效而静默失效；只有来源 ASC/Instigator 本身不可用时才 fail-closed。
- Bow Ability 的正常释放、取消、Montage 失败和角色 teardown 清理不会残留 Projectile、Tag、Delegate 或临时显示。玩家 Death 终结清理由 `TODO-03D` 统一验证。
- Quick Tap、Draw-ready 前 Released、Released/Draw-ready 同帧、重复 Released、Canceled 与非 Held 外部激活均有覆盖：仅合法 Quick Tap 发射一支最小蓄力箭，其余路径不残留 Tag/Delegate 或误发射。
- Bow 装备/路由自动化确认 Primary Ability 的唯一性、`UPrimaryAttackAbility` 不共存；同时覆盖 Dodge 的 Primary 取消与 Guard/Parry 只在 `State.Action.CanCancel.Defense` 存在时结束 Bow Draw。
- 既有 `PolyQuest.Equipment.TransactionMatrix`、`PolyQuest.Melee.TraceSourceGeometry`、`PolyQuest.Enemy.AttackSetSelection`、`PolyQuest.Enemy.CombatSpacing` 继续通过。

### 用户编译与 Editor Readback（03B-1）

- 用户手动编译 `PolyQuestEditor`，报告准确目标和结果。
- 读取 Bow Definition：`UBowWeaponDefinition`、`HandSlot=MainHandTwoHanded`、Bow Mesh/发射 Socket、默认 Projectile Definition、Bow Ability/Defense Profile 引用均有效；确认其 `AssociatedLoadout` 明确为 `Input.PrimaryAttack -> Ability.Attack.Primary`，Bow `BaseGrantedActions` 只含 `UBowDrawFireAbility` 的 Primary 路径，`BP_Player` 的 `StartupAbilities` 也不含 `UPrimaryAttackAbility` 或其他携带 `Ability.Attack.Primary` 的旧 Ability。
- 读取 Projectile Definition：速度、寿命、半径、Damage GE 和显示资源有效；没有运行时状态字段。
- 读取 Bow Montage/Notify/AnimBP/Blueprint/Input 资产，确认 `Event.Attack.Bow.DrawReady` 与 `Event.Attack.Bow.Release` 只绑定当前 Bow Ability，且 Release 事件位于正确的箭离弦时点；所有作者化资产仍由用户保存。

### PIE（03B-1）

在 `Scene01` 验证：

- Bow 装备时不允许 Shield 共存，Sword/Unarmed/Shield 原有路线不回归。
- LMB 按住进入 Draw，释放后在动画 Release 点生成一支箭；极快点击也在 Draw-ready 后走一次最小蓄力 Release。Canceled、Dodge、Guard Break 和 Montage 播放失败均清理且不误发射；玩家 Death 中断不属于当前可验证产品路径。
- 无目标时箭沿确定平面方向直线飞行，命中敌人只造成一次伤害，飞出寿命后消失。
- 箭的显示/碰撞不遮挡摄像机，不产生独立碰撞伤害；近战 Trace 和已有四套自动化测试无回归。

### 03B-2 额外门禁（暂不执行）

- 指针朝向、目标候选筛选、3D 高低差目标点、目标丢失回退和有限追踪的独立 Automation/PIE 证据。
- 目标选择不穿越明确阻挡物、不重新搜索目标、不改变普通近战和全局相机行为。

## 复核、收尾与提交门禁

- Gemini 先进行只读计划审阅；只有 Main 接受计划后执行 `03B-1`，并完成严格执行者自审。
- Main 对最终差异执行 normal review，再执行 adversarial fallback；Luna 不可用时不宣称独立 Fresh Reviewer。
- `03B-1` 通过用户编译、Editor Readback、Automation 和 PIE 后，Main 更新计划记录，再明确开启 `03B-2`；未通过不得进入 `TODO-03C`。
- 03B 完整收尾后同步 `README.md`、`ARCHITECTURE.md`、`ROADMAP.md` 和本文件；稳定运行时契约写入 ARCHITECTURE，临时作者化步骤不写入。
- 用户明确批准后提交；提交包含批准的原生源码、测试、两条 Bow Gameplay Tag 配置和文档，排除 `Content/**`、`.uproject`、`Config/Automation/**`、`Config/Tests/**` 和其他 WIP。

## 委托记录与交接

~~~
Plan explorers: 0
Implementation executors: 1 (Gemini)
Complex Executor: one scoped lifecycle-sensitive projectile runtime slice (03B-1)
Main parallel work: public API/input/tag integration review, Editor/compile/PIE gates, final review and documentation
Reason: Projectile lifecycle can be bounded to new runtime/data/test files; Main must retain ownership of public GAS/input/tag contracts and all authored Editor state.
~~~

## Gemini 交接提示词

~~~
工作目录：E:/GameDevelop/PolyQuest
基线提交：3872a8e；执行工作基线还包含 Main 未提交的 Socket 查询、其 TransactionMatrix 覆盖和两个 Bow Event Tag。保留这些前置改动，不得 reset、checkout、revert 或改写；当前 ROADMAP.md 已记录 TODO-03B 的共享投射物边界和 03B-1/03B-2 顺序。

角色：Gemini，03B-1 的受限 Complex Executor。第一轮只读审阅已完成；本次是 Main 的第二次明确执行指令，可以开始 03B-1。用户确认当前 Editor 会话的 `PolyQuest.Equipment.TransactionMatrix` 成功，但没有单独报告完整 `PolyQuestEditor` 编译日志；不得把该结果称为独立编译证明，完整用户编译仍是实现后的必过门禁。使用 ue-stage-workflow、ue5-cpp-gameplay、ue5-architecture、ue5-debug-validation。不要把自己的审阅称为 Main fresh review。

执行目标：仅实现 03B-1 的一支直线玩家箭完整闭环：`UBowWeaponDefinition`/Projectile Definition 校验、一个具体 travelling Projectile Actor、Projectile 专用 GAS 命中投递、Bow Draw/Fire Ability 和释放事件、配套 Automation。03B-2 的指针瞄准、候选目标、3D 快照和追踪禁止提前实现。

允许路径：plan.md 中列出的 Combat/Projectile、ProjectileDefinition、BowWeaponDefinition、BowDrawFireAbility、Bow Notify/Event 和 Tests 路径。`WeaponEquipmentComponent.h/.cpp` 的 Socket 查询及 `Config/Tags/PolyQuestGameplayTags.ini` 中两个 Bow 动画语义 Tag 由 Main 先行完成，Gemini 只读调用/使用。Projectile 必须使用 `USphereComponent` + `UProjectileMovementComponent`，03B-1 关闭重力、反弹和 Homing，显式 Ignore `ECC_Camera`。禁止手工编辑或删除任何 .uasset/.umap，禁止修改 Content、Config、.uproject、Build.cs、ASC/AttributeSet、公共输入路由或无关源文件；需要其他公共 API、Gameplay Tag、Input 或组件 slot 改动时停止并把最小接口需求报告给 Main，不要自行扩大范围。

硬性契约：不创建 PlayerProjectile/EnemyProjectile 继承树；Projectile 不搜索目标、不读取输入、不查询 AI；不复用 MeleeTraceWindow/MeleeHitResolver；不直接写 Health/Poise；所有伤害通过 Projectile 专用 Resolver -> GameplayEffect；Bow 复用现有 `Input.PrimaryAttack -> Ability.Attack.Primary` 路由且不得与 `UPrimaryAttackAbility` 或任意旧 Primary-tag Ability 并存；Quick Tap 必须在 Bow Draw-ready 后形成一次最小蓄力 Release，Canceled 永不发射，且只有当前 Bow Montage 的 Release 事件可 Spawn；Dodge 复用 Primary 取消链，Guard/Parry 只在既有防御取消窗口中断 Draw；所有异步/碰撞/委托/EndPlay 路径必须检查对象和当前 Ability 有效性。

执行顺序：
1. 先读现有 WeaponDefinition、WeaponEquipmentComponent、PlayerCharacter 输入/Ability 路由、GAS Ability 生命周期和测试；确认 Main 已提供的 Bow 显示 Socket 查询、`Event.Attack.Bow.DrawReady` / `Event.Attack.Bow.Release` 和 `Event.Input.Pressed` 先于 Ability 激活的时序。
2. 实现并测试 Projectile Definition/Actor/Resolver 的 fail-closed、单次命中、寿命和清理。
3. 实现 Bow Draw/Fire 的最小直线释放；Ability 激活时从 Held 状态开始 Draw，释放必须经过当前 Bow Montage 的语义事件，不在输入回调直接生成命中。
4. 运行静态检查、git diff --check 和新增/回归 Automation；不得运行未经用户授权的 UBT/Editor build。
5. 完成严格自审，报告实际修改路径、调用链、测试结果、用户编译/Editor/PIE 缺口、未决公共接口和停止原因。

停止条件：发现需要修改公共输入/Gameplay Tag/ASC/Build.cs、现有近战链、资产类路径或需要 Editor 写入时立即停止，保留已完成的孤立代码并报告，不要提交 Git。
~~~

## Closeout Record (2026-08-21)

### 已交付

- `03B-1` 建立 `UBowWeaponDefinition`、`UProjectileDefinition`、`UBowDrawFireAbility`、Bow Draw/Release Notify、`ACombatProjectile` 与 `FCombatProjectileHitResolver` 的最小直线箭闭环。
- Bow 从当前主手显示 Socket 发射，复用现有 `Input.PrimaryAttack -> Ability.Attack.Primary` 路由但不与 `UPrimaryAttackAbility` 共存；Quick Tap 在有效 Draw-ready 后只形成一次最小 Release。
- Projectile 使用 QueryOnly Sphere + `UProjectileMovementComponent`，无重力、无反弹、无 Homing，并显式忽略 `ECC_Camera`。命中只经 Projectile Resolver 构造 GameplayEffect，不写 Health/Poise，也不接入近战 Trace/Resolver。
- Projectile 在发射时将 Damage GameplayEffect class 保存到 transient reflected snapshot；来源 Actor/ASC 继续使用弱引用。原始 Definition 在发射后不再是命中所需对象，避免 GC 后的悬挂读取。

### 用户验证

- 用户确认当前 `PolyQuestEditor` 已编译并成功加载，可在 Editor 内执行 Automation。
- 用户确认 `PolyQuest.Enemy.AttackSetSelection`、`PolyQuest.Enemy.CombatSpacing`、`PolyQuest.Equipment.TransactionMatrix`、`PolyQuest.Melee.TraceSourceGeometry`、`PolyQuest.Projectile.Lifecycle` 均为 `Success`。
- 用户确认手动验证通过且无明显问题。测试输出中的装备冲突、Socket 缺失、无 AttackSet、无默认装备、无 Poise 配置等日志均来自负向分支/测试夹具，不是本阶段失败。

### 复核

- Gemini 已完成严格执行者自审。
- Main 完成最终 normal review 与 `Main adversarial fallback`：未发现 P0-P2。Luna 不可用，因此没有独立 Fresh Reviewer 结论。
- `git diff --check` 无空白错误；`.code-review-graph` 索引基线为 `3872a8e`，无法可靠覆盖未跟踪的新增 Bow/Projectile 源码，因此最终结论以直接源码、调用链和用户验证为准。

### P3 债务与归属

- **玩家 Death 终结清理，归属 `TODO-03D`：** 当前 `State.Status.Dead` 只阻止新的 Bow 激活，不会取消已激活的 Bow Draw/Release。玩家死亡/重载尚未建立，不应由 Bow 单独创造特殊死亡监听。`TODO-03D` 必须以一个统一终结路径取消所有活跃玩家 Ability，并覆盖 Bow、Light、Charged、Guard、Parry、Dodge 与准备技能的 `EndAbility()` 清理。
- **层级 Primary tag 正向夹具，条件性归属首个 `Ability.Attack.Primary.*` 采用切片：** 当前 Bow 验证正确使用层级 `HasTag`，但自动化只覆盖无关 `UDodgeAbility` 的拒绝，尚未构造真实子标签 CDO。首次引入生产子标签前，补一个隔离正向夹具；不为此虚构生产 Gameplay Tag。

### 提交边界

- 包含：Bow/Projectile 原生源码、Projectile Lifecycle Automation、主手显示 Socket 查询、两条 Bow Event Tag、以及 `README.md` / `ARCHITECTURE.md` / `ROADMAP.md` / `plan.md`。
- 排除：所有 `Content/**` 作者化资产、`PolyQuest.uproject`、`Config/Automation/**`、`Config/Tests/**`、`.zcode/**` 和其他用户 WIP。
