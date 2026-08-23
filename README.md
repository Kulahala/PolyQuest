# PolyQuest

UE 5.8 C++ GAS-first single-player stylized action RPG.

## Status

PolyQuest has completed its native first-enemy combat loop and weapon/AI combat foundation end to end: input-intent routing, a linear light-attack Combo, Stamina exhaustion/recovery, Root Motion Dodge, hold/release Charged Attack, Sprint/Jump/Sprint Attack, shared weapon-motion tracing and GAS hit resolution, the first enemy Sight/StateTree/melee loop with Profile/reach/cooldown, terminal death with teardown and ragdoll presentation, tiered Small/Big/Launch hit reactions with grounded Root Motion interruption and CharacterMovement-owned launch/landing recovery, the player-only authored LandingRecovery-to-Dodge cancellation route, enemy Poise with deferred Stance Break, the player directional Guard with Guard Break, a Q-triggered timed Parry that counters through the enemy Poise path, notify-timed enemy Hyper Armor, a combat-Notify ownership/naming audit, authored Montage rate windows for Light, Charged, and Sprint Attack, player-authored Action Windows and controlled Dodge recovery chaining (`TODO-01C3`), player equipment/loadout with direct GameplayAbility classes and prepared 1-4 skills, world equipment pickup and atomic drop-swap, melee sweep trace socket authoring, OffHand Shield composite defense, ordinary enemy weighted attack sets, static enemy weapon geometry binding with camera collision protection, enemy cooldown repositioning, distance-aware weighted melee approach and attack execution, the cross-stage weapon/combat architecture health and lean review (`TODO-03H1`), and the player Bow/Projectile target-assist and limited-homing slice (`TODO-03B-2`). The user has confirmed the relevant local `PolyQuestEditor` automation suites and focused PIE/Standalone visual routes for these stages. GameplayAbility, GameplayEffect, Montage, AnimBP, Blueprint, input, DataAsset, and map authoring assets remain local mutable WIP, so the focused source/config commits do not claim clean-checkout reproduction of those fixtures.

`TODO-03B-2` extends the TwoHanded Bow with local pointer-facing over a character-center reference plane, one Release-time hostile-target selection inside the current viewport plus a 6% edge overscan, and a projectile-owned target snapshot. Arrows first fly along the pointer direction, then optionally steer under authored duration, turn-rate, and total-turn budgets; a data value above 90 degrees deliberately permits the current casual U-turn behavior. A selected target never triggers a new search, and leaving the screen after Release does not cancel the arrow. The shared projectile collision, GameplayEffect delivery, and ownership boundary remain unchanged. PolyQuest continues to reuse validated player-facing behavior from the old Test project without copying its FSM, save schema, or authored asset topology.

`TODO-02C3E` establishes the fail-closed Hit Reaction Tier classification contract (`Data.Reaction.Small`, `Data.Reaction.Big`, `Data.Reaction.Launch`) and delivers the first non-interrupting Player and Enemy Small reaction: `FHitReactionClassifier` provides pure, context-free tag classification; `APlayerCharacter` and `AEnemyCharacter` bind authoritative Health attribute value change delegates to dispatch target-side reaction events; `UPlayerSmallHitReactionAbility` and `UEnemySmallHitReactionAbility` provide native montage playback on `ReactionOverlayGroup.ReactionOverlay` without stopping movement or cancelling active actions; and `UEnemyHitReactionAbility` re-maps to Enemy Big. 8/8 automation test suites passed, and the user confirmed Scene01 PIE visual validation for non-interrupting overlay reactions.

`TODO-02C3F` completes the grounded, nonlethal Big reaction route for both targets. `UPlayerBigHitReactionAbility` and the retained `UEnemyHitReactionAbility` begin a full-body Montage before cancelling eligible actions; Montage Root Motion is the only planar displacement owner, movement can neither walk off ledges nor remain locked after a Falling transition, and the existing StateTree pauses Enemy attack decisions through `State.Action.HitReacting`. `FHitReactionImpactResolver` snapshots a fail-closed target-local attacker direction for later presentation work without steering current Root Motion. The remaining `Data.Reaction.Interrupt` asset was migrated to `Data.Reaction.Big`, and the retired Interrupt/Enemy.Hit tags were removed after a zero-reference asset scan. The user confirmed `PolyQuest.Combat.HitReaction` Automation and Scene01 PIE, including the migrated Unarmed Charged route.

`TODO-02C3G` completes the nonlethal Player/Enemy Launch route. A matching Launch Gameplay Event starts a full-body Takeoff Montage; the authored Commit Notify pauses that Montage on its airborne pose while `CharacterMovement` exclusively owns the physical arc. A bounded airborne watchdog, phase-aware movement delegate, and landing-time velocity brake prevent stale HitReacting/input/AI state and high-speed ground bounce. Actual grounding stops the paused Takeoff before an in-place prone-to-standing LandingRecovery begins. The shared impact resolver now uses the valid planar actor-center relative line first, with ImpactNormal as a fail-closed fallback, so launch velocity consistently moves the target away from its attacker. The user confirmed Automation and focused PIE after the lifecycle repair; mutable authored Launch assets remain local WIP.

`TODO-01C4` completes the Player-only recovery Dodge route. The existing `ActionDodgeCancelWindow` can authorize a normal grounded Dodge only during a matching active Player Launch `LandingRecovery` Montage; the scoped `State.Action.CanCancel.Dodge` contribution is removed on window end and every Launch cleanup path. Dodge still commits its normal cost before it cancels Launch, preserving the existing positive-Stamina soft-overspend rule and rejecting repeated startup inputs until Dodge opens its own recovery window. The user confirmed Automation and focused PIE; the authored Montage/GA/GE assets remain local WIP.

`TODO-02C3H` completes the Enemy-only landing-deferred Stance Break route. Once an Enemy Launch Takeoff has actually started, a zero-Poise result becomes one Enemy-owned pending intent rather than a mid-air Stance Break; only natural LandingRecovery completion, after Launch cleanup releases `State.Action.HitReacting`, may dispatch the existing Stance Break event. A positive Poise recovery clears the intent. Death, teardown, or an abnormal Launch end clear it without a late break; a surviving zero-Poise Enemy instead restores Poise through the existing recovery GameplayEffect. Player Guard/Parry and immediate Player Guard Break behavior are intentionally unchanged. The user confirmed `PolyQuest.Combat.HitReaction` Automation and focused PIE; authored assets remain local WIP.

`TODO-07A1` completes the core vital-display route: a passive local viewport HUD shows the Player's current/max Health and Stamina, while each living Enemy owns an always-visible Screen Space Health-only bar. Existing ASC attributes remain authoritative; the Controller and Enemy refresh the display through attribute delegates with explicit re-possession, death, and teardown cleanup. The user confirmed `PolyQuest.UI.VitalHUD` Automation, manual `PolyQuestEditor` compilation, Editor asset readback, and focused PIE. The project currently has no playable Player-healing source, so Automation drives the same ASC positive-Health refresh branch without claiming a PIE healing route; `TODO-03E` remains the first gameplay healing loop. Authored UMG/Blueprint assets remain local `Content/**` WIP.

`TODO-03A6A` establishes the native weapon-aware ordinary locomotion presentation contract (`Default`, `LightSword`, `HeavySword`, `SwordShield`, `Bow`). `UWeaponEquipmentComponent::GetResolvedLocomotionMode()` provides a stateless pure query evaluating committed weapon composition with zero caching, delegates, ticks, or GAS tag mutations; 7/7 automation test suites passed; 192 locomotion sequences were batch configured with `Force Root Lock = True`; and the user confirmed `Scene01` PIE visual validation across all weapon modes with smooth 0.15s transitions. `TODO-02C3G` subsequently completed the Launch and Landing reaction slice.

`TODO-01C3` is complete: Bow Draw/Hold/Release/Recovery and Charged Hold expose interruption only through authored `ActionDodgeCancelWindow`; `State.Action.Charging` remains descriptive state rather than a Dodge exemption. Dodge now uses UE 5.8's `InstancedPerActor` re-trigger path: a successor must pass its grounded/Stamina/cancel preflight before the old segment cleans up, and only a matching Dodge recovery window permits that successor. Dodge owns scoped CancelWindow permission, RateWindow playback-rate, invulnerability, and cleanup; its per-Montage-instance task callbacks prevent a stopped predecessor from ending the new segment. User-confirmed Automation and Scene01 PIE cover the completed Slice B route.

## Technology

- Unreal Engine 5.8
- C++
- Gameplay Ability System
- Enhanced Input
- Git LFS for Unreal packages
- Official Unreal MCP with VibeUE-enhanced editor services

## Project Layout

```text
Config/                 Project configuration
Content/                Authored Unreal assets
Source/PolyQuest/       PolyQuest runtime module
AGENTS.md               Collaboration and tooling rules
ARCHITECTURE.md          Verified implemented architecture
ROADMAP.md               Long-term milestone order
plan.md                 Active short-lived stage plan
```

## Local Setup

1. Install Unreal Engine 5.8 and Visual Studio 2022 with C++ game-development tooling.
2. Install Git LFS, then clone the repository and run `git lfs install`.
3. Open `PolyQuest.uproject`, regenerate project files when needed, and build `PolyQuestEditor` manually from Visual Studio.

The user owns compilation, PIE validation, packaging, and commit approval unless explicitly delegated.

## Documentation

- [`ARCHITECTURE.md`](ARCHITECTURE.md): implemented facts only.
- [`ROADMAP.md`](ROADMAP.md): accepted future milestones and adoption gates.
- [`plan.md`](plan.md): active scope, validation, and handoff detail.
- [`AGENTS.md`](AGENTS.md): repository-specific collaboration, MCP, Git, and validation rules.

## 中文说明

PolyQuest 是一个以 UE 5.8、C++ 与 GAS 为核心的新风格化单机动作 RPG 项目。首个敌人的完整战斗闭环与武器/AI 战斗体系已原生落地：输入意图路由、连段轻击、体力耗竭与恢复、Root Motion 闪避、短按/长按蓄力攻击、冲刺/跳跃/冲刺攻击、共享武器轨迹命中与 GAS 解析、首个敌人的 Sight/StateTree/近战循环（含攻击 Profile/触及/冷却）、终态死亡与布娃娃表现、分级 Small/Big/Launch 受击、地面 Root Motion 硬直与 CharacterMovement 击飞/落地恢复、玩家专属的 LandingRecovery-to-Dodge 作者化取消路线、敌人韧性削韧与延后姿态破坏、玩家方向性格挡与破防、Q 键定时弹反（经敌人韧性路径反制）、通知时序的敌人霸体、战斗 Notify 归属与命名审计、Light/Charged/Sprint Attack 的作者化 Montage 速率窗口、玩家作者化 Action Window 与闪避恢复连续链（`TODO-01C3`）、直接绑定 GameplayAbility 类的主手/双手/空手装备系统、按精确 Handle 激活的 `1-4` 战技与首个 `GA_Skill_Whirlwind`、世界武器拾取与原子掉落交换、静态网格体刀刃轨迹 Socket 标定、副手盾牌复合防御、普通敌人加权攻击集、敌人静态武器几何绑定与摄像机碰撞忽略保护、冷却重定位、距离感知加权近战接近与攻击执行、跨阶段武器与战斗架构健康与精简审查门（`TODO-03H1`），以及玩家弓箭的目标辅助与有限追踪（`TODO-03B-2`）。用户已确认各阶段的本地 `PolyQuestEditor` 自动化测试矩阵与聚焦 PIE/Standalone 视觉路由。GA、GE、Montage、AnimBP、Blueprint、输入、DataAsset 和地图等作者化资产仍是本地高频 WIP，因此聚焦的源码/配置提交不宣称可以从干净检出完整复现这些夹具。

`TODO-02C3E` 已完成：确立了由 Damage GE 资产标签作者化的受击分类闭环契约（`Data.Reaction.Small`、`Data.Reaction.Big`、`Data.Reaction.Launch`，多标签冲突 Fail-Closed 保护），并实现了首个玩家与敌人的无打断轻受击（Small）。`FHitReactionClassifier` 提供纯无状态分类；`APlayerCharacter` 与 `AEnemyCharacter` 绑定权威生命值变化委托以分发目标受击事件；`UPlayerSmallHitReactionAbility` 与 `UEnemySmallHitReactionAbility` 通过 `ReactionOverlayGroup.ReactionOverlay` 插槽与动画蓝图 `spine_01` 分层混合叠加播放受击动作，不打断走位与当前技能执行；原有 `UEnemyHitReactionAbility` 重映射为 Enemy Big。8/8 项自动化测试全部通过，用户已在 Scene01 PIE 确认轻受击叠加与蓄力受击打断表现。

`TODO-02C3F` 已完成玩家与敌人的地面非致死 Big 受击闭环。`UPlayerBigHitReactionAbility` 与保留名称的 `UEnemyHitReactionAbility` 先确认全身 Montage 启动，再取消符合条件的动作；Montage Root Motion 是唯一的平面位移所有者，角色在受击期间不能走出边缘，进入 Falling 会清理受击并恢复原有设置，Enemy StateTree 继续通过 `State.Action.HitReacting` 暂停攻击决策。`FHitReactionImpactResolver` 只快照 Fail-Closed 的本地攻击者方向，为后续表现升级保留单一来源，不改变当前 Root Motion 方向。遗留的 `Data.Reaction.Interrupt` 资产已迁移为 `Data.Reaction.Big`，旧 Interrupt/Enemy.Hit Tag 已在零引用资产扫描后删除。用户已确认 `PolyQuest.Combat.HitReaction` Automation 与 Scene01 PIE，包括无武器蓄力迁移回归。

`TODO-02C3G` 已完成玩家与敌人的非致死击飞闭环。匹配的 Launch Gameplay Event 启动全身 Takeoff Montage；作者化 Commit Notify 在水平空中姿态暂停它，随后由 `CharacterMovement` 独占整段抛物线。有限的起飞宽限 watchdog、分阶段 MovementMode 委托和落地即时刹车，避免残留的 HitReacting、输入/AI 锁定及高速触地颠簸；真正触地后才停止暂停的 Takeoff 并播放原地倒地起身 LandingRecovery。共享方向解析器优先使用攻击者与受击者的有限平面相对连线，失败时才回退 ImpactNormal，因此击飞稳定地远离攻击者。用户已确认 Automation 与修复后的聚焦 PIE；Launch 的 GA、GE、Montage、Notify、AnimBP、Blueprint、DataAsset 和地图资产仍是本地 WIP。

`TODO-01C4` 已完成玩家专属的起身翻滚路线。现有 `ActionDodgeCancelWindow` 只会在匹配且仍活跃的 Player Launch `LandingRecovery` Montage 中授权普通的接地 Dodge；该阶段对 `State.Action.CanCancel.Dodge` 的 scoped loose-tag 贡献会在窗口结束和所有 Launch 清理路径中移除。Dodge 仍会先提交其正常 Cost，随后才取消 Launch，因此保留“当前 Stamina 大于 0 即可开始、Cost 后钳制到 0”的软透支规则；连续输入在 Dodge 自身恢复窗口到来前不会覆盖新起手。用户已确认 Automation 与聚焦 PIE，Montage/GA/GE 作者化资产继续保留为本地 WIP。

`TODO-02C3H` 已完成仅限敌人的“落地后延迟姿态破坏”路线。Enemy Launch 的 Takeoff 真正启动后，Poise 归零会变为一个由 Enemy 持有的 pending intent，而不会在空中进入 Stance Break；只有自然完成 LandingRecovery、且 Launch 清理已释放 `State.Action.HitReacting` 后，才能分发现有的 Stance Break 事件。飞行期 Poise 恢复为正值会清除意图；死亡、teardown 或异常结束不会产生延迟破韧，存活且 Poise 为零的敌人则通过既有恢复 GameplayEffect 恢复 Poise。玩家 Guard/Parry 与立即生效的 Player Guard Break 故意保持不变。用户已确认 `PolyQuest.Combat.HitReaction` Automation 和聚焦 PIE；作者化资产仍是本地 WIP。

`TODO-07A1` 已完成基础生命值显示路线：本地 Player 视口 HUD 显示当前/最大 Health 与 Stamina，每个存活 Enemy 则拥有始终可见、仅显示 Health 的 Screen Space 头顶条。现有 ASC Attribute 仍是唯一权威；Controller 与 Enemy 通过属性委托事件驱动刷新，并在重新 Possess、死亡与 teardown 中显式清理。用户已确认 `PolyQuest.UI.VitalHUD` Automation、手动 `PolyQuestEditor` 编译、Editor 资产读回和聚焦 PIE。项目当前没有可玩的 Player 治疗来源，因此 Automation 直接驱动同一 ASC 的 Health 正向变化以覆盖显示刷新分支，但不把它表述为 PIE 治疗路径；首个真实治疗玩法仍由 `TODO-03E` 负责。UMG/Blueprint 作者化资产继续作为本地 `Content/**` WIP。

`TODO-03B-2` 已完成：双手 Bow 在 Draw/Hold/Release 期间以角色中心高度平面上的鼠标指针维持水平朝向；Release 时只在当前视口及 6% 边缘外扩内选择一次合规敌对目标，并把弱目标和制导标量快照交给 Projectile。箭先沿指针方向直飞，之后才按作者化时长、转向速度和总转角有限追踪；总转角大于 90 度时允许当前休闲向的回头追踪。目标在 Release 后离开屏幕不会触发换追或取消，投射物原有碰撞、单次 GameplayEffect 命中投递和生命周期所有权保持不变。

`TODO-01C3` 已完成：Bow 的 Draw/Hold/Release/Recovery 与 Charged Hold 只在作者化 `ActionDodgeCancelWindow` 中开放打断，`State.Action.Charging` 仅表示状态，不再是 Dodge 豁免。Dodge 使用 UE 5.8 的 `InstancedPerActor` 重激活路径：下一段必须先通过地面、Stamina 与取消许可预检，旧段才经 `EndAbility()` 清理；只有匹配的 Dodge Recovery 窗口允许这次连续启动。Dodge 自己拥有 CancelWindow/RateWindow、无敌与播放速率清理，按具体 Montage 实例绑定的 Task 回调确保旧段停止不会终止新段。用户已确认相关 Automation 与 Scene01 PIE 通过。

旧 `Test` 项目保留为独立的 UE 5.7 FSM 参考基线；PolyQuest 会按已验证的玩法合同重新实现功能，而不是直接搬运旧 FSM 和资产。
