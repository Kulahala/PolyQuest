# TODO-03H10：StanceBreak 胸口红点标识

> 最近完成切片交接：03H10 已完成原生、Automation、Editor 资产、Development Editor 编译、Scene01 PIE/视觉验收与 Main Fresh Review，并于 2026-09-14 获得文档收口和精确提交授权。下一规划入口为 TODO-03H11。

## 1. 目标、基线与执行路线

目标：敌人处于真实 StanceBreak 硬直时，在胸口附近显示独立红点；恢复、进入处决锁定、待死亡、死亡或销毁时隐藏。标记只表达敌人破韧状态，不表达玩家当前是否满足处决按键条件。

- 仓库：`E:\GameDevelop\PolyQuest`
- 启动 HEAD：`25f251692834d59d9218a3dd98431d184a2c6baa`
- 引擎基准：`D:\UE\UE_5.8`
- 03H9 固定收据：`ROADMAP-archive.md` 的“TODO-03H9：A/B/C 合并资产 readback 与总验收关闭”条目。
- 启动时四个批准源码/测试文件与阶段文档均无未提交改动；现有 Content/Config 删除、修改及其他 WIP 全部保留并排除。

```text
Outer: ue-stage-workflow
Primary: ue5-ui-umg-slate
Support: ponytail:ponytail (lite)
Route reason: 独立 UWidgetComponent、GAS Tag 事件绑定与 UMG 资产交接
Planning-turn executors: 0
Implementation executor: one in-app gpt-5.6-luna / xhigh sub-agent
Execution route: Luna 仅实施四个批准 C++/测试文件；Main 负责文档、集成、静态检查、Fresh Review 与提交门禁
Main: 用户授权后通过同一 live Unreal MCP endpoint 串行完成 Editor 资产写入、编译、保存与 readback
User: Development Editor 编译、Automation、Scene01 PIE、最终提交批准
```

实施代理不得启动 Editor、写资产、改文档、扩大路径或提交。当前收口提交由用户明确授权，Main 只精确暂存批准清单且不推送。

## 2. 冻结运行时契约

唯一显示谓词：

```text
State.Status.Stunned
&& !State.Action.Execution.VictimLocked
&& !State.Status.DeathPending
&& !State.Status.Dead
&& !bDeathTeardownStarted
```

- ASC Gameplay Tag 是唯一状态权威；不读取 Poise、Montage 时间、Notify、锁定目标或 UI 自有布尔副本。
- 复用上述四个现有 Tag，不新增 Tag、Config、GameplayEffect 或 Ability。
- `VictimLocked` 抑制处决接管期间的红点；其解除后若 `Stunned` 仍存在且无死亡条件，红点恢复。
- `Stunned` 从有到无且无 VictimLocked、DeathPending、Dead 或 teardown 时，红点保持组件可见并在 `0.15s` 内线性淡出，完成后隐藏并恢复满 Alpha 供下次显示。
- VictimLocked、DeathPending、Dead、死亡拆卸、解绑或销毁均取消进行中的淡出并瞬间隐藏；激活失败时保持隐藏。
- 重新获得 Stunned 时取消淡出、恢复满 Alpha 并立即显示；ASC 仍是唯一状态权威，不另写 Ability 回调。
- 不增加玩家距离、角度、武器、输入、锁定目标、LOS、遮挡或交互资格判断。
- 头顶血条继续使用现有 `160x20` Draw Size、Widget、Lock-On 与 AutoFade，03H10 不修改或复用其生命周期。

## 3. Approved Paths

### 3.1 Luna 原生与测试白名单

- `Source/PolyQuest/Public/Character/Enemy/EnemyCharacter.h`
- `Source/PolyQuest/Private/Character/Enemy/EnemyCharacter.cpp`
- `Source/PolyQuest/Private/Tests/VitalHudAutomationTests.cpp`
- `Source/PolyQuest/Private/Tests/EnemyStanceBreakRateWindowAutomationTests.cpp`

只允许修改以下符号或测试段：

- `AEnemyCharacter` 构造、`BeginPlay()`、`EndPlay()`、`HandleDeath()`。
- 新组件、四 Tag 事件的绑定/解绑/刷新/隐藏方法、委托句柄、弱 ASC 与 `VictimLockedStateTag`。
- `WITH_DEV_AUTOMATION_TESTS` 下的最小组件 getter、绑定状态和 bind/unbind/refresh 触发器。
- `PolyQuest.UI.VitalHUD` 中敌人 WidgetComponent 与状态矩阵测试段。
- `PolyQuest.Combat.EnemyStanceBreakRateWindow` 中真实 StanceBreak 生命周期的标记断言。

### 3.2 Main 文档路径

- `plan.md`
- `ROADMAP.md`
- `ARCHITECTURE.md`
- `ROADMAP-archive.md`

用户门禁与 Fresh Review 已完成；四份文档仅记录稳定事实、完成收据和下一路线，不新增债务或扩展运行时范围。

### 3.3 用户授权的 Editor 写入路径

- `Content/_UI/HUD/Combat/WBP_StanceBreakMarker.uasset`
- `Content/_UI/HUD/Combat/Materials/T_StanceBreakMarker.uasset`
- `Content/BP/Characters/Enemy/BP_Enemy_Base.uasset`

只读确认：

- `Content/_Abilities/Enemy/GA_EnemyStanceBreak.uasset`
- `Content/_Abilities/Enemy/GA_EnemyVictimExecution.uasset`

任何其他源码、测试、Config、`.Build.cs`、`.uasset` 或 `.umap` 都不在本片写入范围。

## 4. 原生实现

在 `AEnemyCharacter` 新增 `StanceBreakMarkerWidgetComponent` 默认子对象：

- `UWidgetComponent`，附着 `GetMesh()` 的 `spine_03` 胸口骨骼；该骨骼已在 `BP_Enemy_Base` 当前 Goblin Warrior Skeletal Mesh 上通过 live Editor readback 确认存在。
- `EWidgetSpace::Screen`，Draw Size `20x20`，Pivot `(0.5, 0.5)`。
- Socket 相对位置默认 `(0, 0, 0)`；派生 Blueprint 直接使用继承组件的原生 Parent Socket 字段在 `spine_02` / `spine_03` 间选择，并按模型做小幅 Socket-local 偏移。
- `NoCollision`、`GenerateOverlapEvents=false`、默认 `SetVisibility(false)`。
- 使用现有 `VisibleAnywhere, BlueprintReadOnly, Category="UI|Enemy", AllowPrivateAccess` 风格暴露给派生 Blueprint 配置，不新增生产 getter。
- 不新增 `StanceBreakMarkerSocketName` 平行配置或 Actor Tick；`USceneComponent` 原生附着更新通过 Mesh 的 `GetSocketTransform(AttachSocketName)` 跟随实时骨骼姿态。

新增独立生命周期，不扩展血条绑定：

- `BindStanceBreakMarkerEvents()`：取得当前 ASC；同一 ASC 已绑定时只刷新，ASC 改变时先解绑旧对象；为 Stunned、VictimLocked、DeathPending、Dead 注册 `NewOrRemoved` 事件，保存四个句柄，最后立即刷新。
- `UnbindStanceBreakMarkerEvents()`：只从记录的弱 ASC 移除有效句柄；重置四个句柄和弱引用；主动隐藏。
- `OnStanceBreakMarkerRelevantTagChanged(...)`：仅 `Stunned` 的 `NewCount == 0` 且无抑制条件时启动淡出；其他变化统一刷新。
- `RefreshStanceBreakMarker()`：一次读取绑定 ASC 和冻结谓词；显示时取消淡出、恢复满 Alpha，抑制状态下立即隐藏。
- `BeginStanceBreakMarkerFadeOut()` / `UpdateStanceBreakMarkerFadeOut()`：复用 `FTimerManager`；一次性 Completion Timer 固定在 `0.15s` 隐藏，短间隔 Update Timer 只按 Completion Remaining Time 更新内部 `UUserWidget::RenderOpacity`。完成时长不依赖帧率或更新回调次数；不增加 Tick、Widget 动画或新 Widget 类。
- `HideStanceBreakMarker()`：取消淡出 Timer、恢复满 Alpha，并立即 `SetVisibility(false, true)`。

生命周期接线：

- 构造函数请求 `State.Action.Execution.VictimLocked`。
- `BeginPlay()` 在 `Super::BeginPlay()` 前按现有 Dead/DeathPending 模式补齐 Stunned、VictimLocked、DeathPending、Dead 无效 Tag；在既有死亡/UI 绑定后绑定标记并完成初始刷新。
- `EndPlay()` 设置 `bDeathTeardownStarted` 后立即解绑并隐藏，避免 teardown 中 Tag 变化重新显示。
- `HandleDeath()` 入口显式隐藏，作为死亡终态兜底。

测试接口沿用现有风格并只存在于 `WITH_DEV_AUTOMATION_TESTS`：

```text
GetTestStanceBreakMarkerWidgetComponent()
HasBoundStanceBreakMarkerDelegates()
TriggerTestBindStanceBreakMarkerEvents()
TriggerTestUnbindStanceBreakMarkerEvents()
TriggerTestRefreshStanceBreakMarker()
```

不新增 `UUserWidget` C++ 子类；仅获取组件现有的 UserWidget 实例写入 `RenderOpacity`，不增加 Actor Tick、Widget 动画或通用标记管理器。

## 5. 自动化测试

### 5.1 PolyQuest.UI.VitalHUD

- CDO：新组件存在、Screen、NoCollision、无 Overlap、Pivot `0.5/0.5`、Socket-local 位置 `0/0/0`、Draw Size `20x20`、默认隐藏。
- 原血条仍断言 Screen、Pivot `0.5/1.0`、位置 `0/0/130`、Draw Size `160x20`；既有 Lock-On/AutoFade 测试不改语义。
- 绑定后初始无 Tag 时隐藏；Stunned 添加后显示，移除后启动自然恢复淡出。
- Stunned 自然移除后先进入淡出：中途仍可见且 Alpha 位于 `(0,1)`，`0.15s` 后隐藏并复位 Alpha；淡出中重新添加 Stunned 时立即恢复满 Alpha。
- Stunned 存在时分别添加 VictimLocked、DeathPending、Dead 均隐藏；逐一移除后在其余负向条件为空时恢复。
- 淡出中添加 VictimLocked、DeathPending 或 Dead 时立即隐藏并取消 Timer；解绑同样立即收敛。
- 解绑立即隐藏，旧 ASC 后续 Tag 变化不再影响组件；重绑后读取 ASC 当前状态。
- 两个 Enemy fixture 的状态变化互不影响。

### 5.2 PolyQuest.Combat.EnemyStanceBreakRateWindow

- 复用现有真实 Ability/Montage fixture，不复制世界或动画推进辅助。
- StanceBreak 激活成功并拥有 Stunned 后显示；启动失败收敛后隐藏。
- 普通 Cancel 断言启动自然恢复淡出；由于该 fixture 的 HitStop 使用游戏时间膨胀，完成态通过测试入口收敛，`0.15s` 中间值和真实完成时长由无 HitStop 的 VitalHUD 测试独占覆盖。UnPossess teardown 立即隐藏。
- VictimLocked 接管时隐藏；若接管撤销而原 StanceBreak 仍有效则恢复。
- 连续激活/结束不残留可见状态或旧 ASC 委托。

不得删除、跳过或弱化既有 RateWindow、Task、Montage、Poise、移动或清理断言。若现有 fixture 无法稳定观察某一条，应保留核心状态矩阵在 VitalHUD，并报告真实覆盖缺口，不新增公共 fixture。

## 6. Editor 资产写入与 readback

`WBP_StanceBreakMarker`（本轮由 Main 经用户授权的 live Unreal MCP 写入并在后续视觉调整中改用纹理 Brush）：

- 普通 `UUserWidget`，根尺寸 `20x20`，中央单个 `20x20` Image 使用 `/Game/_UI/HUD/Combat/Materials/T_StanceBreakMarker` 纹理 Brush。
- 不新增材质、动画、Tick、输入或玩法蓝图逻辑。
- Widget 及子项不参与命中测试。

`BP_Enemy_Base`（仅局部配置继承组件，不重建或替换资产）：

- 为 `StanceBreakMarkerWidgetComponent` 设置 `WBP_StanceBreakMarker` Widget Class。
- 组件 Attach Parent 必须为 `CharacterMesh0`，Parent Socket 默认 `spine_03`；需要更低胸口位置时可改为当前骨架已确认存在的 `spine_02`。
- 只微调 Socket-local 相对位置；禁止重新挂回 Root/胶囊体，不修改头顶血条组件。

最终 readback 确认 Widget 树、`20x20` viewport/Image、纹理 Brush 引用、无逻辑/动画、Widget Class、Screen Space、Attach Parent=`CharacterMesh0`、Parent Socket=`spine_03`、零 Socket-local 偏移和默认隐藏；两份 GA 仍使用预期的 Stunned/VictimLocked 所有权且未被修改。

## 7. 验证、停止与完成条件

Main 静态门禁：

- 核对最终四文件 diff、直接生命周期和 Tag 名称。
- 适用时检查 Rider error-level 诊断；始终运行限定路径 `git diff --check`。
- 使用 `ue-strict-review` 对批准 diff 做一次 Main Fresh Review；无未关闭 P0-P2 才可进入文档收口。

用户门禁：

- `PolyQuestEditor (Development Editor)` 编译成功。
- `PolyQuest.UI.VitalHUD` 与 `PolyQuest.Combat.EnemyStanceBreakRateWindow` 成功。
- Scene01 PIE：正常隐藏、真实破韧显示、后倾 Montage 期间跟随胸口骨骼、自然恢复、处决接管、死亡、连续破韧、多敌人独立、未锁定时仍显示。
- 视觉确认自然恢复为约 `0.15s` 平滑淡出；处决接管、DeathPending/Dead 与死亡拆卸仍无尾影、瞬间熄灭。
- 视觉确认：完整后倾/恢复动作中红点不悬空脱节，不同镜头距离下尺寸可读、位置在胸口附近，头顶血条位置、裁切、AutoFade 和 Lock-On 无回归。

同一根因最多一次修复加一次定向复验；再次失败时保留首次证据并停止该回路。资产缺失不阻塞原生静态交付，但阶段不能关闭。用户门禁、Fresh Review、稳定文档收口和明确提交批准全部齐全后，03H10 才完成。

Ponytail lite：直接复用 Mesh 骨骼附着和 Blueprint Parent Socket；不增加独立 Socket 配置字段、跟随 Tick 或通用标记系统。

## 8. 原生、Automation 与资产收据（2026-09-14）

- 按用户指定派发唯一 `gpt-5.6-luna / xhigh` 执行代理；实际修改严格限于第 3.1 节四个文件，未写资产、Config、Build.cs 或其他源码，未提交。
- 已实现独立组件、四 Tag 事件绑定、绑定 ASC 隔离、初始刷新、解绑/死亡/EndPlay 隐藏，以及 VitalHUD 状态矩阵与真实 StanceBreak 生命周期断言；头顶血条实现与测试契约保持原样。
- Main 首轮静态检查发现测试误用不存在的 `UWidgetComponent::GetVisibility()`、解绑刷新读取当前 ASC，以及启动失败缺 marker 断言；同一 Luna 完成一次窄修，统一使用 `GetVisibleFlag()`、刷新只读已绑定弱 ASC，并补齐失败/解绑回归。
- Main 对最终 diff、UE 5.8 `USceneComponent` 可见性接口、Tag 生命周期和批准范围做了用户门禁前静态回读；限定六文件 `git diff --check` 通过，仅有 Git 的 LF→CRLF 工作区提示。`code-review-graph` 基线与当前 HEAD 一致，提供的测试缺口不识别本次 UE Automation 宏，故以实际测试源码回退核对；图谱不计编译或运行时证据。
- 用户确认 Test Run 3：`PolyQuest.Combat.EnemyStanceBreakRateWindow` 与 `PolyQuest.UI.VitalHUD` 均为 `Success`。这是 Automation 运行证据，不替代独立的 Development Editor 编译、资产 readback 或 PIE 视觉证明。
- 用户授权后，Main 通过同一 live Unreal MCP endpoint 创建并保存 `/Game/_UI/HUD/Combat/WBP_StanceBreakMarker`：普通 `UUserWidget`，`RootCanvas -> StanceBreakDot`，组件 viewport 为 `20x20`，圆点 Canvas Slot 居中且为 `12x12`，纯红 `RoundedBox`、半径 `6`、无纹理/材质资源；根与 Image 均为 `HitTestInvisible`，Animation 列表为空，EventGraph 节点数为 `0`。Widget Blueprint 编译返回成功，20x20 离屏预览 readback 成功。
- Main 仅在现有 `/Game/BP/Characters/Enemy/BP_Enemy_Base` 的继承 `StanceBreakMarkerWidgetComponent` 上设置上述 Widget Class；Blueprint 编译、保存成功。写后 readback 为 Screen Space、Draw Size `20x20`、Pivot `(0.5,0.5)`、相对位置 `(0,0,90)`、默认不可见、无 Overlap、NoCollision；未修改 `EnemyHealthBarWidgetComponent`。
- 两份只读 GA 仍分别生成 `EnemyStanceBreakAbility` 与 `EnemyVictimExecutionAbility`；StanceBreak Montage 为 `/Game/BP/Montages/AM_StanceBreak`，Victim 的 Front/Backstab Montage 均为 `/Game/BP/Montages/LightSword/AM_LightSword_BeExecuted`。本轮未修改两份 GA；Tag 所有权继续来自已核对的原生类，并由上述真实 GAS Automation 覆盖。
- 当前尚无独立的 Development Editor C++ 编译、Scene01 PIE/视觉验收或正式收口 Fresh Review。03H10 仍是开放阶段，不构成关闭或可提交证明。

## 9. 后倾姿态错位修复收据（2026-09-14）

- 用户首次 PIE 明确复现：StanceBreak Montage 大幅后倾时，挂在 Root/胶囊体的红点保持绝对高度，与身体脱节；此前 Automation Success 与静态组件 readback 未覆盖该视觉问题。
- live Unreal MCP 写前 readback：`StanceBreakMarkerWidgetComponent` 的实际 Parent 为 `CollisionCylinder`、Socket=None、相对位置 `(0,0,90)`；`BP_Enemy_Base` 使用的 `/Game/PolygonDungeons/Meshes/Characters/SK_Character_Goblin_Warrior_Male` 同时存在 `spine_02` 与 `spine_03`。
- 窄修改为 `SetupAttachment(GetMesh(), "spine_03")` 且 Socket-local 偏移归零；`PolyQuest.UI.VitalHUD` 新增 Attach Parent、Socket 与零偏移断言。UE 5.8 原生 `USceneComponent` 更新链按 Parent `GetSocketTransform(AttachSocketName)` 计算组件世界变换，因此无需 UI Tick。
- 本轮只修改既有批准路径 `EnemyCharacter.cpp`、`VitalHudAutomationTests.cpp` 及阶段文档；未修改 Widget、GA、Montage、Config、Build.cs 或其他资产，未提交。Rider 当前索引仍未识别本片尚未编译的 `EnemyCharacter.h` 新声明，故其成组 unresolved 结果不作为本次新增行的有效诊断；限定 diff whitespace 检查通过。
- 上述源修改尚未取得 Development Editor C++ 编译、修复后两组 Automation、`BP_Enemy_Base` 新 Attach Parent/Parent Socket readback、Scene01 后倾视觉复验或 Fresh Review。此前 Test Run 3 与 Root 附着 readback 是修复前证据，不能替代本轮门禁。

## 10. 自然恢复淡出追加收据（2026-09-14）

- 用户确认删除并重新放置场景敌人后，Mesh `spine_03` 骨骼跟随正常；该问题属于关卡中旧 Actor 实例保留旧组件模板，不再增加运行时重复 Attach。
- 同一 Luna xhigh 执行代理在原四个源码/测试白名单内追加 `0.15s` Timer 驱动的 WidgetComponent Tint Alpha 淡出：无抑制状态的 Stunned 移除启动淡出；重新 Stunned 恢复满 Alpha；VictimLocked、DeathPending、Dead、UnPossess、解绑与 teardown 立即取消并隐藏。
- VitalHUD 覆盖淡出中间态、完成、重新进入、三类抑制和解绑；RateWindow 将普通 Cancel 更新为淡出契约，同时保留 VictimLocked/UnPossess 瞬间隐藏断言。未修改 Widget/纹理/Blueprint/GA/Montage/Config 或 Build.cs。
- Main 限定六文件 `git diff --check` 通过，Rider 对四个 C++/测试文件返回零 error。新代码尚未完成 Development Editor 编译、两组 Automation、Scene01 PIE 淡出视觉验收，因此正式 Fresh Review 与阶段关闭仍待这些门禁后执行。
- 用户首次追加复验中 RateWindow 为 Success，VitalHUD 暴露两处测试问题：测试 World 的 Timer 回调读取 `World->GetTimeSeconds()` 时未观察到当前推进，且多敌人隔离段仍保留旧“Stunned 移除立即隐藏”断言。Main 按同一根因的一次窄修改为每次 Timer 回调累计固定步长，并将隔离段更新为主敌人独立淡出后隐藏；`spine_03` 日志仍是无 SkeletalMesh 的 headless fixture warning。修复后 Automation 尚待一次定向复验。
- 第二次追加复验中 RateWindow 的两个完成断言暴露精确边界假设：`0.01s` 循环 Timer 不保证在测试 World 恰好推进 `0.15s` 时已调度第 15 次回调。运行时未改；三个 marker 完成点统一推进 `0.16s` 越过一个 Timer 步长，同时保留 VitalHUD `0.05s` 中间 Alpha 断言。修复后 Automation 尚待定向复验。
- 第三次复验确认最新 DLL 已加载，但 RateWindow 在 `0.16s` 后仍未完成，证明按 Update Timer 回调次数累计既不适配测试 World 的粗粒度 Tick，也会在真实低帧率下拉长淡出。Main 将根实现收敛为一次性 Completion Timer 负责 `0.15s` 终点，Update Timer 仅从 Completion Remaining Time 采样 Alpha；修复后 Automation 尚待定向复验。
- 用户冻结淡出使用游戏时间。RateWindow fixture 的 HitStop 会把世界时间膨胀降至约 `0.03`，因此该测试不再重复验证 0.15 秒时长：它只验证真实 GAS Cancel 启动淡出，再经 `WITH_DEV_AUTOMATION_TESTS` 完成入口验证隐藏和 Timer 清理。无 HitStop 的 VitalHUD 继续独占中间 Alpha 与 0.15 秒完成时长覆盖；生产 Timer 实现不因测试改动。
- 后续复验中 RateWindow 为 Success；VitalHUD 的 Alpha=`0.266667` 证明手动测试 World 在两次 Tick 之间创建的 Timer 首次 Tick 只从 `PendingTimerSet` 激活，不消耗 FirstDelay。UE 5.8 `FTimerManager::InternalSetTimer` 源码确认该语义。VitalHUD 三个真实计时段在启动 Fade 后先零 Delta Tick 激活 Pending Timer，再推进 0.05/0.16 秒；生产代码不变，修复后待定向复验。
- 用户确认最新复验中 RateWindow 与 VitalHUD 先后成功，但 Scene01 视觉仍为硬切。UE 5.8 源码确认 Screen Space `UWidgetComponent` 不创建 `MaterialInstance`，原 `SetTintColorAndOpacity()` 因此不影响实际 Slate/UMG 输出。Main 将唯一透明度落点改为内部 `UUserWidget::SetRenderOpacity()`，保留 `0.15s` 游戏时间与所有瞬时抑制契约；VitalHUD 注入最小 UserWidget 并直接断言中途 RenderOpacity 和结束复位。Rider 对三份改动源码/测试返回零 error，限定四文件 `git diff --check` 通过；本轮修复后的编译、Automation 与 PIE 视觉复验仍待用户执行。
- 用户确认修复后 Scene01 PIE 通过，证明生产 Widget 的自然恢复淡出可见，胸口跟随与瞬时抑制路径无观察到的回归。同期附件显示 RateWindow 为 Success，但 VitalHUD 因测试夹具直接实例化 UE 5.8 抽象基类 `UUserWidget` 触发 ensure 而 Fail；Main 复用同文件既有的具体 `UEnemyHealthBarWidget` headless fixture 修复测试，不改生产逻辑。该测试修复后的 VitalHUD 定向复验仍待执行，因此暂不进入正式 Fresh Review 或阶段关闭。
- 用户确认测试夹具修复后的 `PolyQuest.UI.VitalHUD` 为 Success；结合此前 `PolyQuest.Combat.EnemyStanceBreakRateWindow` Success 与 Scene01 PIE 通过，本片 Automation 和运行时/视觉门禁已闭合。Main Fresh Review 在 `25f2516` 启动基线和四个批准源码/测试文件内未发现 P0-P2；只校正上述两处 P3 级计划表述漂移。Ponytail 辅助审查未发现值得删除的生产复杂度；双 Timer 分别承担平滑采样和精确完成，保留合理。

## 11. 阶段关闭与提交收据（2026-09-14）

- 用户明确确认 Scene01 PIE 基于当前代码编译并通过：红点跟随 `spine_03` 胸口骨骼，自然恢复的 `0.15s` 淡出可见，处决接管与死亡保持瞬间熄灭；这同时完成 Development Editor 编译和 PIE/视觉门禁。
- 用户确认最终 `PolyQuest.Combat.EnemyStanceBreakRateWindow` 与 `PolyQuest.UI.VitalHUD` 均为 `Success`。测试日志中的 `spine_03: No SkeletalMesh` 仅来自 transient headless Enemy fixture，没有造成测试失败或生产资产回归。
- Main Fresh Review 未发现 P0-P2；Rider 对四个批准 C++/测试文件为零 error，限定 diff whitespace 检查通过。Ponytail review 结论为实现已足够精简，不新增通用标记框架、Actor Tick、Widget 动画或平行 Socket 配置。
- `ARCHITECTURE.md` 记录稳定运行时事实，`ROADMAP.md` 移除已完成 03H10 并将下一入口推进到 03H11，`ROADMAP-archive.md` 保存固定完成收据；`plan.md` 保留为最近完成交接。没有新增验证债务。
- 用户授权精确提交以下 11 个文件：四个原生/测试文件、三个 StanceBreak Marker 资产和四份阶段文档。全部无关 Content/Config/地图/动画 WIP 保留并排除；三个二进制资产按 Git LFS pointer 核验，不推送。
