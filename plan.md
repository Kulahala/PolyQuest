# TODO-03H6-FIX2：投射物命中几何与防御方向修复

## 1. 目标、基线与职责

**状态（2026-09-13）：TODO-03H6-FIX2 完成实现、验证与 Fresh Review，H6-F02 关闭。Gemini 的编译及11/11 Automation收据已核实；Main补测并隔离死亡来源fixture后，用户手动回传 ImpactGeometry Success，其余10项回归沿用未受影响的成功收据。Main最终差异审查通过，无未关闭P0–P2。PIE按用户豁免记录。用户已明确授权文档收尾与提交，本片按15份Source/Test及4份文档封闭提交。**

修复 H6-F02：投射物的 Guard、防御弧、受击及死亡方向使用命中瞬间飞行方向的反向；射手在飞行期间移动不改变判定，伤害继续由原来源 ASC 结算。

- 工作区：E:\GameDevelop\PolyQuest；引擎：D:\UE\UE_5.8。
- 基线 HEAD：f2fea459b6ff8e7cd3cdae3ea45c4b8079ffd36d，FIX1 已提交。落盘前 Source 和阶段文档无未提交差异；既有 Content、Config、tmp WIP 保留并排除，不纳入本片。
- FIX1 完整凭据固定读取：git show f2fea459b6ff8e7cd3cdae3ea45c4b8079ffd36d:plan.md；其收尾已在 ROADMAP-archive.md 归档。FULL-AUDIT 证据固定为 git show 851c7ac:plan.md，不重新全库审计。
- Outer：ue-stage-workflow；Primary：ue5-cpp-gameplay；Support：none。
- Route reason：既有 Native Projectile / Guard / GAS Context / Reaction 的单次命中方向接缝修复，不改变战斗权属。
- Execution route：manual/out-of-band Gemini；Implementation executors：1，由用户手动交接，本轮不自动派发。
- Main：架构、计划、范围、Fresh Review、文档及提交；Gemini：批准路径内源码、测试、实施自审和问题排查；用户：编译、Automation、Editor/PIE 与最终提交批准。

### 用户决定与 Gemini 建议裁决

- 用户已选择命中瞬间飞行方向的反向，追踪弹采用转弯后的实际方向。
- 用户已选择退化几何时伤害照常、Guard 不吸收、方向返回零；不回退射手位置。
- 采纳 Gemini 的 UE 反射识别、显式基类序列化、提前采样速度与真实碰撞前置检查。
- MovementComponent 无效时直接零方向，不增加 GetVelocity 后备。原生 Player/Enemy 构造函数已配置阵营，测试断言实际值即可；300cm、3000cm/s 是测试设置，不将“2～3 次 Tick 必命中”作为前置假设。

### Deliberate Non-goals

不实现远程敌人，不修改追踪算法、生产碰撞配置、资产、RateWindow/SHRINK、阵营分派、Tag、Config、Build.cs 或 GAS 权属。不铺设网络复制、Iris 或全局 Context 分配器。无新增 Tick、轨迹历史、通用 fixture 框架或生产测试开关。

## 2. Runtime Contracts 与接口

### 2.1 投射物方向采样

FCombatProjectileHitRequest 新增 FVector WorldIncomingDirection = FVector::ZeroVector，语义为世界空间中从目标指向来袭侧的水平单位向量。

ACombatProjectile::HandlePawnImpact 通过现有有效性与初始化检查后，立即读取有效 MovementComponent 的 Velocity，在调用命中 resolver、忽略目标、停止移动或尾迹清理前完成采样。来向为速度反向投影到 XY 后归一化，追踪弹采用转弯后的速度。检查原始速度 XYZ 有限性；零速度、水平分量近零、NaN/Inf 或组件失效均归零。保留原始 HitResult 的接触点、法线和目标信息。

合法敌对命中方向为零时，伤害照常进入 GAS，Guard 不吸收，受击/死亡方向返回零并沿用既有无方向处理。不得回退射手位置、初始发射方向或 ImpactNormal。请求默认零值表示明确的退化来向，不表示使用旧位置规则。

### 2.2 按值保存 GAS 命中几何

新增窄用途 USTRUCT FCombatImpactEffectContext : FGameplayEffectContext，仅增加 WorldIncomingDirection。

- 从 SourceASC->MakeEffectContext() 复制原生来源信息，写入校验后的来向和原始 HitResult。保留 Instigator、EffectCauser、来源 ASC、SourceObject 及其他原生上下文信息；SourceObject 沿用 Request.SourceObject 有值时优先，否则 SourceActor 的规则。
- Context 类型表示显式方向存在，即使字段为零也不能执行普通来源位置后备。方向按值保存，后续消费不依赖射手或投射物仍然存活。
- 实现 GetScriptStruct()、Duplicate()、NetSerialize() 及必要 traits。Duplicate 保留派生类型与方向，并深拷贝 HitResult。
- NetSerialize 显式调用 FGameplayEffectContext::NetSerialize(...)，再序列化方向；分别处理函数返回值、bOutSuccess 与归档错误，不用无条件成功掩盖失败。仅实现原生 Context 协议，不配置复制或 Iris，不宣称网络验证。
- 不替换全局 Context 分配器；若真实来源 Context 已有本计划未覆盖的派生状态，停止相关实现并向 Main 报告，不能静默切片丢失数据。

FHitReactionImpactResolver::ResolveImpactDirectionFromContext 的类型判断必须走 UE 反射：

1. 取得 ContextHandle.Get()，检查原始指针及 GetScriptStruct()。
2. 通过 IsChildOf(FCombatImpactEffectContext::StaticStruct()) 后才能 static_cast。
3. 禁止 dynamic_cast、未经类型检查的强转和通过 SourceObject 猜测类别。
4. 派生 Context 使用保存的方向，再转换到目标局部水平坐标；无效或零方向直接返回零。
5. 普通 Context 保持 Instigator 优先、ImpactNormal 后备；近战与处决原契约不变。

### 2.3 Guard 接缝与生命周期

下列三个现有 Native 接口追加末尾可选参数 const FVector* WorldIncomingDirection = nullptr：

- APlayerCharacter::TryResolveIncomingDefense
- APlayerCharacter::TryGuardIncomingMeleeHit
- UPlayerGuardAbility::TryGuardMeleeHit

参数仅在同步调用中读取，不保存指针。nullptr 保持原近战行为；非空参数使用显式方向，零值或非有限值拒绝 Guard，不回退射手位置。内部 IsAttackerInGuardArc 适配显式方向，保持目标命中时朝向、现有 GuardHalfArcDegrees 和 Dot >= Cos(HalfArc) 边界。普通调用方不必改动；不增加 Blueprint 暴露。

- 投射物入口固定 bAllowParry=false：允许 Guard、禁止 Parry。
- Guard 消耗、反馈、体力恢复延迟、GuardBreak 和 EndAbility 清理保持现有流程；恰好耗尽或超过剩余体力的本次格挡仍吸收命中。
- 来源失效/死亡、目标死亡/无敌、同阵营、缺少 ASC/GE 继续拒绝。方向有效不能绕过来源和目标资格校验。
- 成功命中只投递一次，继续使用现有终止尾迹与清理出口；不改变 Ability 激活、取消、委托或 ASC 权属。

## 3. Approved Paths 与符号白名单

以下为封闭清单。既有文件仅修改指定符号、必要 include 与接口注释，不因同文件在白名单内扩大到其他方法。

| 批准绝对路径 | 符号 / 允许修改范围 |
|---|---|
| E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\Combat\Projectile\CombatProjectile.cpp | ACombatProjectile::HandlePawnImpact 的来向采样与请求填充 |
| E:\GameDevelop\PolyQuest\Source\PolyQuest\Public\Combat\Projectile\CombatProjectileHitResolver.h | FCombatProjectileHitRequest::WorldIncomingDirection 及契约注释 |
| E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\Combat\Projectile\CombatProjectileHitResolver.cpp | FCombatProjectileHitResolver::TryResolveHit 的方向校验、Guard 转发、Context 构造；必要文件内私有数学辅助 |
| E:\GameDevelop\PolyQuest\Source\PolyQuest\Public\Character\Player\PlayerCharacter.h | TryResolveIncomingDefense、TryGuardIncomingMeleeHit 声明与注释 |
| E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\Character\Player\PlayerCharacter.cpp | TryResolveIncomingDefense、TryGuardIncomingMeleeHit 参数转发 |
| E:\GameDevelop\PolyQuest\Source\PolyQuest\Public\AbilitySystem\Abilities\PlayerGuardAbility.h | TryGuardMeleeHit、IsAttackerInGuardArc 声明与注释 |
| E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\AbilitySystem\Abilities\PlayerGuardAbility.cpp | TryGuardMeleeHit、IsAttackerInGuardArc 的方向接缝；不改消耗、反馈或清理策略 |
| E:\GameDevelop\PolyQuest\Source\PolyQuest\Public\Combat\Reaction\HitReactionImpactResolver.h | ResolveImpactDirection / ResolveImpactDirectionFromContext 契约注释 |
| E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\Combat\Reaction\HitReactionImpactResolver.cpp | ResolveImpactDirectionFromContext 的显式方向分支，必要文件内私有数学辅助 |
| E:\GameDevelop\PolyQuest\Source\PolyQuest\Public\Combat\CombatImpactEffectContext.h | 新增 FCombatImpactEffectContext 与必要 traits，generated.h 置于 include 末尾 |
| E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\Combat\CombatImpactEffectContext.cpp | 新增 Context 构造、类型、复制与序列化实现 |
| E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\Tests\ProjectileImpactGeometryAutomationTests.cpp | 新增本片专项及必要文件内 fixture 辅助 |
| E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\Tests\PlayerDefenseAudioAutomationTests.cpp | 仅为现有投射物 Guard 用例补充显式来向，保留原断言 |

复用既有 FCombatAutomationFixture 和测试 GE，不修改共享 fixture，不增加生产测试开关。除以下补充授权外，其余测试仅运行回归。

补充授权（首次交付后由用户转交 Gemini 执行；原始清单仍为13路径）：

| 批准绝对路径 | 符号 / 允许修改范围 |
|---|---|
| E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\Tests\FrontExecutionAutomationTests.cpp | RunTest 内局部地面 fixture、Player 对该地面的碰撞忽略、release / blocked cancel 的真实 FindFloor 前置断言；保留 Walking 断言 |
| E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\Tests\BackstabExecutionAutomationTests.cpp | 同上，不改生产移动逻辑或共享 fixture |

Main 文档白名单（Gemini 不得写入）：

- E:\GameDevelop\PolyQuest\plan.md：本片交接、门禁与结果。
- E:\GameDevelop\PolyQuest\ROADMAP-archive.md：核对已有 FIX1 归档，追加 FIX1 → FIX2 指针及最终收口，避免重复归档。
- E:\GameDevelop\PolyQuest\ROADMAP.md：活跃指针、H6-F02 唯一风险与关闭状态。
- E:\GameDevelop\PolyQuest\ARCHITECTURE.md：仅验证与终审通过后更新稳定契约，本轮规划落盘不改。

## 4. 测试与验证矩阵

新增专项：PolyQuest.Projectile.ImpactGeometry。

| 场景 | 核心断言 |
|---|---|
| 射手移动 | 横移/绕后不改变相同来向的 Guard 与反应方向；Instigator、EffectCauser、SourceObject 和来源 ASC 仍正确 |
| 目标转向 | 改变目标朝向后，局部方向和防御弧相应变化；覆盖弧内、边界、弧外 |
| 转弯后命中 | 当前速度与初始方向、射手位置、ImpactNormal 冲突时，以当前速度为准 |
| 退化方向 | 零、纯竖直、NaN、Inf 不触发 Guard；合法伤害生效、方向为零，无位置后备 |
| 防御与体力 | 充足、恰好耗尽、超过剩余体力、初始零体力；当前 GuardBreak 命中仍吸收；Parry 激活也不能弹反投射物 |
| 资格拒绝 | 来源 Actor/ASC 失效、来源 Dead、目标 Dead/Invulnerable、友军继续拒绝 |
| Context | 实际 GE 捕获正确派生类型、来源和方向；复制后 HitResult 独立；Actor 销毁后已存方向仍可解析；原生序列化往返保留方向 |
| 命中链路 | 至少一例真实 UWorld 移动碰撞进入 GAS；补充 sweep/非 sweep 和重复回调用例，断言只投递一次 |
| 受击与死亡 | Player/Enemy 受击来向正确；Enemy 致死冲量沿远离来袭侧方向；零方向不产生伪造定向冲量 |
| 近战/处决兼容 | 普通 Context 的 Instigator 与 ImpactNormal 冲突时，仍以 Instigator 为先 |

### 真实碰撞测试前置

- 创建并初始化独立 Game World，完成 BeginPlay；复用现有 fixture，保留初始化顺序。
- 断言 Sphere/Capsule 已注册、参与查询、生成重叠事件、通道响应兼容，ProjectileMovement 已激活且绑定正确 UpdatedComponent。测试所需配置只作用于 transient fixture，不改生产预设。
- 断言生产 resolver 实际读取 Team.Player 与 Team.Enemy，不用生产分派 bypass 或新增阵营 setter 修测试。
- 初始采用约 300cm 距离、3000cm/s 速度，避免初始重叠；目标保持稳定，排除角色下落等无关运动干扰。
- 使用现有 World Tick 与 GFrameCounter 推进惯例，以 0.05s 步长、最多 10 步等待实际命中。未命中报告前置状态与失败，不无限推进。
- 断言实际位移、伤害/消费和 Context 捕获；手工 Broadcast 单独标注为回调消费证据，不能替代真实碰撞。

### 既有回归入口

- PolyQuest.Projectile.Lifecycle
- PolyQuest.Projectile.TargetAssist
- PolyQuest.Projectile.FlightTrail
- PolyQuest.Combat.DefenseAudio
- PolyQuest.Combat.ParrySuccessFeedback
- PolyQuest.Combat.HitReaction
- PolyQuest.Enemy.DeathRagdoll
- PolyQuest.Combat.FrontExecution
- PolyQuest.Combat.Backstab
- PolyQuest.Combat.ExecutionImpactFeedback

### 门禁与证据所有者

| 门禁 | 要求 | 当前状态 / 所有者 |
|---|---|---|
| 静态与实施自审 | 白名单 diff、git diff --check、适用且可用的 Rider 诊断、严格实施自审 | Main 完成批准15路径的两轮静态审查及窄补测差异核对；diff check 成功。Rider 未执行，静态覆盖回退到源码、UE 5.8 API 和图谱；报告未附干净子代理自审记录，不冒充独立自审 |
| 编译 | UE 5.8 PolyQuestEditor / Development Editor 成功 | Gemini 的 Development Editor 成功收据已核对；后续测试补丁按用户在重新编译/复跑请求后回传的专项 Success 验收，未另附一次全量重编收据；Main 未运行编译 |
| Automation | 新增专项与上述既有回归成功 | Gemini 修复后11/11 Success已核对；Main补测的8.3曾失败，隔离死亡来源fixture后用户最新手动专项Success。其余10项保留既有成功收据，不声称新版全套重跑 |
| PIE | 原计划验证 Bow 命中/死亡方向、尾迹终止，以及近战 Guard、GuardBreak、Parry、前处决和背刺 | 用户本轮明确接受本片豁免；未提供实际 PIE 成功回执，不计为已执行或已通过。后续验证归 ROADMAP 的 Debt-03H6-FIX2-PIE |
| Fresh Review | Main 按 ue-strict-review 审查批准 diff，无未关闭 P0–P2 | 通过：两轮静态审查及用户最新Success后的限定delta review完成；P2测试覆盖与fixture污染已关闭，无新增P0–P2 |

敌对投射物命中 Player 的完整几何矩阵由 Native Automation 覆盖，不提前实现远程敌人。本片没有 Editor 资产写入、迁移或资产 readback 门禁。模拟 Guard 活跃状态只证明命中消费，不证明 Montage 激活；手工回调不证明真实碰撞；Context 序列化测试不证明网络运行。

本片默认修复与回归一并交付，不增加独立等待旧版 RED 回执的暂停门禁。未实际运行的修复前失败不得声称已验证。相同失败根因最多一次有依据修复和一次定向复跑，不削弱断言换绿灯。

初次交接未授权自动运行；后续两份处决测试修复提示词已明确委托 Gemini 编译及运行11项 Automation，用户已转交执行结果。Main 本轮不自动运行编译、Automation、live Editor 或 PIE；新增补测的编译与定向复跑继续由用户/Gemini 执行。查询 live Editor 前应用 unreal-mcp。

## 5. Executor 交付、停止条件与文档收口

### Gemini 自包含交接

- 首先读取 E:\GameDevelop\PolyQuest\AGENTS.md 与本计划，核对实际 HEAD、Source diff 和既有 WIP；仅探索目标、直接依赖与必要测试，不重新 FULL-AUDIT。
- 在第3节封闭路径与符号内完成源码和专项测试，常规算法/私有辅助/测试细节自主闭环，不因局部选择频繁请示。
- 首次交付按项目规则优先使用一个干净只读子代理严格实施自审；后续微调或 Bug 修复单兵复核，不重复派发。自审不冒充 Main Fresh Review。
- 交付精确路径清单、接口变化、关键断言、实际检查收据、自审 Findings/结论与剩余用户门禁。显式核验回调对象/状态有效性、同步 GAS 回调边界和单次命中/统一清理；不改变既有 ReadyForActivation 或 EndAbility 时序。
- 不写文档、资产、Tag、Config、Build.cs、全局工具或清单外文件；不暂存、提交、推送或回滚 WIP。

### 停止条件

发现必须扩大路径、改变生命周期/来源权属、来源 Context 基线不符，或真实碰撞前置无法成立时，停止相关扩展并报告具体证据。按第4节失败重试上限执行，不能以放宽契约、手工 Broadcast 替换真实碰撞或弱化断言消除失败。

### 完成与债务归口

- 当前规划证据：实时源码、定向 CodeGraph、图谱未覆盖区段的聚焦测试读取及本机 UE 5.8 API。全部为静态证据，不是编译、Automation、Editor/PIE、视觉或网络证明。
- 编译、新增专项/既有回归、适用 PIE 和 Main Fresh Review 通过后，关闭 H6-F02，更新 ARCHITECTURE 稳定事实，并由 Main 完成三份阶段文档收尾。
- 未通过门禁或接受的延期风险只在 ROADMAP 保留唯一记录和闭环触发条件；不静默将证据不足写成通过。
- Debt-03H6-ProjectileIntegration 的方向相关覆盖由 FIX2 关闭；剩余发射/飞行到 Dead、Ragdoll、Destroy、SourceASC 失效和重复碰撞终止的完整集成矩阵仍按 ROADMAP 归 03C，不能因新增部分用例关闭整个既有债务。
- FIX1 已关闭及其非阻塞 Debt-03H6-FIX1-PreFixProof 保持原归属，不继承其历史 RED 门禁为本片暂停条件。
- FIX2 完成后进入已排期的有限 SHRINK，不直接放行 TODO-03C。用户现已明确批准本片收尾后提交，仅暂存最终批准的精确路径。
## 6. 首次交付接收与用户 PIE 豁免（历史记录，2026-09-13）

- 用户本轮请求以 ue-strict-review 为主、ponytail-review 为辅进行验收，并表示尚无远程敌人、PIE 难以完成，接受本片免于实测并要求文档记录。此授权按 PIE 验证豁免处理，不伪造 PIE 成功记录；未授权提交，也不自动豁免报告中失败的 Automation。
- 接收报告：C:\Users\Administrator\.codex\attachments\ce19acad-bd19-4f46-8c8c-6ff0feabd877\pasted-text.txt。报告与 Git 路径清单显示 10 个既有 Source 文件修改、3 个新增文件，对应第3节13路径；这只是入口范围核对，不代表源码正确性或测试覆盖已审查。
- Gemini 执行、用户转交的编译结果：PolyQuestEditor Win64 Development Succeeded，增量5.48s。Automation 明列 Success 的9项：ImpactGeometry、Lifecycle、TargetAssist、FlightTrail、DefenseAudio、ParrySuccessFeedback、HitReaction、DeathRagdoll、ExecutionImpactFeedback（完整前缀见第4节）。Main 未重复执行，也不把报告中的“8个Section完整覆盖”当作源码复核结论。
- 报告同时列明 FrontExecution 与 Backstab 失败：release 步期望 MOVE_Walking，实际 MOVE_Falling（3）。Gemini 归因为无地面的 transient world fixture，但未提供同环境修复前基线对照。当前只能记录“执行者归因、尚未排除回归”，不能写成已证实既有问题或计划内全量通过。两文件不在本片写入白名单，不擅自修复或放宽断言。
- ue-strict-review 的入口要求验证门禁完成，缺门禁时非收口静态审查需用户明确指定。本轮停在入口核对，未进入源码/图谱 Fresh Review、对抗审查或 ponytail 复杂度判断；不输出无 Findings 或可提交结论。后续取得两项回归成功/明确验收裁决，或用户明确请求先只读静态审查时继续。
- PIE 豁免的唯一后续验证记录为 ROADMAP 的 Debt-03H6-FIX2-PIE；H6-F02 的自动化与审查仍开放。ARCHITECTURE 和完成态归档本轮不更新，FIX2 不关闭，03C 不放行。现有源码及用户WIP保留。

## 7. 处决测试修复复核与 Main 窄补测（过程记录，最终状态见第8节）

- 接收修复报告：C:\Users\Administrator\.codex\attachments\7e26dede-9fca-40bd-b929-1d14de133282\pasted-text.txt。两份局部地面以 Enemy 实际 Capsule 底部定位；Player 只忽略该测试地面，原 Snap 几何断言、释放/取消的 Walking 断言均保留，并增加 FindFloor/IsWalkableFloor 前置证据。可接受为受害者释放验证，不把它当作 Player 接地物理验证；无生产修复。
- 收据所有者：Gemini 执行、用户转交、Main 读取核实。UBT 日志 C:\Users\Administrator\AppData\Local\UnrealBuildTool\Log.txt:161 为 Result: Succeeded；Saved/Logs/PolyQuest.log:2348–2588 含计划内11项 Success。覆盖原来的 FrontExecution/Backstab 两项失败；这些是 Main 补测前的证据，不声称修复前基线已复现。
- Review 范围：基线 f2fea459b6ff8e7cd3cdae3ea45c4b8079ffd36d 的15个批准 Source 路径及必要直接依赖。一次 code-review-graph 变更雷达辅助导航；源码、UE 5.8 API 和必要测试为实证，图谱不作为运行时证明。未派发应用内子代理。
- 第一轮缺陷审查：未发现生产逻辑 bug；P2 为专项测试覆盖缺口：实际碰撞只断言扣血，手工构造 Context 无法验证 Actor 的方向赋值与真实 GE 接线；原“Actor destruction”在销毁无关 Actor 前解析，不构成来源销毁后的快照证明。
- 第二轮对抗审查：派生 Context 保留来源 ASC 的伤害权属、方向按值且显式零无后备、默认 nullptr 保留普通近战/处决行为、单次投递与清理出口不变，均有当前源码支持。删除 Actor 方向赋值仍可能通过原碰撞测试，故 P2 不能凭已有11/11辩护消除。未发现另一个可复现生产缺陷。
- Main 依据 AGENTS 的单文件窄改动例外，结束只读审查后只修改原批准 ProjectileImpactGeometryAutomationTests.cpp：真实发射初始+Y、当前速度改+X、射手移至侧方；从实际 Health GE 回调捕获 Context 深拷贝，断言类型、来源、HitResult 目标、-X 来向与局部方向；保留真实 Tick 与非致死重复碰撞断言。回调句柄在推进结束后移除，不留下栈引用。
- 补测还将实际捕获的 Context 复用于致死 GE，断言沿+X远离来袭侧的死亡冲量；此段是 GAS 消费测试，不宣称第二次真实飞行。销毁真实投射物和射手后再解析快照，删除旧的伪销毁覆盖。ponytail 辅助仅删除同文件三个无用声明/赋值，不扩展重构 Context 或共享 fixture。
- 当前验证：Main 仅完成修改后的静态差异、UE API 与空白检查，其他 Source 文件内容哈希不变。待 Development Editor 编译及 PolyQuest.Projectile.ImpactGeometry 定向复跑；若修复引起生产/共享代码变化，才扩大到受影响回归。现有其余10项成功收据继续有效；不无理由重复全套。
- H6-F02 的唯一开放状态和闭环条件见 ROADMAP；PIE 延期仍仅归 Debt-03H6-FIX2-PIE。暂不更新 ARCHITECTURE 的稳定事实或追加完成态归档，不提交。

### Test Run 3 失败修复（用户手动运行）

- 用户提供 ImpactGeometry Fail：8.3 的致死GE使Health由20归零，但Dead仍为false、死亡冲量/捕获次数均为0。此前11/11不覆盖此新增断言。
- 根因证据：第5.2节曾给共用Enemy添加Dead Tag，触发HandleDeath设置bDeathTeardownStarted；移除Tag和恢复Health不会复活Actor。OnHealthAttributeChanged遇到该终态标记直接返回。Main补测时漏查此前用例的终态污染，本次修复测试前置，不修改生产死亡契约。
- 单文件修复：第5.2节改用独立SpawnPassiveEnemy作为死亡来源，完成拒绝断言后销毁；第8节增加死亡清理消费次数为0的前置断言。移除排查期间的两条Error级DIAG日志，保留8.3全部行为断言。Instant GE的ActiveHandle无效本身不代表应用失败。
- 本次为该根因的一次修复，静态差异及空白检查通过；由用户重新编译并定向复跑PolyQuest.Projectile.ImpactGeometry。若同根因仍失败，保留结果并报告，不继续盲目试改。Guard fixture提示和World Cleanup缺EndPlay日志不作为本次8.3失败根因；未扩展共享fixture或生产生命周期。
## 8. 最终验收与文档收尾（2026-09-13）

- **最新用户回执**：死亡来源隔离修复后，用户手动运行PolyQuest.Projectile.ImpactGeometry，Result=Success；回执保留Guard恢复延迟/GuardBreak fixture提示与World Cleanup缺EndPlay日志。该成功关闭新增8.0/8.3/8.4及实际Context断言的门禁，不抹去第7节首次失败。
- **Fresh Review通过**：只复核补测及死亡来源隔离差异，其他Source内容与先前审查基线哈希一致。独立DeadSource不再污染共用Enemy；实际Context捕获使用同步Health委托，推进后移除句柄，SourceObject在投递时取证以适应投射物终止销毁。原扣血/重复回调、来源与方向、死亡冲量和销毁后断言全部保留。对抗检查未发现依赖复活已终态Actor或放宽断言的路径；无未关闭P0–P2，ponytail无新增必要删改。
- **有界证据**：本次delta是单文件测试隔离，没有新的共享调用/生命周期疑问，图查询skipped；沿用此前有界雷达和已读取的UE源码契约，不重跑生产审查或派发子代理。Rider未执行，不把静态或图谱证明称为运行时证明。
- **验证组合**：Gemini的Development Editor编译与11项Automation收据，加用户最后专项Success；后续只改该测试，其他10项回归保留。Main仅执行静态、文档与Git检查，未编译/运行Automation/操作Editor。没有另一次最新版全量Automation、PIE、视觉、网络或打包证明。
- **日志边界**：Guard测试人工激活且未配置完整恢复延迟/GuardBreak资产，其日志不证明生产激活或表现；World Cleanup缺EndPlay不影响本次命中/死亡断言的Success，但不作为清理生命周期证明。非阻塞测试维护仅归ROADMAP的REC-03H6-AdditionalShrink，首次维护对应fixture时补正确EndPlay和清理回归，不扩大本片。
- **收口**：H6-F02关闭，ARCHITECTURE更新当前显式几何与来源契约，ROADMAP移除已关闭风险并指向有限SHRINK，ROADMAP-archive只追加本片结果。PIE豁免及剩余ProjectileIntegration矩阵各保留唯一原归属；03C仍须有限SHRINK完成。
- **Git授权与范围**：用户明确表示“fresh review完成就可以开始文档收尾然后提交”。仅15份Source/Test和4份阶段文档，共19路径；2,122项既有Content/Config/tmp WIP排除，历史归档前缀保留。先检查精确暂存清单及cached diff --check，再执行本地提交；不推送。