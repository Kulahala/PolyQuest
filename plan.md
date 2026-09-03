# TODO-05A1-C：Release Outcomes And Paired Presentation

## 阶段状态与基线

- 状态：已实施、用户验证通过、Main Fresh Review 通过；等待明确提交批准。
- 日期：2026-09-03。
- 仓库：E:\GameDevelop\PolyQuest。
- 基线：main @ e97564c435b541aeb9acf3bcc22426ac01509ffc。
- 前置阶段：TODO-05A1-A 的成对锁定/Lock-On retention 与 TODO-05A1-B 的 DeathPending/延迟死亡已完成；详细收口已在 ROADMAP-archive.md 归档。
- 路线：TODO-05A1-A -> TODO-05A1-B -> TODO-05A1-C -> TODO-03C。
- 工作树：包含用户-owned Content/**、资产差异、Config/Automation/Presets/1.json、AGENTS.md 及其他 WIP；这些内容必须保留并排除，不作为本阶段基线或提交内容。

## 阶段目标

本阶段只回答一个运行时问题：处决命中后，Player 与 Victim 能否通过明确的 Release Request/Release 握手，在不重复结算伤害的前提下完成双人锁定解除，并根据命中结果选择延迟死亡/Ragdoll 或非致死 Launch/站立恢复。

核心链路固定为：

~~~text
Hit -> Release Request -> Authenticated Release -> Victim Outcome -> Finalize
~~~

- Hit 是唯一伤害结算点。
- Release 只负责解除锁定和提交结果，不产生第二次伤害。
- Player Montage 是唯一 gameplay 时钟；Victim Montage 只负责表现，不参与时长同步或完成屏障。
- 正常致死处决在 Player 尾段结束前保持 DeathPending，随后沿用现有 CommitExecutionDeath() 死亡链。
- 正常非致死处决在释放后恢复敌人并派发既有 Launch Reaction；无法激活 Launch 时安全降级为存活站立。

## 工具路线与责任

- Outer：ue-stage-workflow。
- Primary：ue5-cpp-gameplay。
- Support：none。
- Route reason：这是现有 GAS 成对执行会话的 Release/结果垂直切片，不引入新的动画同步框架或第二条伤害路径。
- Execution route：manual/out-of-band Gemini。
- Contract owner：Main/Codex。
- Implementation writer：Gemini，仅能写下列批准路径中的冻结实现。
- Main 负责架构、计划、范围、验证解释、ue-strict-review、文档、暂存和提交；用户负责 Visual Studio 编译、Editor readback、Automation、PIE/视觉验证和最终提交批准。
- Gemini 不得修改 plan.md、ROADMAP.md、ROADMAP-archive.md、ARCHITECTURE.md 或 README.md，不得暂存或提交。

## 批准变更路径

1. Config/Tags/PolyQuestGameplayTags.ini
2. Source/PolyQuest/Public/Animation/Combat/AnimNotify_PlayerExecutionRelease.h
3. Source/PolyQuest/Private/Animation/Combat/AnimNotify_PlayerExecutionRelease.cpp
4. Source/PolyQuest/Public/Combat/Execution/ExecutionLockContext.h
5. Source/PolyQuest/Private/Combat/Execution/ExecutionLockContext.cpp
6. Source/PolyQuest/Public/AbilitySystem/Abilities/PlayerFrontExecutionAbility.h
7. Source/PolyQuest/Private/AbilitySystem/Abilities/PlayerFrontExecutionAbility.cpp
8. Source/PolyQuest/Public/AbilitySystem/Abilities/PlayerBackstabExecutionAbility.h
9. Source/PolyQuest/Private/AbilitySystem/Abilities/PlayerBackstabExecutionAbility.cpp
10. Source/PolyQuest/Public/AbilitySystem/Abilities/EnemyVictimExecutionAbility.h
11. Source/PolyQuest/Private/AbilitySystem/Abilities/EnemyVictimExecutionAbility.cpp
12. Source/PolyQuest/Private/Tests/ExecutionReleaseOutcomesAutomationTests.cpp

需要未列出的路径、Tag、Input、Config、资产或公开接口时，Gemini 必须停止并把证据交回 Main 做范围决定。

停止条件：发现需要修改批准路径之外的文件、普通 Melee/Projectile 死亡语义、AttributeSet、Projectile Targeting、Blueprint/资产、Build.cs 或新的 Tag/Input；无法证明同步 Release、AbilityTask 重入、Context 失效或死亡提交的安全顺序；或测试夹具只能绕过而不能驱动真实 GAS 路径时，立即停止，不得自行扩展范围或用测试旁路掩盖契约缺口。

### 实施期编译修复例外

- 允许仅修改 Source/PolyQuest/Private/AbilitySystem/Abilities/EnemyLaunchReactionAbility.cpp：将匿名命名空间中的 AirborneTransitionGraceSeconds 重命名为 EnemyAirborneTransitionGraceSeconds，并同步修改其唯一使用点，以消除 UE Unity Build 与 PlayerLaunchReactionAbility.cpp 的同名符号冲突。
- 该例外是语义不变的编译修复，不属于 C 阶段功能；不得修改 PlayerLaunchReactionAbility.cpp，不得借机重构 Launch、Task、Movement 或其他共享代码。
- 修复完成后必须单独报告该路径和两处名称变更，并重新执行适用的静态检查与 git diff --check；用户仍负责手动编译验证。

## 冻结状态与事件契约

新增 Event.Action.Execution.Request.Release 和 UAnimNotify_PlayerExecutionRelease：

- Notify 只向 Player 自身 ASC 发送 Request。
- Notify Payload 必须使用 Instigator=Owner、Target=Owner、OptionalObject=Animation。
- 正式 Event.Action.Execution.Release 只能由 Front/Backstab 的 Player helper 发送。
- Request 与正式 Release 必须校验当前 Ability、activation token、Source/Target Actor、ASC、Montage/Sequence 身份；错误、重复、迟到和旧 Context 均 fail-closed。

Hit/Release 顺序：

- Hit 先到：只完成命中结算并收敛到 NonLethal 或 DeathPending，不得立即发送正式 Release。
- Release Request 先到：锁存 Request；在 Hit 尚未完成时仍允许唯一一次合法 Hit。
- Hit 完成且已有锁存 Request，或已完成 Hit 后收到合法 Request：调用唯一 formal-release helper。
- 同帧或相邻帧的多个 Request/Hit 只能产生一次正式 Release。
- Montage 自然结束可生成释放兜底；无命中时只解除锁定，不得触发 Launch。
- 取消、中断、销毁和 UnPossess 不得触发非致死 Launch；若已确认致死，异常收尾仍必须完成死亡。

Context 保留两条正交状态轴：

~~~text
Hit:     Ready -> Resolving -> NonLethal/DeathPending -> Finalizing -> Finalized
Release: NotRequested -> Requested -> VictimReleased -> Finalized/Failed
~~~

- ReleaseRequested/锁存状态不能阻断后续合法 Hit。
- 只有正式 Release/VictimReleased 后才拒绝迟到 Hit。
- Lethal 路径必须在 Hit 状态仍为 DeathPending 时执行 BeginFinalization -> CommitExecutionDeath -> CompleteFinalization；Release 状态不得替代 DeathPending 门禁。
- 正常 Victim Release 后不立即 InvalidateSession()；Context 保持到 Player Montage 尾段完成。异常清理才提前失效。

## 实施顺序与生命周期

### 1. Task 与 Player 激活

- Front/Backstab 创建 Montage、Hit Event、Release Request 三个 Task，并先绑定自己的回调。
- CommitAbility 成功后，先 ReadyForActivation() Hit/Release 监听 Task，再激活 Montage Task。
- 每个 ReadyForActivation() 都是同步重入边界：立即检查 Ability 是否仍 Active、Context/token 是否仍 Current、Task 指针是否有效且 IsActive()；若已结束，不得恢复旧 Task 或旧 activation。
- 任一监听 Task 创建、激活或复验失败时，不得启动 Montage，直接统一清理。
- Release Request 到达时，Player 设置 release-expected 屏障；正式 Release 前解绑自己注册的目标 Tag/Destroyed 委托，防止 Victim 同步清理掐断 Player 尾段。
- 正式 Release 发送后 Player 继续播放自身 Montage；只有完成、BlendOut、Interrupted、Cancelled、Destroyed 或其他终止路径才最终 EndAbility()。

### 2. Execution Context

- 增加 Release Request、Victim Released、Source/token/Actor 校验及非致死 outcome 收尾 API。
- Formal Release 和 Victim Released 必须幂等；Context 失效后所有 Hit、Release、委托和异步回调 fail-closed。
- Release 后不得再次授权执行命中；不得在 Player 尾段尚未结束时清除当前 session。

### 3. Victim Ability 与表现

- UEnemyVictimExecutionAbility 增加可选 Front/Backstab Victim Montage，以及默认开启的 bLaunchNonLethalOnRelease。
- Victim Montage 播放失败或提前结束只影响表现，不结束锁定 Ability，不作为 gameplay 完成屏障。
- 停止 Victim Montage 时只移除本 Ability 自己绑定的动态委托，结束 Task，并执行唯一一次约 0.2f BlendOut；不得对共享委托无条件 Clear()，不得由后续 Super::EndAbility() 再次零时长硬停覆盖。
- 在调用 Super::EndAbility() 前缓存 ASC、Enemy、Source Actor、Context 和 Montage 指针。
- Lethal Release：保持 DeathPending，在 Context 有效且 Hit 状态满足 CommitExecutionDeath() 门禁时完成最终死亡；不得恢复 AI、Movement 或 Poise。
- NonLethal 正常 Release：清理 Victim 锁和 AI，恢复 MOVE_Walking 与 Poise，调用 Super::EndAbility() 移除 Stunned 等拥有标签，再派发 Event.Reaction.Enemy.Launch。
- Launch Payload 必须明确设置 Instigator=PlayerActor、Target=EnemyActor；不得在本阶段直接调用 LaunchCharacter()。
- Launch 条件、配置、grounded 校验或事件激活不满足时，保持敌人存活站立，不伪造 Ragdoll。
- Release Notify 注释和 Editor 清单提示美术将 Release 放在 Hit 之后，最好至少间隔一个 Montage tick；代码仍需容错错序事件。

## 非目标与不变量

- 不修改 EnemyCharacter.*、MeleeHitResolver.*、PlayerCharacter.*、CharacterAttributeSet.*、Build.cs 或既有测试文件；除上方明确的一行级 Unity Build 编译修复例外外，不修改 EnemyLaunchReactionAbility.*。
- 不修改普通 Melee/Projectile 即时死亡语义，不放宽全局 Projectile Targeting 或 Lock-On acquisition。
- 不新增通用 PlayerExecutionAbilityBase、双侧完成屏障、全局动画监听器、全局 Time Dilation、Camera/Audio/Hit-Stop、多人复制或预测。
- 不通过文件系统修改 Blueprint、AnimBP、Montage、.uasset 或 .umap；资产作者化由用户在 Editor 完成。
- 不建立第二条伤害或死亡路径；既有 CommitExecutionDeath()、Launch Reaction 和 Lock-On retention 是唯一复用入口。

## Automation 与验证矩阵

新增套件：PolyQuest.Combat.ExecutionReleaseOutcomes。测试旁路只能置于 WITH_DEV_AUTOMATION_TESTS，并优先驱动真实 GAS 授予、激活、GameplayEvent、Task 和清理路径。

必须覆盖：

- Hit 先到不提前 Release；Release 先到锁存并在 Hit 后补发。
- 同帧/重复/错误/旧 Context、错误 Actor/ASC/Montage、malformed Payload 全部拒绝且不改变锁定。
- Hit/Release 监听 Task 先于 Montage 激活，首帧事件不会漏收；ReadyForActivation() 同步结束时不恢复旧状态。
- Victim Montage 缺失、播放失败、提前结束和 BlendOut 清理不影响 gameplay 收尾。
- 正常 NonLethal Release 只派发一次 Launch；Launch 条件不满足时恢复存活站立且不派发 Launch。
- 取消、中断、Destroyed、UnPossess 和无命中兜底不产生 Launch。
- Lethal Release 保持 DeathPending，只提交一次 Dead/Ragdoll，并覆盖同步 CancelAllAbilities 重入。
- Player Montage 尾段不因 VictimLocked/Dead 标签同步移除而提前中断；Context 在尾段结束前保持有效。
- 回归 ExecutionLethalRecovery、ExecutionLockIn、FrontExecution、Backstab、Player.LockOn、HitReaction、Enemy.DeathRagdoll。

实施前静态门禁：读取最终 diff、检查相关 Tag/接口、运行 Rider lint_files 或 get_file_problems 和 git diff --check；不得将静态结果当作编译或 PIE 证据。

用户门禁：

1. Visual Studio 2022 手动编译 PolyQuestEditor (Development Editor)。
2. Editor readback 确认 Notify 类、Victim Montage 属性、Launch 配置、敌人 Blueprint 授予的 Ability 和现有 Montage/GE 引用。
3. /Game/Maps/Scene01 PIE 验证有/无 Victim Montage、Hit/Release 同帧或相邻帧、非致死 Launch、Launch 降级站立、致死完整播放、Player 尾段、Lock-On 保留和异常清理。

## 交接、文档与提交

- Gemini 已报告实际修改路径、生命周期/所有权、静态检查和第二轮修复结果；未把未执行的独立编译或 PIE 收据写成已完成证据。
- 用户此前确认本阶段 PIE 与 Automation 通过，并在第二轮修复后再次确认 `PolyQuest.Combat.ExecutionReleaseOutcomes` Automation 通过；Main 已完成一次独立 `ue-strict-review`，共享改动使用一次有界 `code-review-graph` 雷达和一次定向 CodeGraph 核对。
- Review 通过后，Main 负责更新 ROADMAP.md 的里程碑/债务指针、ROADMAP-archive.md 的阶段收口和 ARCHITECTURE.md 的稳定合同；临时资产调参不写入架构文档。
- 只有用户明确批准后才暂存和提交；提交排除 AGENTS.md、全部 Content/**、Config/Automation/Presets/1.json 及其他 WIP。

## 阶段收口记录

### 实际实施范围

- 12 个计划批准路径均已实施：`Config/Tags/PolyQuestGameplayTags.ini`；`AnimNotify_PlayerExecutionRelease.h/.cpp`；`ExecutionLockContext.h/.cpp`；`PlayerFrontExecutionAbility.h/.cpp`；`PlayerBackstabExecutionAbility.h/.cpp`；`EnemyVictimExecutionAbility.h/.cpp`；`ExecutionReleaseOutcomesAutomationTests.cpp`。
- 另有一项计划明确允许的语义不变 Unity Build 修复：`Source/PolyQuest/Private/AbilitySystem/Abilities/EnemyLaunchReactionAbility.cpp` 仅将匿名命名空间常量重命名为 `EnemyAirborneTransitionGraceSeconds` 并同步唯一使用点。阶段实际源码/Config/test 路径共 13 个。
- 第二轮修复把命中失败时的 formal-release 失败分支收敛到受害者取消、Context 失效和残余 Tag 清理，并补充真实 GE 应用失败的 Section 15；Front/Backstab 的同名匿名辅助函数同时完成 Unity Build 隔离。

### 验证与复核证据

- 用户确认 `PolyQuest.Combat.ExecutionReleaseOutcomes` 在第二轮修复后 15 个章节均为 `Success`；此前已确认本阶段 PIE/Automation 通过。第二轮报告没有提供新的独立编译、Editor readback 或 PIE 收据。
- Gemini 报告涉及源文件 Rider error-level 检查为 0 errors，且 `git diff --check` 通过；这些属于静态证据，不替代编译、Editor 或 PIE 证据。
- Main 的独立 Fresh Review 结论为通过，未发现当前批准范围内可证明的 P0/P1/P2 缺陷；第一批使用与基线匹配的有界 `code-review-graph` 影响雷达，第二批仅对 Release/死亡/Launch 生命周期疑点使用一次定向 CodeGraph，随后按两批门禁停止探索。

### 残余风险与提交边界

- `FrontExecutionVictimMontage`、`BackstabExecutionVictimMontage`、Player Release Notify 放置和 Launch/GE/Montage 等作者化关系仍属于用户-owned `Content/**`；本阶段不宣称干净检出即可复现完整表现基线。没有单独归档的手动 `PolyQuestEditor (Development Editor)` 编译或 Editor readback 收据，作为非阻塞 authored-validation debt 保留。
- 阶段候选提交只包含上述 13 个源码/Config/test 路径与 Main 收口文档 `plan.md`、`ROADMAP.md`、`ROADMAP-archive.md`、`ARCHITECTURE.md`、`README.md`；明确排除 `AGENTS.md`、全部 `Content/**`、`Config/Automation/Presets/1.json` 及其他 WIP。当前尚未暂存或提交，等待用户明确批准。
