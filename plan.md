# TODO-03H6：Combat Architecture And Code Health Audit

## 1. Target Objective、基线与角色

在 TODO-03C 远程敌人前完成有界、只读、缺陷优先的战斗架构审计，交付 Findings、清理候选、接入建议和准入结论。用户于 2026-09-12 批准执行修订计划；本文件现为活跃交接，非规划模式草稿。

- 项目：E:\GameDevelop\PolyQuest；UE 5.8，引擎源码基准 D:\UE\UE_5.8。
- 审计基线：759efcad28f0d04e36e9b74ed07f88d8ec60ed9d；启动时 HEAD 匹配，Source、阶段文档及 ARCHITECTURE 无未提交差异。
- 既有 WIP：Git 短状态中 Content 321 项、Config 1 项、tmp 1 项；全部保留，数量只作启动快照，不是本轮修改清单。
- 前阶段 C 已归档；完整旧计划和各版收据固定由 git show 759efca:plan.md 追溯。归档已追加 C 提交确认与 H6 交接，不重复复制。
- Outer: ue-stage-workflow；Primary: ue-strict-review；Support: none。
- Execution route: Main read-only audit；Implementation executors: 0。用户在执行中明确授权 Main 同时负责计划和审计执行，并允许派发一个干净的只读子代理做 Fresh Review；子代理只审核审计稿与具体证据，不修复、不写文档、不递归派发。独立源码修复后续走 manual/out-of-band Gemini，不在本轮实施。
- 状态：只读审计、独立 Fresh Review 与文档交付完成，用户已批准仅三份文档提交；03C准入与后续修复门禁见第7节。

Deliberate Non-goals：改源码/测试/公开 API/Tag/Input/Config/Build.cs/资产；运行编译、Automation、Editor/PIE；直接清理或重构；创建公共 Ability 基类或 Module；实现远程敌人、Player 死亡或通用施法系统；清零历史债务；暂存、Git 提交或推送。

## 2. Approved Paths 与只读范围

Main 写入白名单严格为三份文档：
- plan.md：H6 契约、覆盖记录、Findings、候选及接入/准入结论。
- ROADMAP.md：当前阶段指针、H6 状态、实际修复依赖和唯一 Known Risks 条目。
- ROADMAP-archive.md：append-only 交接追溯与 H6 收口记录。

ARCHITECTURE.md、Source、测试、Config、Content、tmp 及其他路径只读。没有任何生产/测试符号写入授权；Findings 中的最小修复路径只是后续计划候选。

| 组 | 固定入口与读取范围 | 必查契约 |
|---|---|---|
| 1 Ability/helper | Player/Enemy 战斗 Ability、RateWindow helper/Context、StanceBreak 基准、Motion-Warp helper、AbilityTask、战斗 Notify | Commit/启动时序、ReadyForActivation 重入、实例/代次、取消和单一清理出口；11 套新增 Context 的必要隔离与规则漂移 |
| 2 Character/ASC/反应 | Base/Player/Enemy Character、AttributeSet、Reaction resolver、处决入口及一跳调用 | ASC/属性权属、受击/死亡、委托/计时器、UnPossess/Destroy/EndPlay；与投射物联合检查来源失效及方向 |
| 3 Controller/StateTree | EnemyAIController、Enemy StateTree Tasks/Conditions、AIProfile | 固定 Ability Tag、EngagementRange/AttackRange/导航容差、pending profile、冷却、目标/LOS 与退出 |
| 4 武器/数据 | Weapon/Defense 定义、EquipmentComponent、EnemyAttackProfile/AttackSet、ProjectileDefinition | 数据/运行状态分离、授予/撤销、依赖方向、远程配置/Socket、伤害配置唯一来源及近战兼容 |
| 5 命中/防御/投射物 | MeleeTraceWindow、MeleeHitResolver、CombatProjectile、Projectile resolver/targeting、Player 防御入口 | 去重、阵营/ASC/伤害归属、Guard 夹角/体力/GuardBreak/反馈/终止、Parry 禁用、来源失效及回调时序 |
| 6 反馈/退休残留 | Feedback Profile、相关 Character 回调、战斗 Tag 监听、兼容分支、测试 Set/Get 接口 | 反馈越权/重复、无生产发射源的监听、Physics/Loadout 残留与失去消费者的测试接口 |

读取范围限入口、关键执行/清理函数、一跳依赖和直接测试断言，不通读无关系统或大型测试。本阶段是固定快照审计，不能缩成最近 RateWindow 的 diff；本会话规划已读且无变化的源码证据可沿用。

## 3. Runtime Contracts 与事实修正

- Character-owned ASC 与 GAS 是动作、消耗、伤害、打断和恢复唯一权威；Controller 拥有目标/导航/战术意图，StateTree 编排，Ability/Effect 执行。反馈不产生第二条伤害路径。
- 保持既有 Trace/Resolver 和 Projectile/Resolver 到 GAS 的交付链，保持取消/UnPossess selector、State.Block.Facing、处决锁和 DeathPending 契约。
- 异步回调验证宿主、当前激活和身份；ReadyForActivation 返回点按同步重入处理；结束汇聚到既有 EndAbility/幂等清理出口，不恢复已终止状态。
- EnemyAIController 固定激活 Ability.Attack.Enemy.Melee；MeleeRange 实际来自 AttackSet.EngagementRange。名称本身不定级，必须证明实际近战绑定。
- Projectile 已通过 TryResolveIncomingDefense(..., false) 支持 Guard 并传入 GuardStaminaDamage；Parry 禁用。State.Block.Facing 不是防御结算判据。
- Guard 夹角和 HitReactionImpactResolver 优先读取攻击者/Instigator 位置；后者以 ImpactNormal 回退。核验射手移动造成的方向问题，不能直接修改所有近战/处决方向优先级。
- 来源 Dead 或 SourceActor/SourceASC 弱引用失效沿用拒绝结算策略；分别检查 Dead、Ragdoll、Destroy 和 ASC 失效，不擅自改为死后箭矢继续伤害。
- Player 致死取消入口归 TODO-03D；既有已关闭阶段及 authored 收据债务不据静态审计自动重开或关闭。

## 4. 两批审计与交付规则

第一批恢复 HEAD/工作树/既有门禁、建立六组职责覆盖并形成候选。匹配基线的影响图优先用于 StateTree Tasks -> EnemyAIController -> GAS；没有实际 diff 不伪造 changed_files。

第二批围绕具体疑点核验一跳源码、Tag/Config 和测试断言；CodeGraph 优先用于 CombatProjectile -> HitResolver -> Player Defense/GAS。最多两次图查询，图谱缺失、过期或动态关系不可覆盖时记 coverage fallback，不建图、不重试。单类和图谱裁剪部分用聚焦源码；第二批后停止探索并输出。

Main 常规缺陷审查与关键设计反证使用同一证据集，不追加源码探索轮次。用户随后授权的独立 Fresh Reviewer 核验具体 Findings、覆盖边界和准入裁决，不开展第二次全库审计。主控文档落盘和最终 diff/范围/空白检查不属于新增源码探索。

交付：
1. Findings 表：稳定编号、P0-P3、路径/行号/符号、场景、违反契约、影响、最小修复路径、验证方式、03C blocker。
2. 覆盖/候选表：已核验入口、合理差异、实际验证缺口。C++ 零引用不足以删除反射、Blueprint、DataAsset、软引用或 Config 消费项；缺少 Editor 证据保留候选。
3. 03C 接入表：可复用、正常新增、必须先修复、接受延期。第 3/4 组各给一项有证据的推荐路径及预计影响，比较真实生命周期和兼容性，不附实现代码草案。

RateWindow 必要隔离记录保留理由；H6-F01已构成当前规则漂移证据，共享实例授权归FIX1，剩余同契约绑定样板和无用测试API归已排期SHRINK，详见第8节。03C不默认需要RateWindow，不以宏或公共基类为目标。Main另行冻结每片路径/验证后交Gemini；审计和排期本身不授权源码修改。

## 5. 03C 复杂度边界与接口决策要求

03C v1：一个远程敌人的 HoldRange -> Fire -> Reposition，默认单发直线、非追踪投射物；决策与实际发射均需有效 LOS，目标由 AI 持有，不复用 Player Lock-On。不增加施法框架、预判、复杂抛物线、全向扫描、通用评分、第二套投射物/伤害系统。

- 攻击路由建议必须比较统一请求与专用远程 Task 对冷却、取消、目标快照、LOS 和 StateTree 退出的影响。
- 数据承载建议必须说明扩展 Profile、专用 Profile 或专用 Ability 的选择理由、AttackSet/校验兼容、ProjectileDefinition 与 Profile 的伤害配置唯一来源。
- H6 只给建议、理由、影响路径；新 API/字段/资产迁移由后续独立计划冻结。正常新增远程功能不伪装成现有近战缺陷。

## 6. 验证矩阵、用户门禁与完成标准

| 影响面 | 审计检查/后续修复需要覆盖的场景 |
|---|---|
| Ability/helper | 正常结束、取消、启动失败、同步重入、旧 Context、同资产新实例、重复清理；RateWindow、ActionWindows、Motion-Warping |
| 投射物防御 | 正面/侧后 Guard、体力足够/耗尽、GuardBreak、无防御、Parry 禁用、重复碰撞不重复扣除/反馈；Projectile.Lifecycle 与防御测试 |
| 方向/来源 | 发射后射手移动、玩家转向、Dead/Ragdoll/Destroy/ASC 失效、有效/退化 HitResult；受击方向与伤害归属分离 |
| AI/数据 | 交战范围/攻击距离、固定 Tag、LOS/目标失效、发射前中断、状态退出、冷却/后撤、近战 Profile 兼容 |
| 清理/删除 | 生产/测试引用、反射/Config 入口、适用 readback 和原契约回归；不得只靠零 C++ 引用 |

本轮只核对测试与已有收据，不执行编译、Automation、Editor/PIE。Editor 操作清单为空，readback 仅为后续资产候选的关闭条件。既有结果记录执行者和版本；手工 Notify/委托或逻辑 seam 不冒充跨帧/真实输入证明。

修复另需 Development Editor 编译、影响范围 Focused Automation；真实动画、资产或跨帧行为受影响时增加针对性的用户 readback/Scene01 PIE，不重跑无关全套。

H6 完成条件：六组覆盖与结论可追溯，明确缺陷/新增工作/候选/债务差别；仅三份文档发生本轮差异且 git diff --check 通过；所有延期风险有 ROADMAP 唯一归口和关闭触发。分别给出审计完成和03C准入结论；实际安全/权属/清理/接入 blocker 修复验证后放行。未排期的P3风格建议、合理差异及非阻塞authored收据不拖延03C；用户已接受的有界SHRINK按第8节收口。Git提交仍需用户单独批准。

## 7. 审计结果与证据（2026-09-12）

### 7.1 Findings（Main 审计与独立 Fresh Review 已完成）

本轮没有运行复现；以下为当前源码、引擎 API 和既有契约支持的条件性静态缺陷。未发现可证明的 P0/P1。缺陷范围不外推到未检查的资产或所有 Ability。

| ID / 级别 | 文件、符号与证据场景 | 影响及 03C 门禁 | 最小修复方向与验证 |
|---|---|---|---|
| H6-F01 / P2 | Source/PolyQuest/Private/AbilitySystem/Abilities/EnemyMeleeAbility.cpp:484、512、557：Begin/End/Clear 用 GetMontageInstanceForID 检查旧实例仍存在、未停止；但 MontageRateWindowLifecycle.cpp:302 的恢复及 HandleBegin/End 的调速按资产调用 Montage_SetPlayRate。UE 5.8 AnimInstance.cpp:3094 以 GetActiveInstanceForMontage 选择当前实例（映射查询见4451）。具体前置是同资产以不停止旧实例的方式重新播放，旧 Ability/Context 仍有效；此时旧 Begin/End 或清理能改写新实例速率。独立 Reviewer 核对 Montage_PlayInternal 可在 bStopAllMontages=false 时保留旧实例并于2809更新映射。 | Enemy 写入授权与 Player 07B8-C 补修契约不一致，是条件性静态防御缺陷。作为 H6 基础实例授权修复门禁，按既定路线优先收口；不是已证明的远程运行时依赖。当前生产调用/资产是否生成此前置尚未验证，不称已经发生的回归，不否认 B/C 原收据。 | 候选 TODO-03H6-FIX1：先以 EnemyMeleeAbility.cpp 与 EnemyMontageRateWindowAutomationTests.cpp 为最小反例入口，正式实施同时包含共享helper与适用Player/Enemy消费者的单一校验接入（见第8节）；用真实 ASC/AnimInstance、同资产双实例且旧实例未停止的前置断言，验证旧 Begin/End/Clear 不改新速率，禁用 bypass。同类查询出现在 EnemySmallHitReaction、EnemyHitReaction、EnemyLaunchReaction、EnemyVictimExecution，须在 FIX1 计划冻结前逐一确认并封闭名单；StanceBreak 是独立基准，不能机械套用。 |
| H6-F02 / P2 | Source/PolyQuest/Private/Combat/Projectile/CombatProjectileHitResolver.cpp:66 将射手 SourceActor 交给防御；PlayerGuardAbility.cpp:407 用射手实时位置算夹角。resolver:73 使用来源 ASC 的 EffectContext；HitReactionImpactResolver.cpp:25 优先 Instigator 位置，EnemyCharacter.cpp:335 的致死方向也消费该结果。例：箭从玩家正面飞来，命中前射手移到侧后方，防御弧/受击方向随射手变化；反向也可能出现错误格挡。 | Guard 已有接线和体力结算，但缺少投射物命中几何与来源归属的分离。当前 Bow 到 Enemy 的受击/死亡方向也使用该公共路径；Enemy 到 Player 防御是03C重点接缝。03C 前阻塞项；不是新增 Parry 的理由。 | 候选 TODO-03H6-FIX2：Projectile/HitRequest/Resolver、Player 防御入口、Guard 夹角接口、ImpactResolver 及直接测试。建立投射物专用的明确命中方向输入，同时保留 Instigator/SourceASC 归属和近战/处决原方向优先级；不要简单把 SourceActor 换成箭矢或颠倒全局 fallback。补射手移动、玩家转向、前后弧/零体力、无效命中方向、来源失效与近战/处决回归；编译/Focused Automation 和定向用户 PIE 后关闭。 |

上述候选修复路径没有写入授权，不是完整实施白名单；Main 下一步另写独立计划。验证目的、预期行为和复现前置必须明确后才交 Gemini。

### 7.2 六组覆盖、合理差异与边界

| 组 | 当前证据与结论 | 有意保留的证据边界 |
|---|---|---|
| 1 Ability/helper | 直接核验共享窗口的身份/来源/倍率检查和恢复，Enemy Melee 与 Player Charged 的绑定/清理，检索其他 Enemy 实例查询；沿用同会话已读的 Light/其他 Player 接口和基线C的逐消费者收据。Charged BindRateWindow:1035-1058 在两次 ReadyForActivation 后校验代次、Context、Task 与当前实例。H6-F01 是需要整改的具体漂移。 | 11 套 UObject Context 的弱引用、反射回调和代次隔离有用途；Light 换段、Charged 暂停、Bow Section、Dodge 重触发不能只按行数合并。未逐行重审全部 Ability 非RateWindow路径，也不把共用 helper 的资产级检查当作完整实例授权。 |
| 2 Character/ASC/反应 | BaseCharacter.cpp:70 以自身作为 ASC Owner/Avatar；EndPlay:92 解绑并 CancelAllAbilities。Enemy EndPlay:91 清 timer/锁定/事件，Controller OnUnPossess:201 停逻辑/导航并清 pending/target。AttributeSet:36/64/92 分别有 Current/Base 和 GE 后处理。处决 DeathPending 对 Health 分支的短路保留。方向问题归H6-F02。 | 本次 AttributeSet 为钩子职责与关键分支检查，不宣称所有数值/GE组合已验证。完整处决解除/销毁序列沿用原收据边界，Debt-REC-05A1-03-RET-Teardown 不自动关闭；Player 致死取消仍归03D。 |
| 3 AI/StateTree | 已读 RequestMeleeAttack Enter/Tick、Controller pending/range/请求链；EngagementRange 是进入交战范围，Profile.AttackRange 是单动作执行距离，GetPendingAttackRange 和 MoveTo 按此消费。Approach/Reposition ExitState 停各自移动，UnPossess 清理 Controller状态。 | 固定近战 Tag 是03C所需新增路由的接缝，现有近战行为不因此成为独立bug。当前任务没有远程发射LOS契约，新增LOS属于03C；不把计划中的未来能力写成当前实现。 |
| 4 武器/数据 | EnemyAttackSet.cpp 校验空/重复/无效profile、权重和距离；EnemyAttackProfile::IsValidAttackProfile 当前非虚且强制 Montage+DamageGE。Equipment.cpp:995-1007 防止与Startup/非本组件spec重复，Teardown:1015 只 ClearAbility 本组件记录句柄并复位。Bow 从武器定义取 ProjectileDefinition/LaunchSocket，Projectile 捕获 DamageGE。 | 未对装备事务全矩阵重跑；现有本地WIP Guard资产依赖仍由原债务管理。远程payload缺失是03C正常新增，不是当前近战资产失效。 |
| 5 投射物/防御 | HitRequest 的 SourceActor/SourceASC 是弱引用；resolver:34/56 对空来源、Dead、目标无敌拒绝；:66 允许Guard、禁用Parry；Guard.cpp:211-267 走体力GE及GuardBreak。Projectile.cpp:398/422/425 在成功消费命中后终止飞行。 | ProjectileLifecycle:415 证明测试源码有Dead来源拒绝；不是完整Destroy/ASC失效/同步回调重入的跨帧证明。PlayerDefenseAudio:231-239 已有敌方projectile到Guard的resolver级入口；ParrySuccessImpactFeedback:315 已有projectile路径。它们不能替代真实飞行+发射后射手移动的集成用例。 |
| 6 反馈/退休残留 | Enemy Health/Death 反馈受authority、teardown、DeathPending保护；Guard反馈与体力结算分离。限定战斗源和Tag配置检索未发现 ActiveCombatLoadout、AssociatedLoadout、EActionState、旧LaunchCharacter调用、Ability.Skill.Melee 注册残留。现有Execution.Release和Launch事件有实际原生发射/消费入口，不能按名称删除。 | 不是完整反射/资产零引用证明；不批准删除Tag或资产。测试辅助接口做了Source标识符消费扫描，候选见下表；不宣称它们是运行时风险。 |

### 7.3 清理候选与反证结果

| 候选 | 已有证据 | 裁决与关闭条件 |
|---|---|---|
| EnemyVictimExecutionAbility.h:110 GetTestHasSavedCanWalkOffLedges；EnemyLaunchReactionAbility.h:78 GetTestRootMotionKnockdownCompletedNaturally | Source 中各只有唯一声明，未找到直接C++消费者。 | 已归SHRINK正式排期；实施前核验消费面并逐项删除或具体保留。测试宏内非反射getter不要求无关资产readback，生产字段/状态不随之删除；完整11项见第8节。 |
| DodgeAbility.h:70/71/72 的 GetTestAttackingStateTag / GetTestDodgingStateTag / GetTestHitReactingStateTag | 同上，标识符扫描各只有声明。 | 与上项统一归SHRINK，按第8节在03C前完成有界裁决；H6不直接删除。后续补充已展开全部11项清单，不再只记录5项。 |
| RateWindow Context 样板 | 每个上下文仍承载当前Ability弱引用、反射事件、绑定身份与清理隔离；不同动作有不同切换时序。H6-F01已证明规则漂移，补充核对还确认Charged/Sprint的BindRateWindow类型名归一后完全相同。 | 保留Context隔离，同时实际消除同契约重复：FIX1集中实例授权，SHRINK收敛其余绑定样板。不得把保留生命周期等同于保留重复实现；第8节记录用户接受的排期和完成标准。 |

反证结论：旧实例“仍存活”能证明回调对象存在，但不能证明 Montage_SetPlayRate 的资产映射仍指向它；无法为H6-F01辩护。Instigator位置优先对既有近战是明确契约，HitReactionAutomationTests.cpp:607-620 还断言它优先于冲突ImpactNormal，因此H6-F02必须以投射物专用语义修复，不能推翻全局近战规则。Guard已有结算、Parry禁用、Dead来源拒绝、Profile不含远程参数和有用途的Context重复均不额外列成缺陷。

### 7.4 03C 接入建议与分类

| 分类 | 结论 |
|---|---|
| 可直接复用 | Character-owned ASC、既有Projectile Actor/发射快照/伤害交付与阵营过滤、Player Guard体力/GuardBreak、Enemy受击/死亡/反馈入口、Controller目标/导航/冷却/StateTree编排的所有权；复用不等于所有远程时序已验证。 |
| 必须先修复 | H6-F01 作为基础实例授权修复门禁优先收口，非已证实的远程运行时依赖；H6-F02 是远程防御/命中几何的直接接入 blocker。两项分别独立计划并验证。 |
| 03C正常新增 | 远程Ability及一次发射时机、Profile的远程payload与Socket、AI目标/瞄准点快照、决策/发射LOS、HoldRange/Fire/Reposition的窄状态编排、专属测试与资产。 |
| 已排期代码健康工作 | 11项测试API裁决与实际绑定样板收敛归SHRINK，按第8节在03C前有界收口。它不是已证明的远程运行时依赖。 |
| 非阻塞延期 | 既有authored/readback债务；Player死亡03D；全局/装备/处决未在本轮重跑的已记录证据边界；清单外未知残留和推测性重构。 |

**攻击路由推荐：在现有 Controller 内统一“请求已选攻击”与通用等待动作结束的边界，复用一套 pending profile/冷却/目标所有权；远程独有的 HoldRange、发射LOS和后撤条件保留为03C局部任务/策略。** 理由是当前请求层的公共门禁和 State.Action.Attacking 观察可以复用，真正近战绑定是固定执行Tag；不应复制一套并行Controller目标、pending与冷却。无需在H6先重命名所有Melee字段，也无需为远程套用近战Approach移动行为。预计影响 Controller.h/.cpp、StateTree Tasks/Conditions 和 Profile 的执行选择边界；具体API及旧任务资产兼容在03C计划冻结。

**数据承载推荐：对现有 EnemyAttackProfile 做有界扩展，明确区分近战与远程投递payload，默认保持旧近战配置与校验；不派生一个仍被非虚基类校验强制要求近战DamageGE的伪兼容Profile，也不把每敌人配置塞进运行中Ability成员。** 远程payload引用ProjectileDefinition和发射Socket；远程DamageGE唯一取ProjectileDefinition，近战继续取Profile的现有DamageGE，不要求远程重复填写两处。Montage/range/cooldown/GuardStaminaDamage继续是攻击配置；运行时由Ability捕获，AttackSet只选择。03C计划需明确执行选择、分支校验、旧资产默认值和无效Socket拒绝，不能按“建议”擅自修改当前资产。

### 7.5 工具、验证与准入状态

- Main 两批源码探索已结束；没有第三批。第一批 code-review-graph 读取本地metadata确认 git_head_sha=9ae684c，与审计基线759efca不匹配，skipped/coverage fallback，不执行变更雷达或重建索引。第二批1次CodeGraph核对Projectile/Impact接缝，裁剪部分沿用本会话已读且无变化的源码和精确读取。图谱仅作导航。
- 本轮只执行静态源码/文档核查，没有编译、Automation、Editor/readback、PIE、视觉、网络或打包结果。基线C的构建/12项组合、用户PIE、P2补修后的2项回归均按759efca旧plan中各自版本和执行者保留。
- 独立 Fresh Review：用户授权的 /root/h6_audit_fresh_review 在干净上下文、两批只读核验后确认两条 P2 静态反例，审计稿本身无 P0-P2 问题；支持现有Controller/Profile接入建议。复核范围仅为两条Finding、直接测试边界、接入建议和覆盖表述，不是六组源码的第二次完整审计。Main已采纳唯一措辞收窄：F01是基础正确性修复门禁，未证明当前资产触发或远程运行时依赖。既有Enemy真实retrigger断言旧实例停止，不能替代旧实例仍活着的F01用例。
- H6只读审计、独立复核与文档交付完成；用户随后接受FIX1共享校验及SHRINK代码健康排期，最新03C前顺序见第8节。用户现已批准仅三份H6文档的暂存/提交；没有源码修复或资产写入授权，提交记录以Git历史为准。
- 最终文档检查：三份白名单文档的 git diff --check 通过；Source、ARCHITECTURE、AGENTS无差异，staged为空，HEAD未改变。ROADMAP的两条缺陷和两条候选/集成债务各有唯一条目；archive原有全部行保持不变，仅追加。文档收尾前后排除三份白名单的Git短状态完全一致，用户WIP未被本轮修改。

## 8. 代码去重与清理排期补充（2026-09-12，用户已接受）

本节是H6审计后用户要求的排期修订，不追溯扩大第7.5节独立Fresh Review的覆盖，也不启动源码实施。早先仅挂清理候选、等待未来消费者的措辞由本节取代。

**最新顺序：TODO-03H6-FULL-AUDIT → TODO-03H6-FIX1 → TODO-03H6-FIX2 → TODO-03H6-SHRINK → TODO-03C。全项目范围补充见第9节。**

### 8.1 FIX1：实例授权修复与共享校验

采用Gemini方案A的方向：在既有MontageRateWindowLifecycle边界集中当前实例授权校验，Player和Enemy的所有适用消费者消费单一生产实现，不接受向多个Enemy Ability再复制一套判断。契约包括有效对象、资产当前实例、绑定ID和停止状态；恢复路径不能要求Ability仍Active才能释放仍持有的资源，事件/恢复前提的合理差异需在FIX1计划冻结。

保持各Ability的Context、代次和动作生命周期，不统一EndAbility所有权。Enemy Melee是先构造反例的入口，不代表实施范围只允许补它一个类；后续FIX1计划明确共享helper、Player/Enemy消费者和测试白名单，独立核对StanceBreak。具体函数名、静态或成员形式、头源位置不在路线图上臆定，不为此创建通用基类或宏。

完成不仅是F01测试通过，还包括去除等价本地实例授权实现；提供规则原位置、共享位置和全体适用消费者列表，防止两套权威继续并存。

### 8.2 SHRINK：实际绑定样板收敛与清理

该阶段正式排期，不再只是非阻塞建议，也不命名为仅处理Bug的FIX3。与FIX1分工：
- FIX1负责实例授权规则；SHRINK不重复抽取同一部分。
- SHRINK负责FULL-AUDIT接受的全项目瘦身清单，分模块/契约实施；以下绑定样板、11项测试接口和退役残留只是已知首批，不能代替全项目覆盖。
- 当前已确认的最低去重入口：ChargedAttackAbility.cpp:989与SprintAttackAbility.cpp:642的BindRateWindow各约73行，替换类型名后函数体完全一致；BowDrawFireAbility.cpp:800同结构主要区别为结束状态字段。必须完成实际共用实现与至少这组消费者迁移，不能只删getter就收口。若FIX1已吸收该组，引用实际变更及测试计入，避免重复改造。
- 11套Context仍逐项给出已迁移共用部分/保留差异的表格；Light逐Entry、Charged暂停、Bow Section、Dodge重触发及Enemy专有状态不得因提取被改变。不强制所有UObject Context合成一个类，不引入通用施法/调度框架。
- 不能承诺“代码量必然下降”；必须证明重复规则/流程集中、旧实现移除、无无用转发层。净行数可记录，不能替代实际维护收益。
- 对旧Loadout/Physics等本轮未发现可删除项的结果明确记零，不为满足瘦身指标制造删除。仍服务旧Enemy fixture的BladeTrace fallback不纳入本次无资产迁移的清理。

### 8.3 完整11项测试接口候选清单

以下头文件均在Source/PolyQuest/Public/AbilitySystem/Abilities下；候选为测试宏内普通getter，Source标识符扫描各只有声明。正式实施前复查是否新增消费者；确认无调用即可删除getter，不要求不相关的Editor/Reference Viewer证明，也不连带删除对应生产字段或setter。

| 头文件 | getter / 当前行号 |
|---|---|
| EnemyVictimExecutionAbility.h | GetTestHasSavedCanWalkOffLedges / 110 |
| EnemyLaunchReactionAbility.h | GetTestRootMotionKnockdownCompletedNaturally / 78 |
| DodgeAbility.h | GetTestAttackingStateTag / 70；GetTestDodgingStateTag / 71；GetTestHitReactingStateTag / 72；GetTestPlayerLaunchReactionAbilityTag / 73 |
| ChargedAttackAbility.h | GetTestChargeVFXSystem / 294；GetTestChargeVFXTraceSourceName / 296；GetTestMaximumChargeDuration / 298；GetTestChargeVFXComponent / 300；GetTestAttachParent / 308 |

11项均须形成删除或具体保留消费者/用途的裁决，不能再次整体延期。普通原生死分支/无反射测试接口按代码消费面和编译验证；只有实际存在反射、Config或资产入口的符号才要求相应readback。

### 8.4 验证、交接与边界

- 在FIX1/FIX2通过的实际版本上制定SHRINK独立plan，先冻结允许修改的文件/符号及重复片段，执行者仍走manual/out-of-band Gemini；本轮仅更新三份已批准文档，Source/Test无写入授权。
- 每片需要编译和受影响Focused Automation。测试getter删除不需要PIE；共享绑定/委托/ReadyForActivation/清理位置发生变化时，必须覆盖同步重入、取消、旧Context、同资产替换和适用的用户PIE。现有测试不足时补关键用例，不删断言换取通过。
- 不采纳“纯Refactor只要旧Automation全绿即可”的统一豁免；验证按触及的运行时行为确定。也不要求无关全项目Automation或整套PIE。
- SHRINK是用户已接受的03C前代码健康收口，不是远程运行时技术依赖；清单外资产迁移、未知残留或推测性抽象不得无限扩展其范围。已确认重复的提取和11项裁决完成后即可按门禁收口。
- 本次补充只有静态源码对照和文档检查，无编译/Automation/Editor/PIE执行，也没有新的独立Fresh Review结论；原H6两条Finding及原复核收据保持其证据归属。

## 9. 全项目范围与插件使用补充（2026-09-12）

用户进一步明确：目标是全项目审查与瘦身，不限最近提交或之前的战斗切片。第7节H6完成态只表示原六组有界审计完成，不是全项目审查/清理完成；第8节已知重复和11项getter只是首批证据。

- 最新下一项为TODO-03H6-FULL-AUDIT，建立全模块覆盖和候选台账，然后FIX1 → FIX2 → 项目级SHRINK → 03C。Source/PolyQuest实际存在AbilitySystem、AI、Animation、Camera、Character、Combat、Environment、Framework、UI和Private/Tests，全部列入。Build/Target、Config及实际存在的项目自有脚本/插件也先盘点再审；第三方、引擎、生成目录不作为项目自有代码删除范围。
- 以当前完整源码快照审查；Git提交ID用于定位版本，git diff不是范围过滤器。资产/反射/软引用的消费者证据仍需核对，用户WIP只限制写入和提交，不能被解释成可以忽略的运行时依赖。
- FULL-AUDIT按模块和跨模块接缝制定有界批次；每个模块需覆盖记录，所有候选都需删除/提取/简化/保留/有条件延期裁决。随后按共享契约或模块逐片实施，不能把全项目审查变成一口气全库重构，也不能只完成RateWindow就关闭全项目目标。
- Ponytail 4.9.0本地缓存中，ponytail-audit明确是whole-repo、只输出复杂度清单、不执行修复，正确性/安全/性能不在其范围；ponytail-review面向diff。建议显式用Audit辅助冗余线，ue-strict-review继续负责正确性/生命周期，最终台账保留双方证据。
- 插件审阅是对其能力的评估，未执行其中“持续激活”等指令。初次检查时本地Codex配置enabled=false；用户随后自行启用插件，最新只读配置核对为enabled=true、defaultMode=lite。缓存状态标记不证明钩子已执行。插件含SessionStart/SubagentStart/UserPromptSubmit钩子，可能注入持续规则。推荐保持lite并按需调用Audit，不以净删行数、单实现接口或“最短实现”自动推翻UE/GAS契约；本轮不启用插件、不改其配置。
- 提交不是只读审计的技术前提：当前HEAD为759efca，Source无未提交差异。建议先单独提交已对齐范围的plan.md、ROADMAP.md、ROADMAP-archive.md作为可追溯交接；Content、Config、tmp等WIP排除。用户随后明确回复“可以提交”，现已授权仅这三份文档的暂存/提交；不提交其他WIP、不推送、不自动启动全项目审查。
- 全项目审查尚未开始；本节和路线图完成范围修正。新阶段正式计划获接受前保留本H6交接及归档指针，不覆盖已有审计证据。


## 10. 文档提交批准（2026-09-12）

用户明确批准提交plan.md、ROADMAP.md、ROADMAP-archive.md。包含H6静态审计/独立复核收据、两个P2及修复门禁、共享校验/11项getter/SHRINK排期、全项目FULL-AUDIT范围与历史交接。排除Source、ARCHITECTURE、AGENTS、Config、Content、tmp及其他WIP；不推送。提交哈希以Git历史为准，本次不宣称全项目审查、代码修复、编译、Automation或PIE已经执行。
