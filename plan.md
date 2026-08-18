# TODO-03AI2：Distance-Aware Weighted Melee Approach And Attack Execution v1

## 状态与目标

- **Plan state:** COMPLETED
- **Baseline:** 74ab01c（盾牌防御标签作者化兼容修复）
- **Prerequisites:** TODO-03AI、TODO-03AI1A、TODO-03AI1B、TODO-03A5B 已完成。
- **Primary question:** 当敌人在 EngagementRange 内时，能否先对全部近战招式进行一次加权选择，再根据选中招式的 AttackRange 决定立即攻击或主动接近，而不是在选择阶段提前过滤短招。

~~~
Outer: ue-stage-workflow
Primary: ue5-state-tree-ai
Support: ue5-cpp-gameplay, ue5-debug-validation
Route reason: 本阶段同时涉及 AttackSet 选择契约、Controller 运行时决策状态、StateTree 接近分支和 GAS Ability 交接。
~~~

## 锁定行为

- UEnemyAttackSet::EngagementRange 是进入 Combat 和允许重新决策的外层距离。
- 目标进入 EngagementRange 后，对当前 AttackSet 中所有有效 Entry 按权重抽取一次，不再按当前距离过滤 AttackRange。
- 选中的 AttackProfile 在本次决策期间保持不变，不在 Approach 过程中重新抽招。
- 如果目标距离已经小于等于 SelectedProfile.AttackRange，直接进入攻击。
- 如果目标仍在 EngagementRange 内，但距离大于选中招式的 AttackRange，进入 Approach。
- Approach 使用当前 TargetActor 作为动态目标，每次距离判断使用 2D Actor-center 距离。
- 当 Distance2D 小于等于 SelectedProfile.AttackRange 时停止接近并请求 GAS Ability。
- 当目标离开 EngagementRange、目标失效、敌人死亡/眩晕/受击反应或超出 Leash 时，清除待攻击 Profile，停止 Approach，返回 Chase/Alert。
- Approach 导航失败或超过 UEnemyAIProfile::ApproachTimeout 时，清除待攻击 Profile；若目标仍在 EngagementRange 内，经过现有 Wait = 0.5s 后重新决策一次。
- 纯短招集合（所有招式 AttackRange < EngagementRange）完全合法，抽中后由 Approach 动态接近到对应 AttackRange 出招；所有招式在 EngagementRange 内均参与纯加权选择。
- 示例配置必须成立：普通平砍 AttackRange = 180cm、突进跳劈 AttackRange = 300cm、AttackSet EngagementRange = 300cm。目标在 250cm 时抽中平砍，敌人接近到 180cm 后攻击；抽中跳劈则可直接攻击。纯短招配置（如 EngagementRange = 250cm，所有招式 AttackRange = 180cm）同样合法成立。
- 不新增 Approach Ability、Null Action、Behavior Tree、Blackboard 或第二条伤害路径。

## 实施切片

### Slice A：Weighted Decision And Controller Approach Primitive

Gemini 可执行的范围：

- 修改 UEnemyAttackSet::SelectAttackProfile()：只验证目标是否位于 EngagementRange，在有效范围内让全部有效 Entry 参与权重选择，并保留距离、随机值、Set 配置和权重的 fail-closed 校验。
- 为 UEnemyAIProfile 增加 ApproachTimeout、Getter、作者化字段和有限值校验。
- 为 AEnemyAIController 增加待攻击 Profile 的运行时快照、Profile 距离查询、一次性 Prepare/Select API、动态 TargetActor Approach MoveTo、超时/导航失败/request ID 保护，以及目标丢失、死亡、眩晕、HitReact、Leash、UnPossess、Teardown 清理。
- 待攻击 Profile 只引用当前 AttackSet 中的不可变 DataAsset，不复制可变战斗数据，不在 Controller 中保存伤害、Montage 或 GAS 状态。
- Approach 同一时刻只允许一个活动 MoveTo；目标位置变化时由 Controller 维护动态目标，不允许 StateTree 每帧重复发起请求。

Main-only 集成范围：

- UEnemyMeleeAbility::ActivateAbility() 不再自行调用 FMath::FRand() 或 SelectAttackProfile()。
- Ability 消费 Controller 已准备的 Profile，并在启动前验证 Profile 仍属于当前 AttackSet、配置有效、当前目标有效且当前距离已进入该 Profile 的 AttackRange。
- Ability 激活失败时必须清除待攻击决策并在 Commit 前结束，不得留下旧 Profile。
- 成功取得 Profile 后由 Ability 保存本次攻击快照，继续使用现有 Montage、Trace、Damage GE、Guard Stamina、Poise、Cooldown 和统一 EndAbility() 清理流程。
- TryRequestMeleeAttack() 只允许在待攻击 Profile 的 AttackRange 内请求 GAS，不再只检查 EngagementRange。

### Slice B：StateTree Integration

新增最小的原生 StateTree 类型：

- Enemy Prepare Melee Attack
- Enemy Approach Selected Melee Attack
- Enemy Has Pending Melee Attack
- Enemy Pending Melee Attack In Range
- Enemy Pending Melee Attack Out Of Range

保持现有 FEnemyStateTreeTask_RequestMeleeAttack 的结构、基类和实例数据布局不变；需要不同生命周期时新增任务类型，不改旧任务序列化契约。

用户在 Editor 中将 Goblin Combat 拓扑调整为：

~~~
Decision -> Approach -> Attack -> Reposition -> Wait
~~~

行为约束：

- Decision 负责一次 Prepare。
- 已进入 Profile AttackRange 时转 Attack。
- 超出 Profile AttackRange 时转 Approach。
- Approach 成功后转 Attack。
- Approach 失败/超时但目标仍在 EngagementRange 内时转 Wait，等待 0.5s 后重新 Decision。
- 目标离开 EngagementRange 时转 Chase/Alert。
- 攻击结束后保留现有冷却重定位流程。
- Wait = 0.5s 仍只是 StateTree 轮询间隔，不能替代 Controller 自己的冷却、请求 ID 和超时状态。

## 允许与禁止修改

允许修改：

- E:/GameDevelop/PolyQuest/Source/PolyQuest/Public/Combat/Enemy/EnemyAttackSet.h
- E:/GameDevelop/PolyQuest/Source/PolyQuest/Private/Combat/Enemy/EnemyAttackSet.cpp
- E:/GameDevelop/PolyQuest/Source/PolyQuest/Public/AI/EnemyAIProfile.h
- E:/GameDevelop/PolyQuest/Source/PolyQuest/Private/AI/EnemyAIProfile.cpp
- E:/GameDevelop/PolyQuest/Source/PolyQuest/Public/AI/EnemyAIController.h
- E:/GameDevelop/PolyQuest/Source/PolyQuest/Private/AI/EnemyAIController.cpp
- E:/GameDevelop/PolyQuest/Source/PolyQuest/Public/AI/StateTree/EnemyStateTreeTasks.h
- E:/GameDevelop/PolyQuest/Source/PolyQuest/Private/AI/StateTree/EnemyStateTreeTasks.cpp
- E:/GameDevelop/PolyQuest/Source/PolyQuest/Public/AI/StateTree/EnemyStateTreeConditions.h
- E:/GameDevelop/PolyQuest/Source/PolyQuest/Private/AI/StateTree/EnemyStateTreeConditions.cpp
- 相关 Automation 测试文件。

由用户在 Editor 中作者化：

- ST_Enemy_Goblin_Melee
- DA_EnemyAIProfile_GoblinMelee
- Goblin 的 AttackSet/Profile 参数。

本阶段禁止：

- Gemini 修改 UEnemyMeleeAbility、ASC、GameplayEffect、GameplayTag、输入路由或共享 GAS 生命周期；
- 新增远程敌人、弓箭、法术、Boss、群体协同；
- 引入 Behavior Tree、Blackboard、通用 AI 框架；
- 修改装备系统、近战伤害/Trace 路径或角色移动架构；
- 手动编辑 .uasset、.umap；
- 删除用户-owned Content/** WIP；
- 提交 Git。

## 验证矩阵

自动化：

- PolyQuest.Enemy.AttackSetSelection
  - 目标在 250cm、短招 AttackRange = 180cm 时，短招仍能按权重被选中；
  - 长招和短招都参加权重选择；
  - 目标超过 EngagementRange 时返回空；
  - 纯短招配置（如 EngagementRange = 250cm，所有 Profile AttackRange = 180cm）合法通过校验并能正常加权抽招；
  - 随机边界和无效浮点输入保持安全。
- PolyQuest.Enemy.CombatSpacing
  - ApproachTimeout 默认值有效；
  - 0、负数、NaN、Infinity 被拒绝；
  - 现有冷却重定位、Leash、死亡和请求 ID 保护无回归。
- 保持回归测试通过：PolyQuest.Equipment.TransactionMatrix、PolyQuest.Melee.TraceSourceGeometry。

用户编译与 Editor readback：

- 手动编译 PolyQuestEditor。
- 读取 ST_Enemy_Goblin_Melee 的任务类型、外部 Controller 绑定、完成模式、条件和转换。
- 读取 DA_EnemyAIProfile_GoblinMelee.ApproachTimeout。
- 确认 AttackSet/Profile 的 180cm/300cm/300cm 测试配置。

PIE：

- 目标约 250cm 时抽中短招，敌人主动接近到 180cm 后攻击。
- 抽中长招时不执行多余 Approach。
- Approach 过程中玩家后退但仍在 EngagementRange 内时，敌人继续跟随当前已选 Profile。
- 玩家离开 EngagementRange 时清除决策并 Chase。
- 导航失败/超时后等待 0.5s 并重新抽招。
- 原有攻击 Montage、Trace、Damage、Poise、Guard、Stun、Death、Cooldown Reposition 保持正常。

## 委托记录与交接

~~~
Plan explorers: 0
Implementation executors: 1
Complex Executor: one scoped lifecycle-sensitive Controller/StateTree implementation, sequentially
Main parallel work: none
Reason: Controller、StateTree 和 GAS Ability 共享待攻击 Profile 生命周期，必须顺序实施；Main 保留 GAS/ASC 集成与最终审查。
~~~

Gemini 执行提示词：

~~~
工作目录：E:/GameDevelop/PolyQuest
基线：74ab01c
先读取当前 plan.md，并使用 ue-stage-workflow、ue5-state-tree-ai、ue5-cpp-gameplay、ue5-debug-validation。

按 Slice A -> 用户编译/Automation/readback -> Main 集成 Ability -> Slice B Editor 作者化 -> 用户 PIE 的顺序执行。
严格遵守 EngagementRange 是 Combat 外层距离；AttackSet 在 EngagementRange 内对全部有效 Profile 加权选择；选中的 Profile 在 Approach 中保持不变。
不要在 SelectAttackProfile 中按 AttackRange 过滤，不要在 Approach 中重新抽招。
Approach 必须使用当前 TargetActor，最终用 2D Actor-center 距离小于等于 SelectedProfile.AttackRange 判定可攻击；超出 EngagementRange、目标无效、导航失败或 ApproachTimeout 时清理待攻击 Profile。
Wait = 0.5s 是 StateTree 轮询间隔，不得替代 Controller 的请求状态和超时逻辑。

允许修改仅限计划列出的 EnemyAttackSet、EnemyAIProfile、EnemyAIController、EnemyStateTree 原生类型和 Automation 测试。
不要修改 UEnemyMeleeAbility、ASC、GameplayEffect、GameplayTag、输入、装备系统、Content 资产或文档；不要手动编辑 .uasset/.umap；不要提交。
完成后执行严格自审，只报告修改路径、静态检查、测试结果、Editor/PIE 缺口和停止原因。
~~~

## 文档与提交边界

- 本阶段完成用户编译、Automation、Editor readback、PIE、Gemini 严格自审和 Main fresh/adversarial review 后，才更新 E:/GameDevelop/PolyQuest/ARCHITECTURE.md、E:/GameDevelop/PolyQuest/ROADMAP.md 和本文件的 closeout 记录。
- ROADMAP.md 记录 TODO-03AI2 的完成结果及仍未关闭的风险。
- 最终提交只包含批准的 Source、测试和文档路径，排除当前所有用户-owned Content/**、.uproject、Config WIP 及无关删除。
- 未经用户明确提交批准，不执行 Git commit。

## 收尾记录 (Closeout Summary)

- **核心交付：**
  - `UEnemyAttackSet`：实现 `EngagementRange` 内全 Profile 纯加权抽取；移除了纯短招限制门禁 `bHasReachableProfile`。
  - `UEnemyAIProfile`：增加 `ApproachTimeout`（默认 3.0s）及有限正数校验。
  - `AEnemyAIController`：实现 `PendingAttackProfile` 弱指针快照管理；动态 `TargetActor` Approach MoveTo；超时、导航失败、死亡、眩晕、受击反应、超出 Leash 及超出 `EngagementRange` 的全生命周期清理。
  - `UEnemyMeleeAbility`：消费 Controller 决策并在 Commit 前校验有效性；`EndAbility` 统一、无条件清理 Controller 待攻击决策。
  - `EnemyStateTreeTasks / Conditions`：支持 `Decision -> Approach -> Attack -> Reposition -> Wait (0.5s)` 拓扑。
- **自动化测试验证：**
  - `PolyQuest.Enemy.AttackSetSelection`：Success
  - `PolyQuest.Enemy.CombatSpacing`：Success（含 4.7 GAS 失败清理、4.8 Ability 未启动退出清理、4.9 超出 EngagementRange 判负、4.10 冷却期间 Prepare/StateTree fail-closed）
  - `PolyQuest.Equipment.TransactionMatrix`：Success
  - `PolyQuest.Melee.TraceSourceGeometry`：Success
- **复核结论：**
  - Gemini 完成严格执行者自审；Main 完成 normal review 与 adversarial fallback，冷却门禁修复后未发现 P0-P2 源码问题。
  - Luna/gpt-5.6-luna 因服务 HTTP 503 未启动，因此不宣称存在独立 Reviewer 结果。
