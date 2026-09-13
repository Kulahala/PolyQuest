# TODO-03H9-B：CancelWindow 托管与显式取消策略（调整版）

## 1. 目标、基线与边界

本计划完整替代上一版对话计划。

扩展既有 `UAbilityTask_PlayActionMontage`，统一托管取消窗口的监听、来源校验、重叠集合、Tag 贡献和退出清理；迁移 SprintAttack、ChargedAttack 两个试点，保留现有 GAS 激活、消耗与取消权属。

**已冻结的调整：**

- 重叠 CancelWindow **取并集**，前一窗口结束不能关闭仍有效的后一窗口。
- 策略精简为 `None / DodgeOnly / DodgeAndDefense`，默认 `None`。
- 删除运行时跨 Ability 反射预审；配置诊断放入独立 Automation/验收入口。
- 保留窗口来源归属和重入安全。
- 暂停 End 区分本窗口的结束原因：Queued 使用原生标记并核验事件采样是否到达自身边界，Branching Point 使用独立 Payload；不无条件丢弃或恢复即关全部窗口。
- Executor 白名单缩为 **13 个文件**，不修改 EnemySmallHitReaction。

**基线与证据：**

- 仓库：`E:\GameDevelop\PolyQuest`。
- HEAD：`c2aefff7e4f875bb8569feefe19cdfb3a3d3568c`。
- 引擎：`D:\UE\UE_5.8`。
- 已核对真实 AGENTS、ROADMAP、目标及直接依赖源码、相关测试、Tag 配置和 UE Notify 实现；落盘前源码及阶段文档无未提交改动，保留全部既有 Content/Config WIP。
- UE 5.8 的 Queued 路径提供 `FAnimNotifyEndDataContext::bReachedEnd`，Branching Point Payload 提供 `bReachedEnd`。这是本次冻结暂停处理的**源码依据**，尚无新增编译或运行验证。
- A 的固定交接为 `git show 08f4214:plan.md`。B 可完成规划；实施启动前仍按 ROADMAP 对账 `Debt-03H9-A-ValidationReceipts`，不得自行核销前置验收债。

**非目标：**不实施 C 的全量迁移或 03C；不新增 GameplayTag、Config、Build.cs 依赖、Ability 基类或转换矩阵；不改伤害、消耗时点、Motion Warping、防御恢复或资产。

**状态（2026-09-13）：实现、修复与 Main 复核完成；用户确认最终专项 Automation、PIE 通过并授权文档收尾及提交。** B 剩余验收收据统一见 ROADMAP 的 Debt-03H9-B-RepairValidation，不宣称全矩阵或父阶段完成。

## 2. 实现与运行时契约

### 2.1 标准入口与策略

在 Task 头文件声明：

```cpp
UENUM(BlueprintType)
enum class EActionMontageCancelPolicy : uint8
{
    None,
    DodgeOnly,
    DodgeAndDefense
};
```

工厂末尾追加默认参数 `CancelPolicy = EActionMontageCancelPolicy::None`，保留其他参数顺序及结果委托。

| 策略 | Task 行为 |
|---|---|
| `None` | 不绑定 CancelWindow 事件；RateWindow 照常工作 |
| `DodgeOnly` | 有效窗口期间贡献一次 `State.Action.CanCancel.Dodge` |
| `DodgeAndDefense` | 有效窗口期间分别贡献一次 Dodge 与 `State.Action.CanCancel.Defense` |

策略在 Task 激活前确定，播放期间不切换。Task 不按具体 Ability 类名分支，不扫描其他 Ability 决定自己能否播放。

SprintAttack、ChargedAttack 使用 `DodgeAndDefense`，并补齐既有 `Ability.Action.CancelableBy.Dodge/Defense` 标记。Big/Launch 不迁移，保留原有 Dodge-only 权限与 Launch 阶段门；EnemySmall 保持原调用。

**保留 `OnFailed`：**宿主无效、真实播放失败、绑定失败等仍走 A 的失败与清理出口。仅删除因跨 Ability 配置预审而拒绝播放的机制。

### 2.2 Notify 来源与并集

保留既有 `UAnimNotifyState_ActionDodgeCancelWindow` 的类名、显示名和事件 Tag：

- `Event.Action.CancelWindow.Dodge.Begin`
- `Event.Action.CancelWindow.Dodge.End`

载荷冻结为：

- `OptionalObject`：来源 Animation。
- `OptionalObject2`：Notify 自身。
- TargetData：复用现有来源结构的 AnimInstance/InstanceID；`bReachedEnd` 表示本窗口核验后的结束结果，`bNativeReachedEnd` 仅保存原始结束标记供诊断，不改变 RateWindow 消费逻辑。

Queued 从事件上下文取得实例身份和采样时间；原生结束标记为真时，再核验对应窗口自身终点，直接动画段按其偏移、裁剪及播放速率转换时间。Branching Point 覆盖 Begin/End，保留自身 Payload 的结束结果。禁止用“当前播放实例”补造缺失的事件来源 ID，或以回调当下的位置替代事件采样时间。旧消费者继续读取既有字段，不要求资产重建。

Task 私有状态采用窗口身份集合，身份为当前绑定内的 **来源动画 + Notify 对象**：

1. 首次合法 Begin 加入集合，重复 Begin 不累加。
2. End 只移除自己的身份；未知、重复 End 无副作用。
3. 空集合变非空时贡献策略 Tag，最后一个窗口移除时才撤销。
4. `Begin A → Begin B → End A` 后，B 必须继续授权。
5. 不合并 RateWindow 的速率覆盖算法，不新增独立窗口框架。

事件仍须检查：精确 Tag、有效且 Active 的 Ability、Avatar、动画归属、Notify 类型及其确实属于来源动画、绑定 AnimInstance/InstanceID，以及当前播放实例授权。旧播放、旧 Task、错误角色或伪造窗口身份全部拒绝。

### 2.3 Charged 暂停与恢复

使用一个暂停锁存状态和一个**待结算自然结束窗口集合**，均属于当前 Task 绑定。

- HoldReady 暂停前开启锁存；只有当前已经存在合法窗口才锁存，不凭暂停产生取消权限。
- 锁存期间收到合法 End：
  - `bReachedEnd == true`：将该窗口记入待结算集合，暂停期间仍保留权限。
  - `bReachedEnd == false`：视为未自然走到窗口终点的退出，不把它记成自然结束；在锁存期间保留窗口。
- 同一窗口重复 End 幂等；重复 Begin 不撤销已记录的待结算自然结束。
- Charged 成功释放、恢复动画前解除锁存：只移除待结算自然结束的窗口，重新按并集计算权限；其余窗口保持授权，等待恢复后的合法 End。
- 解除锁存本身不重建窗口、不重新捕获速率、不代替 `Montage_Resume`。
- 无锁存时，合法 End 正常关闭自己的窗口。
- 释放失败、取消、死亡、播放结束和 EndAbility 清空全部状态，并撤销自身贡献。

这样，暂停产生的提前退出不会误关仍有效的窗口；已自然结束的窗口也不会仅因等待一个不存在的第二次 End 而残留。

**验收必须证明此区分在真实 Queued/Branching Point 派发中成立。** 若实际上下文与引擎静态依据不符，记录首个失败并停止调整该契约，不自行改成另一套暂停规则。

### 2.4 Tag 所有权与退出

- Task 分别记录自己对每个策略 Tag 的贡献，只加一、减一；禁止清零 ASC 总计数，保留外部贡献。
- 先更新集合和贡献所有权，再执行 Add/Remove Tag 等外部调用；返回后核验 Task、Ability 与绑定是否仍有效。
- Add/Remove Tag、ASC 播放、结果广播和 `ReadyForActivation()` 均按同步重入边界处理，清理后不能继续补写旧状态。
- 在既有 `CleanupTask()` 内收敛取消事件解绑、集合/暂停状态清空和自身贡献撤销，再沿用 A 的速率恢复、Root Motion 和实例授权停止。
- 清理幂等。旧 Task 失去播放权后仍撤销自身贡献，但不能停止或修改新实例。

Task 不尝试激活目标、不提前结束原动作。Dodge 保持 Commit 成功后再执行取消；目标 CanActivate 或真实 Commit 失败时，原动作保持运行。Guard/Parry 保留现有流程，不增加事务回滚或新的 Commit 机制。

### 2.5 两个试点迁移

**SprintAttack：**删除专属 CancelWindow WaitTask、回调、布尔副本和 Tag 增减，改用 Task 策略；保留 Trace、Commit、朝向及 Guard/Sprint 收尾顺序。

**ChargedAttack：**改用标准 Task，同时移除重复 RateWindow Context/监听/绑定状态、AnimInstance 全局结束监听和直接 Montage 停止；通过 Task 结果收敛 EndAbility。保留 HoldReady、输入、反馈、Trace 和释放时 Commit，只在暂停/恢复边界调用上述窄接口。

## 3. 配置诊断、白名单与分工

### 独立配置验收

在现有测试支持文件内提供共享检查函数，由专项 Automation 使用，不进入运行时 Task 激活路径。

检查输入包含：待验收 Ability、Montage、实际创建的 Task（可空）、明确的期望策略及目标 Ability 列表。期望策略由测试/验收用例声明，不能从默认 `None` 猜测作者意图。

检查范围：

- 期望启用窗口却没有标准 Task，或实际策略与期望不符。
- 对应可被取消标记缺失。
- 源动作阻止声明、目标 ActivationBlockedTags、提前取消声明与预期取消行为冲突。
- 期望有窗口但 Montage 及直接动画段没有对应 Notify。
- 明确期望 `None` 时识别为主动禁用，不误报。

固定 protected UPROPERTY 的只读反射仅用于该测试检查。输出包含 Ability、Montage、目标与冲突项；不能只为 Sprint/Charged 写专属判断。再用真实 GAS 正反向激活测试验证动态行为，静态诊断不替代它。

### Approved Paths

下列路径均相对 `E:\GameDevelop\PolyQuest`；对应 Public/Private 文件均为精确授权，合计 **13 个文件**：

| 文件 | 授权内容 |
|---|---|
| `Source/PolyQuest/Public/AbilitySystem/Tasks/AbilityTask_PlayActionMontage.h` | 策略、工厂参数、暂停接口、私有状态及必要测试访问器 |
| `Source/PolyQuest/Private/AbilitySystem/Tasks/AbilityTask_PlayActionMontage.cpp` | 取消事件、来源校验、并集、贡献、暂停结算及清理 |
| `Source/PolyQuest/Public/Animation/Combat/AnimNotifyState_ActionWindows.h` | Cancel Notify 覆盖声明、来源数据补充 |
| `Source/PolyQuest/Private/Animation/Combat/AnimNotifyState_ActionWindows.cpp` | Cancel 来源及结束原因透传、必要私有发送辅助 |
| `Source/PolyQuest/Public/AbilitySystem/Abilities/SprintAttackAbility.h` | 取消窗口旧接线移除 |
| `Source/PolyQuest/Private/AbilitySystem/Abilities/SprintAttackAbility.cpp` | 策略及标记声明、取消窗口迁移 |
| `Source/PolyQuest/Public/AbilitySystem/Abilities/ChargedAttackAbility.h` | 标准 Task、旧窗口接线移除及测试访问器 |
| `Source/PolyQuest/Private/AbilitySystem/Abilities/ChargedAttackAbility.cpp` | 两类窗口迁移、暂停适配、播放结果与清理 |
| `Source/PolyQuest/Private/Tests/TestManagedMontageAbility.h` | 测试配置/诊断声明、必要测试类型 |
| `Source/PolyQuest/Private/Tests/TestManagedMontageAbility.cpp` | 诊断与事件 helper、真实 Commit 失败测试支持 |
| `Source/PolyQuest/Private/Tests/ManagedMontageCancelWindowAutomationTests.cpp`（新增） | 专项窗口、暂停、GAS 与配置验收 |
| `Source/PolyQuest/Private/Tests/PlayerActionWindowAutomationTests.cpp` | 替换 Charged 旧接线测试，保留其他覆盖 |
| `Source/PolyQuest/Private/Tests/PlayerMontageRateWindowAutomationTests.cpp` | Charged 转向 Task 验证，保留原行为断言 |

Main 文档范围：`plan.md`、`ROADMAP.md`、`ARCHITECTURE.md`、`ROADMAP-archive.md`。稳定事实仅在验证与终审后更新。本次落盘只修改前两份文档；这些 Main 文档不属于 Executor 源码交付白名单。

```text
Outer: ue-stage-workflow
Primary: ue5-cpp-gameplay
Support: ponytail:ponytail (lite)
Route reason: GAS Montage Task 的取消窗口权属、暂停适配与两个动作消费者迁移
Execution route: manual/out-of-band Gemini
Current implementation executors: 0
```

Gemini 负责批准源码/测试和实施自审；首次交付按 AGENTS 优先使用一个干净只读自审子代理，后续修复不再派发。Main 负责契约、Fresh Review、文档及提交门禁；用户负责编译、Editor、PIE 和最终提交批准。

Ponytail lite：Task 内的集合与少量暂停状态即可，不增加独立生命周期类、配置注册框架或新的运行时扫描器。

## 4. 验证矩阵

新增 `PolyQuest.Combat.ManagedMontageCancelWindow`，使用真实 ASC、可播放的内存动画与实际时间推进；手工事件仅用于非法输入和迟到事件补充断言。

| 验证面 | 必须覆盖 |
|---|---|
| 并集 | 单窗、重叠、相邻窗两种派发顺序、重复 Begin/End、End-before-Begin、End A 不关闭 B |
| 暂停 | 窗口内部暂停、窗口终点暂停；记录原生 `bReachedEnd`；无窗暂停、恢复后的重复 Begin、一个自然结束而另一个仍有效、退出清理 |
| 来源 | 错误 Avatar/动画/Notify/TargetData、缺失或错误 ID、同 Montage 新旧实例、连续激活及旧 Task |
| 重入与贡献 | 第一项 Tag 增加时同步取消、撤销时重入、重复清理；外部 Tag 贡献不被撤销 |
| 策略与诊断 | None、DodgeOnly、DodgeAndDefense；漏接入口、期望不符、标记缺失、阻止冲突均有可定位结果 |
| GAS 正向 | 普通动作窗口外拒绝、窗口内真实 Dodge 成功，并证明源动作被实际取消 |
| GAS 失败 | CanActivate 拒绝；测试 Dodge 子类只在 CommitCheck 阶段失败，证明生产 Activate 确实进入 Commit，而源动作和窗口仍有效 |
| 试点 | Sprint 与 Charged 真实运行；Charged 暂停/释放/失败；两类窗口同时存在时速率正确恢复 |
| 派发模式 | Queued 与 Branching Point 均有真实动画推进证据，不用直接调用 Notify 冒充 |

回归保留现有入口：

- `PolyQuest.Combat.ManagedMontageRateWindow`。
- `PolyQuest.Combat.PlayerMontageRateWindow` 六组及 EnemyMontageRateWindow。
- `PolyQuest.Player.ActionWindows`、PlayerBigHitReactionWindows、PlayerLaunchReactionRootMotion。
- PlayerMeleeMotionWarping、ChargedAttackNiagaraFeedback 对应现有测试组。

**分层门禁：**

- Executor：精确路径 diff、`git diff --check`、适用 Rider 诊断、严格实施自审；工具不足明确记录 fallback。
- 用户编译：UE 5.8 Development Editor；非 Editor Win64 Development 检查公开类型与测试宏组合。
- 用户 Editor readback：实际 GA→Montage、Skeleton/Slot、策略与标记、目标阻止声明，以及 Cancel/Rate/HoldReady 的区间和派发模式。
- 用户 PIE：两试点窗口内外的 Dodge/Guard/Parry、低体力拒绝、暂停/释放、重叠衔接、死亡/打断/重激活、防御恢复及 Big/Launch 权限回归。
- Main：按 `ue-strict-review` 完成批准 diff 的缺陷优先 Fresh Review，无未关闭 P0–P2。

**资产写入清单为空。** 内存测试资产不得保存或修改全局 CDO。生产资产若缺少验收窗口，先由用户明确具体路径和制作范围，再补白名单；不根据文件名推断引用关系。精确同刻 Branching Point 的引擎事件丢失不在本片修复范围，必须与已派发事件的并集正确性分开报告。

## 5. 交付、停止与收口

按“来源及结束原因透传 → Task 托管 → 两试点迁移 → 专项与旧测试适配”顺序实施；不新增阶段或并行写入者。

实施前核对真实 AGENTS、本 plan.md、HEAD、批准文件 diff 及第 1 节的 A 阶段前置门禁；基线漂移只检查相关 delta。计划落盘产生的 plan.md/ROADMAP.md 修改由 Main 所有，Executor 不覆盖、不归因、不纳入源码交付。

Executor 交付必须包含实际路径清单、API/行为变化、暂停与并集说明、自审发现及修复、静态结果，以及待用户执行的验证清单。不得通过保留已废弃 Context 或删掉有意义断言来规避测试迁移。

遇到清单外修改、资产写入、需要改变既有权限/阻止规则，或同一根因一次修复和一次复验后仍失败，停止并提交具体证据。暂停的实际原生事件若不支持本计划假设，同样停止，不自行替换契约。

全部适用门禁及 Main 终审完成后，只关闭 **03H9-B**，不宣称父阶段完成。A/H7/H6 债务按各自关闭条件保留；新增未通过或获准延期项在 ROADMAP 维护唯一记录与关闭触发。

Executor 不修改阶段文档、不提交、不推送、不自动启动 C。提交仍须用户明确批准。

## 6. Fresh Review 与 Main 修复交接（2026-09-13）

本轮按 ue-strict-review 两批证据预算完成只读审查，再依据用户“后面的修复由你接手”的明确授权进入 Main 修复回路；未派发子代理或 Executor。审查基线仍为第 1 节 HEAD，图谱仅作为定位证据。

| Finding | 原交付问题 | Main 修复 |
|---|---|---|
| B-R1 / P1 | Charged、Sprint 把工厂参数写成 `false, 1.0f, 0.0f`，实际为 Root Motion 缩放 0、起播 1 秒 | 改为 `1.0f, 0.0f, false`；Charged 测试辅助入口同修；生产激活测试断言起播位置和 Root Motion 缩放 |
| B-R2 / P2 | Charged 解除锁存撤 Tag 可同步 EndAbility，返回后仍解引用已清空的 AnimInstance | 恢复动画前重新检查结束状态与对象有效性；测试在撤 Tag 回调中取消真实 Charged |
| B-R3 / P2 | Dodge Tag 回调重入关窗后，外层仍按旧快照增加 Defense Tag | 第一项 Tag 回调返回后根据当前集合计算 Defense 需求；补增加/撤销时取消及不结束 Task 的重入关窗测试 |
| B-R4 / P2 | Cancel 回调未严格核对 EventTag，接受来源资产中的其他 NotifyState 类型 | 校验精确 Begin/End Tag 与 Cancel Notify 类型；同一合法绑定补正反对照 |
| B-R5 / P2 | 配置检查接受手填策略和空 Montage；GAS 测试直接加权限 Tag，暂停测试未证明原生 End 时序 | 检查实际 Task、源/目标声明及直接动画段；Sprint 用真实时间推进开窗，失败 Dodge 计数证明进入 CommitCheck；新增真实 Charged 的 Queued/Branching 暂停、终点及释放测试 |

第二轮对抗性复核：Task 并集与逐 Tag 贡献保留，配置反射仍仅在测试 helper；未恢复运行时预审或引入新框架。Ponytail 仅提出非阻塞的重复测试访问器与不可达 Notify 遍历分支简化建议，本轮不作为额外修复任务。

Main 本轮改动为 7 个原白名单源码/测试文件，以及 plan.md、ROADMAP.md；未改生产资产、Config、Build.cs，未提交。7 个代码文件逐文件 Rider 错误检查返回 `errors: []`；这只代表 IDE 静态检查，不是编译或运行证明。精确范围 `git diff --check` 通过。

修复后 Test Run 3 已收到，PIE 也已获用户确认；当前收据与待验证项统一维护在 ROADMAP 的 `Debt-03H9-B-RepairValidation`。用户已授权下面的 Queued End 判定修订并由 Main 实施；新增结果尚待复验，不将静态修复或部分测试通过写成 B/父阶段完成。

## 7. Queued End 判定修订（已获用户授权并实施）

证据来自用户附件 `61d1b005-6222-4574-9281-946612eb995e/pasted-text.txt` 的 Test Run 3。首个失败：`Charged Queued inside only a natural End is pending`，期望 0，实际 1；记录暂停位置 0.200、early Ends=0、natural Ends=1，随后释放时窗内权限被撤销。重复的 release reentry 场景同根因。Branching 内部暂停没有 End，终点有自然 End，未报告断言失败。

UE 5.8 源码机制：`AnimNotifyQueue.cpp:23` 的 GatherTickRecordData 直接共享 TickRecord.ContextData；`AnimNotifyQueue.h:89` 的 AddContextData 向共享数组追加数据；`AnimSequenceBase.cpp:489` 附近在当前时间经过任一 Notify 终点时添加 reached-end=true，单次 HoldReady 也会命中。结果是同批 CancelWindow 可读取其他 Notify 的结束上下文。公共 NotifyStateReachedEnd API 缺失上下文时返回 false，但并不隔离共享上下文。之前“原生标记属于本窗口”的推断不成立。

用户以“由你来”明确授权以下技术判定修订，并保留并集、暂停保权与释放结算的玩法目标：

1. Queued End 的自然结束需要核验本次事件对应窗口的结束边界，原生 bool 不再单独作为充分证据；保留合法实例与 Notify 归属校验。
2. 使用事件采样时的时间和本窗口边界；Montage 与直接动画段的坐标必须正确换算，不把 Montage 时间直接与 Sequence 局部时间比较，不用收到回调时的新播放位置替代来源采样。
3. Branching Point 继续采用自身 Payload 的 bReachedEnd；不改引擎源码、资产、Tag 或 GAS 权属。
4. 修改面仅限原白名单内 Cancel Notify/来源数据、必要消费适配及专项测试。保留当前失败断言，并补同批短 Notify/另一个窗口结束与直接动画段的正反对照；日志区分原生原始标记与核验后的本窗口结束结果。
5. 验收仍为：窗口内部暂停后权限保持，释放后到自身终点才关窗；已经到终点的窗口释放时立即结算。通过后再回归共享 Task 与用户相关 PIE。

实施结果：在 Cancel Notify 发送端核验采样时间和本窗口边界；直接动画段复用 FAnimSegment::ConvertTrackPosToAnimPos，处理偏移、裁剪及反向播放的退出边界。Task 的集合、暂停结算与清理代码保持原实现。保留导致 Test Run 3 失败的语义断言，新增同批短窗口结束、带偏移的正/反向 2x 动画段及原始/核验结果日志。

本轮只修改 AnimNotifyState_ActionWindows.h/.cpp、ManagedMontageCancelWindowAutomationTests.cpp 及两份交接文档。三份代码逐文件 Rider 错误检查均返回 errors: []，精确范围空白检查通过；未代用户编译、运行 Automation 或 PIE，未提交。增量复核核对了事件采样来源、动画段转换、原生标记隔离及原失败断言保留；最终运行结论仍待专项测试输出。

## 8. 直接动画段测试派发前提修复

最新用户附件 ad96831e-d72e-4b32-9eaf-f5da54839eb1 的专项整体仍为 Fail；Montage Queued 内部/终点暂停、Branching 与短窗口对照未报告失败，新增正反向直接动画段六场景没有收到 End。UE 的段 Notify 进入 UnfilteredMontageAnimNotifies，需有效 Slot 相关性及 ApplyMontageNotifies；原 montage-only fixture 未建立这些前提。

用户明确授权 gpt-5.6-luna / high 子代理执行本轮窄修复。仅在 ManagedMontageCancelWindowAutomationTests.cpp 为隔离实例注册 DefaultSlot、初始化双缓冲权重并启用原生 autonomous pose 队列派发入口；增加暂停后窗口集合断言。未手工注入 Begin/End 替代动画推进，原有时序与重入断言保留，生产代码不变。Main 补充 fixture 原因注释并核对增量；该文件 Rider errors: []，含未跟踪测试文件的空白检查通过。图谱本轮错定位，采用指定源码 coverage fallback；测试局部改动不扩展影响图查询。

本次尚未编译或运行；先由用户编译并重跑 PolyQuest.Combat.ManagedMontageCancelWindow，尤其检查正反向动画段是否实际派发及其内窗/终点结算。原八组回归和旧版 PIE 确认保留各自证据时点。B 的唯一验证债仍为 ROADMAP 的 Debt-03H9-B-RepairValidation，不宣布收口或提交。

第 8 节复验结果：用户附件 7a494e49-c1b0-4bb4-8f95-752c243226b9 的专项仍为 Fail。正反向直接动画段六场景仍均为 raw=0、early=0、natural=0，新增暂停后集合断言也失败；Slot 修订未消除原症状，不能据此认定 Slot 是已证实的唯一根因。按第 5 节同根因一次修复与一次复验停止规则，停止继续试改代码。下一步建议仅在原测试文件补分层诊断：窗口 Begin/End 入口、Slot relevance、事件来源/实例 ID 与 Task 接纳结果，先定位事件丢失层，再决定修复；用户随后明确回复“确认修复”，已授权此追加回路。

## 9. 派发诊断与受控测试推进（用户已确认继续修复）

保持原批准路径、生产取消契约、真实原生 Notify 派发和既有断言；不修改资产。当前直接证据：fixture 使用真实 HeroMesh，AdvanceWorld 仅调用 World->Tick，未隔离 Mesh 自动动画更新；引擎完整 Proxy::PreUpdate 会清空 Slot 权重，因此上轮一次初始化权重不构成持续有效的派发前提。修复应控制第 9 节动画推进来源并记录实际 Begin 收据，不能以暂停后的集合断言代替事件派发证明。

实施结果：仅修改 ManagedMontageCancelWindowAutomationTests.cpp。移除第 8 节无效的全局一次性 Slot 设置，将注册与每步权重刷新收敛到第 9 节；保存并关闭 Mesh/CharacterMovement 自动 tick，退出恢复。每个最多 0.05s 步长仍推进 World，并仅显式执行一次 TickMontageOnly + 原生 DispatchQueuedAnimEvents。新增 Begin 总收据、来源匹配、实例匹配、Slot 相关性诊断；原窗口集合、暂停、释放及重入断言保留。前八节保持原 World 推进方式。

gpt-5.6-luna / high 完成执行，Main 增量复核补齐分层日志与集合断言；最终 Rider errors: []，未跟踪测试文件的 no-index 空白检查通过。两轮静态核对覆盖唯一动画推进者、退出恢复、回调解绑、来源过滤与原断言保留，未发现新的已证实阻塞缺陷；这不是运行通过结论。未编译、未运行 Automation/PIE，先重跑同一专项，保留所有既有通过项的原证据时点。

## 10. 文档与提交收尾（2026-09-13）

用户先确认最终专项自动化通过，Main 对第 9 节修复及直接依赖完成有界 Fresh Review，无 P0–P2；用户随后确认 PIE 通过并明确授权文档收尾和提交。本轮收据核销此前专项失败和修复后 PIE 待验证状态，前述修复记录中的“待复验”仅保留为历史时点。Rider 与空白检查属静态证据；不把 Automation/PIE 确认外推为非 Editor 构建或完整资产 readback。

ARCHITECTURE 更新已验证的 Task 取消窗口托管、三项策略、Charged 暂停结算及实际迁移消费者；ROADMAP 精简 B 为剩余收据入口，核心交付归档至 ROADMAP-archive 的 TODO-03H9-B 条目。A/H7/H6 等既有债不变，B 尚未单独报告项唯一归 ROADMAP，不自动实施 C。

提交范围为 12 个实际变更源码/测试文件与 4 份批准文档。PlayerActionWindowAutomationTests.cpp 在白名单内但无改动，EnemySmallHitReaction 不在本轮提交；Content/Config、二进制资产、tmp 与其他用户 WIP 排除。保留本 plan 作为最近交接，下一切片替换时固定本轮提交引用。
