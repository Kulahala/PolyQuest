# PolyQuest

![PolyQuest 游戏场景封面](docs/images/cover.jpg)

PolyQuest 是使用 **Unreal Engine 5.8、C++ 与 Gameplay Ability System（GAS）** 开发的单人低多边形动作 RPG。游戏采用固定俯斜视角，以近战攻防、体力管理和清晰的动作反馈为核心。

[观看战斗演示 · Bilibili](https://www.bilibili.com/video/BV14Ntw6RELr)

## 项目状态

项目仍在开发，当前重点是玩家战斗与首个近战敌人的可玩闭环。已经具备近战、弓箭、装备切换、锁定、受击反应和处决等基础；远程敌人、存档与休息点、消耗品、完整关卡和 Boss 流程仍属于后续工作。具体排期见 [开发路线](ROADMAP.md)。

演示与阶段验证基于本地制作的资产。部分 Blueprint、Montage、AnimBP、DataAsset、输入与地图仍是本地 WIP，**当前仓库不承诺干净克隆后即可复现全部演示，也不代表已具备发布构建**。各项验证的详细记录与剩余限制见阶段文档。

## 主要玩法

- **近战与体力管理**：轻攻击连段、蓄力攻击、冲刺攻击、跳跃与 Root Motion 闪避，配合体力消耗、恢复和耗尽约束。
- **防御与破绽惩罚**：方向格挡、盾牌防御、格挡破坏、限时弹反、敌人韧性与姿态破坏，以及正面处决和背刺。
- **弓箭与锁定**：移动拉弓、蓄持与释放，发射时选定目标及有限制导；屏幕空间锁定、目标切换与遮挡保持策略。
- **装备与交互**：主副手组合、装备驱动的攻击/防御能力与 1–4 技能槽，世界武器拾取、交换和交互提示。
- **敌人与战斗反馈**：基于 Sight/StateTree 的近战敌人、加权攻击与距离调整、四向轻重受击、接地 Root Motion 击倒、死亡布娃娃，以及刀光、箭矢拖尾、音效、镜头反馈和生命/体力 HUD。
- **场景可读性**：固定视角下的遮挡处理与地牢楼层可见性切换。

## 技术特点

- **GAS 统一战斗权威**：Ability、Effect、Tag 和 Attribute 承担动作激活、打断、消耗、状态和伤害；不维护平行动作状态机。
- **AI 与动作分工**：Controller/StateTree 管理感知、导航与战术意图，具体战斗动作由 Ability 执行。
- **数据驱动配置**：武器、投射物、攻击 Profile 与表现 DataAsset 描述内容，复用现有 C++ 运行时路径。
- **动画与玩法协作**：Montage/Notify 驱动攻击和动作窗口，Root Motion/CharacterMovement 管位移与碰撞，局部 Motion Warping 用于适用的攻击接近与对齐。
- **验证方式**：使用 Unreal Automation 覆盖关键合同与生命周期，并以 Editor/PIE 验证实际资产和游戏表现；两类证据分别记录。

更详细的模块所有权和数据流见 [架构说明](ARCHITECTURE.md)。

## 本地开发

环境基线为 Windows、Unreal Engine 5.8、Visual Studio 2022 C++ 游戏开发工具及 Git LFS。

1. 安装 Git LFS，在克隆前执行 `git lfs install`，克隆后执行 `git lfs pull` 拉取已跟踪的大文件。
2. 按 [PolyQuest.uproject](PolyQuest.uproject) 准备当前启用的插件。项目描述包含 VibeUE、MCP 工具集及其他 Editor 插件依赖，仓库没有项目级 `Plugins/` 目录，需在本机引擎环境中满足这些依赖。
3. 生成项目文件，使用 Visual Studio 构建 `PolyQuestEditor` 的 Development Editor / Win64 配置，再打开项目。
4. 本地内容基线齐备后，可在 `/Game/Maps/Scene01` 进行开发验证。缺少的本地 WIP 资产不会由 Git LFS 自动补齐。

当前输入验证以键鼠为准，尚不宣称手柄硬件支持。构建与运行需具备相应的本地插件和资产基线。

## 仓库导航

| 路径 | 内容 |
| --- | --- |
| [Source/PolyQuest](Source/PolyQuest/) | C++ 运行时模块及自动化测试 |
| [Config](Config/) | 项目与运行时配置 |
| [Content](Content/) | 已纳入仓库的 Unreal 资产 |
| [ARCHITECTURE.md](ARCHITECTURE.md) | 当前架构、所有权与运行时合同 |
| [ROADMAP.md](ROADMAP.md) | 当前入口、后续路线及开放风险 |
| [plan.md](plan.md) | 当前或最近切片的实施范围、验证与交接 |
| [ROADMAP-archive.md](ROADMAP-archive.md) | 历史交付和验证记录 |
| [AGENTS.md](AGENTS.md) | 仓库协作与修改规则 |
