# PolyQuest

UE 5.8 C++ GAS-first single-player stylized action RPG.

## Status

PolyQuest has completed its first single-player native GAS player-combat vertical slice: input-intent routing, a linear light-attack Combo, Stamina exhaustion/recovery, Root Motion Dodge, and hold/release Charged Attack. The user has confirmed local `PolyQuestEditor` compilation and `Scene01` PIE for this fixture. GameplayAbility, GameplayEffect, Montage, AnimBP, Blueprint, input, and map authoring assets remain local mutable WIP, so the focused source/config commits do not claim clean-checkout reproduction of that fixture.

The next combat foundation is Sprint, followed by the first enemy GAS/StateTree slice. PolyQuest continues to reuse validated player-facing behavior from the old Test project without copying its FSM, save schema, or authored asset topology.

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

PolyQuest 是一个以 UE 5.8、C++ 与 GAS 为核心的新风格化单机动作 RPG 项目。当前已完成首条单机 GAS 玩家战斗纵切：输入意图路由、两段轻攻击连击、体力耗尽/恢复、根运动翻滚和按住/松开的蓄力攻击。用户已确认本地 `PolyQuestEditor` 编译及 `Scene01` PIE；GA、GE、Montage、AnimBP、Blueprint、输入和地图等作者化资产仍是本地高频 WIP，因此聚焦的源码/配置提交不宣称可以从干净检出完整复现该夹具。

旧 `Test` 项目保留为独立的 UE 5.7 FSM 参考基线；PolyQuest 会按已验证的玩法合同重新实现功能，而不是直接搬运旧 FSM 和资产。
