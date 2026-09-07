# TODO-02B4: Lock-On Visibility Gate And Occlusion Grace v1 实施计划与收口记录

## 1. 阶段定位、基线与路由

- **Target Objective**：为玩家 Lock-On 增加独立的 Camera-to-Target LOS 门禁，阻止透过可见掩体的初始锁定、循环切锁和死亡后重锁；已持有目标可在连续遮挡时获得有限宽限，避免掩体边缘造成锁定抖动。
- **基线**：`main @ 31616b571df586db9ff46993ee85ba01d6b8ec01`（`TODO-07A2-A` 已提交）。
- **工作树**：非 clean。全部 `Content/**`、Config、Blueprint、AnimBP、Montage、地图、插件和其他本地 WIP 均为用户所有，必须保留并排除。
- **Archive preflight**：PASS。本次为同一 `TODO-02B4` 的计划修订，不是新阶段替换，不重复归档上一阶段。
- **技能路由**：
  - Outer: `ue-stage-workflow`
  - Primary: `ue5-cpp-gameplay`
  - Support: `ue5-debug-validation`
  - Route reason: 该切片只改玩家 Native C++ 锁定候选、射线判定、Tick 生命周期和聚焦 Automation。
- **执行路线**：`manual/out-of-band Gemini`。Codex/Main 保留架构、计划、范围、验证解释、Fresh Review、文档、暂存和提交所有权；Codex 不创建 in-app 子代理。Gemini 按项目规则完成实施及隔离自审，但不得越出批准范围或提交。

## 2. 冻结契约与刻意非目标

### 冻结运行时契约

1. **LOS 门禁**
   - 在 `APlayerCharacter` 中新增 `LockOnTraceChannel`，默认 `ECC_Visibility`；新增 `LockOnOcclusionGraceDuration`，默认 `2.5f` 秒。两者仅类默认值可调：`EditDefaultsOnly, BlueprintReadOnly`，不提供运行时 Blueprint 写入口。
   - 起点依次为 `PlayerCameraManager->GetCameraLocation()`、`FollowCamera->GetComponentLocation()`、安全的玩家位置回退；终点统一复用 `FCombatProjectileTargeting::GetTargetAimPoint(TargetActor)`。
   - 射线忽略玩家自身、递归附属 Actor（武器等）及目标自身。无归属 Actor 的世界静态几何、可见 Actor 和重复隐藏命中均视为有效阻挡。
   - 不得使用一次 `LineTraceMultiByChannel` 实现隐藏物穿透。UE 5.8 仅生成最近的一个 blocking hit；应使用重复 `LineTraceSingleByChannel`：命中隐藏 Actor 时将其加入忽略列表后重新追踪。
   - 总追踪次数最多 `8` 次（含初始追踪）。已忽略 Actor 再次命中或达到上限时 fail-closed，返回无 LOS；这既防异常物理状态死循环，也不把深层未知遮挡放行。
   - 视觉透明条件仅为 `HitActor->IsHidden()`。不在 v1 读取组件 `bHiddenInGame`：项目现有 `AFloorVolume` 契约是 Actor 级 `SetActorHiddenInGame()` 且保留碰撞；组件级语义会扩大到未证实的复合 Actor 行为。
2. **候选统一门禁**
   - 在 `BuildLockOnCandidates` 中，候选已通过现有敌对/ASC 有效性检查和严格屏幕投影后，才调用 `HasLineOfSightToTarget`；失败即跳过。
   - `TryAcquireLockOnTarget`、`HandleTargetCycleTriggered` 与 `TryRetargetAfterLockedTargetDeath` 继续复用该候选列表，因此自动共享 LOS 规则。
   - 当前被遮挡但尚在宽限期内的目标不获得候选例外，不得重新成为可获取对象；现有循环找不到合法替换时的 no-op 行为保持不变。
3. **遮挡宽限生命周期**
   - `Tick` 先执行既有 `ValidateCurrentLockedTarget()`；只有锁仍有效时才调用私有 `UpdateLockOnOcclusion(DeltaSeconds)`。死亡、屏幕 retention 边界和既有 GAS 无效路径先短路，避免对无效目标发射射线。
   - `UpdateLockOnOcclusion`：无有效目标时清零；`CanRetainExecutionLockedTarget(CurrentTarget, SourceASC)` 为真时清零并返回；有 LOS 时清零；无 LOS 时累计，达到有效宽限即通过现有 `ClearLockedTarget()` 安全清锁。
   - 无效、负数或非有限宽限配置按 `0.0f` fail-closed 处理。
   - `ClearLockedTarget()`、真正切换到不同目标、以及 Tick 发现 `LockedTarget` 弱引用失效时必须清零。对相同目标的重复 `SetLockedTarget()`、循环 no-op 或缓存刷新不得重置计时，防止输入刷新宽限。
4. **成对处决边界**
   - `CanRetainExecutionLockedTarget(...)` 为真时，严格豁免本阶段新增的 LOS 检查和遮挡超时，计时保持为零。
   - 该豁免不改变既有 15% 屏幕 retention、死亡/销毁、目标 GAS 有效性或现有处决屏幕边界清锁契约。

### Deliberate Non-goals

- 不改 `LockOnRetentionMarginRatio = 0.15f`，不让 retention 泄漏到初始获取、循环候选或 Bow Target Assist。
- 不改 Bow 6% 视口 Target Assist、投射物锁定、攻击/伤害路径、GAS 权属或处决流程。
- 不让 See-Through / `MF_VisionTunnelFade` 参与玩法 LOS；不改 `AFloorVolume`、碰撞资产或其物理碰撞保留策略。
- 不增加网络复制、Targeting Subsystem、泛化框架、`.Build.cs`、Config 或任何 `.uasset`/`.umap` 改动。

## 3. 批准路径、接口与所有权

### Gemini 实施白名单

- `Source/PolyQuest/Public/Character/Player/PlayerCharacter.h`
- `Source/PolyQuest/Private/Character/Player/PlayerCharacter.cpp`
- `Source/PolyQuest/Private/Tests/PlayerLockOnAutomationTests.cpp`

### 接口约束

- 新增私有辅助：安全起点解析、LOS 判定、遮挡更新和计时清零；运行时计时器保持私有，不为 HUD 或其他玩法系统暴露新 Shipping 查询 API。
- 自动化所需的 LOS Hook、遮挡更新触发器、计时只读查询、生产起点只读查询和真实 LOS 调用入口全部放在 `WITH_DEV_AUTOMATION_TESTS` 下，沿用现有 `PlayerCharacter` 测试包装模式。
- 不修改 `FCombatProjectileTargeting`、`PlayerLockOnTargeting`、`FloorVolume`、Build.cs、Tag 或资产。已有 `GetTargetAimPoint` 是只读复用点，不得为本阶段扩展其职责。

### Main 收口所有权

- 用户完成编译、Automation、Editor readback 与 PIE 门禁，且 Main Fresh Review 完成后，才由 Main 更新 `ARCHITECTURE.md`、`ROADMAP.md`、`ROADMAP-archive.md` 和 `plan.md` 收口记录。
- 本阶段实施交付不包含文档收口，也不包含 Git Commit。

## 4. Focused Automation 与验证矩阵

### `PolyQuest.Player.LockOn` 测试

1. `AcquireRejectsBlockedCandidate`：LOS Hook 返回遮挡时，鼠标/按键不能初始锁定该敌人。
2. `CycleSkipsBlockedCandidates`：当前目标可见时，循环只选择 LOS 合法候选，跳过遮挡敌人。
3. `DeathRetargetSkipsBlockedCandidates`：死亡目标的顺时针重锁只选择 LOS 合法候选。
4. `OcclusionGraceRetainsAndRecovers`：遮挡小于 `2.5s` 时保持锁定和高亮；恢复 LOS 后计时清零。
5. `OcclusionTimeoutClearsAndSameTargetCannotRefresh`：到达阈值时走 `ClearLockedTarget()`；同目标重复设锁或循环 no-op 不得重置已累计时间。
6. `ExecutionExemptionSkipsOcclusionOnly`：既有成对处决标签夹具加持续 LOS 遮挡后保持锁定且计时为零；不推翻既有“超出 15% retention 仍清锁”测试。
7. `HiddenActorRetryKeepsVisibleObstacleBlocking`：隐藏 Actor 位于前方、可见阻挡位于后方时仍无 LOS，证明重新追踪没有放行后方可见障碍。
8. `HiddenActorOnlyDoesNotBlock`：仅隐藏 Actor 保留碰撞时 LOS 通过，证明 Actor 级视觉透明契约。

真实射线用现有临时 World fixture 的可控 `ECC_Visibility` 碰撞体完成；行为矩阵可用仅开发期 LOS Hook 保持确定性。现有 15% retention 测试必须保留并运行，作为正交回归证据。

### 证据门禁

| 门禁 | 所有者 | 需要的证据 |
| --- | --- | --- |
| 静态回查 | Gemini | 仅批准文件的最终 diff、`git diff --check`、隔离自审；不能表述为编译或运行时验证。 |
| 手动编译 | 用户 | `PolyQuestEditor (Development Editor)` 的实际结果。 |
| Automation | 用户 | `PolyQuest.Player.LockOn` 全部 Success。 |
| Editor readback | 用户 | `BP_PlayerCharacter` 的两个默认值；Scene01 墙/柱对 `Visibility` 为 Block；非活动 `AFloorVolume` 管理 Actor 隐藏后碰撞仍保留。 |
| Scene01 PIE | 用户 | 隔墙不可初始锁定；短暂绕柱不抖动；持续遮挡约 2.5 秒后清锁；处决镜头遮挡不中断；Bow Target Assist、15% retention 与 HUD 无回归。 |

若 Scene01 没有既有可复现的 `Visibility` 掩体，不得为本阶段新建或修改资产；记录该 PIE 门禁为待用户另行授权的资产范围决定。

## 5. 停止条件与 Gemini 交付要求

- 任何额外 Source 文件、Config、Build.cs、Tag、`.uasset`、`.umap` 或资产碰撞修改需求出现时立即停止并回报。
- 不得改变 Bow、15% retention、See-Through、投射物、攻击/伤害、GAS 或处决的既有契约。
- Gemini 交付必须包含：仅三份批准文件的 diff；静态检查结果；Automation 新增/保留用例说明；对 Trace 上限、隐藏 Actor 重试、`HitActor == nullptr`、同目标计时不刷新、弱引用清理和处决豁免边界的实施自审；留给用户的编译、Editor、Automation 和 PIE 门禁。
- 不得执行 Git Commit、`git add -A`、破坏性命令或 Editor/资产写入。任何 Git 提交均等待用户明确批准。

## 6. 已确认验证与证据边界

- 用户确认 `PolyQuestEditor (Development Editor)` 手动编译通过，`PolyQuest.Player.LockOn` Focused Automation 全部 `Success`，Scene01 PIE 通过，且锁定默认值符合最终约定。
- 本记录不把上述 PIE 结果扩展为逐个 Scene01 掩体或 `AFloorVolume` 字段的独立 Editor readback；本阶段没有资产改动，也不据此声明新的资产基线。
- 最终 `LockOnOcclusionGraceDuration` 为 `2.5f` 秒：原始 `1.5f` 秒短于主角约 `3s` 的体力衰竭回复窗口，用户基于实测节奏确认后完成同一切片的窄调优。该值是 Lock-On 私有默认值，不是 ROADMAP 全局常量。

## 7. 收口记录（2026-09-07）

- **实际交付**：仅批准的 `PlayerCharacter.h`、`PlayerCharacter.cpp` 与 `PlayerLockOnAutomationTests.cpp` 实现 Camera-to-Target LOS 门禁、隐藏 Actor 重试、8 次总追踪上限的 fail-closed 策略，以及已持有目标的连续遮挡宽限。
- **稳定契约**：获取、循环与死亡重锁共用严格 LOS 候选门禁；清晰 LOS、显式清锁、真实目标切换和弱引用失效清零计时；同目标重复设置不能刷新宽限。既有 15% 屏幕 retention、Bow 6% Target Assist 与现有死亡/GAS 有效性规则保持独立；成对处决仅豁免新增 LOS/宽限路径。
- **Main Fresh Review**：已完成一轮两批有界审查，未发现 P0/P1/P2；`1.5s -> 2.5s` 的窄增量也已按最终源码、测试阈值与 fail-closed 超时路径复核，未发现阻塞问题。
- **提交边界**：候选提交只包含上述三份 Source/Test、`ARCHITECTURE.md`、`ROADMAP.md`、`ROADMAP-archive.md` 与本记录；全部 `Content/**`、Config、Blueprint、地图、插件和其他用户 WIP 明确排除。`plan.md` 保留为最近阶段的详细交接与验证凭据，直到下一份正式计划被接受。
