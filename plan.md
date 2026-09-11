# TODO-05A1-F: Non-Lethal Execution Victim Root-Motion Handoff v1

## 1. 目标、基线与本次修订

Target Objective：处决开始只锁定敌人；捅入处的 Hit 只结算伤害；抽刀处的 VictimStart 才决定受害者表现。非致死只从头播放所配置的完整 Victim Montage：原地反应、倒地、滑移和起身由用户作者化决定，有 Root Motion 时由 CMC 消费位移，无 Root Motion 的合法原地动画同样可播放；真正播完后恢复 AI。致死在 Hit 后保持 DeathPending，到 VictimStart 才提交死亡并进入现有布娃娃流程。

- 工作目录：E:\GameDevelop\PolyQuest；运行时模块：PolyQuest；引擎源码唯一基准：D:\UE\UE_5.8。
- Git HEAD：09db3a05d740efb6cae1d7be53bd4d9a244db080。执行起点是当前未提交工作树，不是干净 HEAD：Victim、两个 Player Ability、相关处决测试已有修改，ExecutionVictimRootMotionAutomationTests.cpp 未跟踪。Hit -> VictimStart 和旧 Release 动画入口退休均据用户确认已执行，当前 diff 也显示旧 Notify 文件与请求 Tag 已删除；本次在现有工作树上删除击飞开关/fallback 并允许原地 Montage，禁止整体回滚或重新套用旧版。
- 退休前置证据（用户，2026-09-12）：用户明确确认目前只有一个处决动画，已移除 Release 通知，并授权清理；附图展示 AM_LightSword_PlayerExecution 的 Editor 状态。接受该用户作者化证据作为旧 Notify 类退休前提，不重复请求同一确认；不扩写为 Agent 已完成全资产 Reference Viewer/Asset Registry 扫描。仅在发现具体遗漏引用时报告。
- 当前验证状态（2026-09-12）：本阶段用户已确认编译、PIE 与相关 Automation 通过；末轮 Test Run 4 明确记录 ExecutionVictimPresentation、Player.LockOn 两套 Success。Main 已完成限定 delta review，原启动实例身份与真实生命周期测试 Findings 关闭，源码/测试/用户 PIE 验收完成，用户已批准 Git Commit；提交记录以 Git 历史为准。逐项证据边界见第 9 节；早期 None 引用/旧开关截图不再代表当前状态。
- 本文件保留 TODO-05A1-F 的最终交接和修复历史；本次收口摘要追加至 ROADMAP-archive.md，下一阶段正式接受前不替换 plan.md。上一阶段 TODO-07B11-RET 不重复归档。
- 用户已确认：Hit 只结算伤害；VictimStart 位于抽刀处；非致死播放完整恢复 Montage，致死立即布娃娃；玩家按自己的 Montage 收尾，恢复期保留 Victim 锁与无敌。旧动画 Release Notify、请求 Tag 和无效 Player 监听已进入退休结果，继续保持删除；本次同时退休 bLaunchNonLethalOnRelease 及向 Enemy Launch Ability 的备用派发，不增加替代开关。内部释放协议继续保留。
- Deliberate Non-goals：双人前段配合动画、Section 跳转/切链、致死播放起身动画、死亡专用 Montage、修改 Player 输入恢复时机、Motion Warping、真实腾空/抛掷、墙体特殊动画、第二伤害路径、网络功能、通用处决框架。用户明确由被处决 Montage 决定是否带击退/倒地动画；只有未来出现具体距离调整需求时才另行考虑 Motion Warping，本阶段不增加 Warp Target、距离参数或组件。
- CMC 继续负责 capsule sweep、地面与台阶；位移只来自作者化 Root Motion，没有 Root Motion 时不补偿位移。GAS/ASC 仍是状态权威，不新增平行动作状态机或 Gameplay Tag。
- 当前 Main 按用户“测试通过，可以继续”的指令完成限定终审与四份阶段文档收口，不自动启动其他切片。

## 2. 冻结的 Runtime Contracts

### 2.1 正常时序与作者接口

```text
处决开始 -> 配对锁定、一次性 Snap、停止敌人移动与 AI
Hit（捅入） -> 唯一伤害结算；致死只记录 DeathPending
VictimStart（抽刀） -> 已结算结果的一次性释放交接
    非致死 -> 从头播放作者化 Victim Montage（原地或 Root Motion）
              -> OnCompleted -> Victim EndAbility -> 恢复 AI/状态
    致死   -> Victim EndAbility 内提交现有死亡流程 -> 布娃娃
Player 自身 Montage BlendOut/结束 -> 按既有时机恢复操作
```

- 复用 FrontExecutionVictimMontage、BackstabExecutionVictimMontage，语义统一为抽刀时开始播放的非致死完整恢复 Montage；按请求方向在激活时快照引用。
- 删除本阶段尚未提交的 FrontNonLethalRecoverySection、BackstabNonLethalRecoverySection 及其测试接口、运行时快照和分段控制代码；不新增替代 Section 字段、前段 Montage 字段或模式开关。
- 恢复 Montage 从默认入口/时间 0 播至真正结束，Section 0 合法；不调用 Montage_JumpToSection、Montage_SetNextSection，不切断或改写实例 Section 连接，不修改共享动画资产。
- 唯一表现路径要求非空 Montage、有效 Slot、有限正播放长度、可用且兼容的 AnimInstance 及真实播放成功。移除 HasRootMotion() 作为必需激活条件：合法无 Root Motion Montage 走同一播放与恢复生命周期，不是失败或备用分支。有限且无循环的完整流程由 Editor readback 确认，不以空 transient Montage 或测试 bypass 伪造正向结果。
- 有位移动画的 Root Motion 配置和实际提取由用户作者化/readback；原地动画保持原地表现。代码不检测动画是否「像击飞」、不自动选择普通受击 Montage、不补 LaunchCharacter/Impulse/位移，也不增加 bUseRootMotion 一类替代开关。
- 处决开始不播放 Victim Montage，不人为插入空配合段；保留现有锁定/竞争 Ability 取消/动画图姿态行为，不额外引入 Montage pause 或全局停动画机制。抽刀前姿态由实际 AnimBP/资产验证。

### 2.2 Hit 只结算伤害

- 正面与背刺继续使用现有鉴权、几何校验、FMeleeHitResolver -> Damage GameplayEffect 和 bDamageEventConsumed exactly-once 入口，保留已有 Hit impact feedback。
- Hit 成功后不播放恢复 Montage、不释放 Victim、不提交布娃娃；删除旧 bReleaseRequestLatched 及其初始化、清理和测试接口，不保留无消费者状态。
- 致死 Hit 继续写 DeathPending，暂不变 Dead/布娃娃。没有成功 Hit 的 VictimStart 不得启动任何正常分支。
- 错序 VictimStart（Hit 前）、错误来源/动画/token/目标事件直接拒绝且不占用一次性成功标记；不缓存它等待 Hit 自动播放。后续真正合法的 VictimStart 仍可被接受。

### 2.3 Player 在 VictimStart 建立安全交接

正面、背刺采用相同顺序，只窄改现有事件处理与清理路径：

1. 在 HandleVictimStartEventReceived 中验证当前 activation token、Ability、Context、事件标签、Player 来源/目标与当前 Player Montage 来源；要求 bDamageEventConsumed 且 HitState 为 NonLethal 或 DeathPending，拒绝已交接/重复事件。
2. 缓存本次 Context 与目标 ASC，验证目标仍属于本会话。调用现有 TryBeginRelease(this, CurrentActivationToken, false, true) 开始一次性释放事务，不新增 Context API。
3. 在向 Victim 同步转发 VictimStart 前设置 bVictimStartForwarded、bVictimReleaseExpected 并 UnbindTargetDelegates。顺序必须保证合法致死、VictimLocked 移除或目标销毁不会被 Player 当作意外失效提前结束自己的 Montage。
4. 继续转发现有 Event.Action.Execution.Request.VictimStart，OptionalObject 携带本次 Context、OptionalObject2 携带已验证的动画来源；正常路径不额外发送 Event.Action.Execution.Release 来再次驱动表现。
5. 同步返回后按缓存 Context 核验 IsVictimReleased()（兼容既有 Finalized 语义）。若未被确认，安全取消对应 Victim、使本会话失效并结束失败的 Player Ability；不能只依赖 EndAbility 再发 Release，因为 TryBeginRelease 已可能把 IsReleaseSent 置真。

所有转发、取消、Montage 启动与任务激活返回点均按同步重入边界处理。只操作本次缓存且仍有效的对象，不把已清空的成员恢复为旧状态。

### 2.4 VictimStart 结果分流

- Victim 复核活跃 Ability、Context 身份、来源/目标/本 Victim 注册、动画载荷、已完成 Hit、IsReleaseSent 及未被取消；成功 MarkVictimReleased(this) 后才能执行一次性结果。
- 致死：不读取恢复 Montage 有效性，不播放任何含起身的动画；直接经唯一 EndAbility() 中已有 CommitExecutionDeath -> SetDeadState/HandleDeath/CancelAllAbilities/StartDeathRagdoll 链提交死亡。保证死亡/重入清理一次，不另开伤害或 ragdoll 实现路径。
- 非致死：原地和 Root Motion Montage 共用一条 grounded 恢复路径。停止事件监听，建立本地恢复状态；确认存活有效宿主、可用 CMC 和真实地面支撑后，保存并关闭 bCanWalkOffLedges，清理交接前速度，仅将本 Ability 持有的 MOVE_None 切回 MOVE_Walking，然后创建并从时间 0 激活 Montage Task。原地动画的位移为零，不为它新增另一套运动模式/状态机。
- 地面判断不能仅凭 MOVE_None 或陈旧 CurrentFloor 判为有效；使用本次 capsule 位置的有效地面检查。不得在外部 Falling/其他模式接管后强制 Walking。
- ReadyForActivation() 返回后核验 Ability、宿主、Task、实际播放实例与当前运动模式。启动中建立的状态只用于可回收的准备；播放失败时完整撤销本次准备，不保留假恢复状态。
- 仅在确认恢复成功接管后，恢复完成、中断和 EndAbility 依赖本地恢复事实；Player 结束或 Context 被置为 inactive/Failed 不得提前终止恢复。启动成功前保留必要的交接/结果信息以完成失败清理，不能先清掉 Context 再猜结果；失败也不转入其他受击 Ability。
- 恢复期间保持 State.Action.Execution.VictimLocked、State.Status.Invulnerable、State.Status.Stunned、State.Input.Block.Movement、State.Input.Block.Jump 和 AI 锁；玩家不等待敌人起身。自然 OnBlendOut 不结束 Task/Ability，OnCompleted 才恢复 AI 与状态。

### 2.5 旧入口/击飞 fallback 退休与内部清理保留

- 删除 UAnimNotify_PlayerExecutionRelease 的 .h/.cpp 文件，以及 Config/Tags/PolyQuestGameplayTags.ini 中唯一的 Event.Action.Execution.Request.Release 注册行。用户已确认实际处决动画移除旧通知；本次批准两个源码文件删除和这一行 Config 修改，无需另开退休阶段。
- 两个 Player Ability 及其回调 Context 中完整删除旧请求 Tag 查询/激活门禁、WaitReleaseRequestEventTask 的创建/绑定/ReadyForActivation/检查/清理、OnReleaseRequestEventReceived、HandleReleaseRequestEventReceived、bReleaseRequestLatched，以及只服务该入口的 TestTriggerReleaseRequestEvent、失效注入与状态查询接口。相关注释同步更新，不保留 no-op listener、空 UCLASS、重定向或测试专用兼容壳。
- 更新四个批准的处决表现/结果/Root Motion/反馈测试：删除旧 Notify payload、旧请求 Tag 必须存在、旧 latch/Task 失效及 no-op 回调测试；正常场景使用 Hit -> VictimStart，清理场景使用真实 Player EndAbility/取消或既有内部 Release 入口。不得仅删调用而让场景失去有效触发。
- Event.Action.Execution.Release 与 SendFormalReleaseToVictim 保留为 Player EndAbility/取消/缺失事件等内部兜底；不能把外部旧 Notify 与内部可信清理混为同一路。
- 保留 Victim 的 WaitReleaseTask/OnReleaseReceived、Context 的 TryBeginRelease/MarkVictimReleased/IsVictimReleased 和相关释放状态；它们仍参与正常交接确认与异常收敛。禁止按名称含 Release 批量删除，不重命名内部协议来制造额外 diff。
- 缺失 VictimStart 时，Player 结束仍必须回收锁与会话；非致死只做安全解锁/Poise/AI 清理，不启动任何补播或 Enemy Launch；致死待定在清理出口完成死亡，禁止留下零血活体。缺失通知是作者化失败，不声称正常拔刀时机已通过。
- 已经成功 VictimStart 交接后，Player EndAbility 不再重发释放，也不取消独立 Victim 恢复；重复/迟到 Release 无副作用。
- 删除 bLaunchNonLethalOnRelease 的 UPROPERTY、构造初始化、GetTest/SetTest 接口、专属默认值/开关测试及 EndAbility 中向 Event.Reaction.Enemy.Launch 派发的完整分支；清理只服务该分支的局部变量/依赖，不移除仍用于其他清理或结果诊断的状态。
- 合法非致死 VictimStart 遇到 None/无效 Montage、播放失败或不满足运动条件时，给出本次失败原因并进入唯一 EndAbility 清理出口，释放本次锁/委托/运动设置；不回滚已结算 Hit、不补播其他 Montage、不启动任何普通受击 Ability。缺失/无效配置不能因备用反应被掩盖。
- Victim Ability 在所有成功、失败、取消和缺失事件路径中，派发 Event.Reaction.Enemy.Launch 的次数均为零。全局 Event.Reaction.Enemy.Launch Tag 和 UEnemyLaunchReactionAbility 保留给普通战斗，不删除、不改写、不重命名。
- 正常播放完成、中断、死亡与 teardown 的内部释放/结果清理不属于表现 fallback，必须继续保留。

### 2.6 运动、异步与单一清理出口

- 复用原生 MovementModeChangedDelegate：OnMovementModeChanged(ACharacter*, EMovementMode PrevMovementMode, uint8 PreviousCustomMode)。新模式从 CMC->MovementMode 读取；先建立本地准备状态并绑定，再切换 Walking，返回后重新核验。恢复中离开 Walking 即取消，不强制改回。
- CMC 处理 sweep、阻挡、滑动、贴地与台阶，骨骼动画按自身时序结束；不做穿墙、瞬移修正或额外推动。bCanWalkOffLedges 不是腾空防护。
- 保留 bAllowInterruptAfterBlendOut 的 UE 5.8 契约和本次播放实例身份。异常停止精确作用于本次实例并使用 0 秒淡出，覆盖已 BlendOut 情况；不能仅按同名 Montage 停止后来重播的实例。
- 清零速度只限存活有效宿主且本次恢复仍持有运动；死亡、Destroy、外部 Falling/动作接管后不无条件清零新所有者速度，不清空其他系统 Root Motion 来源。
- 自然结束、中断、取消、播放失败、死亡、Destroy、UnPossess 收敛至幂等 EndAbility：清理 Task/委托/本地状态/Victim 注册，只恢复本次修改的 ledge 设置；存活有效时按既有 StanceBreak handoff 恢复 Poise/AI，死亡不重启 AI。
- 保留 Ability.Action.Teardown.OnUnpossess 与现有 ASC Tag 贡献所有权，不靠移除别人的 Loose Tag 修补状态，不修改 Character/Controller/GAS 核心架构。

## 3. Approved Paths 与所有权

本阶段累计批准清单为下列十五个路径，均相对 E:\GameDevelop\PolyQuest；只允许列出的符号/行为范围。用户已于 2026-09-12 明确批准增加 PlayerCharacter.cpp 与 PlayerLockOnAutomationTests.cpp，用于非致死处决独立恢复期间的当前锁定保持。本轮修复范围以第 7 节为准，不为凑齐累计清单重复修改其他路径。

| 文件 | 允许修改范围 |
|---|---|
| Source/PolyQuest/Public/AbilitySystem/Abilities/EnemyVictimExecutionAbility.h | 保持既有退休；新增最小 Native 只读恢复来源查询与弱引用来源快照，及必要启动重入测试入口；不新增 Blueprint 接口/Tag |
| Source/PolyQuest/Private/AbilitySystem/Abilities/EnemyVictimExecutionAbility.cpp | 保持唯一作者化表现；实现本次恢复来源记录/查询/清理，补齐启动期间精确零秒停止与重入防护；保留已修的方向快照/实例停止/地面模式保护 |
| Source/PolyQuest/Private/AbilitySystem/Abilities/PlayerFrontExecutionAbility.cpp | 保留既定 Hit/VictimStart/内部兜底；删除旧请求 Tag 门禁、Task 全生命周期、Context 转发 callback、空 handler、latch 与测试接口定义，同步注释 |
| Source/PolyQuest/Private/AbilitySystem/Abilities/PlayerBackstabExecutionAbility.cpp | 与 Front 对称的旧入口完整删除，保留正常交接与内部异常清理 |
| Source/PolyQuest/Private/Tests/ExecutionVictimPresentationAutomationTests.cpp | 移除旧 Launch 开关/fallback 消费，覆盖原地/Root Motion 直接播放、完成/错序和失败后无补播；保留已完成 Notify 退休 |
| Source/PolyQuest/Private/Tests/ExecutionReleaseOutcomesAutomationTests.cpp | 删除开关默认值/切换与 fallback 正向场景；验证各种结果均不派发 Enemy Launch，保留 VictimStart 回执、死亡/取消/缺失事件内部清理 |
| Source/PolyQuest/Private/Tests/ExecutionVictimRootMotionAutomationTests.cpp（现有未跟踪文件） | 将合法无 Root Motion 从失败/fallback 用例改为同生命周期正向用例；Root Motion 正向及地面/ledge/Context/重入/清理覆盖保留；失败和成功均验证零 Enemy Launch |
| Source/PolyQuest/Private/Tests/ExecutionImpactFeedbackAutomationTests.cpp | 更新旧请求 Tag/测试调用与收尾方式，保持 Hit feedback exactly-once 断言 |
| Source/PolyQuest/Public/AbilitySystem/Abilities/PlayerFrontExecutionAbility.h | 仅删除旧请求 callback/handler、Task、latch 和专属测试 seam；内部释放 API/状态不动 |
| Source/PolyQuest/Public/AbilitySystem/Abilities/PlayerBackstabExecutionAbility.h | 与 Front header 对称的旧请求声明/状态/测试 seam 删除 |
| Source/PolyQuest/Public/Animation/Combat/AnimNotify_PlayerExecutionRelease.h | 删除整个已退休 Notify 头文件，不留空反射类 |
| Source/PolyQuest/Private/Animation/Combat/AnimNotify_PlayerExecutionRelease.cpp | 删除整个已退休 Notify 实现文件 |
| Config/Tags/PolyQuestGameplayTags.ini | 仅删除 Event.Action.Execution.Request.Release 注册行；保留 Event.Action.Execution.Release 和所有其他 Tag |
| Source/PolyQuest/Private/Character/Player/PlayerCharacter.cpp | 仅在 ValidateCurrentLockedTarget 增加当前目标的本次非致死恢复保留判定，及其文件内最小辅助/include；不得放宽获取/切换、修改共享投射物候选筛选或延长遮挡豁免 |
| Source/PolyQuest/Private/Tests/PlayerLockOnAutomationTests.cpp | 补充真实 Victim 恢复来源资格、Player 先结束、恢复完成/取消、普通无敌及遮挡/死亡/主动解除回归；保留原配对阶段契约 |

- ExecutionLockContext、EnemyCharacter/AIController、其他 Abilities、Build.cs、所有未列出的 Config/Notify/Tag 定义只读。禁止修改或恢复现有已删除的 Config/Automation/Presets/1.json；这是用户既有 WIP。
- 测试构造辅助留在批准测试文件内，不把 Mock Montage 工厂放入生产头文件；允许无 Root Motion 是本次已批准的生产契约调整，不是测试 bypass。不得弱化 Slot、长度、真实播放成功、地面和生命周期校验。测试可监听仍合法的 Event.Reaction.Enemy.Launch 来断言零派发，不把该全局 Tag 的测试观察误认为生产 fallback 残留。
- 本阶段未提交实现是本次修改起点；只移除与新契约冲突的 Section/事件顺序代码，保留有效的 Root Motion、实例身份及清理防护。不得整体回滚或重写无关实现。
- Main-only 文档：plan.md、ROADMAP.md、ROADMAP-archive.md、ARCHITECTURE.md；当前收口写入这四份文档。Gemini 不修改文档、不暂存、不提交。
- 既有 Content/Blueprint/AnimBP/Montage/地图/导入资源与其他 WIP 保留且排除；Config 唯一新增授权是上述 Tag 注册行，不扩展到其他 Config 改动。

路由与分工：

- Outer: ue-stage-workflow
- Primary: ue5-cpp-gameplay
- Support: none
- Route reason: 同阶段收敛为唯一作者化 Victim Montage 表现，退休跨 Ability Launch fallback、允许原地动画，保留伤害、死亡、释放确认与独立恢复所有权。
- Execution route: manual/out-of-band Gemini 完成阶段实施；用户随后明确授权 Main 亲自修复最后两条回归及其夹具问题。Implementation executors: 1（原 Gemini 路由）；Main 承担批准范围内的后续窄修复；in-app delegation: 0。
- Main 拥有计划、架构、验证解释、Fresh Review、文档与提交；Gemini 负责批准路径的增量实施、测试与自审；用户拥有 Editor 制作、手动编译、PIE/视觉验证及提交批准。
- 这是首次实施后的同阶段调整，Gemini 在当前会话单兵复核，不再派发实施自审子代理。

## 4. 用户 Editor 清单与验证矩阵

### Editor 作者化/readback

- 用户核对实际 /Game/_Abilities/Enemy/GA_EnemyVictimExecution 和所引用的 Front/Backstab Victim Montage；已知 /Game/BP/Montages/AM_Enemy_BeExecuted 仅为磁盘候选，不代替真实引用 readback。
- Player Montage：Hit 放捅入命中处，VictimStart 放抽刀完成/敌人开始倒下处，且在 Player BlendOut 前。两个事件不能以同一时间点的未定义顺序代替明确先后。
- 退休前置证据已由用户提供：当前只有一个处决动画，已移除 Release Notify；截图显示 AM_LightSword_PlayerExecution。此确认足以按本计划进入源码退休，不要求用户重复证明或先验收保留空监听的版本。
- 退休后用户 readback：保存并重新载入实际处决 Montage/相关 GA，确认无 Missing Class/Failed to find class 或旧请求 Tag 警告，Notify 选择列表不再出现旧 Player Execution Release，Hit 与抽刀处 VictimStart 仍在正确位置。不得将截图解读为 Agent 已扫描全部资产或已验证 VictimStart 时机。
- 用户给实际使用方向的 FrontExecutionVictimMontage/BackstabExecutionVictimMontage 配置有效资产，不能保留 None 后用其他反应代替验收。Montage 从默认入口完整播放且无循环；用户自行选择原地反应或带位移/倒地/起身的序列，不强制必须包含击飞。未来确有距离调整需求再独立考虑 Motion Warping，当前不实现。
- 回读 Skeleton、Slot、AnimBP Root Motion from Montages Only、两个 Montage 引用及 GA 载入/编译状态。带位移资产核验 Root Motion/Root Lock 与 capsule 实际跟随；无 Root Motion 资产核验原地正常播放/结束。同一次 Editor 编译/重启后确认 Launch Non Lethal on Release 字段已消失，不再出现被删除字段的蓝图使用错误。用户提供当前 GA 为纯数据蓝图的截图，仅作为当前可见作者化证据。
- 致死分支无需有效恢复 Montage：Hit 后仍保持被捅姿态，VictimStart 抽刀时才进入布娃娃，不播放起身。
- 本计划不授权 Agent 写入任何 .uasset/.umap；Editor 写入由用户完成并提供 readback。角色冻结的视觉效果需真实观察，不以 DisableMovement 推断 Mesh 姿态。

### 验证矩阵

| 门禁 | 必须覆盖 |
|---|---|
| 静态/范围 | 十五个累计批准路径与本轮限定符号；git diff --check；Rider 可用时无错误；仅删除指定 Tag 行，无其他 Config/Build.cs 差异；两个旧 Notify 文件已删除；Source 与指定 Tag 配置中的旧类/include/请求 Tag/Task/latch/callback/专属 seam 零残留（历史文档不计）；保留内部 Release Tag/接口与清理测试；Victim .h/.cpp 无 bLaunchNonLethalOnRelease、专属 seam、Enemy Launch 派发/请求残留，全部 Source 无已退休开关/seam 消费；不要求全局 Enemy Launch Tag 零引用 |
| 非致死事件 | 激活不播放；Hit 只扣血/反馈且保持锁；合法 VictimStart 从时间 0 播放 Montage，默认 Section 0 合法；有 Root Motion 与完全无 Root Motion 都成功且共用同一锁/AI/结束生命周期；重复 Hit/VictimStart 不重复伤害/播放 |
| 致死事件 | Hit 后 DeathPending、无布娃娃；抽刀 VictimStart 才提交一次死亡；空/无效恢复 Montage 不阻挡致死；目标死亡/销毁不使正常 Player Montage提前取消 |
| 错序/鉴权 | Hit 前 VictimStart 不缓存/不消费成功标记；错误 token/来源/目标/动画、无成功 Hit 均拒绝；失败的同步回执无残锁；不为已删除入口保留 no-op 正向测试 |
| Player/Context | Front/Backstab 对称；Player 先结束、取消或 Context inactive/Failed 不破坏已接管的独立恢复；VictimStart 缺失和 Player 提前终止仍安全回收、无零血活体；按第 7 节保留当前目标 Lock-On，主动解除后不自动重锁 |
| 运动/生命周期 | 当前 capsule 有效地面检查；MOVE_None 不等于有地面；非 Walking 拒绝/取消；ledge 原值 true/false 均恢复；外部接管不被强制 Walking/清零；ReadyForActivation 同步结束、BlendOut 后中断、同 Montage 新实例、Destroy/UnPossess/迟到回调幂等 |
| 失败与零备用派发 | null/缺 Slot/零负 NaN/Infinity 长度/播放失败/不满足地面条件安全退出；None 不能被误当作正常原地动画；合法无 Root Motion 是正向用例；成功恢复、所有失败、缺失 VictimStart、死亡/取消均不派发 Enemy Launch，锁/AI/Poise/委托按契约收敛 |
| 回归 | PolyQuest.Combat.ExecutionLockIn、ExecutionLethalRecovery、ExecutionReleaseOutcomes、ExecutionVictimPresentation、ExecutionVictimRootMotion、ExecutionHitNotify、ExecutionImpactFeedback、ExecutionSnapAlignment；PolyQuest.Combat.EnemyStanceBreakRateWindow、EnemyLaunchReactionRootMotion；新增 PlayerLockOnAutomationTests.cpp 对应 Lock-On 套件 |
| 用户编译/PIE | PolyQuestEditor (Development Editor)；Scene01 Front/Backstab 原地与 Root Motion 非致死反应及致死，捅入/抽刀节奏，玩家先恢复，平地/轻坡/台阶/墙角/边缘，动画取消、死亡、Destroy/UnPossess；配置缺失时明确失败且无备用击飞；无重复击退/提前布娃娃/死亡起身/提前 AI 恢复/残留保护 |

Automation 尽量使用现有 transient fixture 和真实 ASC 事件；测试 seam 只用于必要的边界注入。手动回调测试不能替代真实 Montage/Root Motion 提取/跨帧物理与视觉证据。

用户已确认的通知移除满足本次退休前置条件。清理后统一执行本阶段编译、Automation、Editor 重新载入/readback 与 PIE，无需先验收旧入口再另开退休切片，不能沿用旧 Section 版本结果宣告成功。涉及反射类删除，用户编译/Editor 重启后复查加载状态，不能仅靠旧会话仍驻留的类认定退休成功。未经用户另行授权，Gemini 不运行手动编译、Editor 写入或 PIE，未执行项明确标注。

## 5. 执行交付、停止条件与收口

- 先核对实际仓库、当前计划、目标 diff 和十五个累计批准路径，本轮只执行第 7 节修复。按 AGENTS.md 探索预算，用已有源码或定向 CodeGraph 回答必要调用关系，覆盖不足才聚焦读取；不再重规划已冻结的动画时序，不全库漫游。
- 本计划明确授权既有 Notify/请求 Tag 退休，以及本次 bLaunchNonLethalOnRelease 字段、专属 seam 和备用派发删除；不得因旧计划写过保留 fallback 而再次请求相同授权。常规算法、局部辅助与测试设计自主闭环；若发现具体遗漏资产引用、必须修改白名单外文件、增加资产写入或改变死亡/ASC 核心所有权，带最小证据交回 Main，不能自行扩范围。
- 同一失败根因最多一次有证据修复加一次定向重跑；重复根因或两轮连续修复失败停止循环并报告。
- Gemini 在当前会话完成严格实施自审，不派发新子代理。交付实际变更清单、契约实现摘要、正常及对抗性自审 findings、静态/测试结果、未执行门禁和精确用户 Editor/PIE 清单；不得改本文记录交付。
- Main 后续按 ue-strict-review 做有界 Fresh Review。完成标准：旧动画请求入口及 Victim Launch fallback 零残留、反射退休后资产载入正常，原地/Root Motion 两种作者化 Montage 均按 Hit -> VictimStart 时序工作，致死抽刀布娃娃与内部取消/缺失事件清理正常，具备对应编译/Automation/readback/PIE 证据且无 P0-P2 阻塞问题。
- 稳定后才由 Main 更新 ARCHITECTURE.md、ROADMAP.md 与归档；未通过门禁或接受延期风险在 ROADMAP.md 有唯一记录及关闭条件。本阶段不因移除表现 fallback 删除内部失败清理；Commit 仍需用户明确批准。

当前状态（2026-09-12）：实现与本轮定向验证已验收，Main delta review 未发现新的 P0-P2 问题；第 6–8 节为历史修复凭据，第 9 节为最终状态。用户已批准本阶段 Git Commit；提交记录以 Git 历史为准。

## 6. Main Fresh Review 与定向修复交接（2026-09-12）

### 已接纳的证据与边界

- 用户在本轮明确确认编译、PIE 与 Automation 通过；直接接纳，不重复执行。Gemini 交付附件明确列出 ExecutionReleaseOutcomes、ExecutionVictimPresentation、ExecutionImpactFeedback、ExecutionVictimRootMotion 四个套件 Success，并报告 Rider 0 errors/0 warnings。用户确认与执行者报告分开记录，不扩写为所有计划回归套件均逐项验收。
- 本轮 Main 静态证据：实际变更在十三个批准路径内；Tag 配置仅删除旧 Request.Release 行；旧 Notify/请求入口/击飞开关检索零残留；git diff --check 通过。Content 与 Config/Automation/Presets/1.json 既有 WIP 排除。
- Review 已按两批预算完成：一次有界 code-review-graph 雷达（built_at_sha 与 HEAD 一致），一次目标 CodeGraph 及必要的一跳 UE 5.8 Task/AnimInstance 源码。图谱标注的 tests/gaps 不代替测试或运行时证据，资产/动态时序覆盖回退至当前源码与用户证据。
- 未把截图/执行者说明扩大为完整资产 readback、Editor 重启后退休验证或 clean-checkout authored baseline。修复后需针对受影响门禁追加新证据。

### 首轮 Findings（历史行号；修复进展与本轮要求见第 7 节）

1. [P1] EnemyVictimExecutionAbility.cpp:631-688：SetMovementMode/ReadyForActivation 是同步重入边界，但恢复状态、ActiveVictimMontage 与实例 ID 在 ReadyForActivation 返回后才建立，且 Task 配置 bStopWhenAbilityEnds=false。若播放触发同步取消/EndAbility，清理时尚无动画所有权信息，已启动 Montage 可在解锁后继续播放；返回分支仍可操作宿主并再次进入清理。必须建立能覆盖启动期间的播放所有权与重入防护，终态返回后不继续操作/恢复状态，并实际验证本次播放实例。
2. [P1] EnemyVictimExecutionAbility.cpp:784-794：先按旧实例 ID 验证，随后却调用按资产查找当前活动实例的 Montage_Stop。旧实例正在淡出而同一资产已启动新实例时会停止新实例；仅有旧淡出实例时也可能未被精确停止。必须对本次 ID 对应实例实施停止，不以 asset-level API 替代实例所有权。
3. [P2] EnemyVictimExecutionAbility.cpp:601、631-632、998-1000：bHasValidFloor 将 MOVE_None/MOVE_Walking 或陈旧 CurrentFloor 当作地面证据；正常锁定必为 MOVE_None，因而无需任何实际支撑即可通过。外部 Falling 也可能被陈旧 floor 接受并强制 Walking，未接管恢复的 EndAbility 还会无条件恢复 Walking。必须做当前 capsule 的真实地面检查，并在接管与退出时保全外部运动模式所有权。
4. [P2] EnemyVictimExecutionAbility.cpp:568-572，EnemyVictimExecutionAbility.h:65-69：PendingVictimMontage 为空时回退读取任意方向的当前字段，绕过方向快照和 None 配置失败契约；测试 setter 也将 Pending 快照直接改成 Front 优先。必须仅使用激活时按请求方向取得的快照，删除运行时跨方向 fallback；测试在激活前配置数据，不为适配测试改写进行中的快照。

### 修复范围与验收

- 由 manual/out-of-band Gemini 在当前会话定向修复，不再派发子代理。Main 本轮不改运行时代码：四项合并涉及启动、停止、运动所有权及 fixture 调整，不属于狭窄孤立修复例外。
- 首轮修复白名单为 EnemyVictimExecutionAbility.h/.cpp 与三个处决测试；该轮原本不包含 PlayerCharacter。用户后续已明确批准新增 PlayerCharacter.cpp 与 PlayerLockOnAutomationTests.cpp，本轮以第 7 节的七文件修复清单为准。ExecutionLockContext、EnemyCharacter/AIController、资产与 Config 继续只读；不引入新 Tag、外部库、通用框架或 Motion Warping。
- 保留已冻结的 Hit -> VictimStart、原地/Root Motion 同一路径、致死抽刀布娃娃、零 Enemy Launch 和旧 Notify 退休结果；只修上面四项及相应测试。
- 测试补齐：Front=None/Backstab=有效及反向；激活后修改配置不影响快照；MOVE_None 无地面、外部 Falling 带陈旧 floor；运动模式切换/播放启动期间同步取消；旧 Montage 淡出期间同资产新实例不被旧清理停止、仅旧淡出实例可被停止。
- 当前正向 fixture 普遍使用 bTestBypassMontageActiveCheck 并手动触发 Completed；这不足以证明播放实例和同步重入。新回归必须真实触达所修边界，不仅设置标记后直接 EndAbility。不要用改变生产 fallback 或扩大 ExpectedError 抑制来让测试变绿；若 fixture 所需接口超出白名单，带最小证据交回 Main。
- 按既有一次有证据修复/一次定向重跑上限执行。交付限定 diff、四项对应测试/静态结果、严格实施自审与未执行门禁。用户重新确认受影响的编译、Automation 和定向 PIE 后，再交 Main 做 delta review；先前通过记录保留但不替代修复版验证。

## 7. 本轮合并修复：恢复期 Lock-On 保持与启动清理（2026-09-12）

### 证据、根因与验收目标

- 用户已确认首轮修复后的 PIE 和四套处决 Automation 成功，同时报告「敌人非致死倒下并起身后脱锁」。这是需要修复的新运行时问题，不以绿色测试覆盖掉该反馈。
- 静态根因：普通 FCombatProjectileTargeting::IsValidTargetCandidate 排除 Invulnerable；CanRetainExecutionLockedTarget 又要求 PlayerLocked 与 VictimLocked 同时存在。Player 先结束后 PlayerLocked 消失，仍无敌的恢复中 Victim 不满足两条保留路径，ValidateCurrentLockedTarget 清锁。PlayerLockOnAutomationTests 原有「只有 VictimLocked 则清锁」测试只覆盖旧配对模型。
- CanRetainExecutionLockedTarget 同时用于 UpdateLockOnOcclusion 的配对阶段遮挡豁免，因此不能简单删除它的 PlayerLocked 条件，否则会意外扩大遮挡豁免。
- 首轮修复已看到 FindFloor、严格方向快照和按实例 Stop 等对应改动；未据此宣布四项全部验收关闭。当前启动期仍使用 bNonLethalRecoveryActive ? 0.0f : 0.2f，取消测试位于 ReadyForActivation 调用前，不能证明播放内部同步取消已闭环。
- 目标：同一玩家原本锁定的非致死 Victim，在 Player 收刀结束后到 Victim 恢复完成期间保持锁定；死亡、销毁、主动解除、屏幕边界和恢复期正常遮挡条件仍可解除。成功起身后自然返回普通目标校验，不自动重新锁定已经清除/切换的目标。

### 最小接口与 Lock-On 改动

1. 在 UEnemyVictimExecutionAbility 增加 Native public const 查询 IsNonLethalRecoveryFrom(const AActor* SourceActor)，不暴露 Blueprint/UFUNCTION。仅当 Ability Active、非结束中、已成功进入本次非致死恢复、宿主有效存活且未销毁、来源身份匹配时返回 true；启动准备、DeathPending、失败、取消和结束均为 false。
2. 使用一个 TWeakObjectPtr<AActor> 记录本次合法 VictimStart 的来源，激活开始复位，接管前从已鉴权 payload/context 记录，所有 EndAbility/失败出口清理。查询不得要求 Player Ability 或 ExecutionLockContext 仍 Active；不延长来源 Actor 生命周期，不改 Context/EnemyCharacter 接口。
3. 仅在 PlayerCharacter::ValidateCurrentLockedTarget 的已锁定目标校验中补充恢复保留路径：普通候选与原配对保留均失败时，通过当前目标 ASC 查找实际活跃的 Native Victim Ability 实例，要求其 Avatar 正是当前目标且 IsNonLethalRecoveryFrom(this) 为 true。保留有效对象、同 World、敌对队伍、双方非 Dead、目标非销毁等基础资格；以实际 Ability 恢复事实为准，不能仅凭手工添加 VictimLocked/Invulnerable 放行。
4. 原 CanRetainExecutionLockedTarget 保持原配对语义，UpdateLockOnOcclusion 的配对豁免保持原样。新恢复判定只补偿 Invulnerable 导致的当前目标失效，不加入获取/切换候选，不修改 FCombatProjectileTargeting，不引入对所有无敌目标的豁免。
5. 新路径继续经过 CacheCurrentLockedTargetCandidate 的屏幕保留检查。PlayerLocked 消失后继续运行既有 LOS/grace 检查，遮挡超时照常清锁；不延长 grace、不在每帧强制 SetLockedTarget、不在 Montage 结束时重新锁定。
6. PlayerCharacter.h 保持只读：已有 ValidateCurrentLockedTarget/测试入口足够；新增判定用函数内代码或文件内私有辅助完成，不为一次使用抽取通用系统。Victim 查询及来源快照是 GAS 事实的只读入口，不是新的锁定权威。

### 启动期残余修复

- StopVictimMontagePresentation 的异常退出对本次拥有的启动中/恢复中 Montage 都采用零秒停止；删除启动期 0.2 秒分支，不能解锁后残留 Root Motion。
- 播放尚未生成实例时取消，不能因 instance ID 为 INDEX_NONE 而停止已有的同资产无关实例；生成实例后发生同步取消，必须回收本次启动实例。保留已修的按 ID 精确停止，不能退回按资产误停。
- 补充真实触达播放启动内部的同步取消回归。当前「调用 ReadyForActivation 前直接 Cancel」保留为前置取消测试，但不能充当内部重入证明；断言实例停止、无残余推进、无新 Task/状态回写及无错误地清零接管者速度。
- 在运动模式切换、播放回调后检查终态及宿主有效性，继续使用唯一 EndAbility 清理；不修改已经正确的原地/Root Motion、致死、零 Launch 和方向快照契约。

### 本轮批准路径与测试

- 本轮仅允许七个文件：EnemyVictimExecutionAbility.h/.cpp、PlayerCharacter.cpp、PlayerLockOnAutomationTests.cpp、ExecutionVictimRootMotionAutomationTests.cpp、ExecutionVictimPresentationAutomationTests.cpp、ExecutionReleaseOutcomesAutomationTests.cpp；完整路径见第 3 节。优先只改直接需要的文件。
- 新增 Lock-On 回归须建立真实、来源匹配的 Victim 恢复，断言 Player Ability 结束并移除 PlayerLocked 后，ValidateCurrentLockedTarget 仍保留同一目标；Victim Completed 后普通校验继续有效。
- 负向覆盖：只有 Tags 没有实际恢复、恢复来源不是当前玩家、启动失败/已取消/非 Active/DeathPending，不得获得新豁免；普通 Invulnerable 仍不能被新获取或切换选中。
- 生命周期覆盖：玩家主动解除/切换后不自动锁回；目标死亡/销毁、屏幕越界或恢复期遮挡超时正常解除；短暂遮挡按已有 grace 恢复。既有配对阶段遮挡豁免不变。
- 保留旧的「只有 VictimLocked、无真实恢复应清锁」测试，增加新正向场景，不简单翻转旧断言。复用现有 projection/LOS 测试 hook，不修改生产筛选以适配 fixture。
- 必跑受影响 Automation：PlayerLockOnAutomationTests 对应套件，加 ExecutionVictimRootMotion、ExecutionVictimPresentation、ExecutionReleaseOutcomes、ExecutionImpactFeedback；启动取消回归要明确说明是真实播放回调还是 fixture/seam，不能混淆。
- 用户门禁：修复版 Development Editor 编译；Scene01 非致死 Front/Backstab，玩家先结束而敌人仍在恢复时保持锁定、起身后仍锁定；恢复期间主动解锁不抢回、墙体遮挡超时可脱锁、致死正常清锁/转移；原地与 Root Motion 动画节奏无回归。
- 此轮直接修正用户观察到的行为，定向 PIE 是必要门禁，不能由「只是安全加固」或静态/Automation 通过代替。已有通过证据保留，但不扩写为本轮修复版验证。

执行仍为 manual/out-of-band Gemini，当前会话自审，不派发子代理，不改文档/资产/Config，不暂存、不提交。按既定有界探索和修复重试预算交付；只在出现具体清单外依赖时报告。Main 收到修复和用户定向验证后做 delta review，未闭环前不更新稳定架构或完成归档。

## 8. Main 定向复核历史：实例身份与测试覆盖问题（2026-09-12；已由第 9 节关闭）

### 已接纳证据与已关闭项

- 用户明确确认最新修复版 PIE 和 Automation 通过，直接采纳；随附报告逐项列出 ExecutionReleaseOutcomes、ExecutionVictimRootMotion、ExecutionVictimPresentation、Player.LockOn 四套 Success。该报告未单列最新 ExecutionImpactFeedback 结果，不扩写为本轮五套逐项通过；此前通过记录保留。
- Main 本轮按两批预算完成限定 diff、一次有界 code-review-graph、一次定向 CodeGraph 与必要 UE 5.8 查询接口核对。HEAD 为 09db3a05d740efb6cae1d7be53bd4d9a244db080，图谱基线匹配；git diff --check 通过，Git 的 LF/CRLF 转换提示不等于 whitespace 检查失败。
- 外部移动模式缺陷已关闭：入口明确仅允许本 Ability 持有的 MOVE_None 或 Walking，Flying/Custom 在任何速度清零前拒绝。ExecutionReleaseOutcomes 新增真实地面下的 Flying/Custom 拒绝与速度、CustomMovementMode 保全测试。
- 原 StopVictimMontagePresentation 仍按实例 ID 零秒停止；Presentation Section 14 已建立两个真实 FAnimMontageInstance 并断言无关实例继续播放。这是有效进展，但没有覆盖新增启动清理分支。
- Lock-On 当前目标保留路径与既有遮挡豁免边界保持正确。报告中的“实例级 Section 断开”属于旧方案描述，不是本版契约；当前方案仍从头播放整段 Montage。

### 当时未关闭 Findings（历史行号；现已关闭）

1. **[P1] 启动取消清理仍以资产匹配替代本次实例身份。** EnemyVictimExecutionAbility.cpp:740-746 在 ID 丢失后回退 GetActiveInstanceForMontage/GetInstanceForMontage；:862、890-906 的启动监听也会接受同资产后续实例并覆盖 ID。EndAbility 经 StopVictimMontagePresentation 在 :980-981 清空原实例身份。如果启动回调在原实例已捕获后取消 Victim，再重入播放同资产的新动作，仍存活的启动监听会将新实例当作待回收实例并停止；返回点的资产回退也不能证明查到的实例属于本次调用。UE 5.8 GetActiveInstanceForMontage 只查询资产映射，GetInstanceForMontage 返回数组内首个资产匹配实例，两者都不提供启动调用身份。必须保留本次播放/激活的真实实例身份，禁止身份丢失后猜测资产归属，并防止旧启动清理影响重入后的新动作。
2. **[P2] 播放启动与 Lock-On 集成测试尚未触达要求的运行路径。** Presentation :1266 启用 bTestSimulateOldMontageStopCancel；生产测试分支 :692-714 在 ReadyForActivation 之前取消并返回，且专门跳过正常解绑，测试随后手工创建实例并广播 OnMontageStarted。它证明了人工晚到实例拦截，未覆盖真实播放调用及返回清理。PlayerLockOn :319-320 仍手工设置 bIsActive/恢复状态，未增加合法 Hit→VictimStart→Player End→Victim Completed 的锁定衔接测试。RootMotion 套件仍绕过实际播放并手调完成回调，不能以套件名称推定真实位移提取已由 Automation 证明；用户 PIE 证据另行保留。

### 后续修复边界与验收

- 保持此前提示词限定的六文件：EnemyVictimExecutionAbility.h/.cpp、ExecutionVictimPresentationAutomationTests.cpp、ExecutionVictimRootMotionAutomationTests.cpp、ExecutionReleaseOutcomesAutomationTests.cpp、PlayerLockOnAutomationTests.cpp；完整路径见第 3 节。PlayerCharacter.cpp/.h 本轮只读，稳定文档、资产与 Config 不变。
- 这是启动身份/测试边界的重复未闭环，Main 不自动继续代码试错。下一次执行先建立能暴露当前误停的失败回归，再做一次有证据修复与一次定向重跑；若 fixture 无法在批准路径内建立，报告具体条件和必要依赖，不增加绕过生产时序的成功分支。
- 播放边界至少覆盖：旧 Montage 停止回调在新实例创建前取消；新实例创建后、播放调用返回前取消；取消回调重入同资产新实例时只回收本次旧实例；结束后不恢复旧 Task/状态。保留已通过的普通按 ID 停止与移动模式用例。
- Lock-On 增加真实 ASC/Ability 激活和合法事件进入恢复的衔接用例，验证 PlayerLocked 正常移除后仍保留当前目标、恢复完成后普通候选校验继续有效；隔离测试可以保留，但不替代该用例。
- 交付明确区分真实引擎播放、真实实例、手动广播/回调及手工状态测试，逐项给出实际结果。真实播放与测试缺口未闭环前，不更新 ARCHITECTURE.md 为稳定事实、不完成归档、不提交。


## 9. 最终验收与文档收口（2026-09-12）

### 本轮结论与修复闭环

- Main 在既有 Fresh Review 与修复记录上完成限定 delta review，未发现新的 P0/P1/P2 blocker。冻结的 Hit -> VictimStart、整段作者化 Montage、原地/Root Motion 同生命周期、致死抽刀死亡、零 Enemy Launch、独立 Victim 恢复与恢复期 Lock-On 契约均保留。
- 原启动实例身份 P1 关闭：启动实例身份与 active 身份分开保存，捕获一次后解绑；返回清理只使用已确认 ID，不以资产匹配猜测待停实例。Presentation Case 13C 实际进入 ReadyForActivation，在新实例创建回调内取消并重入播放同资产，验证本次实例停止而替代实例存活。
- 原创建前取消缺口关闭：Presentation Case 13D 实际播放旧 Montage，由引擎停止旧组时的原生淡出回调取消 Victim；断言取消时新实例尚不存在，随后真实生成的实例被零秒停止，Task/ID/恢复状态清空，且外部接管后的速度被保留。此用例没有手工创建替代实例或广播启动委托。
- 原 Lock-On 集成缺口关闭：Case 2C 使用独立 Player/Enemy/武器夹具，通过 ASC 激活 Backstab 和真实 Hit/VictimStart 事件进入恢复；通过公开的 montage-only 更新和队列事件派发推进动画。Player 自然结束后，其 PlayerLocked 被移除且共享 Context inactive，目标仍保持锁定；Victim 自然 BlendOut 期间继续保持，实际 Completed 后转回普通候选校验。既有合成负向隔离测试保留，但不冒充集成证明。
- Main 测试夹具修复一并闭环：为合成 Sequence 建立有效 root 骨架/动画数据模型/骨轨道并等待自身压缩完成；采用公开的 FAlphaBlend::SetBlendTime；移除 UAnimMontage 构造时的空默认 Slot，索引前检查 Slot/Segment 数量。此前编译错误与数组越界崩溃不再作为当前未解决问题。
- 本轮只读复核使用一次有界 code-review-graph；图谱 built_at_sha 与 HEAD 09db3a05d740efb6cae1d7be53bd4d9a244db080 匹配。图谱未能识别的 UE 动态委托测试关系以直接源码和用户实际测试收据补足；未因风险分数继续扩张。CodeGraph 本轮 skipped：现有 diff/源码已足够解释修复边界，没有新的跨文件调用疑点。未派发子代理。

### 验证收据与证据边界

| 证据 | 来源与结论 |
|---|---|
| 最后两套回归 | 用户提供 Test Run 4：PolyQuest.Combat.ExecutionVictimPresentation、PolyQuest.Player.LockOn 均 Success；附件为 c89f57c8-66fa-4d18-acd5-97f9fd53169b/pasted-text.txt |
| 其余本阶段测试 | 采纳此前用户已确认的 ExecutionVictimRootMotion、ExecutionReleaseOutcomes、ExecutionImpactFeedback 通过记录；不把本次两套结果扩写为同一轮全量 Automation |
| 编译 | 本阶段已有用户明确“编译通过”证据；后续私有接口访问错误已修复并由用户成功运行新版测试，不额外声称 Main 执行 UBT 或有独立全量重编译收据 |
| PIE | 采纳用户此前明确确认的修复版 PIE 通过，包含恢复期锁定问题修复后的确认；最后增量仅为测试代码，无生产逻辑改动，不要求重复 PIE |
| 作者化 | 采纳用户对唯一处决动画已移除 Release Notify 的明确确认及已有截图/PIE；不声称 Main 做过全资产扫描、修改二进制资产或建立干净 authored baseline |
| 静态 | Main 的两个测试文件 Rider 错误检查为 0 errors；本轮 git diff --check 通过。Rider 曾漏报 C++ 访问权限错误，因此其结果不替代编译 |
| 覆盖限制 | 真实 Montage 生命周期/实例身份与用户 PIE 是不同证据；本测试不宣称跨帧 CMC 物理位移、全地形、视觉、网络或打包已获自动化证明。Presentation 输出中已有负向夹具 warning 不等于套件失败 |

### 文档、债务与提交边界

- ARCHITECTURE.md 更新当前事件时序、受害者独立恢复、精准实例清理、锁定保持及延迟死亡事实；ROADMAP.md 移除本阶段开放候选与旧 Section/Launch fallback 要求；ROADMAP-archive.md 追加本阶段追溯条目。
- 原 P1/P2 没有接受延期，均已闭环。二进制资产仍为用户 WIP，干净作者化基线限制沿用 ROADMAP.md 的既有资产范围记录；当前阶段不增加 Motion Warping、真实腾空、全地形或包装承诺。TODO-07B14 仍是条件性独立候选，TODO-03C 不因本阶段改变权属或自动启动。
- 待提交范围为第 3 节的 15 个累计 Source/Test/Tag 路径，加本节四份 Main 文档，共 19 个路径。包含两个旧 Notify 文件删除、单行 Request.Release Tag 退休与新增 ExecutionVictimRootMotionAutomationTests.cpp。
- 排除全部 Content/**、Config/Automation/Presets/1.json 和其余用户 WIP；不使用 git add -A。提交前按精确白名单暂存并检查 cached diff。
- 建议提交标题：[Feature] 处决受害者改由动画驱动恢复 (Montage-Driven Victim Recovery)。正文记录最终行为、回归证据及资产排除边界。用户已明确批准按上述 19 个路径提交；提交记录以 Git 历史为准。
