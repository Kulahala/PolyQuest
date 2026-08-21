# TODO-03B-2: Player Bow Target Assistance And Limited Homing v1

## 完成状态与边界

- Plan state: COMPLETE，2026-08-21。
- Baseline: `47ad6d2` - `[Feature] 玩家直线弓箭与投射物 (Player Straight Bow And Projectile)`。
- 前置 `TODO-03B-1` 已提供 Bow 装备、Montage 语义事件、Socket 生成、Projectile 生命周期和独立 GameplayEffect 命中路径；本阶段没有重构这些所有权边界。
- 已完成范围：Bow Draw 指针水平朝向、Release 时一次目标辅助、限制追踪、屏幕边缘外扩候选与自动化覆盖。
- 明确未做：Bow `1-4` 弹种准备/切换、存档、背包、远程敌人、法杖、Beam、持续 Area Effect、Projectile 继承树、全局 Lock-on、自由垂直瞄准、近战/Guard/Parry/Dodge 行为重构、任何 Content/Config/.uproject 作者化写入。

~~~
Outer: ue-stage-workflow
Primary: ue5-cpp-gameplay
Support: ue5-debug-validation
Route reason: 本阶段跨越玩家鼠标投影、Bow GAS Release、目标资格、Projectile 生命周期和自动化边界。
~~~

## 已实现运行时契约

1. `UBowDrawFireAbility` 在激活后向 `APlayerCharacter` 登记唯一 Bow requester。Draw、Hold、Release 期间，角色只更新水平 Yaw；`EndAbility()`、取消、Montage 失败和 teardown 均由同一 requester 清理。
2. `APlayerCharacter` 通过本地 `APlayerController` deproject 鼠标射线，并与 `GetActorLocation().Z` 的角色中心参考平面求交，再从角色中心到交点的 XY 推导 Bow Yaw。该平面与旋转参考点一致，避免原 Capsule 底部平面在斜视透视下造成的系统性偏差。无本地 Controller、无有效 deprojection、平行射线、后方交点或非有限值时，保留最后合法 Bow Yaw；Release 回退到既有 `GetActionWorldDirection()`。
3. 只有当前 Bow Montage 的已验证 `Event.Attack.Bow.Release` 能进行一次候选查询。候选必须是异阵营、存活、非无敌、双方 ASC 有效的 `ACharacter`；稳定上半身瞄准点还要通过距离、前方夹角、高度、仰俯角、第三方 `ECC_Visibility`、相机前方和本地 Viewport 检查。
4. Viewport 检查采用当前屏幕宽高的固定 6% 矩形 overscan，允许刚越过边缘的目标在 Release 时入选；没有 Local Controller、Viewport 或有效投影时 fail-closed，不选择目标但仍按指针方向发射。当前本地 PlayerController 的 CameraManager 在可用时还会拒绝相机后方点。这个判断只在 Release 发生一次。
5. `FCombatProjectileLaunchRequest` 快照初始指针方向、可选弱目标、制导标量和目标约束。`ACombatProjectile` 先沿指针方向直飞，延迟后只调整 `UProjectileMovementComponent` 的 Velocity 方向；位置 Sweep 仍完全由该组件负责。飞行中不重搜、不换追、不重做 Visibility/Viewport 查询；目标离屏不取消追踪。目标失效、超出快照约束、耗尽时长/总转向预算、命中或 EndPlay 时停止制导并沿最后合法速度直飞。
6. `HomingMaxTotalTurnDegrees` 大于 90 度时，目标进入后半球仍可继续追踪。这是当前明确接受的休闲型回头箭玩法，而非错误；Actor lifespan 仍是箭的最终存活上限。
7. Projectile 保持一个具体 Actor：QueryOnly Sphere、`ECC_Camera` Ignore、零重力、无反弹、原生 Homing 关闭、单次 Projectile Resolver -> GameplayEffect 命中投递。没有新增 Health/Poise 直接写入、近战 Resolver 路径或玩家/敌人 Projectile 派生树。

## 验证与复核记录

### 用户确认的验证

- 当前 Editor Automation：`PolyQuest.Enemy.AttackSetSelection`、`PolyQuest.Enemy.CombatSpacing`、`PolyQuest.Equipment.TransactionMatrix`、`PolyQuest.Melee.TraceSourceGeometry`、`PolyQuest.Projectile.Lifecycle`、`PolyQuest.Projectile.TargetAssist` 均为 `Success`。
- 测试日志中的装备冲突、无效 Socket、无 AttackSet、无默认装备、无 Poise Recovery、无 Physics Asset 等 Warning 均来自 Automation 的负向夹具，不是本阶段失败。
- 用户完成 Standalone 视觉验证：Bow Aim Debug 显示 `ActorYaw: 18.2 | AimYaw: 18.2`，角色前向与计算的 Bow Aim 方向一致；用户确认当前指向基本一致并接受。

### 静态与代码复核

- Main 读取 Bow Ability -> Targeting Helper -> Launch Snapshot -> Projectile Tick/Hit Resolver 的直接调用链；确认屏幕筛选只在 Release-time 候选查询中发生，Projectile Tick 不会重搜、换追或因离屏停止。
- `PolyQuest.Projectile.TargetAssist` 覆盖中心平面求交、屏幕内、6% 边缘外扩、远端屏幕外、投影失败、无 Local Controller、失效目标、后半球回头及原有制导限制分支。
- Main fresh review 未发现 P0-P2。Luna 当前不可用，本阶段没有独立 Fresh Reviewer 结论；该限制已如实记录。
- 提交前运行 `git diff --check` 和缓存区 `git diff --cached --check`。

## 债务交接

- **玩家终结 Ability 清理，归属 `TODO-03D`：** 当前 `State.Status.Dead` 阻止新的 Bow 激活，但玩家死亡/重载尚未建立统一终结路径来取消已激活的 Bow Draw/Release、Light、Charged、Guard、Parry、Dodge 和 Prepared Ability。`TODO-03D` 必须建立该路径并验证每个 Ability 经 `EndAbility()` 收敛。
- **层级 Primary Tag 正向夹具，条件性归属首个 `Ability.Attack.Primary.*` 采用切片：** 当前 Bow 定义正确使用层级 `HasTag`，但尚未构造真实生产子 Tag CDO 的隔离正向夹具。首次采用该类子 Tag 前补充测试，不为关闭债务而虚构生产 Tag。
- **固定 6% Viewport Overscan：** 当前是所有 Bow Target Assist 共用的 C++ 策略值，不是 `UProjectileDefinition` 作者化字段。只有当不同弹种已证明需要不同屏幕外捕获范围时，才在对应 Projectile Definition 切片引入有限、可校验的作者化数据；本阶段不预建参数系统。

## 文档与提交边界

- `README.md`、`ARCHITECTURE.md`、`ROADMAP.md` 已同步 03B-2 的完成事实。`TODO-01C3` 是用户批准的后续 Roadmap 项，用于 Dodge Recovery、Bow Release 后取消窗口和连续链；它不包含在本阶段的源码实现中。
- 本次提交包含：03B-2 的 Bow/Player/Projectile 原生源码、`ProjectileTargetAssistAutomationTests.cpp`、以及上述阶段文档。
- 明确排除：所有 `Content/**`、`Config/Automation/**`、`Config/Tests/**`、`.zcode/**`、`PolyQuest.uproject` 和其他用户 WIP。
