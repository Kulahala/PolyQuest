# TODO-07B8-C：Player RateWindow 生命周期统一重构

## 1. Target Objective、基线与边界

> 当前执行安排（用户最新指令优先）：六类实现和 Codex 编译／Automation 已交付。用户随后确认 PIE 清单“测试通过”，并明确允许本会话派发一个干净的只读子代理执行 Fresh Review，取代此前“另一个会话审查”的安排。Main 负责文档收口；原文件白名单、运行时契约、readback 收据和最终提交边界保持有效。

将 Light、MeleeSkill、Sprint、Charged、Bow、Dodge 六个现有 Player 消费者迁移到 `FAbilityMontageRateWindowLifecycle`，替换本地计数、单窗口布尔状态和固定恢复 `1.0` 的旧实现。统一窗口身份、重叠选择和基线恢复，同时保留各 Ability 的动作生命周期。

Enemy 作为既有实现基准和回归对照，本阶段不修改 Enemy 源码。Player HitReaction 当前没有 RateWindow，本阶段不新增支持。

- 项目：`E:\GameDevelop\PolyQuest`；模块：PolyQuest；引擎源码唯一基准：`D:\UE\UE_5.8`。
- 规划与落盘基线 HEAD：`9ae684cea97e682bc98c58c8776dc0f61639719c`。规划检查中 Source／阶段文档无未提交差异；落盘前 `plan.md` 无新增改动。Content／Config 等既有用户 WIP 全部保留。
- 前阶段 TODO-07B8-B 已在 `ROADMAP-archive.md` 的 `TODO-07B8-B Enemy Montage RateWindow Adoption And Identity Repair Closeout (2026-09-12)` 条目归档；完整旧计划与验证收据可从上述提交的 `plan.md` 读取，不依赖旧归档中的“当前 plan.md”字样定位。
- 状态（2026-09-12）：六个 Player 消费者的实现、编译、Automation、用户 PIE 和审查收口已完成。独立 Fresh Reviewer 找到的唯一 P2 已由 Main 在批准范围补修，先复现失败再通过 Light／Motion Warping 定向回归与 Main delta 复核，见第 7.5 节。逐资产配置 readback 仍为非阻塞收据债务；用户已批准仅提交本阶段 15 个 Source／Test 路径及四份收口文档，提交记录以 Git 历史为准。
- Outer: ue-stage-workflow
- Primary: ue5-cpp-gameplay
- Support: none；本轮独立 Fresh Reviewer 使用 ue-strict-review。
- Route reason：六个现有 Native Player GAS Ability 的 RateWindow 消费迁移与生命周期收敛。
- Execution route: none（实施）。用户授权本会话 Codex 完成剩余五类实现、编译和 Automation；Implementation executors: 0。Fresh Review route: user-authorized clean-context subagent `/root/ratewindow_fresh_review`，只读、禁止递归派发；Main 负责文档和最终结论，用户负责 authored Editor／PIE 与最终提交批准。
- CodeGraph 已用于定向定位消费者、helper 和 Notify；图谱裁剪或未覆盖的启动／清理上下文采用聚焦源码读取。图谱与源码核查只构成静态证据。

Deliberate Non-goals：修改 Enemy、共享 helper、Notify、Tag、Input、Build.cs、伤害或 GAS 权属；新增 Player HitReaction RateWindow；全局监听、时间膨胀、通用 Ability 基类、网络能力；资产写入、批量重存、调参及 Git 提交。

## 2. Approved Paths 与接口变化

### 2.1 生产代码：12 个封闭路径

以下表格每行精确对应一个头文件和一个实现文件，不授权目录内其他文件：

| 头文件 | 实现文件 | 允许修改的符号范围 |
|---|---|---|
| `Source/PolyQuest/Public/AbilitySystem/Abilities/LightAttackAbility.h` | `Source/PolyQuest/Private/AbilitySystem/Abilities/LightAttackAbility.cpp` | RateWindow 状态与回调；ActivateAbility、StartComboEntry、EndAbility 中的绑定、交接和清理。 |
| `Source/PolyQuest/Public/AbilitySystem/Abilities/PlayerMeleeSkillAbility.h` | `Source/PolyQuest/Private/AbilitySystem/Abilities/PlayerMeleeSkillAbility.cpp` | RateWindow 状态与回调；播放确认、Commit 后绑定及 EndAbility 清理。 |
| `Source/PolyQuest/Public/AbilitySystem/Abilities/SprintAttackAbility.h` | `Source/PolyQuest/Private/AbilitySystem/Abilities/SprintAttackAbility.cpp` | RateWindow 状态与回调；启动确认和 EndAbility 清理。 |
| `Source/PolyQuest/Public/AbilitySystem/Abilities/ChargedAttackAbility.h` | `Source/PolyQuest/Private/AbilitySystem/Abilities/ChargedAttackAbility.cpp` | RateWindow 状态与回调；启动确认、暂停／释放衔接和 EndAbility 清理。 |
| `Source/PolyQuest/Public/AbilitySystem/Abilities/BowDrawFireAbility.h` | `Source/PolyQuest/Private/AbilitySystem/Abilities/BowDrawFireAbility.cpp` | RateWindow 状态与回调；启动确认、Section 跳转衔接和 EndAbility 清理。 |
| `Source/PolyQuest/Public/AbilitySystem/Abilities/DodgeAbility.h` | `Source/PolyQuest/Private/AbilitySystem/Abilities/DodgeAbility.cpp` | RateWindow 状态与回调；启动确认、重触发和 EndAbility 清理。 |

所有相对路径均以 `E:\GameDevelop\PolyQuest` 为根。允许在上述头／源文件内增加仅服务 RateWindow 的瞬态 Context、私有绑定／清理方法及 `WITH_DEV_AUTOMATION_TESTS` 测试接口。删除旧调速状态；Bow／Dodge 的 `GetTestRateWindowApplied()` 调用改为读取 `GetTestRateWindowLifecycle().GetActiveWindowCount()`，删除旧 getter，不保留平行状态或仅服务旧断言的兼容接口。非 RateWindow 路径只允许为正确接入所必需的窄修改，不重构其他行为。

### 2.2 测试代码：3 个封闭路径

- 新增 `Source/PolyQuest/Private/Tests/PlayerMontageRateWindowAutomationTests.cpp`。
- 修改 `Source/PolyQuest/Private/Tests/PlayerActionWindowAutomationTests.cpp`，适配身份载荷和旧测试接口。
- 修改 `Source/PolyQuest/Private/Tests/PlayerMeleeMotionWarpingAutomationTests.cpp`，仅适配迁移影响的生命周期夹具，保留原 Motion Warping 断言。

### 2.3 Main 文档路径与只读参考

Main 文档白名单：`plan.md`、`ROADMAP.md`、`ROADMAP-archive.md`；`ARCHITECTURE.md` 仅在实现通过验证与终审后更新。首次计划落盘仅修改 `plan.md`；Light 试点补修复核后由 Main 同步本计划进度和 ROADMAP 当前阶段指针。

共享 `MontageRateWindowLifecycle.h/.cpp`、`AnimNotifyState_ActionWindows.h/.cpp`、Enemy 消费者／测试、其他测试基础设施、`Config/Tags/PolyQuestGameplayTags.ini`、`.Build.cs`、Content 和所有清单外路径只读。

### 2.4 公共接口兼容

不增加 Blueprint 作者参数，不改变现有 Notify 类或事件载荷。六个消费者直接使用共享 helper 的 `BindAndCapture`、`HandleBegin`、`HandleEnd`、`RestoreAndClear`，不修改这些接口。新增 Context 和测试访问仅服务本阶段的绑定、隔离与验证，不形成新的玩法 API。

## 3. 冻结的 Runtime Contracts

### 3.1 窗口协议

- 继续使用 `Event.Action.RateWindow.Begin/End`。Instigator、Target 均须为当前 Avatar；OptionalObject 为当前 Montage 或其包含的 Sequence；OptionalObject2 必须是该来源实际声明的 RateWindow Notify。
- `EventMagnitude` 是正有限的目标播放率，不与 baseline 连乘。
- 以“来源动画＋Notify 对象”配对，最后开始且仍活跃的窗口生效。重复 Begin 不增加计数或刷新优先级；未知、重复和无身份 End 不改变其他窗口。
- 最后一个窗口结束时恢复本次播放成功后捕获的 baseline。例如 baseline 为 `1.25`：A=`0.5`、B=`0.2`；B 先结束恢复 A，A 先结束保持 B，全部结束恢复 `1.25`。
- Light／MeleeSkill 的 RateWindow 来源校验统一接受合法内嵌 Sequence；其他事件继续使用各自现有校验，不扩大 Trace、Combo 或取消事件的接受范围。

### 3.2 绑定、重入与清理

- 每个消费者显式持有独立 lifecycle、RateWindow Context、绑定代次及 Montage instance ID。Context 只转发 RateWindow，其他委托保留原归属。
- 播放成功且当前启动流程仍有效后捕获 baseline；监听使用 exact tag。绑定前事件忽略，不缓存，不新增时间零点窗口保证。
- 每个相关 `ReadyForActivation()` 返回点核验 Ability 状态、Context／代次和任务身份。同步结束后不得重新绑定、恢复旧 Task 或重建旧播放状态。
- 每次转发和恢复前确认：当前活跃 Montage 实例仍是绑定的实例。仅按资产相同或旧 instance 尚能查询到，不足以授权写入。
- 统一清理顺序：使旧 Context 失效 → 移除 RateWindow 监听 → 恢复仍持有的实例 baseline → 清空 lifecycle／实例身份 → 继续原动作清理。
- 已停止或被新实例替换时，只清空本地状态。外部 GAS Cancel 可能先停播，不要求给已停止实例强写恢复值。
- 正常终止仍收敛到原 `EndAbility()`；连段换播放只关闭旧 RateWindow 绑定，不提前结束 Ability。
- UE 5.8 的按 Montage 资产调速接口操作 `GetActiveInstanceForMontage()` 返回的当前实例；清理保护必须与该实际写入目标一致。暂停不等于实例停止，RateWindow 调速不得调用 Resume。

### 3.3 逐项迁移顺序与专属契约

| 顺序 | 消费者 | 必须保持的行为 |
|---|---|---|
| 1 | Light | 每个 Combo Entry 独立捕获 baseline；切换前清理旧窗口，切换后建立新绑定；保留输入缓冲、逐段消耗、Trace 和 Motion Warping。 |
| 2 | MeleeSkill | 保留“播放确认 → Commit 成功 → 启用窗口”的顺序；Commit 失败不得留下监听、窗口或动作 Tag。 |
| 3 | Sprint | 保留消耗、Guard／Sprint 退出及 Motion Warping 时点。 |
| 4 | Charged | Pause／Resume 保持同一播放绑定，不重复捕获调速后的值；调速不能恢复暂停；取消窗口的暂停伪 End 防护不迁入 RateWindow。 |
| 5 | Bow | 保留 Draw／Hold／Release、提前松手、输入取消、单次射箭及现有 BlendOut 结束契约；同实例 Section 跳转不重新捕获 baseline，退出窗口按匹配 End 清理。 |
| 6 | Dodge | 保留实例绑定的 MontageTask 回调、无敌窗口和取消窗口；不新增 BlendOut 终止；真实重触发按旧 EndAbility → 新激活建立独立绑定。 |

先完成 Light 与既有 Enemy Melee 的对照门禁，再按表逐个推进其余五项。六项范围已在本计划冻结，门禁通过后不因普通实现细节重新请求范围批准。

沿用既有支持边界：不处理同一 Notify 对象在同一播放中并行重复出现的 occurrence 身份，不补偿引擎漏发通知，不统一 Hold、Recovery 或取消窗口生命周期。

### 3.4 Light 的 Entry 级监听与 Charged 的单次捕获

- Light 的 RateWindow 任务与 Context 随 Combo Entry 播放绑定创建和销毁。`ActivateAbility()` 不再预创建、绑定或激活 RateWindow Begin／End 任务，并同步移除相应的预创建成功检查；其他事件任务保持原有生命周期。
- `StartComboEntry()` 在覆盖旧 Montage／任务身份前调用统一的 `ClearRateWindow(true)`，解除旧 Context、监听并安全恢复旧 baseline。新 Entry 完成既有播放成功、任务身份及重入检查后，再分配新 Context、推进绑定代次、记录真实实例 ID、创建并激活本 Entry 的两个 RateWindow 任务，最后 `BindAndCapture()`。每个任务激活返回点均须重新核验同一 Entry／代次、当前任务和实际活跃实例；捕获后确认 lifecycle 已绑定。
- Entry 绑定失败时沿用现有失败返回和唯一终止路径；若 Ability 已结束或绑定代次已变化，退出旧调用，不复活旧 Task、不结束新代次。`EndAbility()` 复用同一 `ClearRateWindow(true)`。不能清掉 Entry 0 的任务后让 Entry 1／2 沿用空指针或已结束任务。
- Charged 每次成功激活只捕获一次 baseline，位置在 Montage 启动确认之后、首次 Hold／Release 后续处理之前。`BeginRelease()` 保留既有消耗、反馈和伤害参数逻辑；其播放衔接只沿用必要的 `Montage_Resume()`，不得重新 `BindAndCapture()`。暂停期间收到合法 RateWindow End 仍按窗口身份处理，不能套用取消窗口的伪 End 忽略规则。

## 4. 验证矩阵

### 4.1 Automation

新增套件前缀：`PolyQuest.Combat.PlayerMontageRateWindow`，覆盖六个生产消费者。

| 验证层 | 必须覆盖 |
|---|---|
| 事件与速率 | 真实 ASC 激活、生产监听任务、真实 Notify Begin／End 传输及实际 `Montage_GetPlayRate`；非 1 baseline、顺序、嵌套、交叉、相邻窗口两种交接顺序。 |
| 拒绝路径 | 重复 Begin／End、未知 End、错误 Avatar／Tag／来源／身份、非法倍率；负向用例保持其他字段有效，避免因无关条件提前拒绝而假通过。 |
| 生命周期 | 完成委托、取消、中断、销毁、适用的 UnPossess、重复清理、启动失败、同步结束、旧 Context 回调及同资产新实例不受旧清理污染；时间轴自然结束由适用 PIE 补证。死亡仅验证已有路由，本阶段不新增 Player 死亡系统，见第 7 节适用性说明。 |
| 专属回归 | Light Entry 0 → 1 → 2 均能通过各自新建监听接收 Begin／End，旧 Context／任务失效且 baseline 独立，并覆盖启动／绑定失败；MeleeSkill Commit 失败；Charged 调速中暂停、暂停中 End、恢复播放后 baseline 未被重捕获；Bow 提前松手和 Section 跳转；Dodge 真实 ASC 重触发。 |
| 既有套件 | `PolyQuest.Player.ActionWindows`、`PolyQuest.Combat.PlayerMeleeMotionWarping`、`PolyQuest.Player.MobileBow`、`PolyQuest.Combat.ChargedAttackNiagaraFeedback`，以及既有 Enemy RateWindow 套件。 |

测试夹具使用临时动画对象及现有测试基础设施，不写入 Content。必要的 bypass 仅用于逻辑层测试；手动设置 Active／token 或只检查集合大小不能作为真实播放、重触发或实例隔离证据。

既有 Bow／Dodge 测试载荷适配：

- 在临时 Montage／Sequence 的 `Notifies` 中挂入实际的 `UAnimNotifyState_MontageRateWindow` 对象，并以同一对象填充 `OptionalObject2`；不能只创建未声明到来源动画的 Notify。
- 每组拒绝测试先证明同一有效 Ability／绑定上下文能接受合法事件，再仅破坏所测字段。非法倍率用例保留合法来源／身份；外来 Sequence 用例使用该外来 Sequence 自己声明的 Notify，只让它不属于当前 Montage。
- 未知／重复／错误来源 End 必须在已有合法窗口生效时测试，确认当前窗口和实际速率保持不变。恢复／清理断言前先打开合法窗口，不能把从未调速的空状态当成恢复成功。
- 断言直接检查 lifecycle 的绑定、活跃窗口数及实际速率；涉及播放率和完整链路的证据由真实实例测试提供，既有 seam 测试继续标为逻辑层。

Headless bypass 与实例 ID 边界：

- 不将“Headless／无 Tick”直接等同于“不能建立真实 Montage 实例”；参考既有 Enemy 真实 ASC／Montage 夹具，新增集成门禁关闭 bypass，并断言确有活跃实例且记录的 ID 与该实例一致。
- 仅在既有纯逻辑夹具确有需要的消费者中保留或补充 `WITH_DEV_AUTOMATION_TESTS` bypass；setter 同步 Ability 与 helper 的开关，清理／重置后两者保持一致，默认关闭。不得为形式统一给六类全部增加未使用的测试开关。
- 不默认复制 `ActiveMontageInstanceID = 1`。确需 mock ID 的纯逻辑用例可在测试宏和显式 bypass 内使用，但不得将其作为真实实例查询或恢复写入的授权依据；真实播放、实例替换和重触发测试不得走该分支。bypass 不豁免 Actor／Tag／来源／Notify 身份验证。

既有 Enemy 回归至少包括 `PolyQuest.Combat.EnemyMontageRateWindow`、StanceBreak RateWindow 既有套件和一个实际 Enemy Melee 的代表性 PIE；不为完成回归改动 Enemy 生产代码或测试文件。

### 4.2 用户 Editor 操作与 Readback

- 编译 `PolyQuestEditor — Development Editor` 并运行对应 Automation。
- Light 试点先在 Scene01 验证，与一个既有 Enemy Melee RateWindow 对照；通过后验证剩余五类。
- 记录六类实际 GA／Montage 路径、窗口位置、倍率及 Tick Type。相邻窗口验收使用两个独立 Notify 和 Queued；覆盖 Section 边界、不同帧率及各类中断。
- 现有原生 Notify 已携带身份，无需批量重存资产。缺少验证窗口时，由用户配置并提供 readback；Agent 不写资产。
- 同 TriggerTime 的 Branching Point 可能漏发通知，不能将其归为身份集合的配对保证；如遇此配置，由用户调整后再验证，不通过代码延迟或时间容差掩盖。

### 4.3 门禁和证据归属

| 门禁 | 所有者 | 通过标准 |
|---|---|---|
| 静态 | 本会话 Codex | 限定路径 diff、自审及 `git diff --check` 通过；Rider 可用时检查适用文件，不可用时明确 coverage fallback。 |
| Light 试点 | 用户运行，Gemini／Main 分析 | Light 新专项与相关既有 Automation、Development Editor 编译、代表性 readback／Scene01 PIE 通过，Enemy Melee 对照无回归，再进入其他五项。 |
| 后续五类接入 | 本会话 Codex | 完成五类实现、专项及相关回归，不再逐项等待 Gemini／Main 放行。 |
| 最终集成 | Codex 编译／Automation；用户 readback／PIE | 六类迁移完成后的编译、专项／既有 Automation、适用 readback／PIE 证据齐备。 |
| Fresh Review | 用户授权的独立子代理；Main 负责窄修复复核 | 依据 ue-strict-review 做有界缺陷优先审查；独立审查找到的具体问题须在批准范围修复并定向验证。本会话早先实施自检不替代独立初审。 |

手工调用 Notify 的测试与引擎时间轴／用户 PIE 证据分别记录，不混称。编译、运行、readback 和视觉结果必须注明实际执行者；规划静态核查不构成这些门禁的通过证据。

## 5. Executor 交付、收口与停止条件

### 5.1 自包含交接要求

- 在 `E:\GameDevelop\PolyQuest` 工作，以本 `plan.md` 为当前 07B8-C 交接依据；实施前复核 HEAD、15 个 Source／Test 白名单路径 diff 和用户 WIP，不覆盖后来出现的用户改动。
- 仅实现本计划 12 个生产文件和 3 个测试文件；按 Light → MeleeSkill → Sprint → Charged → Bow → Dodge 顺序推进，遵守第 3 节冻结契约及第 4 节试点／逐项门禁。
- 生产／测试修改限于第 2 节白名单；本轮 Codex 同步 `plan.md` 与 `ROADMAP.md` 交接状态。共享 helper／Notify、Enemy、Tag／Config、Build.cs 和资产保持只读；不提交 Git，不自行扩展批准范围。
- 完整交付附实施自检，按用户本轮安排不派发自审子代理。交付逐项列明改动、静态证据、测试覆盖和适用用户门禁。
- 显式核验异步回调对象／状态有效性、`ReadyForActivation()` 重入边界、同资产新实例隔离和单一 `EndAbility()` 终止出口。

### 5.2 文档与完成态

- 07B8-B 已归档，旧计划通过基线提交保留；本 `plan.md` 自 2026-09-12 起切换为 07B8-C。C 已完成源码／编译／Automation／用户 PIE／审查收口，ROADMAP 下一项为 TODO-03H6，保留本计划直到新阶段正式启动。
- 用户授权的独立子代理初审和 Main 针对唯一 P2 的补修复核完成后，更新 `ARCHITECTURE.md` 稳定事实并在 `ROADMAP-archive.md` 追加 C 收口记录；不将尚无具体资产明细的 readback 债务写成已关闭。
- 完成标准：六个消费者全部移除旧调速逻辑，专属契约保全，编译、专项 Automation、适用 readback／PIE 和终审均有明确证据。仅完成 Light 不将 07B8-C 标为完成。
- 实际延期项在 ROADMAP 留唯一记录和闭环触发条件；既有 `Debt-07B8-AuthoredValidation` 等历史债务不因此次静态规划自动关闭。下一阶段仍为 TODO-03H6。

### 5.3 停止条件与 Git 门禁

- 如发现必须修改共享 helper／Notify、Enemy、资产、Build.cs 或其他清单外路径，保留证据并交 Main 决策，不自行扩展。若会改变既定 GAS／动作生命周期契约，由 Main 向用户收敛决策。
- 相同失败最多一次有依据的修复和一次定向重跑；同根因重复失败时停止并报告首个错误与修复结果，不进行无依据的循环尝试。
- Git 提交仍需用户单独批准；仅暂存批准路径，核验 staged scope，排除既有 Content／Config 和其他用户 WIP，不使用 `git add -A`。

## 6. Light 试点补修验收与 MeleeSkill 交接（历史记录，2026-09-12）

- 实际 Source／Test 范围：`LightAttackAbility.h/.cpp`、新增 `PlayerMontageRateWindowAutomationTests.cpp`，以及 `PlayerMeleeMotionWarpingAutomationTests.cpp` 的单行逻辑夹具适配。其他五类生产文件、共享 helper／Notify 和 Enemy 未修改。
- 上轮 P1 已修复：生产 `EndAbility()` 不再提前设置 `bIsActive = false`，测试 bypass setter 也不再改写 Ability 活跃状态；GAS 结束流程由父类正常执行。
- 上轮 P2 已补齐：集成测试通过真实 ASC `TryActivateAbility()` 激活，使用生产 Combo 事件完成 Entry 0 → 1 → 2，检查真实 Montage 播放率、旧 Context 隔离、ASC Cancel 后的 Ability／Spec／owned tags 清理及再次激活；该集成段不手动设置 Active，不开启 bypass。
- 测试证据边界：非 1 baseline 用例在真实 Montage 上先解除 helper 绑定、设置实际速率后显式重新捕获，不宣称生产启动时直接以 1.25 播放；正常结束用例手工广播 `OnMontageEnded`，验证 GAS 结束接线，不宣称其由动画时间轴自然推进触发。手工 Notify 调用同样不冒充引擎时间轴调度。
- 用户验证：本次用户明确确认“补修后编译、两组 Automation 和 PIE 均已通过”；两组为 `PolyQuest.Combat.PlayerMontageRateWindow`、`PolyQuest.Combat.PlayerMeleeMotionWarping`。记录为用户执行证据，不声称 Main 运行；未单独提供编译方式／日志及 authored 资产 readback 收据，不补写这些细节，既有收据边界仍由 `Debt-07B8-AuthoredValidation` 归口。
- Main 静态与复核：批准路径 diff、测试新集成段及原有单行夹具适配已核对，`git diff --check` 和未跟踪测试文件行尾空白检查通过。本次为上轮两个问题的定向复核，未发现新的 P0-P2；共享 helper／Notify 未改变，既有一跳 GAS 契约已核实，重复图查询 skipped。
- 交接结论：Light 补修问题与本次用户验证闭环，允许按既定白名单进入 MeleeSkill。不得为新消费者复制手动控制 GAS 活跃状态的旧补丁；保留“播放确认 → Commit 成功 → 窗口启用”的专属顺序。
- 当时的下一步是 MeleeSkill → Sprint → Charged → Bow → Dodge；本轮用户已改为由 Codex 完成剩余实施，最新交接见第 7 节。C 整体验收、文档归档与 Git 提交尚未完成。

## 7. 六类实现交付与 Fresh Review 交接（2026-09-12）

### 7.1 最终实现范围

- 六类已复用 `FAbilityMontageRateWindowLifecycle`，删除原计数／布尔调速状态与固定恢复 `1.0` 的实现。Light 是此前已接受的生产改动，本轮补强其测试；本轮新实施 MeleeSkill、Sprint、Charged、Bow、Dodge。
- 后五类在已确认的播放上捕获 baseline，并建立 transient Context、绑定 token、真实 Montage instance ID 和 exact-tag Begin／End 任务。回调验证当前 Context／代次、Ability／Avatar 状态和 `GetActiveInstanceForMontage()` 的 ID；新建等待任务的每个激活返回点防御同步结束或换代。
- `ClearRateWindow()` 先让 Context 失效、移除监听，再只对仍持有的当前实例恢复 baseline，最后清空 helper 与快照。`EndAbility()` 沿用 GAS 父类清理，不新增手动改写 GAS 活跃状态的生产补丁。原有其他事件、Tag 和动作所有权保持原契约。
- MeleeSkill 保持播放确认 → Commit 成功 → RateWindow 绑定；Charged 的 Pause／Resume 与 Bow 同实例 Section 跳转不重捕获；Dodge 保留原实例任务回调、取消窗口与真实 ASC 重触发路径。
- `PlayerActionWindowAutomationTests.cpp` 中无真实绑定的 Bow／Dodge 旧 RateWindow seam 用例由新的消费者专项接替，其他动作窗口断言保留；`PlayerMeleeMotionWarpingAutomationTests.cpp` 保留此前单行 Light 逻辑夹具适配。
- Source／Test 差异共第 2 节的 15 个路径。新增测试文件仍是 Git untracked，审查时须显式读取，不能只看 `git diff`。实施交付时文档同步本计划与 ROADMAP；审查收口后补齐 ARCHITECTURE 和 ROADMAP-archive。Content／Config WIP 不纳入交付，未暂存或提交。

### 7.2 测试覆盖与证据边界

- 专项前缀 `PolyQuest.Combat.PlayerMontageRateWindow` 现有 `.Light`、`.MeleeSkill`、`.Sprint`、`.Charged`、`.Bow`、`.Dodge` 六个叶子测试。原 Light 同名根测试改为 `.Light`，避免前缀下新增子项后被 Automation 叶子筛选遗漏。
- 真实链路：ASC `GiveAbility/TryActivateAbility` → 生产 Montage／WaitGameplayEvent → 手工调用真实 Notify → Context → helper → 实际 `Montage_GetPlayRate`。后五类不开启 Montage bypass，不手动设置 Ability／Spec Active。覆盖交叉、嵌套、两种相邻交接、重复／未知 Begin／End、非法倍率、错误 Avatar／来源／身份、合法内嵌 Sequence 及外来 Sequence 拒绝。
- 生命周期：活跃窗口下完成委托、ASC Cancel、真实 Montage Stop 后实例更新、Actor Destroy、旧 Context 回调、重复 RateWindow 清理；MeleeSkill 覆盖现有 UnPossess selector。替换实例测试在旧窗口活跃时重播同一资产，再调用旧 Context 与 RateWindow 清理，断言新实例仍为 `2.0`；该用例专门隔离调速写入，不宣称重构了各 Ability 原有的 Montage Stop 权属。
- 专属路径：Light Entry 0／1／2 真实事件接入及换代；MeleeSkill 使用独立 Player／ASC 夹具在 CanActivate 之后令体力归零，触发真实 Commit 失败；Charged 暂停中 End 不 Resume、释放后 baseline 不重捕获；Bow 提前松手与 Release Section 跳转；Dodge 真实 ASC 重触发。各类覆盖空 Montage 启动失败。
- 非 1 baseline 用例在真实播放实例上显式重绑 helper 后捕获 `1.25`，不声称生产初始播放率直接配置为 `1.25`。完成委托用例触发生产委托接线；Stop 用例更新停止实例以触发原生结束事件；二者不替代引擎跨帧自然播放／Queued Notify 时间轴验收。Notify 测试也不证明真实资产上的窗口布局、Section 边界或帧率表现。
- 适用性：当前 `APlayerCharacter::OnHealthAttributeChanged()` 在致死时直接返回，尚无 Player 死亡流程。本阶段以已有 Cancel／Destroy／UnPossess 入口验证清理，不伪造“死亡自动取消已验证”，也不在 RateWindow 迁移中新增死亡系统。将来接入死亡流程时须验证其统一 Cancel／EndPlay 收口；路线图既有死亡阶段负责该入口。

### 7.3 编译、Automation 与实施自检

- 集成交付构建（P2 补修前）：Codex 执行 `Build.bat PolyQuestEditor Win64 Development -Project=E:\GameDevelop\PolyQuest\PolyQuest.uproject -WaitMutex -NoHotReloadFromIDE`，结果 `Succeeded`，日志 `Saved/Logs/Codex-07B8C-BuildFinal.log`。编译存在既有 UE 5.8 弃用警告（AbilityTags、测试用 SequenceLength 等），不宣称零 warning；补修后构建见第 7.5 节。
- 集成交付 Automation（P2 补修前）：Codex 使用 `UnrealEditor-Cmd.exe`、`-unattended -nop4 -NullRHI -nosound -nosplash`、`-TestExit="Automation Test Queue Empty"` 运行下列组合。`Saved/Automation/Codex07B8C/Final/index.json` 中 **12/12 state=Success，failed=0，notRun=0**；3 项无警告、9 项带警告。警告来自测试用空 Cooldown GE、故意的启动失败／无效 baseline 等夹具，逐项未记录 Error。完整日志：`Saved/Logs/Codex-07B8C-Final.log`；补修后影响范围的定向回归见第 7.5 节。

| 最终选择器 | 测试数 | 结果 |
|---|---:|---|
| `PolyQuest.Combat.PlayerMontageRateWindow` | 6 | 全部 Success |
| `PolyQuest.Player.ActionWindows` | 1 | Success |
| `PolyQuest.Combat.PlayerMeleeMotionWarping` | 1 | Success |
| `PolyQuest.Player.MobileBow` | 1 | Success |
| `PolyQuest.Combat.ChargedAttackNiagaraFeedback` | 1 | Success |
| `PolyQuest.Combat.EnemyMontageRateWindow` | 1 | Success |
| `PolyQuest.Combat.EnemyStanceBreakRateWindow` | 1 | Success |

以上选择器通过 `+` 连接到 `-ExecCmds="Automation RunTests ..."`。构建和测试均为本轮 Codex 实际执行证据；用户此前确认的 Light PIE 不外推到新增五类。

- 最终 `git diff --check -- Source/ plan.md ROADMAP.md` 和未跟踪测试文件行尾空白检查通过；staged diff 为空。第 2 节 15 个 Source／Test 路径均在白名单内，共享 helper／Notify、Enemy、Tag 与 Build.cs 无本阶段差异。
- 初次专项的 Bow／Charged 失败来自输入夹具启动了独立 PrimaryAttack router，导致 Tag 基线错误；隔离 router 后六类通过。补强 Stop 用例后，MeleeSkill／Sprint／Charged 需要一次停止实例更新才触发结束委托，已依据 UE 5.8 时序修正。后续 MeleeSkill 零体力负向测试的 Exhausted 状态影响了下一次激活；一次无跨帧 Tick 的恢复尝试未解决，最终改为独立 Player／ASC 夹具，保留真实虚弱语义。
- 首次失败和后续报告全部保留在 `Saved/Automation/Codex07B8C/`，不覆盖失败证据。最终以测试报告内每项 `state` 和错误数判定，不能以 UnrealEditor-Cmd 的退出码单独判定成功。
- 实施自检聚焦批准 diff、绑定位置、Context 弱引用／失效、每个 RateWindow ReadyForActivation 返回点、当前实例写入授权和唯一 EndAbility 清理。后五类绑定实现逐项核对，没有另加共享框架；随后进行的独立 Fresh Review 及 P2 补修见第 7.4／7.5 节。
- CodeGraph 用于定向导航，裁剪部分回退到聚焦源码。code-review-graph 已在匹配 HEAD 的索引上查询 15 个批准文件；其动态调用／测试关系覆盖不完整，风险分数与自动 test-gap 数不作为测试结论。Rider `lint_files` 返回空 `items`，不据此声称逐文件无错误；编译日志和直接 diff／空白检查构成实际证据。

### 7.4 用户 PIE 确认与本轮独立 Fresh Review

- Fresh Review：相对 HEAD `9ae684cea97e682bc98c58c8776dc0f61639719c` 检查第 2 节 15 个 Source／Test 路径，并显式读取未跟踪测试文件；以当前源码和最终报告为准。优先核对启动／Commit 顺序、Paused 实例、同资产新实例、旧 Context、同步重入和 GAS 父类 EndAbility 清理。不要将无关 Content／Config WIP 归因于本次代码。
- 用户 PIE：2026-09-12 用户在收到 Scene01 最小验证清单后明确回复“测试通过，允许你派发子代理作为fresh review”。本次确认承接 MeleeSkill／Sprint／Charged／Bow／Dodge 的正常结束和合法中断、Charged Hold／释放、Bow 提前松手／Section、Dodge 重触发、代表性重叠／相邻 Queued 窗口与 30／60 FPS、Enemy Melee 对照。记录为用户确认，不声称 Codex 操作或独立观察了 PIE；Light 既有通过证据继续保留。
- readback 边界：用户未提交实际六类 GA／Montage 路径、窗口位置／倍率／Tick Type 的逐项清单，不补写这些配置值；独立 authored 收据仍归 `Debt-07B8-AuthoredValidation`，不将本次 PIE 确认扩写为 clean-checkout 资产基线或打包证明。
- 独立 Fresh Review 初审：子代理 `/root/ratewindow_fresh_review` 在只读、两批限定审查后发现一个 P2：Light Begin／End／Clear 使用 `GetMontageInstanceForID()` 判断旧实例存在且未停止，未验证它仍为按资产调速的当前目标。同资产以 `bStopAllMontages=false` 重播并保留旧实例时，旧事件或恢复可能污染新实例。其余批准范围未发现其他可证明的 P0-P2；该结论未执行运行时复现。Main 接回已批准的窄修复回路，只修改 Light `.h/.cpp` 和现有专项测试，不再次派发 Reviewer。
- C 的代码、验证和审查结果在下节收口；资产配置收据统一归 `Debt-07B8-AuthoredValidation`。用户现已明确批准本阶段源码／文档提交。

### 7.5 Review P2 补修、验证与最终收口

- 补修仅涉及 `LightAttackAbility.h/.cpp` 与现有 `PlayerMontageRateWindowAutomationTests.cpp`。Light Begin／End／Clear 三处改为查询 `GetActiveInstanceForMontage(ActiveEntryMontage)`，比较返回实例 ID 与绑定 ID 并检查未停止。新增接口仅为测试调用 RateWindow 清理，未改变共享 helper、其他五类生产代码或动作终止语义。
- 新 `.Light` 用例走真实 ASC 激活和实际播放，不开启 bypass；以 `bStopAllMontages=false` 重播同一无 Root Motion 强制停止的临时 Montage，断言旧实例仍存在且未停止、当前实例 ID 已变化。有效旧 Begin、匹配旧 End 和旧 RateWindow 清理都必须保持新实例 `2.0`。
- 补修前实际复现：`Saved/Automation/Codex07B8C/ReviewP2Red/index.json` 记录 `.Light` 失败，旧 Begin／End 将新实例改为 `0.2`，旧清理又改为 `1.0`；旧实例存活和身份变化的前置断言通过。未用无法触发的假设代替缺陷证据。
- 补修后构建：`Saved/Logs/Codex-07B8C-ReviewP2-Build.log` 为 `Succeeded`。定向回归 `PolyQuest.Combat.PlayerMontageRateWindow.Light` 与 `PolyQuest.Combat.PlayerMeleeMotionWarping` 均为 `Success`，`Saved/Automation/Codex07B8C/ReviewP2Green/index.json` 为 2/2、0 失败、0 未运行。其他未受影响消费者沿用第 7.3 节 12 项组合收据，不宣称该组合在补修后全量重跑。
- Main 定向复核确认三处实际写入授权与引擎 API 目标一致，新增用例包含有效载荷与旧实例未停止的前置条件，既有 baseline／换段／取消回归通过。独立初审的唯一 P2 关闭，未发现新增 P0-P2；未再次派发子代理。此前用户 PIE 属于补修前版本，保留其原始归属；本次窄修复仅拒绝对已非当前实例的调速写入，由实际复现和定向回归覆盖，无新资产或操作流程，因此不要求重复整套 PIE。
- C 源码／编译／Automation／用户 PIE／审查收口通过。ARCHITECTURE 更新稳定契约，ROADMAP-archive 追加本阶段记录，ROADMAP 指向 TODO-03H6。`Debt-07B8-AuthoredValidation` 保留独立资产明细收据边界；用户随后批准本阶段 19 个路径提交，排除 Content／Config、tmp 和其他 WIP，不自动启动下一阶段。
