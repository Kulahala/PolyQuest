# TODO-03H6-FULL-AUDIT：全项目架构、冗余与退役残留静态审计（已完成）

## 1. 目标、基线与授权边界

审查当前完整工作树中的全项目自有代码，覆盖架构权属、生命周期、重复实现、过度抽象、死代码和退役残留，交付全覆盖记录、逐项裁决台账及后续 FIX／SHRINK 分组。**Git 历史用于定位证据，不作为范围过滤器。**

**本轮状态（2026-09-12）：Main 已完成 230/230 Source 文件及外围清单的静态审计，交付 14 项 Findings、24 项候选与逐文件证据记录。Gemini 初稿的完成／收益声明已按真实源码校正，历史保留于归档。全项目静态审计完成；03C 尚不准入。用户授权仅提交三份阶段文档，不修改源码、不运行编译／Automation／Editor／PIE、不派发子代理或额外 Fresh Review、不进入 FIX1。**

本版补充分批检查点、分级审阅、测试冗余标准、反射删除门槛及收益估算。保留已接受的阶段顺序：FULL-AUDIT → FIX1 → FIX2 → 已接受项目级 SHRINK → 03C。

- 项目：`E:\GameDevelop\PolyQuest`；UE 5.8，引擎依据为 `D:\UE\UE_5.8`。
- 历史 H6 基线：`9f2d8e0b1b8c34dabc4f17bfb6aea6b0a2adb229`，其提交仅三份阶段文档。已批准计划及本轮执行基线 HEAD：`d99ddcd54b70d043265479719ab74f05a48ed144`；Source 与 `759efca` 无差异。审计范围为完整在盘工作树，不按提交差异筛选。
- 完成盘点：230 个 Source 文件（生产／构建 158、Tests 72），共 75,281 物理行；行数不代表可删除量。原有 WIP 共 2,122 项（Content 2,117、Config 1、tmp 4）保留，暂存／提交仅包含三份文档；最终保护核验见 CP-77。
- Outer：`ue-stage-workflow`；Primary：`ue5-architecture`；Support：`ponytail-audit`。
- Execution route：Main 单代理只读审计与文档；Implementation executors：0。不派发 Gemini／子代理，不设置独立或额外 Fresh Review 门禁。后续源码实施另按各自计划和项目路线交接，不能沿用本轮文档批准越权。

### Approved Paths 与 Deliberate Non-goals

正式落盘仅允许修改三份文档：

| 文档 | 允许内容 |
|---|---|
| plan.md | 本计划、逐文件覆盖状态、继承 Findings、候选台账、检查点及最终结论 |
| ROADMAP.md | 活跃阶段指针、确定的后续归属、唯一风险条目及关闭条件 |
| ROADMAP-archive.md | 仅追加阶段交接与收口摘要，保留历史正文 |

H6 已有归档，本轮先追加交接及完整历史计划指针 `git show 9f2d8e0:plan.md`，再替换本文件。路线图中指向旧 H6 第 7／8 节的引用须改指历史版本或本计划第 4 节，避免证据漂移。

**本阶段没有任何生产或测试符号写入授权，也没有公开 API／类型变更。** 不改源码、测试、Tag、Config、Build.cs、资产、工具配置或 ARCHITECTURE.md；不运行编译、Automation、Editor／PIE。用户本轮明确批准审计完成后仅暂存并提交三份文档，不推送；既有 WIP 不得回滚、恢复、格式化、暂存或归因于本次交付。批准记录和保护核验见第 7 节。

## 2. 全模块覆盖与增量检查点

当前只有一个自有 UE Runtime Module：`PolyQuest`。下列是功能分组，文件数合并 Public／Private 统计。

| 覆盖单元 | 文件数 | 必查范围 |
|---|---:|---|
| 模块与构建入口 | 5 | 模块注册、日志、Game／Editor Target、Build 依赖和 Public／Private 暴露 |
| AbilitySystem | 60 | 全部 Player／Enemy Ability、AttributeSet、AbilityTask、窗口及朝向 helper，包含非 RateWindow 路径 |
| AI | 8 | Controller、AIProfile、StateTree Tasks／Conditions；目标、导航、pending、冷却、退出与 GAS 分工 |
| Animation | 10 | 全部 Notify／NotifyState；生产者／消费者、反射入口、迟到回调与退役事件 |
| Camera | 2 | FOV modifier、消费者、状态恢复与呈现职责 |
| Character | 8 | Base／Player／Enemy、Lock-On；ASC、输入、移动、死亡、反馈、遮挡和资源生命周期 |
| Combat | 47 | Combo、Enemy Profile／Set、Equipment／Pickup、Execution、Feedback、Melee、Projectile、Reaction 全子域 |
| Environment | 4 | FloorVolume／FloorTriggerVolume；查找规则、可见性、MPC、Overlap 与生命周期 |
| Framework | 4 | GameMode／PlayerController；控制入口、Possess／UnPossess、HUD 与委托解绑 |
| UI | 10 | Vital、SkillBar／Slot、EnemyHealthBar、InteractionPrompt；数据消费、呈现重复与反射绑定 |
| Tests | 72 | 全部 Automation 与 fixture；消费者、重复支撑、失效 seam、断言及关键覆盖缺口 |
| **Source 合计** | **230** | 每个文件都有审阅记录，不以抽查代表整库完成 |

外围覆盖另外登记：

- 项目描述与 Config：检查模块／插件声明、GAS、Tag、输入、碰撞、AssetManager 及测试配置。当前有 8 个在盘 Config 文件及 1 个已有删除 WIP，记录删除状态，不恢复文件。
- 自有脚本／插件：当前未发现项目级自有插件或正式工具目录；已发现的临时 PDF 检查脚本登记为非游戏工具 WIP，保留并排除游戏瘦身范围。启动时复查新增自有代码，不因未跟踪而漏审。
- 第三方、引擎、生成文件与构建缓存不作为自有代码清理对象；自有代码实际使用的外部接口和依赖仍需核验。
- Blueprint、DataAsset、软引用和配置消费关系属于删除裁决证据。资产为 WIP 不等于无消费者。

### 增量落盘规则

1. 正式执行前先完成计划及完整待审文件清单落盘；该检查点已完成；本轮从实际补审批次继续。
2. 审计执行按表中顺序串行推进；Combat 按现有子域、AbilitySystem 按动作族拆批，相关测试随生产代码读取，最终对全部 72 个 Tests 文件核对遗漏。
3. 每批最多 12 个文件；大型文件按函数区段拆批，记录最后完成的区段与下一入口。每批结束即增量更新 plan.md，不等全库读完。
4. 每个检查点保存：基线、已完成文件／区段、候选及裁决、未解问题、证据引用、下一批入口。恢复任务先核对这些记录及对应工作树差异。
5. 未经审阅的部分保持“待审／部分完成”；上下文切换或预算耗尽不能把它们改成“无问题”或“延期”。

### 审阅深度分三级

- **继承速验**：H6 已有明确符号／契约证据，且相关实现和直接契约无漂移时，继承结论及原证据边界。不能据此跳过同文件未审路径，也不能忽略 Config／资产依赖变化。
- **声明核验**：纯枚举、常量、短声明文件完整检查定义、消费者及反射／配置入口，不人为增加无关调查。
- **完整审阅**：其余实现，包括 H6 未覆盖的战斗路径和 UI、Environment、Framework 等子域，完成声明、实现、条件编译分支及直接依赖核验。

逐文件记录至少包含：所属单元、完成区段、审阅方式、证据来源、消费者／测试、结论或候选 ID、剩余缺口。第 6 节为真实清单；全部 230 行已替换为真实审阅记录；历史检查点中的待审计数仅表示当时进度。

## 3. 审计标准、测试专项与反射保护

Main 在同一批读取中检查架构、正确性和生命周期；Ponytail Audit 辅助复杂度审计。两条线进入同一台账，不另建一轮 Fresh Review。

跨模块接缝必须覆盖：输入／装备 → GAS；StateTree → Controller → Ability；Notify／Task → 激活身份 → 清理；Trace／Projectile → Defense／Resolver → GAS／Reaction；Character／Equipment／ASC → Controller／UI；Camera／Player → Environment／MPC。

### Runtime Contracts

- Character-owned ASC 与 GAS 是玩法状态唯一权威；Controller／StateTree 管意图及编排，UI／反馈不建立第二套玩法状态。
- 异步回调校验宿主、当前激活与身份；`ReadyForActivation()` 返回点按同步重入处理；取消、失败、结束及销毁收敛到既有幂等清理出口。
- 保留近战／处决方向优先级、投射物允许 Guard 且禁用 Parry、失效来源拒绝策略及各动作独立生命周期。
- 审计可以指出架构问题，但不授权推翻架构、引入公共 Ability 基类或新增模块。

### 通用证据要求

- **缺陷**：当前路径／行号／符号、触发前提、调用或状态链、违反契约、影响及可检验反例。区分静态缺陷、已运行复现和证据不足。
- **重复**：至少两处具体实现与消费者，对照输入输出、身份、生命周期及合理差异；说明最小共享位置和旧重复的移除方式。
- **删除**：排查适用的直接调用、继承／虚函数、委托、宏、反射、配置及资产入口。单实现、单调用者、空类或零 C++ 引用均不能独立证明无用。
- CodeGraph 优先导航；图谱版本或覆盖不满足需求时记录 coverage fallback，转向精确源码，不重建索引、不伪造 changed-files。引擎争议只查本机 UE 5.8。图谱、静态检查和原测试源码均不是编译、运行、视觉或资产 readback 证据。
- 一个疑点首次核验后，最多追加一次有明确目标的补证；仍不足则记录具体缺口和关闭触发。此限制不减少全文件审阅范围。

### Tests 专项标准

- `delete`：确认没有测试调用、注册、继承或反射消费者的孤立 helper／fixture 方法。
- `shrink`：跨文件重复的 ASC、Character、AttributeSet、World 等初始化与清理样板；必须核对 ActorInfo、生命周期、对象所有权和清理差异，再判断能否共用。
- `yagni`：对应生产逻辑已退役、且不再验证有效契约的 mock／seam。历史名称或测试专用身份本身不是删除理由。
- 逐项追踪测试 getter／setter 的真实消费者，检查断言是否仍能失败、是否绕过关键前置。
- 异常边界、不同前置下的相似用例、多重断言和真实集成覆盖不得因代码相似被误删；不通过删断言、弱化 fixture 或扩大 bypass 换取精简。
- 只有存在实际重复收益时才建议提取测试支撑，不预建通用测试框架。

### 反射与资产保护门槛

带 `UFUNCTION`、`UPROPERTY`、Blueprint 可派生类型、Notify、GameplayTag 事件／监听或软引用入口的候选，如果只有“C++ 零引用”证据，只能裁决为“保留”或“有条件延期”，不能批准删除。最终删除裁决必须取得其实际入口所需的资产／蓝图 readback 证据。

普通非反射测试 getter 按原生消费面核验，不要求无关 Editor 证明；其生产字段、setter 和运行时状态分别裁决。**本阶段 Editor 操作清单为空**，只记录需要补充的 readback 对象、属性／引用和关闭条件；不因补证需求自行操作 Editor。

## 4. 最终台账、收益估算及 FIX／SHRINK 分组

### 4.1 H6 继承项与逐项 getter 裁决

H6 原证据固定读取 `git show 9f2d8e0:plan.md` 第 7／8 节；本次执行基线为 `d99ddcd54b70d043265479719ab74f05a48ed144`，Source 未漂移。H6-F01／F02 保留原编号、P2 等级和证据归属，详见第 4.2 节；既有投射物集成、authored/readback、Player 致死入口及处决 teardown 债务维持 ROADMAP 原唯一归属，不因本次静态检查关闭。

绑定重复继承 Charged:989／Sprint:642 的类型归一相同证据，补齐 MeleeSkill:775／Dodge:445；Bow:800 同结构但保留结束状态差异。归 CAND-SHRINK-A，净收益须扣 FIX1 重叠，不把约 73 行函数长度当作净删量。

11 项 getter 已逐项核验（CP-76）：全 Source 精确符号搜索只命中单行定义，结合完整头源审阅确认均为 `WITH_DEV_AUTOMATION_TESTS` 内普通非虚、非反射接口，无委托／宏间接消费证据。**全部裁决删除，合计净 11 行，属于已接受 SHRINK-B；本轮未修改。** 实施时仍复核输入基线。头文件均位于 `Source/PolyQuest/Public/AbilitySystem/Abilities/`。

| 头文件／行号 | getter | 消费者与逐项裁决 |
|---|---|---|
| EnemyVictimExecutionAbility.h:110 | `GetTestHasSavedCanWalkOffLedges` | 全 Source 仅定义；删除 1 行，保留关联生产字段／setter |
| EnemyLaunchReactionAbility.h:78 | `GetTestRootMotionKnockdownCompletedNaturally` | 全 Source 仅定义；删除 1 行，保留关联生产字段／setter |
| DodgeAbility.h:70 | `GetTestAttackingStateTag` | 全 Source 仅定义；删除 1 行，保留关联生产字段／setter |
| DodgeAbility.h:71 | `GetTestDodgingStateTag` | 全 Source 仅定义；删除 1 行，保留关联生产字段／setter |
| DodgeAbility.h:72 | `GetTestHitReactingStateTag` | 全 Source 仅定义；删除 1 行，保留关联生产字段／setter |
| DodgeAbility.h:73 | `GetTestPlayerLaunchReactionAbilityTag` | 全 Source 仅定义；删除 1 行，保留关联生产字段／setter |
| ChargedAttackAbility.h:294 | `GetTestChargeVFXSystem` | 全 Source 仅定义；删除 1 行，保留关联生产字段／setter |
| ChargedAttackAbility.h:296 | `GetTestChargeVFXTraceSourceName` | 全 Source 仅定义；删除 1 行，保留关联生产字段／setter |
| ChargedAttackAbility.h:298 | `GetTestMaximumChargeDuration` | 全 Source 仅定义；删除 1 行，保留关联生产字段／setter |
| ChargedAttackAbility.h:300 | `GetTestChargeVFXComponent` | 全 Source 仅定义；删除 1 行，保留关联生产字段／setter |
| ChargedAttackAbility.h:308 | `GetTestAttachParent` | 全 Source 仅定义；删除 1 行，保留关联生产字段／setter |

保护例外：`GetTestSavedCanWalkOffLedges` 在 ExecutionVictimRootMotionAutomationTests.cpp:407 有真实消费者，必须保留；EnemyLaunch setter 及 Charged 运行时状态分别仍有用途。普通 getter 删除不需要无关 Editor/readback；这不授权删除生产字段或资产。

### 4.2 正确性与测试 Findings（14 项）

以下均为当前源码静态结论，**没有本轮 UE 编译、Automation、Editor／PIE 运行收据**。F05／F10 的独立标量反例仅验证数学关系。P1 共 1 项、P2 共 13 项；没有新增已证实 P0。每项按下列唯一 FIX 归属交接，不按消费者重复造单。路径相对项目根目录，精确区段、引擎核验和排除项保存在第 7 节对应 CP。

| Finding／等级／归属 | 源码锚点与触发证据 | 影响与证据边界 | 最小候选修改面／验证／门禁 |
|---|---|---|---|
| **H6-F04／P1**<br>FIX-BuildGuard | `Source/PolyQuest/Public/AbilitySystem/Abilities/PlayerFrontExecutionAbility.h:90`<br>`Source/PolyQuest/Public/AbilitySystem/Abilities/PlayerBackstabExecutionAbility.h:90`<br>`Source/PolyQuest/Private/AbilitySystem/Abilities/PlayerFrontExecutionAbility.cpp:336`<br>`Source/PolyQuest/Private/AbilitySystem/Abilities/PlayerBackstabExecutionAbility.cpp:376`<br>两头文件75–122将TestEvaluate*GeometryVectors声明置于WITH_DEV_AUTOMATION_TESTS，生产cpp却无条件调用/定义（Front336/347、Backstab376/387）。UE5.8 UEBuildTarget.cs:6313–6328确认Test/Shipping默认宏为0；当前Target未强开。<br>证据：CP-11 | 宏关闭时成员无声明，构建变体无法通过；静态确定，未实际编译。 | 修改建议：两Ability正式私有几何声明/实现与宏内测试薄入口分开；不通过强开测试规避。<br>关闭验证：Development Editor及Shipping/Test宏组合编译；Front/Backstab几何Automation。<br>门禁：Shipping/Test构建与打包门禁；不自动阻塞03C的Development实施。 |
| **H6-F01／P2**<br>FIX1 | `Source/PolyQuest/Private/AbilitySystem/Abilities/EnemyMeleeAbility.cpp:484`<br>`Source/PolyQuest/Private/AbilitySystem/Abilities/MontageRateWindowLifecycle.cpp:302`<br>`Source/PolyQuest/Private/Tests/EnemyMontageRateWindowAutomationTests.cpp:905`<br>旧同资产实例按ID仍live、旧Ability/Context仍有效，但新实例已占据资产映射；Begin/End/Clear的资产级Montage_SetPlayRate可写到新实例。Melee/Big/Small/Launch/Victim同根；StanceBreak另核身份。Enemy测试905旧实例已Stopped，不能覆盖双live；Player915–954双live只证明Player。<br>证据：旧H6第7.1节；CP-05/10/12/47/49/74 | 条件性实例授权缺陷；没有当前生产资产触发或本轮运行证明。Victim按ID Stop正确，不能混同。 | 修改建议：既有MontageRateWindowLifecycle集中当前映射实例/ID/停止状态授权，接入全部适用消费者；保留各Context与恢复权属。<br>关闭验证：先真实Enemy双live反例，再编译、受影响Player/Enemy RateWindow Automation、取消/重入/旧Context及适用PIE。<br>门禁：既定路线基础前置；不称远程运行时依赖。 |
| **H6-F02／P2**<br>FIX2 | `Source/PolyQuest/Private/Combat/Projectile/CombatProjectileHitResolver.cpp:66`<br>`Source/PolyQuest/Private/Combat/Reaction/HitReactionImpactResolver.cpp:25`<br>`Source/PolyQuest/Private/AbilitySystem/Abilities/PlayerGuardAbility.cpp:407`<br>Projectile将射手命中时位置交给Guard，Reaction优先Instigator。射手在飞行中横移/绕后，会改变同一投射物来袭的防御弧和反应方向。<br>证据：旧H6第7.1节；CP-09/24 | 来袭几何与GAS伤害来源混用；静态链成立，未运行复现。 | 修改建议：在Projectile/Defense/Reaction接缝分离来袭方向与来源归属；保留近战/处决Instigator优先、允许Guard、禁用Parry策略。<br>关闭验证：编译、射手移动/玩家转向/前后弧/体力/退化方向/无效来源矩阵、近战与处决回归、针对性PIE。<br>门禁：直接03C运行时接缝门禁。 |
| **H6-F03／P2**<br>FIX-ParryWindow | `Source/PolyQuest/Private/AbilitySystem/Abilities/PlayerParryAbility.cpp:364`<br>`Source/PolyQuest/Private/AbilitySystem/Abilities/PlayerParryAbility.cpp:382`<br>`Source/PolyQuest/Private/AbilitySystem/Abilities/PlayerParryAbility.cpp:181`<br>每次Begin AddLooseGameplayTag，End/EndAbility按单bool最多Remove一次；Begin→Begin→End→EndAbility计数0→1→2→1。UE ASC.h:656–668为计数语义。<br>证据：CP-09 | ASC ParryActive loose tag残留；原生防御读取bool，不能称永久弹反。当前资产有无重叠窗口未知。 | 修改建议：本Ability固定单窗口幂等或有身份的重叠语义，清理仅撤销本激活贡献。<br>关闭验证：编译、重复Begin/End、取消、重激活及外部tag贡献测试；相关Notify readback按实施计划。<br>门禁：普通修复；不自动新增03C前置。 |
| **H6-F05／P2**<br>FIX-TargetOrder | `Source/PolyQuest/Private/Character/Player/PlayerLockOnTargeting.cpp:14`<br>`Source/PolyQuest/Private/Combat/Projectile/CombatProjectileTargeting.cpp:300`<br>`Source/PolyQuest/Private/Character/Player/PlayerCharacter.cpp:1412`<br>近似相等作排序层级不满足严格弱序：LockOn角/距A=(0,3),B=(7.5e-9,2),C=(1.5e-8,1)，容差1e-8；Projectile A=(0,300),B=(.0075,200),C=(.015,100)，角容差.01距1；均出现A<C、C<B、B<A。滚轮/死亡后继与投射物assist消费。<br>证据：CP-15/18/24（统一早期FIX-LockOnOrder暂名） | 排序非传递，候选顺序与后继不保证一致。已做PowerShell single标量反例，非UE运行。 | 修改建议：两排序器建立严格弱序与确定tie-break；保留不同单位和优先级，不用近似相等充当等价关系。<br>关闭验证：编译、三元传递性/同值/输入排列矩阵及LockOn/Projectile多目标集成。<br>门禁：独立修复；是否接入具体选择路径由03C计划核对，未自动升级前置。 |
| **H6-F06／P2**<br>FIX-ExhaustionTimer | `Source/PolyQuest/Public/Character/Player/PlayerCharacter.h:135`<br>`Source/PolyQuest/Private/Character/Player/PlayerCharacter.cpp:2734`<br>`Source/PolyQuest/Private/Character/Player/PlayerCharacter.cpp:2778`<br>合法最短力竭时间0传SetTimer；UE TimerManager.cpp:653–733仅InRate>0注册，否则invalidate。完成flag只由未排出的回调设true，2784持续拒绝清理。<br>证据：CP-19/32 | 即使体力恢复，Exhausted/移速效果仍保留至其他终止路径；未断言当前资产设0。现有测试只覆盖2秒。 | 修改建议：Player耗尽计时明确定义0为立即或下一tick满足等待，保留动作完成门槛和外部tag贡献。<br>关闭验证：编译、0/正值、动作中耗尽、恢复、取消/销毁Automation及适用PIE。<br>门禁：普通修复；不自动阻塞03C。 |
| **H6-F07／P2**<br>FIX-InputTeardown | `Source/PolyQuest/Private/Character/Player/PlayerCharacter.cpp:1076`<br>`Source/PolyQuest/Private/Character/Player/PlayerCharacter.cpp:320`<br>Started写HeldCombatInputStartTimes并拒绝重复，只有Released/Canceled移除；UnPossessed/repossess未清。UE Pawn.cpp:713–735销毁InputComponent但不调用项目释放。Started→无Release断连→同Pawn重持有→新Started被Contains拒绝。<br>证据：CP-19a/34/61 | 旧物理输入资格和长按时间跨持有生命周期保留；不是已复现换Pawn故障。 | 修改建议：Player持有终止清除物理输入/长按/重试资格，逐项保留已活动能力策略；不要求CancelAll。<br>关闭验证：编译、无Release断连/重持有/旧timer测试；保留MotionWarping1667–1747的Guard不取消契约。<br>门禁：普通修复；不自动阻塞03C。 |
| **H6-F08／P2**<br>FIX-TeamDispatch | `Source/PolyQuest/Private/Combat/Projectile/CombatProjectileTargeting.cpp:82`<br>`Source/PolyQuest/Private/Combat/Melee/MeleeHitResolver.cpp:46`<br>优先直接GetCombatTeamTag_Implementation且原生tag有效即返回，绕过BlueprintNativeEvent override；近战/反馈用Execute。合法BP覆写不同阵营时投射物/LockOn与近战读取不同值。<br>证据：CP-24 | 接口动态分派不一致；当前资产有无覆写未知。 | 修改建议：共用官方Execute_GetCombatTeamTag入口；测试fixture遵循正式接口，不迫使生产绕过反射。<br>关闭验证：编译、原生接口与BP覆写消费矩阵、必要资产readback/PIE。<br>门禁：首次接入BP动态阵营前关闭；当前不自动扩大03C资产扫描。 |
| **H6-F09／P2**<br>FIX-FloorVisibility | `Source/PolyQuest/Private/Environment/FloorVolume.cpp:46`<br>`Source/PolyQuest/Private/Environment/FloorVolume.cpp:289`<br>`Source/PolyQuest/Private/Environment/FloorVolume.cpp:319`<br>289/319/352隐藏managed actors；EndPlay46–54只恢复MPC。inactive Volume先Destroy而这些actors继续存活时，hidden仍保留。<br>证据：CP-25/32 | 管理宿主结束没有撤销可见性贡献；整关卡一起卸载不构成该反例。 | 修改建议：本Volume记录并恢复自己施加的隐藏，保留外部初始hidden；多Volume需求另有证据才定仲裁。<br>关闭验证：编译、inactive销毁/原本hidden/重复清理测试，适用场景PIE；不以MPC恢复代替actor可见性。<br>门禁：普通环境修复；不阻塞03C。 |
| **H6-F10／P2**<br>FIX-UIFlash | `Source/PolyQuest/Private/UI/EnemyHealthBarWidget.cpp:224`<br>`Source/PolyQuest/Private/UI/PlayerVitalHUDWidget.cpp:210`<br>两widget的peak hold算法在合法duration<=.03时decay<=0，到期仍以weight1写peak白色，后续timer>0不成立，颜色不恢复。独立计算.01/.02/.03与大delta均确认。<br>证据：CP-27 | 短闪白配置永久停留峰值颜色；只有标量验证，无Slate/UE运行证明。 | 修改建议：两个消费者到期无条件恢复base color，再保留短时peak语义；后续UI去重扣除同段修复。<br>关闭验证：编译、两个widget的.01/.03/.15、跨结束大delta/重复受击与视觉检查。<br>门禁：普通呈现修复；不阻塞03C。 |
| **H6-F11／P2**<br>FIX-TestTiming | `Source/PolyQuest/Private/Tests/VitalHudAutomationTests.cpp:736`<br>`Source/PolyQuest/Private/UI/PlayerVitalHUDWidget.cpp:185`<br>100→40后单次SimulateTick(.60)立即期望buffer=.4；实际初始delay=.5减delta后return，buffer仍1。<br>证据：CP-33 | 当前测试断言与生产帧推进静态矛盾；未实际运行，不能称suite失败收据。 | 修改建议：测试分步证明delay期不变和之后收敛；若要跨边界同帧消费余时，先独立冻结生产契约。<br>关闭验证：编译、VitalHUD Automation；保留收敛断言，不能删断言换绿。<br>门禁：测试修复；不自动阻塞03C。 |
| **H6-F12／P2**<br>FIX-TestCancellation | `Source/PolyQuest/Private/Tests/SkillBarHudAutomationTests.cpp:365`<br>`Source/PolyQuest/Private/Tests/SkillBarHudAutomationTests.cpp:389`<br>`Source/PolyQuest/Private/Tests/TestPreparedSkillCooldownFixtures.cpp:62`<br>365–366 grant创建InstancedPerActor实例，389–392才把CDO bAutoEndAbility=false；实例仍true，396–400激活即自然结束再Cancel，未断言取消前Active。UE ASC_Abilities.cpp:319–322/1196–1220确认实例创建时点。<br>证据：CP-35 | 不能验证活动技能取消后冷却保留；不是生产冷却回归。 | 修改建议：grant前配置并恢复CDO或配置真实primary instance，先断言Active再取消。<br>关闭验证：编译、SkillBar/PreparedCooldown Automation；以实际取消结束理由和原冷却断言证明。<br>门禁：独立测试修复；不自动阻塞03C。 |
| **H6-F13／P2**<br>FIX-TestAI | `Source/PolyQuest/Private/Tests/EnemyCombatSpacingTests.cpp:202`<br>202–676用局部bool/时间和Simulate* lambda模拟重试/Approach/GAS失败/StateTree；无生产Controller/Task/Ability调用，删除生产分支仍通过这些断言。1–200真实Profile/几何有效。<br>证据：CP-37 | 自我模拟未覆盖声称的生产回归；不是已证实AI运行缺陷。 | 修改建议：真实Controller/Task入口+可控时间/导航驱动原场景；保留有效Profile和异常边界，不直接删整套测试。<br>关闭验证：编译、相关AI Automation；证明改变生产状态转移能使断言失败。<br>门禁：独立测试修复；03C不得把旧模拟段当新运行门禁证据。 |
| **H6-F14／P2**<br>FIX-TestExecution | `Source/PolyQuest/Private/Tests/ExecutionVictimPresentationAutomationTests.cpp:872`<br>`Source/PolyQuest/Private/Tests/ExecutionVictimPresentationAutomationTests.cpp:927`<br>前节868–869 CleanupExec经452–458清LockedTarget；第8节两用例仅grant/config，897/927激活因无目标提前失败，898/928只断言inactive。删除Ready失效hook仍可通过；第13节真实Montage startup测试是另一入口。<br>证据：CP-64/65/66/70/73/74 | WaitEvent Ready失效测试假阳性；不能拿来关闭生产同步重入风险。 | 修改建议：第8节先建立可激活控制样本，再单变量注入失效并证明Ready入口抵达/锁清理；其他同类弱前置单独列白名单。<br>关闭验证：编译、VictimPresentation Automation；检查Front478–486、Backstab551–559换装前置及ExecutionLockIn385–398/8.1/8.2前置，保留已有有效第13节。<br>门禁：独立测试修复；不自动升级03C前置。 |

F14 的同类前置问题作为 FIX-TestExecution 的待冻结测试白名单：Front:478–486／Backstab:551–559 用非法 weapon/null pickup，不能证明 execution 换装门禁；ExecutionLockIn:385–398 的空 Controller 缺目标/配置，四个 false 不能单独证明 lock gate；其 8.1／8.2 未配置 StanceBreak，未证明 Ready hook 抵达。这些是验证缺口，不新增生产缺陷。已有 VictimPresentation 第 13 节真实 Montage_Play 同步取消、PlayerLockOn 的 playable root-track 恢复路径及 Front／Backstab 真实 StanceBreak 准入测试继续保留。

### 4.3 Ponytail 候选主表（24 项）

本表是复杂度候选唯一事实表；第 4.4 节单行总览由同一批数据生成。类别使用 `delete / stdlib / native / yagni / shrink`；无足够证据的 stdlib 替换不凑数。正确性 Findings 不强套复杂度标签。

“删除／提取合并／简化”是有证据支持的技术裁决，**不等于本轮源码写入授权或自动接受实施排期**；只有 A／B 的已接受核心范围进入当前 03C 前路线，其余建议在独立切片接受时冻结白名单。“有条件延期”逐项给出具体关闭触发，不把未读源码伪装为延期。每项锚点同时列出实现与直接消费者，细节引用 CP。

| ID／类别 | 实现／消费者／证据 | 削减内容与最小替换 | 裁决／实施归属 | 净行数估算 | 风险／验证与关闭触发 |
|---|---|---|---|---:|---|
| **CAND-SHRINK-A**<br>shrink | `Source/PolyQuest/Private/AbilitySystem/Abilities/ChargedAttackAbility.cpp:989`<br>`Source/PolyQuest/Private/AbilitySystem/Abilities/SprintAttackAbility.cpp:642`<br>`Source/PolyQuest/Private/AbilitySystem/Abilities/PlayerMeleeSkillAbility.cpp:775`<br>`Source/PolyQuest/Private/AbilitySystem/Abilities/DodgeAbility.cpp:445`<br>`Source/PolyQuest/Private/AbilitySystem/Abilities/BowDrawFireAbility.cpp:800`<br>CP-07/08/49 | 收敛重复的 BindRateWindow 绑定样板；复用既有 MontageRateWindowLifecycle 边界；4处类型归一相同，Bow保留结束状态差异 | 提取合并；SHRINK-A（已接受） | 待定；扣 FIX1 重叠 | 风险：高；不合并11套Context/动作时序。验证／触发：编译、Player/Enemy RateWindow、取消/重入/同资产双live及适用PIE |
| **CAND-SHRINK-B**<br>delete | `Source/PolyQuest/Public/AbilitySystem/Abilities/EnemyVictimExecutionAbility.h:110`<br>`Source/PolyQuest/Public/AbilitySystem/Abilities/EnemyLaunchReactionAbility.h:78`<br>`Source/PolyQuest/Public/AbilitySystem/Abilities/DodgeAbility.h:70`<br>`Source/PolyQuest/Public/AbilitySystem/Abilities/ChargedAttackAbility.h:294`<br>CP-76 | 删除4头文件中的11项无消费者测试getter；删除第4.1节精确列举的11行；保留字段/setter及其他有效getter | 删除；SHRINK-B（已接受） | 11 | 风险：低；非反射宏内接口。验证／触发：源码消费面复核、编译和受影响Automation，无PIE/readback需求 |
| **CAND-CONFIG-DUP-01**<br>shrink | `Config/DefaultEngine.ini:13`<br>`Config/DefaultEngine.ini:38`<br>CP-03 | 移除两组同值标量配置的重复键；每组保留一行；不改 -/+AxisConfig | 简化；配置小片（建议，未排期） | 2 | 风险：低；正式实施须单独批准Config路径。验证／触发：文本键值对账、启动配置读取按实施计划 |
| **CAND-ATTR-CLAMP-01**<br>shrink | `Source/PolyQuest/Private/AbilitySystem/CharacterAttributeSet.cpp:40`<br>`Source/PolyQuest/Private/AbilitySystem/CharacterAttributeSet.cpp:68`<br>CP-04 | 收敛两Hook中的相同属性Clamp政策；本类私有纯校验函数；保留两个UE Hook和死亡/pending语义 | 提取合并；属性规则片（建议） | 待定 | 风险：中；GAS执行次序不能改变。验证／触发：编译、Attribute/HitReaction/DeathPending Automation |
| **CAND-WARP-DUP-01**<br>shrink | `Source/PolyQuest/Private/AbilitySystem/Abilities/ChargedAttackAbility.cpp:647`<br>`Source/PolyQuest/Private/AbilitySystem/Abilities/SprintAttackAbility.cpp:455`<br>`Source/PolyQuest/Private/AbilitySystem/Abilities/PlayerMeleeSkillAbility.cpp:588`<br>CP-07/08/61 | 收敛3处各154行的MotionWarp同步流程；诊断部分对接既有REC；整体流程若另获接受再复用MeleeMotionWarping，保留各Ability快照和时序 | 有条件延期；REC-03A7-01（原边界保留；整体合并未接受） | 待定；462物理行不是净收益 | 风险：高；捕获机会/Controller/ground前置不能变。验证／触发：原REC禁止合并快照/判定/生命周期；整体流程须独立接受新边界后，编译、PlayerMeleeMotionWarping及四动作PIE |
| **CAND-REACTION-YAW-01**<br>shrink | `Source/PolyQuest/Private/AbilitySystem/Abilities/EnemyLaunchReactionAbility.cpp:485`<br>`Source/PolyQuest/Private/AbilitySystem/Abilities/PlayerLaunchReactionAbility.cpp:456`<br>CP-10 | 收敛双方相同的44行facing yaw数学；在既有Reaction私有helper保留一份finite/几何规则 | 提取合并；Reaction纯规则片（建议） | 待定 | 风险：中；各自落地/取消/Deferral不合并。验证／触发：编译、双方LaunchRootMotion与HitReaction方向矩阵 |
| **CAND-EXEC-SNAP-01**<br>shrink | `Source/PolyQuest/Private/AbilitySystem/Abilities/PlayerFrontExecutionAbility.cpp:397`<br>`Source/PolyQuest/Private/AbilitySystem/Abilities/PlayerBackstabExecutionAbility.cpp:439`<br>CP-11/65/66 | 收敛前后处决相同的snap施加与rollback；既有ExecutionSnapAlignment私有实现；保留side和两端准入差异 | 提取合并；Execution Snap片（建议） | 待定 | 风险：高；碰撞ignore/sweep/rollback/Context清理时机。验证／触发：编译、Front/Backstab阻挡与取消回归，适用PIE |
| **CAND-BACKSTAB-HANDLE-01**<br>delete | `Source/PolyQuest/Public/AbilitySystem/Abilities/PlayerBackstabExecutionAbility.h:211`<br>CP-11 | 删除背刺未绑定的TargetStunnedTagDelegateHandle；仅删该非反射私有字段；Front同名字段保留 | 删除；Execution字段小片（建议） | 1 | 风险：低；不连带Front绑定。验证／触发：编译、Front/Backstab Automation |
| **CAND-STANCE-COMPAT-01**<br>shrink | `Source/PolyQuest/Private/AbilitySystem/Abilities/PlayerBackstabExecutionAbility.cpp:26`<br>`Source/PolyQuest/Private/AbilitySystem/Abilities/EnemyVictimExecutionAbility.cpp:21`<br>CP-12/65/74 | 收敛两处StanceBreakCompatibility纯规则；既有Execution私有helper；Victim先扣自身Stunned贡献仍由调用端负责 | 提取合并；Execution兼容规则片（建议） | 待定 | 风险：中；请求与已激活Victim计数不同。验证／触发：编译、真实StanceBreak握手/额外tag/多spec矩阵 |
| **CAND-NOTIFY-DUP-01**<br>shrink | `Source/PolyQuest/Private/Animation/Combat/AnimNotifyState_ActionWindows.cpp:13`<br>`Source/PolyQuest/Private/Animation/Combat/AnimNotifyState_ActionWindows.cpp:39`<br>CP-14 | 合并只有EventMagnitude赋值差异的事件发送函数；同cpp内部函数带默认magnitude；保留所有反射Notify与payload | 提取合并；Notify片（建议） | 20–25 | 风险：中；OptionalObject/2及标签必须一致。验证／触发：编译、ActionWindow/RateWindow测试和必要Notify readback |
| **CAND-LAUNCH-SMOOTH-01**<br>yagni | `Source/PolyQuest/Private/AbilitySystem/Abilities/LaunchFacingSmoothingState.cpp:1`<br>`Source/PolyQuest/Private/AbilitySystem/Tasks/AbilityTask_TurnToFacing.cpp:1`<br>`Source/PolyQuest/Private/Animation/Combat/AnimNotify_ReactionLaunchCommit.cpp:1`<br>`Source/PolyQuest/Private/Tests/LaunchFacingSmoothingAutomationTests.cpp:1`<br>`Source/PolyQuest/Private/Tests/TestLaunchFacingSmoothingAbility.h:1`<br>CP-02/36/51/52/55/61 | 裁决旧Launch平滑/Commit退役闭包；先查资产Notify/Tag/Blueprint，再迁移仍有效的测试宿主；通过后独立退役 | 有条件延期；RET-LaunchClosure（条件建议） | 待定；9文件874物理行（生产357/测试517），不得当净删量 | 风险：高；反射/资产及MotionWarping Guard fixture仍有消费者。验证／触发：readback旧Notify/Tag所有资产引用、迁移MotionWarping1667–1747，保留新Launch/HitReaction测试 |
| **CAND-REACTION-LEGACY-01**<br>yagni | `Source/PolyQuest/Private/Combat/Reaction/HitReactionImpactResolver.cpp:88`<br>`Source/PolyQuest/Private/Tests/HitReactionAutomationTests.cpp:705`<br>CP-24/55 | 随Launch退役闭包裁决旧速度构建接口和705–928矩阵；与LAUNCH-SMOOTH-01共同核价；保留930行起当前四向/impact矩阵 | 有条件延期；RET-LaunchClosure（同一归属） | 待定；子项，不与父项重复加总 | 风险：高；LaunchFacingSmoothingState仍是原生消费者。验证／触发：先完成父项readback/消费者迁移，再对账速度接口和测试目的 |
| **CAND-PLAYER-REACTION-DUP-01**<br>shrink | `Source/PolyQuest/Private/Character/Player/PlayerCharacter.cpp:2913`<br>CP-19 | 收敛三段仅EventTag不同的反应派发；同函数先选Tag，再构造一次payload发送 | 简化；Player反应片（建议） | 15–22 | 风险：中；保留None/Invalid/Stunned与Context。验证／触发：编译、Player HitReaction/Launch方向和Deferral |
| **CAND-MELEE-VALIDATION-DUP-01**<br>shrink | `Source/PolyQuest/Private/Combat/Equipment/MeleeWeaponDefinition.cpp:180`<br>`Source/PolyQuest/Private/Combat/Equipment/MeleeWeaponDefinition.cpp:278`<br>CP-20 | 收敛重复socket与trace数值校验；同cpp私有函数；玩家完整准入与Enemy仅几何准入分开 | 提取合并；Weapon校验片（建议） | 25–35 | 风险：中；socket可选性和错误信息。验证／触发：编译、Definition/Equipment/EnemyAttackSet/多Trace测试 |
| **CAND-NATIVE-ATTACH-01**<br>native | `Source/PolyQuest/Private/Combat/Melee/MeleeTraceSourceComponent.cpp:12`<br>CP-23 | 替换手写祖先链遍历；使用UE5.8 USceneComponent::IsAttachedTo；保留调用端非空门槛 | 简化；Melee原生替换片（建议） | 10–14 | 风险：低；本机SceneComponent.cpp2794–2807确认语义。验证／触发：编译、直接/深层/不相关祖先及多Trace |
| **CAND-FLOOR-HIDE-DUP-01**<br>shrink | `Source/PolyQuest/Private/Environment/FloorVolume.cpp:294`<br>`Source/PolyQuest/Private/Environment/FloorVolume.cpp:313`<br>`Source/PolyQuest/Private/Environment/FloorVolume.cpp:346`<br>CP-25 | 收敛三段结构Actor显示遍历；同类私有函数；激活立即显示、停用fade结束隐藏各时机保留 | 有条件延期；Environment片（建议） | 8–12；待F09后扣重 | 风险：中；F09尚未解决隐藏所有权。验证／触发：先关闭FIX-FloorVisibility；再核验原本hidden/销毁与fade |
| **CAND-UI-CURVES-01**<br>shrink | `Source/PolyQuest/Private/UI/EnemyHealthBarWidget.cpp:133`<br>`Source/PolyQuest/Private/UI/PlayerVitalHUDWidget.cpp:508`<br>`Source/PolyQuest/Private/UI/EnemyHealthBarWidget.cpp:224`<br>`Source/PolyQuest/Private/UI/PlayerVitalHUDWidget.cpp:210`<br>CP-27 | 收敛两个widget相同shake/flash数学；私有纯函数；各widget保留timer/绑定，不造Widget基类 | 有条件延期；UI数学片（建议） | 20–30；待F10后扣重 | 风险：中；F10终止恢复修复优先。验证／触发：先关闭FIX-UIFlash；再验证两个widget全部flash边界 |
| **CAND-TEST-EMPTY-01**<br>delete | `Source/PolyQuest/Private/Tests/TestPreparedSkillCooldownFixtures.h:45`<br>`Source/PolyQuest/Private/Tests/TestPreparedSkillCooldownFixtures.cpp:68`<br>CP-29 | 删除空且未调用的SetTestCooldownDuration；删一声明和四行实现，不动CooldownGE | 删除；SHRINK-B附加候选（需接受） | 5 | 风险：低；普通非反射接口。验证／触发：编译、SkillBar与PreparedCooldown Automation |
| **CAND-TEST-WORLDTICK-01**<br>shrink | `Source/PolyQuest/Private/Tests/SkillBarHudAutomationTests.cpp:27`<br>`Source/PolyQuest/Private/Tests/PlayerExhaustionAutomationTests.cpp:20`<br>`Source/PolyQuest/Private/Tests/ExecutionImpactFeedbackAutomationTests.cpp:49`<br>CP-34/42/56/68 | 收敛跨测试World Tick与GFrameCounter样板；复用CombatAutomationFixture测试支撑；0.05/0.1步长与prime/BeginPlay差异保留 | 提取合并；测试Tick支撑片（建议） | 待定 | 风险：中；误改步长会改变Timer/physics行为。验证／触发：编译、每个迁移suite；保留前置和所有原有效断言 |
| **CAND-TEST-MONTAGE-FIXTURE-01**<br>shrink | `Source/PolyQuest/Private/Tests/PlayerMontageRateWindowAutomationTests.cpp:69`<br>`Source/PolyQuest/Private/Tests/EnemyMontageRateWindowAutomationTests.cpp:58`<br>`Source/PolyQuest/Private/Tests/ExecutionReleaseOutcomesAutomationTests.cpp:67`<br>`Source/PolyQuest/Private/Tests/ExecutionVictimPresentationAutomationTests.cpp:70`<br>`Source/PolyQuest/Private/Tests/ExecutionVictimRootMotionAutomationTests.cpp:63`<br>CP-48/59/61/71/75 | 收敛测试Montage构造的重复部分；分别复用metadata和playable root-track工厂；既有fixture容纳，保留skeleton/片长/名称参数 | 提取合并；测试Montage支撑片（建议） | 待定 | 风险：高；先处理F14相关验证前置；不将metadata当可播放。验证／触发：编译、RateWindow/Launch/Execution/LockOn相关suite |
| **CAND-STANCE-TEST-NOOP-01**<br>delete | `Source/PolyQuest/Private/Tests/EnemyStanceBreakRateWindowAutomationTests.cpp:524`<br>CP-45/66/76 | 删除524–542只AddTag/HasTag的伪Front资格段；真实资格继续由Front352–366和Backstab387–399覆盖 | 删除；SHRINK-B附加候选（需接受） | 19 | 风险：低；不动真实资格测试。验证／触发：编译、StanceBreak/Front/Backstab Automation |
| **CAND-EXEC-COMPAT-01**<br>yagni | `Source/PolyQuest/Public/Combat/Execution/ExecutionLockContext.h:128`<br>`Source/PolyQuest/Private/Combat/Execution/ExecutionLockContext.cpp:269`<br>`Source/PolyQuest/Private/Tests/ExecutionLethalRecoveryAutomationTests.cpp:162`<br>CP-69/71/76 | 删除仅旧测试使用的三项B阶段finalization状态入口；迁移必要断言至Outcome API；ReleaseOutcomes196–210已有回滚/完成覆盖 | 简化；Execution旧native接口片（建议） | 待定；扣迁移/新断言 | 风险：中；不是零引用；普通非反射；保留Release/HitState事实。验证／触发：编译、LethalRecovery/ReleaseOutcomes，保留授权/回滚负例 |
| **CAND-TEST-DROP-BOUNDS-01**<br>shrink | `Source/PolyQuest/Private/Tests/WeaponEquipmentComponentAutomationTests.cpp:529`<br>`Source/PolyQuest/Private/Tests/WeaponEquipmentComponentAutomationTests.cpp:560`<br>`Source/PolyQuest/Private/Tests/WeaponEquipmentComponentAutomationTests.cpp:614`<br>CP-62 | 收敛3处八角变换求最低点测试算法；本文件局部纯函数；三种transform和2cm断言保留，不复用生产结果作expected | 提取合并；测试局部纯函数片（建议） | 30–36 | 风险：低；独立expected计算仍存在。验证／触发：编译、Equipment.TransactionMatrix |
| **CAND-TEST-EXEC-FIXTURE-01**<br>shrink | `Source/PolyQuest/Private/Tests/BackstabExecutionAutomationTests.cpp:61`<br>`Source/PolyQuest/Private/Tests/ExecutionImpactFeedbackAutomationTests.cpp:64`<br>`Source/PolyQuest/Private/Tests/ExecutionLethalRecoveryAutomationTests.cpp:51`<br>`Source/PolyQuest/Private/Tests/ExecutionReleaseOutcomesAutomationTests.cpp:99`<br>`Source/PolyQuest/Private/Tests/ExecutionVictimPresentationAutomationTests.cpp:111`<br>`Source/PolyQuest/Private/Tests/ExecutionVictimRootMotionAutomationTests.cpp:95`<br>CP-65/68/69/72/74/75 | 收敛6处相同StanceBreak激活测试支撑；复用已有CombatAutomationFixture并精确保留CDO快照/恢复；synthetic ActiveCount场景保持单独 | 提取合并；测试Execution支撑片（建议） | 待定 | 风险：中；不能掩盖F14前置或混淆真实/人工激活。验证／触发：编译、6套受影响Execution测试，激活后明确Active和Tag计数 |

保留裁决：各动作 Context／Token、Light 跨 entry 快照、Charged 暂停、Bow Section 和 Dodge 重触发语义；Bow Ability 内阶段状态；Launch 当前有效阶段、两端独立落地／取消／Deferral；Front／Backstab 准入差异；真实边界／多断言测试；旧 Enemy fixture 仍消费的 BladeTrace fallback。没有批准删除的旧 Loadout／Physics 分支，不设退役配额，不建公共 Ability 基类或通用测试框架。

### 4.4 Ponytail 单行总览（与第 4.3 节同源）

按已接受项优先，其余先已量化、再待定、最后条件项展示；同类优先低风险，行数不压过契约风险。括号保留候选 ID，多个实现锚点见主表。

```text
delete: 删除4头文件中的11项无消费者测试getter. 删除第4.1节精确列举的11行；保留字段/setter及其他有效getter. [Source/PolyQuest/Public/AbilitySystem/Abilities/EnemyVictimExecutionAbility.h:110]（CAND-SHRINK-B；预估净削减约 11 行）
shrink: 收敛重复的 BindRateWindow 绑定样板. 复用既有 MontageRateWindowLifecycle 边界；4处类型归一相同，Bow保留结束状态差异. [Source/PolyQuest/Private/AbilitySystem/Abilities/ChargedAttackAbility.cpp:989]（CAND-SHRINK-A；估算待定；扣 FIX1 重叠）
shrink: 移除两组同值标量配置的重复键. 每组保留一行；不改 -/+AxisConfig. [Config/DefaultEngine.ini:13]（CAND-CONFIG-DUP-01；预估净削减约 2 行）
delete: 删除背刺未绑定的TargetStunnedTagDelegateHandle. 仅删该非反射私有字段；Front同名字段保留. [Source/PolyQuest/Public/AbilitySystem/Abilities/PlayerBackstabExecutionAbility.h:211]（CAND-BACKSTAB-HANDLE-01；预估净削减约 1 行）
native: 替换手写祖先链遍历. 使用UE5.8 USceneComponent::IsAttachedTo；保留调用端非空门槛. [Source/PolyQuest/Private/Combat/Melee/MeleeTraceSourceComponent.cpp:12]（CAND-NATIVE-ATTACH-01；预估净削减约 10–14 行）
delete: 删除空且未调用的SetTestCooldownDuration. 删一声明和四行实现，不动CooldownGE. [Source/PolyQuest/Private/Tests/TestPreparedSkillCooldownFixtures.h:45]（CAND-TEST-EMPTY-01；预估净削减约 5 行）
delete: 删除524–542只AddTag/HasTag的伪Front资格段. 真实资格继续由Front352–366和Backstab387–399覆盖. [Source/PolyQuest/Private/Tests/EnemyStanceBreakRateWindowAutomationTests.cpp:524]（CAND-STANCE-TEST-NOOP-01；预估净削减约 19 行）
shrink: 收敛3处八角变换求最低点测试算法. 本文件局部纯函数；三种transform和2cm断言保留，不复用生产结果作expected. [Source/PolyQuest/Private/Tests/WeaponEquipmentComponentAutomationTests.cpp:529]（CAND-TEST-DROP-BOUNDS-01；预估净削减约 30–36 行）
shrink: 合并只有EventMagnitude赋值差异的事件发送函数. 同cpp内部函数带默认magnitude；保留所有反射Notify与payload. [Source/PolyQuest/Private/Animation/Combat/AnimNotifyState_ActionWindows.cpp:13]（CAND-NOTIFY-DUP-01；预估净削减约 20–25 行）
shrink: 收敛三段仅EventTag不同的反应派发. 同函数先选Tag，再构造一次payload发送. [Source/PolyQuest/Private/Character/Player/PlayerCharacter.cpp:2913]（CAND-PLAYER-REACTION-DUP-01；预估净削减约 15–22 行）
shrink: 收敛重复socket与trace数值校验. 同cpp私有函数；玩家完整准入与Enemy仅几何准入分开. [Source/PolyQuest/Private/Combat/Equipment/MeleeWeaponDefinition.cpp:180]（CAND-MELEE-VALIDATION-DUP-01；预估净削减约 25–35 行）
shrink: 收敛两Hook中的相同属性Clamp政策. 本类私有纯校验函数；保留两个UE Hook和死亡/pending语义. [Source/PolyQuest/Private/AbilitySystem/CharacterAttributeSet.cpp:40]（CAND-ATTR-CLAMP-01；估算待定）
shrink: 收敛双方相同的44行facing yaw数学. 在既有Reaction私有helper保留一份finite/几何规则. [Source/PolyQuest/Private/AbilitySystem/Abilities/EnemyLaunchReactionAbility.cpp:485]（CAND-REACTION-YAW-01；估算待定）
shrink: 收敛两处StanceBreakCompatibility纯规则. 既有Execution私有helper；Victim先扣自身Stunned贡献仍由调用端负责. [Source/PolyQuest/Private/AbilitySystem/Abilities/PlayerBackstabExecutionAbility.cpp:26]（CAND-STANCE-COMPAT-01；估算待定）
shrink: 收敛跨测试World Tick与GFrameCounter样板. 复用CombatAutomationFixture测试支撑；0.05/0.1步长与prime/BeginPlay差异保留. [Source/PolyQuest/Private/Tests/SkillBarHudAutomationTests.cpp:27]（CAND-TEST-WORLDTICK-01；估算待定）
yagni: 删除仅旧测试使用的三项B阶段finalization状态入口. 迁移必要断言至Outcome API；ReleaseOutcomes196–210已有回滚/完成覆盖. [Source/PolyQuest/Public/Combat/Execution/ExecutionLockContext.h:128]（CAND-EXEC-COMPAT-01；估算待定；扣迁移/新断言）
shrink: 收敛6处相同StanceBreak激活测试支撑. 复用已有CombatAutomationFixture并精确保留CDO快照/恢复；synthetic ActiveCount场景保持单独. [Source/PolyQuest/Private/Tests/BackstabExecutionAutomationTests.cpp:61]（CAND-TEST-EXEC-FIXTURE-01；估算待定）
shrink: 收敛前后处决相同的snap施加与rollback. 既有ExecutionSnapAlignment私有实现；保留side和两端准入差异. [Source/PolyQuest/Private/AbilitySystem/Abilities/PlayerFrontExecutionAbility.cpp:397]（CAND-EXEC-SNAP-01；估算待定）
shrink: 收敛测试Montage构造的重复部分. 分别复用metadata和playable root-track工厂；既有fixture容纳，保留skeleton/片长/名称参数. [Source/PolyQuest/Private/Tests/PlayerMontageRateWindowAutomationTests.cpp:69]（CAND-TEST-MONTAGE-FIXTURE-01；估算待定）
shrink: 收敛三段结构Actor显示遍历. 同类私有函数；激活立即显示、停用fade结束隐藏各时机保留. [Source/PolyQuest/Private/Environment/FloorVolume.cpp:294]（CAND-FLOOR-HIDE-DUP-01；预估净削减约 8–12；待F09后扣重）
shrink: 收敛两个widget相同shake/flash数学. 私有纯函数；各widget保留timer/绑定，不造Widget基类. [Source/PolyQuest/Private/UI/EnemyHealthBarWidget.cpp:133]（CAND-UI-CURVES-01；预估净削减约 20–30；待F10后扣重）
shrink: 收敛3处各154行的MotionWarp同步流程. 诊断部分对接既有REC；整体流程若另获接受再复用MeleeMotionWarping，保留各Ability快照和时序. [Source/PolyQuest/Private/AbilitySystem/Abilities/ChargedAttackAbility.cpp:647]（CAND-WARP-DUP-01；估算待定；462物理行不是净收益）
yagni: 裁决旧Launch平滑/Commit退役闭包. 先查资产Notify/Tag/Blueprint，再迁移仍有效的测试宿主；通过后独立退役. [Source/PolyQuest/Private/AbilitySystem/Abilities/LaunchFacingSmoothingState.cpp:1]（CAND-LAUNCH-SMOOTH-01；估算待定；9文件874物理行（生产357/测试517），不得当净删量）
yagni: 随Launch退役闭包裁决旧速度构建接口和705–928矩阵. 与LAUNCH-SMOOTH-01共同核价；保留930行起当前四向/impact矩阵. [Source/PolyQuest/Private/Combat/Reaction/HitReactionImpactResolver.cpp:88]（CAND-REACTION-LEGACY-01；估算待定；子项，不与父项重复加总）
```

### 4.5 收益统计、无用接口与依赖

- **已接受可计量：11 行**（CAND-SHRINK-B）。CAND-SHRINK-A 待 FIX1 落地后扣重核价。
- **10 项无附加证据条件的可计量候选合计 138–170 行**，包含上面的 11 行和 9 项未接受建议；这是部分候选合计，不是全项目完整收益，也不是已批准实施量。
- **条件可计量 28–42 行**：Floor 8–12、UI 20–30，先关闭 F09／F10 并扣重，未计入 138–170。
- 其余 **12 项净收益待定**：包括 A、旧 Launch 父子闭包和需迁移测试的候选。旧闭包 9 文件共 874 物理行（357 生产＋517 测试）；现有消费者迁移、替代代码与必要测试增量未扣除，不能称 874 或 987 行净删除。REACTION-LEGACY 与 LAUNCH-SMOOTH 为同一闭包，不叠加。
- 无用／待退役接口单列：11 getter；Backstab 未用 delegate handle；空 SetTestCooldownDuration；Execution 三项仅旧测试消费的 finalization 入口（须迁移有效断言）；旧 Launch 速度／Task／Notify／Tag 接口（须完成原生与资产闭包）。getter、include 和文件数均不是依赖数量。
- **已证实可移除模块／插件／包声明：0**；这不等于已经证明所有声明必需。当前插件运行和资产消费证据不足，不批准依赖删除。
- Source 内 `ponytail:` 注释 **0 处**；43 个 `*AutomationTests.cpp` 只是命名统计，Tests 审阅覆盖仍为全部 72 文件。

```text
net: -138..-170 lines, -0 deps possible.
范围：仅10项已量化候选，包含未接受建议；另有条件28–42行及12项待定，不是全项目完整估算。
```

净削减考虑旧实现删除、替换代码、调用迁移与必要测试增量；互斥方案、父子项和 FIX1／SHRINK 同段只计算一次。删行不是验收指标，严禁删断言、扩大 bypass 或移除有效生命周期差异换取收益。

### 4.6 后续切片、有限终点与 03C 门禁

**已接受顺序保持 FULL-AUDIT（本次完成）→ FIX1 → FIX2 → 有限项目级 SHRINK → 03C。** 下一任务由 Main 制定 FIX1 独立计划，实施按项目 manual/out-of-band Gemini 路线交接；本轮不实施、不派发，也不额外设置 Fresh Review。

- FIX1 处理 F01 和共享实例授权；FIX2 处理 F02 来袭方向。每片冻结输入基线、文件／符号白名单、运行契约和验证。
- 已接受 SHRINK 的有限核心是 A 的真实绑定共享及全部适用消费者裁决、B 的 11 getter 逐项清理；审计范围已扩展至全项目 24 候选，额外建议须明确接受后才增加实施片。完成 A／B 并将其余项保留为有触发的独立建议，可按已接受范围收口；不能只删 getter 或只做 RateWindow 一项代替整个接受范围。
- F03–F14 各归第 4.2 节独立 FIX，同根消费者同片；纯测试修复、生产生命周期抽取、配置和资产迁移分别冻结范围。F04 建议作为后续修复中最高优先级构建问题，必须在 Shipping/Test 构建或打包前关闭；不擅自改变已接受 FIX1→FIX2 顺序。
- SHRINK-B 的空 setter／伪资格段是附加建议，当前不自动纳入；跨文件 Tick／Montage／Execution fixture 提取独立成片。先解决相应 F12／F14 验证问题，保留 metadata／playable 及真实／synthetic 前置区别。
- MotionWarp 候选对接既有 REC-03A7-01，不重建同根债务；原 REC 禁止合并快照／判定／生命周期，整体同步流程提取尚未接受，必须先独立接受新边界，不能借审计建议放宽既定契约；RET-LaunchClosure 只在独立接受、资产 readback 和有效测试消费者迁移完成后才可退役。其他未接受建议在各模块下一次有界维护或明确接受时复核，不延长本次审计。

**03C 尚不准入。** FIX2 是直接运行时接缝门禁；FIX1 与已接受 SHRINK 是排期前置。F03–F14 的具体影响见主表和 ROADMAP 唯一风险项：普通清理／额外建议不自动成为前置；F04 约束发布构建，F08 约束 BP 动态阵营采用，F13／F14 等不能继续被引用为已证明对应生产契约。若03C实际采用相关路径，独立计划按证据处理，不能用“仅P0/P1才能阻塞”或“所有审计项都阻塞”替代契约分析。

## 5. 验证矩阵与结束条件

| 影响面 | 审计检查 | 后续实施验证 |
|---|---|---|
| 全覆盖／检查点 | 文件及区段对账、继承证据边界、无遗漏 | 按实际修改片验证 |
| 授权／窗口／委托 | 对照生命周期与现有断言 | 编译、Focused Automation；取消、同步重入、同资产替换、旧 Context、重复清理及适用 PIE |
| 投射物／防御 | F02 反例、来源归属、近战契约 | 射手移动、玩家转向、前后弧、体力边界、退化方向、来源失效及近战／处决回归 |
| UI／环境／交互 | 数据所有权、绑定／解绑、失效对象和测试覆盖 | 对应重绑定、销毁、状态与必要呈现验证 |
| 测试清理／共享 fixture | 消费者、前置、清理及断言语义 | 编译及受影响 Automation，保留原有效断言；纯 getter 删除无需 PIE |
| 反射候选 | 明确资产入口与证据缺口 | 对应 readback 后才能批准删除 |
| 文档／WIP | 白名单、归档追加、引用及 git diff --check | 不将文档检查当成运行证明 |

### 计划落盘完成条件

三份文档职责、完整覆盖清单、分批规则、继承台账、证据标准、阶段归属和验证矩阵明确；正式写入后通过文档差异检查。此时 FULL-AUDIT 仍未完成。该完成态已在 d99ddcd 提交；用户现已授权 Main 继续实际审计，按下列全项目完成条件收口。

### 全项目审计完成条件

1. 当前全部自有源码、测试及外围清单完成对账，每个文件均有完整静态审阅记录；继承项有确切证据，未读部分不能作为延期项蒙混收口。
2. 所有覆盖单元及跨模块接缝都有结论；动态资产证据缺口与源码覆盖缺口明确分开。
3. 每条 Finding／候选都有裁决、验证、唯一归属及实施分组；H6 编号、11 项 getter 和历史收据完整保留。
4. Ponytail 总览与主台账一致；收益估算说明范围、重叠和未知项，不以净行数代替安全与维护收益。
5. 已接受 SHRINK 清单有明确终点；所有接受延期的风险在 ROADMAP 有唯一记录和关闭触发。
6. 除三份文档外无本阶段新增修改；已有 WIP 和暂存状态未被改变，归档仅追加，文档检查通过。
7. 分别报告“全项目静态审计是否完成”和“03C 是否准入”，明确未执行编译、Automation、Editor／PIE及 Fresh Review。

达到完成条件即停止探索；本轮按既有批准完成三文档提交，不自动启动修复、清理或推送。完成态与证据边界见第 8 节。

## 6. 逐文件覆盖记录与外围清单（230/230）

本清单与当前在盘 Source 路径逐一对账，路径相对项目根目录。**230/230 文件已完成全部区段静态审阅**；每行记录读取方式、证据、直接消费者／测试及结论，分批历史见 CP-03～CP-76。H6 局部继承未用于跳过同文件其他路径；动态资产 readback 缺口独立保留，不计为源码漏审，也不作为删除批准。

### 模块与构建入口（5 文件）

| 文件 | 总行数 | 状态／已完成区段 | 审阅方式、证据、消费者／测试、结论与缺口 |
|---|---:|---|---|
| Source/PolyQuest.Target.cs | 15 | 已核验／1-15 | CP-03 构建入口与外围配置；完整源码／未漂移 Target 继承速验；uproject 的唯一 PolyQuest Runtime 注册与 Game/Editor Target 对齐；无新正确性缺陷；依赖删除不以声明列表推断，后续全源码消费面对账 |
| Source/PolyQuest/PolyQuest.Build.cs | 45 | 已核验／1-45 | CP-03 构建入口与外围配置；完整源码／未漂移 Target 继承速验；uproject 的唯一 PolyQuest Runtime 注册与 Game/Editor Target 对齐；无新正确性缺陷；依赖删除不以声明列表推断，后续全源码消费面对账 |
| Source/PolyQuest/PolyQuest.cpp | 8 | 已核验／1-8 | CP-03 构建入口与外围配置；完整源码／未漂移 Target 继承速验；uproject 的唯一 PolyQuest Runtime 注册与 Game/Editor Target 对齐；无新正确性缺陷；依赖删除不以声明列表推断，后续全源码消费面对账 |
| Source/PolyQuest/PolyQuest.h | 8 | 已核验／1-8 | CP-03 构建入口与外围配置；完整源码／未漂移 Target 继承速验；uproject 的唯一 PolyQuest Runtime 注册与 Game/Editor Target 对齐；无新正确性缺陷；依赖删除不以声明列表推断，后续全源码消费面对账 |
| Source/PolyQuestEditor.Target.cs | 15 | 已核验／1-15 | CP-03 构建入口与外围配置；完整源码／未漂移 Target 继承速验；uproject 的唯一 PolyQuest Runtime 注册与 Game/Editor Target 对齐；无新正确性缺陷；依赖删除不以声明列表推断，后续全源码消费面对账 |

### AbilitySystem（60 文件）

| 文件 | 总行数 | 状态／已完成区段 | 审阅方式、证据、消费者／测试、结论与缺口 |
|---|---:|---|---|
| Source/PolyQuest/Private/AbilitySystem/Abilities/BowDrawFireAbility.cpp | 919 | 已核验／1-919 | CP-08 MeleeSkill／Bow／Dodge；完整头源；GAS 内部 Draw/Hold/Release 阶段、输入早释放、Notify 单发、装备 socket/瞄准/投射物请求、移动 GE 和 aim requester 收敛清理；内部阶段枚举不构成第二套玩法状态机；F02 接缝继承。 |
| Source/PolyQuest/Private/AbilitySystem/Abilities/ChargedAttackAbility.cpp | 1108 | 已核验／1-1108 | CP-07 Light／Charged／SprintAttack；全文；ReleaseHandoff、hold pause/cancel latch、释放时成本和伤害/Poise、Niagara/Delay 清理；SHRINK-A、11 getter 及 CAND-WARP-DUP-01，资产消费者不得零引用删除。 |
| Source/PolyQuest/Private/AbilitySystem/Abilities/DodgeAbility.cpp | 564 | 已核验／1-564 | CP-08 MeleeSkill／Bow／Dodge；完整头源；落地/取消窗口/重触发门槛、无敌 GE Handle、取消范围、Sequence 兼容与实例授权；4 getter 仍待消费表核验，状态字段/生产 Tag 保留。 |
| Source/PolyQuest/Private/AbilitySystem/Abilities/EnemyHitReactionAbility.cpp | 458 | 已核验／1-458 | CP-10 Enemy 受击与双方 Launch；全文；四向 Big/ledge/Falling/AI Stop，当前 RateWindow 仍 GetMontageInstanceForID 存活授权→H6-F01 同根 FIX1；不更改非致死/霸体契约。 |
| Source/PolyQuest/Private/AbilitySystem/Abilities/EnemyLaunchReactionAbility.cpp | 710 | 已核验／1-710 | CP-10 Enemy 受击与双方 Launch；全文含 CDO test 分支；地面 RootMotion 唯一阶段、即时 facing/ledge、结束后恢复/中止 StanceBreak deferral；RateWindow 同根 F01，getter 属 SHRINK-B。 |
| Source/PolyQuest/Private/AbilitySystem/Abilities/EnemyMeleeAbility.cpp | 584 | 已核验／1-584 | CP-05 Enemy Melee／StanceBreak；完整头源；AI pending/Profile/Commit/Task/Trace/HyperArmor/Cooldown 单一结束链；同资产旧实例授权缺陷 H6-F01；共享校验归 FIX1 |
| Source/PolyQuest/Private/AbilitySystem/Abilities/EnemySmallHitReactionAbility.cpp | 445 | 已核验／1-445 | CP-10 Enemy 受击与双方 Launch；全文；叠加/重触发、旧 Task 解绑、四向选择；RateWindow 同根 F01；保留小受击不取消移动/攻击。 |
| Source/PolyQuest/Private/AbilitySystem/Abilities/EnemyStanceBreakAbility.cpp | 432 | 已核验／1-432 | CP-05 Enemy Melee／StanceBreak；完整头源；Poise→延迟事件→Stunned/Cancel→恢复；保留 Context 身份与 Movement 交接；RateWindow 未绑定实例 ID，FIX1 独立核验其授权接入 |
| Source/PolyQuest/Private/AbilitySystem/Abilities/EnemyVictimExecutionAbility.cpp | 1572 | 已核验／1-1572 | CP-12 Victim 处决事务；完整头源；Front/Backstab 兼容性、授权 Hit scope、DeathPending/Outcome finalization、同步收据、真实 FindFloor、newborn instance 捕获/只停本实例、非致死恢复、旧 Context/RateWindow F01；非反射 getter 属 SHRINK-B。 |
| Source/PolyQuest/Private/AbilitySystem/Abilities/JumpAbility.cpp | 91 | 已核验／1-91 | CP-06 基础移动与 Primary 路由；完整头源；跳跃明确绕过正体力门槛、不延迟恢复；Sprint 空中速度 GE 在提交失败时回收，相关输入/耗尽测试待整合。 |
| Source/PolyQuest/Private/AbilitySystem/Abilities/LaunchFacingSmoothingState.cpp | 75 | 已核验／1-75 | CP-04 Attribute、Task 与窗口 helper；完整头源及此前未漂移声明；非普通生产消费，专属测试和外部 Notify/Tag 测试仍存在；条件退役 CAND-LAUNCH-SMOOTH-01，不宣称可直接删除 |
| Source/PolyQuest/Private/AbilitySystem/Abilities/LightAttackAbility.cpp | 1072 | 已核验／1-1072 | CP-07 Light／Charged／SprintAttack；全文含条件编译与测试 seam；唯一 Montage/每段身份、续段成本、输入/分支窗口、同步 End 不复活旧 Task；Light 的快照跨段复用，保留独立流程。 |
| Source/PolyQuest/Private/AbilitySystem/Abilities/MeleeTraceWindowLifecycle.cpp | 116 | 已核验／1-116 | CP-04 Attribute、Task 与窗口 helper；完整头源；共享 Task 的创建/激活/关闭与多源采样均已读；同步命中回调导致清理的疑点待直接取消链证实 |
| Source/PolyQuest/Private/AbilitySystem/Abilities/MeleeTraceWindowLifecycle.h | 44 | 已核验／1-44 | CP-04 Attribute、Task 与窗口 helper；完整头源；共享 Task 的创建/激活/关闭与多源采样均已读；同步命中回调导致清理的疑点待直接取消链证实 |
| Source/PolyQuest/Private/AbilitySystem/Abilities/MontageRateWindowLifecycle.cpp | 344 | 已核验／1-344 | CP-04 Attribute、Task 与窗口 helper；完整头源；Begin/End/Restore 身份/来源/Notify/速率检查，H6-F01 归 FIX1；测试 seam 是集合级辅助，不等于真实实例证明 |
| Source/PolyQuest/Private/AbilitySystem/Abilities/PlayerBackstabExecutionAbility.cpp | 1303 | 已核验／1-1303 | CP-11 前处决与背刺；完整头源/全部条件编译；装备/距离快照、前/背几何与 StanceBreak 差异、Commit→Victim 同步握手→Snap→Hit→VictimStart/Release 收据、token/解绑/幂等结束；H6-F04、CAND-EXEC-SNAP-01；背刺额外 CAND-BACKSTAB-HANDLE-01。 |
| Source/PolyQuest/Private/AbilitySystem/Abilities/PlayerBigHitReactionAbility.cpp | 278 | 已核验／1-278 | CP-09 玩家防御与轻重受击；完整头源；四向方向快照、HyperArmor/地面门槛、11类取消/阻止、ledge 保存/恢复及 Falling 清理；轻重反应差异合理。 |
| Source/PolyQuest/Private/AbilitySystem/Abilities/PlayerFrontExecutionAbility.cpp | 1281 | 已核验／1-1281 | CP-11 前处决与背刺；完整头源/全部条件编译；装备/距离快照、前/背几何与 StanceBreak 差异、Commit→Victim 同步握手→Snap→Hit→VictimStart/Release 收据、token/解绑/幂等结束；H6-F04、CAND-EXEC-SNAP-01；背刺额外 CAND-BACKSTAB-HANDLE-01。 |
| Source/PolyQuest/Private/AbilitySystem/Abilities/PlayerGuardAbility.cpp | 470 | 已核验／1-470 | CP-09 玩家防御与轻重受击；完整头源；持续防御确认/GE 事务回收、体力归零仍吸收当前击、GuardBreak/恢复意图、前弧与音效位置；保留持续防御独立权属。 |
| Source/PolyQuest/Private/AbilitySystem/Abilities/PlayerGuardBreakAbility.cpp | 204 | 已核验／1-204 | CP-09 玩家防御与轻重受击；完整头源；体力零触发、确认 Montage 后移动锁和动作取消、死亡不恢复；不按 Guard/Parry 表面相似合并生命周期。 |
| Source/PolyQuest/Private/AbilitySystem/Abilities/PlayerLaunchReactionAbility.cpp | 643 | 已核验／1-643 | CP-10 Enemy 受击与双方 Launch；全文含 test 分支；地面 knockdown、Dodge cancel 但不 Block Dodge、落地/角色失效/清理；与 Enemy 的纯 facing 几何重复候选，完整生命周期保留。 |
| Source/PolyQuest/Private/AbilitySystem/Abilities/PlayerMeleeSkillAbility.cpp | 894 | 已核验／1-894 | CP-08 MeleeSkill／Bow／Dodge；完整头源与 WITH_EDITOR/PostCDOCompiled；能力标签修复、Montage 确认后单次 cost/cooldown、回调激活护栏、统一清理；绑定/warp 与 Sprint/Charged 同构，纳入两候选直接消费者。 |
| Source/PolyQuest/Private/AbilitySystem/Abilities/PlayerParryAbility.cpp | 449 | 已核验／1-449 | CP-09 玩家防御与轻重受击；完整头源；成本与自然结束冷却分离、移动锁/外部 Falling 优先、Poise 反制/反馈；F03 重复 Begin 引发 ParryActive loose tag 计数残留，详检查点。 |
| Source/PolyQuest/Private/AbilitySystem/Abilities/PlayerSmallHitReactionAbility.cpp | 264 | 已核验／1-264 | CP-09 玩家防御与轻重受击；完整头源；四向 selector、叠加/可重触发不阻断动作，旧 Task 委托先解绑、同步返回检查；原四向资产字段保留。 |
| Source/PolyQuest/Private/AbilitySystem/Abilities/PrimaryAttackAbility.cpp | 218 | 已核验／1-218 | CP-06 基础移动与 Primary 路由；完整头源；临时移动/跳跃阻塞、松手/取消/阈值单次 Light/Charged 派发及清理；消费者为 Player 输入。 |
| Source/PolyQuest/Private/AbilitySystem/Abilities/SprintAbility.cpp | 212 | 已核验／1-212 | CP-06 基础移动与 Primary 路由；完整头源；速度/周期扣体力 Handle、体力委托、耗尽释放锁及 EndAbility 清理；施加 drain 时的同步体力回调与 Character 消费链留在角色批核验。 |
| Source/PolyQuest/Private/AbilitySystem/Abilities/SprintAttackAbility.cpp | 761 | 已核验／1-761 | CP-07 Light／Charged／SprintAttack；全文；真实 Sprint/落地准入、成本后临时 Tags、启动确认后才取消 Sprint；身份授权与清理；SHRINK-A 与 CAND-WARP-DUP-01。 |
| Source/PolyQuest/Private/AbilitySystem/Abilities/StaminaActionAbility.cpp | 111 | 已核验／1-111 | CP-06 基础移动与 Primary 路由；完整头源；正体力即可启动的最后一次动作契约保留，CostCommitted 每次重置；两个提交入口的冷却差异合理。 |
| Source/PolyQuest/Private/AbilitySystem/CharacterAttributeSet.cpp | 117 | 已核验／1-117 | CP-04 Attribute、Task 与窗口 helper；完整头源；Current/Base 钩子均必要，重复 Clamp 政策候选 CAND-ATTR-CLAMP-01；BaseCharacter/GE/属性测试消费，待 Tests 批次完成回归矩阵 |
| Source/PolyQuest/Private/AbilitySystem/Tasks/AbilityTask_MeleeTraceWindow.cpp | 284 | 已核验／1-284 | CP-05 Enemy Melee／StanceBreak；完整实现与直接 Parry/Poise/StanceBreak 链核验；现有 Poise 归零在 EnemyCharacter.cpp:902 延后下一帧派发，不据该链推断同步数组清空崩溃；多源去重/终止交给配套 Tests 核验 |
| Source/PolyQuest/Private/AbilitySystem/Tasks/AbilityTask_TurnToFacing.cpp | 130 | 已核验／1-130 | CP-04 Attribute、Task 与窗口 helper；完整头源及此前未漂移声明；非普通生产消费，专属测试和外部 Notify/Tag 测试仍存在；条件退役 CAND-LAUNCH-SMOOTH-01，不宣称可直接删除 |
| Source/PolyQuest/Public/AbilitySystem/Abilities/BowDrawFireAbility.h | 255 | 已核验／1-255 | CP-08 MeleeSkill／Bow／Dodge；完整头源；GAS 内部 Draw/Hold/Release 阶段、输入早释放、Notify 单发、装备 socket/瞄准/投射物请求、移动 GE 和 aim requester 收敛清理；内部阶段枚举不构成第二套玩法状态机；F02 接缝继承。 |
| Source/PolyQuest/Public/AbilitySystem/Abilities/ChargedAttackAbility.h | 360 | 已核验／1-360 | CP-07 Light／Charged／SprintAttack；全文；ReleaseHandoff、hold pause/cancel latch、释放时成本和伤害/Poise、Niagara/Delay 清理；SHRINK-A、11 getter 及 CAND-WARP-DUP-01，资产消费者不得零引用删除。 |
| Source/PolyQuest/Public/AbilitySystem/Abilities/DodgeAbility.h | 195 | 已核验／1-195 | CP-08 MeleeSkill／Bow／Dodge；完整头源；落地/取消窗口/重触发门槛、无敌 GE Handle、取消范围、Sequence 兼容与实例授权；4 getter 仍待消费表核验，状态字段/生产 Tag 保留。 |
| Source/PolyQuest/Public/AbilitySystem/Abilities/EnemyHitReactionAbility.h | 174 | 已核验／1-174 | CP-10 Enemy 受击与双方 Launch；全文；四向 Big/ledge/Falling/AI Stop，当前 RateWindow 仍 GetMontageInstanceForID 存活授权→H6-F01 同根 FIX1；不更改非致死/霸体契约。 |
| Source/PolyQuest/Public/AbilitySystem/Abilities/EnemyLaunchReactionAbility.h | 208 | 已核验／1-208 | CP-10 Enemy 受击与双方 Launch；全文含 CDO test 分支；地面 RootMotion 唯一阶段、即时 facing/ledge、结束后恢复/中止 StanceBreak deferral；RateWindow 同根 F01，getter 属 SHRINK-B。 |
| Source/PolyQuest/Public/AbilitySystem/Abilities/EnemyMeleeAbility.h | 189 | 已核验／1-189 | CP-05 Enemy Melee／StanceBreak；完整头源；AI pending/Profile/Commit/Task/Trace/HyperArmor/Cooldown 单一结束链；同资产旧实例授权缺陷 H6-F01；共享校验归 FIX1 |
| Source/PolyQuest/Public/AbilitySystem/Abilities/EnemySmallHitReactionAbility.h | 180 | 已核验／1-180 | CP-10 Enemy 受击与双方 Launch；全文；叠加/重触发、旧 Task 解绑、四向选择；RateWindow 同根 F01；保留小受击不取消移动/攻击。 |
| Source/PolyQuest/Public/AbilitySystem/Abilities/EnemyStanceBreakAbility.h | 183 | 已核验／1-183 | CP-05 Enemy Melee／StanceBreak；完整头源；Poise→延迟事件→Stunned/Cancel→恢复；保留 Context 身份与 Movement 交接；RateWindow 未绑定实例 ID，FIX1 独立核验其授权接入 |
| Source/PolyQuest/Public/AbilitySystem/Abilities/EnemyVictimExecutionAbility.h | 295 | 已核验／1-295 | CP-12 Victim 处决事务；完整头源；Front/Backstab 兼容性、授权 Hit scope、DeathPending/Outcome finalization、同步收据、真实 FindFloor、newborn instance 捕获/只停本实例、非致死恢复、旧 Context/RateWindow F01；非反射 getter 属 SHRINK-B。 |
| Source/PolyQuest/Public/AbilitySystem/Abilities/JumpAbility.h | 48 | 已核验／1-48 | CP-06 基础移动与 Primary 路由；完整头源；跳跃明确绕过正体力门槛、不延迟恢复；Sprint 空中速度 GE 在提交失败时回收，相关输入/耗尽测试待整合。 |
| Source/PolyQuest/Public/AbilitySystem/Abilities/LaunchFacingSmoothingState.h | 42 | 已核验／1-42 | CP-04 Attribute、Task 与窗口 helper；完整头源及此前未漂移声明；非普通生产消费，专属测试和外部 Notify/Tag 测试仍存在；条件退役 CAND-LAUNCH-SMOOTH-01，不宣称可直接删除 |
| Source/PolyQuest/Public/AbilitySystem/Abilities/LightAttackAbility.h | 249 | 已核验／1-249 | CP-07 Light／Charged／SprintAttack；全文含条件编译与测试 seam；唯一 Montage/每段身份、续段成本、输入/分支窗口、同步 End 不复活旧 Task；Light 的快照跨段复用，保留独立流程。 |
| Source/PolyQuest/Public/AbilitySystem/Abilities/MontageRateWindowLifecycle.h | 105 | 已核验／1-105 | CP-04 Attribute、Task 与窗口 helper；完整头源；Begin/End/Restore 身份/来源/Notify/速率检查，H6-F01 归 FIX1；测试 seam 是集合级辅助，不等于真实实例证明 |
| Source/PolyQuest/Public/AbilitySystem/Abilities/PlayerBackstabExecutionAbility.h | 220 | 已核验／1-220 | CP-11 前处决与背刺；完整头源/全部条件编译；装备/距离快照、前/背几何与 StanceBreak 差异、Commit→Victim 同步握手→Snap→Hit→VictimStart/Release 收据、token/解绑/幂等结束；H6-F04、CAND-EXEC-SNAP-01；背刺额外 CAND-BACKSTAB-HANDLE-01。 |
| Source/PolyQuest/Public/AbilitySystem/Abilities/PlayerBigHitReactionAbility.h | 104 | 已核验／1-104 | CP-09 玩家防御与轻重受击；完整头源；四向方向快照、HyperArmor/地面门槛、11类取消/阻止、ledge 保存/恢复及 Falling 清理；轻重反应差异合理。 |
| Source/PolyQuest/Public/AbilitySystem/Abilities/PlayerFrontExecutionAbility.h | 219 | 已核验／1-219 | CP-11 前处决与背刺；完整头源/全部条件编译；装备/距离快照、前/背几何与 StanceBreak 差异、Commit→Victim 同步握手→Snap→Hit→VictimStart/Release 收据、token/解绑/幂等结束；H6-F04、CAND-EXEC-SNAP-01；背刺额外 CAND-BACKSTAB-HANDLE-01。 |
| Source/PolyQuest/Public/AbilitySystem/Abilities/PlayerGuardAbility.h | 146 | 已核验／1-146 | CP-09 玩家防御与轻重受击；完整头源；持续防御确认/GE 事务回收、体力归零仍吸收当前击、GuardBreak/恢复意图、前弧与音效位置；保留持续防御独立权属。 |
| Source/PolyQuest/Public/AbilitySystem/Abilities/PlayerGuardBreakAbility.h | 80 | 已核验／1-80 | CP-09 玩家防御与轻重受击；完整头源；体力零触发、确认 Montage 后移动锁和动作取消、死亡不恢复；不按 Guard/Parry 表面相似合并生命周期。 |
| Source/PolyQuest/Public/AbilitySystem/Abilities/PlayerLaunchReactionAbility.h | 173 | 已核验／1-173 | CP-10 Enemy 受击与双方 Launch；全文含 test 分支；地面 knockdown、Dodge cancel 但不 Block Dodge、落地/角色失效/清理；与 Enemy 的纯 facing 几何重复候选，完整生命周期保留。 |
| Source/PolyQuest/Public/AbilitySystem/Abilities/PlayerMeleeSkillAbility.h | 243 | 已核验／1-243 | CP-08 MeleeSkill／Bow／Dodge；完整头源与 WITH_EDITOR/PostCDOCompiled；能力标签修复、Montage 确认后单次 cost/cooldown、回调激活护栏、统一清理；绑定/warp 与 Sprint/Charged 同构，纳入两候选直接消费者。 |
| Source/PolyQuest/Public/AbilitySystem/Abilities/PlayerParryAbility.h | 136 | 已核验／1-136 | CP-09 玩家防御与轻重受击；完整头源；成本与自然结束冷却分离、移动锁/外部 Falling 优先、Poise 反制/反馈；F03 重复 Begin 引发 ParryActive loose tag 计数残留，详检查点。 |
| Source/PolyQuest/Public/AbilitySystem/Abilities/PlayerSmallHitReactionAbility.h | 110 | 已核验／1-110 | CP-09 玩家防御与轻重受击；完整头源；四向 selector、叠加/可重触发不阻断动作，旧 Task 委托先解绑、同步返回检查；原四向资产字段保留。 |
| Source/PolyQuest/Public/AbilitySystem/Abilities/PrimaryAttackAbility.h | 79 | 已核验／1-79 | CP-06 基础移动与 Primary 路由；完整头源；临时移动/跳跃阻塞、松手/取消/阈值单次 Light/Charged 派发及清理；消费者为 Player 输入。 |
| Source/PolyQuest/Public/AbilitySystem/Abilities/SprintAbility.h | 64 | 已核验／1-64 | CP-06 基础移动与 Primary 路由；完整头源；速度/周期扣体力 Handle、体力委托、耗尽释放锁及 EndAbility 清理；施加 drain 时的同步体力回调与 Character 消费链留在角色批核验。 |
| Source/PolyQuest/Public/AbilitySystem/Abilities/SprintAttackAbility.h | 228 | 已核验／1-228 | CP-07 Light／Charged／SprintAttack；全文；真实 Sprint/落地准入、成本后临时 Tags、启动确认后才取消 Sprint；身份授权与清理；SHRINK-A 与 CAND-WARP-DUP-01。 |
| Source/PolyQuest/Public/AbilitySystem/Abilities/StaminaActionAbility.h | 59 | 已核验／1-59 | CP-06 基础移动与 Primary 路由；完整头源；正体力即可启动的最后一次动作契约保留，CostCommitted 每次重置；两个提交入口的冷却差异合理。 |
| Source/PolyQuest/Public/AbilitySystem/CharacterAttributeSet.h | 61 | 已核验／1-61 | CP-04 Attribute、Task 与窗口 helper；完整头源；Current/Base 钩子均必要，重复 Clamp 政策候选 CAND-ATTR-CLAMP-01；BaseCharacter/GE/属性测试消费，待 Tests 批次完成回归矩阵 |
| Source/PolyQuest/Public/AbilitySystem/Tasks/AbilityTask_MeleeTraceWindow.h | 78 | 已核验／1-78 | CP-04 Attribute、Task 与窗口 helper；完整头源；共享 Task 的创建/激活/关闭与多源采样均已读；同步命中回调导致清理的疑点待直接取消链证实 |
| Source/PolyQuest/Public/AbilitySystem/Tasks/AbilityTask_TurnToFacing.h | 51 | 已核验／1-51 | CP-04 Attribute、Task 与窗口 helper；完整头源及此前未漂移声明；非普通生产消费，专属测试和外部 Notify/Tag 测试仍存在；条件退役 CAND-LAUNCH-SMOOTH-01，不宣称可直接删除 |

### AI（8 文件）

| 文件 | 总行数 | 状态／已完成区段 | 审阅方式、证据、消费者／测试、结论与缺口 |
|---|---:|---|---|
| Source/PolyQuest/Private/AI/EnemyAIController.cpp | 1172 | 已核验／1-1172 | CP-13 AI 全单元；完整头源；Perception/retained/leash、profile校验/pending快照、RequestID回执/重定位速度归还、处决锁同 Pawn恢复、root-motion facing接管；目标死亡消费留 Character 接缝对账。 |
| Source/PolyQuest/Private/AI/EnemyAIProfile.cpp | 50 | 已核验／1-50 | CP-13 AI 全单元；完整声明/验证；正有限径距、非负侧移/重试、Leash/Timeout，反射 getter 为 StateTree/资产契约，保留。 |
| Source/PolyQuest/Private/AI/StateTree/EnemyStateTreeConditions.cpp | 113 | 已核验／1-113 | CP-13 AI 全单元；完整九种条件；ExternalData 读取 Controller，PendingOutOfRange 额外 HasPending 不等同简单反向；空 InstanceData 为反射节点契约保留。 |
| Source/PolyQuest/Private/AI/StateTree/EnemyStateTreeTasks.cpp | 362 | 已核验／1-362 | CP-13 AI 全单元；完整 Enter/Tick/Exit；一次请求/观察 GAS Tag、Approach pending 保留/超时清除、cooldown reposition 退出停导航；共享概念但失败/成功语义差异保留。 |
| Source/PolyQuest/Public/AI/EnemyAIController.h | 330 | 已核验／1-330 | CP-13 AI 全单元；完整头源；Perception/retained/leash、profile校验/pending快照、RequestID回执/重定位速度归还、处决锁同 Pawn恢复、root-motion facing接管；目标死亡消费留 Character 接缝对账。 |
| Source/PolyQuest/Public/AI/EnemyAIProfile.h | 87 | 已核验／1-87 | CP-13 AI 全单元；完整声明/验证；正有限径距、非负侧移/重试、Leash/Timeout，反射 getter 为 StateTree/资产契约，保留。 |
| Source/PolyQuest/Public/AI/StateTree/EnemyStateTreeConditions.h | 208 | 已核验／1-208 | CP-13 AI 全单元；完整九种条件；ExternalData 读取 Controller，PendingOutOfRange 额外 HasPending 不等同简单反向；空 InstanceData 为反射节点契约保留。 |
| Source/PolyQuest/Public/AI/StateTree/EnemyStateTreeTasks.h | 144 | 已核验／1-144 | CP-13 AI 全单元；完整 Enter/Tick/Exit；一次请求/观察 GAS Tag、Approach pending 保留/超时清除、cooldown reposition 退出停导航；共享概念但失败/成功语义差异保留。 |

### Animation（10 文件）

| 文件 | 总行数 | 状态／已完成区段 | 审阅方式、证据、消费者／测试、结论与缺口 |
|---|---:|---|---|
| Source/PolyQuest/Private/Animation/Combat/AnimNotify_PlayerBowAction.cpp | 57 | 已核验／1-57 | CP-14 Animation／Camera；完整头源；Bow Draw/Release 与 Execution Hit/VictimStart 语义时点，Owner/ASC/Tag 校验、Animation payload 消费链已读；反射资产类型保留。 |
| Source/PolyQuest/Private/Animation/Combat/AnimNotify_PlayerExecutionHit.cpp | 47 | 已核验／1-47 | CP-14 Animation／Camera；完整头源；Bow Draw/Release 与 Execution Hit/VictimStart 语义时点，Owner/ASC/Tag 校验、Animation payload 消费链已读；反射资产类型保留。 |
| Source/PolyQuest/Private/Animation/Combat/AnimNotify_PlayerExecutionVictimStart.cpp | 41 | 已核验／1-41 | CP-14 Animation／Camera；完整头源；Bow Draw/Release 与 Execution Hit/VictimStart 语义时点，Owner/ASC/Tag 校验、Animation payload 消费链已读；反射资产类型保留。 |
| Source/PolyQuest/Private/Animation/Combat/AnimNotify_ReactionLaunchCommit.cpp | 39 | 已核验／1-39 | CP-14 Animation／Camera；完整头源；退役 Takeoff/flight 语义，仍有测试/Tag/潜在资产入口，CAND-LAUNCH-SMOOTH-01 有条件延期。 |
| Source/PolyQuest/Private/Animation/Combat/AnimNotifyState_ActionWindows.cpp | 195 | 已核验／1-195 | CP-14 Animation／Camera；完整头源（cpp 已于 CP-09 全文读取）；九类 Notify/Window 路由到自身 ASC，身份/幅值/BeginEnd 对应 Ability；F03 生产者；CAND-NOTIFY-DUP-01。 |
| Source/PolyQuest/Public/Animation/Combat/AnimNotify_PlayerBowAction.h | 27 | 已核验／1-27 | CP-14 Animation／Camera；完整头源；Bow Draw/Release 与 Execution Hit/VictimStart 语义时点，Owner/ASC/Tag 校验、Animation payload 消费链已读；反射资产类型保留。 |
| Source/PolyQuest/Public/Animation/Combat/AnimNotify_PlayerExecutionHit.h | 19 | 已核验／1-19 | CP-14 Animation／Camera；完整头源；Bow Draw/Release 与 Execution Hit/VictimStart 语义时点，Owner/ASC/Tag 校验、Animation payload 消费链已读；反射资产类型保留。 |
| Source/PolyQuest/Public/Animation/Combat/AnimNotify_PlayerExecutionVictimStart.h | 24 | 已核验／1-24 | CP-14 Animation／Camera；完整头源；Bow Draw/Release 与 Execution Hit/VictimStart 语义时点，Owner/ASC/Tag 校验、Animation payload 消费链已读；反射资产类型保留。 |
| Source/PolyQuest/Public/Animation/Combat/AnimNotify_ReactionLaunchCommit.h | 20 | 已核验／1-20 | CP-14 Animation／Camera；完整头源；退役 Takeoff/flight 语义，仍有测试/Tag/潜在资产入口，CAND-LAUNCH-SMOOTH-01 有条件延期。 |
| Source/PolyQuest/Public/Animation/Combat/AnimNotifyState_ActionWindows.h | 119 | 已核验／1-119 | CP-14 Animation／Camera；完整头源（cpp 已于 CP-09 全文读取）；九类 Notify/Window 路由到自身 ASC，身份/幅值/BeginEnd 对应 Ability；F03 生产者；CAND-NOTIFY-DUP-01。 |

### Camera（2 文件）

| 文件 | 总行数 | 状态／已完成区段 | 审阅方式、证据、消费者／测试、结论与缺口 |
|---|---:|---|---|
| Source/PolyQuest/Private/Camera/CameraModifier_FovPunch.cpp | 40 | 已核验／1-40 | CP-14 Animation／Camera；完整头源；原生 CameraModifier 叠加 FOV 后插值归零，独立于基准 Camera FOV；Player/CombatFeedback 测试消费者待对应批记录，保留小组件。 |
| Source/PolyQuest/Public/Camera/CameraModifier_FovPunch.h | 51 | 已核验／1-51 | CP-14 Animation／Camera；完整头源；原生 CameraModifier 叠加 FOV 后插值归零，独立于基准 Camera FOV；Player/CombatFeedback 测试消费者待对应批记录，保留小组件。 |

### Character（8 文件）

| 文件 | 总行数 | 状态／已完成区段 | 审阅方式、证据、消费者／测试、结论与缺口 |
|---|---:|---|---|
| Source/PolyQuest/Private/Character/BaseCharacter.cpp | 262 | 已核验／1-262 | CP-15 BaseCharacter／LockOn 几何；完整头源；Character-owned ASC/AttributeSet、Possess去重授予/BeginPlay ActorInfo、速度委托、Overlay 外部所有者恢复/Timer 清理；旧 WeaponMesh 命名挂接保留待资产证据。 |
| Source/PolyQuest/Private/Character/Enemy/EnemyCharacter.cpp | 1369 | 已核验／1-1369 | CP-16 EnemyCharacter 完整补审；完整 1-1369；GE Spec 去重、Poise next-tick 仲裁、处决授权反馈、死亡幂等与 UI 原 ASC 解绑。无新增已证实缺陷。 |
| Source/PolyQuest/Private/Character/Player/PlayerCharacter.cpp | 3455 | 已核验／1-3455 | CP-19 PlayerCharacter 完整补审；完整1-3455（含CodeGraph已读段）；输入/装备/目标/朝向/体力/反馈/交互与所有条件编译；F05、F06、反射兼容和候选见CP-19。 |
| Source/PolyQuest/Private/Character/Player/PlayerLockOnTargeting.cpp | 209 | 已核验／1-209 | CP-15 BaseCharacter／LockOn 几何；完整头源；屏幕候选 pure helper、viewport严格边界/finite、nearest/cycle/successor；H6-F05 比较器非传递，消费点在 Player 批核验。 |
| Source/PolyQuest/Public/Character/BaseCharacter.h | 100 | 已核验／1-100 | CP-15 BaseCharacter／LockOn 几何；完整头源；Character-owned ASC/AttributeSet、Possess去重授予/BeginPlay ActorInfo、速度委托、Overlay 外部所有者恢复/Timer 清理；旧 WeaponMesh 命名挂接保留待资产证据。 |
| Source/PolyQuest/Public/Character/Enemy/EnemyCharacter.h | 247 | 已核验／1-247 | CP-16 EnemyCharacter 完整补审；完整声明/反射/测试分支；ASC、处决/Launch deferral、死亡/UI 分开所有权，测试消费者随后逐文件对账。 |
| Source/PolyQuest/Public/Character/Player/PlayerCharacter.h | 633 | 已核验／1-633 | CP-17 PlayerCharacter 声明与启动段；完整633行声明、反射和测试分支；控制意图、ASC消费、呈现/交互入口；Look资产兼容入口不批准删除。 |
| Source/PolyQuest/Public/Character/Player/PlayerLockOnTargeting.h | 34 | 已核验／1-34 | CP-15 BaseCharacter／LockOn 几何；完整头源；屏幕候选 pure helper、viewport严格边界/finite、nearest/cycle/successor；H6-F05 比较器非传递，消费点在 Player 批核验。 |

### Combat（47 文件）

| 文件 | 总行数 | 状态／已完成区段 | 审阅方式、证据、消费者／测试、结论与缺口 |
|---|---:|---|---|
| Source/PolyQuest/Private/Combat/Enemy/EnemyAttackProfile.cpp | 12 | 已核验／1-12 | CP-20 Combat 定义与Enemy攻击集；完整定义/实现；Controller选择与Ability快照消费，权重/距离有限性、重复profile拒绝。 |
| Source/PolyQuest/Private/Combat/Enemy/EnemyAttackSet.cpp | 116 | 已核验／1-116 | CP-20 Combat 定义与Enemy攻击集；完整定义/实现；Controller选择与Ability快照消费，权重/距离有限性、重复profile拒绝。 |
| Source/PolyQuest/Private/Combat/Equipment/BowWeaponDefinition.cpp | 120 | 已核验／1-120 | CP-20 Combat 定义与Enemy攻击集；完整定义/验证；Combo只持有配置；Bow/OffHand/Defense差异与消费契约保留。 |
| Source/PolyQuest/Private/Combat/Equipment/MeleeWeaponDefinition.cpp | 324 | 已核验／1-324 | CP-20 Combat 定义与Enemy攻击集；完整定义/实现；玩家整武器与敌人几何验证分离，保留owner/static/socket模式；CAND-MELEE-VALIDATION-DUP-01。 |
| Source/PolyQuest/Private/Combat/Equipment/ProjectileDefinition.cpp | 113 | 已核验／1-113 | CP-21 Equipment 与世界拾取完整审阅；完整验证；有限数值/依赖关系；FlightTrail timeout消费者在Projectile批核对。 |
| Source/PolyQuest/Private/Combat/Equipment/WeaponEquipmentComponent.cpp | 1366 | 已核验／1-1366 | CP-21 Equipment 与世界拾取完整审阅；完整1-1366；CodeGraph+遗漏段；preflight、事务rollback、新handle、prepared绑定和输入profile；UI轮询补证后裁决刷新疑点。 |
| Source/PolyQuest/Private/Combat/Equipment/WorldPickupGrounding.cpp | 98 | 已核验／1-98 | CP-21 Equipment 与世界拾取完整审阅；完整98行；8顶点变换投影含负/零scale契约；无平台等效简化可直接删除校验。 |
| Source/PolyQuest/Private/Combat/Equipment/WorldPickupGrounding.h | 34 | 已核验／1-34 | CP-21 Equipment 与世界拾取完整审阅；完整34行；纯几何接口，Pickup/Test消费者。 |
| Source/PolyQuest/Private/Combat/Equipment/WorldWeaponPickup.cpp | 415 | 已核验／1-415 | CP-21 Equipment 与世界拾取完整审阅；完整415行；deferred drop staging/grounding、双侧候选注册与destroy清理；见CP-21。 |
| Source/PolyQuest/Private/Combat/Execution/ExecutionLockContext.cpp | 439 | 已核验／1-439 | CP-22 Execution 与反馈定义；完整439行（CodeGraph+遗漏段）；绑定双方与token、RAII abort、nonlethal/deathpending/release幂等；compat API消费待专项。 |
| Source/PolyQuest/Private/Combat/Execution/ExecutionSnapAlignment.cpp | 136 | 已核验／1-136 | CP-22 Execution 与反馈定义；完整136行；250cm范围/有限/planar forward/Z保留；旧snap复制可归此层但不合并Ability。 |
| Source/PolyQuest/Private/Combat/Feedback/CombatFeedbackDataAsset.cpp | 63 | 已核验／1-63 | CP-22 Execution 与反馈定义；完整63行；两种tier返回类型不同，小switch保留，CDO默认值不做模板提取。 |
| Source/PolyQuest/Private/Combat/Melee/MeleeHitResolver.cpp | 182 | 已核验／1-182 | CP-23 Melee 接触与拖尾；继承Main在CP-09已完整读取182行；校验source/ASC/team/dead/invulnerable/Execution、Parry/Guard与一次GE；F02下游契约保留。 |
| Source/PolyQuest/Private/Combat/Melee/MeleeMotionWarping.cpp | 130 | 已核验／1-130 | CP-23 Melee 接触与拖尾；完整130行；有限/地面/角度/范围/零修正拒绝，保留独立快照生命期。 |
| Source/PolyQuest/Private/Combat/Melee/MeleeTraceSourceComponent.cpp | 335 | 已核验／1-335 | CP-23 Melee 接触与拖尾；完整335行；endpoint有限/不重合及组件祖先；CAND-NATIVE-ATTACH-01。 |
| Source/PolyQuest/Private/Combat/Melee/MeleeWeaponTrailComponent.cpp | 252 | 已核验／1-252 | CP-23 Melee 接触与拖尾；CodeGraph完整252行；new requester不受旧End清理，EndPlay撤child；测试tracking不能证明真实Niagara呈现。 |
| Source/PolyQuest/Private/Combat/Melee/MeleeWeaponTrailComponent.h | 74 | 已核验／1-74 | CP-23 Melee 接触与拖尾；完整74行；每source requester和Niagara child所有权；测试tracking是显式无渲染seam。 |
| Source/PolyQuest/Private/Combat/Projectile/CombatProjectile.cpp | 671 | 已核验／1-671 | CP-24 Projectile 与 Reaction 完整审阅；完整183/671行（图谱+补全）；flight快照、限时转角、碰撞终止与trail timeout；无异步运行证明。 |
| Source/PolyQuest/Private/Combat/Projectile/CombatProjectileHitResolver.cpp | 112 | 已核验／1-112 | CP-24 Projectile 与 Reaction 完整审阅；完整声明/实现；Projectile Guard允许/Parry禁用、source弱引用、GAS结算；或Reaction精确tag/方向选择，保留最小层。 |
| Source/PolyQuest/Private/Combat/Projectile/CombatProjectileTargeting.cpp | 317 | 已核验／1-317 | CP-24 Projectile 与 Reaction 完整审阅；完整声明/实现；候选过滤、Native/BP team分派、单目标选择；F05排序扩展与F08契约。 |
| Source/PolyQuest/Private/Combat/Reaction/HitReactionClassifier.cpp | 59 | 已核验／1-59 | CP-24 Projectile 与 Reaction 完整审阅；完整声明/实现；Projectile Guard允许/Parry禁用、source弱引用、GAS结算；或Reaction精确tag/方向选择，保留最小层。 |
| Source/PolyQuest/Private/Combat/Reaction/HitReactionFourWayMontageSelector.cpp | 30 | 已核验／1-30 | CP-24 Projectile 与 Reaction 完整审阅；完整声明/实现；Projectile Guard允许/Parry禁用、source弱引用、GAS结算；或Reaction精确tag/方向选择，保留最小层。 |
| Source/PolyQuest/Private/Combat/Reaction/HitReactionFourWayMontageSelector.h | 39 | 已核验／1-39 | CP-24 Projectile 与 Reaction 完整审阅；完整声明/实现；Projectile Guard允许/Parry禁用、source弱引用、GAS结算；或Reaction精确tag/方向选择，保留最小层。 |
| Source/PolyQuest/Private/Combat/Reaction/HitReactionImpactResolver.cpp | 175 | 已核验／1-175 | CP-24 Projectile 与 Reaction 完整审阅；完整51/175行；Instigator优先和ImpactNormal回退继承F02；旧Launch速度helper待消费专项。 |
| Source/PolyQuest/Public/Combat/ComboChainDataAsset.h | 61 | 已核验／1-61 | CP-20 Combat 定义与Enemy攻击集；完整定义/验证；Combo只持有配置；Bow/OffHand/Defense差异与消费契约保留。 |
| Source/PolyQuest/Public/Combat/Enemy/EnemyAttackProfile.h | 50 | 已核验／1-50 | CP-20 Combat 定义与Enemy攻击集；完整定义/实现；Controller选择与Ability快照消费，权重/距离有限性、重复profile拒绝。 |
| Source/PolyQuest/Public/Combat/Enemy/EnemyAttackSet.h | 78 | 已核验／1-78 | CP-20 Combat 定义与Enemy攻击集；完整定义/实现；Controller选择与Ability快照消费，权重/距离有限性、重复profile拒绝。 |
| Source/PolyQuest/Public/Combat/Equipment/BowWeaponDefinition.h | 31 | 已核验／1-31 | CP-20 Combat 定义与Enemy攻击集；完整声明/inline验证；候选union、Base/Prepared互斥、exact tags、序列化枚举与反射配置保留。 |
| Source/PolyQuest/Public/Combat/Equipment/DefenseProfileDefinition.h | 32 | 已核验／1-32 | CP-20 Combat 定义与Enemy攻击集；完整定义/验证；Combo只持有配置；Bow/OffHand/Defense差异与消费契约保留。 |
| Source/PolyQuest/Public/Combat/Equipment/MeleeWeaponDefinition.h | 114 | 已核验／1-114 | CP-20 Combat 定义与Enemy攻击集；完整定义/实现；玩家整武器与敌人几何验证分离，保留owner/static/socket模式；CAND-MELEE-VALIDATION-DUP-01。 |
| Source/PolyQuest/Public/Combat/Equipment/OffHandWeaponDefinition.h | 58 | 已核验／1-58 | CP-20 Combat 定义与Enemy攻击集；完整声明/inline验证；候选union、Base/Prepared互斥、exact tags、序列化枚举与反射配置保留。 |
| Source/PolyQuest/Public/Combat/Equipment/ProjectileDefinition.h | 108 | 已核验／1-108 | CP-21 Equipment 与世界拾取完整审阅；完整声明；不可变flight/assist/homing配置，能力/Actor持有快照；Reflection保留。 |
| Source/PolyQuest/Public/Combat/Equipment/WeaponDefinition.h | 325 | 已核验／1-325 | CP-20 Combat 定义与Enemy攻击集；完整声明/inline验证；候选union、Base/Prepared互斥、exact tags、序列化枚举与反射配置保留。 |
| Source/PolyQuest/Public/Combat/Equipment/WeaponEquipmentComponent.h | 197 | 已核验／1-197 | CP-21 Equipment 与世界拾取完整审阅；完整声明；组件owns授予handle/显示，GAS动作权威；事件为最终逻辑组成。 |
| Source/PolyQuest/Public/Combat/Equipment/WorldWeaponPickup.h | 119 | 已核验／1-119 | CP-21 Equipment 与世界拾取完整审阅；CodeGraph完整119行；反射overlap与BP consumed入口、former owner cooldown和事务friend。 |
| Source/PolyQuest/Public/Combat/Execution/ExecutionLockContext.h | 217 | 已核验／1-217 | CP-22 Execution 与反馈定义；完整217行；双能力会话凭证、Hit/Release枚举、RAII事务，非外部玩法状态机。 |
| Source/PolyQuest/Public/Combat/Execution/ExecutionSnapAlignment.h | 44 | 已核验／1-44 | CP-22 Execution 与反馈定义；完整44行；纯距离/侧向transform辅助，Front/Backstab/Weapon/Test共同消费。 |
| Source/PolyQuest/Public/Combat/Feedback/CombatFeedbackDataAsset.h | 201 | 已核验／1-201 | CP-22 Execution 与反馈定义；完整201行；共用overlay+Player/Enemy类型化数据，反射配置实际由Character/Defense消费。 |
| Source/PolyQuest/Public/Combat/Melee/CombatTeamAgent.h | 23 | 已核验／1-23 | CP-23 Melee 接触与拖尾；完整23行；反射team最小接口，Base/Resolver/Feedback真实消费，非单实现可删。 |
| Source/PolyQuest/Public/Combat/Melee/MeleeHitResolver.h | 35 | 已核验／1-35 | CP-23 Melee 接触与拖尾；完整35行；hit request含Execution credential与GAS数据，旧单值和map消费者分开保留。 |
| Source/PolyQuest/Public/Combat/Melee/MeleeMotionWarping.h | 56 | 已核验／1-56 | CP-23 Melee 接触与拖尾；完整56行；配置与每Ability快照、纯几何函数，CAND-WARP-DUP-01最小共享层。 |
| Source/PolyQuest/Public/Combat/Melee/MeleeTraceSourceComponent.h | 77 | 已核验／1-77 | CP-23 Melee 接触与拖尾；完整77行；装备/Static/legacy三入口；反射固定名无readback不删。 |
| Source/PolyQuest/Public/Combat/Projectile/CombatProjectile.h | 183 | 已核验／1-183 | CP-24 Projectile 与 Reaction 完整审阅；完整183/671行（图谱+补全）；flight快照、限时转角、碰撞终止与trail timeout；无异步运行证明。 |
| Source/PolyQuest/Public/Combat/Projectile/CombatProjectileHitResolver.h | 33 | 已核验／1-33 | CP-24 Projectile 与 Reaction 完整审阅；完整声明/实现；Projectile Guard允许/Parry禁用、source弱引用、GAS结算；或Reaction精确tag/方向选择，保留最小层。 |
| Source/PolyQuest/Public/Combat/Projectile/CombatProjectileTargeting.h | 71 | 已核验／1-71 | CP-24 Projectile 与 Reaction 完整审阅；完整声明/实现；候选过滤、Native/BP team分派、单目标选择；F05排序扩展与F08契约。 |
| Source/PolyQuest/Public/Combat/Reaction/HitReactionClassifier.h | 31 | 已核验／1-31 | CP-24 Projectile 与 Reaction 完整审阅；完整声明/实现；Projectile Guard允许/Parry禁用、source弱引用、GAS结算；或Reaction精确tag/方向选择，保留最小层。 |
| Source/PolyQuest/Public/Combat/Reaction/HitReactionImpactResolver.h | 51 | 已核验／1-51 | CP-24 Projectile 与 Reaction 完整审阅；完整51/175行；Instigator优先和ImpactNormal回退继承F02；旧Launch速度helper待消费专项。 |

### Environment（4 文件）

| 文件 | 总行数 | 状态／已完成区段 | 审阅方式、证据、消费者／测试、结论与缺口 |
|---|---:|---|---|
| Source/PolyQuest/Private/Environment/FloorTriggerVolume.cpp | 96 | 已核验／1-96 | CP-25 Environment 完整审阅；完整96行；匹配FloorIndex否则nearest、begin overlap切换；多volume/关卡证据缺口单列。 |
| Source/PolyQuest/Private/Environment/FloorVolume.cpp | 373 | 已核验／1-373 | CP-25 Environment 完整审阅；完整373行；bounds分类、tag优先、keyword heuristic、插值与teardown；F09、CAND-FLOOR-HIDE-DUP-01。 |
| Source/PolyQuest/Public/Environment/FloorTriggerVolume.h | 45 | 已核验／1-45 | CP-25 Environment 完整审阅；完整45行；反射引用与overlap入口，目标floor声明保留。 |
| Source/PolyQuest/Public/Environment/FloorVolume.h | 93 | 已核验／1-93 | CP-25 Environment 完整审阅；完整93行；楼层集合/隐藏与MPC是呈现状态，碰撞保持；F09 生命周期。 |

### Framework（4 文件）

| 文件 | 总行数 | 状态／已完成区段 | 审阅方式、证据、消费者／测试、结论与缺口 |
|---|---:|---|---|
| Source/PolyQuest/Private/Framework/PolyQuestGameMode.cpp | 8 | 已核验／1-8 | CP-26 Framework 完整审阅；完整8行；空ctor无行为，删除收益微小且类资产挂接保留。 |
| Source/PolyQuest/Private/Framework/PolyQuestPlayerController.cpp | 582 | 已核验／1-582 | CP-26 Framework 完整审阅；完整582行（含图谱461-515）；重绑定解绑旧ASC，HUD三类型创建，时间膨胀外部接管后不覆盖。 |
| Source/PolyQuest/Public/Framework/PolyQuestGameMode.h | 21 | 已核验／1-21 | CP-26 Framework 完整审阅；完整21行；BP_GameMode反射根，空类不能证明退役。 |
| Source/PolyQuest/Public/Framework/PolyQuestPlayerController.h | 147 | 已核验／1-147 | CP-26 Framework 完整审阅；完整147行；local HUD、绑定ASC、real-time hitstop所有权。 |

### UI（10 文件）

| 文件 | 总行数 | 状态／已完成区段 | 审阅方式、证据、消费者／测试、结论与缺口 |
|---|---:|---|---|
| Source/PolyQuest/Private/UI/EnemyHealthBarWidget.cpp | 316 | 已核验／1-316 | CP-27 UI 完整审阅；完整头/源含全部测试分支；health/flash/shake/渐隐呈现数据，F10与CAND-UI-CURVES-01；不修改ASC。 |
| Source/PolyQuest/Private/UI/PlayerSkillBarHUDWidget.cpp | 248 | 已核验／1-248 | CP-27 UI 完整审阅；完整74/248行（cpp由CodeGraph）；每tick重读真实prepared handles与ASC cooldown、旧ASC解绑，否定rollback缓存handle疑点。 |
| Source/PolyQuest/Private/UI/PlayerSkillSlotWidget.cpp | 132 | 已核验／1-132 | CP-27 UI 完整审阅；完整86/132行；enum是呈现状态，MID只创建一次、InitializeSlot幂等、仅状态/percent变化写视觉。 |
| Source/PolyQuest/Private/UI/PlayerVitalHUDWidget.cpp | 767 | 已核验／1-767 | CP-27 UI 完整审阅；完整头/源含全部测试分支；health/flash/shake/渐隐呈现数据，F10与CAND-UI-CURVES-01；不修改ASC。 |
| Source/PolyQuest/Private/UI/WorldInteractionPromptWidget.cpp | 24 | 已核验／1-24 | CP-27 UI 完整审阅；完整32/24行；被动Prompt无输入/timer/GAS，BindWidget资产入口保留。 |
| Source/PolyQuest/Public/UI/EnemyHealthBarWidget.h | 109 | 已核验／1-109 | CP-27 UI 完整审阅；完整头/源含全部测试分支；health/flash/shake/渐隐呈现数据，F10与CAND-UI-CURVES-01；不修改ASC。 |
| Source/PolyQuest/Public/UI/PlayerSkillBarHUDWidget.h | 74 | 已核验／1-74 | CP-27 UI 完整审阅；完整74/248行（cpp由CodeGraph）；每tick重读真实prepared handles与ASC cooldown、旧ASC解绑，否定rollback缓存handle疑点。 |
| Source/PolyQuest/Public/UI/PlayerSkillSlotWidget.h | 86 | 已核验／1-86 | CP-27 UI 完整审阅；完整86/132行；enum是呈现状态，MID只创建一次、InitializeSlot幂等、仅状态/percent变化写视觉。 |
| Source/PolyQuest/Public/UI/PlayerVitalHUDWidget.h | 235 | 已核验／1-235 | CP-27 UI 完整审阅；完整头/源含全部测试分支；health/flash/shake/渐隐呈现数据，F10与CAND-UI-CURVES-01；不修改ASC。 |
| Source/PolyQuest/Public/UI/WorldInteractionPromptWidget.h | 32 | 已核验／1-32 | CP-27 UI 完整审阅；完整32/24行；被动Prompt无输入/timer/GAS，BindWidget资产入口保留。 |

### Tests（72 文件）

| 文件 | 总行数 | 状态／已完成区段 | 审阅方式、证据、消费者／测试、结论与缺口 |
|---|---:|---|---|
| Source/PolyQuest/Private/Tests/BackstabExecutionAutomationTests.cpp | 1456 | 已核验／1-1456 | CP-65 Backstab 完成；完整17节；真实输入优先级/旧Context/同步重入/物理阻挡snap；F04变体仍缺 |
| Source/PolyQuest/Private/Tests/ChargedAttackNiagaraFeedbackAutomationTests.cpp | 414 | 已核验／1-414 | CP-42 Tests 蓄力反馈与Trace生命周期；Graph+全部gap；真实held时间WaitDelay、注入Active/charging、Niagara tracking与装备attachment/default/explicit失败；5 getter无此文件消费，最终全Source对账。 |
| Source/PolyQuest/Private/Tests/CombatAutomationFixture.cpp | 148 | 已核验／1-148 | CP-28 Tests 公共fixture与GE类型；完整；PreBeginPlay authoring→FinishSpawning→DispatchBeginPlayOnce；共享现成fixture优先，不弱化启动校验。 |
| Source/PolyQuest/Private/Tests/CombatAutomationFixture.h | 31 | 已核验／1-31 | CP-28 Tests 公共fixture与GE类型；完整；PreBeginPlay authoring→FinishSpawning→DispatchBeginPlayOnce；共享现成fixture优先，不弱化启动校验。 |
| Source/PolyQuest/Private/Tests/CombatHitFeedbackAutomationTests.cpp | 1151 | 已核验／1-1151 | CP-58 Tests Combat 反馈完成；全文1151三suite；攻受击配置解耦/真实shake对象+失效状态、FOV纯计算/PCM创建复用移除；Counter与资源播放证据区分。 |
| Source/PolyQuest/Private/Tests/EnemyAttackSetAutomationTests.cpp | 239 | 已核验／1-239 | CP-31 Tests 数据与几何；完整239行；Graph+gap补全；权重边界/overflow/距离不预过滤Profile，真实expected对象比较。 |
| Source/PolyQuest/Private/Tests/EnemyCombatSpacingTests.cpp | 681 | 已核验／1-681 | CP-37 Tests AI 间距与朝向；全文；1-200真实Profile/几何核验；202-676模拟lambda未调用生产编排，F13测试覆盖缺口，保留各边界目标。 |
| Source/PolyQuest/Private/Tests/EnemyCombatTargetRetentionAutomationTests.cpp | 288 | 已核验／1-288 | CP-36 Tests Launch 状态与AI目标保留；全文；perception/revalidation test入口、丢视野/距离/leash/非目标/UnPossess迟到与RM恢复；无实际Sight/导航运行证明。 |
| Source/PolyQuest/Private/Tests/EnemyDeathRagdollAutomationTests.cpp | 385 | 已核验／1-385 | CP-38 Tests 敌人死亡；全文；真实致死GE/context隔离、无来源/重合/0速度、重复死亡幂等；passive fixture关闭ragdoll，只证明pending/capture/consume逻辑，不证明物理Impulse或视觉。 |
| Source/PolyQuest/Private/Tests/EnemyLaunchReactionRootMotionAutomationTests.cpp | 793 | 已核验／1-793 | CP-51 Tests Enemy LaunchRootMotion；Graph+全部gap；ASC grant后CallActivate绕过CanActivate，显式bypass；RM元数据/朝向/ledge/poise deferral/UnPossess/destroy/late callback；getter无消费，setter488有用。 |
| Source/PolyQuest/Private/Tests/EnemyMontageRateWindowAutomationTests.cpp | 1058 | 已核验／1-1058 | CP-47 Tests Enemy RateWindow 完成；全文1-1058；1-8 synthetic/payload；9真实ASC Small重激活/实际montage rate/旧context拒绝与scope解绑，905旧实例Stopped，不覆盖F01真实双实例授权。 |
| Source/PolyQuest/Private/Tests/EnemyRootMotionFacingAutomationTests.cpp | 395 | 已核验／1-395 | CP-37 Tests AI 间距与朝向；全文；真实Controller/RM、handoff平滑、pace重入恢复、独立FacingTag仲裁；直接触发update不证明StateTree资产调度。 |
| Source/PolyQuest/Private/Tests/EnemyStanceBreakRateWindowAutomationTests.cpp | 547 | 已核验／1-547 | CP-45 Tests StanceBreak RateWindow；Graph+全部gap；synthetic helper/真实ASC with explicit Montage bypass、tag/cleanup/CDO恢复；524-541只验手加Stunned未调用Front gate，CAND-STANCE-TEST-NOOP-01。 |
| Source/PolyQuest/Private/Tests/ExecutionHitNotifyAutomationTests.cpp | 512 | 已核验／1-512 | CP-67 Execution Hit Notify 完成；完整9节；真实Notify→ASC→Task→Health；旧Tag未注册拒绝，保持规范 |
| Source/PolyQuest/Private/Tests/ExecutionImpactFeedbackAutomationTests.cpp | 659 | 已核验／1-659 | CP-68 Execution Feedback 完成；全量反馈/一次命中/死亡延迟/异常几何与normal GE回归；计数非视觉证据 |
| Source/PolyQuest/Private/Tests/ExecutionLethalRecoveryAutomationTests.cpp | 883 | 已核验／1-883 | CP-69 Lethal Recovery 完成；完整12节；真实GE DeathPending/Release链、外部tag贡献、失败回退；兼容API候选消费确认 |
| Source/PolyQuest/Private/Tests/ExecutionLockInAutomationTests.cpp | 736 | 已核验／1-736 | CP-70 Execution Lock-In 完成；完整Context/双边握手/权限矩阵；无BeginPlay/人工ActiveCount/多重拒绝前置边界 |
| Source/PolyQuest/Private/Tests/ExecutionReleaseOutcomesAutomationTests.cpp | 1226 | 已核验／1-1226 | CP-72 Release Outcomes 完成；完整release/failure/outcome/移动mode所有权；合成Montage，直接回调无视觉证明 |
| Source/PolyQuest/Private/Tests/ExecutionSnapAlignmentAutomationTests.cpp | 304 | 已核验／1-304 | CP-31 Tests 数据与几何；完整304行；front/back/XY/Z/finite/范围/250cap/Weapon验证，保留多断言和数值边界。 |
| Source/PolyQuest/Private/Tests/ExecutionVictimPresentationAutomationTests.cpp | 1552 | 已核验／1-1552 | CP-74 Victim Presentation 完成；全量含13C/13D真实Montage_Play同步重入与按ID Stop；F14、F01边界 |
| Source/PolyQuest/Private/Tests/ExecutionVictimRootMotionAutomationTests.cpp | 532 | 已核验／1-532 | CP-75 全部 Source 审阅完成；完整532行；独立recovery/ledge恢复/外部Falling；HasSaved getter未消费，Saved getter有用 |
| Source/PolyQuest/Private/Tests/FloorVisibilityAutomationTests.cpp | 252 | 已核验／1-252 | CP-32 Tests 力竭与楼层；完整252行；分类/排Player/隐藏保留collision/直接overlap handler幂等，缺F09管理Actor先销毁。 |
| Source/PolyQuest/Private/Tests/FrontExecutionAutomationTests.cpp | 985 | 已核验／1-985 | CP-66 Front Execution 完成；全量12节；真实Front资格/输入/损伤/snap；人工StanceBreak ActiveCount；STANCE-TEST-NOOP-01裁决 |
| Source/PolyQuest/Private/Tests/HitReactionAutomationTests.cpp | 2223 | 已核验／1-2223 | CP-55 Tests HitReaction 完成；全文2223；classifier/CDO/impact/current selector、旧速度705-928/Notify551、Health与Deferral GE顺序真实dispatch、synthetic Small回调；末段不再dispatch原ASC，未证5节lambda悬空触发。 |
| Source/PolyQuest/Private/Tests/LaunchFacingSmoothingAutomationTests.cpp | 346 | 已核验／1-346 | CP-36 Tests Launch 状态与AI目标保留；Graph+补齐全部gap；状态冻结/commit顺序/复位及真实TurnTask tick、短弧同步完成/取消/非法参数；退役岛候选需资产readback，不删不同前置断言。 |
| Source/PolyQuest/Private/Tests/MeleeMultiTraceSourceAutomationTests.cpp | 712 | 已核验／1-712 | CP-44 Tests 多近战源；全文；真实双源World sweep/25伤害去重/右左独立窗口、任一端点失效整窗关闭、profile与source拒绝、NotifyState/Trail token；9节tracking不冒充Niagara。 |
| Source/PolyQuest/Private/Tests/MeleeTraceSourceComponentAutomationTests.cpp | 396 | 已核验／1-396 | CP-43 Tests 近战源与拖尾；全文；reflected static def与实际component、socket/错type/mesh/NaN/fallback、FinishSpawning碰撞契约；Legacy路径真实被测试，须资产保护。 |
| Source/PolyQuest/Private/Tests/MeleeTraceWindowLifecycleAutomationTests.cpp | 333 | 已核验／1-333 | CP-42 Tests 蓄力反馈与Trace生命周期；全文；真实ASC宿主/bootstrap释放、scalar/map open-keep-replace/close、同步失败与null actorinfo；两route边界不得合并删断言。 |
| Source/PolyQuest/Private/Tests/MeleeWeaponTrailAutomationTests.cpp | 460 | 已核验／1-460 | CP-43 Tests 近战源与拖尾；全文；真实ASC Task/world tick、双requester旧清理/端点失效/Player+Enemy销毁/null资产；GetAutoDestroy有79行直接消费者，保留。 |
| Source/PolyQuest/Private/Tests/ParrySuccessImpactFeedbackAutomationTests.cpp | 394 | 已核验／1-394 | CP-56 Tests 防御反馈；Graph+全部gap；真实Melee/Projectile resolver与poise、hitstop world tick/真实CameraShake实例，注入parry window+audio bypass；不测F03反复Begin。 |
| Source/PolyQuest/Private/Tests/PlayerActionWindowAutomationTests.cpp | 866 | 已核验／1-866 | CP-50 Tests 玩家动作窗口；全文；Dodge真实CanActivate与各cancel-window直接事件、Charged pause latch、Launch阶段门槛/cleanup；手工tag重激活不是ASC重激活；不含Parry Begin计数F03。 |
| Source/PolyQuest/Private/Tests/PlayerDefenseAudioAutomationTests.cpp | 608 | 已核验／1-608 | CP-56 Tests 防御反馈；全文；实际Guard GE吸收/投射物允许Guard、nonlethal音效dispatch/同spec双Health去重+不同spec各触发、profile错配；audio bypass无听觉证明，602恒真只作smoke。 |
| Source/PolyQuest/Private/Tests/PlayerExhaustionAutomationTests.cpp | 242 | 已核验／1-242 | CP-32 Tests 力竭与楼层；完整242行；真实World/ASC与Jump零耗、外部tag贡献、动作延迟、默认2s恢复/死亡/销毁；缺F06零时间。 |
| Source/PolyQuest/Private/Tests/PlayerLaunchReactionRootMotionAutomationTests.cpp | 713 | 已核验／1-713 | CP-52 Tests Player LaunchRootMotion；全文；synthetic RM/CallActivate、任务持续监听/真实ASC event/cancel、不同初始ledge、fall/UnPossess/destroy；末尾force cleanup只有限证明，不能称销毁自动结束已独立断言。 |
| Source/PolyQuest/Private/Tests/PlayerLockOnAutomationTests.cpp | 1349 | 已核验／1-1349 | CP-59 Lock-On 测试全量核验；完整静态审阅；视口/排序/锁定/LOS/朝向，2C真实ASC及Montage委托链；F05/F07缺口 |
| Source/PolyQuest/Private/Tests/PlayerMeleeMotionWarpingAutomationTests.cpp | 1759 | 已核验／1-1759 | CP-61 MotionWarping 完成与退役闭包更正；全量几何/四动作快照/取消矩阵；synthetic/bypass边界；退役fixture仍承载Guard取消契约 |
| Source/PolyQuest/Private/Tests/PlayerMobileBowAutomationTests.cpp | 299 | 已核验／1-299 | CP-35 Tests 移动弓与冷却取消补证；全文；真实ASC/primary instance移动GE、Sprint取消桥、正常/取消清理与外部GE共存；Draw/Hold/Release直接注入不证明真实Montage转换。 |
| Source/PolyQuest/Private/Tests/PlayerMontageRateWindowAutomationTests.cpp | 1366 | 已核验／1-1366 | CP-49 Tests Player RateWindow 完成；全文；Light真实同资产旧live实例915-954保护已测；五consumer template真实ASC/entry phase/cleanup，1217默认stop替换不等于旧live；保留已有template复用。 |
| Source/PolyQuest/Private/Tests/ProjectileFlightTrailAutomationTests.cpp | 614 | 已核验／1-614 | CP-40 Tests 投射物拖尾与生命周期前段；全文；component CDO、null/socket/fallback、collision手工广播与Niagara tracking、真实World timer超时/EndPlay；mock system与testActive不证明渲染。 |
| Source/PolyQuest/Private/Tests/ProjectileLifecycleAutomationTests.cpp | 1001 | 已核验／1-1001 | CP-41 Tests 投射物生命周期完成；全文1-1001；定义/弓装备、真实resolver伤害、手工碰撞、postlaunch快照污染、Bow事件身份seam及locked/auto/fallback；无Guard方向反例、无完整实际输入Montage发射。 |
| Source/PolyQuest/Private/Tests/ProjectileTargetAssistAutomationTests.cpp | 560 | 已核验／1-560 | CP-39 Tests 投射物辅助瞄准；Graph+全部gap；定义边界/真实target查询与screen hook/标量ray/actor Tick homing；排序只两候选不含F05环，native team不含F08 BP override；无真实飞行积分。 |
| Source/PolyQuest/Private/Tests/SkillBarHudAutomationTests.cpp | 498 | 已核验／1-498 | CP-34 Tests 技能栏与交互；完整498行（Graph+全部缺口）；真实GAS cooldown/handle/pending remove/解绑；取消测试CDO改写疑点待UE实例时序补证。 |
| Source/PolyQuest/Private/Tests/TestExhaustionMoveSpeedGE.cpp | 14 | 已核验／1-14 | CP-28 Tests 公共fixture与GE类型；完整反射测试类/构造；属性/GE duration/SetByCaller各不相同，确有测试fixture消费；不以短类为冗余。 |
| Source/PolyQuest/Private/Tests/TestExhaustionMoveSpeedGE.h | 15 | 已核验／1-15 | CP-28 Tests 公共fixture与GE类型；完整反射测试类/构造；属性/GE duration/SetByCaller各不相同，确有测试fixture消费；不以短类为冗余。 |
| Source/PolyQuest/Private/Tests/TestGuardStaminaCostGE.cpp | 19 | 已核验／1-19 | CP-28 Tests 公共fixture与GE类型；完整反射测试类/构造；属性/GE duration/SetByCaller各不相同，确有测试fixture消费；不以短类为冗余。 |
| Source/PolyQuest/Private/Tests/TestGuardStaminaCostGE.h | 18 | 已核验／1-18 | CP-28 Tests 公共fixture与GE类型；完整反射测试类/构造；属性/GE duration/SetByCaller各不相同，确有测试fixture消费；不以短类为冗余。 |
| Source/PolyQuest/Private/Tests/TestHitFeedbackCameraShake.cpp | 19 | 已核验／1-19 | CP-29 Tests 冷却、反馈与Poise类型；完整；隔离真实GE/ASC或camera对象，数值与身份差异有目的；正式Automation调用在后续逐文件核验。 |
| Source/PolyQuest/Private/Tests/TestHitFeedbackCameraShake.h | 61 | 已核验／1-61 | CP-29 Tests 冷却、反馈与Poise类型；完整；隔离真实GE/ASC或camera对象，数值与身份差异有目的；正式Automation调用在后续逐文件核验。 |
| Source/PolyQuest/Private/Tests/TestJumpGameplayEffects.cpp | 31 | 已核验／1-31 | CP-28 Tests 公共fixture与GE类型；完整反射测试类/构造；属性/GE duration/SetByCaller各不相同，确有测试fixture消费；不以短类为冗余。 |
| Source/PolyQuest/Private/Tests/TestJumpGameplayEffects.h | 36 | 已核验／1-36 | CP-28 Tests 公共fixture与GE类型；完整反射测试类/构造；属性/GE duration/SetByCaller各不相同，确有测试fixture消费；不以短类为冗余。 |
| Source/PolyQuest/Private/Tests/TestLaunchFacingSmoothingAbility.cpp | 105 | 已核验／1-105 | CP-30 Tests 任务宿主；完整；真实TurnTask/ASC host和计数，CAND-LAUNCH-SMOOTH-01退休闭包内，保留至Notify/Tag readback。 |
| Source/PolyQuest/Private/Tests/TestLaunchFacingSmoothingAbility.h | 66 | 已核验／1-66 | CP-30 Tests 任务宿主；完整；真实TurnTask/ASC host和计数，CAND-LAUNCH-SMOOTH-01退休闭包内，保留至Notify/Tag readback。 |
| Source/PolyQuest/Private/Tests/TestMeleeTrailAbility.cpp | 128 | 已核验／1-128 | CP-30 Tests 任务宿主；完整；真实TraceTask与Notify identity替换，测试驱动≠正式Ability所有回调集成，保留。 |
| Source/PolyQuest/Private/Tests/TestMeleeTrailAbility.h | 54 | 已核验／1-54 | CP-30 Tests 任务宿主；完整；真实TraceTask与Notify identity替换，测试驱动≠正式Ability所有回调集成，保留。 |
| Source/PolyQuest/Private/Tests/TestMobileBowMoveSpeedGE.cpp | 14 | 已核验／1-14 | CP-29 Tests 冷却、反馈与Poise类型；完整；隔离真实GE/ASC或camera对象，数值与身份差异有目的；正式Automation调用在后续逐文件核验。 |
| Source/PolyQuest/Private/Tests/TestMobileBowMoveSpeedGE.h | 15 | 已核验／1-15 | CP-29 Tests 冷却、反馈与Poise类型；完整；隔离真实GE/ASC或camera对象，数值与身份差异有目的；正式Automation调用在后续逐文件核验。 |
| Source/PolyQuest/Private/Tests/TestMobileBowSprintAbility.cpp | 21 | 已核验／1-21 | CP-29 Tests 冷却、反馈与Poise类型；完整；隔离真实GE/ASC或camera对象，数值与身份差异有目的；正式Automation调用在后续逐文件核验。 |
| Source/PolyQuest/Private/Tests/TestMobileBowSprintAbility.h | 18 | 已核验／1-18 | CP-29 Tests 冷却、反馈与Poise类型；完整；隔离真实GE/ASC或camera对象，数值与身份差异有目的；正式Automation调用在后续逐文件核验。 |
| Source/PolyQuest/Private/Tests/TestParryCounterPoiseGE.cpp | 19 | 已核验／1-19 | CP-28 Tests 公共fixture与GE类型；完整反射测试类/构造；属性/GE duration/SetByCaller各不相同，确有测试fixture消费；不以短类为冗余。 |
| Source/PolyQuest/Private/Tests/TestParryCounterPoiseGE.h | 18 | 已核验／1-18 | CP-28 Tests 公共fixture与GE类型；完整反射测试类/构造；属性/GE duration/SetByCaller各不相同，确有测试fixture消费；不以短类为冗余。 |
| Source/PolyQuest/Private/Tests/TestPoiseRecoveryGE.cpp | 68 | 已核验／1-68 | CP-29 Tests 冷却、反馈与Poise类型；完整；HealthFirst/PoiseFirst顺序是回归前置，不能合并顺序差异；三GE各有契约。 |
| Source/PolyQuest/Private/Tests/TestPoiseRecoveryGE.h | 54 | 已核验／1-54 | CP-29 Tests 冷却、反馈与Poise类型；完整；HealthFirst/PoiseFirst顺序是回归前置，不能合并顺序差异；三GE各有契约。 |
| Source/PolyQuest/Private/Tests/TestPreparedSkillCooldownFixtures.cpp | 71 | 已核验／1-71 | CP-29 Tests 冷却、反馈与Poise类型；完整；真实GAS冷却GE/Ability，空SetTestCooldownDuration无消费者，CAND-TEST-EMPTY-01。 |
| Source/PolyQuest/Private/Tests/TestPreparedSkillCooldownFixtures.h | 52 | 已核验／1-52 | CP-29 Tests 冷却、反馈与Poise类型；完整；真实GAS冷却GE/Ability，空SetTestCooldownDuration无消费者，CAND-TEST-EMPTY-01。 |
| Source/PolyQuest/Private/Tests/TestProjectileDamageGE.cpp | 14 | 已核验／1-14 | CP-29 Tests 冷却、反馈与Poise类型；完整；隔离真实GE/ASC或camera对象，数值与身份差异有目的；正式Automation调用在后续逐文件核验。 |
| Source/PolyQuest/Private/Tests/TestProjectileDamageGE.h | 18 | 已核验／1-18 | CP-29 Tests 冷却、反馈与Poise类型；完整；隔离真实GE/ASC或camera对象，数值与身份差异有目的；正式Automation调用在后续逐文件核验。 |
| Source/PolyQuest/Private/Tests/TestStaminaRegenGE.cpp | 6 | 已核验／1-6 | CP-28 Tests 公共fixture与GE类型；完整反射测试类/构造；属性/GE duration/SetByCaller各不相同，确有测试fixture消费；不以短类为冗余。 |
| Source/PolyQuest/Private/Tests/TestStaminaRegenGE.h | 18 | 已核验／1-18 | CP-28 Tests 公共fixture与GE类型；完整反射测试类/构造；属性/GE duration/SetByCaller各不相同，确有测试fixture消费；不以短类为冗余。 |
| Source/PolyQuest/Private/Tests/VitalHudAutomationTests.cpp | 1013 | 已核验／1-1013 | CP-33 Tests Vital HUD；完整1013行（CodeGraph+缺口+后半）；真实UMG属性与ASC委托、默认flash/渐隐；缺F10短duration；F11 timing断言矛盾。 |
| Source/PolyQuest/Private/Tests/WeaponEquipmentComponentAutomationTests.cpp | 1404 | 已核验／1-1404 | CP-63 Equipment 完成；全量事务/失败rollback/双手布局/route与资产CDO检查；TEST-DROP-BOUNDS-01 |
| Source/PolyQuest/Private/Tests/WorldInteractionPromptAutomationTests.cpp | 312 | 已核验／1-312 | CP-34 Tests 技能栏与交互；完整312行；空widget/本地化、手工候选/门禁/移动与repossess；非真实BeginPlay场景，不证明实际Overlap注册。 |
| Source/PolyQuest/Private/Tests/WorldPickupGroundingAutomationTests.cpp | 187 | 已核验／1-187 | CP-31 Tests 数据与几何；完整187行；corner-ground clearance、旋转/缩放/斜面/失败清零，测试计算物理约束无生产替代。 |

### 外围清单（不计入 Source 230 文件）

| 路径／对象 | 已完成检查与保留边界 |
|---|---|
| PolyQuest.uproject | CP-03 全文；唯一自有 Runtime Module 为 PolyQuest，Target/Build 对齐；插件消费须考虑编辑器与资产，不按无C++调用删除 |
| Config/DefaultEngine.ini | CP-03；地图／渲染／平台／碰撞入口核验；13/14、38/39两组同值键列 CAND-CONFIG-DUP-01；未修改 |
| Config/DefaultGame.ini | CP-03；游戏／AssetManager配置核验，GameFeatureData扫描缺无消费证明，保留 |
| Config/DefaultInput.ini | CP-03；输入声明核验；-/+AxisConfig属于数组替换，保留，不按重复删除 |
| Config/DefaultEditor.ini | CP-03；三个大型引擎预览序列化条目按Profile／资源引用／启用项核验，不当游戏实现精简 |
| Config/DefaultEditorPerProjectUserSettings.ini | CP-03；本地Editor设置，非游戏运行时清理对象，保留 |
| Config/Tags/PolyQuestGameplayTags.ini | CP-03及生产／Tests接缝核验；旧Launch/Commit仍有原生与潜在资产入口，按RET-LaunchClosure等待readback，不批准直接删除 |
| Config/Tests/Tags.ini | CP-03；测试映射与真实Automation注册核验，保留 |
| Config/Automation/Presets/UI.json | CP-03；预设名与实际测试注册对账；预设存在不证明用例已运行或通过 |
| Config/Automation/Presets/1.json | 既有删除WIP，继续缺失；不恢复，不拿历史版本冒充在盘配置 |
| tmp/pdfs/inspect_layout.py | 已盘点为临时PDF布局检查脚本，与游戏运行／构建无关；排除游戏瘦身，保留WIP |
| Plugins/ 与正式工具目录 | 已复核未发现项目级自有插件／正式工具目录；没有因未跟踪漏掉的额外自有游戏源码 |
| .codex/config.toml | 项目本地代理配置；保留，不作为游戏源码瘦身目标 |
| Content/** | 资产／反射／软引用证据边界；未操作Editor或读取蓝图图表，旧Launch、Notify、Tag、软引用及BP阵营覆写缺口见第8节 |
| 引擎／第三方／生成及构建缓存 | 非自有删除范围；仅查实际消费的本机UE5.8接口，0个已证实可移除模块／插件依赖 |

## 7. 检查点与本轮文档交付收据

### CP-00：计划落盘（2026-09-12）

- 用户确定范围：仅落盘计划、230 文件待审清单及交接指针；全量只读审计留待后续明确启动。
- 基线：HEAD `9f2d8e0b1b8c34dabc4f17bfb6aea6b0a2adb229`；Source 与 `759efca` 无差异；新增已完成文件 0/230，已完成源码区段为无。
- 继承：H6 两条 P2、绑定重复、11 项 getter 候选及其原证据边界，详见第 4.1 节。未生成新的全项目 Findings 或收益统计。
- 待解问题：230 文件与外围项的实际覆盖、新增候选、动态资产消费者及净收益均待后续审计；这些是未执行工作，不是已接受延期的源码覆盖。
- 下一入口：用户明确启动审计后，复核基线／WIP／清单变化，进入模块与构建入口 5 文件及相关项目／配置声明；按第 2 节检查点推进。
- 文档验证：230 条待审记录与当前 Source 路径逐一对账，无缺失、重复或额外项；H6 历史计划、继承项和路线图章节引用已对齐；三份白名单文档的 `git diff --check` 通过。
- 保护核验：Source、AGENTS.md、ARCHITECTURE.md、PolyQuest.uproject 及在盘 Config／tmp 文件的 SHA-256 汇总与启动快照一致；archive 原有 386,160 字节前缀哈希未变，仅追加交接。排除三份白名单后，2,122 项原有 Git 状态逐项一致；HEAD 未变，暂存区仍为空。
- 本轮未运行编译、Automation、Editor／PIE、视觉／网络／打包验证；未派发子代理、未执行 Fresh Review；未暂存、提交或推送。

### CP-00 文档提交批准（2026-09-12）

- 用户明确回复“可以提交”，授权单独提交 plan.md、ROADMAP.md、ROADMAP-archive.md；保留原 H6 提交 9f2d8e0，不 amend、不推送，不包含 Source、Config、Content、tmp 或其他 WIP。
- FULL-AUDIT 就是当前阶段；本提交仅形成其已批准计划与 230 文件待审清单的版本基线，不表示全量审计完成，也不自动启动审计。当前阶段接下来的工作是执行本计划；后续阶段从 FIX1 开始。
- 上述 CP-00 的“未提交／HEAD 未变”等收据保留为本次提交批准前的检查时点。最终提交哈希与包含路径以 Git 历史为准；本次提交不增加编译、Automation、Editor／PIE 或 Fresh Review 证据。

### CP-02：Main 接管与证据纠正（2026-09-12）

- 执行基线：d99ddcd54b70d043265479719ab74f05a48ed144。Source 与已提交计划基线无差异；三份文档包含 Gemini 待验收结果，用户现已授权 Main 补审并在完成后提交。
- Gemini 初稿的“230/230 全覆盖”、错配分组行数、“31 处 ponytail 标记全部对账”、H6-F01 已由既有测试覆盖及 987 行净收益不予采纳；其历史归档保留，追加本纠正说明。
- 既有 H6-F01/F02、绑定重复和 11 项 getter 候选保留。CAND-LAUNCH-SMOOTH-01 作为有条件退役候选继续核验：九个列举文件实际 357 行生产代码、517 行测试代码，共 874 个物理行；HitReactionAutomationTests 等仍有 Notify/Tag 消费者，不能按九文件直接删除。
- 证据标准：完整读取记录不等于审计通过；本轮重新记录完整实现、直接消费者、测试和结论。未运行的测试不得写“通过”。归档仅追加，源码与原有 2,122 项 WIP 保留。
- 执行授权：仅 plan.md、ROADMAP.md、ROADMAP-archive.md；本轮完成审计后可单独提交，不推送，不进入 FIX1。
- 当前补审覆盖：0/230。下一入口：模块与构建入口及项目配置。

### CP-03 构建入口与外围配置

- 核验模块注册与日志宏、Build/Target 全文、uproject 声明及 8 个在盘 Config。CodeGraph 未返回模块宏文件时精确读取；DefaultEditor 的三条大体积引擎预览序列化按 Profile/资源引用/启用覆盖项核验，不当作自有运行时代码。
- 发现 CAND-CONFIG-DUP-01：DefaultEngine.ini:13/14 的同值自动曝光布尔键和 :38/39 的相同 DefaultGraphicsRHI 重复，各保留一项即可；仅作为后续两行配置清理候选，不在本轮修改。Input 的 -/+AxisConfig 是配置数组替换，不按表面重复删除；非目标平台/控制器默认项保留。
- 三项测试配置名均找到真实 Automation 注册；编辑器工具插件与 GameFeatureData 扫描保留，缺少完整插件运行/资产消费证据时不声称无多余依赖。Tag 注册接缝留待对应生产和测试读取核验。下一批：AbilitySystem 基础 Attribute/Task/helper。

- 累计完整文件记录：5/230；未完成文件保持待审或部分完成。

### CP-04 Attribute、Task 与窗口 helper

- CodeGraph 对 C++ Task 仅返回片段，故对遗漏头源作全文精确读取；逐文件记录明确继承声明和本轮完整实现，不用图谱大小推断覆盖。
- 新疑点：AbilityTask_MeleeTraceWindow.cpp:255 调用同步 HitResolver，返回后 :262/263 仍索引可被 OnDestroy 清空的 ActiveSourceSamples。已确认 Parry 会同步向攻击者施加 Poise GE；下一批核验 Enemy Poise→StanceBreak→取消→EndTask 链后裁定，不在本轮修复。
- CAND-ATTR-CLAMP-01：CharacterAttributeSet.cpp:40-60 与 :68-88 为相同属性 Clamp 政策，两个 UE Hook 本身保留；可由同一个 const 私有校验函数集中政策，最终净收益需扣除接口与必要测试，暂不计量。Trace Scalar/Map 两个入口的参数语义不同，不能机械合并。
- 下一批：Enemy Melee/StanceBreak 完整头源与上述直接取消链。

- 累计完整文件记录：16/230；未完成文件保持待审或部分完成。

### CP-05 Enemy Melee／StanceBreak

- 完整核验四个头源。H6-F01 保持条件性实例授权缺陷；StanceBreak 也只凭 Context/Active 和共享 helper 资产有效性授权，FIX1 需独立冻结其身份接入，不能机械忽略或改写恢复权属。
- CP-04 的直接 Parry 反例不成立：EnemyCharacter.cpp:901/902 用 SetTimerForNextTick 派发 StanceBreak，故不会沿这条已存在路径在当前 TraceCurrentSegment 中同步取消任务。本轮不把纯理论回调重入升级为新崩溃 Finding。
- 已读直接链区段：MeleeHitResolver.cpp 全文、PlayerParryAbility.cpp:243-278、PlayerCharacter.cpp:641-701、EnemyCharacter.cpp:820-975；相应大文件须在所属批次补齐其他区段后才标全文件完成。下一批：基础移动与 Primary 路由 Ability。

- 累计完整文件记录：21/230；未完成文件保持待审或部分完成。

### CP-06 基础移动与 Primary 路由

- 完整审阅 Jump/Sprint/Primary/StaminaAction 共八文件。零体力跳跃和正体力最后一次动作均为既有契约；保留 cooldown、成本与恢复延迟差异，未因表面重复建议合并。
- Sprint drain 的 Handle 赋值与角色耗尽回调顺序作为明确接缝问题留待 Character 批，不据假定立即扣费标缺陷。下一批：Light/Charged/SprintAttack 动作族完整读取。

- 累计完整文件记录：29/230；未完成文件保持待审或部分完成。

### CP-07 Light／Charged／SprintAttack

- 完整补齐三组头源；CodeGraph 大段缺失，精确源码覆盖全部遗漏。现有 RateWindow 当前映射实例校验与旧实例存活校验不能混称；绑定样板继承 SHRINK-A。
- CAND-WARP-DUP-01：ChargedAttackAbility.cpp:647-800 与 SprintAttackAbility.cpp:455-608 的 TryApplyMeleeMotionWarpTarget 在类名归一后逐语义相同；Light.cpp:919-1072 共享目标捕获/验证流程但有跨段快照及入口配置差异。建议后续 SHRINK-Warp 在既有 MeleeMotionWarping 层收敛 Charged/Sprint 同步流程，保持每 Ability 的 Snapshot 和原调用时机，不新建公共 Ability 基类。两份各 154 物理行，净收益须扣共享实现和调用迁移，暂不等于 308 行删除。
- Charged 的底层 Sequence 事件兼容与 Light/Sprint 仅 Montage 身份有差异，缺资产迁移证据时保留；不盲目提取事件判断。下一批：MeleeSkill、Bow 与 Dodge。

- 累计完整文件记录：35/230；未完成文件保持待审或部分完成。

### CP-08 MeleeSkill／Bow／Dodge

- 完整读取三组头源；MeleeSkill 的先确认 Montage 再提交 cost/cooldown 与 SprintAttack 先提交成本有不同既定事务边界，不能为合并取消保护条件。
- MeleeSkill.cpp:588-741 与 Charged/Sprint 的 MotionWarp 主流程相同，追加 CAND-WARP-DUP-01；MeleeSkill.cpp:775-847 与 Dodge.cpp:445-517 的 BindRateWindow 可按动作类型归一对照 SHRINK-A；Bow 还需保留 bEndAbilityInProgress 差异。
- Bow 的内部 EBowState 由 Ability 独占，用于动作内阶段及单发门槛；不是 GAS 外部并行状态机。BowSection 和 Notify 为资产入口，仅凭原生引用不能退役。下一批：玩家防御与轻/重受击反应。

- 累计完整文件记录：41/230；未完成文件保持待审或部分完成。

### CP-09 玩家防御与轻重受击

- 完整审阅十文件。Guard 吸收耗尽体力的当前一击；Parry 只在自然结束提交冷却；Small overlay 与 Big 全身/ledge 约束不同，保留这些显式动作边界。
- H6-F03（静态条件性状态清理缺陷，非已运行复现）：PlayerParryAbility.cpp:364-379 每次合法 Begin 都 AddLooseGameplayTag；:382-397 与 :181-188 由一个 bool 决定最多 Remove 一次。相同激活内 Begin→Begin→End→End/EndAbility 使 ParryActive 计数 0→1→2→1，残留 1。当前 NotifyState_ActionWindows.cpp:152-159 可产生这些事件，未禁止重叠 authored 窗口；UE5.8 AbilitySystemComponent.h:656-668 确认加减计数语义。现有原生拦截使用 IsParryActive() 的 bool，因此不能称为永久弹反；准确影响为 ASC loose tag 清理不完整。后续独立 FIX-ParryWindow，先固定单窗口幂等或重叠窗口身份语义，补重复 Begin/取消/再次激活计数测试；不自动增加 03C 门禁。资产是否存在重叠本轮未知。
- Guard/Parry 的小型前弧几何和部分音效位置选择相似，但事务/反馈不同，暂保留；若 FIX2 需要共用几何，应按其明确白名单处理，不预建防御基类。
- AnimNotifyState_ActionWindows.cpp 195 行已由 CodeGraph 全文读完，Animation 批补头文件和其余消费者后登记。下一批：敌人轻/重/Launch 与玩家 Launch。

- 累计完整文件记录：51/230；未完成文件保持待审或部分完成。

### CP-10 Enemy 受击与双方 Launch

- 八文件全部静态核验。Enemy Big/Small/Launch 的 RateWindow Begin/End/Restore 均仍用 GetMontageInstanceForID 的旧实例存活校验，与 F01 同根归 FIX1；保留单条 Finding，不按消费者重复计数。
- CAND-REACTION-YAW-01：EnemyLaunchReactionAbility.cpp:485-528 与 PlayerLaunchReactionAbility.cpp 的 TryResolveRootMotionFacingYaw 为相同纯几何/finite 防护；后续 SHRINK-Reaction 可放入既有 Reaction 私有 helper，两个 Ability 仅保留调用，不合并取消/落地/Deferral 逻辑。净收益待扣函数声明、迁移和测试。
- Launch 的两状态 enum 当前仍表达活跃 knockdown 与无阶段；不能仅因历史曾多阶段就宣布死代码。CDO 测试注入、双重 Montage Stop 等需结合真实测试审阅裁决，不以名字直接删。下一批：前/背处决与 Victim 事务。

- 累计完整文件记录：59/230；未完成文件保持待审或部分完成。

### CP-11 前处决与背刺

- 四文件完整读取。H6-F04／P1（静态构建变体缺陷）：两头文件 :75-122 的 WITH_DEV_AUTOMATION_TESTS 包含 :90 的 TestEvaluateFront/BackstabGeometryVectors 声明，而 Front.cpp:336/347、Backstab.cpp:376/387 的生产调用/成员定义无条件保留。UE5.8 UEBuildTarget.cs:6313-6328 对 Test/Shipping 默认定义该宏为 0；当前 Target/Build 未强制开发测试。宏关闭后声明消失却仍定义/调用成员，构建无法通过；未执行编译。归独立 FIX-BuildGuard：生产几何留在正式私有声明/实现，测试薄封装留宏内；验证 Development+Shipping/Test 宏组合，不通过强开测试规避。
- CAND-EXEC-SNAP-01：Front.cpp:397-485 与 Backstab.cpp:439-527 的碰撞忽略、sweep、1cm/1deg 验证与失败位置回滚相同，只有 EExecutionSnapSide 不同；后续 SHRINK-Execution 可在既有 ExecutionSnapAlignment 私有实现提供统一施加操作。保留两 Ability 的前/背门槛、命中距离/夹角重查差异与各自 Context，不提取整个处决基类。
- CAND-BACKSTAB-HANDLE-01：Backstab.h:211 的非反射私有 TargetStunnedTagDelegateHandle 无任何读取/写入/注册，完整头源及全 Source 搜索仅该声明；Front.h 同名字段有 :1104 注册/:1134-1138 解绑，必须保留。建议删背刺单字段，净 1 行，归 SHRINK-Execution。
- 下一批：EnemyVictimExecutionAbility 的授权命中、DeathPending、非致死恢复与 RateWindow。

- 累计完整文件记录：63/230；未完成文件保持待审或部分完成。

### CP-12 Victim 处决事务

- 1,572 行实现与 295 行头完整审阅，未以 H6 旧结论跳过非 RateWindow 路径。死亡待定由授权 Hit scope 驱动，最终死亡与非致死 RootMotion 恢复分别有清理出口；保存的 ledge/移动、AI 锁和上下文收据均保留。
- H6-F01 的 Victim RateWindow 是旧实例存活+资产调速接缝；StopVictimMontagePresentation 本身按实例 ID Stop，不能将这两种权限混为一谈或删除 newborn 捕获保护。BoundAnimInstance 是测试环境注入字段，其使用均按 WITH_DEV_AUTOMATION_TESTS 封闭，不属于 H6-F04。
- Backstab.cpp:26-50 与 Victim.cpp:21-45 的 StanceBreakCompatibility 规则重复，允许在 SHRINK-Execution 的独立纯规则片评估复用；请求方与已激活 Victim 的 Stunned 计数先扣自身贡献差异必须保留。处决两端的握手/释放完整状态机不纳入大范围基类重构。
- AbilitySystem 60/60 已有完整记录。下一批：AI 8 文件，再 Animation/Camera；测试仍须逐文件完成。

- 累计完整文件记录：65/230；未完成文件保持待审或部分完成。

### CP-13 AI 全单元

- 八文件完整静态核验：StateTree 是条件与动作编排，Controller 管目标/pending/导航/时间节奏，Ability 返回 GAS 动作真值及冷却开始；没有引入并行战斗状态机。
- InRange/Outside/AttackReady 等显式反射节点不以薄 wrapper 或空 InstanceData 判死代码。已核对本机 UE5.8 StateTreeConditionBase.h:82，FStateTreeConditionCommonBase 没有 bInvert 属性；不得把其他 UE 版本的 bInvert 经验套进当前代码报错。
- 原生 target 有效性与角色死亡的消费行为在 Character/Combat 接缝继续对账；已看全部 AI 文件不等于该接缝已有运行复现。导航 request ID 匹配、失败侧重试、任务 Exit 停止与 pace 归还保留。下一批：Animation 和 Camera。

- 累计完整文件记录：73/230；未完成文件保持待审或部分完成。

### CP-14 Animation／Camera

- Animation 10 与 Camera 2 文件全部静态核验。Notify 只发送事件，不取得伤害/动作状态权威；执行方按 payload 校验。退役 LaunchCommit 仍为有条件候选，不能在缺少资产 readback 时批准删除。
- CAND-NOTIFY-DUP-01：AnimNotifyState_ActionWindows.cpp:13-37 的 SendGameplayEvent 与 :39-64 的 SendGameplayEventWithMagnitude 仅多 EventMagnitude 赋值，调用方共有固定窗口及 RateWindow。建议同文件一个默认 magnitude=0 的内部函数，保留事件标签、OptionalObject/2、日志与反射类，预计净 20-25 行；归 SHRINK-Notify，不新建跨模块事件总线。Bow/Execution 几个短路由暂不为复用新建框架。
- FOV Punch 使用 UE CameraModifier 对当前 POV 叠加，未写回基准 FOV；无需替换为自建相机状态系统。下一批：BaseCharacter、PlayerLockOnTargeting、EnemyCharacter。

- 累计完整文件记录：85/230；未完成文件保持待审或部分完成。

### CP-15 BaseCharacter／LockOn 几何

- 四文件完整读取。ASC Owner/Avatar 均 Character，启动授予去重；Overlay 定时恢复检查当前材质所有者，不覆盖外部更改。固定 WeaponMesh 是资产命名入口，未有 readback 不按 v1 名字退役。
- H6-F05／P2（静态排序契约缺陷）：PlayerLockOnTargeting.cpp:14-26 用 IsNearlyEqual 决定角度/距离层级，再交给 :106-109 的 TArray::Sort。epsilon 等价不传递：角度 A=0/B=7.5e-9/C=1.5e-8、距离平方 A=3/B=2/C=1，得到 A<C、C<B、B<A 的比较环。本机 UE5.8 UnrealMathUtility.h:130/388-390 的默认 float tolerance=1e-8，独立 PowerShell single 数值演算确认三边均 true；不是 UE Automation 或运行复现。结果不满足严格弱序，顺时针排序与后继选择不能保证一致。
- 后续独立 FIX-LockOnOrder：使用严格字典序或明确量化后的整数键，保留 StableKey 终决；验证三候选传递性/相同角度距离/绕回，不能只测两元素；不自动增加 03C 门禁。下一批 EnemyCharacter。

- 累计完整文件记录：89/230；未完成文件保持待审或部分完成。

### CP-16 EnemyCharacter 完整补审

- 完整读完 EnemyCharacter h247/cpp1369；保留 Poise deferral 与 GE 同 Spec 去重，两者分别处理时序和身份，不合并为共享状态机。
- `OnPoiseAttributeChanged:901-902` 明确 next-tick StanceBreak，否定 Parry Poise0 在 Trace 调用内同步取消该能力的推测。死亡路径先置 teardown 标志，再取消能力、停移动和 ragdoll；UI 保存实际绑定 ASC 后解绑。
- 普通命中与处决命中反馈的授权条件不同；目前只复用现有 DispatchSound/Blood/HitStop，不建议合并完整授权分支。
- 下一入口：PlayerCharacter 完整声明与 cpp1 起，完成 stamina/Sprint、Lock-On、目标与呈现接缝。

- 累计完整文件记录：91/230；未完成文件保持待审或部分完成。

### CP-17 PlayerCharacter 声明与启动段

- CodeGraph 导航返回分段源码，未覆盖部分回退逐行读取；h633已完整审阅，cpp1-640已读。固定相机的 LookAction/MouseLookAction 是已说明的 authored 兼容声明；缺 readback 不批准删除。
- Character 提供物理输入意图和 GAS 触发；Bow requester 是呈现身份，Lock-On 是目标选择，不新建并行动作状态机。
- 下一入口：cpp641-1337，再接1476起；UnPossess 的实际清理由 UnbindSprintStateEvents 等后续实现闭合后裁决。

- 累计完整文件记录：92/230；未完成文件保持待审或部分完成。

### CP-19 PlayerCharacter 完整补审

- 完成 h633/cpp3455，Character 单元8/8。SeeThrough MPC 与 Lock-On LOS 分别服务呈现和目标许可，禁止互相替代；bounded LOS 失败闭合保留。Player Health<=0 当前明确不实现死亡（cpp2860），属于已有产品非目标，不新增“AI打尸体”缺陷。
- **H6-F06 / P2 / FIX-ExhaustionTimer**：h135-137 与 setter368-369 允许最短力竭时间0；cpp2734-2740传0给SetTimer。UE5.8 `Engine/Private/TimerManager.cpp:653-733` 仅在InRate>0排回调，否则Invalidate。`bExhaustionMinimumDurationElapsed` 只在cpp2778置true，故0配置耗尽后即使体力恢复，cpp2784拒绝清理，Exhausted/移速效果保留至其他终止路径。静态确证合法边界缺陷，未声称当前资产为0或已运行复现。修复需明确定义0为立即/下一tick满足最短等待，保留动作结束等待，验证0/正值、动作中耗尽、恢复及销毁；不自动追加03C门禁。
- Sprint 初次GE应用疑点：Player exhaustion回调只加Exhausted并施加移速，不直接取消Sprint；不能据此证明ApplyGameplayEffect返回点的handle泄漏。保留既有Sprint回调顺序，最终测试核验实际覆盖。
- **CAND-PLAYER-REACTION-DUP-01 / SHRINK-PlayerReaction**：cpp2913-2951三段仅EventTag不同，同Context/Target/Magnitude及发送；参照Enemy先选tag再统一构造，同文件净估约15-22行，保持None/Invalid与Stunned契约，无需跨Character基类。
- UnPossess只取消明确Teardown能力，Guard被既有测试刻意保留；本阶段不将“没有CancelAll”定成缺陷。物理输入重持有边界待直接父类/测试补证后判定，不扩大整体解绑契约。
- 下一入口：Combat 按Enemy配置、Equipment定义/执行、Execution、Feedback、Melee、Projectile、Reaction推进。

- 累计完整文件记录：93/230；未完成文件保持待审或部分完成。

### CP-20 Combat 定义与Enemy攻击集

- 本批12文件完整读完；CodeGraph未命中多数目标定义，记coverage fallback，使用精确完整源码。图谱额外返回WorldWeaponPickup.h119行已读，归后续Pickup批记录。
- WeaponDefinition的Reusable/Exclusive是兼容authoring分组，Deprecated_Reserved保留枚举数值3，Bow为4；不因名称Deprecated/双数组判成可删，无readback不迁移资产。
- **CAND-MELEE-VALIDATION-DUP-01 / SHRINK-WeaponValidation**：MeleeWeaponDefinition.cpp180-211与278-309的socket存在/有限/不重合验证相同，230-239与311-320共享trace数值规则。可同文件private helper统一相同规则，保留玩家完整验证与敌人仅几何准入、socket必选差异；净估25-35行（含helper及调用）。
- EnemyAttackSet先全量验证再重复验证每次加权属低收益本地简化机会；本轮保留，选择规模小且无行为缺陷，不为此建立额外阶段。
- 下一入口：ProjectileDefinition、WeaponEquipmentComponent声明与实现。

- 累计完整文件记录：105/230；未完成文件保持待审或部分完成。

### CP-21 Equipment 与世界拾取完整审阅

- 完成8文件；保留装备preflight/恢复、两手占用规则、既有最小grounding helper和Pickup事务，不以函数长为由创建通用事务框架。
- `EquipWeapon`原生API拒绝不合法两手组合；`TryEquipWorldPickup`含Unarmed fallback与displaced drop，是不同入口契约，不能直接合并二者逻辑。
- ApplyComposition失败会撤销临时grant/组件；成功rollback重新授予handle但不广播逻辑composition变化。UI是否持有旧handle待UI完整读取核验，当前仅疑点。
- `ComputeKeepIfCompatibleLayout:1288`只SetNum不Reset输出，在当前生产调用点均为新建空数组；保留，不把潜在外部复用假设写成已发生缺陷。
- 下一入口：ExecutionLockContext/SnapAlignment，再Feedback/Melee/Projectile/Reaction。

- 累计完整文件记录：113/230；未完成文件保持待审或部分完成。

### CP-22 Execution 与反馈定义

- 完成6文件。ExecutionContext是瞬态配对事务身份和release收据；HitState与ReleaseState刻画不同维度，保留，不能当GAS以外动作FSM删除。RAII禁copy，失败hit与释放结果收敛。
- Snap纯数学层是CAND-EXEC-SNAP-01后续最小共享位置；保持使用Player地面Z的既有语义，碰撞移动仍须调用端负责。
- Feedback仅承载authoring配置，Player/Enemy字段和返回类型不同；短switch不引入新模板/映射依赖。
- 下一入口：Melee trace/trail/resolver完整核验。

- 累计完整文件记录：119/230；未完成文件保持待审或部分完成。

### CP-23 Melee 接触与拖尾

- 完成9文件；Main早前已完整读完MeleeHitResolver.cpp182行，本批对账归入正式覆盖，不重复继承Gemini的未证实完成态。
- **CAND-NATIVE-ATTACH-01 / SHRINK-MeleeNative**：MeleeTraceSourceComponent.cpp12-23自写祖先循环，调用245；本机UE5.8 `SceneComponent.cpp:2794` 有同语义IsAttachedTo。ResolveConfiguredComponents先保证两个端点与display非空，后续可直接使用Native方法并删除匿名helper，净约14行（不含测试增量），验证直接/深层/不相关祖先；最终净估范围10-14行。
- `GetAutoDestroy`只有MeleeWeaponTrailAutomationTests.cpp79调用，用于检查真实Niagara内部flag；保留测试目的，可后续移入WITH_DEV_AUTOMATION_TESTS但不把有效断言当冗余。
- 三套trace source包含既有固定fixture与资产挂接契约，无Editor readback不能退休Legacy分支；不把每帧校验换成未经证实的缓存。
- Execution兼容Begin/Complete/AbortFinalization目前只有ExecutionLethalRecoveryAutomationTests消费，生产使用Outcome系列；Tests批需裁决迁移旧状态测试，暂记CAND-EXEC-COMPAT-01待定，不批准删断言。
- 下一入口：Projectile Actor/Resolver/Targeting。

- 累计完整文件记录：128/230；未完成文件保持待审或部分完成。

### CP-24 Projectile 与 Reaction 完整审阅

- 本批12文件，Combat47/47完成。Projectile碰撞terminal先关collision/移动再deactivate，timeout先排后Deactivate，能处理同步finished回调；Homing精确对齐时cpp293-296只return并保留追踪，不是永久停止。
- **H6-F05扩展（同根排序缺陷）**：CombatProjectileTargeting.cpp300-313也以近似相等作sort比较；角度容差0.01、距离1。A=(角0,距300)，B=(0.0075,200)，C=(0.015,100)产生A<C、C<B、B<A。本轮独立PowerShell标量计算三者均true，仅是数学反例验证。与LockOn同归**FIX-TargetOrder**（替代暂名FIX-LockOnOrder），保持两者不同角度单位、选择优先级；后续建立传递性与多候选集成用例。普通修复，非自动03C门禁。
- **H6-F08 / P2 / FIX-TeamDispatch**：CombatProjectileTargeting.cpp82-88优先调用`GetCombatTeamTag_Implementation`且有效就返回，绕过UCombatTeamAgent的BlueprintNativeEvent override；MeleeHitResolver.cpp46-47与Character反馈使用Execute_GetCombatTeamTag。触发为BaseCharacter原生CombatTeamTag有效且派生BP覆写不同Tag，同一Actor的投射物/LockOn与近战/反馈阵营判断不一致。静态分派缺陷成立，当前资产是否覆写未读取，不能称现有地图复现。最小修复统一官方Execute入口，验证native和BP覆写消费者；测试fixture不应迫使生产绕过反射。该FIX首次接入BP动态阵营前关闭，不自动将资产扫描扩大为03C门禁。
- **文档措辞修正依据**：当前与原H6均为“投射物允许Guard、禁用Parry”（ProjectileHitResolver.cpp64-70、历史H6-F02），原计划缩写“Guard／Parry禁用”存在歧义，最终契约表将写全，不能据其推翻已有Guard接线。
- `TryBuildLaunchFacingAndVelocity/TryBuildLaunchVelocity`当前production调用消费待最终精确对账；HitReactionAutomationTests保有大量旧速度断言，不能把这些行直接算作安全净删除。新增CAND-REACTION-LEGACY-01条件测试迁移候选，与CAND-LAUNCH-SMOOTH-01去重。
- 下一入口：Environment4、Framework4和UI10；随后Tests72全读。

- 累计完整文件记录：140/230；未完成文件保持待审或部分完成。

### CP-25 Environment 完整审阅

- 本批4文件。现有floor是空间/名字启发式呈现工具，保留碰撞，不将其视为玩法状态权威。FloorTrigger的默认查找是先遇到任一exact FloorIndex就停，否则nearest；不能声称多个同索引实例下仍按距离稳定选择，当前资产布置未核验。
- **H6-F09 / P2 / FIX-FloorVisibility**：FloorVolume.cpp289/319/352可把存活managed actor置hidden，EndPlay46-54只把MPC截止高度置10000，未撤销自身隐藏贡献。反例：楼层inactive隐藏道具/结构 → Destroy该管理Volume且这些Actor仍在world → Actor隐藏保持，材质cutoff恢复也不能显示。静态生命周期缺陷成立；未声称整地图统一卸载会保留对象。后续按拥有的隐藏状态恢复并保留外部原始hidden状态，验证inactive销毁、原本hidden、重复清理；多Volume重叠若资产确有此需求再定共享写入仲裁，不预建全局楼层管理器。
- **CAND-FLOOR-HIDE-DUP-01 / SHRINK-Environment（依赖F09裁决）**：cpp294-302/313-321/346-354重复遍历结构Actor写visibility；可同文件私有函数共用，保持激活立即显示、停用fade完才隐藏，净估8-12行；FIX-FloorVisibility重叠扣除后再计，不自动接受新阶段。
- 动态缺口：同世界多个FloorVolume写同一MPC参数、Gathers对象跨Volume/streaming生命周期与命名分类是否符合场景。需readback volume范围/引用/初始active/managed集合，不能据静态读码关闭，归环境证据债；不升级03C门禁。
- 下一入口：Framework4与UI10。

- 累计完整文件记录：144/230；未完成文件保持待审或部分完成。

### CP-26 Framework 完整审阅

- Framework4/4完整；GameMode原生根是BP继承入口，保留。Controller HUD生命周期/原ASC解绑明确，SkillBar/Prompt/Vitals后处理不同，不提取通用widget factory。
- hitstop使用RealTime到期，单调min dilation/max expiry，检测其他系统接管后放弃恢复；三销毁入口复用同一幂等Restore，保留必要兜底。
- 装备rollback UI疑点已由真实消费者排除：PlayerSkillBarHUDWidget.cpp41-50每可见tick调用RefreshAllSlots，149-175重新取得现有class/handle，不缓存旧handle。rollback不广播组成变化不会使HUD永久绑定过期handle。
- SkillBar cpp248行由CodeGraph完整读取，本批先记证据，UI批正式覆盖；下一入口为UI声明及其余9文件。

- 累计完整文件记录：148/230；未完成文件保持待审或部分完成。

### CP-27 UI 完整审阅

- UI10/10完整，生产/模块入口共158/158已读，Tests72仍待全读。Vitals/Enemy头注释声称no tick已过时，实际tick只做呈现插值；后续相关窄片修正文案，不据此指控第二套玩法状态。
- **H6-F10 / P2 / FIX-UIFlash**：EnemyHealthBarWidget.cpp224-244与PlayerVitalHUDWidget.cpp210-230复制peak-hold算法；头允许duration最小0.01，但duration<=0.03时decay<=0，timer归零仍保留Weight=1写peak白色，后续外层timer>0为false不再恢复。合法反例duration0.02、一次0.05 tick，remaining0而weight1；本轮PowerShell独立标量复算0.01/0.02/0.03均如此（仅数学验证，非Automation/Slate运行）。修复需到期无条件恢复base color，再保留短时peak语义；验证两widget、0.01/0.03/0.15、跨结束的大delta及重复受击。UI反馈普通缺陷，不阻塞03C。
- **CAND-UI-CURVES-01 / SHRINK-UI（与F10去重）**：Enemy CalculateShakeOffset133-145和Player508-520为相同阻尼正弦，flash核心上述亦相同；可私有纯数学函数复用参数，widget仍持有独立timer与UMG绑定。净估20-30行在FIX-UIFlash实际修改后重算，禁止以公共Widget基类/事件框架扩大范围。
- SkillBar每tick轮询4槽获取真实cooldown且无第二套倒计时是明确边界；InitializeSlot自身幂等，不能凭Refresh每tick调用断言每帧重建文字/MID。
- 下一入口：Tests72全量33,854行，先公共fixture与短test类型，再按测试族分块；不以生产已读替代Tests覆盖。

- 累计完整文件记录：158/230；未完成文件保持待审或部分完成。

### CP-28 Tests 公共fixture与GE类型

- Tests首批12文件完整；图谱未返回目标测试源码，使用完整文件fallback。现有FCombatAutomationFixture已覆盖前置authoring和一次BeginPlay，不再发明新通用测试框架。
- TestStaminaRegenGE是明确的persistent no-op，证明启动接线、不模拟实际体力恢复数字；后续读到使用它的测试时不能把native fixture成功说成authored regen集成证明。
- Jump零消耗和无限delay是显式观察seam；Guard体力、Parry Poise与Exhaustion MoveSpeed GE验证不同属性/操作/周期，保留独立类。
- 测试反射类未逐个套WITH_DEV_AUTOMATION_TESTS不等于可简单宏删除（UHT类声明要求单独处理）；本阶段不修改构建/模块，不建立未证实的移除依赖。
- 下一入口：其余短test类型，随后Automation执行体。

- 累计完整文件记录：170/230；未完成文件保持待审或部分完成。

### CP-29 Tests 冷却、反馈与Poise类型

- 本批12文件完整。PoiseFirst/HealthFirst专门覆盖同Spec属性顺序，不能因成员初始化相似删除任一。三种Shake类用于检验类别/实例替换，test pattern不产生实际摄像机位移，不作为视觉证明。
- **CAND-TEST-EMPTY-01 / SHRINK-B附加候选**：TestPreparedSkillCooldownFixtures.h45声明、cpp68-71空实现SetTestCooldownDuration，Source精确搜索仅这两处，无宏/虚/反射消费者；删除该非反射空接口，净5行，不影响CooldownGE或bAutoEndAbility。
- 旧Launch速度两个helper：production仅自互调，无其他生产调用；仍有HitReactionAutomationTests原生断言，待该文件审阅决定迁移/移除旧契约测试，不直接删大量断言来获取收益。
- 下一入口：TestLaunchFacingSmoothingAbility与TestMeleeTrailAbility四文件，再按Automation测试族。

- 累计完整文件记录：182/230；未完成文件保持待审或部分完成。

### CP-30 Tests 任务宿主

- 四文件完整；TestMeleeTrailAbility实际驱动trace task，和只存bool的mock不同，不能用更弱stub替换。Launch宿主承载退休的平滑状态/Task测试，纳入条件移除闭包而非独立删除。
- **更正CP-29措辞**：`TryBuildLaunchFacingAndVelocity`仍被Source中的`LaunchFacingSmoothingState.cpp:26`调用，准确结论是“已退役平滑岛内生产文件仍相互消费，未发现当前Player/Enemy Launch Ability调用”。不能写成production只有自互调。该岛整体裁决须包含此边；`TryBuildLaunchVelocity`仍需测试段完整核验。
- Tests类型/fixture共28文件完成；下一入口从较短的几何/数据Automation开始，再大型执行体。

- 累计完整文件记录：186/230；未完成文件保持待审或部分完成。

### CP-31 Tests 数据与几何

- 三文件完整。重复invalid输入矩阵覆盖不同前置，保留；Grounding测试重构输出world corners测plane clearance，是独立几何不变量，不直接调用生产内部结果自证。
- EnemyAttackSet测试覆盖全权重overflow、端点抽样以及Engagement与AttackRange分离；无仅以功能名称声称通过。
- 下一入口：PlayerExhaustion与FloorVisibility现有用例，核对F06/F09缺口。

- 累计完整文件记录：189/230；未完成文件保持待审或部分完成。

### CP-32 Tests 力竭与楼层

- 两文件完整；PlayerExhaustion验证2秒和手动attribute恢复，no-op regen fixture不等于真实周期恢复；已覆盖外部Exhausted贡献保留。未覆盖duration0，H6-F06保持开放。
- FloorVisibility未启动World BeginPlay而是直接调用Gather/SetFloorActive/overlap handler，证明原生分类和状态切换断言存在，不证明关卡自动查找、真实Overlap或MPC渲染。无Volume先销毁而managed actors继续存活用例，H6-F09保持。
- 两种World fixture初始化深度不同，不能盲目用统一BeginPlay/physics模板替换；共用前须按用例生命周期分组。
- 下一入口：VitalHudAutomationTests（1013行）分段。

- 累计完整文件记录：191/230；未完成文件保持待审或部分完成。

### CP-33 Tests Vital HUD

- 完整读完1013行。F10缺口确认：402-453和518-564使用默认0.15s flash，0.03是推进量而不是duration配置，没有短duration结束恢复用例。保留现有色彩、延迟独立、重复hit、healing恢复断言。
- **H6-F11 / P2 / FIX-TestTiming**：VitalHudAutomationTests.cpp736-744在100→40伤害后仅调用一次SimulateTickForTesting(0.60)，立即期望buffer==0.4；生产UpdateBufferHealth.cpp185-189看到初始BufferDelayTimer0.5后减delta并return，buffer仍1.0。因此当前源码该断言与被测时序静态矛盾，不能声称当前整个VitalHUD suite通过；本阶段没有实际执行测试。后续保留“delay后应收敛”目标，使用与帧推进一致的分步tick并验证延迟期间不变和结束后收敛；若产品要求跨边界同帧消费剩余时间，先冻结该算法契约，不能简单删/弱化断言。
- 测试525-526的`timer>0 || timer==0`虽弱，但仍可被负数/NaN反例打破，不误称恒真。UI widget getter大量是真实输出断言消费者，不删除它们来瘦身。
- 下一入口：SkillBarHud与WorldInteractionPrompt测试，再回到战斗/AI各族。

- 累计完整文件记录：192/230；未完成文件保持待审或部分完成。

### CP-34 Tests 技能栏与交互

- 两文件完整；SkillBar空/非法/冷却值与ASC失败分支各有不同意义，保留。测试CDO `bAutoEndAbility` 在392改false但此前已Equip/GiveAbility，新实例是否接收该值待本机UE实例构造补证；未先把cooldown保留断言当取消生命周期已覆盖。
- `WorldInteractionPrompt`直接注册候选与手工MovementUpdated，测试名带integration不代表真实Overlap/PIE；重持有测试不涉及HeldCombatInputStartTimes，因此不能关闭H6-F07。
- 测试支撑可收敛项先列真实同义重复：SkillBarHud.cpp27-45与PlayerExhaustion.cpp20-26/36-45都有World->Tick + GFrameCounter + 最大0.1分步；Exhaustion另有两次0-delta prime，必须保留为显式选项或留在用例，不能统一掉。候选CAND-TEST-WORLDTICK-01，最小复用现有CombatAutomationFixture测试支撑，净估待所有测试族对照后算；不要把不同physics/nav/world初始化强行合并。
- 下一入口：继续其余Automation（先短PlayerMobileBow/LaunchFacingSmoothing，之后Projectile/AI/动作/Execution）。

- 累计完整文件记录：194/230；未完成文件保持待审或部分完成。

### CP-35 Tests 移动弓与冷却取消补证

- PlayerMobileBow完整299行；状态setter为seam，保留速度/CMC/外部效果断言，不冒充完整弓激活/发射证明。
- **H6-F12 / P2 / FIX-TestCancellation**：SkillBarHudAutomationTests.cpp365-366先装备AnotherSkillWeapon并GiveAbility，389-392之后仅将CDO的bAutoEndAbility改false，396-400激活再Cancel。TestPreparedSkillCooldownFixtures.cpp62-65读取已存在实例自己的bAutoEndAbility，仍为true，能力已自然结束；用例未断言cancel前实例Active，无法覆盖运行中取消后冷却保留。本机UE5.8 AbilitySystemComponent_Abilities.cpp319-322在GiveAbility为InstancedPerActor创建实例，1196-1220由NewObject保存实例，补证成立。后续在grant前配置并恢复CDO或配置实际primary instance，先断言Active再取消并保留冷却断言；是测试覆盖缺口，不是已证实生产冷却回归。未执行Automation。
- 下一入口：LaunchFacingSmoothing及AI/Projectile测试族。

- 累计完整文件记录：195/230；未完成文件保持待审或部分完成。

### CP-36 Tests Launch 状态与AI目标保留

- Graph只返回Launch主体且有gap；已逐段补齐并完整读取AI目标保留测试。
- Launch状态与TurnTask测试有独立边界价值；是否整体退役取决于CAND-LAUNCH-SMOOTH-01闭包，不因其大量断言相似裁剪。
- AI测试直接调用test入口，验证状态逻辑而非感知系统或StateTree资产调度；RootMotion清空是控制前置。未发现新增可证缺陷。
- 下一入口：AI spacing与facing/ragdoll。

- 累计完整文件记录：197/230；未完成文件保持待审或部分完成。

### CP-37 Tests AI 间距与朝向

- **H6-F13 / P2 / FIX-TestAI**：EnemyCombatSpacingTests.cpp202-676在本地bool/时间/SimulateRequest、SimulatePreparePending、SimulateTryRequestMeleeAttack、SimulateEndAbility及SimulateStateTreePrepareMeleeAttackTask上断言，生产Controller/StateTree/Ability在这些区段没有被调用。修改或删除生产的重试/approach/失败清理分支不会令这些断言失败。1-200真实Profile和CalculateTargetRelativeRepositionPoint覆盖有效；不否定整个文件。后续用实际Controller与可控时间/导航结果驱动相同场景，生产状态转移有回归时断言必须失败；保留异常前置，不通过只删模拟测试获得绿灯。独立测试修复，不是生产AI缺陷或03C自动门禁。
- TargetRetention和RootMotionFacing的transient AttackProfile/AttackSet/AIProfile准备及World cleanup存在相同样板；可在后续测试fixture片复用既有CombatAutomationFixture，先保留各suite BeginPlay、RootMotion清理差异，未批准新增通用框架。
- 下一入口：Enemy死亡与Projectile测试。

- 累计完整文件记录：199/230；未完成文件保持待审或部分完成。

### CP-38 Tests 敌人死亡

- 不同来源和无来源的致死上下文验证有独立价值；使用test counters及无PhysicsAsset的passive enemy，不把ConsumeCount当物理执行证据。
- 重复SetLooseGameplayTagCount(1)主要覆盖计数不变路径，真正额外伤害也有断言；不据单条注释扩大为已运行重复delegate证明。
- 下一入口：ProjectileTargetAssist、FlightTrail、Lifecycle。

- 累计完整文件记录：200/230；未完成文件保持待审或部分完成。

### CP-39 Tests 投射物辅助瞄准

- 两候选角度/距离断言有效但不覆盖三候选非传递排序，F05保留；只修改native team字段不能关闭F08。
- Homing直接Actor Tick不推进ProjectileMovement位置；timeout或转角cap二择一断言不能宣称独立验证cap触发，作为后续投射物集成验证矩阵缺口保留，不另增普通清理门禁。
- 下一入口：FlightTrail全文与Lifecycle分段。

- 累计完整文件记录：201/230；未完成文件保持待审或部分完成。

### CP-40 Tests 投射物拖尾与生命周期前段

- FlightTrail手工OnHit/Overlap/OnSystemFinished验证原生接线，timeout用真实World tick；保留单次伤害及terminal fade断言。FBoolProperty查找失败时默认false的CDO断言只能作有限结构证据，不代替属性readback。
- FlightTrail MaxTickStep=0.05不同于SkillBar/Exhaustion=0.1，合并World tick helper必须显式保留step与priming差异；候选仍待实施片核价。
- 下一入口：ProjectileLifecycle281-1001。

- 累计完整文件记录：202/230；未完成文件保持待审或部分完成。

### CP-41 Tests 投射物生命周期完成

- Bow 5.3通过test setter注入Drawing及ActorInfo，能检测身份过滤分支，不能证明IsActive/真实Montage事件交付；6节真实装备与SpawnProjectile结合但绕过激活，通过hook屏幕坐标。
- F02仍缺射手移动/玩家转向/Guard弧集成；F08测试precondition也直接_Implementation，后续补BP override需保留该差异。
- 测试的snapshot Definition污染、重复terminal碰撞、无效来源是有效独立契约断言，不能因样板重复删除。
- 下一入口：Charged Niagara与Melee trace/trail测试。

- 累计完整文件记录：203/230；未完成文件保持待审或部分完成。

### CP-42 Tests 蓄力反馈与Trace生命周期

- Charged与FlightTrail的World tick+0.05步进样板类型归一相同，纳入CAND-TEST-WORLDTICK-01的明确对照；当前只有已有公共fixture复用建议，不新建生产抽象。
- Trace helper suite真实ASC激活宿主与同步Task fail-closed覆盖有效；它不证明damage触发EndAbility的同步重入全部路径，按既有契约保留后续验证。
- 下一入口：TraceSource、MultiTrace、MeleeTrail。

- 累计完整文件记录：205/230；未完成文件保持待审或部分完成。

### CP-43 Tests 近战源与拖尾

- Legacy marker fallback有静态接口与测试消费者，名字为Legacy不等于退役；不授删除。
- MeleeTrail的GetAutoDestroy包装在测试79实际验证native属性；保留，不把单调用当无用。双ASC ability/task竞争测试有真实任务身份，不能与F01双montage问题混为一谈。
- 下一入口：MeleeMultiTraceSource1-712。

- 累计完整文件记录：207/230；未完成文件保持待审或部分完成。

### CP-44 Tests 多近战源

- 双源/单源/失效源场景虽然ASC准备相似，身份、几何和清理预期不同，保留。5节确有真实World sweep与Health断言，区别于手工碰撞广播。
- CASE8的Notify begin/end调用test Ability宿主，证明宿主与Task协议；生产每动作的Notify/激活身份仍由各生产文件与专属测试覆盖，不以此替代。
- 下一入口：Player/Enemy MontageRate、StanceBreak Rate与ActionWindow。

- 累计完整文件记录：208/230；未完成文件保持待审或部分完成。

### CP-45 Tests StanceBreak RateWindow

- StanceBreak用生产测试分支在Activate复制CDO fixture，区别于F12普通InstancedPerActor测试字段；此处不报相同CDO问题。
- 真实ASC event/Task绑定配合bypass，证明清理与身份集合逻辑，不能证明F01真实双实例调速。synthetic TestApplyBegin也不能补足。
- **CAND-STANCE-TEST-NOOP-01**：524-541标题称Front Execution Availability，实际仅AddLooseTag后HasTag，530-537新建ExecAbility和ActorInfo后未调用任何Front资格方法。待与Front/Backstab suite的有效资格用例对账，保留契约测试目标；不得把这段视为处决准入证明。
- 下一入口：EnemyMontageRateWindow1-1058。

- 累计完整文件记录：209/230；未完成文件保持待审或部分完成。

### CP-47 Tests Enemy RateWindow 完成

- WITH_EDITOR9节真实playable montage、ASC event触发和实例ID/retrigger/cleanup断言完整；Notify按962手工驱动，非时间线调度。
- 905明确旧instance stopped，不能作为H6-F01双实例存活授权反例；保留正向证明与缺口，不否定已有重激活测试价值。
- 下一入口：PlayerMontageRateWindow1-1366。

- 累计完整文件记录：210/230；未完成文件保持待审或部分完成。

### CP-49 Tests Player RateWindow 完成

- Light915-954确有bStopAllMontages=false、old not stopped与新ID验证，证明该Player消费者现有防护；不能把它迁移成Enemy F01已关闭结论。
- 五consumer RunConsumer共享现有测试模板，覆盖真实ASC+Montage及各动作分支；1217默认Montage_Play替换只证明替换后保护，未证明同资产双live实例。此限制入FIX1验证矩阵。
- 测试数据/reflection注入用于构建前置，production state/cleanup断言有效；不建议为消除if constexpr再建通用Ability基类。
- 下一入口：PlayerActionWindow281-866。

- 累计完整文件记录：211/230；未完成文件保持待审或部分完成。

### CP-50 Tests 玩家动作窗口

- Bow/Charged/Dodge/Launch各窗口共享形状但DefenseTag、pause latch、phase语义不同，保留独立断言；合并样板不允许合并权属。
- 3节直接spawn非公共fixture，未World BeginPlay，CanActivate资格矩阵不代表完整生产Startup/regen流程；9.9是手工Dodging标签模拟，新ASC retrigger由PlayerRate补足。
- 四个Dodge候选getter未被本文件调用，最终做全Source原生消费面核验，不删除对应Tag字段。
- 下一入口：双方LaunchRootMotion测试、HitReaction。

- 累计完整文件记录：212/230；未完成文件保持待审或部分完成。

### CP-51 Tests Enemy LaunchRootMotion

- Synthetic montage只HasRootMotion元数据，bypass不证明RootMotion轨迹与实际Montage结束；CallActivate直接驱动不等同TryActivateAbility全前置。
- 第7节仍通过Event.Reaction.Launch.Commit验证已退役事件不会影响当前RootMotion分支，CAND-LAUNCH-SMOOTH-01必须处理这处测试消费，不能直接删Tag后留下RequestGameplayTag有效性断言。
- GetTestRootMotionKnockdownCompletedNaturally无本文件读取，但SetTestRootMotionKnockdownCompletedNaturally(true)用于自然结束前置且production字段有行为用途，getter/field/setter必须分开裁决。
- 下一入口：PlayerLaunchRootMotion1-713。

- 累计完整文件记录：213/230；未完成文件保持待审或部分完成。

### CP-52 Tests Player LaunchRootMotion

- Player与Enemy synthetic knockdown构造相似，Player额外OutSeq由5节消费；不能删OutSeq或把无roottrack的元数据fixture当真实可播放fixture。
- Player8.5在late回调后可主动EndAbility，最终None只证明这条收敛序列，非独立Actor Destroy自动清理验证；现有审计不据标题夸大。
- 旧Launch.Commit又有148/335消费，退役片需改为反向无监听证明或移除仅旧协议断言，经资产readback后才能裁决。
- 下一入口：HitReactionAutomationTests1-2223。

- 累计完整文件记录：214/230；未完成文件保持待审或部分完成。

### CP-55 Tests HitReaction 完成

- 5节未解绑引用局部lambda的风险已追完：6节是另一DeferralASC且有Remove；7节原Player/Enemy只运行synthetic Task、Spec与纯resolver，没有再次投递Reaction事件。当前无可证悬空调用，不列已复现/确证行为bug；若未来复用这两个ASC或提取fixture，必须让观察delegate作用域与捕获数据一致，不能带入共享fixture。
- 6.10e/f区分不同context及共享context的不同GE事务，不能合并成单一相似用例；后续生产属性/deferral改动保留全部前置矩阵。
- 旧速度矩阵的唯一归属为CAND-REACTION-LEGACY-01，与Launch退役岛合并核价；四向selector和当前impact全部保留。
- 下一入口：Combat/Parry/Defense反馈测试。

- 累计完整文件记录：215/230；未完成文件保持待审或部分完成。

### CP-56 Tests 防御反馈

- Parry窗口直接setter+ParryingTag，不能覆盖F03 AddLoose重复Begin；投射物禁Parry而Guard允许由两个suite明确区分。
- 音效使用dispatch计数与位置，未执行音频/视觉验证；PlayerDefense602的TestTrue(true)只标流程未抛出，不能作为解绑后的状态证明，不将其计入有效断言收益。
- 相同0.05 World tick样板继续纳入CAND-TEST-WORLDTICK-01；UIFlash数学缺陷F10在这些反馈suite无对应短duration边界。
- 下一入口：CombatHitFeedback1-1151。

- 累计完整文件记录：217/230；未完成文件保持待审或部分完成。

### CP-58 Tests Combat 反馈完成

- 三suite均全文核验。FOV首次创建/第二次复用/PCM移除后无同类对象的断言是有效结构证据；未运行，不证明视觉手感。
- 相似攻方/受方shake前置用于验证配置解耦，不合并成共享隐式状态；公共World步进可单独评估。
- 下一入口：PlayerLockOn1-1349。

- 累计完整文件记录：218/230；未完成文件保持待审或部分完成。

### CP-59 Lock-On 测试全量核验

- CodeGraph 先导航，因分段截断补齐全部缺口。排序只覆盖精确相等与四象限，未覆盖 F05 非传递比较器。
- 417–672 的 2C 使用 ASC TryActivateAbility、真实可播放 root track、UpdateAnimation/DispatchQueuedAnimEvents，覆盖玩家结束而受害者继续 recovery 的实际委托链；不是 setter-only，保留。342–414 的状态注入矩阵提供不同边界，不能因相似删除。
- 1117–1215 有实际 World 碰撞 LOS；1020 的 hook/投影模拟只说明逻辑。1292–1296 的 true 断言是 smoke，不证明输入 held 清空或所有解绑，因此不关闭 F07。未运行测试。
- 可播放 Montage fixture 与 TEST-MONTAGE-FIXTURE-01 有重复，保留 2C 的共享 skeleton、不同片长、montage-only tick 与实际双边生命周期差异；估算待定。
- 下一入口：PlayerMeleeMotionWarpingAutomationTests.cpp。

- 累计完整文件记录：219/230；未完成文件保持待审或部分完成。

### CP-61 MotionWarping 完成与退役闭包更正

- 已核验剩余 901–1759；Light combo 快照与 Charged Release、Sprint、Skill 各自入口必须保留，重复可提取部分限于 WARP-DUP-01，不能合成公共 Ability 基类。
- 1203–1231、1359–1387 的 Charged/Sprint reset 先手动清理再调用 EndAbility，不能称其直接证明 EndAbility 清理 warp；Skill 1648–1664 先重建实际 target 再直接 EndAbility，有有效断言。作为证据边界记录，不弱化测试。
- 1667–1747 实际 grant/activate 两 fixture 并断言 UnPossessed 只取消 Teardown tag，保留 Guard；这支持 F07 修复不能 CancelAll 的冻结契约。
- LAUNCH-SMOOTH-01 退役闭包增加消费者：1679、1700、1724 使用 UTestLaunchFacingSmoothingAbility 作为 Guard fixture。删除该 fixture 前需迁移仍有效的取消测试；874 物理行不是直接可删量，更不是净收益。
- 下一入口：WeaponEquipmentComponentAutomationTests.cpp。

- 累计完整文件记录：220/230；未完成文件保持待审或部分完成。

### CP-63 Equipment 完成

- 1051–1404 验证 Bow/Shield/Unarmed/HeavySword 布局、回滚后的 display scale、真实 ASC grant 清除，以及直接 Primary/Sprint route 的 fail-closed。
- 1239–1268 的 Blueprint CDO ActivationOwnedTags 检查依赖资产加载；静态读到调用不等于本轮 readback 或运行通过。
- CDO临时tag恢复与Sword临时socket均是现有测试实现，仅记录风险边界；未扩展运行或资产改动。无新确认行为缺陷。
- 下一入口：Backstab/Front Execution 测试。

- 累计完整文件记录：221/230；未完成文件保持待审或部分完成。

### CP-65 Backstab 完成

- 901–1456 保留实际 Primary fallback、StanceBreak→Backstab 仲裁与重复激活身份矩阵；ContextA 旧回调被明确直接投递，1155/1191 是人工失效/同步结束注入，能验证返回点保护但不等于真实动画触发。
- 1227–1258 使用实际碰撞物阻挡 snap 并核验 rollback/受害者释放；1289–1295 保留别的 motion warp target。不同生命周期断言不列重复删除。
- Tests 宏为 1 的几何调用无法发现 F04 宏为 0 的生产编译变体；不关闭 FIX-BuildGuard。
- 下一入口：FrontExecutionAutomationTests.cpp。

- 累计完整文件记录：222/230；未完成文件保持待审或部分完成。

### CP-66 Front Execution 完成

- 全部 985 行已核验。352–366 明确调用生产 CanActivateAbility，并验证无 Stunned 拒绝/有 Stunned 接受；213–223 仅伪造 ActiveCount，不能当真实 StanceBreak 自然生命周期证明。
- STANCE-TEST-NOOP-01 可删除原 StanceBreakTests 524–541 的重复伪资格段：其只手动 AddTag/HasTag，530–537 创建的 ExecAbility/ActorInfo 未调用。真实资格已由本文件 352–366 和 Backstab 387–399/943–1003 覆盖。推荐独立测试清理项，净估待精确圈定；不删除真实边界断言。
- Front 478–486 与 Backstab 的无效换装前置问题相同；672–687 同时移除 tag、取消 StanceBreak、手工结束 Victim，不能声称只移除 Stunned 就独立导致结束。归测试证据边界，不伪报生产 bug。
- 下一入口：ExecutionHitNotifyAutomationTests.cpp。

- 累计完整文件记录：223/230；未完成文件保持待审或部分完成。

### CP-67 Execution Hit Notify 完成

- 完整512行；468–475 直接调用真实 Notify 后经 ASC/WaitGameplayEventTask 消费并改变 Health，不能归为无消费者或纯 mock；没有播放 authored Montage 触发 Notify。
- 68–76 明确旧 Front/Backstab Hit Tag 未注册；314–326 的旧Tag变量实际为 Invalid，因此只证明拒绝非法Tag，父Tag和Release/VictimStart有独立有效拒绝断言。保留规范Tag回归，不把历史名字列退役代码。
- 测试 grant/config 样板存在，但 Front/Backstab 方向参数、手动 StanceBreak 状态和真实激活场景有差异；收益须在独立 fixture 片测算，不泛化成框架。
- 下一入口：ExecutionImpactFeedbackAutomationTests.cpp。

- 累计完整文件记录：224/230；未完成文件保持待审或部分完成。

### CP-68 Execution Feedback 完成

- 全659行；Front NonLethal、Backstab DeathPending 实际经伤害路径推动 Context，随后检查一次反馈与 VictimStart 不重放；normal GE 646–652 也有生产回归入口。
- 49–62 的 .05 World 步进纳入 TEST-WORLDTICK-01；64–98 的 StanceBreak激活 helper 与 Backstab61–96重复，但保留 CDO恢复和该Ability显式复制fixture契约。
- 549标题列NaN/Inf/非正dilation，实际565–583只覆盖0/负duration与dilation>1；不能声称全参数边界测试完成。音效/血液计数是dispatch证据，不是实际音画；本轮未运行。
- 下一入口：ExecutionLethalRecoveryAutomationTests.cpp。

- 累计完整文件记录：225/230；未完成文件保持待审或部分完成。

### CP-69 Lethal Recovery 完成

- 全883行：Front/Backstab 通过 ASC/GE/事件实现 Health0→DeathPending→Release→Dead；外部Tag计数保留与未确认pending回退直接调用生产，保留其失败边界。NotifyLethal/HitState setter 部分不冒充完整链。
- EXEC-COMPAT-01 的真实测试消费已确认在162–172：验证Begin/Abort/CompleteFinalization旧包装；生产已使用Outcome版本。候选应将这些断言迁往当前Outcome入口后移除纯native兼容wrapper，不能连finalization rollback断言一起删；最后全Source核对其余消费者/反射后裁决，估算待定。
- 852–865 未配置完整Backstab/Victim事件前置，不能独立证明仅DeathPending导致拒绝；作为验证限制纳入既有处决矩阵，不新增未经证明运行时缺陷。
- 下一入口：ExecutionLockInAutomationTests.cpp。

- 累计完整文件记录：226/230；未完成文件保持待审或部分完成。

### CP-70 Execution Lock-In 完成

- 全736行；Context凭据、双边 ASC tag握手和 SourceObject 授权有实际生产调用。该World无BeginPlay，与LethalRecovery/LockOn真实动画fixture不同，不能盲并。
- 385–398 的空Controller缺目标/导航/配置，除了 IsExecutionLocked 翻转本身，四个拒绝断言不能独立证明锁门禁；8.1/8.2 StanceBreak没有可播放配置或bypass，未证明Ready hook真的达到。保留证据边界，不能以全绿宣称实际Task失败链被驱动。
- 608–617 同时移除Tag且手工结束Victim；只能证明该组合收敛。No-weapon 722–725 同时缺目标/有效spec，不能独立证明装备准入。后续测试质量片需先正向控制再单变量反例。
- 下一入口：ExecutionReleaseOutcomesAutomationTests.cpp。

- 累计完整文件记录：227/230；未完成文件保持待审或部分完成。

### CP-72 Release Outcomes 完成

- 911–1226 补齐：重复release、GE modifier失效后恢复、取消失败无Launch，以及无floor/Falling/Flying/Custom不同所有权边界均保留。
- 所有局部Launch delegate在正常路径有Remove；早return依赖World销毁，未发现可证的后续投递悬空引用路径，不按假设报bug。
- StanceBreak激活helper、reset weapon与metadata Montage候选可共享纯测试支撑，但前置/断言不得削弱。下一入口ExecutionVictimPresentationAutomationTests.cpp。

- 累计完整文件记录：228/230；未完成文件保持待审或部分完成。

### CP-74 Victim Presentation 完成

- 1051–1552 已审。13C 的 MontageStarted hook 取消后同资产重播、13D 的原生旧实例 blendout 在新实例出生前取消，均实际走 Montage_Play；1436–1452 对迟出生实例停止/不恢复旧任务/保留外部速度有精确断言。不能把整文件称为synthetic-only。
- 第14节手工创建两FAnimMontageInstance验证按ID Stop，不通过资产级Montage_SetPlayRate，所以不关闭F01；也不能反过来把Stop判为F01缺陷。
- 第8节F14与第13节有效测试不同入口：后者配置锁定和Handshake后测试Victim montage startup，前者想测Player/Victim WaitEvent startup而未满足激活前置。保留后者不能自动修复前者的假阳性。
- 下一入口：最后一个Source文件 ExecutionVictimRootMotionAutomationTests.cpp。

- 累计完整文件记录：229/230；未完成文件保持待审或部分完成。

### CP-75 全部 Source 审阅完成

- 最后532行已核验；metadata Montage+bypass/直接Completed与MovementMode回调说明状态恢复，不证明实际位移。407实际消费GetTestSavedCanWalkOffLedges，不能连同H6候选GetTestHasSavedCanWalkOffLedges一起删。
- 全Source覆盖230/230（生产与构建158，Tests72）；后续只做已列候选最终消费面核验、台账/ROADMAP/归档对齐与文档提交，不继续无界探索。
- TEST-MONTAGE-FIXTURE-01增加ReleaseOutcomes67–97、VictimPresentation70–109、VictimRootMotion63–93的metadata重复；与真正playable fixture分开提取并保留条件参数。
- 下一入口：11 getter、EXEC-COMPAT-01与STANCE-TEST-NOOP-01最终裁决。

- 累计完整文件记录：230/230；未完成文件保持待审或部分完成。

### CP-76 候选最终消费核验

- CodeGraph先查询getter与Finalization调用，图谱未完整覆盖宏内接口，随后全Source精确符号搜索：11 getter均仅有各自单行定义；结合全头源审阅确认均非虚、非UFUNCTION、处于WITH_DEV_AUTOMATION_TESTS，无委托/宏间接消费者。逐项删除裁决，净11行；生产字段、setter及GetTestSavedCanWalkOffLedges保留。
- EXEC-COMPAT-01：ExecutionLockContext.h128–135三非反射声明，cpp269–303旧状态实现，唯一测试消费LethalRecovery164/166/170/171；ReleaseOutcomes196–210已有Outcome rollback/完成测试。裁决简化：迁移必要旧断言到正式Outcome入口后删除旧入口；不把仅测试消费误写为零引用，不连带删事务状态。
- STANCE-TEST-NOOP-01准确文件是EnemyStanceBreakRateWindowAutomationTests.cpp，裁决删除524–542伪资格段（19物理行），不删除Front/Backstab真实资格用例；早期简称仅作历史导航。
- Source全文未找到ponytail:标记，统计为0；rg无匹配退出码1属预期。当前实际43个AutomationTests.cpp；Tests覆盖为72个全部自有文件，不以命名后缀计覆盖。
- 所有原生源码覆盖完成；资产/Blueprint/Tag挂接readback缺口仍保留为条件，禁止转为已批准资产退役。进入文档总账和提交收据对齐。

- 累计完整文件记录：230/230；未完成文件保持待审或部分完成。

### CP-77 最终对账与提交边界（2026-09-12）

- Source实际230文件与逐文件表230条一一匹配，无重复、缺失或未完成行；158生产/构建+72 Tests，总75,281物理行。全部Source逐文件SHA-256与Main接管基线一致，相对HEAD `d99ddcd` 和 `759efca` 均零Source差异。
- 最终主表14 Findings、24候选、11 getter已逐项对齐；Ponytail单行总览由同一候选数据生成。已量化10项138–170、条件2项28–42、其余12项待定，0个已证实可移除依赖；Source注释统计0处。当前阶段静态审计完成，03C前置尚未完成。
- 排除三文档后的2,122项WIP状态与接管快照一致；SHA-256摘要 `04B39B32ABD8F4C21335E3914D145A98B3CB4E8F694F500D8E957EAC51A0A0AE`。这项证明为Git路径/状态对账，不冒充全部二进制资产内容readback。
- ROADMAP-archive.md接管时前391,700字节保持原样，SHA-256 `96AA8BDA220B2340327888CBE1459654CA56BFED9B310A70796D913E3A937FCB`；本轮仅追加Main接管纠正与最终摘要，保留Gemini历史正文。
- 三文档 `git diff --check` 通过。暂存前HEAD仍为d99ddcd，index为空；依用户既有提交批准，下一步只暂存三文档、核对cached范围/格式后形成H6全量静态审计提交，不amend、不push。提交结果由Git历史和本轮交付回复定位，不在提交内容中自引用未生成的hash。
- 未改Source/测试/Config/资产/Build.cs/ARCHITECTURE.md；未执行编译、Automation、Editor／PIE、资产readback、子代理或额外Fresh Review。到此停止审计探索，不自动进入FIX1实施。

## 8. 最终审计结论与交接（2026-09-12）

### 全模块与跨模块接缝结论

**全项目静态审计已完成；生产实现／构建158文件、Tests72文件，共230文件、75,281物理行，均有完整区段记录。** 外围8份在盘Config、uproject、自有工具／插件盘点已对账；1份已有删除Config维持缺失。完成读取不代表没有缺陷或测试通过；本次发现见第4节。

| 单元 | 完成 | 结论／相关Findings与候选 |
|---|---:|---|
| 模块／构建 | 5/5 | 唯一PolyQuest Runtime与Target/Build对齐；F04是Ability声明宏问题，0个依赖获准删除 |
| AbilitySystem | 60/60 | GAS权属与独立动作生命周期保留；F01/F03/F04；绑定、Clamp、Snap/兼容规则与条件Warp候选 |
| AI | 8/8 | Controller/StateTree管理意图、pending与冷却，Ability管理执行；现有测试真实性缺口F13，未把模拟段当生产验证 |
| Animation | 10/10 | Notify身份/payload契约保留；事件发送可局部去重；旧Launch反射入口等待闭包readback |
| Camera | 2/2 | FOV modifier只呈现，状态恢复路径保留；本次没有新确认缺陷 |
| Character | 8/8 | Character-owned ASC保留；F05/F06/F07；反应派发局部简化候选，死亡/Root Motion既定权属保留 |
| Combat | 47/47 | Equipment事务、Execution Context、GAS结算保持唯一权属；F02/F05/F08；校验/原生attach/旧native接口候选及条件退役 |
| Environment | 4/4 | F09隐藏写入的清理缺口；重叠Volume需求无证据时不建全局管理器 |
| Framework | 4/4 | PlayerController的Possess/HUD/委托解绑完整核验，F07归Player输入缓存；不重复报同根问题 |
| UI | 10/10 | 只消费ASC和装备事实；F10短闪白恢复缺陷，后续纯数学共享不造Widget基类 |
| Tests | 72/72 | F11/F12/F13/F14为测试时序/前置/真实性缺口；11 getter可删除，fixture提取保留有效断言及真实/人工边界 |

| 接缝 | 审计结论／后续责任 |
|---|---|
| 输入／装备→GAS | 装备事务和能力授权保留；F07关闭物理输入缓存；F12/F14修正测试前置 |
| StateTree→Controller→Ability | 意图与动作权属分离成立；F13需生产入口回归证明，不能靠模拟重试证明 |
| Notify／Task→激活身份→清理 | F01资产级调速授权、F03tag计数、F14 Ready测试缺口分别归属；有效startup同步重入测试保留 |
| Trace／Projectile→Defense／Resolver→GAS／Reaction | 允许Guard且禁用Parry；F02分离来袭与来源，F05严格排序、F08接口分派独立修复 |
| Character／Equipment／ASC→Controller／UI | F06力竭零时长、F10恢复颜色、F11/F12测试语义；呈现不建立第二套玩法状态 |
| Camera／Player→Environment／MPC | F09管理者结束需撤销Actor隐藏贡献；MPC参数恢复不能替代Actor恢复 |

### 动态证据缺口与删除保护

源码覆盖没有待审文件；以下是已识别的动态／采用证据缺口：

- RET-LaunchClosure：旧ReactionLaunchCommit Notify、GameplayTag监听/注册、Blueprint派生/软引用等实际资产引用；先迁移MotionWarping Guard宿主等仍有效原生测试消费者，再做readback并重新裁决，不能直接删除874行闭包。
- F01同资产双live、F03重叠Parry、F06零时长、F08 BP阵营覆写及F09独立Volume销毁均有明确静态前提；当前生产资产是否配置/触发未读取。缺实际触发证据不抹掉静态缺陷，也不能声称现有场景已复现。
- 各GA/Montage窗口、Root Motion、Notify与MPC/UI视觉的历史readback/compile债务仍在ROADMAP原唯一条目；未以本次源码读取关闭。普通宏内非反射getter不附加无关资产门禁。
- 未发现足以批准移除模块/插件/包的证据；编辑器工具/资产扫描声明保留。自有Source之外的第三方、生成缓存、临时PDF工具没有被纳入游戏删除收益。

### 结束条件与下一步

1. 逐文件清单、11个单元、6条跨模块接缝、14项Findings、24项候选与11 getter已对账；当前裁决以第4/6/8节为准，CP-00～CP-76保留当时的发现和后续纠正过程。
2. Ponytail总览与主表同源；138–170只覆盖10项已量化候选，条件28–42和12项待定单列，未把未接受建议或物理行数当批准净收益。
3. ROADMAP保留每项开放FIX、条件候选和历史债务的唯一归属/触发。FULL-AUDIT本轮关闭，修复与SHRINK未执行；03C仍须FIX1→FIX2→已接受有限SHRINK。
4. Source零改动、WIP隔离、archive只追加和文档检查收据见CP-77；用户已批准完成后提交，仅三文档，不推送。
5. 本轮没有编译、Automation、Editor/readback、PIE、视觉／网络／打包验证，也没有子代理或额外Fresh Review；不从历史绿色报告推断当前所有测试通过。
6. 下一任务建议新会话由Main先制定FIX1封闭计划，明确真实双live反例、共享授权及StanceBreak差异；计划接受后按项目路线交给Gemini实施，再由Main按该片门禁收口。当前plan保留至下一计划正式接受，不自动开始修复。


### CP-73 Victim Presentation 前段与 F14

- 1–1050已核验：Front/Backstab/实际StanceBreak交接、Poise恢复、LateStart及in-place montage均有保留价值；metadata fixture无动画track内容且大多bypass，不能当root-motion/自然完成证明。
- **H6-F14／P2／FIX-TestExecution**：本文件872–929的两项Ready失效测试缺少锁定目标及Stunned前置。前一节868–869 CleanupExec调用452–458清空LockedTarget；后续只grant/config，897/927 TryActivateAbility因无目标先失败，898/928只断言inactive，即使删除失效hook/Ready保护仍可通过。这是静态可证测试假阳性，不是生产回归。修改范围推荐本测试第8节，先建立可激活控制样本，再单独注入失效并证明入口抵达/锁清理。
- 同归测试前置修复候选面：Front478–486、Backstab551–559换装用非法definition/null pickup，不能隔离active gate；ExecutionLockIn385–398空controller与8.1/8.2未经证明Ready入口，逐项加正向控制。不得直接删异常断言。该FIX为验证质量，不自动阻塞03C。
- 下一入口1051–1552。

- 累计完整文件记录：228/230；未完成文件保持待审或部分完成。

### CP-71 Release Outcomes 前段

- 已读1–910；196–210 已直接验证 Outcome Begin/Abort/Complete 和 rollback，支持 EXEC-COMPAT-01迁走旧wrapper消费而保留语义。
- 67–96 是无skeleton/data model的合成metadata Montage，139预期SequencerDataModel错误；不能与LockOn可播放root track混为一类。第13节直接TestTriggerBlendOut/Completed，不证明真实动画自然事件；LockOn2C是独立补充证据。
- Release失败、取消不触发Launch、顺序颠倒与DeathPending链是不同边界，保留。下一入口911–1226。

- 累计完整文件记录：227/230；未完成文件保持待审或部分完成。

### CP-64 Backstab 前段

- 1–900 已核验。387–399 使用实际 StanceBreak 激活并验证单一 Stunned 所有权；401–436 的 synthetic ActiveCount 只说明计数状态表，不替代真实所有权。
- 515–590 通过输入路由进入实际 Backstab、Victim lock 和 snap，619–713 验证 payload 身份、一次伤害及失败结束；MontageTask 被跳过，不证明播放完成。
- 551–559 使用配置不完整的 SecondaryWeapon 和 nullptr pickup，只能作为拒绝 smoke，不能单独证明 active execution 的换装门禁；后续实施需使用原本可装备的有效对象验证，归既有处决验证矩阵，不扩大运行时缺陷。
- 下一入口：901–1456。

- 累计完整文件记录：221/230；未完成文件保持待审或部分完成。

### CP-62 Equipment 事务测试前段

- CodeGraph 导航后补齐 1–1050：有真实 Equip/TryEquipWorldPickup、ASC spec、socket、drop、失败注入与 rollback 断言，保留全部不同前置用例；不因行数多当死测试。
- 70–82 依赖具体项目 Hero/Sword/Shield 资产，398–433 临时修改已加载 SwordMesh socket 后恢复；本轮未执行，不可把这些源码当资产 readback。无 BeginPlay 的事务 fixture 不应盲并成其他 World fixture。
- TEST-DROP-BOUNDS-01：529–546、560–577、614–631 八角变换最低点计算重复，可在本测试局部共用纯计算，保持三种 transform 与独立 2cm 断言，不调用生产 drop 计算作为 expected；净约 30–36 行，额外候选待接受。
- 下一入口：1051–1404。

- 累计完整文件记录：220/230；未完成文件保持待审或部分完成。

### CP-60 MotionWarping 前半

- 已核验 1–900，Light 的一次捕获、配置不合法不消耗机会、后续 entry 共用快照，以及真实 Enemy Dead tag/Destroy 清理入口均有断言；Montage bypass 不证明 authored warp 轨迹。
- 几何边界与共享 helper 等价断言有效，不能削弱为少量 happy path；后续从 901 行恢复。

- 累计完整文件记录：219/230；未完成文件保持待审或部分完成。

### CP-57 Tests HitFeedback 主套件

- 1-770确有实际CameraShake对象与overlay值、World timer和真实GE transaction断言；Niagara/音效dispatch仍非视觉/听觉证明。
- 不同source团队、多个Health modifiers vs多个spec、外部overlay与time dilation覆盖各自权属，不能为了净删行移除这些断言。
- 下一入口：AttackerImpactCameraShake781-1151，再LockOn/MotionWarping/Equipment和Execution余项。

- 累计完整文件记录：217/230；未完成文件保持待审或部分完成。

### CP-54 Tests HitReaction 中段

- 888-927旧TryBuildLaunchVelocity包装测试与705-927速度矩阵属于CAND-REACTION-LEGACY-01；930起四向selector是当前活跃契约，不能随旧速度逻辑删除。
- 5节原始Player/Enemy的lambda引用局部计数且未存handle，离开区段后仍留在ASC；后续区段目前用独立DeferralEnemy，暂未证明悬空lambda被触发。需末段核验实际后续dispatch，不能凭悬空存储直接声称本suite运行崩溃。
- 6节真实GE Poise/Health modifier先后顺序有独立价值，保留。
- 下一入口：HitReaction1741-2223。

- 累计完整文件记录：214/230；未完成文件保持待审或部分完成。

### CP-53 Tests HitReaction 前段

- 607-675精确验证近战/一般impact的Instigator优先与normal fallback，FIX2必须保留该契约并单独引入projectile来袭几何，不能直接翻转全局优先级。
- 551-559明确消费旧CommitNotify类型；705起旧TryBuildLaunchFacingAndVelocity矩阵与LaunchSmoothing退役岛关联，净收益去重且待readback，不准直接删整个HitReaction测试文件。
- 下一入口：HitReaction871-2223。

- 累计完整文件记录：214/230；未完成文件保持待审或部分完成。

### CP-48 Tests Player RateWindow 前段

- PlayerRate与EnemyRate均有synthetic montage以及真实playable root track fixture，结构重复但ExistingSkeleton、名称与notify setup有差异；记CAND-TEST-MONTAGE-FIXTURE-01，后续测试fixture单片评估，不与生产生命周期抽取混做。
- Light真实ASC entry 0→1及旧context拒绝已读，1.25 baseline有实际Montage_GetPlayRate断言；保留本机引擎/编辑器分支边界。
- 下一入口：PlayerRate841-1366。

- 累计完整文件记录：210/230；未完成文件保持待审或部分完成。

### CP-46 Tests Enemy RateWindow 前段

- 1-730诸多Integration标题使用直接helper或test Active/context/bypass；证据分层按实际调用标注。不同窗口集合/顺序/非法身份边界保留。
- 不把CreateTestMontage与CreatePlayableRateMontage合为一个fixture：前者UAnimComposite用于集合匹配，后者root track可实际播放。
- 下一入口：EnemyRate731-1058，随后PlayerRate。

- 累计完整文件记录：209/230；未完成文件保持待审或部分完成。

### CP-19a Player 输入断连补证

- **H6-F07 / P2 / FIX-InputTeardown**：`PlayerCharacter.cpp:1076-1091` 把按下Tag保存在HeldCombatInputStartTimes并拒绝重复Started；只有`:1094-1104`收到Released/Canceled才移除。`:320-348` UnPossessed及`:310-318`重持有没有清空，UnbindSprintStateEvents也仅解绑。UE5.8 `Engine/Private/Pawn.cpp:713-735` 清Controller并销毁输入组件，不调用本项目输入释放。可检验序列：PrimaryAttack Started → UnPossess且没有Released/Canceled → 再Possess同Pawn → 新Started被Contains拒绝，旧held时长跨持有者延续。原生路径静态确证；未假定当前产品已发生换Pawn，也未把刻意保留Guard的策略判错。修复片只关闭物理输入/长按/重试资格，逐项确定已活动能力兼容策略，验证断连、重持有、取消及旧timer；不要求CancelAll，不自动阻塞03C。
- 直接父类补证已用完；后续在Tests完整阅读中只核对现有对应用例与缺口。

- 累计完整文件记录：93/230；未完成文件保持待审或部分完成。

### CP-18 Player 输入与 Lock-On

- 完整静态覆盖推进到 cpp1880。角色只发起装备/Ability 请求，真正激活成功后才截断输入降级路由；Guard resume 资格是一次物理意图许可，不等同 GAS 动作状态。
- H6-F05 的排序器由滚轮切换 `PlayerCharacter.cpp:1412` 和死亡后继 `:1772` 直接消费，纳入 FIX-LockOnOrder；不能凭现有简单排序测试宣布严格弱序安全。
- 处决锁定保留、非致命恢复和普通目标准入条件不同；保留独立契约，未把局部判空相似当抽象理由。
- 下一入口：cpp1881，完成朝向/遮挡、委托解绑和 exhaustion 后再裁决生命周期。

- 累计完整文件记录：92/230；未完成文件保持待审或部分完成。
