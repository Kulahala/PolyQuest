# TODO-07B8-B：敌人 Montage RateWindow 扩展与窗口身份补修

## 1. Target Objective、基线与路线

让近战、大受击、小受击、击飞和处决非致死恢复五类 Enemy Ability 支持 RateWindow，并修复 Section 分界吸附时 B Begin 先于 A End 导致 LIFO 恢复错误的问题。最终规则为按窗口身份维护活跃集合，最后开始且仍活跃的窗口生效。交付能力接入、乱序退出安全与代表性验证；最终动画节奏由用户调整。

- 项目：E:\GameDevelop\PolyQuest；模块：PolyQuest；引擎源码唯一基准：D:\UE\UE_5.8。
- 规划基线 HEAD：7f245f55ddf805086d1695b3536593479919d059。Source 在规划检查时无未提交差异；Content/Config 有既有 WIP，ROADMAP.md 有既有文档修改，全部保留。Executor 实施前重新记录 HEAD、状态和批准路径 diff，不覆盖后来出现的用户改动。
- 前阶段 TODO-05A1-F 已完成归档；完整旧 plan 可从上述提交读取。当前文件自 2026-09-12 起为 TODO-07B8-B 的交接依据。
- 状态（2026-09-12）：五类 Enemy 接入与窗口身份补修已完成；用户已确认补修版 PIE/Automation 通过，最终 EnemyMontageRateWindow Test Run 3 为 Success。Main Fresh Review 的两项 P2 测试缺口经单文件补测、用户运行和最终 delta review 闭环，未发现新的 P0-P2 blocker。源码、专项 Automation、用户 PIE 与 Review 收口，文档已归档，用户已批准本阶段源码/文档提交，哈希以 Git 历史为准。独立 authored readback/编译日志收据边界统一见 ROADMAP.md 的 Debt-07B8-AuthoredValidation；完整证据分层见第 9 节。
- Outer: ue-stage-workflow
- Primary: ue5-cpp-gameplay
- Support: none；Main 终审使用 ue-strict-review。
- Route reason：五类 Native GAS Ability 的局部 Montage 播放率监听和生命周期接入。
- Execution route: manual/out-of-band Gemini。Main 负责计划、范围、终审和文档；Gemini 负责批准代码与测试；用户负责 Editor、手动编译、PIE 和提交批准。不通过 Codex in-app task 派发实施。
- 本轮为已交付实现的补修，Gemini 当前会话单兵自审，不再派发子代理。

Deliberate Non-goals：Player 迁移、07B8-C 统一重构、Character 全局监听、Time Dilation、平行动作状态、Tag/Config 修改、位移补偿、第二伤害路径、处决协议调整、资产写入与 Git 提交。

## 2. Approved Paths（封闭白名单）

以下均相对项目根目录。允许修改的生产符号限于五类 Ability 的 RateWindow 监听、绑定、激活代次/实例防护、清理，以及必要测试接口；不顺手重构原选择、伤害、移动和处决逻辑。

1. Source/PolyQuest/Public/AbilitySystem/Abilities/EnemyMeleeAbility.h
2. Source/PolyQuest/Private/AbilitySystem/Abilities/EnemyMeleeAbility.cpp
3. Source/PolyQuest/Public/AbilitySystem/Abilities/EnemyHitReactionAbility.h
4. Source/PolyQuest/Private/AbilitySystem/Abilities/EnemyHitReactionAbility.cpp
5. Source/PolyQuest/Public/AbilitySystem/Abilities/EnemySmallHitReactionAbility.h
6. Source/PolyQuest/Private/AbilitySystem/Abilities/EnemySmallHitReactionAbility.cpp
7. Source/PolyQuest/Public/AbilitySystem/Abilities/EnemyLaunchReactionAbility.h
8. Source/PolyQuest/Private/AbilitySystem/Abilities/EnemyLaunchReactionAbility.cpp
9. Source/PolyQuest/Public/AbilitySystem/Abilities/EnemyVictimExecutionAbility.h
10. Source/PolyQuest/Private/AbilitySystem/Abilities/EnemyVictimExecutionAbility.cpp
11. Source/PolyQuest/Private/Tests/EnemyMontageRateWindowAutomationTests.cpp（首次交付已新增，继续补测）
12. Source/PolyQuest/Public/AbilitySystem/Abilities/MontageRateWindowLifecycle.h
13. Source/PolyQuest/Private/AbilitySystem/Abilities/MontageRateWindowLifecycle.cpp
14. Source/PolyQuest/Public/Animation/Combat/AnimNotifyState_ActionWindows.h
15. Source/PolyQuest/Private/Animation/Combat/AnimNotifyState_ActionWindows.cpp
16. Source/PolyQuest/Private/Tests/EnemyStanceBreakRateWindowAutomationTests.cpp

用户已批准本次共享基础设施补修：路径 12/13 仅修改 FAbilityMontageRateWindowLifecycle 的窗口身份、活跃集合与必要测试接口；路径 14/15 仅修改 UAnimNotifyState_MontageRateWindow 的载荷/注释，以及其发送辅助函数的必要可选参数，其他通知的行为和载荷不得改变；路径 16 仅适配身份协议并回归 StanceBreak。原 10 个 Ability 文件只允许接入兼容、清理和真实测试入口所需窄改，不重做已有接入。StanceBreak 生产文件、Player、其他测试、Config、Build.cs 和资产仍只读。不得扩大到通用 AbilityTask/基类框架。

Main 文档路径：plan.md、ROADMAP.md、ROADMAP-archive.md；ARCHITECTURE.md 仅在验证和终审后更新。Executor 不修改这些文档。所有 Content/** 与其他 WIP 不暂存、不回滚、不归因于本片。

## 3. 冻结的 Runtime Contracts

### 3.1 播放率、事件与所有权

- RateMultiplier 按当前源码直接作为 Montage_SetPlayRate 的目标播放率，不乘 baseline，也不与外层窗口连乘。
- 播放成功后捕获实际 baseline；Begin 按窗口身份加入活跃集合，最后开始且仍活跃的窗口提供目标速率；End 只移除匹配身份，重新选择最后活跃窗口，无窗口才恢复 baseline。支持顺序、严格嵌套、不同窗口交叉重叠及边界交接两种到达顺序；不再盲目 LIFO 出栈，也不采用纯计数归零策略。详细载荷和去重规则见第 8 节。
- 复用 Event.Action.RateWindow.Begin/End；Instigator、Target 必须均为本 Avatar，OptionalObject 必须匹配当前 Montage 或其包含的 Sequence。拒绝非法数值、错误 Tag/来源、过期回调和非活跃状态。
- 无窗口 Montage 保持原行为。GAS/ASC 保持唯一状态权威，不新增 Tag、网络能力或状态机。
- Notify 窗口身份由共享 helper 管理；Montage 实例与激活代次隔离继续由现有消费者负责，不把二者混作同一种身份。本轮无需新增作者参数或改变现有 helper 生产调用入口签名。

| 消费者 | 接入对象与保留契约 |
|---|---|
| UEnemyMeleeAbility | 本次 Attack Profile 选择的 Montage；保留 Trace、霸体、命中去重和伤害权属。调速可改变窗口真实持续时间，不增加伤害入口。 |
| UEnemyHitReactionAbility | 本次选中的大受击方向 Montage；保留方向选择、优先级与移动恢复。 |
| UEnemySmallHitReactionAbility | 本次选中的小受击方向 Montage；保留叠加表现和 Retrigger，不增加移动锁。 |
| UEnemyLaunchReactionAbility | Root Motion Knockdown Montage；保留 Root Motion-only、CMC、离地中断及原完成时机。 |
| UEnemyVictimExecutionAbility | 正面/背刺的非致死恢复 Montage；保留 Hit、VictimStart、死亡提交、Player 恢复与 Lock-On 协议。 |

### 3.2 四类常规消费者接入

- 每类拥有独立 lifecycle、Begin/End 任务及仅服务新增事件的瞬态 Context。Context 保存弱 Ability 引用与激活 token；转发前核验代次、对象和结束状态。
- Context 只代理 RateWindow Begin/End，不迁移既有 Montage 结束、Trace、HyperArmor 或处决复杂委托，不抽取新公共框架。
- 准备监听后启动 Montage；播放成功、任务有效且通过重入检查后捕获 baseline。绑定前事件直接忽略、不缓存；时间零点窗口不在本片保证范围。
- 每次 ReadyForActivation() 返回后核验当前 token、任务身份和激活状态；失败统一进入原 EndAbility()，不得恢复旧 Task 或激活状态。
- 保存本次绑定 Montage instance ID；事件和恢复前确认仍是本次实例，避免同资产后续播放被旧清理污染。

### 3.3 Victim 特殊时序

- ActivateAbility() 锁定阶段不创建 RateWindow 监听；致死分支直接绕过绑定。
- 仅在 OnVictimStartReceived 非致死分支中，恢复 Montage 启动成功、bNonLethalRecoveryActive 成立且实例 ID 确定之后，创建 Context/监听任务并捕获 baseline。
- 每个任务激活返回点继续核验恢复状态、激活代次与实例；失败走既有终止路径。
- Ability 在转发 Begin/End 前比对 AnimInstance 实际活跃实例与 ActiveVictimMontageInstanceID；抽刀前、致死和启动取消期间均不应用调速。
- 保留自然 BlendOut 不结束恢复、真正 Completed 才正常收口的契约。不得延长抽刀前锁定或改变结算时点。

### 3.4 清理、Retrigger 与实例隔离

- 先使 Context 失效并解除新增监听，再恢复本次仍拥有的实例速率，随后执行原有 Montage/任务清理。
- 实例失效或被替换时，只重置本地 lifecycle，不调用可能命中新实例的按资产恢复接口；继续保留 lifecycle 值初始化清空的安全退出行为；新活跃集合必须同样被清空。
- UE 外部 Cancel 可先广播至 MontageTask 并停播，再进入 Ability 清理；已停止实例不满足活跃实例恢复条件，此路径验证停止、监听/集合清空及后续播放无污染。正常 Retrigger 的旧 EndAbility 则在停播前恢复仍活跃的旧实例 baseline；两种时序不得共用“必定恢复已停止实例数值”的测试断言。
- Victim 的 RateWindow 清理置于 StopVictimMontagePresentation() 清空 Montage/instance ID 之前，供既有退出路径复用，保持幂等。
- UE 5.8 的 bRetriggerInstancedAbility 路径先执行旧 EndAbility()，再重新激活：Engine/Plugins/Runtime/GameplayAbilities/Source/GameplayAbilities/Private/AbilitySystemComponent_Abilities.cpp:1836-1844。禁止沿用“Retrigger 跳过 EndAbility”的错误前提。
- 小受击正常 Retrigger 依赖旧 EndAbility 清理；ActivateAbility 既有残留清理块复用同一幂等 RateWindow 清理方法，必须在覆盖旧 Montage/上下文之前执行。不建立第二套恢复逻辑。

## 4. 测试接口与 Automation

新增测试套件前缀：PolyQuest.Combat.EnemyMontageRateWindow。测试经过真实 ASC 激活、事件派发和消费者清理入口，不能仅手动注入 helper 状态或断言集合大小。

必须覆盖：
- 五类消费者 Begin/End 接线、非 1 baseline、顺序/嵌套/交叉恢复、边界交接两种顺序、非法倍率和错误来源；身份专项矩阵见第 8 节。
- 播放失败、任务同步结束、重复清理、旧 Context 回调、同 Montage 资产新实例隔离。
- 自然完成、中断、取消、死亡、销毁以及适用的 UnPossess；不引入速率、监听或状态残留，不修改既有取消 Tag。
- 小受击真实 ASC Retrigger：旧窗口生效时再次触发，旧激活先结束，新激活建立独立 baseline；同方向与切换方向都覆盖，旧回调不能影响新播放。
- Victim：抽刀前不监听、致死不绑定、非致死恢复后生效、BlendOut 不提前结束、启动取消与实例替换安全。

测试访问接口限定 WITH_DEV_AUTOMATION_TESTS，可提供只读 lifecycle/Context 状态及必要事件入口。仅确有需要的 fixture 使用 bypass，不要求五类统一增加接口。已有 Launch/Victim bypass 若用于新测试，必须与 helper 同步并在清理重置。bypass 只证明逻辑/接线，不证明真实播放、实例隔离或 Root Motion。实际播放率和实例隔离须由有效动画实例测试或用户 PIE 证明，不能靠手动设置 Active 状态冒充。

## 5. 用户 Editor 操作与 Readback

本计划不授权 Agent 写入或暂存任何 uasset/umap。能力接入可先实施；实际作者资产名单属于后续用户验证记录，未完成 readback 不能标记阶段完成。

1. 从当前 Enemy 的真实配置记录：一个近战 Attack Profile Montage、大/小受击各一个方向 Montage、一个 Root Motion Knockdown Montage，以及正面/背刺实际恢复 Montage；同资产复用时仍分别验证入口。不凭磁盘文件名推断引用。
2. 在 Montage 上放入口之后、结束之前的 RateWindow；优先起手准备或后摇恢复段，避免修改共享 Sequence 扩大影响。
3. 使用 0.5、1.5 作为常规验证值；边界用 A=0.5、B=0.2 便于区分，均非最终美术参数。记录消费者引用、完整资产路径、窗口起止、倍率、Slot、Montage Tick Type、触发偏移与适用的 Root Motion 配置。将两个不同 RateWindow 的 A End/B Begin 吸附同一 Section 分界，实测 B 段持续采用 B 速率。
4. 若窗口覆盖高速 Root Motion 位移段，追加下坡、台阶、边缘 PIE；不预设总位移、碰撞结果不变，检查异常离地中断和穿模。
5. 其余受击方向验证选择无回归；代表窗口之外、打断后下一次播放及多敌人之间均不得残留或串扰。

## 6. 验证矩阵与完成态

| 门禁 | 必需证据 / 所有者 |
|---|---|
| 静态 | Gemini：白名单 diff、git diff --check、适用且可用的 Rider 诊断；不可用记录 coverage fallback；异步与重入自审。 |
| 编译 | 用户执行 UE 5.8 PolyQuestEditor 编译并确认结果；未另外授权时 Executor 不自行启动编译。 |
| Automation | 新增专项；既有 EnemyStanceBreakRateWindow、EnemyLaunchReactionRootMotion、ExecutionVictimPresentation、ExecutionVictimRootMotion、ExecutionReleaseOutcomes、ExecutionImpactFeedback、Player.LockOn 与受影响近战/受击套件。既有测试除白名单第 16 项外只读；追加 PlayerActionWindow 回归以核对共享 Notify 载荷兼容。执行条件不可用时明确列待跑。 |
| Readback | 用户提供第 5 节实际引用和窗口配置；Agent 不假称已查询 Editor。 |
| Scene01 PIE | 用户：补修版 Section 分界吸附、交叉/嵌套恢复；StanceBreak 与五类消费者代表性调速；小受击快速连击、打断重播、多敌人隔离、无窗口基线和既有 Player 回归。 |
| 专项行为 | 近战无重复伤害；Launch 坡面/台阶/离地；Front/Backstab 非致死恢复、致死结算及 Player/Lock-On 无回归。 |
| Main Fresh Review | ue-strict-review 在批准 diff 内缺陷优先复核；不扩大到 C 或无关历史代码。 |

五类专项验证、用户编译、代表性 readback/PIE 和 Main 终审完成才可关闭 B。未经执行不得写通过；接受延期必须在 ROADMAP.md 有唯一记录与闭环条件。既有 A 的历史验证债务不由本片静态检查自动关闭。

## 7. Executor 交付与停止条件

- 以当前首次交付 diff 为起点做增量补修，不回滚重写：先建立乱序退出失败回归，再补窗口身份载荷和 helper 活跃集合，适配两个白名单测试文件，补齐真实 ASC Retrigger 与通知派发验证，最后统一交付。
- 先读真实 AGENTS.md、本 plan、目标 diff 和直接依赖；CodeGraph 有界导航，覆盖不足时明确回退，不全库漫游。常规算法、私有辅助与测试设计在白名单内自主闭环。
- 本次修复当前会话自审，不派发子代理。附本轮新增差异清单、协议兼容说明、静态/Automation 实际结果及用户待跑项，区分旧版证据与新修复结果。
- 共享 helper/Notify 仅按第 2、8 节批准范围修改；不改资产、Tag、Config、Build.cs、文档或白名单外测试，不暂存、不提交、不启动 C。需要进一步越界时给出具体阻塞证据，由 Main 决定。
- 同一根因保留首次错误，最多一次有依据修复和一次定向复测；再次失败则报告，不循环试错。
- Main 验收后更新稳定架构、归档及路线图，B 仅为 C 提供实际重复点与语义差异。用户现已批准 Main 按第 9 节范围提交；Executor 仍不得暂存或提交。

## 8. 本轮补修：Section 边界交接与窗口身份（2026-09-12）

### 8.1 问题与证据边界

- 首次 helper 保存匿名速率栈：A Begin(0.5) → B Begin(0.2) → A End 会弹出 A 的旧速率并覆盖仍活跃的 B。本问题不应推迟到 C 才解决；B 在补修和终审完成前不收口。
- UE 5.8 AnimInstance.cpp 的 Queued 派发先处理本批退出 End，再处理新 Begin；不能笼统宣称所有同帧事件无序，但也不能用该局部顺序保证所有资产的相邻窗口无交叠。
- AnimMontage.cpp 的 AddBranchingPointMarker 明确警告相同实际 TriggerTime 的 Branching Point 可能漏触发。此次算法解决已收到事件的配对和顺序问题，不补回引擎未发送的通知。
- 本片支持不同作者化通知项 A/B；同一个 Sequence 的同一个 Notify 对象被同一 Montage 实例并行重复使用的多次出现不属于本片支持范围，不为其引入 occurrence 框架。代表性测试和用户资产采用 Montage 上两个独立窗口。

### 8.2 事件身份与兼容契约

- 保留现有 Tag、Instigator、Target、OptionalObject（来源 Montage/Sequence）以及 Begin 的 EventMagnitude。
- RateWindow Begin/End 均设置 OptionalObject2 = this，即该作者化 UAnimNotifyState_MontageRateWindow 对象。UE 5.8 FAnimNotifyEvent::NotifyStateClass 是 Instanced UObject，足以区分本片两个独立作者窗口；身份仅作只读标识。
- 不使用 EventReference.GetNotify() 的裸地址作持久 key：引擎会复制/重设通知事件引用；不使用 WITH_EDITORONLY_DATA 内的 Notify Guid；不在共享 Notify 对象保存每个 Actor 的活动状态，不为每次回调创建身份 UObject。
- helper 的 key 包含来源动画与 Notify 对象身份，状态局限于当前绑定 lifecycle。继续校验来源属于当前 Montage；身份须为有效 RateWindow Notify，且属于声明来源的 Notifies 项。Begin/End 缺失身份、错误类型或身份/来源不匹配均忽略，不能退回匿名栈兼容。
- 原生 RateWindow Notify 自动补充身份，不新增 UPROPERTY 作者字段、不要求批量重存资产。现有 Player 消费者继续使用原载荷字段且忽略 OptionalObject2，保持原行为；本片不把 Player 重叠策略同步改掉。
- 既有手工构造事件的两个白名单测试文件必须补真实匹配身份。不按倍率、Notify 类名或动画资产单独配对；A/B 相同倍率也必须是不同窗口。

### 8.3 活跃窗口选择

- 用有序活跃项集合替换匿名 RateStack，每项保存弱来源/Notify 身份及 Begin 时快照的正有限倍率；条目顺序就是有效 Begin 的接收顺序。
- 首次合法 Begin 加入尾部并生效；相同活跃 key 的重复 Begin 直接忽略，不增加计数、不更新优先级、不替换快照。
- End 精确移除对应 key，保留其他条目原顺序，应用最后一个仍有效条目的倍率；集合为空恢复本次捕获 baseline。未知/重复 End、被拒绝 Begin 对应的 End 不改变其他窗口。
- 两种边界顺序在交接处理结束后都由 B 生效：A End → B Begin 可先恢复 baseline 再进入 B；B Begin → A End 必须持续保留 B。不得用帧末延迟任务、固定时间容差或 Timeline 微移来掩盖配对错误。
- 内层 B 先结束时回到仍活跃的 A；外层 A 先结束时保留 B。多个窗口同时 Begin 时以实际合法接收顺序为准，不增加作者优先级参数。
- RestoreAndClear/实例失效/重复清理须清空全部活跃项。baseline、ReadyForActivation 重入边界、消费者实例校验和 Victim 延迟监听契约保持第 3 节要求。
- TestApplyBegin/End 等匿名测试入口改成身份明确的测试入口，或移除并改测实际 HandleBegin/End；测试状态命名使用 ActiveWindowCount 等准确语义，不保留只为旧断言服务的假 LIFO。

### 8.4 补修验证矩阵

| 场景 | 必须断言 |
|---|---|
| A Begin → A End → B Begin → B End | 0.5 → baseline → 0.2 → baseline。 |
| A Begin → B Begin → A End → B End | 0.5 → 0.2 → 0.2 → baseline；同时覆盖同次派发和分次派发。 |
| A Begin → B Begin → B End → A End | 内层结束回 0.5，最终回非 1 baseline。 |
| 三窗口交叉/相同倍率不同身份 | 删除任意非末尾窗口不误删当前赢家，身份与倍率解耦。 |
| 重复 Begin/End、未知 End、无身份、来源不匹配、非法倍率 | 集合和当前赢家不被污染，合法后续事件仍可工作。 |
| Notify 到 ASC 的真实传输 | 实际调用作者化 RateWindow Notify Begin/End，经 ASC/已绑定任务抵达消费者，验证身份一致、实际 Montage_GetPlayRate 及清理；不只直接调用 helper。 |
| 实际时间轴 Section 吸附 | 至少一个有效 Montage/AnimInstance 经动画更新或用户 PIE 产生真实边界通知；覆盖不同步长及 Section 跳转，不把手工事件序列冒充引擎派发。跳转退出窗口必须清理，不猜测丢失事件。 |
| 小受击 Retrigger | 在真实 ASC 激活和再次触发路径观察旧 EndAbility → 新 ActivateAbility；覆盖同方向/切向，新实例 baseline 独立，旧 Context 无效。手动 SetTestAbilityActive/token 只作独立负向测试。 |
| 集成回归 | StanceBreak 与五类 Enemy；非致死/致死 Victim 时序；既有 PlayerActionWindow 和 LightAttack 代表性 PIE；死亡/取消/UnPossess/同资产新实例不残留。 |

先增加能暴露旧匿名栈错误的回归；无法运行时如实说明“已补失败场景，未执行”，不得声称已见红。已有测试中纯 bypass、手动 token 与只看集合大小的用例可以保留为逻辑层测试，但须补足上述真实链路，不能再称为真实 Retrigger/播放率证明。

用户 readback 先确认实际 RateWindow 的 Montage Tick Type。Section 吸附验收采用 Queued；若实际为相同 TriggerTime 的 Branching Point，说明引擎限制并由用户调整该资产后验证，Agent 不改资产。不承诺在通知丢失时算法仍能还原作者意图。

### 8.5 交付和关闭条件

- 本节窗口身份缺陷已完成补修版用户 PIE/Automation 和 Main Fresh Review；最终真实 ASC Retrigger 与 Notify 消费测试结果见第 9 节，不能用首次实现的收据替代。
- Gemini 本轮不改文档、不派子代理、不暂存/提交。报告逐项标出真实调用链与 seam/bypass，给出新增白名单路径和仍待用户运行项。
- 原 B/C 窗口身份 blocker 已关闭并归档；独立 authored 验证收据仅在 ROADMAP.md 的 Debt-07B8-AuthoredValidation 记录。C 后续只承接 Player/Enemy 接入统一，不重复修复已在 B 关闭的窗口选择算法。

## 9. 最终验收与文档收口（2026-09-12）

- **交付边界**：第 2 节 16 个 Source/Test 路径。Gemini 完成五类接入、共享 helper 身份活跃集合、Notify OptionalObject2 载荷与 StanceBreak 测试适配；Main 按 AGENTS.md 单文件窄改动例外，仅补修 EnemyMontageRateWindowAutomationTests.cpp。无 Player、Tag、Config、Build.cs 或资产修改。
- **Review 闭环**：首次 Fresh Review 的两项 P2 为“手动 token 测试未覆盖真实 Retrigger”和“Notify 只到 ASC mailbox，未验证生产消费者/实际播放率”。现在有效临时 Montage 与 AnimInstance 经真实 ASC 授予、事件激活和重触发，覆盖同向/切向的新实例、旧 End → 新 Activate、基线恢复、Context 失效、任务/监听清理；真实 Notify 回调通过生产 WaitGameplayEvent/Context/helper 检查实际播放率、交叉/嵌套恢复及取消后的独立播放。
- **测试边界**：旧 seam/bypass 用例明确保留为逻辑层测试。新增 Notify 回调由测试按顺序调用，不声称覆盖引擎时间轴调度；实际 Section 分界表现沿用用户对补修版 PIE 的确认。瞬时 ASC 重触发不自动关闭 07B5 的 Player/跨帧完整链路债务。
- **夹具修正**：复制 Montage 后重设时长；按命名 Notify 查找 A/B，避免数组排序影响身份；移除未导出的 HasValidSlotSetup 调用；按 UE 外部 Cancel 先停播的时序检查清理。未为通过测试修改生产逻辑或绕过 Montage 活跃检查。

| 证据 | 最终记录与边界 |
|---|---|
| 用户运行 | 最新 EnemyMontageRateWindow Test Run 3 为 Success，日志包含 initial front、same front、switch right 三段。此前用户明确确认身份补修后的 PIE 和 Automation 通过；不是只沿用首版结果。 |
| 执行者报告 | 身份补修报告列出 EnemyStanceBreakRateWindow、ExecutionVictimPresentation、专项 Automation 和 Scene01 PIE 成功；完整报告为附件 0db4e41d-918d-4579-aaec-44e6c04cd55a/pasted-text.txt，归为执行者交付证据，不宣称 Main 运行。 |
| 静态 | Main 对最终测试文件 Rider errors 为 0，Source git diff --check 与未跟踪测试空白检查通过。Rider 不证明链接或运行；曾出现的未导出符号调用已移除，后续用户成功运行最终测试版本。 |
| 最终复核 | Main 有界 delta review 无新的 P0/P1/P2。此前完整生产变更审查结论保留；本轮只有测试补修，无共享生产契约变化，CodeGraph/影响图重复查询 skipped。 |
| 收据限制 | 未独立归档本阶段 Development Editor 编译日志及五类 Montage/Notify Tick Type/窗口配置 readback；不宣称 Agent 查询过 Editor、不宣称全量回归在本轮全部重跑，也不宣称干净检出即可复现用户资产。唯一后续触发见 ROADMAP.md。 |

ARCHITECTURE.md 记录当前 Enemy 身份协议及与 Player 的现存差异；ROADMAP-archive.md 追加历史收口；ROADMAP.md 下一步指向 TODO-07B8-C，随后为用户批准新增的 TODO-03H6 健康审查、必要有界修复和 TODO-03C。保留本 plan 作为最近阶段凭据，C 正式规划前不覆盖。用户已批准本阶段 16 个 Source/Test 文件与四份阶段文档共 20 路径提交，包含此次路线更新；提交哈希以 Git 历史为准。Content/Config 与其他 WIP 不暂存、不回滚。
