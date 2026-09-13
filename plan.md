# TODO-03H7：玩家大硬直 Dodge 取消窗口与 RateWindow

## 1. 目标、基线与职责

**状态（2026-09-13）：实现与 Main Fresh Review 收口，用户已批准文档归档和有界提交；修复后验证收据缺口按 ROADMAP 的 Debt-03H7-PostRepairValidation 保留。第 1–7 节保留批准时的计划，最新完成态以第 9 节为准。**

让玩家四向大硬直正确消费既有Dodge Cancel Window和RateWindow：窗口外不能闪避，窗口内通过既有Dodge检查与消耗后取消大硬直，反应结束时不残留取消Tag或调速状态。

- 工作区：E:\GameDevelop\PolyQuest；引擎基准：D:\UE\UE_5.8。
- 规划基线：22cd208dfaeec48ea89f89c6c804e482bcd2fe66。落盘前Source与plan.md无未提交差异；已有ROADMAP、Content、Config等WIP保留，不归因于本次。
- 上一片TEST-SHRINK完成态已存在于ROADMAP-archive.md的2026-09-13 Main Implementation Closeout；完整交接固定读取git show 22cd208dfaeec48ea89f89c6c804e482bcd2fe66:plan.md。无需重复归档。历史文档当时的下一入口03C保留为历史；当前按最新ROADMAP执行TEST-SHRINK → 03H7 → 03H8 → 03C。
- Outer：ue-stage-workflow；Primary：ue5-cpp-gameplay；Support：ponytail:ponytail lite。
- Route reason：Native玩家反应Ability接入现有GAS取消目标与Montage窗口设施，无共享架构调整。
- Execution route：manual/out-of-band Gemini；Implementation executors：1（用户手动交接）；本轮实际派发：0。
- Main负责计划、范围、Fresh Review、文档与提交；Gemini负责批准源码/测试和实施自审；用户负责资产制作、Development Editor编译、Automation、Editor readback、Scene01 PIE与最终提交批准。

规划证据为真实AGENTS、源码、配置、Git及文档静态核对；CodeGraph返回不足的函数区段已聚焦读取。未执行编译、Automation或live Editor/PIE，不把图谱或静态检查作为运行时证明。

## 2. Deliberate Non-goals与修订决策

不改敌人、不做敌人排障，不改Small/Launch行为，不扩展攻击/Guard/Parry取消权限，不改Dodge或PlayerCharacter输入路由。不新增Tag、作者配置字段、Blueprint可调用接口、模块依赖、Build.cs、外部库、全局时间膨胀、网络机制或通用取消框架。不重构共享RateWindow，不追加测试瘦身或修改共享fixture。

采纳Gemini建议：取消窗口直接AddDynamic绑定Ability，不新增取消Context或独立代次；仅RateWindow保留微型Context与binding token，承担现有模板绑定及旧回调隔离。新测试聚焦Big Reaction接入，不复制共享RateWindow的完整排列组合。

取消监听的跨激活保证是旧任务结束并解除订阅、新激活重新建立监听；不声称具有RateWindow的token隔离。共享取消Notify载荷没有历史激活编号，本片不扩展载荷，也不承诺识别重新构造并投递给新监听器的同资产旧事件。

Ponytail lite最省方案：复用Ability.Action.CancelableBy.Dodge和FMontageRateWindowBinding，不修改共享Dodge，不新建取消框架。

## 3. Approved Paths

Gemini仅可修改以下四个文件及限定符号；同文件不等于允许修改其他功能。

| 绝对路径 | 允许修改范围 |
|---|---|
| E:\GameDevelop\PolyQuest\Source\PolyQuest\Public\AbilitySystem\Abilities\PlayerBigHitReactionAbility.h | 取消任务/回调/状态、RateWindow瞬态Context与绑定成员、必要测试访问点；四向Montage属性原名保持 |
| E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\AbilitySystem\Abilities\PlayerBigHitReactionAbility.cpp | 构造、ActivateAbility、EndAbility、ValidateActivationSetup、窗口授权/绑定/清理 |
| E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\Tests\HitReactionAutomationTests.cpp | 仅PlayerBig的十项阻止、十一项打断及Dodge取消标记断言 |
| E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\Tests\PlayerMontageRateWindowAutomationTests.cpp | 新增PolyQuest.Combat.PlayerBigHitReactionWindows，复用本文件可播放Montage和局部支撑，保留既有测试行为 |

不为四向反应扩展现有单Montage泛型消费者，不新增测试框架。测试访问点仅在WITH_DEV_AUTOMATION_TESTS内增加，且必须有直接断言消费者。

Main文档白名单：E:\GameDevelop\PolyQuest\plan.md、E:\GameDevelop\PolyQuest\ROADMAP.md、E:\GameDevelop\PolyQuest\ROADMAP-archive.md、E:\GameDevelop\PolyQuest\ARCHITECTURE.md。Gemini不得修改这些文档。规划时用户要求仅写plan.md；现已批准其他文档在收口时按职责更新，ARCHITECTURE只记录验证和终审后的稳定事实。

## 4. Runtime Contracts与实施顺序

### 4.1 Dodge取消闭环

1. Big Reaction的AbilityTags增加现有Ability.Action.CancelableBy.Dodge，复用UDodgeAbility在Commit成功后的取消目标。
2. BlockAbilitiesWithTag仅排除Ability.Dodge，其余十项保持；AbilitiesToCancel保持十一项，仍能打断先前Dodge。同步修正ValidateActivationSetup集合数量检查及CDO断言。
3. 创建精确匹配Event.Action.CancelWindow.Dodge.Begin/End的WaitGameplayEvent任务，EventReceived直接AddDynamic(this, ...)到本Ability。
4. SetDodgeCancelable以bDodgeCancelable幂等管理自身一份State.Action.CanCancel.Dodge，使用AddLooseGameplayTag/RemoveLooseGameplayTag；不清空其他贡献者，不添加防御取消Tag。
5. 回调检查IsActive、bEndAbilityRequested、Avatar/AnimInstance/ActiveMontage有效及宿主未销毁、当前Montage活动、精确事件类型、自身Instigator/Target；OptionalObject只能是ActiveMontage或其直接引用的Sequence。
6. 窗口外由既有HitReacting + CanCancel.Dodge判定阻止Dodge；每次激活初始关闭自身取消状态，不继承旧监听任务。

用户已确认成功边界：精力不足、阻止状态、前置配置或Commit失败，原大硬直继续；Commit成功后、自身Montage启动阶段异常沿用现有Dodge行为，不增加动作回滚。输入维持短按释放时请求Dodge，不能把按下时刻当作激活时刻。

### 4.2 启动与RateWindow绑定

1. 激活入口清除上次窗口资源，重置结束标志和取消状态，执行原有四向选择及前置检查。
2. 创建取消监听并直接绑定Ability，在Montage启动前准备监听；每个ReadyForActivation返回后检查任务有效性、Ability Active及结束标志。
3. 保留原Commit与Montage启动关系。MontageTask->ReadyForActivation返回后先检查同步结束，再确认BoundAnimInstance与ActiveMontage有效且Montage_IsActive。
4. 仅在上述确认后调用BindRateWindow，复用FMontageRateWindowBinding和FAbilityMontageRateWindowLifecycle；不能前置，因为helper要求真实活动Montage实例。绑定失败沿用helper fail-closed，返回，不继续修改移动状态。
5. 保持原停止速度、悬崖设置、MovementMode委托及打断旧动作的相对顺序。每个同步重入边界后不得恢复旧任务/状态。

RateWindow保留SourceAnimation/Notify身份、binding token、当前实例ID、窗口优先级和捕获基准恢复契约。窗口值仍直接设置Montage播放速率，不改成乘以基准速率。不修改共享设施或放宽实例校验。

### 4.3 EndAbility单一出口

1. 幂等检查后立即置bEndAbilityRequested=true。
2. SetDodgeCancelable(false)。
3. ClearRateWindow：失效Context、结束RateWindow任务；仅仍持有原Montage实例时恢复捕获速率，然后清除绑定状态。
4. EndTask并清空取消窗口监听。
5. 执行原Montage/移动委托解绑、Montage停止、悬崖设置恢复和Super::EndAbility。

自然结束、取消、死亡、Falling、启动失败统一走该出口。Tag/速率清理必须先于Montage_Stop；同步结束后不还原旧状态。保持ASC权属、InstancedPerActor、ServerOnly、接地要求、四向属性、Root Motion位移与移动恢复策略。

## 5. 用户资产清单与Readback

Agent无资产写入权限。只读核实/Game/_Abilities/Player/HitReaction/GA_PlayerBigHitReaction的原生父类、继承Ability/Block Tags及四向引用。

用户已接受按需补齐窗口的四个现有Montage：

- /Game/BP/Montages/Hit/AM_BigHitReaction_F
- /Game/BP/Montages/Hit/AM_BigHitReaction_B
- /Game/BP/Montages/Hit/AM_BigHitReaction_L
- /Game/BP/Montages/Hit/AM_BigHitReaction_R

上述资产目前仅确认文件存在，尚无live Editor内容证明。优先保留已放置窗口和现有手感参数；缺失时由用户补齐并提供readback。记录方向、完整实际引用、Slot/Sequence、Dodge Cancel Window与Montage RateWindow区间及速率数值。

实际引用超出清单，或需要修改GA默认值、底层Sequence、AnimBP时，暂停该资产步骤并确认新范围；不得自行扩围、手工编辑二进制或猜测关系。

## 6. 验证矩阵

新增专项使用真实ASC授予/激活、可播放Montage及原生Notify → ASC → Ability路径。不以直接调用Activate或绕过实例校验代替集成证明。

| 验证组 | 最小必要覆盖 |
|---|---|
| CDO | 十项阻止、十一项打断、Dodge取消标记正确；其他权限不变 |
| 四向接入 | 四方向选中正确Montage，各自窗口可使真实Dodge成功取消反应 |
| 取消边界 | 窗口外拒绝、窗口内成功；精力不足、阻止状态、前置配置/Commit失败保留反应；重复Begin/End无Tag泄漏 |
| RateWindow接入 | 选中Montage的Begin改速、End恢复；反应中断清理绑定/速率；保留一个旧RateWindow Context回调隔离用例 |
| 生命周期 | 自然结束、取消、死亡、Falling、启动失败后无自身取消Tag/任务残留；重新激活初始关闭，旧取消任务已结束 |
| 真实派发 | 至少一条实际推进Montage时间触发Notify的用例；其他边界可调用原生Notify补充 |

共享RateWindow重叠/乱序、非法载荷及完整实例授权矩阵沿用已有测试，不在Big Reaction专项复制。新增取消回调自身的角色/来源拒绝仍需窄断言，不因共享设施已测试而跳过新代码。

- Gemini静态：四文件范围、Tag和生命周期契约、适用且可用的Rider诊断、git diff --check；诊断不可用记录coverage fallback，不擅自探测无关端点。
- 用户编译：UE 5.8 Development Editor、PolyQuestEditor，保留结果收据；Agent未经明确委托不代跑UBT/UAT/Editor编译。
- 用户Automation：PolyQuest.Combat.PlayerBigHitReactionWindows；PolyQuest.Combat.HitReaction；PolyQuest.Combat.PlayerMontageRateWindow全组；PolyQuest.Combat.PlayerLaunchReactionRootMotion。
- 用户Editor/Scene01 PIE：四向引用和窗口readback；窗口内外短按释放Dodge；失败保留反应；速率与自然结束/取消/死亡/Falling恢复；攻击/防御权限不变。
- Main Fresh Review：按ue-strict-review审查批准diff，重点检查取消闭环、清理次序、接入测试和证据边界。

## 7. Executor交付与完成态

1. 先读真实AGENTS.md和本plan，核对工作区、基线、四文件差异；只探索目标与必要直接依赖，证据充分即实现，不全库漫游。
2. 按四文件白名单连续完成局部实现与必要测试；不得修改Dodge、共享RateWindow、Main文档、资产、Config、Build.cs或清单外Source。需要越界或改变运行时契约时停止报告，不自行扩大范围。
3. 首次交付按项目规则优先使用一个干净只读子代理严格实施自审，禁止递归派发；后续微修单兵复核，不再派发。Main另行承担终审。
4. 交付四文件diff摘要、静态检查、严格实施自审记录、证据来源与未执行门禁。显式核验回调对象/状态有效性、ReadyForActivation重入及单一EndAbility出口。不把静态结果说成编译或PIE通过。
5. 同一根因最多一次有据修复和一次针对性重跑；再次失败或两轮连续修复失败时保留首个错误并停止，不重复跑未变化命令。
6. 不暂存、不提交、不推送，不自动进入03C，不代用户运行编译/Automation/Editor/PIE，除非用户另行明确委托。

适用门禁全部通过后才关闭03H7；真实延期风险在ROADMAP保留唯一记录与关闭条件。既有H6债务不随本片关闭，也不增加无依据前置。Main维护完成态、归档及稳定架构事实；随后独立规划03C。最终Git Commit须用户明确批准，只暂存批准路径并保留无关WIP。

## 8. Main Fresh Review定向修复（2026-09-13）

- 用户确认本阶段编译、PIE、Editor readback通过；此前专项Automation成功日志已核实。上述收据属于本轮生产修复前版本，不追认为修复后验证。
- 用户批准将上一轮单测试文件修复扩至必要生产文件。本轮使用Main狭窄修复例外，仅修改PlayerBigHitReactionAbility.cpp的OnActiveMontageEnded和PlayerMontageRateWindowAutomationTests.cpp的Big Reaction专项；不改头文件、共享设施、资产、Config或Build.cs，不派发子代理，不提交。
- 结束广播仅携带Montage资产；同资产仍有运行实例时忽略旧结束广播，避免取消后立即重激活被旧广播结束。保留原单一EndAbility清理出口。
- 专项删除激活前停止/冲刷旧Montage的逻辑，以及自动Notify/自然结束失败后的手工Notify与Broadcast兜底。使用UE公开TickMontageOnly与DispatchQueuedAnimEvents推进真实播放/派发；新增取消后排队旧结束事件、同资产立即重激活、再派发旧事件的实例ID及活动状态断言。
- 非收口静态复核：Rider两份修改文件errors为空，git diff --check通过。初次静态检查发现Montage_UpdateWeight/Montage_Advance为不可访问成员，已一次修正为公开入口并复查通过。未编译、未运行Automation或PIE；本轮没有修复后Success收据。
- 待用户门禁：Development Editor编译；PolyQuest.Combat.PlayerBigHitReactionWindows及PlayerMontageRateWindow全组、HitReaction、PlayerLaunchReactionRootMotion；Scene01四向受击取消/连续受击与自然结束回归。资产未改，既有readback继续有效。通过后进行有界delta复核，尚不宣布03H7关闭。
- ExecutionReleaseOutcomes和VitalHUD日志失败按用户要求留待独立安排，本次不排障、不修改其代码，不将其称为已证实的历史问题或本轮已修复项。

### 第8.1节测试前置修复

用户回传专项Fail：8.1a/b/c失败，期望活动窗口及0.5速率，实际速率1.0。源码确认第7节仅结束RateWindow，Big Reaction仍Active；第8.1节重发事件被HitReacting阻止，而helper仅凭旧实例IsActive误报激活成功。Main在该测试文件内显式取消第7节Ability并断言结束，helper同时要求HandleGameplayEvent返回激活数大于0。未恢复队列冲刷、手工Notify或自然结束广播兜底；生产回调未再修改。此根因本轮一次修复后，用户回传专项 Success，随后 Main 有界 delta Fresh Review 通过。日志中的空Montage启动失败Warning属于第8.5节既有负向场景，不作为本次8.1失败根因。

## 9. Main Closeout（2026-09-13）

- **交付与终审**：四份批准 Source/Test 完成玩家四向 Dodge 取消及 RateWindow 接入。两轮严格审查及定向修复复核后，无未关闭 P0–P2；P3 四向测试 getter 简化建议不阻塞、不自动扩展修改。
- **修复结果**：同资产旧 Montage 结束广播不再提前结束当前仍运行的新实例；测试以真实 TickMontageOnly/DispatchQueuedAnimEvents 验证 Notify、自然结束和立即重激活，不用播放失败后的手工事件兜底。取消来源与幂等断言区分 Closed/Open；真实 Commit 失败与 CanActivate 拒绝分开验证。
- **证据归属**：用户最新回传 PolyQuest.Combat.PlayerBigHitReactionWindows 为 Success。更早日志已确认 PlayerMontageRateWindow 六组（Bow、Charged、Dodge、Light、MeleeSkill、Sprint）、HitReaction、PlayerLaunchReactionRootMotion 成功；用户曾确认编译、Editor readback 和 PIE 通过，但这些完整收据早于最终生产回调修复。Main 实际执行适用 Rider 诊断及 git diff --check，通过；Main 未代跑编译、Automation 或 PIE。
- **验收边界**：用户知悉修复后完整收据缺口后授权收尾提交；实现/终审收口不代表所有运行时门禁在最终版本重新执行。唯一延期记录和关闭触发见 ROADMAP 的 Debt-03H7-PostRepairValidation；资产未改，先前 readback 仍有效。
- **日志归口**：空 Montage 启动失败输出属于专项 8.5 负向场景。ExecutionReleaseOutcomes、VitalHUD 的真实失败统一进入独立 TODO-03H8，不认定为已证实的历史问题或本片回归；本次不修改其代码。
- **归档与提交**：ROADMAP-archive.md 保存本片摘要，ARCHITECTURE.md 更新稳定契约；提交精确包含第 3 节四份 Source/Test 与四份 Main 文档。Content、Config 及其他 WIP 保留并排除，不推送。下一入口为独立制定 TODO-03H8 计划，随后 TODO-03C；本 plan 保留完成态至下一片替换。
