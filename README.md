# PolyQuest

UE 5.8 C++ GAS-first single-player stylized action RPG.

## Status

PolyQuest has completed its native first-enemy combat loop and weapon/AI combat foundation end to end: input-intent routing, a linear light-attack Combo, Stamina exhaustion/recovery, Root Motion Dodge, hold/release Charged Attack, Sprint/Jump/Sprint Attack, shared weapon-motion tracing and GAS hit resolution, the first enemy Sight/StateTree/melee loop with Profile/reach/cooldown, terminal death with teardown and ragdoll presentation, charged-damage hit reaction with safe interrupt, enemy Poise with deferred Stance Break, the player directional Guard with Guard Break, a Q-triggered timed Parry that counters through the enemy Poise path, notify-timed enemy Hyper Armor, a combat-Notify ownership/naming audit, authored Montage rate windows for Light, Charged, and Sprint Attack, player-authored Action Windows and controlled Dodge recovery chaining (`TODO-01C3`), player equipment/loadout with direct GameplayAbility classes and prepared 1-4 skills, world equipment pickup and atomic drop-swap, melee sweep trace socket authoring, OffHand Shield composite defense, ordinary enemy weighted attack sets, static enemy weapon geometry binding with camera collision protection, enemy cooldown repositioning, distance-aware weighted melee approach and attack execution, the cross-stage weapon/combat architecture health and lean review (`TODO-03H1`), and the player Bow/Projectile target-assist and limited-homing slice (`TODO-03B-2`). The user has confirmed the relevant local `PolyQuestEditor` automation suites and Standalone visual routes for these stages. GameplayAbility, GameplayEffect, Montage, AnimBP, Blueprint, input, DataAsset, and map authoring assets remain local mutable WIP, so the focused source/config commits do not claim clean-checkout reproduction of those fixtures.

`TODO-03B-2` extends the TwoHanded Bow with local pointer-facing over a character-center reference plane, one Release-time hostile-target selection inside the current viewport plus a 6% edge overscan, and a projectile-owned target snapshot. Arrows first fly along the pointer direction, then optionally steer under authored duration, turn-rate, and total-turn budgets; a data value above 90 degrees deliberately permits the current casual U-turn behavior. A selected target never triggers a new search, and leaving the screen after Release does not cancel the arrow. The shared projectile collision, GameplayEffect delivery, and ownership boundary remain unchanged. PolyQuest continues to reuse validated player-facing behavior from the old Test project without copying its FSM, save schema, or authored asset topology.

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

PolyQuest 是一个以 UE 5.8、C++ 与 GAS 为核心的新风格化单机动作 RPG 项目。首个敌人的完整战斗闭环与武器/AI 战斗体系已原生落地：输入意图路由、连段轻击、体力耗竭与恢复、Root Motion 闪避、短按/长按蓄力攻击、冲刺/跳跃/冲刺攻击、共享武器轨迹命中与 GAS 解析、首个敌人的 Sight/StateTree/近战循环（含攻击 Profile/触及/冷却）、终态死亡与布娃娃表现、蓄力伤害受击硬直与安全打断、敌人韧性削韧与延后姿态破坏、玩家方向性格挡与破防、Q 键定时弹反（经敌人韧性路径反制）、通知时序的敌人霸体、战斗 Notify 归属与命名审计、Light/Charged/Sprint Attack 的作者化 Montage 速率窗口、玩家作者化 Action Window 与闪避恢复连续链（`TODO-01C3`）、直接绑定 GameplayAbility 类的主手/双手/空手装备系统、按精确 Handle 激活的 `1-4` 战技与首个 `GA_Skill_Whirlwind`、世界武器拾取与原子掉落交换、静态网格体刀刃轨迹 Socket 标定、副手盾牌复合防御、普通敌人加权攻击集、敌人静态武器几何绑定与摄像机碰撞忽略保护、冷却重定位、距离感知加权近战接近与攻击执行、跨阶段武器与战斗架构健康与精简审查门（`TODO-03H1`），以及玩家弓箭的目标辅助与有限追踪（`TODO-03B-2`）。用户已确认各阶段的本地 `PolyQuestEditor` 自动化测试矩阵与 Standalone 视觉路由。GA、GE、Montage、AnimBP、Blueprint、输入、DataAsset 和地图等作者化资产仍是本地高频 WIP，因此聚焦的源码/配置提交不宣称可以从干净检出完整复现这些夹具。

`TODO-03B-2` 已完成：双手 Bow 在 Draw/Hold/Release 期间以角色中心高度平面上的鼠标指针维持水平朝向；Release 时只在当前视口及 6% 边缘外扩内选择一次合规敌对目标，并把弱目标和制导标量快照交给 Projectile。箭先沿指针方向直飞，之后才按作者化时长、转向速度和总转角有限追踪；总转角大于 90 度时允许当前休闲向的回头追踪。目标在 Release 后离开屏幕不会触发换追或取消，投射物原有碰撞、单次 GameplayEffect 命中投递和生命周期所有权保持不变。

`TODO-01C3` 已完成：Bow 的 Draw/Hold/Release/Recovery 与 Charged Hold 只在作者化 `ActionDodgeCancelWindow` 中开放打断，`State.Action.Charging` 仅表示状态，不再是 Dodge 豁免。Dodge 使用 UE 5.8 的 `InstancedPerActor` 重激活路径：下一段必须先通过地面、Stamina 与取消许可预检，旧段才经 `EndAbility()` 清理；只有匹配的 Dodge Recovery 窗口允许这次连续启动。Dodge 自己拥有 CancelWindow/RateWindow、无敌与播放速率清理，按具体 Montage 实例绑定的 Task 回调确保旧段停止不会终止新段。用户已确认相关 Automation 与 Scene01 PIE 通过。

旧 `Test` 项目保留为独立的 UE 5.7 FSM 参考基线；PolyQuest 会按已验证的玩法合同重新实现功能，而不是直接搬运旧 FSM 和资产。
