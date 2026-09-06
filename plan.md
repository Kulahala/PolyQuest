# TODO-07B9: Dungeon Multi-Floor Trigger & Visibility System v1 实施计划

## 阶段定位与基线

- **目标**：构建一套针对地牢多层立体结构的楼层触发与可见性分层管理系统，彻底解决 45° 俯视角固定相机下上层地板、外墙与拱梁对下层的视线大面积遮挡，同时杜绝物理穿模与 Overdraw 性能灾难。
- **分支**：`main`；代码基线：`9a6f42f`。
- **工作树状态**：非 clean，包含用户-owned `Content/**`、Config、Blueprint、地图及其他本地 WIP；全部严格保留。
- **历史归档**：前一阶段 `Post-E Presentation Polish Baseline Realignment` 已完整归档至 `ROADMAP-archive.md:1355-1367`；Archive Preflight：PASS。
- **职责划分**：Main 拥有架构决策、计划维护、终审验收与提交把控；执行者仅限在批准的 Source/Test 路径内实施，严禁越权修改非批准路径、资产或直接提交。

## 核心铁律与架构细节（吸纳两轮会话评审意见）

1. **铁律 1（物理安全，严禁触碰 Collision）**：
   - 隐藏非活动楼层的 Actor 时，**只调用 `SetActorHiddenInGame(true)` 切断渲染与 Draw Call，绝对禁止关闭静态与动态 Collision**！
   - 二层巡逻 AI、NavMesh 寻路、可破坏物与战利品箱的物理摆放、以及弓箭与投射物的物理碰撞判定完全不受影响，坚决杜绝“上层怪物下饺子跌落”与“角色掉入虚空”。
2. **铁律 2（动静分离与 `FloorCutoffZ` 高度切面，杜绝误伤一层结构）**：
   - **MPC 参数采用高度标量 `FloorCutoffZ`**：
     - 在 `MPC_PlayerGlobals` 中不存容易误伤全场景的“通用透明度”，而是存储**当前允许渲染的最大高度标量 `FloorCutoffZ`**。
     - 玩家在 1 层时：`FloorCutoffZ` 平滑插值到二层地面高度（例如 `320.0f`）；
     - 玩家在 2 层时：`FloorCutoffZ` 插值到屋顶高度（例如 `1000.0f`）；
     - 材质中判定：凡是 `WorldPos.Z > FloorCutoffZ` 的像素才执行 Dither 消隐。1 层所有地面与大墙（$Z < 300$）在数学上天然免疫，绝无误伤可能。
   - **大小件自愈分类准则**：
     - **结构大件（`ManagedStructuralActors`）**：凡 StaticMesh 命名包含 `Wall / Floor / Arch / Ceiling / Roof`，或使用 `M_Tiling_Master` 材质资产的 Actor 自动归入大件，受 `FloorCutoffZ` 平滑 Dither 切面消隐控制；
     - **内景道具小件（`ManagedInteriorActors`）**：默认所有其他扫描到的 Actor（桌椅、木桶、箱子、火把、吊灯等）全部归为 Interior，在切层时由空间容器直接一键硬切显隐（Collision 严格保留），0 材质侵入，保护 Early-Z 并切断无用 Draw Call。
3. **铁律 3（空间聚合防漏，杜绝手动字符串 Tag）**：
   - 在关卡中放置 `AFloorVolume`，在 `BeginPlay()` 中依据自身的 Box Bounds 自动搜集管辖空间内的所有 Actor，彻底消除手动打字符串 Tag 遗漏导致的“半空漂浮火把与孤立木桶”。
4. **铁律 4（楼梯两端双 Trigger 防抖 + 既有 Vision Tunnel 兜底）**：
   - 楼梯底端放置 `AFloorTriggerVolume (TargetFloorIndex = 1)`，顶端放置 `AFloorTriggerVolume (TargetFloorIndex = 2)`；中间楼梯段为状态滞后区间（Hysteresis），维持当前楼层状态不变，彻底根除单边界 Overlap 反复横跳。
   - **下楼视线兜底**：下楼梯过程中若二楼门梁或拱门遮挡镜头，由既有的 `MF_VisionTunnelFade`（2D 深度 + 45° 锥角）负责即时视线开孔透视，确保下楼台阶与角色始终清晰可见。
5. **铁律 5（架构极简，拒绝过度设计）**：
   - 单人固定视角地牢场景不引入全局引擎级 Subsystem，状态由关卡内的 `AFloorVolume` 驱动并同步到已有的 `MPC_PlayerGlobals`。

## 批准修改的路径

### 允许新增的 Source / Test 路径
- `Source/PolyQuest/Public/Environment/FloorVolume.h`
- `Source/PolyQuest/Private/Environment/FloorVolume.cpp`
- `Source/PolyQuest/Public/Environment/FloorTriggerVolume.h`
- `Source/PolyQuest/Private/Environment/FloorTriggerVolume.cpp`
- `Source/PolyQuest/Tests/FloorVisibilityAutomationTests.cpp`

### 允许修改的头文件与文档路径
- `Source/PolyQuest/Public/Character/Player/PlayerCharacter.h`（仅用于同步已存在的 `SeeThroughChestZOffset = 0.0f`）
- `ARCHITECTURE.md`（同步多层楼层管理与动静分离可见性契约）
- `plan.md`（本计划）
- `ROADMAP.md`（同步切片指针）

### 明确排除的路径
- 排除全部 `Content/**`（`.uasset`、`.umap` 等资产由用户在 Unreal Editor 内独立操作或验证）。
- 排除 Config、GameplayTags、Input、Build.cs。

## 详细设计与实现细节

### 1. `AFloorTriggerVolume`（两端防抖触发器）
- 派生自 `AActor`，拥有 `UBoxComponent` 作为触发盒。
- 关键属性：
  - `UPROPERTY(EditInstanceOnly, Category="Floor") int32 TargetFloorIndex = 1;`
  - `UPROPERTY(EditInstanceOnly, Category="Floor") TObjectPtr<AFloorVolume> TargetFloorVolume;`
- Overlap 契约：
  - 仅响应 `APlayerCharacter`。
  - 触发时通知 `TargetFloorVolume->SetFloorActive(true)` 或将全局楼层状态提交给目标楼层。

### 2. `AFloorVolume`（空间管理与可见性驱动容器）
- 派生自 `AActor`，拥有 `UBoxComponent` 作为空间范围界定。
- 关键属性：
  - `UPROPERTY(EditInstanceOnly, Category="Floor") int32 FloorIndex = 2;`
  - `UPROPERTY(EditDefaultsOnly, Category="Floor") float ActiveCutoffZ = 1000.0f;`（本楼层激活时允许可见的最高 Z，覆盖本层天花板）
  - `UPROPERTY(EditDefaultsOnly, Category="Floor") float InactiveCutoffZ = 320.0f;`（本楼层未激活时切除高度，低于二层地板）
  - `UPROPERTY(EditDefaultsOnly, Category="Floor") float FadeDuration = 0.25f;`
  - `UPROPERTY(EditDefaultsOnly, Category="Rendering") TObjectPtr<UMaterialParameterCollection> PlayerGlobalsMPC;`
  - `UPROPERTY(Transient) TArray<TWeakObjectPtr<AActor>> ManagedInteriorActors;`（内景小件：桌椅家具、箱桶、火把，硬切显隐，Collision 严格保留）
  - `UPROPERTY(Transient) TArray<TWeakObjectPtr<AActor>> ManagedStructuralActors;`（结构大件：地面、大墙，由 `FloorCutoffZ` 平滑切面控制）
- 逻辑契约：
  - `BeginPlay()`：依据 Box Bounds 自动扫入内部 Actor。判断 Actor 包含 `Wall / Floor / Arch / Ceiling / Roof` 归为 Structural，其余全部归为 Interior；严格排除 Player 与全局 Actor。
  - `SetFloorActive(bool bActive)`：
    - `bActive == false`：所有 `ManagedInteriorActors` 立即 `SetActorHiddenInGame(true)`（Collision 绝不关闭）；开启平滑插值将 MPC `FloorCutoffZ` 平滑降至 `InactiveCutoffZ`。
    - `bActive == true`：所有 `ManagedInteriorActors` 立即 `SetActorHiddenInGame(false)`；平滑插值将 MPC `FloorCutoffZ` 恢复至 `ActiveCutoffZ`。

## 验证计划

### 1. Focused Automation 单元测试
- **空间自动搜集与分类测试**：验证 `AFloorVolume` 根据 Bounds 扫描包含 Actor，正确将 Wall/Floor 归入 Structural，小件归入 Interior，并排除 Player。
- **碰撞保留核心测试**：**重点验证**当调用 `SetFloorActive(false)` 时，被隐藏的 Actor 其 `GetActorHiddenInGame()` 为 true，但 `GetCollisionEnabled()` 严格保持原样，物理阻挡依然生效。
- **Trigger 防抖与去重测试**：验证重复进入相同目标楼层 Trigger 不产生重复切换。

### 2. 用户 Scene01 PIE 手动验证
- 在 Scene01 中为二层结构配置 `AFloorVolume`，并在一段代表性楼梯上下两端分别布置 `AFloorTriggerVolume`；
- 验证角色在楼梯上行走时视野切换顺畅，二层巡逻怪与木桶不发生掉落穿模，一层地面大墙无误伤，无闪烁、无漂浮火把。

## 阶段实施收口与验证记录 (Closeout)

- **实施结果**：
  - 新增 `FloorVolume.h/.cpp`：基于空间 Bounds 自动搜集管辖空间内 Actor，按命名或材质划分为结构大件（`ManagedStructuralActors`）与内景道具（`ManagedInteriorActors`）；未激活时严格只调用 `SetActorHiddenInGame(true)` 切断 Draw Call，**永久保留 Actor/Component 物理碰撞（Collision）**；支持根据 `bHideStructuralActorsWhenInactive` 联动隐藏大件并驱动 MPC `FloorCutoffZ` 平滑切面；Tick 动态按需休眠/唤醒，`EndPlay` 安全重置 MPC。
  - 新增 `FloorTriggerVolume.h/.cpp`：楼梯两端双 Trigger 防抖，严格过滤 `APlayerCharacter`；支持显式指定目标楼层或自动就近匹配 `AFloorVolume`；楼梯中间段为滞后区间，彻底消除单边界横跳与闪烁。
  - 新增 `FloorVisibilityAutomationTests.cpp`：4 套专项测试（`PolyQuest.Environment.FloorVisibility.ActorClassification`、`PlayerExclusion`、`CollisionPreservedWhenHidden`、`TriggerHysteresis`）全部通过。
  - 关联优化：`PlayerCharacter.h` 将 `SeeThroughChestZOffset` 调整为 `0.0f`（对齐胶囊体中心，消除贴墙视野盲区）。
  - 关卡视效与氛围：通过 MCP 实测并指导完成露天天空球清除与 `ExponentialHeightFog` 体积雾自发光/散射归零，彻底消除了露天蓝天，确立纯正暗黑地牢深渊氛围。
- **实施自审 (Implementation Self-Review)**：由独立子代理完成实施自审，确认 4 大核心铁律（物理安全不碰 Collision、动静分离、空间聚合防漏、两端 Trigger 防抖）完全兑现，无空指针与生命周期隐患。
- **验证结论**：
  - Rider 代码静态检查 0 Errors / 0 Warnings。
  - Focused Automation 4/4 全部通过（Success）。
  - 用户编辑器视口与 PIE 运行验证确认：二层切层顺畅、物理碰撞稳定不掉落、露天黑幕氛围建立。
- **提交边界**：
  - 仅包含批准的 5 个 Source/Test 路径、1 个 Player 头文件以及文档（`plan.md`、`ROADMAP.md`、`ARCHITECTURE.md`、`README.md`）。
  - 用户-owned `Content/**` 资产保留为本地 WIP，不纳入本次代码提交。
