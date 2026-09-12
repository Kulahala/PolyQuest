# TODO-03H6-FULL-AUDIT：全项目架构、冗余与退役残留审计计划（修订版）

## 1. 目标、基线与授权边界

审查当前完整工作树中的全项目自有代码，覆盖架构权属、生命周期、重复实现、过度抽象、死代码和退役残留，交付全覆盖记录、逐项裁决台账及后续 FIX／SHRINK 分组。**Git 历史用于定位证据，不作为范围过滤器。**

**本轮交付状态（2026-09-12）：用户已接受本修订计划，并明确本轮只落盘计划、230 文件待审清单及交接指针。全项目审计尚未启动；第 2～5 节定义后续审计执行规则，第 6 节当前新增完整覆盖为 0/230。本轮不执行源码审计批次。**

本版补充分批检查点、分级审阅、测试冗余标准、反射删除门槛及收益估算。保留已接受的阶段顺序：FULL-AUDIT → FIX1 → FIX2 → 已接受项目级 SHRINK → 03C。

- 项目：`E:\GameDevelop\PolyQuest`；UE 5.8，引擎依据为 `D:\UE\UE_5.8`。
- 计划制定基线 HEAD：`9f2d8e0b1b8c34dabc4f17bfb6aea6b0a2adb229`；Source 与 H6 审计基线 `759efca` 无差异。提交 `9f2d8e0` 已核实只包含 H6 的三份阶段文档；本计划文档提交版本以 Git 历史为准，审计正式执行前刷新实际基线。
- 当前盘点：230 个 Source 文件，共 75,281 个物理行；行数只作定位及分批参考，不代表审阅覆盖或可删除量。已有 WIP 共 2,122 项（Content 2,117、Config 1、tmp 4）；实际执行时重新记录基线并保留 WIP。
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

**本阶段没有任何生产或测试符号写入授权，也没有公开 API／类型变更。** 不改源码、测试、Tag、Config、Build.cs、资产、工具配置或 ARCHITECTURE.md；不运行编译、Automation、Editor／PIE；不自动暂存、提交或推送。既有 WIP 不得回滚、恢复、格式化、暂存或归因于本次交付。旧 H6 提交批准不延续；用户已在本轮单独批准仅三份阶段文档提交，具体边界见第 7 节提交批准记录。

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

1. 正式执行前先完成计划及完整待审文件清单落盘；本轮完成此检查点，不自动进入下一批。
2. 审计执行按表中顺序串行推进；Combat 按现有子域、AbilitySystem 按动作族拆批，相关测试随生产代码读取，最终对全部 72 个 Tests 文件核对遗漏。
3. 每批最多 12 个文件；大型文件按函数区段拆批，记录最后完成的区段与下一入口。每批结束即增量更新 plan.md，不等全库读完。
4. 每个检查点保存：基线、已完成文件／区段、候选及裁决、未解问题、证据引用、下一批入口。恢复任务先核对这些记录及对应工作树差异。
5. 未经审阅的部分保持“待审／部分完成”；上下文切换或预算耗尽不能把它们改成“无问题”或“延期”。

### 审阅深度分三级

- **继承速验**：H6 已有明确符号／契约证据，且相关实现和直接契约无漂移时，继承结论及原证据边界。不能据此跳过同文件未审路径，也不能忽略 Config／资产依赖变化。
- **声明核验**：纯枚举、常量、短声明文件完整检查定义、消费者及反射／配置入口，不人为增加无关调查。
- **完整审阅**：其余实现，包括 H6 未覆盖的战斗路径和 UI、Environment、Framework 等子域，完成声明、实现、条件编译分支及直接依赖核验。

逐文件记录至少包含：所属单元、完成区段、审阅方式、证据来源、消费者／测试、结论或候选 ID、剩余缺口。第 6 节为真实清单；当前统一默认值不能当作审阅证据。

## 3. 审计标准、测试专项与反射保护

Main 在同一批读取中检查架构、正确性和生命周期；Ponytail Audit 辅助复杂度审计。两条线进入同一台账，不另建一轮 Fresh Review。

跨模块接缝必须覆盖：输入／装备 → GAS；StateTree → Controller → Ability；Notify／Task → 激活身份 → 清理；Trace／Projectile → Defense／Resolver → GAS／Reaction；Character／Equipment／ASC → Controller／UI；Camera／Player → Environment／MPC。

### Runtime Contracts

- Character-owned ASC 与 GAS 是玩法状态唯一权威；Controller／StateTree 管意图及编排，UI／反馈不建立第二套玩法状态。
- 异步回调校验宿主、当前激活与身份；`ReadyForActivation()` 返回点按同步重入处理；取消、失败、结束及销毁收敛到既有幂等清理出口。
- 保留近战／处决方向优先级、投射物 Guard／Parry 禁用、失效来源拒绝策略及各动作独立生命周期。
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

## 4. 台账、收益估算及 FIX／SHRINK 分组

### 4.1 H6 继承项与原证据边界

H6 原证据固定读取 `git show 9f2d8e0:plan.md` 第 7／8 节，保留编号及证据归属。以下是已确认局部证据的继承，不是本轮重审或全模块完成记录。

| 继承项 | 本阶段处理与归属 |
|---|---|
| **H6-F01／P2** | 旧 Montage 实例存活不代表资产级调速仍授权该实例。原锚点：EnemyMeleeAbility.cpp:484/512/557、MontageRateWindowLifecycle.cpp:302；完整引擎 API 及反例见历史第 7.1 节。归 FIX1：真实双实例反例、共享实例授权及全部适用消费者；StanceBreak 独立核验。不改称已证实生产回归或远程运行时依赖。 |
| **H6-F02／P2** | 投射物防御弧及反应方向依赖射手命中时位置。原锚点：CombatProjectileHitResolver.cpp:66/73、PlayerGuardAbility.cpp:407、HitReactionImpactResolver.cpp:25；完整链路见历史第 7.1 节。归 FIX2：分离来袭几何与伤害来源，保持近战／处决契约；直接阻塞 03C。 |
| **绑定样板** | 历史第 8.2 节确认 ChargedAttackAbility.cpp:989 与 SprintAttackAbility.cpp:642 的 BindRateWindow 类型名归一后相同，各约 73 行；BowDrawFireAbility.cpp:800 同结构但有结束状态差异。归 SHRINK-A；FIX1 已消除部分按实际变更计入，不能将现有片段行数直接当作净收益。 |
| **11 项 getter** | 完整继承历史第 8.3 节的 Source 零直接消费者候选，实施前重新核验宏和消费面。归 SHRINK-B；逐项删除或有具体消费者／用途的保留裁决。 |
| **原有债务** | 投射物方向覆盖归 FIX2，其余真实发射／飞行／来源失效集成矩阵仍归 03C；authored/readback、Player 致死入口及处决 teardown 等保持 ROADMAP 原唯一归属，不据本次静态检查关闭。 |

11 项 getter 完整清单（头文件位于 `Source/PolyQuest/Public/AbilitySystem/Abilities/`；行号来自未漂移的 H6 源码基线）：

| 头文件 | getter／原行号 | 当前裁决状态 |
|---|---|---|
| EnemyVictimExecutionAbility.h | GetTestHasSavedCanWalkOffLedges／110 | H6 候选继承；SHRINK-B 实施前复核 |
| EnemyLaunchReactionAbility.h | GetTestRootMotionKnockdownCompletedNaturally／78 | H6 候选继承；SHRINK-B 实施前复核 |
| DodgeAbility.h | GetTestAttackingStateTag／70；GetTestDodgingStateTag／71；GetTestHitReactingStateTag／72；GetTestPlayerLaunchReactionAbilityTag／73 | H6 候选继承；四项逐项裁决 |
| ChargedAttackAbility.h | GetTestChargeVFXSystem／294；GetTestChargeVFXTraceSourceName／296；GetTestMaximumChargeDuration／298；GetTestChargeVFXComponent／300；GetTestAttachParent／308 | H6 候选继承；五项逐项裁决 |

### 4.2 候选总表与 Ponytail 单行总览

执行审计时，候选总表记录稳定 ID、类别／严重程度、源码锚点、消费者、证据、裁决、收益、风险、最小候选修改面、验证、实施分组和门禁性质。裁决限定为：**删除、提取合并、简化、保留、有条件延期**；延期必须有具体触发。已有 H6 编号不重复造单。

由同一台账生成 Ponytail 单行总览，不维护第二份独立事实表。格式：

```text
<tag>: <建议削减的内容>. <替换方案>. [路径:行号]（候选 ID；预估净削减约 N 行／估算待定）
```

标签限定为 `delete`、`stdlib`、`native`、`yagni`、`shrink`。正确性缺陷仍按 P0～P3 单列，不能强套复杂度标签。当前尚未执行全量审计，不新增全项目 Findings，不生成伪造的净削减总数。

收益统计口径：

- 净削减估算考虑旧实现删除、替代实现、调用迁移及必要测试增量；允许范围值或“待定”，不制造精准数字。
- 同一代码段只计算一次；互斥方案不相加，FIX1 与 SHRINK 重叠收益不重复计算。
- 分别列出已确认可计量候选合计、条件候选及尚无法估算项。只有全部纳入项可计量时，才称为完整估算。
- 输出 `net: -N lines, -M deps possible.` 时注明统计覆盖范围；未知项另列，不把部分合计写成全项目最终收益。
- 依赖仅计实际可移除的模块／插件／包声明；无用接口单列，不把 getter、include 或文件数冒充依赖数量。
- 删行不是验收指标；收益排序同时考虑契约风险和实施成本。先按严重程度列正确性 Findings，再展示复杂度候选。

### 4.3 后续分组、交接与 03C 门禁

- FIX1：H6-F01 与共享实例授权；FIX2：H6-F02 与投射物方向。
- SHRINK-A：已确认 RateWindow 绑定样板及有证据可共用的消费者部分。11 套 Context 逐项记录已共享部分和必须保留的身份／动作时序差异，不强制合成一个 UObject，不重新抽取 FIX1 已集中的规则。
- SHRINK-B：11 项测试 getter；发现其他已证实测试清理项后再纳入具体白名单。跨文件测试 fixture 提取若影响验证行为，另拆片。
- 其他模块只有出现已证实、已接受候选才建立分组；不预先创建没有 Findings 的 UI／Feedback 清理阶段。
- 同根行为缺陷归已有 FIX；新确认行为缺陷另列独立 FIX。纯测试删除、生产生命周期抽取和资产迁移不混成一片。
- 每片独立冻结文件／符号白名单、输入基线、验证及停止条件，再按项目路线交接实施。Main 保留计划、架构与文档所有权；后续实现按独立计划走 manual/out-of-band Gemini，本轮没有实施交接或派发。
- 仍有用途的 Context 隔离、动作差异和旧 Enemy fixture 的 BladeTrace fallback 保留；当前没有可批准删除的旧 Loadout／Physics 分支，不设删除配额。无用生产状态、setter、反射成员与测试接口分别裁决。

**03C 门禁保持两层含义：** FIX2 是直接运行时接缝门禁；FIX1 和已接受的有限项目级 SHRINK 是现有路线规定的前置工作。普通清理不能冒充运行时依赖，但不能因此取消用户已接受的 SHRINK 排期。新发现的问题依据实际契约影响判断，不能用“只有 P0/P1 才能阻塞”代替分析；未接受建议、未知残留和额外资产迁移不自动扩大门禁。不能只删 getter 或完成 RateWindow 就宣称项目级 SHRINK 完成。

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

三份文档职责、完整覆盖清单、分批规则、继承台账、证据标准、阶段归属和验证矩阵明确；正式写入后通过文档差异检查。此时 FULL-AUDIT 仍未完成。本轮用户明确选择此完成态，检查通过后即返回，不启动全量审计。

### 全项目审计完成条件

1. 当前全部自有源码、测试及外围清单完成对账，每个文件均有完整静态审阅记录；继承项有确切证据，未读部分不能作为延期项蒙混收口。
2. 所有覆盖单元及跨模块接缝都有结论；动态资产证据缺口与源码覆盖缺口明确分开。
3. 每条 Finding／候选都有裁决、验证、唯一归属及实施分组；H6 编号、11 项 getter 和历史收据完整保留。
4. Ponytail 总览与主台账一致；收益估算说明范围、重叠和未知项，不以净行数代替安全与维护收益。
5. 已接受 SHRINK 清单有明确终点；所有接受延期的风险在 ROADMAP 有唯一记录和关闭触发。
6. 除三份文档外无本阶段新增修改；已有 WIP 和暂存状态未被改变，归档仅追加，文档检查通过。
7. 分别报告“全项目静态审计是否完成”和“03C 是否准入”，明确未执行编译、Automation、Editor／PIE及 Fresh Review。

达到对应完成条件即停止，不为凑删行继续探索，不自动启动修复、清理或提交。

## 6. 逐文件待审清单与外围清单

本清单由当前在盘 Source 文件枚举生成；下表路径相对项目根目录。**所有 230 行当前均为待审，无已完成区段；审阅方式、证据来源、消费者／测试、结论与缺口均待核验。** 表内“待核验”是这些字段的共同初值；执行时必须替换为实际审阅方式、可追溯证据、消费者／测试、结论／候选 ID 及剩余缺口，不能只改状态。H6 局部继承见第 4.1 节，不抵扣完整文件数量。

### 模块与构建入口（5 文件）

| 文件 | 总行数 | 状态／已完成区段 | 审阅方式、证据、消费者／测试、结论与缺口 |
|---|---:|---|---|
| Source/PolyQuest.Target.cs | 15 | 待审／无 | 待核验 |
| Source/PolyQuest/PolyQuest.Build.cs | 45 | 待审／无 | 待核验 |
| Source/PolyQuest/PolyQuest.cpp | 8 | 待审／无 | 待核验 |
| Source/PolyQuest/PolyQuest.h | 8 | 待审／无 | 待核验 |
| Source/PolyQuestEditor.Target.cs | 15 | 待审／无 | 待核验 |

### AbilitySystem（60 文件）

| 文件 | 总行数 | 状态／已完成区段 | 审阅方式、证据、消费者／测试、结论与缺口 |
|---|---:|---|---|
| Source/PolyQuest/Private/AbilitySystem/Abilities/BowDrawFireAbility.cpp | 919 | 待审／无 | 待核验 |
| Source/PolyQuest/Private/AbilitySystem/Abilities/ChargedAttackAbility.cpp | 1108 | 待审／无 | 待核验 |
| Source/PolyQuest/Private/AbilitySystem/Abilities/DodgeAbility.cpp | 564 | 待审／无 | 待核验 |
| Source/PolyQuest/Private/AbilitySystem/Abilities/EnemyHitReactionAbility.cpp | 458 | 待审／无 | 待核验 |
| Source/PolyQuest/Private/AbilitySystem/Abilities/EnemyLaunchReactionAbility.cpp | 710 | 待审／无 | 待核验 |
| Source/PolyQuest/Private/AbilitySystem/Abilities/EnemyMeleeAbility.cpp | 584 | 待审／无 | 待核验 |
| Source/PolyQuest/Private/AbilitySystem/Abilities/EnemySmallHitReactionAbility.cpp | 445 | 待审／无 | 待核验 |
| Source/PolyQuest/Private/AbilitySystem/Abilities/EnemyStanceBreakAbility.cpp | 432 | 待审／无 | 待核验 |
| Source/PolyQuest/Private/AbilitySystem/Abilities/EnemyVictimExecutionAbility.cpp | 1572 | 待审／无 | 待核验 |
| Source/PolyQuest/Private/AbilitySystem/Abilities/JumpAbility.cpp | 91 | 待审／无 | 待核验 |
| Source/PolyQuest/Private/AbilitySystem/Abilities/LaunchFacingSmoothingState.cpp | 75 | 待审／无 | 待核验 |
| Source/PolyQuest/Private/AbilitySystem/Abilities/LightAttackAbility.cpp | 1072 | 待审／无 | 待核验 |
| Source/PolyQuest/Private/AbilitySystem/Abilities/MeleeTraceWindowLifecycle.cpp | 116 | 待审／无 | 待核验 |
| Source/PolyQuest/Private/AbilitySystem/Abilities/MeleeTraceWindowLifecycle.h | 44 | 待审／无 | 待核验 |
| Source/PolyQuest/Private/AbilitySystem/Abilities/MontageRateWindowLifecycle.cpp | 344 | 待审／无 | 待核验 |
| Source/PolyQuest/Private/AbilitySystem/Abilities/PlayerBackstabExecutionAbility.cpp | 1303 | 待审／无 | 待核验 |
| Source/PolyQuest/Private/AbilitySystem/Abilities/PlayerBigHitReactionAbility.cpp | 278 | 待审／无 | 待核验 |
| Source/PolyQuest/Private/AbilitySystem/Abilities/PlayerFrontExecutionAbility.cpp | 1281 | 待审／无 | 待核验 |
| Source/PolyQuest/Private/AbilitySystem/Abilities/PlayerGuardAbility.cpp | 470 | 待审／无 | 待核验 |
| Source/PolyQuest/Private/AbilitySystem/Abilities/PlayerGuardBreakAbility.cpp | 204 | 待审／无 | 待核验 |
| Source/PolyQuest/Private/AbilitySystem/Abilities/PlayerLaunchReactionAbility.cpp | 643 | 待审／无 | 待核验 |
| Source/PolyQuest/Private/AbilitySystem/Abilities/PlayerMeleeSkillAbility.cpp | 894 | 待审／无 | 待核验 |
| Source/PolyQuest/Private/AbilitySystem/Abilities/PlayerParryAbility.cpp | 449 | 待审／无 | 待核验 |
| Source/PolyQuest/Private/AbilitySystem/Abilities/PlayerSmallHitReactionAbility.cpp | 264 | 待审／无 | 待核验 |
| Source/PolyQuest/Private/AbilitySystem/Abilities/PrimaryAttackAbility.cpp | 218 | 待审／无 | 待核验 |
| Source/PolyQuest/Private/AbilitySystem/Abilities/SprintAbility.cpp | 212 | 待审／无 | 待核验 |
| Source/PolyQuest/Private/AbilitySystem/Abilities/SprintAttackAbility.cpp | 761 | 待审／无 | 待核验 |
| Source/PolyQuest/Private/AbilitySystem/Abilities/StaminaActionAbility.cpp | 111 | 待审／无 | 待核验 |
| Source/PolyQuest/Private/AbilitySystem/CharacterAttributeSet.cpp | 117 | 待审／无 | 待核验 |
| Source/PolyQuest/Private/AbilitySystem/Tasks/AbilityTask_MeleeTraceWindow.cpp | 284 | 待审／无 | 待核验 |
| Source/PolyQuest/Private/AbilitySystem/Tasks/AbilityTask_TurnToFacing.cpp | 130 | 待审／无 | 待核验 |
| Source/PolyQuest/Public/AbilitySystem/Abilities/BowDrawFireAbility.h | 255 | 待审／无 | 待核验 |
| Source/PolyQuest/Public/AbilitySystem/Abilities/ChargedAttackAbility.h | 360 | 待审／无 | 待核验 |
| Source/PolyQuest/Public/AbilitySystem/Abilities/DodgeAbility.h | 195 | 待审／无 | 待核验 |
| Source/PolyQuest/Public/AbilitySystem/Abilities/EnemyHitReactionAbility.h | 174 | 待审／无 | 待核验 |
| Source/PolyQuest/Public/AbilitySystem/Abilities/EnemyLaunchReactionAbility.h | 208 | 待审／无 | 待核验 |
| Source/PolyQuest/Public/AbilitySystem/Abilities/EnemyMeleeAbility.h | 189 | 待审／无 | 待核验 |
| Source/PolyQuest/Public/AbilitySystem/Abilities/EnemySmallHitReactionAbility.h | 180 | 待审／无 | 待核验 |
| Source/PolyQuest/Public/AbilitySystem/Abilities/EnemyStanceBreakAbility.h | 183 | 待审／无 | 待核验 |
| Source/PolyQuest/Public/AbilitySystem/Abilities/EnemyVictimExecutionAbility.h | 295 | 待审／无 | 待核验 |
| Source/PolyQuest/Public/AbilitySystem/Abilities/JumpAbility.h | 48 | 待审／无 | 待核验 |
| Source/PolyQuest/Public/AbilitySystem/Abilities/LaunchFacingSmoothingState.h | 42 | 待审／无 | 待核验 |
| Source/PolyQuest/Public/AbilitySystem/Abilities/LightAttackAbility.h | 249 | 待审／无 | 待核验 |
| Source/PolyQuest/Public/AbilitySystem/Abilities/MontageRateWindowLifecycle.h | 105 | 待审／无 | 待核验 |
| Source/PolyQuest/Public/AbilitySystem/Abilities/PlayerBackstabExecutionAbility.h | 220 | 待审／无 | 待核验 |
| Source/PolyQuest/Public/AbilitySystem/Abilities/PlayerBigHitReactionAbility.h | 104 | 待审／无 | 待核验 |
| Source/PolyQuest/Public/AbilitySystem/Abilities/PlayerFrontExecutionAbility.h | 219 | 待审／无 | 待核验 |
| Source/PolyQuest/Public/AbilitySystem/Abilities/PlayerGuardAbility.h | 146 | 待审／无 | 待核验 |
| Source/PolyQuest/Public/AbilitySystem/Abilities/PlayerGuardBreakAbility.h | 80 | 待审／无 | 待核验 |
| Source/PolyQuest/Public/AbilitySystem/Abilities/PlayerLaunchReactionAbility.h | 173 | 待审／无 | 待核验 |
| Source/PolyQuest/Public/AbilitySystem/Abilities/PlayerMeleeSkillAbility.h | 243 | 待审／无 | 待核验 |
| Source/PolyQuest/Public/AbilitySystem/Abilities/PlayerParryAbility.h | 136 | 待审／无 | 待核验 |
| Source/PolyQuest/Public/AbilitySystem/Abilities/PlayerSmallHitReactionAbility.h | 110 | 待审／无 | 待核验 |
| Source/PolyQuest/Public/AbilitySystem/Abilities/PrimaryAttackAbility.h | 79 | 待审／无 | 待核验 |
| Source/PolyQuest/Public/AbilitySystem/Abilities/SprintAbility.h | 64 | 待审／无 | 待核验 |
| Source/PolyQuest/Public/AbilitySystem/Abilities/SprintAttackAbility.h | 228 | 待审／无 | 待核验 |
| Source/PolyQuest/Public/AbilitySystem/Abilities/StaminaActionAbility.h | 59 | 待审／无 | 待核验 |
| Source/PolyQuest/Public/AbilitySystem/CharacterAttributeSet.h | 61 | 待审／无 | 待核验 |
| Source/PolyQuest/Public/AbilitySystem/Tasks/AbilityTask_MeleeTraceWindow.h | 78 | 待审／无 | 待核验 |
| Source/PolyQuest/Public/AbilitySystem/Tasks/AbilityTask_TurnToFacing.h | 51 | 待审／无 | 待核验 |

### AI（8 文件）

| 文件 | 总行数 | 状态／已完成区段 | 审阅方式、证据、消费者／测试、结论与缺口 |
|---|---:|---|---|
| Source/PolyQuest/Private/AI/EnemyAIController.cpp | 1172 | 待审／无 | 待核验 |
| Source/PolyQuest/Private/AI/EnemyAIProfile.cpp | 50 | 待审／无 | 待核验 |
| Source/PolyQuest/Private/AI/StateTree/EnemyStateTreeConditions.cpp | 113 | 待审／无 | 待核验 |
| Source/PolyQuest/Private/AI/StateTree/EnemyStateTreeTasks.cpp | 362 | 待审／无 | 待核验 |
| Source/PolyQuest/Public/AI/EnemyAIController.h | 330 | 待审／无 | 待核验 |
| Source/PolyQuest/Public/AI/EnemyAIProfile.h | 87 | 待审／无 | 待核验 |
| Source/PolyQuest/Public/AI/StateTree/EnemyStateTreeConditions.h | 208 | 待审／无 | 待核验 |
| Source/PolyQuest/Public/AI/StateTree/EnemyStateTreeTasks.h | 144 | 待审／无 | 待核验 |

### Animation（10 文件）

| 文件 | 总行数 | 状态／已完成区段 | 审阅方式、证据、消费者／测试、结论与缺口 |
|---|---:|---|---|
| Source/PolyQuest/Private/Animation/Combat/AnimNotify_PlayerBowAction.cpp | 57 | 待审／无 | 待核验 |
| Source/PolyQuest/Private/Animation/Combat/AnimNotify_PlayerExecutionHit.cpp | 47 | 待审／无 | 待核验 |
| Source/PolyQuest/Private/Animation/Combat/AnimNotify_PlayerExecutionVictimStart.cpp | 41 | 待审／无 | 待核验 |
| Source/PolyQuest/Private/Animation/Combat/AnimNotify_ReactionLaunchCommit.cpp | 39 | 待审／无 | 待核验 |
| Source/PolyQuest/Private/Animation/Combat/AnimNotifyState_ActionWindows.cpp | 195 | 待审／无 | 待核验 |
| Source/PolyQuest/Public/Animation/Combat/AnimNotify_PlayerBowAction.h | 27 | 待审／无 | 待核验 |
| Source/PolyQuest/Public/Animation/Combat/AnimNotify_PlayerExecutionHit.h | 19 | 待审／无 | 待核验 |
| Source/PolyQuest/Public/Animation/Combat/AnimNotify_PlayerExecutionVictimStart.h | 24 | 待审／无 | 待核验 |
| Source/PolyQuest/Public/Animation/Combat/AnimNotify_ReactionLaunchCommit.h | 20 | 待审／无 | 待核验 |
| Source/PolyQuest/Public/Animation/Combat/AnimNotifyState_ActionWindows.h | 119 | 待审／无 | 待核验 |

### Camera（2 文件）

| 文件 | 总行数 | 状态／已完成区段 | 审阅方式、证据、消费者／测试、结论与缺口 |
|---|---:|---|---|
| Source/PolyQuest/Private/Camera/CameraModifier_FovPunch.cpp | 40 | 待审／无 | 待核验 |
| Source/PolyQuest/Public/Camera/CameraModifier_FovPunch.h | 51 | 待审／无 | 待核验 |

### Character（8 文件）

| 文件 | 总行数 | 状态／已完成区段 | 审阅方式、证据、消费者／测试、结论与缺口 |
|---|---:|---|---|
| Source/PolyQuest/Private/Character/BaseCharacter.cpp | 262 | 待审／无 | 待核验 |
| Source/PolyQuest/Private/Character/Enemy/EnemyCharacter.cpp | 1369 | 待审／无 | 待核验 |
| Source/PolyQuest/Private/Character/Player/PlayerCharacter.cpp | 3455 | 待审／无 | 待核验 |
| Source/PolyQuest/Private/Character/Player/PlayerLockOnTargeting.cpp | 209 | 待审／无 | 待核验 |
| Source/PolyQuest/Public/Character/BaseCharacter.h | 100 | 待审／无 | 待核验 |
| Source/PolyQuest/Public/Character/Enemy/EnemyCharacter.h | 247 | 待审／无 | 待核验 |
| Source/PolyQuest/Public/Character/Player/PlayerCharacter.h | 633 | 待审／无 | 待核验 |
| Source/PolyQuest/Public/Character/Player/PlayerLockOnTargeting.h | 34 | 待审／无 | 待核验 |

### Combat（47 文件）

| 文件 | 总行数 | 状态／已完成区段 | 审阅方式、证据、消费者／测试、结论与缺口 |
|---|---:|---|---|
| Source/PolyQuest/Private/Combat/Enemy/EnemyAttackProfile.cpp | 12 | 待审／无 | 待核验 |
| Source/PolyQuest/Private/Combat/Enemy/EnemyAttackSet.cpp | 116 | 待审／无 | 待核验 |
| Source/PolyQuest/Private/Combat/Equipment/BowWeaponDefinition.cpp | 120 | 待审／无 | 待核验 |
| Source/PolyQuest/Private/Combat/Equipment/MeleeWeaponDefinition.cpp | 324 | 待审／无 | 待核验 |
| Source/PolyQuest/Private/Combat/Equipment/ProjectileDefinition.cpp | 113 | 待审／无 | 待核验 |
| Source/PolyQuest/Private/Combat/Equipment/WeaponEquipmentComponent.cpp | 1366 | 待审／无 | 待核验 |
| Source/PolyQuest/Private/Combat/Equipment/WorldPickupGrounding.cpp | 98 | 待审／无 | 待核验 |
| Source/PolyQuest/Private/Combat/Equipment/WorldPickupGrounding.h | 34 | 待审／无 | 待核验 |
| Source/PolyQuest/Private/Combat/Equipment/WorldWeaponPickup.cpp | 415 | 待审／无 | 待核验 |
| Source/PolyQuest/Private/Combat/Execution/ExecutionLockContext.cpp | 439 | 待审／无 | 待核验 |
| Source/PolyQuest/Private/Combat/Execution/ExecutionSnapAlignment.cpp | 136 | 待审／无 | 待核验 |
| Source/PolyQuest/Private/Combat/Feedback/CombatFeedbackDataAsset.cpp | 63 | 待审／无 | 待核验 |
| Source/PolyQuest/Private/Combat/Melee/MeleeHitResolver.cpp | 182 | 待审／无 | 待核验 |
| Source/PolyQuest/Private/Combat/Melee/MeleeMotionWarping.cpp | 130 | 待审／无 | 待核验 |
| Source/PolyQuest/Private/Combat/Melee/MeleeTraceSourceComponent.cpp | 335 | 待审／无 | 待核验 |
| Source/PolyQuest/Private/Combat/Melee/MeleeWeaponTrailComponent.cpp | 252 | 待审／无 | 待核验 |
| Source/PolyQuest/Private/Combat/Melee/MeleeWeaponTrailComponent.h | 74 | 待审／无 | 待核验 |
| Source/PolyQuest/Private/Combat/Projectile/CombatProjectile.cpp | 671 | 待审／无 | 待核验 |
| Source/PolyQuest/Private/Combat/Projectile/CombatProjectileHitResolver.cpp | 112 | 待审／无 | 待核验 |
| Source/PolyQuest/Private/Combat/Projectile/CombatProjectileTargeting.cpp | 317 | 待审／无 | 待核验 |
| Source/PolyQuest/Private/Combat/Reaction/HitReactionClassifier.cpp | 59 | 待审／无 | 待核验 |
| Source/PolyQuest/Private/Combat/Reaction/HitReactionFourWayMontageSelector.cpp | 30 | 待审／无 | 待核验 |
| Source/PolyQuest/Private/Combat/Reaction/HitReactionFourWayMontageSelector.h | 39 | 待审／无 | 待核验 |
| Source/PolyQuest/Private/Combat/Reaction/HitReactionImpactResolver.cpp | 175 | 待审／无 | 待核验 |
| Source/PolyQuest/Public/Combat/ComboChainDataAsset.h | 61 | 待审／无 | 待核验 |
| Source/PolyQuest/Public/Combat/Enemy/EnemyAttackProfile.h | 50 | 待审／无 | 待核验 |
| Source/PolyQuest/Public/Combat/Enemy/EnemyAttackSet.h | 78 | 待审／无 | 待核验 |
| Source/PolyQuest/Public/Combat/Equipment/BowWeaponDefinition.h | 31 | 待审／无 | 待核验 |
| Source/PolyQuest/Public/Combat/Equipment/DefenseProfileDefinition.h | 32 | 待审／无 | 待核验 |
| Source/PolyQuest/Public/Combat/Equipment/MeleeWeaponDefinition.h | 114 | 待审／无 | 待核验 |
| Source/PolyQuest/Public/Combat/Equipment/OffHandWeaponDefinition.h | 58 | 待审／无 | 待核验 |
| Source/PolyQuest/Public/Combat/Equipment/ProjectileDefinition.h | 108 | 待审／无 | 待核验 |
| Source/PolyQuest/Public/Combat/Equipment/WeaponDefinition.h | 325 | 待审／无 | 待核验 |
| Source/PolyQuest/Public/Combat/Equipment/WeaponEquipmentComponent.h | 197 | 待审／无 | 待核验 |
| Source/PolyQuest/Public/Combat/Equipment/WorldWeaponPickup.h | 119 | 待审／无 | 待核验 |
| Source/PolyQuest/Public/Combat/Execution/ExecutionLockContext.h | 217 | 待审／无 | 待核验 |
| Source/PolyQuest/Public/Combat/Execution/ExecutionSnapAlignment.h | 44 | 待审／无 | 待核验 |
| Source/PolyQuest/Public/Combat/Feedback/CombatFeedbackDataAsset.h | 201 | 待审／无 | 待核验 |
| Source/PolyQuest/Public/Combat/Melee/CombatTeamAgent.h | 23 | 待审／无 | 待核验 |
| Source/PolyQuest/Public/Combat/Melee/MeleeHitResolver.h | 35 | 待审／无 | 待核验 |
| Source/PolyQuest/Public/Combat/Melee/MeleeMotionWarping.h | 56 | 待审／无 | 待核验 |
| Source/PolyQuest/Public/Combat/Melee/MeleeTraceSourceComponent.h | 77 | 待审／无 | 待核验 |
| Source/PolyQuest/Public/Combat/Projectile/CombatProjectile.h | 183 | 待审／无 | 待核验 |
| Source/PolyQuest/Public/Combat/Projectile/CombatProjectileHitResolver.h | 33 | 待审／无 | 待核验 |
| Source/PolyQuest/Public/Combat/Projectile/CombatProjectileTargeting.h | 71 | 待审／无 | 待核验 |
| Source/PolyQuest/Public/Combat/Reaction/HitReactionClassifier.h | 31 | 待审／无 | 待核验 |
| Source/PolyQuest/Public/Combat/Reaction/HitReactionImpactResolver.h | 51 | 待审／无 | 待核验 |

### Environment（4 文件）

| 文件 | 总行数 | 状态／已完成区段 | 审阅方式、证据、消费者／测试、结论与缺口 |
|---|---:|---|---|
| Source/PolyQuest/Private/Environment/FloorTriggerVolume.cpp | 96 | 待审／无 | 待核验 |
| Source/PolyQuest/Private/Environment/FloorVolume.cpp | 373 | 待审／无 | 待核验 |
| Source/PolyQuest/Public/Environment/FloorTriggerVolume.h | 45 | 待审／无 | 待核验 |
| Source/PolyQuest/Public/Environment/FloorVolume.h | 93 | 待审／无 | 待核验 |

### Framework（4 文件）

| 文件 | 总行数 | 状态／已完成区段 | 审阅方式、证据、消费者／测试、结论与缺口 |
|---|---:|---|---|
| Source/PolyQuest/Private/Framework/PolyQuestGameMode.cpp | 8 | 待审／无 | 待核验 |
| Source/PolyQuest/Private/Framework/PolyQuestPlayerController.cpp | 582 | 待审／无 | 待核验 |
| Source/PolyQuest/Public/Framework/PolyQuestGameMode.h | 21 | 待审／无 | 待核验 |
| Source/PolyQuest/Public/Framework/PolyQuestPlayerController.h | 147 | 待审／无 | 待核验 |

### UI（10 文件）

| 文件 | 总行数 | 状态／已完成区段 | 审阅方式、证据、消费者／测试、结论与缺口 |
|---|---:|---|---|
| Source/PolyQuest/Private/UI/EnemyHealthBarWidget.cpp | 316 | 待审／无 | 待核验 |
| Source/PolyQuest/Private/UI/PlayerSkillBarHUDWidget.cpp | 248 | 待审／无 | 待核验 |
| Source/PolyQuest/Private/UI/PlayerSkillSlotWidget.cpp | 132 | 待审／无 | 待核验 |
| Source/PolyQuest/Private/UI/PlayerVitalHUDWidget.cpp | 767 | 待审／无 | 待核验 |
| Source/PolyQuest/Private/UI/WorldInteractionPromptWidget.cpp | 24 | 待审／无 | 待核验 |
| Source/PolyQuest/Public/UI/EnemyHealthBarWidget.h | 109 | 待审／无 | 待核验 |
| Source/PolyQuest/Public/UI/PlayerSkillBarHUDWidget.h | 74 | 待审／无 | 待核验 |
| Source/PolyQuest/Public/UI/PlayerSkillSlotWidget.h | 86 | 待审／无 | 待核验 |
| Source/PolyQuest/Public/UI/PlayerVitalHUDWidget.h | 235 | 待审／无 | 待核验 |
| Source/PolyQuest/Public/UI/WorldInteractionPromptWidget.h | 32 | 待审／无 | 待核验 |

### Tests（72 文件）

| 文件 | 总行数 | 状态／已完成区段 | 审阅方式、证据、消费者／测试、结论与缺口 |
|---|---:|---|---|
| Source/PolyQuest/Private/Tests/BackstabExecutionAutomationTests.cpp | 1456 | 待审／无 | 待核验 |
| Source/PolyQuest/Private/Tests/ChargedAttackNiagaraFeedbackAutomationTests.cpp | 414 | 待审／无 | 待核验 |
| Source/PolyQuest/Private/Tests/CombatAutomationFixture.cpp | 148 | 待审／无 | 待核验 |
| Source/PolyQuest/Private/Tests/CombatAutomationFixture.h | 31 | 待审／无 | 待核验 |
| Source/PolyQuest/Private/Tests/CombatHitFeedbackAutomationTests.cpp | 1151 | 待审／无 | 待核验 |
| Source/PolyQuest/Private/Tests/EnemyAttackSetAutomationTests.cpp | 239 | 待审／无 | 待核验 |
| Source/PolyQuest/Private/Tests/EnemyCombatSpacingTests.cpp | 681 | 待审／无 | 待核验 |
| Source/PolyQuest/Private/Tests/EnemyCombatTargetRetentionAutomationTests.cpp | 288 | 待审／无 | 待核验 |
| Source/PolyQuest/Private/Tests/EnemyDeathRagdollAutomationTests.cpp | 385 | 待审／无 | 待核验 |
| Source/PolyQuest/Private/Tests/EnemyLaunchReactionRootMotionAutomationTests.cpp | 793 | 待审／无 | 待核验 |
| Source/PolyQuest/Private/Tests/EnemyMontageRateWindowAutomationTests.cpp | 1058 | 待审／无 | 待核验 |
| Source/PolyQuest/Private/Tests/EnemyRootMotionFacingAutomationTests.cpp | 395 | 待审／无 | 待核验 |
| Source/PolyQuest/Private/Tests/EnemyStanceBreakRateWindowAutomationTests.cpp | 547 | 待审／无 | 待核验 |
| Source/PolyQuest/Private/Tests/ExecutionHitNotifyAutomationTests.cpp | 512 | 待审／无 | 待核验 |
| Source/PolyQuest/Private/Tests/ExecutionImpactFeedbackAutomationTests.cpp | 659 | 待审／无 | 待核验 |
| Source/PolyQuest/Private/Tests/ExecutionLethalRecoveryAutomationTests.cpp | 883 | 待审／无 | 待核验 |
| Source/PolyQuest/Private/Tests/ExecutionLockInAutomationTests.cpp | 736 | 待审／无 | 待核验 |
| Source/PolyQuest/Private/Tests/ExecutionReleaseOutcomesAutomationTests.cpp | 1226 | 待审／无 | 待核验 |
| Source/PolyQuest/Private/Tests/ExecutionSnapAlignmentAutomationTests.cpp | 304 | 待审／无 | 待核验 |
| Source/PolyQuest/Private/Tests/ExecutionVictimPresentationAutomationTests.cpp | 1552 | 待审／无 | 待核验 |
| Source/PolyQuest/Private/Tests/ExecutionVictimRootMotionAutomationTests.cpp | 532 | 待审／无 | 待核验 |
| Source/PolyQuest/Private/Tests/FloorVisibilityAutomationTests.cpp | 252 | 待审／无 | 待核验 |
| Source/PolyQuest/Private/Tests/FrontExecutionAutomationTests.cpp | 985 | 待审／无 | 待核验 |
| Source/PolyQuest/Private/Tests/HitReactionAutomationTests.cpp | 2223 | 待审／无 | 待核验 |
| Source/PolyQuest/Private/Tests/LaunchFacingSmoothingAutomationTests.cpp | 346 | 待审／无 | 待核验 |
| Source/PolyQuest/Private/Tests/MeleeMultiTraceSourceAutomationTests.cpp | 712 | 待审／无 | 待核验 |
| Source/PolyQuest/Private/Tests/MeleeTraceSourceComponentAutomationTests.cpp | 396 | 待审／无 | 待核验 |
| Source/PolyQuest/Private/Tests/MeleeTraceWindowLifecycleAutomationTests.cpp | 333 | 待审／无 | 待核验 |
| Source/PolyQuest/Private/Tests/MeleeWeaponTrailAutomationTests.cpp | 460 | 待审／无 | 待核验 |
| Source/PolyQuest/Private/Tests/ParrySuccessImpactFeedbackAutomationTests.cpp | 394 | 待审／无 | 待核验 |
| Source/PolyQuest/Private/Tests/PlayerActionWindowAutomationTests.cpp | 866 | 待审／无 | 待核验 |
| Source/PolyQuest/Private/Tests/PlayerDefenseAudioAutomationTests.cpp | 608 | 待审／无 | 待核验 |
| Source/PolyQuest/Private/Tests/PlayerExhaustionAutomationTests.cpp | 242 | 待审／无 | 待核验 |
| Source/PolyQuest/Private/Tests/PlayerLaunchReactionRootMotionAutomationTests.cpp | 713 | 待审／无 | 待核验 |
| Source/PolyQuest/Private/Tests/PlayerLockOnAutomationTests.cpp | 1349 | 待审／无 | 待核验 |
| Source/PolyQuest/Private/Tests/PlayerMeleeMotionWarpingAutomationTests.cpp | 1759 | 待审／无 | 待核验 |
| Source/PolyQuest/Private/Tests/PlayerMobileBowAutomationTests.cpp | 299 | 待审／无 | 待核验 |
| Source/PolyQuest/Private/Tests/PlayerMontageRateWindowAutomationTests.cpp | 1366 | 待审／无 | 待核验 |
| Source/PolyQuest/Private/Tests/ProjectileFlightTrailAutomationTests.cpp | 614 | 待审／无 | 待核验 |
| Source/PolyQuest/Private/Tests/ProjectileLifecycleAutomationTests.cpp | 1001 | 待审／无 | 待核验 |
| Source/PolyQuest/Private/Tests/ProjectileTargetAssistAutomationTests.cpp | 560 | 待审／无 | 待核验 |
| Source/PolyQuest/Private/Tests/SkillBarHudAutomationTests.cpp | 498 | 待审／无 | 待核验 |
| Source/PolyQuest/Private/Tests/TestExhaustionMoveSpeedGE.cpp | 14 | 待审／无 | 待核验 |
| Source/PolyQuest/Private/Tests/TestExhaustionMoveSpeedGE.h | 15 | 待审／无 | 待核验 |
| Source/PolyQuest/Private/Tests/TestGuardStaminaCostGE.cpp | 19 | 待审／无 | 待核验 |
| Source/PolyQuest/Private/Tests/TestGuardStaminaCostGE.h | 18 | 待审／无 | 待核验 |
| Source/PolyQuest/Private/Tests/TestHitFeedbackCameraShake.cpp | 19 | 待审／无 | 待核验 |
| Source/PolyQuest/Private/Tests/TestHitFeedbackCameraShake.h | 61 | 待审／无 | 待核验 |
| Source/PolyQuest/Private/Tests/TestJumpGameplayEffects.cpp | 31 | 待审／无 | 待核验 |
| Source/PolyQuest/Private/Tests/TestJumpGameplayEffects.h | 36 | 待审／无 | 待核验 |
| Source/PolyQuest/Private/Tests/TestLaunchFacingSmoothingAbility.cpp | 105 | 待审／无 | 待核验 |
| Source/PolyQuest/Private/Tests/TestLaunchFacingSmoothingAbility.h | 66 | 待审／无 | 待核验 |
| Source/PolyQuest/Private/Tests/TestMeleeTrailAbility.cpp | 128 | 待审／无 | 待核验 |
| Source/PolyQuest/Private/Tests/TestMeleeTrailAbility.h | 54 | 待审／无 | 待核验 |
| Source/PolyQuest/Private/Tests/TestMobileBowMoveSpeedGE.cpp | 14 | 待审／无 | 待核验 |
| Source/PolyQuest/Private/Tests/TestMobileBowMoveSpeedGE.h | 15 | 待审／无 | 待核验 |
| Source/PolyQuest/Private/Tests/TestMobileBowSprintAbility.cpp | 21 | 待审／无 | 待核验 |
| Source/PolyQuest/Private/Tests/TestMobileBowSprintAbility.h | 18 | 待审／无 | 待核验 |
| Source/PolyQuest/Private/Tests/TestParryCounterPoiseGE.cpp | 19 | 待审／无 | 待核验 |
| Source/PolyQuest/Private/Tests/TestParryCounterPoiseGE.h | 18 | 待审／无 | 待核验 |
| Source/PolyQuest/Private/Tests/TestPoiseRecoveryGE.cpp | 68 | 待审／无 | 待核验 |
| Source/PolyQuest/Private/Tests/TestPoiseRecoveryGE.h | 54 | 待审／无 | 待核验 |
| Source/PolyQuest/Private/Tests/TestPreparedSkillCooldownFixtures.cpp | 71 | 待审／无 | 待核验 |
| Source/PolyQuest/Private/Tests/TestPreparedSkillCooldownFixtures.h | 52 | 待审／无 | 待核验 |
| Source/PolyQuest/Private/Tests/TestProjectileDamageGE.cpp | 14 | 待审／无 | 待核验 |
| Source/PolyQuest/Private/Tests/TestProjectileDamageGE.h | 18 | 待审／无 | 待核验 |
| Source/PolyQuest/Private/Tests/TestStaminaRegenGE.cpp | 6 | 待审／无 | 待核验 |
| Source/PolyQuest/Private/Tests/TestStaminaRegenGE.h | 18 | 待审／无 | 待核验 |
| Source/PolyQuest/Private/Tests/VitalHudAutomationTests.cpp | 1013 | 待审／无 | 待核验 |
| Source/PolyQuest/Private/Tests/WeaponEquipmentComponentAutomationTests.cpp | 1404 | 待审／无 | 待核验 |
| Source/PolyQuest/Private/Tests/WorldInteractionPromptAutomationTests.cpp | 312 | 待审／无 | 待核验 |
| Source/PolyQuest/Private/Tests/WorldPickupGroundingAutomationTests.cpp | 187 | 待审／无 | 待核验 |

### 外围清单（不计入 Source 230 文件）

| 路径／对象 | 盘点状态与后续审计边界 |
|---|---|
| PolyQuest.uproject | 在盘；声明的自有 Runtime Module 为 PolyQuest；模块／插件声明及消费关系待审 |
| Config/DefaultEngine.ini | 在盘；地图／渲染／平台／碰撞等配置入口待审 |
| Config/DefaultGame.ini | 在盘；游戏／AssetManager／GAS 配置入口待审 |
| Config/DefaultInput.ini | 在盘；输入配置待审 |
| Config/DefaultEditor.ini | 在盘；Editor 配置消费关系待审 |
| Config/DefaultEditorPerProjectUserSettings.ini | 在盘；本地 Editor 配置，保留，用途与排除理由待审 |
| Config/Tags/PolyQuestGameplayTags.ini | 在盘；Tag 注册与原生／动态消费者待审 |
| Config/Tests/Tags.ini | 在盘；测试映射及消费者待审 |
| Config/Automation/Presets/UI.json | 在盘；Automation 预设与真实测试名对应关系待审 |
| Config/Automation/Presets/1.json | 已有删除 WIP；只登记当前缺失，不恢复，不将旧版本冒充在盘配置 |
| tmp/pdfs/inspect_layout.py | 已盘点为临时 PDF 布局检查脚本，与游戏运行／构建无关；排除游戏代码瘦身，保留 WIP |
| Plugins/ 与正式工具目录 | 当前未发现项目级自有插件／正式工具目录；执行前再核对新增自有代码，不据此删除外部依赖 |
| .codex/config.toml | 项目本地代理配置；保留且不修改，不作为游戏源码瘦身目标 |
| Content/** | 只作为适用资产／反射／软引用消费者证据边界；本阶段不操作 Editor、不改资产，缺口逐候选登记 |
| 引擎／第三方／生成及构建缓存 | 不作为自有代码删除范围；仅按实际消费关系定位必要外部接口 |

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
