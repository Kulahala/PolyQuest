# TODO-03H9-C：全量接入与仅配置验收（修订版）

> 最近完成交接：原生实现、构建/Automation、源码审查修复、用户最终测试及 2026-09-14 Unreal MCP 合并资产 readback 已完成；A/B/C 与父阶段 03H9 总验收关闭，本凭据随 C 收口提交保存。资产验收摘要见 ROADMAP-archive 的“TODO-03H9：A/B/C 合并资产 readback 与总验收关闭”。下一规划为 03H10，尚未以新计划覆盖本凭据。

## 1. 目标、基线与执行路线

本计划是用户已批准的 C 实施交接，完整替代上一版对话计划及 B 活跃交接。

目标：当前全部适用 Player/Enemy 玩法 Montage 使用 UAbilityTask_PlayActionMontage；普通新动作仅通过标准入口、取消策略及动画窗口配置使用 RateWindow/CancelWindow，不增加专属窗口监听。

- 仓库：E:\GameDevelop\PolyQuest。
- HEAD：931f5de89adf87a432307740ad248259a8718223。
- 引擎：D:\UE\UE_5.8。
- B 固定交接：git show 931f5de:plan.md；核心归档已存在于 ROADMAP-archive.md 的 TODO-03H9-B 条目。
- 启动前已核对真实 AGENTS、源码、配置、直接测试和 Git 状态。Source 与阶段文档无未提交改动，全部 Content/Config/tmp 及其他 WIP 保留。
- A/B 剩余收据仍归 ROADMAP 的各自债务，不以启动 C 核销。
- 2026-09-13：用户已明确批准实施修订版计划；后续按批取得专项收据并授权继续。当前进度见第 12/13 节，构建和 Automation 已授权 Main 执行，资产 readback/PIE 仍由用户完成。

保留现有九参数工厂；特殊停播配置用 Setter。新配置验收复用 B 第 9 节受控动画推进，既有行为断言不得弱化。Enemy 四个旧消费者已有 RateWindow，不视为机械替换。原生 bStopWhenAbilityEnds 只约束特定 OnDestroy 路径，不能代表本片的完整停播权属选项。

```text
Outer: ue-stage-workflow
Primary: ue5-cpp-gameplay
Support: ponytail:ponytail (lite)
Executor: Main（用户最新指定，原 Luna 已中断）
Execution route: Main 亲自实施，按原批次门禁继续
Main: 契约、源码/测试实施、静态检查、文档、最终验收与提交门禁
Fresh Reviewer: 本次全量实施及验证通过后才派发
Main validation: 命令行 Automation 及所需增量构建，持续至 Batch 4 结束
用户: Editor、资产 readback/PIE 结果、最终提交批准
```

当前用户指定路线覆盖项目默认 Gemini 路线。用户进一步明确：打断 Luna，由 Main 亲自执行，全量通过后再派 Fresh Reviewer；原 Luna 已中断并停止写入。2026-09-14 用户授权 Main 使用命令行运行自动化至 Batch 4 结束，包含运行当前源码所需的增量构建；每批构建与专项成功后自主继续，资产边界及最终提交批准规则不变。

历史流程记录（已由上方最新指令替代实施分工）：2026-09-13 用户追加流程指令：后续源码/测试实施及修复全部由原 Luna 执行，Main 负责静态检查和阶段文档；实施过程中不派发 reviewer 或自审子代理，本次全量工作完成后再派发 Fresh Reviewer。此指令优先于 AGENTS/Skill 的默认首次自审派发规则；已发生的历史自审记录保留，不重复执行。

非目标：03C、伤害或消耗重设计、新框架、新 Ability 基类、运行时配置扫描、具体 Ability 注册表、Tag/Config/Build.cs 修改、生产资产写入及文件物理删除。

## 2. 全量清单与批次门禁

本片启动基线为 20 个原生玩法 Ability 中 3 个已接入、17 个待迁移。表内省略 U 前缀及 Ability 后缀；所有条目最终支持 RateWindow，None 仅禁用取消权限。

| 批次 | 消费者 | 取消策略与边界 |
|---|---|---|
| 已接入，只回归 | SprintAttack、ChargedAttack | DodgeAndDefense；Charged 暂停结算不改 |
| 已接入，只回归 | EnemySmallHitReaction | None |
| Batch 1A（已交付，用户专项通过） | PlayerSmallHitReaction、PlayerGuard、PlayerGuardBreak、PlayerParry | None；补 RateWindow，保留播放确认、效果和完成时点 |
| Batch 1B（已交付，用户专项通过） | EnemyHitReaction、EnemyMelee、EnemyLaunchReaction、EnemyStanceBreak | None；迁移已有 RateWindow，保留受击、攻击、Root Motion 和 StanceBreak 生命周期 |
| Batch 2 | LightAttack、BowDrawFire、PlayerMeleeSkill | DodgeAndDefense |
| Batch 2 | Dodge、PlayerBigHitReaction、PlayerLaunchReaction | DodgeOnly |
| Batch 3 | PlayerFrontExecution、PlayerBackstabExecution | None；0.2s 停播混出 |
| Batch 3 | EnemyVictimExecution | None；保留用户已确认的独立表现停播权属 |
| Batch 4 | 新最小测试动作、配置诊断、全量对账 | 两份 Montage 仅配置验收与父阶段收据对账 |

- Batch 1A、1B 分别检查。Batch 2 依次 Light、Bow/MeleeSkill、Dodge/Big/Launch，每组同步适配直接测试。
- 每个正式批次交付前完成范围、实施自查和 Main 静态检查，不派 reviewer；Development Editor 构建及适用 Automation 成功后才进入下一批。按用户最新授权，Main 命令行执行并留存报告，不再等待用户重复运行。Batch 1A 的首次编译检查点已通过。
- 混出 Setter 因 Bow 原有 0.1s 停播已获批提前至 Batch 2。Batch 3 先完成剩余停播权属 Setter 及测试，再迁移处决三方。
- Batch 4 不积压前面批次的编译错误或旧测试适配。
- Main 按当前批白名单、冻结契约和已确认结果分批继续；不恢复已中断的执行代理。
- 批次检查点不等于 Git 提交授权。任何提交都须用户单独批准。
- 未完成消费者始终留在 C 采用清单，不转成后续逐动作 TODO。

## 3. 冻结实现契约

### 3.1 标准入口与窗口

保持 PlayActionMontage 现有九参数签名。显式 Root Motion 缩放、起播时间和取消策略写参数名注释。真实激活测试保留起播位置与 Root Motion 缩放断言，防止 B-R1 重现。

普通消费者移除纯窗口 Context、Begin/End WaitTask、绑定代次和权限布尔副本。业务 Session、Trace、输入、运动和处决上下文保留原 Ability 所有权。

- RateWindow：绝对速率；最后开始且仍有效窗口优先；退出恢复基准；保留来源动画/Notify/AnimInstance/InstanceID 授权。
- CancelWindow：窗口并集；逐 Tag 自有贡献；外部贡献保留；清理幂等。
- 沿用 Event.Action.RateWindow.*、Event.Action.CancelWindow.Dodge.*、State.Action.CanCancel.* 和现有取消标记，不增加 Tag。
- GAS 保留激活、阻止、Commit、实际取消权属；不移动消耗，不通过新增标记改变旧取消路线。
- 结果回调先绑定再 ReadyForActivation；返回后校验 Active、结束状态、对象有效性及当前 Task 身份。不得复活结束动作、恢复旧 Task 或让旧回调结束新实例。
- 普通退出统一经过 Ability EndAbility；Task 清理自身窗口与播放资源，旧实例不能影响新实例。
- 保留 Notify 名称、反射属性、资产引用及 B 来源协议；不改已验证暂停算法。

### 3.2 特殊停播（混出 Setter 已批准提前至 Batch 2，其余 Batch 3）

Task 增加两个激活前 C++ Setter，不增加工厂位置参数：

```cpp
bool SetTaskOwnsMontageStop(bool bOwnsStop);
bool SetOverrideBlendOutTime(float InOverrideBlendOutTime);
```

- 默认 bTaskOwnsMontageStop=true、OverrideBlendOutTime=0.0f。
- 激活开始后或已结束拒绝修改，返回 false 并诊断，保留原配置。
- 混出接受有限值，负值沿用 UE 原生资产 BlendOut。NaN/Inf 返回 false、保留原值，不新增播放 OnFailed 分支。
- Front/Backstab 激活前设 0.2f，移除原重复停播。
- Victim 激活前设停播权属 false，保留 bAllowInterruptAfterBlendOut=true；覆盖取消、失败清理、显式 EndTask 等所有 Task 主动停播路径，但继续窗口、委托、Tag 与适用 Root Motion 清理。
- Victim 保留原实例级表现清理、自然尾段、异常停止和启动重入残留保护。公共 Task 不按类名识别它。
- Setter 不改变结果委托语义；分别断言停播命令和结束结果。

### 3.3 动作差异

- Light 每段独立 Task；换段前解绑旧结果并清理旧窗口，旧 Task 暂留异常停播责任，待新 Montage 按原生 BlendIn 接管后 EndTask；保留缓冲、分支和 Motion Warping。
- Bow 保留 Draw/Hold/Release、动态 Charging、移动效果、发射与自然 BlendOut 收尾。
- Dodge 保留原生重激活、Commit 成功后取消源动作；Big/Launch 不授予 Defense。
- Launch 按当前 RootMotionKnockdown 迁移，不恢复历史 Takeoff/LandingRecovery 状态机。
- Guard/GuardBreak 保留播放确认后的效果/移动操作；Parry 仅自然完成提交冷却。
- Batch 1A 源码核对：Guard/GuardBreak/Parry 的业务退出原本等待完整混出结束，继续使用原全局 OnMontageEnded；标准 Task 负责播放/窗口/停播，不用其混出起点 OnInterrupted 提前结束业务。播放实例 ID 只用于忽略旧播放的延迟结束通知，不复制窗口状态；Small 保留原 Task 结果收尾路线。
- 处决保留握手、Hit/VictimStart、释放、非致死恢复与死亡收尾。

### 3.4 配置验收（Batch 4）

复用现有离线检查：先确认真实标准 Task 和 Montage，再检查取消策略；None 不能豁免入口检查。显式 None 且无 Cancel Notify 合法；缺入口、策略不符、缺窗口、标记或阻止冲突须输出 Ability、Montage、目标、原因。

普通新动作检查现有 CancelableBy 标记；旧显式取消路线由用例声明必要源 Tag，并以真实 GAS 证明；不在检查函数维护具体类名表。

现有测试支持文件新增 UTestConfigurationOnlyActionAbility，仅配置、标准播放入口和结束回调，不含窗口监听。现有专项文件新增 PolyQuest.Combat.ManagedMontageConfigurationOnly；同一动作更换两份 Montage 后重复全套验收。

将 ManagedMontageCancelWindowAutomationTests.cpp 第 9 节受控推进抽为同文件私有辅助，供原第 9 节及新用例复用：
- 保存并关闭 Mesh/CharacterMovement 自动 tick，退出恢复；
- 每步最多 0.05s，推进 World 后刷新 Slot 双缓冲权重；
- 每步一次 TickMontageOnly + DispatchQueuedAnimEvents，唯一动画推进者；
- 保存/恢复临时 tick/pose 状态，保留 Begin/End、来源和实例收据；
- 不改原前八节推进方式，不改全项目公共 fixture；
- 正向不得手工 Begin/End、直接加权限 Tag 或 bypass 冒充原生派发。

### 3.5 测试断言纪律

允许更新 Task 类型、访问器和准备方式。被删除 Context 的拒绝断言迁为等价旧 Task/实例拒绝断言，并报告映射；不得保留无意义旧类型。不得注释、跳过、放宽期望或用“不崩溃”替代行为断言。

## 4. Approved Paths

以下为封闭展开规则，总计 54 个源码/测试文件，仅激活当前批必要子集；不要求每个文件都产生 diff。

### 4.1 消费者 34 个文件

第 2 节 17 个待迁移名称分别补 Ability 后缀，精确展开：
- Source/PolyQuest/Public/AbilitySystem/Abilities/<名称>Ability.h
- Source/PolyQuest/Private/AbilitySystem/Abilities/<名称>Ability.cpp

仅 Task 类型/创建、回调、窗口迁移、直接初始化/清理和测试访问器。三个已接入试点源文件不授权修改。

### 4.2 公共 Task 两文件（混出 Setter 为 Batch 2 已批准依赖）
- Source/PolyQuest/Public/AbilitySystem/Tasks/AbilityTask_PlayActionMontage.h
- Source/PolyQuest/Private/AbilitySystem/Tasks/AbilityTask_PlayActionMontage.cpp

仅 Setter、配置冻结、停播分支、必要测试访问器。

### 4.3 测试辅助两文件
- Source/PolyQuest/Private/Tests/TestManagedMontageAbility.h
- Source/PolyQuest/Private/Tests/TestManagedMontageAbility.cpp

### 4.4 测试 16 文件

以下均位于 Source/PolyQuest/Private/Tests/：
- ManagedMontageRateWindowAutomationTests.cpp
- ManagedMontageCancelWindowAutomationTests.cpp
- PlayerMontageRateWindowAutomationTests.cpp
- EnemyMontageRateWindowAutomationTests.cpp
- EnemyStanceBreakRateWindowAutomationTests.cpp
- PlayerActionWindowAutomationTests.cpp
- PlayerLaunchReactionRootMotionAutomationTests.cpp
- PlayerMobileBowAutomationTests.cpp
- HitReactionAutomationTests.cpp
- FrontExecutionAutomationTests.cpp
- BackstabExecutionAutomationTests.cpp
- ExecutionHitNotifyAutomationTests.cpp
- ExecutionVictimPresentationAutomationTests.cpp
- ExecutionVictimRootMotionAutomationTests.cpp
- ExecutionReleaseOutcomesAutomationTests.cpp
- ExecutionLethalRecoveryAutomationTests.cpp

### 4.5 Main 文档
plan.md、ROADMAP.md、ARCHITECTURE.md、ROADMAP-archive.md；均由 Main 维护。稳定架构事实仅验证后更新。旧私有 helper 物理删除不纳入本片。

### 4.6 Batch 1A 已交付子集

消费者为 PlayerSmallHitReaction、PlayerGuard、PlayerGuardBreak、PlayerParry 的上述 8 个 .h/.cpp。
直接测试仅激活：
- Source/PolyQuest/Private/Tests/HitReactionAutomationTests.cpp
- Source/PolyQuest/Private/Tests/PlayerActionWindowAutomationTests.cpp
- Source/PolyQuest/Private/Tests/ManagedMontageRateWindowAutomationTests.cpp
- Source/PolyQuest/Private/Tests/TestManagedMontageAbility.h
- Source/PolyQuest/Private/Tests/TestManagedMontageAbility.cpp

Batch 1A 已获用户专项复验 Success 和继续推进授权，现有改动保留。本批不回改已交付 Player 消费者，不因测试替代 GE 的冷却 Tag 提示扩展生产冷却配置。

### 4.7 Batch 1B 已交付子集

2026-09-14 R1 单字段扩围已获用户明确批准：额外允许修改 `Source/PolyQuest/Public/AI/EnemyAIController.h`，仅将 `MeleeAttackCooldownEndTime` 从 float 改为 double，以匹配 UE 世界时间精度。其他 AI 字段与实现不纳入写入范围。

生产仅激活以下 8 文件（仓库相对路径）：
- Source/PolyQuest/Public/AbilitySystem/Abilities/EnemyHitReactionAbility.h
- Source/PolyQuest/Private/AbilitySystem/Abilities/EnemyHitReactionAbility.cpp
- Source/PolyQuest/Public/AbilitySystem/Abilities/EnemyMeleeAbility.h
- Source/PolyQuest/Private/AbilitySystem/Abilities/EnemyMeleeAbility.cpp
- Source/PolyQuest/Public/AbilitySystem/Abilities/EnemyLaunchReactionAbility.h
- Source/PolyQuest/Private/AbilitySystem/Abilities/EnemyLaunchReactionAbility.cpp
- Source/PolyQuest/Public/AbilitySystem/Abilities/EnemyStanceBreakAbility.h
- Source/PolyQuest/Private/AbilitySystem/Abilities/EnemyStanceBreakAbility.cpp

直接测试仅激活：EnemyMontageRateWindowAutomationTests.cpp、EnemyStanceBreakRateWindowAutomationTests.cpp、HitReactionAutomationTests.cpp、ManagedMontageRateWindowAutomationTests.cpp，均位于 Source/PolyQuest/Private/Tests/。只修改实际依赖本批消费者的测试段；保留已通过 Batch 1A 的测试段。其他测试可只读检查依赖，不自动扩大写入路径。

迁移已有 RateWindow，不新建窗口 Context 或平行生命周期；Task 的 None 策略不禁用 RateWindow。StanceBreak Context 若同时承载业务结束代次，保留必要业务部分，仅去掉窗口职责。基线依赖完整混出结束的业务不能替换成 Task 的混出起点 OnInterrupted；保留原始结束时点与实例隔离。旧窗口 Context/WaitTask 断言必须映射到真实标准 Task/实例的等价语义，禁止为凑旧测试保留空壳、静态可写假生命周期或降低断言。

本批直接验证入口为 ManagedMontageAdoptionBatch1B、EnemyMontageRateWindow、EnemyStanceBreakRateWindow、HitReaction，以及现有 EnemyLaunchReactionRootMotion（只读测试文件）；必要的原生派发/标准入口证明可在已批准测试文件内补齐，复用既有受控推进，不新建公共 fixture。公共 Task、Batch 2/3/4、资产/Config/Tag/Build.cs 不在当前写入范围。Main 实施、单兵自查和静态检查，不派 reviewer；用户本批构建/Automation 确认后才进入 Batch 2。

## 5. 验证、停止与收口

### 当前 Batch 2 激活范围（2026-09-14）

用户已确认 Batch 1B R1 的 EnemyMontageRateWindow 与 ManagedMontageAdoptionBatch1B 均 Success，并授权继续。结合此前三个 Success，Batch 1B 五项专项收据齐全；完整构建对账继续归最终验证债务。

按第 2 节顺序推进 Light → Bow/MeleeSkill → Dodge/Big/Launch，Main 单兵实施、自查及静态检查。写入仅限这六个消费者的 12 个 .h/.cpp，以及第 4.4 节中的 PlayerMontageRateWindowAutomationTests.cpp、PlayerActionWindowAutomationTests.cpp、PlayerLaunchReactionRootMotionAutomationTests.cpp、PlayerMobileBowAutomationTests.cpp、ManagedMontageRateWindowAutomationTests.cpp、ManagedMontageCancelWindowAutomationTests.cpp 和第 4.3 节测试辅助文件。每组先完成直接测试适配与静态检查；已通过 Batch 1A/1B 源码保持。PlayerMeleeMotionWarpingAutomationTests.cpp 为只读兼容回归，不纳入写入范围。用户随后批准 SetOverrideBlendOutTime 及直接测试提前到 Bow/MeleeSkill 组（第 9 节）；停播权属 Setter、处决与配置验收仍留各自批次。

| 验证面 | 必须取得的结果 |
|---|---|
| 每批静态 | 实际路径、参数核对、适用 Rider 诊断、精确 git diff --check、Main 实施自查及静态检查 |
| 每批构建/回归 | 用户 Development Editor、直接 Automation；通过前不叠加下一批 |
| 配置正向 | 真实 ASC/动画改速恢复、窗外拒绝、窗内取消、CanActivate/Commit 失败保留动作、自然/取消/销毁清理；换 Montage 重跑 |
| 配置负向 | None 漏入口、策略、窗口、标记、目标阻止及提前取消冲突可定位 |
| 公共回归 | Managed Rate/Cancel、Player Rate 全组、Enemy Rate、StanceBreak、ActionWindows；并集、暂停、来源、重入、外部贡献 |
| 特殊停播 | 默认 0、0.2、负值资产配置、激活后 Setter 拒绝；Victim 自然尾段/异常/启动重入/新旧实例隔离 |
| 最终构建 | 用户非 Editor Win64 Development，公共接口和测试宏组合 |
| 最终审查 | 全量实施完成后派 Fresh Reviewer，Main 验收；无未关闭 P0-P2，全量采用与债务对账 |

Editor 写入清单为空。内存测试不保存包、不修改全局 CDO。

用户 readback：实际 GA 授予/配置到 Montage，Combo/武器数据、四向选择、处决双方、Skeleton/Slot/Section、两类 Notify 的类型/数值/区间/派发方式；确认蓝图无遗漏播放入口。代表性 PIE 覆盖各动作组的低体力、打断/死亡/重激活、Launch 和处决表现。

清单外代码/蓝图入口或资产补写交 Main 决策，不扩范围，不计为完成。同根因一次修复和一次复验后仍失败，保留首证停止相关回路。

按用户最新指令：原 Luna 已中断，后续批次及修复由 Main 亲自实施并静态检查，中途不派 reviewer；本次全量通过后再派 Fresh Reviewer。

C 完成要求：20 项对账、配置范例、回归、readback/PIE 与 Main 终审齐全。03H9 父阶段还需 A/B 总验收对账；H7/H6 债务保留各自条件。尚未执行的门禁不是失败，也不得写成通过。

## 6. Batch 1A 交付与用户验证记录

2026-09-13：用户批准实施，Main 落盘 C 计划并同步 ROADMAP 当前入口；B 原交接固定到 `git show 931f5de:plan.md`。以下为 Batch 1A 分时交付收据，不代表 C 总验收完成。

- 实际实现：四消费者的 8 个 .h/.cpp 迁至标准 Task，显式 None，保持九参数、Root Motion 与起播位置；移除重复主动停播，保留效果、移动和冷却时点。ReadyForActivation 返回后先检查 Active、结束态和 Task 身份，旧调用不能结束新激活。三个全局 Ended 消费者记录已确认播放的实例 ID，忽略当前实例尚存时收到的旧播放结束通知；统一 EndAbility 清理实例收据。
- 测试：HitReaction 原用例只更新 Small 的 Task 类型/工厂，既有断言保留。ManagedMontageRateWindow 原用例不变，新增 `PolyQuest.Combat.ManagedMontageAdoptionBatch1A`，覆盖四个真实 Ability 的自然完成、窗口内 ASC 取消、自然混出后中断、非零混出中断及取消后立即重激活。验证真实入口/None、原生 Rate Notify 改速恢复、起播位置/Root Motion、效果/移动/状态收尾和 Parry 冷却时点。复用 B 第 9 节的单一动画推进模式，不提前实施 Batch 4 配置验收；只配置本次 Ability 实例与内存 Montage，不改 CDO 或包。
- 分工收据：按用户指定派发 `gpt-5.6-luna / xhigh` 执行者完成生产迁移和旧测试适配；Main 依据 AGENTS 单文件测试例外补齐真实激活用例，并在批准路径内完成窄修。移除本轮重复的手工窗口测试及专用接口，不删除基线语义断言。首次干净只读实施自审由 Main 代派一次，正常与对抗两轮已完成；后续修复由 Main 单兵增量复核，未重复派发。
- 自审与修复：自审发现 C-1A-R1/P1（三防御动作在中断混出起点过早收尾）、C-1A-R2/P2（测试对已离开 ActiveMap 的自然混出实例调用资产级 Montage_Stop，无实际中断）。R1 恢复三者原 Ended 业务时点并新增非零混出状态保持检查；R2 改为按 InstanceID 获取实例，断言自然混出已到达，再调用原生实例 Stop。Main 已核对修复 diff 与 UE 5.8 对应源码；运行验证仍待用户门禁，不能将静态修复写为测试通过。
- 静态收据：当前实际修改 10 个源码/测试文件，均在 Batch 1A 激活清单中；另有 Main 的 plan、ROADMAP、ROADMAP-archive 三份文档。10 个源码文件 Rider errors 为空，修复后受影响 7 文件复查仍为空、无超时；限定路径 git diff --check 通过。Main 用有界图谱补充影响导航，对动态委托和新增测试以实际源码/diff 回退；图谱不计运行时证据。本次为批次静态增量检查，尚非 C 收口 Fresh Review。
- 首次交付时未执行：Development Editor/非 Editor 构建、Automation、Editor readback、PIE；未暂存或提交，未修改 ARCHITECTURE。现有 Content/Config/tmp WIP 保留；初始大范围状态输出被截断，不声称完成全部 WIP 的逐字节对账。后续用户收据如下，不回写成首次交付即通过。

- 用户 Test Run 4：`PolyQuest.Combat.HitReaction`、`PolyQuest.Combat.ManagedMontageRateWindow`、`PolyQuest.Player.ActionWindows` 为 Success；`PolyQuest.Combat.ManagedMontageAdoptionBatch1A` 为 Fail，在 Small / exit 0 尚未播放时因 directional montage selection failed 结束。后续 case 尚未得到执行证明。这三项 Success 按本次实际时点采纳，不要求无关重跑；Development Editor 完整构建尚无单独确认。
- 当前修复 C-1A-R3 已由原 Luna 落盘，仅修改 ManagedMontageRateWindowAutomationTests.cpp：Small 的首次激活与立即重激活共用 HandleGameplayEvent，携带现有 FCombatImpactEffectContext 的前方来向快照；其他三者沿用 TryActivateAbility。根因是原测试未提供 TriggerEventData，零方向被生产四向选择器正确拒绝。交付前 Main 静态检查还去除了临时裸 Actor 的位置依赖，最终不新增测试 Actor，不改生产契约、不弱化断言。最终测试文件 Rider errors 为空，限定 diff --check 通过；待用户重新编译并复验 ManagedMontageAdoptionBatch1A，本根因尚未进行修复后运行复验。Main 仅静态检查，本轮未派 reviewer，未启动 Batch 1B。

用户后续 Test Run 3 确认 `PolyQuest.Combat.ManagedMontageAdoptionBatch1A` Success，并明确“可以继续推进”；C-1A-R3 运行复验闭环，按此明确授权进入 Batch 1B，不再次询问路线或重复已通过专项。该次 CooldownGameplayEffectClass 无授予 Tag 提示来自用作冷却提交计数的 TestMobileBowMoveSpeedGE；该用例证明提交时点，不作为正式冷却 Tag 配置有效性的证明。未单独报告的完整 Development Editor 构建记录与最终资产/PIE 收据不伪造，统一留在 ROADMAP 的 `Debt-03H9-C-Validation` 对账。

## 7. Batch 1B 交接与收据

用户已授权继续；Batch 1B 由原 `gpt-5.6-luna / xhigh` 开始生产迁移，随后按用户最新指令中断，由 Main 接手修复、测试适配、静态检查与文档，不派 reviewer。批准子集见第 4.7 节。初始四消费者生产文件及 Enemy Rate/Stance 专项无当前 diff，Batch 1A 与所有无关 WIP 保留。最新用户运行结果及修复状态见下方 R1；用户已确认修复后两个专项 Success 并授权进入 Batch 2。

2026-09-13 Batch 1B 静态交付（待用户运行验证）：

- 实际本批改动为 11 个源码/测试文件：四个 Enemy 消费者的 8 个 .h/.cpp，加 EnemyMontageRateWindowAutomationTests.cpp、EnemyStanceBreakRateWindowAutomationTests.cpp、ManagedMontageRateWindowAutomationTests.cpp。HitReactionAutomationTests.cpp 本批未改；只读 EnemyLaunchReactionRootMotionAutomationTests.cpp 未改。未扩大公共 Task 或后续批次范围。
- 四消费者均使用标准九参数入口与 None，移除各自纯窗口 Context、WaitTask 和生命周期副本；Melee Trace/HyperArmor、Launch RootMotionKnockdown/韧性延迟结算、StanceBreak 业务 Context/token、移动与 Poise 恢复保持。完整混出后的业务结束仍由全局 Ended 驱动，按真实实例 ID 拒绝旧播放结束影响新实例。
- Main 接手修正：Commit/ReadyForActivation 返回先核验 Active、结束态及本次 Task 身份；Melee 的四个业务 WaitTask 逐个检查同步重入。生产代码从真实 AnimInstance 捕获 ID，未调用测试宏内的 Task getter。Launch 的死亡/销毁路径仍可清理；原 RootMotion 测试入口改为先停止并单步推进对应真实实例、派发结束事件，以兼容新增实例过滤，原断言及测试文件保留。
- 旧断言映射：CDO 不持有窗口 Context → CDO 不持有运行 Task；窗口 Tag getter → Task 在指定 Begin/End Tag 上的实际订阅；窗口 WaitTask 存在/活跃/销毁 → 标准 Task 活跃、两类订阅建立/移除；Melee 旧 Context Begin/End/Clear → 原 Task 所属实例的带来源事件/Task 清理。原 rate、嵌套、来源、并存实例不被旧事件改速、运动、Poise、标签与重入语义断言保持。Stance 专项的 mock 播放改用已有可播放内存 fixture，配置授予实例，不再写全局 CDO；3.5 两份真实双实例、无 bypass 用例保留，并补来源收据。
- 新增 `PolyQuest.Combat.ManagedMontageAdoptionBatch1B`：四个真实 Enemy Ability × 五种退出方式（自然、ASC 取消、自然混出后中断、非零混出中断、立即重激活）。使用与已通过 Batch 1A/B 第 9 节相同的单一动画推进方式，验证原生 Rate Notify 改速/恢复、None 无取消权限、起播位置/Root Motion scale、完整混出业务时点、新旧实例隔离及运动/Poise/标签清理。该测试代码已落盘，尚未运行，不以手工派发或静态检查代替其通过证据。
- 静态收据：11 个本批变更文件 Rider errors 为空；另外 HitReaction 和只读 EnemyLaunchReactionRootMotion 两文件兼容诊断为空，无超时。诊断发现并修正新专项误用不存在的方向 setter，改为读取真实反射属性后配置实例，复查通过。限定 Source/文档路径 git diff --check 通过。Batch 1A 的 8 个生产文件和 HitReaction 测试 SHA-256 与本批开始记录一致；Managed 原 Batch 1A 用例保留，仅新增 Enemy 用例及其 includes。本批自查为 Main 静态检查，未派 reviewer。
- 本批未执行编译、Automation、Editor readback、PIE、暂存或提交；HEAD 保持 931f5de。Content/Config/tmp WIP 未由本批写入，ARCHITECTURE 不更新。当前等待用户 Development Editor 编译及下面五项 Automation，通过后 Main 进入 Batch 2；全量通过后才派 Fresh Reviewer。

本批用户验证命令入口（逐项执行）：

```text
PolyQuest.Combat.ManagedMontageAdoptionBatch1B
PolyQuest.Combat.EnemyMontageRateWindow
PolyQuest.Combat.EnemyStanceBreakRateWindow
PolyQuest.Combat.HitReaction
PolyQuest.Combat.EnemyLaunchReactionRootMotion
```

### Batch 1B R1：2026-09-14 用户失败定位与修复

- 用户 Test Run 3 已确认 EnemyLaunchReactionRootMotion、EnemyStanceBreakRateWindow、HitReaction 为 Success；EnemyMontageRateWindow 和 ManagedMontageAdoptionBatch1B 为 Fail。后者在 Melee / exit 4 提前返回，后续 Launch/Stance 新采用用例尚无运行收据。不把旧专项通过当作新采用用例通过，也不把测试运行推定为完整构建通过。
- EnemyMontageRateWindow 的 H6-F01 三个 case 共用前置遗漏 `FAnimNotifyMontageInstanceContext`，标准 Task 因来源实例为 INDEX_NONE 拒绝开窗。已复用同文件 EventReference 上下文写法补入真实旧实例 ID，并增加窗口数为 1 的前置断言；原 0.5 改速、旧 Begin/End/Clear 不影响新实例等断言保留。此项仍是手工 Notify 白盒验证，不冒称原生动画推进。
- Melee 即时重激活根因：UE 5.8 `UWorld::GetTimeSeconds()` 返回 double，AI 的 `MeleeAttackCooldownEndTime` 却为 float。按本用例三次 0.05f 推进，世界时间为 0.15000000223517418，零冷却截止时间窄化后向上舍入，导致同帧比较误判。已执行独立数值复现，float 版本误判为冷却中、double 版本不会；这不是 UE 运行验证。
- 已在新采用用例增加零冷却同帧可用、Attacking 已清理和目标仍在距离内的精确断言，保留真实 AI→GAS 激活路径与立即重激活断言，不增加等待。用户已明确批准单字段扩围，`Source/PolyQuest/Public/AI/EnemyAIController.h` 的 `MeleeAttackCooldownEndTime` 已改成 double；生产 diff 仅此一行，未改冷却时长、比较规则或其他计时字段。
- 静态收据：两个修复后的测试文件，以及单字段修复后的 EnemyAIController.h 和只读依赖 EnemyAIController.cpp，Rider errors 均为空、无超时；精确 git diff --check 通过。Main 继续单兵检查，未派 reviewer、未编译或运行 Automation、未操作 Editor、未暂存或提交。交付时等待用户 Development Editor 重新编译及两个失败专项复验；随后用户确认 EnemyMontageRateWindow、ManagedMontageAdoptionBatch1B 均 Success，并授权继续。Batch 1B 五项专项收据齐全，完整构建对账仍保留。剩余门禁唯一归口仍为 ROADMAP 的 Debt-03H9-C-Validation。

## 8. 当前 Batch 2 / Light 检查点（2026-09-14）

- 已由 Main 完成 LightAttack 的 .h/.cpp 及 PlayerMontageRateWindowAutomationTests.cpp，共 3 个源码/测试文件。Batch 1A/1B 原有变更文件 SHA-256 与本组开始记录一致。Bow/MeleeSkill、Dodge/Big/Launch 尚未修改，Batch 2 整批未完成；先取得本组编译/直接回归收据，再继续后两组。
- 每段通过标准九参数 Task 播放，明确 DodgeAndDefense、RootMotion scale=1、起播=0；移除纯窗口 Context/token、Rate/Cancel WaitTask、权限布尔副本和重复停播。业务 Trace、输入缓冲、分支成本和 Motion Warping 保持。全局 Ended 仍负责完整混出业务结束，并校验真实实例存活，拒绝旧实例结束新动作。
- 换段前预检失败保留原 Task/窗口；初版在换段开始时结束旧 Task，最终 Fresh Review 发现其零混出回归，已按第 14 节修正为先清理窗口、保留停播责任至原生交接完成。播放失败经 OnFailed→EndAbility 收敛，不能复活已结束旧 Task。Commit、业务 WaitTask 和 ReadyForActivation 返回核验 Active、结束态及本次 Task/业务订阅身份，避免旧栈帧清理同步重激活的新动作。
- 旧测试映射：窗口 Context → 标准 Task；Token 递增/归零 → 不同 Task/实例 ID 以及旧 Task terminated、旧 binding 清除；CDO 窗口 Tag → 实际 Task 的精确 Begin/End 订阅；生命周期引用改为每次读取当前 Task，避免跨段仍检查旧对象。手工 Notify 已补真实实例来源。既有独立数学/MotionWarp fixture 仍在测试宏内使用原有显式 bypass 模式，新运行验收不使用此模式；未保留旧窗口类或可写空生命周期。
- 在 Light 原专项第 7.9 节增加唯一动画推进者（World 分步、每步刷新 Slot 双缓冲、TickMontageOnly + DispatchQueuedAnimEvents），保存/恢复 Mesh/Movement tick 和 pose 状态。使用既有内存 Montage，验证原生 Rate/Cancel Begin/End、窗外无权限、带窗口真实分支换段、旧窗口清理、自然/中断完整混出、ASC 取消、同帧重激活、预检失败保留当前动作及播放失败不恢复旧 Task。未保存包或修改 CDO。
- 静态检查：Light .h/.cpp、修改后的 Player Rate 文件及只读 PlayerMeleeMotionWarping 测试 Rider errors 均为空。首次诊断的测试 const 指针/整数重载错误已修复并复查通过。精确 git diff --check 通过；同文件后续其他消费者测试正文逐字对账保持，原语义断言保留，Light 区段新增边界断言。Main 单兵自查，未派 reviewer；没有编译、Automation 或 PIE 的新通过声明。
- 下一门禁：用户 Development Editor 编译，执行 PolyQuest.Combat.PlayerMontageRateWindow.Light 与 PolyQuest.Combat.PlayerMeleeMotionWarping；通过后按顺序继续 Bow/MeleeSkill，再 Dodge/Big/Launch。最终仍需 Player Rate 全组、ActionWindows 和相关消费者完整回归；所有缺口归 ROADMAP 的 Debt-03H9-C-Validation，不把 Light 检查点记为 C 或 Batch 2 完成。未暂存/提交，ARCHITECTURE 不更新。

- Light 编译修复 R1：用户报告 LightAttackAbility.cpp:527 的 C4458。局部 bCostCommitted 遮蔽 UStaminaActionAbility 的同名成员；仅将局部返回值及其判断改名为 bEntryCostCommitted，保留基类消耗记录及提交时序。Rider errors 为空、精确 git diff --check 通过；此前 IDE 静态检查未检出该编译器遮蔽诊断，不能替代真实编译。等待用户重新编译及上述两个专项结果。

## 9. Batch 2 / Bow-MeleeSkill 启动（2026-09-14）

用户已确认 PlayerMontageRateWindow.Light 与 PlayerMeleeMotionWarping Success 并授权继续；Light 检查点通过，C 的最终构建/资产/审查债务保持。本组 Main 亲自实施 BowDrawFire、PlayerMeleeSkill 的 4 个 .h/.cpp，以及 PlayerMontageRateWindowAutomationTests.cpp、PlayerMobileBowAutomationTests.cpp 和必要的既有 Managed 专项。保留此前通过文件；中途不派 reviewer、不编译、不提交。

发现 Bow 原有 EndAbility 使用 MontageStop(0.1f)，计划仅列出的处决特殊混出不完整。已请求把既定 SetOverrideBlendOutTime 及其测试提前到当前组，以保留 Bow 0.1s；未获批前不修改公共 Task 和 Bow，先推进 MeleeSkill。停播权属 Setter 和处决迁移仍留 Batch 3。

用户已批准提前混出 Setter：本组激活公共 Task 两文件的 SetOverrideBlendOutTime、配置冻结与混出分支，以及直接测试；不提前停播权属 Setter。Bow 直接旧取消窗口测试位于总白名单 PlayerActionWindowAutomationTests.cpp，本组同步适配，保持全部语义断言。


### Bow/MeleeSkill 组静态交付（等待用户编译与运行）

- 实际本组 9 个源码/测试文件：Bow/MeleeSkill 四个 .h/.cpp，标准 Task 两文件，PlayerMontageRateWindowAutomationTests.cpp、PlayerActionWindowAutomationTests.cpp、ManagedMontageRateWindowAutomationTests.cpp。PlayerMobileBow 和 PlayerMeleeMotionWarping 测试只读兼容检查，未修改。Batch 1A/1B 和 Light 的生产文件及其他既有源码 WIP SHA-256 与本组起点一致；两个共享测试文件只改本组相关段，原已通过段保留。
- 两消费者使用九参数标准 Task、DodgeAndDefense、Root Motion scale=1、start=0；移除纯窗口 Context、Rate/Cancel WaitTask、权限副本与重复主动停播。Bow 仍使用 Draw/Hold/Release、移动 GE、Charging 和原结果回调，Task 设置 0.1s；MeleeSkill 保留真实播放确认后唯一 Commit、Trace、Motion Warping，以及全局 Ended 后的完整混出业务收尾，按实例 ID 拒绝旧 End。Ready/Commit 返回检查 Active、结束态与当前 Task 身份。
- SetOverrideBlendOutTime 仅允许激活前配置，激活开始/结束后或 NaN/Inf 拒绝且保留原值；有限负值沿用 UE 5.8 资产 BlendOut 语义。默认 0 不变，不增加工厂参数、不修改结果委托语义、不提前实现 Victim 停播权属。Managed 专项新增真实实例停播的 0/0.1/0.2/负值矩阵，分别检查混出时长、清理和 GAS 取消仍报告 Interrupted，显式 EndTask 不凭空报告 Cancelled；拒绝配置不触发 OnFailed。
- 旧断言映射：Context/Token → 真实 Task/InstanceID；旧回调 → 已注册 ASC 原生委托快照广播至原 Task，由生产终止/来源检查拒绝；旧顺序重绑定 → 同一 InstancedPerActor Ability 经 GAS Cancel/Reactivate 获取独立 Task/实例；无效绑定 → 无 AnimInstance 的真实激活失败；inactive/销毁回调不得重建绑定。保留原改速、负向矩阵、嵌入 Sequence、Bow Section、退出、Commit 失败及新旧实例隔离断言。逐次刷新手工 Notify 的真实实例 ID，负向矩阵补有效来源，避免全部因缺来源被拒绝。Dodge 仍走原 API 和语义分支，生产未迁移。
- Player Rate 的 Bow/MeleeSkill 各补原生动画推进验收：复用 Light 7.9 / B 第 9 节的唯一推进者模式，保存/恢复 tick 与 pose、step≤0.05、Slot 双缓冲、TickMontageOnly + DispatchQueuedAnimEvents。验证原生 Rate/Cancel Begin/End、速率恢复、权限清理、自然结束、ASC 取消、中断混出和立即重激活；真实入口不使用 bypass。ActionWindows 原 Bow 部分仍是隔离的来源校验测试：改为标准 Task 的既有显式 bypass fixture，并补声明 Notify/TargetData，保留原所有断言，不将其当原生推进证据。
- Main 单兵静态检查：六个生产文件及 Player Rate、ActionWindows 共 8 文件 Rider errors 为空；两个只读兼容测试 Rider errors 为空。Rider 初始引用旧 header 的诊断在加载当前 header 后消失；Managed Rate 文件的 IDE 分析返回无法加载文件内容，按聚焦源码、UE 5.8 API 与 diff 检查回退，不宣称此文件 Rider 通过。精确路径 git diff --check 通过；本组未编译、运行 Automation/PIE、操作 Editor、派 reviewer、暂存或提交，ARCHITECTURE 不更新。
- 下一门禁：用户 Development Editor 编译及以下 7 项；通过后 Main 继续 Dodge/Big/Launch。C/Batch 2 未完成，剩余收据唯一归 ROADMAP 的 Debt-03H9-C-Validation。

```text
PolyQuest.Combat.PlayerMontageRateWindow.Bow
PolyQuest.Combat.PlayerMontageRateWindow.MeleeSkill
PolyQuest.Combat.PlayerMontageRateWindow.Dodge
PolyQuest.Player.MobileBow
PolyQuest.Combat.PlayerMeleeMotionWarping
PolyQuest.Player.ActionWindows
PolyQuest.Combat.ManagedMontageRateWindow
```

### Bow/MeleeSkill R1：用户 Test Run 3 失败修复

- 用户确认 5 项 Success：PlayerMontageRateWindow.Dodge、Player.MobileBow、PlayerMeleeMotionWarping、Player.ActionWindows、ManagedMontageRateWindow；Bow/MeleeSkill 为 Fail。Managed 专项的混出矩阵已有用户运行通过收据，之前 Rider 无法加载的历史记录不冒充 IDE 通过。MeleeSkill 在中断退出检查后提前返回，后续用例尚未得到运行证明。
- 本轮仅修改 PlayerMontageRateWindowAutomationTests.cpp。MeleeSkill 的真实 Stop 后原 fixture 直接调用 FAnimMontageInstance::Advance，UE 5.8 Terminate 在清空 Montage 前发送 Ended；不经正常 Montage_Advance 排队时，生产实例有效性防护会拒绝这次过早回调。改为 TickMontageOnly + DispatchQueuedAnimEvents，复用正常结束时序，并增加真实实例存在与收到 Stop 的前置断言，不放宽生产过滤。
- Bow 的原固定 Advance(0.26f) 未证明已越过 0.3s 的 Cancel End。改为每步 0.05s、最多 12 步推进至实际 Montage 位置至少 0.35s，并先断言同一 Task/Ability 仍活跃，再执行原速率、窗口数和权限清理断言；不得通过自然结束后的清理冒充 Notify End。
- 修复后测试文件 Rider errors 为空，限定 git diff --check 通过。Main 单兵静态检查，未编译、运行 Automation、操作 Editor、派 reviewer或提交；生产与其他测试文件保持不变。
- 等待用户重新编译并复验 PlayerMontageRateWindow.Bow、PlayerMontageRateWindow.MeleeSkill，以及共享中断 fixture 的 PlayerMontageRateWindow.Dodge。其余 4 项已通过且本轮未受影响，不要求重复运行。修复后的运行结果未取得，暂不进入 Dodge/Big/Launch；本根因修复加复验回路仍按原停止条件执行。

## 10. Batch 2 / Dodge-Big-Launch 启动

用户后续 Test Run 3 已确认 Bow/MeleeSkill/Dodge 三项均 Success，并明确授权继续；R1 闭环，Bow/MeleeSkill 组专项收据齐全。当前按既定白名单推进 Dodge、PlayerBigHitReaction、PlayerLaunchReaction 六个生产文件及直接测试，统一 DodgeOnly，保留原生 Dodge 重激活、Commit 后取消源动作、Big 四向选择/运动清理与 Launch 当前 RootMotionKnockdown。Main 亲自实施，用户编译与回归为下一门禁，中途不派 reviewer、不提交。

用户本轮另提出项目 AGENTS.md 增补编辑规则，已在核心原则处加入默认 Edit/apply_patch、脚本例外需明确收益和 diff 检查的规定；此为本轮明确授权的文档范围增补。

### Dodge/Big/Launch 组静态交付（等待用户编译与运行）

- 本组实际修改六个生产文件及 PlayerMontageRateWindowAutomationTests.cpp、PlayerActionWindowAutomationTests.cpp、PlayerLaunchReactionRootMotionAutomationTests.cpp。三个消费者均使用九参数 PlayActionMontage、DodgeOnly、Root Motion scale=1、start=0；移除纯窗口 Context、WaitTask、权限副本与重复主动停播。公共 Task 和此前已通过的生产文件 SHA-256 与本组起点一致。全部写入使用 apply_patch；既有 Content/Config、其他源码 WIP 和暂存区未动。
- Dodge 保留原生 bRetriggerInstancedAbility、播放前 Commit、Commit 成功后取消源动作，以及无敌 GE/Notify。Big 保留四向选择、播放确认后的运动和取消操作；Launch 保留当前 RootMotionKnockdown、朝向、剩余 Root Motion 拒绝和 ledge 恢复。Big/Launch 继续以真实实例失效后的全局 Ended 完成业务收尾，拒绝旧实例结束新动作。Ready/Commit 返回检查 Active、结束态和 Task 身份；结果回调在 Ready 前绑定，退出在 EndAbility 解绑并清理。
- 测试映射：旧 Context/token 重绑定断言改为真实 GAS Cancel/Reactivate 的新 Task/实例及旧 ASC 委托快照拒绝验证；Dodge 独立保留真实 ASC retrigger，并增加新动作窗外立即再次请求被拒绝的断言。旧 Cancel 监听接口迁为标准 Task 接收器及有效 Notify/TargetData；原 ActionWindows 来源校验保持显式隔离 fixture，不充当原生动画证据。删除无人调用的旧窗口来源 helper；不保留空壳类型或弱化语义断言。
- Player Rate 的 Dodge/Big 复用已通过的受控单步方式，保存/恢复 Mesh/Movement tick 和 pose 状态、每步最多 0.05s、刷新 Slot 双缓冲、唯一 TickMontageOnly + DispatchQueuedAnimEvents。Dodge 的时间轴明确只授予 Dodge；Big 保留四向、窗外拒绝/窗内取消、CanActivate/Commit 失败保留源动作、自然/取消/跌落/死亡/销毁及旧实例隔离验证。
- Launch 原 RootMotion 测试改为实际 ASC 事件激活和有真实 root track 的内存动画，移除 CDO 写入和播放 bypass 依赖；原候选、朝向、ledge、持久窗口、自然/中断、UnPossess 和销毁断言继续保留。第 9 节新增原生 Rate/Cancel Begin/End（含动画、Notify、实例来源收据）、恢复基准、DodgeOnly、窗外拒绝、两类失败保留源动作、成功 Dodge 取消、完整中断混出、清理和立即同资产重激活。Notify 正向证明不手工派发、不直接加权限 Tag，不保存测试包。
- Main 单兵实施自查完成：九个本组源码/测试文件 Rider errors 均为空；HitReaction 只读兼容检查 errors 为空。中途 Rider endpoint 曾断开，用户恢复后重新检查最终版本，发现并移除 Launch EndAbility 中的旧 bypass 字段残留，复查通过。UE 5.8 的 TriggerAbilityFromGameplayEvent、Root Motion 提取签名和 Montage 结束时序已按引擎源码核对；精确 git diff --check 通过。静态检查不是编译或运行收据；本组尚未编译、运行 Automation/PIE、派 reviewer、暂存或提交，ARCHITECTURE 不更新。
- 下一门禁：用户 Development Editor 编译并运行下面六组；全部通过后继续 Batch 3，当前不提前修改处决或停播权属 Setter。完整采用、仅配置证明、非 Editor 构建、资产 readback/PIE 和最终 Fresh Reviewer 仍归 ROADMAP 的 Debt-03H9-C-Validation。

```text
PolyQuest.Combat.PlayerMontageRateWindow
PolyQuest.Combat.PlayerBigHitReactionWindows
PolyQuest.Combat.PlayerLaunchReactionRootMotion
PolyQuest.Player.ActionWindows
PolyQuest.Combat.HitReaction
PolyQuest.Combat.ManagedMontageCancelWindow
```

### Dodge/Big/Launch R1：重复 DefaultSlot 修复与验收入口纠正

- 用户 Test Run 3 确认 9 项 Success：PlayerMontageRateWindow 下 Bow/Charged/Dodge/Light/MeleeSkill/Sprint 六项，以及 HitReaction、ManagedMontageCancelWindow、ActionWindows；PlayerLaunchReactionRootMotion 为 Fail。首个根因日志为 DefaultSlot 重复，测试在 7.1a 因不存在真实播放实例提前返回，第 8/9 节尚无运行通过证据。
- UE 5.8 UAnimMontage 构造函数已 AddSlot(DefaultSlotName)；本测试的 CreateSyntheticKnockdownMontage 又追加同名 Track。此次只在该公共测试构造函数填入实际轨道前 Reset SlotAnimTracks，并补充单槽及 DefaultSlot 含可播放片段的前置断言，覆盖所有调用该 helper 的场景。生产 Ability/Task 和原有行为断言不变，不过滤重复槽错误、不恢复播放 bypass。
- 上轮交付清单漏列了实际名为 PolyQuest.Combat.PlayerBigHitReactionWindows 的独立专项；它虽位于 PlayerMontageRateWindowAutomationTests.cpp，却不属于 PlayerMontageRateWindow 测试前缀。已纠正文档清单，本次附件没有它的收据，不能算作已通过。
- Rider 对修改后的 Launch 测试 errors 为空；限定 git diff --check 通过。其余已有源码/测试 WIP SHA-256 与本轮起点一致。只修改一个测试文件和 plan/ROADMAP，未编译、运行 Automation、派 reviewer、暂存或提交。
- Main 按用户新授权完成 Development Editor 增量构建（exit 0），随后 UnrealEditor-Cmd 无界面执行 PlayerLaunchReactionRootMotion 和 PlayerBigHitReactionWindows，2 项均 Success、errors=0（报告记为 succeededWithWarnings=2），进程 exit 0。报告：Saved/Automation/TODO-03H9-C-Batch2-R1/index.json；日志：Saved/Logs/TODO-03H9-C-Batch2-R1.log。重复槽 R1 闭环，连同用户已确认九项，Batch 2 专项门禁齐全。

## 11. Batch 3 启动

Main 先完成公共 Task 停播权属 Setter 及直接测试，再迁移 Front/Backstab/Victim 六文件和实际依赖的白名单测试。Front/Backstab 保留 0.2s；Victim 保留独立实例级表现清理。构建与专项由 Main 命令行执行，成功后继续 Batch 4；不派中途 reviewer，不写资产，不提交。

- 公共 Setter 及分离停播/结果矩阵构建成功、ManagedMontageRateWindow Success（Saved/Automation/TODO-03H9-C-Batch3-Setter）。停播权属在统一停播函数检查，关闭后不丢失 GAS Interrupted / ExternalCancel 结果，不增工厂参数。
- 处决三方完成标准入口迁移；Front/Backstab 保留业务 Context 与 0.2s，Victim 删除纯窗口 Context/WaitTask，保留实例级表现停播与启动残留保护。Ready 返回检查当前 Task/业务代次，失败收敛 EndAbility。
- 初次回归暴露五处旧 StanceBreak CDO/空 Montage 准备未随 Batch 1B 适配，LethalRecovery 因握手失败继续解引用空 Context 而崩溃。以现有 TestManagedMontageAbility 支持文件增加真实内存动画/ASC 激活 helper，五个白名单测试复用；保留原语义断言，空 Context 前置断言失败即返回，避免崩溃掩盖原因。不改生产 StanceBreak 或全局公共 fixture。
- Development Editor 构建成功；Saved/Automation/TODO-03H9-C-Batch3-R1/index.json 记录 Backstab、ExecutionHitNotify、ExecutionLethalRecovery、ExecutionReleaseOutcomes、ExecutionVictimPresentation、ExecutionVictimRootMotion、FrontExecution、ManagedMontageAdoptionBatch3 共 8 项 Success、errors=0，命令退出 0。EnemyMontageRateWindow 在初次运行中已 Success。新增接入测试真实播放三方，原生 Rate Begin/End、默认 None、Root Motion scale=1/start=0，以及玩家 0.2s 混出与恢复清理。
- 额外回归 ExecutionImpactFeedback 暴露同样旧 StanceBreak 准备导致的连锁失败；该文件不在原 54 文件清单，已提交具体范围审批，未写入，不计通过。其原始日志为 Saved/Logs/TODO-03H9-C-Batch3.log。批准范围内 Batch 3 门禁通过，继续独立 Batch 4；C 最终仍受此缺口约束。

## 12. Batch 4 启动

在 TestManagedMontageAbility 两文件与 ManagedMontageCancelWindowAutomationTests.cpp 完成仅配置动作、标准入口/策略诊断、两份 Montage 全量验收；同文件抽取第 9 节受控推进方式。不修改前八节动画推进、不修改生产资产。全量回归与非 Editor 构建留存命令行报告，资产 readback/PIE 仍为用户证据。

### Batch 4 实施与运行收据（2026-09-14）

- 用户已批准追加五份测试文件的限定修复：Source/PolyQuest/Private/Tests/ExecutionImpactFeedbackAutomationTests.cpp 仅复用已验证 StanceBreak 激活 fixture；同目录 ExecutionSnapAlignmentAutomationTests.cpp、ProjectileTargetAssistAutomationTests.cpp、ProjectileFlightTrailAutomationTests.cpp、ProjectileLifecycleAutomationTests.cpp 仅补 <limits> 并替换报错 INFINITY 常量。保留全部行为断言，不扩大生产代码范围。下文“待批”为此次批准前的历史记录，以本条及后续收据为准。

- 已增加最小配置动作：仅配置、标准九参数播放入口、结果绑定和 EndAbility 清理，无窗口专属监听。离线检查先确认真实 Task、宿主和 Montage，再检查策略；None 不豁免标准入口，既有显式取消路线由调用方声明源 Tag，不维护类名表。
- 第 9 节与新增专项共用同文件私有受控推进 helper，保存/恢复 Mesh、Movement tick 与 pose 状态，每步不超过 0.05s，刷新 Slot 双缓冲，每步唯一 TickMontageOnly + DispatchQueuedAnimEvents；前八节推进方式不改。
- 同一配置动作更换两份独立创建的内存 Montage，取得实际 Rate/Cancel Begin/End 和实例来源收据，覆盖改速恢复、窗外拒绝、窗内真实 GAS 取消、CanActivate/CommitCheck 失败保留源动作、自然/取消/销毁清理，以及入口/策略/窗口/标记/目标冲突诊断。处决业务 Hit/VictimStart 采用真实 ASC 事件，不冒称业务 Notify 原生派发；窗口正向验收采用原生动画派发。
- 新专项调试中修复旧诊断用例缺 ActorInfo、第二份 Montage 复制后窗口表现异常，以及临时 Outer 误实例化抽象 UObject 导致 ensure。第二份改为独立构建并以测试 Player 为 Outer；不忽略 ensure 或放宽行为断言。Saved/Automation/TODO-03H9-C-Batch4-R2/index.json 中 ManagedMontageCancelWindow、ManagedMontageConfigurationOnly 均 Success、errors=0。
- 最新 Development Editor 构建成功（Saved/Logs/TODO-03H9-C-Final-Editor-Build.log）。完整受影响回归 Saved/Automation/TODO-03H9-C-Final/index.json：30 项 Success，6 项无警告、24 项带警告，failed=0、notRun=0、errors 合计为 0，进程 exit 0。包含 Managed 三专项、Adoption 1A/1B/3、Player Rate 六组、Big/Launch、Enemy Rate/Stance/Launch、HitReaction、ActionWindows、MobileBow、MeleeMotionWarping 和处决九项。此结果不包含已知待修复的 ExecutionImpactFeedback。
- 采用对账：批准的全部 20 个原生玩法 Ability 均有 PlayActionMontage 标准入口；该 Ability 目录未检出 PlayMontageAndWait、Montage_Play 或直接 PlayMontage 调用。此项是源码对账，不能代替蓝图额外入口和实际资产 readback。
- 非 Editor Win64 Development 初次构建发现 EnemyRate 第 2.2 节与 StanceRate 动画 fixture 的 WITH_EDITOR 边界遗漏，以及 HitReaction 的三个 INFINITY 常量溢出。已限定 Editor 动画创建及其依赖区段、保留其他可编译区段与全部 Editor 行为断言；三个常量改用 std::numeric_limits<float>::infinity()。三文件 Rider errors 为空，精确 diff --check 通过，上述 30 项回归在修复后的 Editor 构建上执行。
- 非 Editor R1 构建仍失败（Saved/Logs/TODO-03H9-C-NonEditor-R1-Build.log），已授权文件的报错消失，剩余仅为清单外四文件的 C4756：ExecutionSnapAlignmentAutomationTests.cpp、ProjectileTargetAssistAutomationTests.cpp、ProjectileFlightTrailAutomationTests.cpp、ProjectileLifecycleAutomationTests.cpp。已提出仅补 <limits> 和替换报错 INFINITY 的限定扩围，连同先前 ExecutionImpactFeedback 的 StanceBreak fixture 适配共五文件待用户批准；批准前不写入这些文件，不重复运行未修复失败。
- Batch 4 代码与配置专项已交付，C 尚未关闭：待上述扩围修复及复验、非 Editor 构建成功、最终 Fresh Reviewer，以及用户资产 readback/PIE。所有缺口唯一归 ROADMAP 的 Debt-03H9-C-Validation；不更新 ARCHITECTURE 为稳定事实，不提交。父阶段 A/B 和 H6/H7 原有债务不随本次源码回归自动关闭。

### 五文件扩围修复收据

- 用户批准后完成限定修复：ImpactFeedback 复用真实 StanceBreak fixture，删除该 helper 原有 CDO 临时写入；其余四文件仅补 <limits> 和替换五处编译报错的 INFINITY，全部原语义断言保留。
- Editor Win64 Development 与非 Editor Win64 Development 均构建成功（Saved/Logs/TODO-03H9-C-ScopeFix-Editor-Build.log、TODO-03H9-C-ScopeFix-NonEditor-Build.log，exit 0）。非 Editor 产物为 Binaries/Win64/PolyQuest.exe；这不是打包或运行产物验收。
- Saved/Automation/TODO-03H9-C-ScopeFix/index.json：ExecutionImpactFeedback、ExecutionSnapAlignment、Projectile.FlightTrail、Projectile.Lifecycle、Projectile.TargetAssist 共 5 项 Success、errors=0；前者带 fixture 警告。与此前 30 项报告合计覆盖 34 个不同用例（SnapAlignment 重复一次），不冒称最新版本单次跑了 34 项。生产源码未在这五文件修复中变化。
- 五文件 Rider errors 为空、精确 diff --check 通过。先前 ImpactFeedback 及非 Editor 编译阻塞均已关闭。按用户指定顺序，接下来派发一次独立只读 Fresh Reviewer 审查全阶段源码；这是最终用户资产 readback/PIE 之前的源码验收，不据此宣布 C 全门禁完成或可提交。

## 13. 合并用户验收（源码收口后集中一次执行）

不需要逐批重复编译或 Automation。用户在最终代码构建上集中完成以下 readback 与 PIE，记录通过项和异常日志；发现问题后仅复验受影响项。

- 只读配置核对：从真实授予的 GA / Combo / 武器数据追到实际 Montage，覆盖 Sprint/EnemySmall 两试点、四向选择和处决双方；确认 Skeleton、Slot、Section，以及 Rate/Cancel/HoldReady Notify 类型、数值、区间和派发模式，排查蓝图额外播放入口。显式 None 可无 Cancel Notify；支持取消的动作须有对应窗口/策略与现有标记和目标阻止声明。核对真实 Parry/Guard 冷却和效果配置，替代测试 GE 警告不能证明生产配置正确。
- PIE 攻击/移动组：Light 连段与换段、Charged 暂停蓄力、Sprint、Bow Draw/Hold/Release、MeleeSkill、Dodge；确认窗口改速及退出恢复，窗外拒绝/窗内取消，低体力或目标动作激活/Commit 失败时源动作保持。
- PIE 受击/防御组：Player Small/Big/Launch、Guard/GuardBreak/Parry，Enemy Small/Big/Melee/Launch/StanceBreak；覆盖四向、连续受击、重激活、打断/死亡，确认 Root Motion、朝向、移动及结束状态恢复，Parry 自然完成冷却时点正确。
- PIE 处决组：Front/Backstab 与 Victim 配对、伤害与释放时点、非致死恢复、致死收尾、打断及连续重触发；确认玩家 0.2s 混出和 Victim 尾段表现保留，无旧实例影响新动作。
- 提供最终版本上的上述结果后，由 Main 按 A/B 固定交接及 H6/H7 债务逐项对账；同一场景能覆盖的收据复用，但不能只以笼统“一次 PIE 正常”核销所有历史债务。C 关闭及 Git 提交仍须单独通过对应门禁。

A/B 当前源码构建及所列回归已与 C 报告完成映射，ROADMAP 两条债务现在只保留各自精确资产 readback；历史验证时点不改写。H7 的 Falling/自然结束/死亡/四向取消/连续受击场景可在本轮 PIE 合并记录；H6 的修复前 RED 对照、未来远程敌人 PIE 和 Shipping/Test 宏构建等独立条件不由本轮 Development 构建或一次 PIE 自动核销。

## 14. 独立 Fresh Review 与 Light 混合修复闭环

- 独立只读 Reviewer c_fresh_review 完成全阶段源码审查，CRG 基线对应 931f5de，定向 CodeGraph 裁剪部分采用限定源码补足，按两批预算结束。发现一项 P2、无 P0/P1：Light 换段先 EndTask，标准 Task 默认 CurrentMontageStop(0) 提前清零旧姿态，丢失下一 Montage 非零 BlendIn 原本提供的交叉混合。其余未发现已证实 P0–P2；用户资产/PIE 仍独立待验。
- Main 在批准的 Light cpp、公共 Task h/cpp 与 Player Rate 测试内完成一次限定修复。Task 增加窄的 PrepareForMontageTransition：只撤销窗口/订阅/自有 Tag 并恢复速率，以不可逆标志拒绝迟到窗口事件；保留 Montage 和取消委托的清理责任。Light 先调用它，待下一 Task 的 Ready 返回后才 EndTask 旧 Task。由原生 ASC PlayMontage 处理完整 BlendIn 设置，不复制引擎混合算法、不改变默认退出停播或现有 Setter。公共 Task 写入范围据此包含保持既定 Light 交叉混合所需的窗口交接分支；工厂仍为九参数。
- 修复前新增真实实例断言：下一 Montage BlendIn=0.2s，旧段有权重时分支；五个 case 均测得旧混出=0、权重=0（Saved/Automation/TODO-03H9-C-LightBlend-Red），稳定复现审查缺陷。保留全部既有窗口/身份/结束断言，新增交接期间撤销权限触发同步 GAS 取消以及下一段播放失败后的旧动画清理验证。
- 修复后 Development Editor 和非 Editor Win64 Development 构建成功（Saved/Logs/TODO-03H9-C-LightBlend-R1-Editor-Build.log、TODO-03H9-C-LightBlend-R1-NonEditor-Build.log，exit 0）。Saved/Automation/TODO-03H9-C-LightBlend-R1/index.json 在同一最终源码版本单次执行 34 项，全部 Success，9 项无警告、25 项带警告，failed=0、notRun=0、errors合计=0。旧段混出0.2/权重保留、窗口及时清理、同步取消与失败清理均通过。
- Main 增量复核了本次四文件修改：Prepare 标记在撤销 Tag 前设定，迟到窗口被拒绝；撤销 Tag 引发同步取消时旧 Task 仍走正常停播；新播放返回后旧实例清理受身份授权保护，不能停掉新段；失败不恢复旧 Task。四文件 Rider errors 为空、精确 diff --check 通过。此 P2 已闭环，无未关闭 P0–P2；后续修复按规则未再派 Reviewer，不冒称独立 Reviewer 复审了修复版本。
- 当前交付状态：Batch 1A–4 实施、构建/Automation、源码审查与定向修复全部完成。剩余用户按第 13 节集中做一次配置 readback 和代表性 PIE；收到结果后再完成 C/父阶段适用收据收口和稳定文档更新。未修改生产资产，未暂存、提交或推送，ARCHITECTURE 暂不更新。

## 15. 用户验收与文档收尾（2026-09-14）

- 用户在最终合并验收交接后明确“测试通过，可以开始文档收尾”。记录为最终用户测试/PIE 通过，不再要求重复运行本轮场景；未虚构逐个资产路径、Notify 数值或蓝图 readback 结果。独立配置核对收据仍唯一归 ROADMAP 的 C/A/B readback 债务，不把未附记录当作配置错误或新的代码修复任务。
- ARCHITECTURE 已按真实源码与已验证原生行为更新：全部 20 个标准 Task 消费者、三种取消策略、停播 Setter/权属、Light 原生交叉混合交接与仅配置诊断，删除旧的逐 Ability 窗口 Context/监听说明；没有将未读回的资产配置写成稳定事实。ROADMAP-archive 已追加 C 的核心结果和证据边界。
- ROADMAP 删除 C 已完成批次的详细计划，只保留独立资产收据入口；新增 03C 前的 03H10 处决机会红标和 03H11 受击位移反馈试调。仅接受目标排期，技术取舍标为推荐：红标表达破韧机会，Small 优先试小幅击退，Big/Launch 保留 Root Motion；正式实现另开最小计划，不覆盖当前交接。
- 本轮仅修改 plan、ROADMAP、ARCHITECTURE、ROADMAP-archive。未修改 Source/Config/Content、未操作 Editor、未编译/重跑已通过测试、未派代理、未暂存/提交/推送。阶段文档使用归档标题追溯，尚无本次提交号，不虚构固定 commit 引用。

## 16. 合并资产 readback 与父阶段关闭（2026-09-14）

- 用户授权直接开始总验收，Main 通过 Unreal MCP 对实际 PolyQuest / Scene01 做只读配置验收。32 份 GA、15 份相关数据资产、39 份 Montage、48 份源动画的授予/引用、四向、处决双方、Skeleton/Slot/Section、Rate/Cancel/HoldReady 和相关蓝图入口已对账，真实 Parry/Guard 效果配置已读回。读取范围、结论与证据边界保留在 ROADMAP-archive 同名总验收条目，后续替换本计划仍可追溯。
- 本次未发现阻塞 03H9 的配置问题；复用此前最终构建/Automation、用户测试/PIE 和已闭环审查，关闭 A/B/C 资产收据债及父阶段总验收。历史章节中的“待 readback”表示当时状态，以本节为最新交接；不重跑未变源码用例，不外推为全项目资产审计、Shipping/Test 或打包证明。
- 本轮仅写 plan、ROADMAP 和 ROADMAP-archive；未改 Source/Content/Config、未保存 Editor 资产、未写个人 memory、未暂存/提交/推送。H6/H7 独立债务保持原关闭条件；下一阶段为 03H10，Big 倍率仍按 03H12 条件开启。
