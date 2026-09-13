# TODO-03H9-A：统一动作 Montage 播放与 RateWindow 托管

## 1. 目标、基线与反馈取舍

**状态（2026-09-13）：14 文件源码/测试切片已实施，修复回路结束，Main 最终增量 Fresh Review 通过，无未关闭 P0–P2；用户确认 Development Editor 编译通过，并已确认专项 Automation 与 PIE。当前保留本片交接，验证证据分层见第 6 节；未单独报告的门禁唯一归 ROADMAP.md 的 Debt-03H9-A-ValidationReceipts。用户已明确批准本片 14 个源码/测试文件与 4 份阶段文档提交，不推送。**

新增 UAbilityTask_PlayActionMontage，让 Ability 选择标准播放入口后自动支持 RateWindow，不再复制监听、Context、实例状态和恢复逻辑。

本片迁移 SprintAttack 与 EnemySmallHitReaction，分别验证普通播放和连续重激活/方向切换。CancelWindow 属于 03H9-B，其余适用动作迁移与配置验收属于 03H9-C。

- 仓库：E:\GameDevelop\PolyQuest。
- 引擎依据：D:\UE\UE_5.8。
- Git 基线：6ca8a39923c6d00d12f84b0ae850c72a0dd9b178。落盘前复查源码、AGENTS 和阶段文档没有未提交变动；保留全部既有 Content/Config WIP。
- 03H8 核心已归档于 ROADMAP-archive.md 的“TODO-03H8 — Main Implementation Closeout（2026-09-13）”及其提交授权补充；本次不重复归档。完整旧交接读取 git show 6ca8a39923c6d00d12f84b0ae850c72a0dd9b178:plan.md。
- 本计划完整替代上一版对话计划。ROADMAP 已接受 03H9-A → B → C → 03C；Main 已按用户继续指令将 Current handoff 同步至本片，03H8 完整凭据保留在其提交与归档中。
- 初始规划证据：真实 AGENTS、Git、目标源码、直接测试及 UE 5.8 引擎实现；当时没有新增运行时验证。实施后的用户验证及 Main 审查以第 6 节为准。

Gemini 反馈处理：

| 意见 | 冻结决定 |
|---|---|
| Branching Point 防双发 | 采纳：按派发模式走互斥单一路线；重写 Begin/End 不调用 Super 默认回落 |
| 旧 Task 停止新实例 | 采纳并强化：改速、停止、解除实例委托和清除 ASC 播放所有权均必须校验原实例归属 |
| 删除 Ability 并行停止逻辑 | 采纳：两个试点移除直接 Montage_Stop，Sprint 移除 AnimInstance 全局结束监听 |
| TargetData 与测试 helper | 实现 GetScriptStruct()；集中 helper 放入本片测试支持文件，不扩展通用 CombatAutomationFixture。Handle 拷贝共享指针，并非对象切片问题 |
| 为 B 避免返工 | 现在采用中性名称 PlayActionMontage；内部以私有函数分开播放和 RateWindow 操作，B 扩展同一个 Task，不提前建设策略接口或窗口框架 |

Ponytail lite：继续扩展 Binding helper 虽更短，但仍要求每个新 Ability 接线；专用播放 Task 是满足本次目标的最小方案。

Deliberate Non-goals：不实施 CancelWindow 封装、全项目迁移或 03C；不引入通用 Ability 基类、角色级监听/第二状态机、跨项目插件、网络复制、全局 Time Dilation、外部依赖或无关动作重构；不修改资产或既有窗口算法。

## 2. 实现与运行时契约

### 2.1 统一入口与所有权

- 新 Task 直接继承 UAbilityTask，内部组合既有 FAbilityMontageRateWindowLifecycle，通过 ASC::PlayMontage 播放；不嵌套具有独立停止权的播放 Task。
- 提供 Blueprint/C++ 工厂 PlayActionMontage，参数限定为 OwningAbility、TaskInstanceName、Montage、Rate、StartSection、RootMotionTranslationScale、StartTimeSeconds、AllowInterruptAfterBlendOut，默认值沿用 UE 原生播放入口。
- 输出 OnCompleted、OnBlendedIn、OnBlendOut、OnInterrupted、OnCancelled、OnFailed。Task 拥有播放和窗口资源；Ability 保留 Commit、玩法状态与唯一 EndAbility() 出口。
- 一次 Task 对应一次播放。换 Montage 或重播必须创建新 Task；切段、暂停、恢复继续使用同一实例。
- RateWindow 默认支持，无逐 Ability 开关、注册表或转发函数。Task 使用 ASC 精确 Tag 委托及解除句柄集中监听，不创建专属 Context 或两份 WaitGameplayEvent Task。
- 播放、Root Motion scale 和委托行为以已核对的 UE 5.8 原生实现为依据；停止授权必须增强到实例级，不能照搬仅比较 Ability/Montage 指针的实现。
- 私有职责分为启动/实例检查、RateWindow 订阅与处理、统一退出三部分；只用函数和已有 Lifecycle，不增加第二层管理器、基类或可插拔接口。

### 2.2 事件身份、兼容与单次派发

保持现有字段及语义：

- Tag：Event.Action.RateWindow.Begin / Event.Action.RateWindow.End。
- Instigator、Target 为 Avatar；OptionalObject 为来源 Montage/Sequence；OptionalObject2 为原 RateWindow Notify。
- RateMultiplier 属性和 Notify 类身份保留；运行时仍为绝对播放速率。
- 来源校验、有效速率、窗口身份、重复事件忽略、Last-Active-Wins 与基准恢复继续复用现有 Lifecycle，不重写算法。

为 RateWindow 增加最小本地来源数据：

- 在 Notify 头文件声明 FGameplayAbilityTargetData_MontageRateWindowSource，包含弱引用 AnimInstance 和 MontageInstanceID，正确实现 GetScriptStruct() 返回 StaticStruct()。
- 数据由 FGameplayEventData::TargetData 持有，仅用于本地事件，不增加网络传输、存档或配置。
- Queued 路线：从 EventReference.GetContextData<UE::Anim::FAnimNotifyMontageInstanceContext>() 取得原实例 ID。
- Branching Point 路线：重写 BranchingPointNotifyBegin/End，从 Payload 取得原实例 ID，直接发送一次；禁止调用 Super，也不再转调 NotifyBegin/End。
- 两种模式各自只经过一条发送路线。其他 Notify 类型的派发行为不变。
- 新 Task 先验证 TargetData 类型、来源 AnimInstance 和实例 ID，再调用 Lifecycle。缺失/错误身份拒绝处理，并输出包含 Ability、Montage 的可定位诊断；禁止从“当前活动 Montage”补填未知来源 ID。
- 原消费者继续读取原字段，不强制本片迁移；无需重命名 Notify、重建资产或改变资产引用。

### 2.3 启动、重入与统一清理

- Activate() 校验 ASC、Avatar、AnimInstance、Montage、速率和 Tag，建立事件订阅，通过 ASC 播放；确认真实实例并捕获基准后才接受窗口事件。
- 将播放调用、任务激活及向外广播视为同步重入边界。返回后确认原 Task 未结束、Ability 仍激活且未被新播放替换；禁止恢复旧字段或继续写新播放。
- 启动失败先清理自身资源，再报告 OnFailed。试点将失败接到现有取消结束出口；失败、完成、打断和主动取消不能重复发出终止结果。
- 所有退出共用一个幂等清理函数：先关闭事件入口、解除监听；仍拥有原实例时恢复速率；再解除本次实例委托并执行适用的停止；最后释放引用。
- 每次具有外部副作用的清理前重新校验归属，继续使用 FAbilityMontageRateWindowLifecycle::IsCurrentMontageInstance 作为实例授权规则。停止还须符合 ASC 当前播放归属。
- 旧 Task 若发现 ID 已改变，只清自身状态，绝不停止新 Montage、解除新实例委托、重置新播放的 Root Motion scale 或清除新 ASC 所有权。
- 两个试点主动结束时保留既有零秒停止。自然结束保持原 BlendOut/Completed 时机；外部已停止或替换原实例时不补写恢复值。
- 暂停不等于停止，保留窗口和基准；恢复后继续消费。无窗口 Montage 保持初始播放速率和原退出行为。

### 2.4 试点迁移

SprintAttack：

- 替换播放 Task；删除 RateWindow 专属 Context、字段、绑定转发、清理及相关 friend。
- 删除 BoundAnimInstance->OnMontageEnded 的手动绑定/解绑，使用 Task 结果委托。
- 删除 Ability 内直接 Montage_Stop；EndAbility() 只结束并释放 Task，由 Task 恢复和停止。
- 保持 Commit → 动作 Tag/朝向 → 播放 → Motion Warping/Guard/Sprint 收尾的相对顺序；Trace、CancelWindow 和伤害逻辑不改。

EnemySmallHitReaction：

- 替换播放 Task，移除 RateWindow 专属 Context、token、监听和实例副本，删除 Ability 内直接停止。
- 保留四向选择、bRetriggerInstancedAbility、Commit 先于播放、Small Reaction 非阻断契约，以及 BlendOut 后允许打断的设置。
- 结束旧 Task 时先解除其面向 Ability 的结果绑定，防止旧 Task 结果进入新激活。
- 两个 Ability 可以保留其他玩法需要的 ActiveMontage/AnimInstance 引用，但不再拥有 RateWindow 或独立停止权。

## 3. Approved Paths 与交接

以下为本片源码/测试实施白名单；未列源码路径保持只读。已完成的实现与后续修复均沿用此范围。

| 绝对路径 | 允许修改范围 |
|---|---|
| E:\GameDevelop\PolyQuest\Source\PolyQuest\Public\AbilitySystem\Tasks\AbilityTask_PlayActionMontage.h（新增） | 标准播放 Task 声明、事件与私有状态 |
| E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\AbilitySystem\Tasks\AbilityTask_PlayActionMontage.cpp（新增） | 播放、监听、来源验证和统一清理 |
| E:\GameDevelop\PolyQuest\Source\PolyQuest\Public\Animation\Combat\AnimNotifyState_ActionWindows.h | 来源数据类型，以及 RateWindow Branching Point 声明 |
| E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\Animation\Combat\AnimNotifyState_ActionWindows.cpp | 仅 RateWindow Queued/Branching Point 派发与必要私有辅助 |
| E:\GameDevelop\PolyQuest\Source\PolyQuest\Public\AbilitySystem\Abilities\SprintAttackAbility.h | 播放迁移、旧 RateWindow/重复停止删除、相关测试接口 |
| E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\AbilitySystem\Abilities\SprintAttackAbility.cpp | 播放迁移、旧 RateWindow/重复停止删除、相关测试接口 |
| E:\GameDevelop\PolyQuest\Source\PolyQuest\Public\AbilitySystem\Abilities\EnemySmallHitReactionAbility.h | 播放迁移、旧 RateWindow/重复停止删除、相关测试接口 |
| E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\AbilitySystem\Abilities\EnemySmallHitReactionAbility.cpp | 播放迁移、旧 RateWindow/重复停止删除、相关测试接口 |
| E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\Tests\TestManagedMontageAbility.h（新增） | 最小测试 Ability；本片共享 MakeRateWindowEventData 测试 helper 声明 |
| E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\Tests\TestManagedMontageAbility.cpp（新增） | 最小测试 Ability；本片共享 MakeRateWindowEventData 测试 helper 实现 |
| E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\Tests\ManagedMontageRateWindowAutomationTests.cpp（新增） | 公共 Task、事件来源和真实时间推进集成验收 |
| E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\Tests\PlayerMontageRateWindowAutomationTests.cpp | Sprint 对应用例迁移；其他消费者继续原测试路线 |
| E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\Tests\EnemyMontageRateWindowAutomationTests.cpp | Small Reaction 对应 CDO、窗口、连续重激活用例迁移 |
| E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\Tests\HitReactionAutomationTests.cpp | Enemy Small Section 7.2 的 Task 类型和旧回调隔离断言 |

测试 helper 接受显式事件 Tag、Avatar、来源动画、Notify、速率，以及可选的来源数据；统一构造裸数据或带身份的数据。错误 ID、错误 AnimInstance 由调用方明确传入，空 TargetData 明确构造；helper 不自动从当前播放推导或修正身份。

既有 Lifecycle、Binding、CombatAutomationFixture、其他 Ability/Character、Build.cs、Tags/Config、Content 不修改。不为保住旧测试结构重新增加生产 Context 或实例 bypass。

```text
Outer: ue-stage-workflow
Primary: ue5-cpp-gameplay
Support: ponytail:ponytail（lite）
Route reason: 项目内 GAS Montage 播放所有权及 RateWindow 生命周期封装，含两个动作试点和伴生测试。
Execution route: manual/out-of-band Antigravity / Gemini
Implementation executors: 1（用户手动交接 Gemini）；Main 未自动派发
```

- Main 拥有计划、文档、集成和终审。03H8 归档已核对；用户在最终修复复核及编译确认后要求继续，Main 据此同步本片交接、路线图、架构事实与归档记录。
- Gemini 只实现白名单源码和测试，首次交付按真实 AGENTS 优先安排一个干净只读子代理严格自审，不递归派发；后续修复单兵复核。若没有可用子代理能力，明确报告回退及自审证据边界，不伪称独立审查。
- Gemini 不修改文档、不暂存/提交/推送，不未经用户明确授权代跑编译、Automation 或操作 Editor。Main 在验证、终审后才更新 ARCHITECTURE.md 的 A 阶段稳定事实。
- Main 本次文档维护路径限 E:\GameDevelop\PolyQuest\plan.md、E:\GameDevelop\PolyQuest\ROADMAP.md、E:\GameDevelop\PolyQuest\ROADMAP-archive.md、E:\GameDevelop\PolyQuest\ARCHITECTURE.md；Gemini 对这些文档保持只读。
- 需要越界、变更玩法权限、资产、ASC 权属或依赖时报告具体阻塞，不自行扩围。同一根因最多一次有据修复和一次针对性重跑，再次失败保留首个证据并停止该修复回路。

## 4. 验证矩阵与完成态

| 门禁 | 验收要求 |
|---|---|
| 静态 | 白名单封闭；两个试点无专属 RateWindow 接线、全局 Montage 结束监听或直接停止；公共层无具体 Ability 分支；git diff --check 通过。适用时使用 Rider 诊断，不可用则记录回退 |
| 用户编译 | UE 5.8 Win64 Development Editor 通过；非 Editor Development 编译核验新增公共类型和测试宏边界，Editor 动画构建代码不能泄漏 |
| 公共 Automation | PolyQuest.Combat.ManagedMontageRateWindow：真实 ASC 授予/激活、可播放瞬态 Montage、时间推进和真实 Notify 派发 |
| 既有回归 | PolyQuest.Combat.PlayerMontageRateWindow 六组（Light/MeleeSkill/Sprint/Charged/Bow/Dodge）、PolyQuest.Combat.EnemyMontageRateWindow、PolyQuest.Combat.EnemyStanceBreakRateWindow、PolyQuest.Combat.HitReaction、PolyQuest.Combat.PlayerBigHitReactionWindows、PolyQuest.Player.ActionWindows、PolyQuest.Combat.PlayerMeleeMotionWarping |
| 用户 Editor readback | 两个试点的实际 GA→Montage 引用、Small 四向选择、Skeleton/Slot、Notify 类型/数值/区间及派发模式保持有效，无需重新连接监听 |
| 用户 PIE | Scene01：Sprint 改速恢复和取消；Enemy Small 同方向立即重播、跨方向连续受击、自然结束、打断、销毁/适用 UnPossess；无残留速率、旧回调误结束或旧 Task 停止新动画 |
| Main Fresh Review | 使用 ue-strict-review 检查来源身份、同步重入、停止权、清理顺序、测试可信度和债务归口；无阻塞 finding |

核心测试必须覆盖：

1. 默认支持：最小测试 Ability 只有标准播放调用及结束回调，无窗口接线；换 Montage/窗口数据无需修改公共层。
2. 速率语义：无窗口保持非 1.0 初始速率；窗口覆盖和最终恢复基准；重叠、交叉、相邻窗口、切段、暂停恢复。
3. 互斥单发：分别用 Queued 和 Branching Point 时间推进，观察 ASC 事件次数与来源数据，每个实际 Begin/End 只派发一次；不能仅用最终速率判断，因为重复 Begin 会被幂等逻辑掩盖。
4. 身份防御：缺失、错误类型或错误实例 TargetData，错误 Avatar/动画/Notify，重复 Begin、未知 End、非法速率均不污染状态；Handle 拷贝后仍能识别派生来源数据。
5. 同资产重播误杀回归：先建立旧窗口，再播放同一资产产生不同真实 ID；对旧 Task 执行迟到事件、取消和销毁，断言新实例仍播放，速率、委托和 ASC 所有权未被清除。另测跨 Montage 替换。
6. 重入与失败：无效播放、Commit 拒绝、播放或结果回调中同步取消/重激活，不恢复旧 Task，不产生重复终止回调，不遗留监听。
7. 退出：自然结束、Ability 结束、ASC 取消、Task 取消及 Avatar 销毁；能停止原实例的路径先恢复速率，已经失去原实例的路径不写新播放。
8. 重复执行：同一 Editor 会话再次运行目标测试，确认任务、监听和测试前置没有残留。

正向集成不得用直接广播、手调 Notify 或 bypass 冒充时间推进；集中 EventData helper 只用于补充非法输入和迟到事件断言。沿用已有真实动画构建方式和 CombatAutomationFixture，测试资产仅存在内存，不保存、不修改全局 CDO。

资产写入清单为空，Agent 不操作 live Editor。若实际试点缺少验收窗口，由用户先确认具体资产路径与最小制作范围，再更新明确清单；不得默认修改 Content 或虚构 readback。

精确同刻 Branching Point 的引擎事件丢失不在本片修复范围。03H7/H6 债务继续按 ROADMAP 原关闭条件独立对账；新增未通过门禁或获准延期必须有唯一记录及关闭触发。

全部适用门禁具备证据并通过 Main 终审后，只关闭 03H9-A，进入 03H9-B；不宣称父阶段或全项目迁移完成。Git 提交仍需用户明确批准。

## 5. Executor 交付要求

1. 先核对真实 AGENTS.md、本 plan.md、仓库根目录、HEAD 与目标 diff，识别并保留既有 WIP。计划落盘时的 plan.md 变动由 Main 所有，不能覆盖、归因或纳入 Executor 的源码交付。
2. 仅围绕目标与直接依赖收集证据。调用链或引擎签名需要确认时，按项目规则先定向 CodeGraph；覆盖不足则记录后聚焦读取。证据充分即实施，不做全库审计或其他阶段规划。
3. 在 14 条 Approved Paths 及其符号范围内完成代码和测试。具体算法、私有函数与边界断言自主闭环，不因局部细节频繁请示。
4. 按第 3 节完成首次严格实施自审，交付路径清单、行为/API 变化、关键生命周期说明、发现与修复记录、git diff --check 和适用 Rider 结果。
5. 验证报告区分本人静态检查、子代理自审和用户编译/Automation/Editor/PIE；未执行门禁明确列为待用户验证。直接事件测试不作为真实时间推进证据。
6. 停止在代码/测试/静态与实施自审交付，不更新阶段完成态，不修改文档，不提交、不推送，不自动开展 03H9-B/C 或 03C。Main 另行执行 Fresh Review 与文档收口。

## 6. 实施、审查与验证交接（2026-09-13）

### 6.1 已完成实现与修复

- `UAbilityTask_PlayActionMontage` 通过 ASC 播放并集中托管 RateWindow；SprintAttack、EnemySmallHitReaction 已迁移，移除其专属窗口 Context/监听和并行 Montage 停止逻辑。其余消费者保留现有实现。
- Notify 以本地 TargetData 携带来源 AnimInstance 与 InstanceID；Queued 从实际上下文取 ID，Branching Point 从 Payload 取 ID 且不调用 Super。缺失或错误身份拒收，不从当前活动实例补填。
- Task 的幂等清理先解绑、在仍拥有实例时恢复速率，再授权停止；直接 EndTask 保留零秒停止，默认配置下 ASC 取消正常通知，自然结束不再因实例已停止而吞掉 Completed。
- Root Motion scale 仅在本 Task 实际设置过且没有其他资产/新实例接管时恢复。启动失败不污染其他播放；两个恢复入口共用判断，恢复及终结清除自身标记。
- 公共测试已包含实际 ASC/Task 播放、Queued 与 Branching Point 时间推进派发、完成次数与 ActivationOwnedTags 清除，以及启动失败/同资产重播/不同资产接管防污染。不同资产迟到清理测试主动解除旧实例委托来保留待清理前置，属于有真实播放的定向边界测试，不宣称该延迟由引擎自然调度产生。

### 6.2 已确认的证据

| 类型 / 所有者 | 已确认结果与边界 |
|---|---|
| 编译 / 用户 | 用户明确确认“编译通过，不然怎么测试自动化”；记录 Development Editor 通过，不再追问该结果。不将此语句扩写为独立非 Editor Development 构建结果 |
| Automation / 用户回传 | 本片已报告 PlayerMontageRateWindow 六组（Light/MeleeSkill/Sprint/Charged/Bow/Dodge）、EnemyMontageRateWindow、ManagedMontageRateWindow Success；最终 Root Motion 授权修复后再次明确报告 Managed Success。其他套件保留其各自报告时点，不追认为最终改动后全部重跑 |
| PIE / 用户 | 本片多次确认 PIE 通过；明确报告过 Sprint 自然播完后恢复移动、翻滚和继续攻击。没有独立逐项资产 readback 凭据，不从 PIE 推导资产配置已逐字段核验 |
| 静态 / Gemini | 最终五个 Task/测试文件 Rider 0 errors、Source 空白检查通过；Main 直接采纳，不伪称由 Main 重跑 |
| Fresh Review / Main | 完成批准范围内的缺陷审查及逐轮有界增量复核；最后 Root Motion 授权 P2 已关闭，无未关闭 P0–P2。Ponytail 辅审无新增独立复杂度建议；图谱只作影响导航 |

最终修复报告为本会话附件 `e1305cec-0d59-44fa-9b4a-0e6f158eba21/pasted-text.txt`；源码状态基于工作树，HEAD 仍为 `6ca8a39923c6d00d12f84b0ae850c72a0dd9b178`。

### 6.3 剩余门禁与后续

- 源码实现及修复审查已完成；尚未单独确认的构建、回归与 readback 证据只引用 `ROADMAP.md / Debt-03H9-A-ValidationReceipts` 的唯一清单及关闭条件。本记录不擅自豁免原验证矩阵，也不新增代码修复任务。
- 本片实现/审查记录已沉淀到 ROADMAP-archive.md，ARCHITECTURE.md 更新已验证的两个试点运行时事实；plan.md 保留本片交接。03H9-A 总验收标记仍待原门禁对账，不启动 B/C 或 03C；后续路线保持 A → B → C → 03C。
- 文档整理时 Main 仅修改四份阶段文档，不修改源码、Config、Content，不运行编译、Automation 或 Editor/PIE。用户随后明确批准提交；本记录随 14 个批准源码/测试文件及 4 份阶段文档共 18 文件提交，采用显式路径暂存，保留全部无关 WIP，不推送。提交批准不改写剩余门禁的证据状态。
