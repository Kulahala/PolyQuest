# PolyQuest

UE 5.8 C++ GAS-first single-player stylized action RPG.

## Status

PolyQuest has completed its native first-enemy combat loop end to end: input-intent routing, a linear light-attack Combo, Stamina exhaustion/recovery, Root Motion Dodge, hold/release Charged Attack, Sprint/Jump/Sprint Attack, shared weapon-motion tracing and GAS hit resolution, the first enemy Sight/StateTree/melee loop with Profile/reach/cooldown, terminal death with teardown and ragdoll presentation, charged-damage hit reaction with safe interrupt, enemy Poise with deferred Stance Break, the player directional Guard with Guard Break, a Q-triggered timed Parry that counters through the enemy Poise path, notify-timed enemy Hyper Armor, a combat-Notify ownership/naming audit, authored Montage rate windows for Light, Charged, and Sprint Attack, the first player equipment/loadout foundation with an authored melee-weapon definition, runtime equip/swap transaction, and equipped trace markers, and the equipment-domain contract (`UWeaponDefinition` hand-slot base, reference-list combat actions, prepared `1-4` action slots, and Effective Defense Profile input resolution). The user has confirmed the relevant local `PolyQuestEditor` compilations and `Scene01` PIE routes for these stages. GameplayAbility, GameplayEffect, Montage, AnimBP, Blueprint, input, DataAsset, and map authoring assets remain local mutable WIP, so the focused source/config commits do not claim clean-checkout reproduction of those fixtures.

The equipment-domain contract is complete. `TODO-03A2: Player Main-Hand And Unarmed Combat v1` is the next roadmap stage, followed by world pickup/drop-swap, the OffHand Shield, and ordinary enemy weapon presets before the Bow. PolyQuest continues to reuse validated player-facing behavior from the old Test project without copying its FSM, save schema, or authored asset topology.

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

PolyQuest 是一个以 UE 5.8、C++ 与 GAS 为核心的新风格化单机动作 RPG 项目。首个敌人的完整战斗闭环已原生落地：输入意图路由、连段轻击、体力耗竭与恢复、Root Motion 闪避、短按/长按蓄力攻击、冲刺/跳跃/冲刺攻击、共享武器轨迹命中与 GAS 解析、首个敌人的 Sight/StateTree/近战循环（含攻击 Profile/触及/冷却）、终态死亡与布娃娃表现、蓄力伤害受击硬直与安全打断、敌人韧性削韧与延后姿态破坏、玩家方向性格挡与破防、Q 键定时弹反（经敌人韧性路径反制）、通知时序的敌人霸体、战斗 Notify 归属与命名审计、Light/Charged/Sprint Attack 的作者化 Montage 速率窗口、首个玩家装备/Loadout 基础（作者化近战武器定义、运行时装备/切换事务和随装备生效的轨迹标记），以及装备域合同（`UWeaponDefinition` 手持槽位基类、引用清单式战斗动作、`1-4` 预备动作槽与 Effective Defense Profile 输入解析）。用户已确认各阶段的本地 `PolyQuestEditor` 编译与 `Scene01` PIE 路由。GA、GE、Montage、AnimBP、Blueprint、输入、DataAsset 和地图等作者化资产仍是本地高频 WIP，因此聚焦的源码/配置提交不宣称可以从干净检出完整复现这些夹具。

装备域合同已完成。`TODO-03A2: Player Main-Hand And Unarmed Combat v1` 是下一阶段，其后依次是世界拾取/掉落置换、副手盾牌与普通敌人武器预设，最后是弓。

旧 `Test` 项目保留为独立的 UE 5.7 FSM 参考基线；PolyQuest 会按已验证的玩法合同重新实现功能，而不是直接搬运旧 FSM 和资产。
