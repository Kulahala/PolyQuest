# TODO-05A1-B：Hit And Lethal Recovery

## 阶段状态与基线

- 状态：已实施并通过阶段门禁，文档已收口。
- 日期：2026-09-03。
- 仓库：E:\GameDevelop\PolyQuest。
- 基线：main @ 9d9721bc89ac618fdfdb022734869f3aff4aba6c。
- 上一阶段：TODO-05A1-A 已完成并已归档；其成对锁定、执行 context、AI 锁和 Lock-On retention 是本阶段前置条件。
- 路线：TODO-05A1-A -> TODO-05A1-B -> TODO-05A1-C -> TODO-03C。
- 工作树：不干净，包含用户-owned Content、资产差异、Config/Automation/Presets/1.json 和其他 WIP；这些内容必须保留并排除，不作为本阶段基线或提交内容。

## 阶段目标

本阶段只回答一个运行时问题：Front/Backstab 的授权命中致死后，敌人能否在 Dead 标签出现前保持成对锁定并完整播放玩家处决 Montage，随后通过现有死亡链只提交一次死亡；非致死、取消、销毁和 UnPossess 路径能否安全恢复或 fail-safe 收尾。

预期结果：

- 新增 State.Status.DeathPending，Health 可到 0，但执行致死期间不立即写入 Dead。
- Player 和 Victim 在 DeathPending 期间继续持有 PlayerLocked、VictimLocked、Invulnerable，外部 Melee/Projectile 伤害仍被阻断。
- Player Montage 完整结束或异常结束后同步发送 Release；Victim 再通过现有 SetDeadState -> HandleDeath -> CancelAllAbilities -> StartDeathRagdoll 链完成最终死亡。
- 非致死执行命中继续正常 Release；命中前取消保持敌人存活并恢复原有 Poise、Movement 和 AI。
- 命中后取消、Destroyed、UnPossess 或会话失效不能留下零血、非 Dead、永久 pending 的敌人。

## 工具路线与责任

- Outer：ue-stage-workflow。
- Primary：ue5-cpp-gameplay。
- Support：none。
- Route reason：这是现有 GAS 成对执行会话、唯一近战伤害入口与敌人终端死亡链的窄垂直闭环，不引入新动画系统或第二条伤害路径。
- Execution route：manual/out-of-band Gemini。
- Contract owner：Main/Codex。
- Implementation writer：Gemini，只能写下列批准路径中的冻结实现。
- Main 负责架构、计划、范围决定、验证解释、ue-strict-review、文档、暂存和提交；用户负责 Visual Studio 编译、Editor readback、Automation、PIE/视觉验证和最终提交批准。
- Gemini 不得修改 plan.md、ROADMAP.md、ROADMAP-archive.md、ARCHITECTURE.md 或 README.md，不得暂存或提交。

## 批准变更路径

1. Config/Tags/PolyQuestGameplayTags.ini
2. Source/PolyQuest/Public/Combat/Execution/ExecutionLockContext.h
3. Source/PolyQuest/Private/Combat/Execution/ExecutionLockContext.cpp
4. Source/PolyQuest/Public/AbilitySystem/Abilities/EnemyVictimExecutionAbility.h
5. Source/PolyQuest/Private/AbilitySystem/Abilities/EnemyVictimExecutionAbility.cpp
6. Source/PolyQuest/Public/Character/Enemy/EnemyCharacter.h
7. Source/PolyQuest/Private/Character/Enemy/EnemyCharacter.cpp
8. Source/PolyQuest/Private/Combat/Melee/MeleeHitResolver.cpp
9. Source/PolyQuest/Private/AbilitySystem/Abilities/PlayerFrontExecutionAbility.cpp
10. Source/PolyQuest/Private/AbilitySystem/Abilities/PlayerBackstabExecutionAbility.cpp
11. Source/PolyQuest/Private/Character/Player/PlayerCharacter.cpp
12. Source/PolyQuest/Private/Tests/ExecutionLethalRecoveryAutomationTests.cpp

本阶段不修改 MeleeHitResolver.h、CharacterAttributeSet.*、CombatProjectileTargeting.*、EnemyStanceBreakAbility.* 或既有测试文件；它们可作为回归验证对象。需要其他路径时，Gemini 必须停止并交 Main 做范围决定。

## 冻结状态与所有权契约

新增 Gameplay Tag：

- State.Status.DeathPending

DeathPending 由目标侧 EnemyVictimExecutionAbility 独占管理，不由 Player 跨 Actor 写入。使用明确的目标侧 tag ownership，只移除本次 Ability 自己增加的数量。

UExecutionLockContext 增加带 activation token 的命中事务状态：

~~~text
Ready -> Resolving -> NonLethal
                    -> DeathPending -> Finalizing -> Finalized
                    -> Failed
~~~

上下文必须能：

- 开始、完成和中止一次授权命中事务，拒绝重复或迟到命中；
- 记录 Release 是否由正常完成或取消触发；
- 验证当前 source/victim Actor、ASC、Ability、context 和 activation token；
- 在 DeathPending 或最终化后拒绝任何后续执行命中；
- 在 InvalidateSession 后让所有 Hit、Release、Delegate 和异步回调 fail-closed。

## 命中事务与 RAII 门禁

FMeleeHitResolver::TryResolveHit 仍是唯一伤害入口。执行 context 存在时，必须在标准 Damage GE 应用前后使用栈上 FExecutionHitScopeGuard 或等价 RAII 守卫：

- 构造时开启 context 的 Resolving 状态和 Victim 的授权 hit scope；
- 记录 GE 是否成功应用以及目标是否确认进入 DeathPending；
- 析构时无论哪条 return 路径都关闭 scope：
  - 成功且 Health 大于 0：收敛到 NonLethal；
  - 成功且 Health 等于 0 且 pending 已确认：收敛到 DeathPending；
  - GE 失败、对象失效或状态不完整：收敛到 Failed 并清除临时 marker；
- 守卫只持有非拥有弱引用；context 已失效时不得访问旧对象、恢复新一轮状态或调用 EndAbility；
- 授权 scope 建立失败时不得应用 GE；
- 若 GE 已使 Health 为 0 但无法建立可证明的 pending，必须 fail-closed 走现有即时死亡路径，不能遗留零血非 Dead。

普通 Melee、Projectile、错误 context、旧 context 和重复 Notify 仍不能穿透 Invulnerable。

## EnemyCharacter 与 Victim 生命周期

EnemyCharacter：

- 增加 IsDeathPending() 和仅供 Victim Ability 使用的授权 hit scope/最终死亡窄接口。
- OnHealthAttributeChanged 在有效 execution scope 内收到致死变化时，沿用现有 PendingDeathRagdollVelocityChange 的一次性捕获，跳过 SetDeadState，并通知 Victim 进入 DeathPending。
- pending 期间异常治疗必须复位为 Health 0；不修改通用 UCharacterAttributeSet。
- CommitExecutionDeath(Context) 只在 authority、当前 pending context、Health 小于等于 0 且尚未 Dead 时调用现有 SetDeadState。
- HandleDeath、CancelAllAbilities 和 StartDeathRagdoll 保持唯一死亡实现；不复制死亡流程或新增 Ragdoll 速度缓存。
- EndPlay、UnPossessed 和销毁路径清除 execution marker；对象仍有效且有权限时先完成最终死亡，否则安全失效弱引用。

EnemyVictimExecutionAbility：

- 激活要求 DeathPending Tag 有效，并继续持有 VictimLocked、Invulnerable、Stunned、Movement/Jump 阻断。
- 保存授权 hit scope、pending tag ownership、最终化重入保护和当前 context。
- 正常 Release：
  - 无 pending：按现有路径恢复 Movement/AI，并在 Front handoff 时恢复 Poise；
  - 有 pending：先调用 CommitExecutionDeath(Context)，再清理。
- 命中后取消、Victim 自身取消、目标 Tag 提前移除、UnPossess 或其他异常结束，都必须最终提交一次死亡。
- CommitExecutionDeath 触发的同步 CancelAllAbilities 只能被外层清理路径处理一次；重入不得跳过外层 Super::EndAbility。
- pending/dead 路径绝不恢复 Movement、AI 或 Poise。
- DeathPending 保持到 Dead 提交完成后再由拥有者清理。

Release 的同步顺序固定为：

~~~text
Player EndAbility
  -> HandleGameplayEvent(Release)
  -> Victim OnReleaseReceived
  -> CommitExecutionDeath / normal cleanup
  -> return to Player
  -> invalidate context
~~~

## Player 与 Lock-On

- Front/Backstab 保留 activation-time 几何快照、唯一命中消费、Montage 和 callback token 约束。
- EndAbility 在失效 context 前派发带 context 的 Release，并记录 bWasCancelled；不得因 Health 为 0 或 DeathPending 提前结束。
- APlayerCharacter::CanRetainExecutionLockedTarget 只扩展已有目标持有分支，使同时具备 PlayerLocked、VictimLocked、Invulnerable、DeathPending 的目标在真正 Dead 前保持锁定。
- 真正 Dead、Destroyed 或跨 World 仍优先走现有清除/重选逻辑。
- 不修改初次获取、循环切换、FCombatProjectileTargeting::IsValidTargetCandidate 或 Projectile Targeting 全局规则。

## 表现边界与非目标

- 不新增受害者 Montage、双人动画、双侧完成屏障或动画时长协调框架。
- 不在 Notify 中直接 LaunchCharacter、击飞、击退或触发第二条表现路径。
- 不新增 Camera、Hit-Stop、Audio、通用 PlayerExecutionAbilityBase、全局动画监听器、全局 Time Dilation、多人复制或预测。
- 不修改普通 Melee/Projectile 的即时死亡语义。
- 不修改 CharacterAttributeSet、Build.cs、Blueprint、Montage、AnimBP、资产或 .uasset/.umap。
- TODO-05A1-C 再处理受害 Montage、双人完成屏障、非致死 Launch/Knockback 和 Release 后表现。

## Automation 与用户验证

新增套件：PolyQuest.Combat.ExecutionLethalRecovery。测试 seam 只能置于 WITH_DEV_AUTOMATION_TESTS，且必须使用真实 GAS 授予、激活、GameplayEvent、Resolver 和清理路径。

必须覆盖：

- 普通非执行致死仍立即 Dead/Ragdoll；
- Front 与 Backstab 致死命中后 Health 为 0、DeathPending 存在、Dead 不存在；
- pending 期间双方锁定、无敌和 Lock-On 保持；
- Player Montage 完成后同步 Release，Dead、HandleDeath 和 Ragdoll 各只发生一次；
- RAII scope 在 GE 失败、早退、对象失效和重入后不残留 Resolving；
- 非致死命中正常 Release，不进入 pending；
- 命中前取消保持存活并恢复 Poise/Movement/AI；
- 命中后取消、Victim 取消、Destroyed、UnPossess 不留下零血非 Dead 的永久状态；
- 错误/旧 context、错误 source/target、重复 Notify、错误 Release 和外部 Melee/Projectile 全部 fail-closed；
- pending 期间治疗不会复活敌人；
- Ragdoll 冲量捕获/消费各至多一次；
- session A 的迟到回调不能影响 session B；
- 回归运行 ExecutionLockIn、FrontExecution、Backstab、Player.LockOn、Enemy.DeathRagdoll、HitReaction。

用户门禁：

1. Visual Studio 2022 手动编译 PolyQuestEditor (Development Editor)。
2. Editor readback 确认敌人 Blueprint 仍授予 Victim Ability，现有 Front/Backstab Montage 和 Damage GE 可用，未新增受害 Montage。
3. 在 /Game/Maps/Scene01 PIE 中验证致死执行不会立即进入 Dead/Ragdoll，玩家 Montage 完整播放且 Lock-On 不丢失；Release 后只进入一次现有死亡/Ragdoll；非致死和取消路径恢复正常。
4. 静态检查、Automation 和文档不能替代 PIE/视觉证据。

## Gemini Handoff Prompt

~~~text
你是 PolyQuest 的 Implementation Executor（Gemini）。

cwd: E:\GameDevelop\PolyQuest
baseline: main @ 9d9721bc89ac618fdfdb022734869f3aff4aba6c
active plan: E:\GameDevelop\PolyQuest\plan.md
Outer: ue-stage-workflow
Primary: ue5-cpp-gameplay
Support: none

Contract owner: Main/Codex
Implementation writer: Gemini
只修改本计划列出的 12 个批准路径；不得修改任何文档、Content、资产、Build.cs、CharacterAttributeSet、Projectile Targeting、其他 WIP；不得暂存或提交。

按顺序实现：
1. 注册 State.Status.DeathPending，扩展 ExecutionLockContext 的命中、Release 和最终化状态。
2. 在 EnemyVictimExecutionAbility 与 EnemyCharacter 建立目标侧 hit scope、pending tag ownership、Health=0 延迟死亡和一次性 CommitExecutionDeath。
3. 在 FMeleeHitResolver 中使用弱引用 RAII guard 包住标准 GE 应用，任何早退都必须离开 Resolving；普通调用语义保持不变。
4. 更新 Front/Backstab 的 Release cancellation 记录，并扩展 Player Lock-On retention 到 DeathPending。
5. 新增真实 GAS/GameplayEvent 驱动的 Automation。

冻结不变量：
- 执行致死期间 Health 可为 0，但 Dead 必须延迟到 Release 或异常收尾；
- 授权命中仍只经过 FMeleeHitResolver；
- DeathPending 只由目标 Victim Ability 所有；
- 普通伤害仍即时死亡，外部伤害不能穿透 Invulnerable；
- Dead 只通过 SetDeadState -> HandleDeath -> CancelAllAbilities -> StartDeathRagdoll；
- RAII 析构不得访问失效对象、恢复新 activation 或调用 EndAbility；
- 不全局放宽 Lock-On acquisition、Projectile Targeting 或其他 Ability。

停止条件：需要未批准路径、AttributeSet/Projectile Targeting/资产改动、Tag/Input 扩展、改变普通死亡语义，或无法证明同步重入安全时，立即停止并将证据交回 Main。

完成后仅报告实际路径、生命周期/所有权、Rider error-level 检查、git diff --check、未运行的用户门禁、实施自审 findings 和剩余风险。禁止提交。
~~~

## 阶段假设与收口

- 继续沿用单机、ServerOnly、InstancedPerActor 和 TODO-05A1-A 的成对锁定合同。
- DeathPending 使用目标 Ability 的显式瞬态 ownership，不创建新的 GameplayEffect 资产。
- CharacterAttributeSet 保持通用语义不变，pending Health 防护由 AEnemyCharacter 处理。
- 现有敌人 Ragdoll 资产和玩家处决 Montage 继续由用户维护；源码提交不声称包含可复现的 Content 基线。
- 通过编译、Automation、PIE 和 Main 的 ue-strict-review 后，Main 同步 ROADMAP.md、ROADMAP-archive.md、ARCHITECTURE.md 和 README.md；本阶段现已满足提交门禁。
- 本阶段替换前的 05A1-A 详细记录已存在于 ROADMAP-archive.md；本文件保留 B 阶段完整 handoff 与收口记录，直到下一阶段计划正式替换它。

### 实际实施结果

- 实际修改严格落在本计划的 12 个批准路径：`Config/Tags/PolyQuestGameplayTags.ini`、`ExecutionLockContext.h/.cpp`、`EnemyVictimExecutionAbility.h/.cpp`、`EnemyCharacter.h/.cpp`、`MeleeHitResolver.cpp`、`PlayerFrontExecutionAbility.cpp`、`PlayerBackstabExecutionAbility.cpp`、`PlayerCharacter.cpp` 和 `ExecutionLethalRecoveryAutomationTests.cpp`。
- 授权执行命中通过 RAII hit scope 进入 `Resolving`，致死结果收敛到 `DeathPending`；Victim Ability 独占 pending tag，并在合法 Release 或异常收尾时通过 `CommitExecutionDeath` 进入既有 `SetDeadState -> HandleDeath -> CancelAllAbilities -> StartDeathRagdoll` 链。
- 非执行伤害仍保持即时死亡语义；外部 Melee/Projectile 不能穿透执行期间的 Invulnerable。Player Lock-On 在 `DeathPending` 直到真正 Dead 前保持已拥有目标。
- 第三轮修复收紧 `OnReleaseReceived` 的 EventTag、Instigator、Target、OptionalObject 和 Context ownership 校验；null、错误或旧 Release 均 fail-closed，合法 Release 的同步顺序不变。

### 验证与复核收据

- 用户确认 `PolyQuestEditor (Development Editor)` 编译、相关 Editor readback 与 `/Game/Maps/Scene01` PIE 通过；用户确认 `PolyQuest.Combat.ExecutionLethalRecovery`、`ExecutionLockIn`、`FrontExecution`、`Backstab` 和 `Player.LockOn` Automation 通过。
- Gemini 报告 12 个批准路径的 Rider error-level 检查为 0 errors，`git diff --check` 通过；Main 本轮实际再次执行 `git diff --check` 并通过。
- Main 的 Fresh Review 先使用与基线一致的 `code-review-graph` 有界雷达，再用一次定向 CodeGraph 核对执行 Release 生命周期。首轮发现一个 P2 malformed Release 校验缺陷；第三轮修复后 delta Fresh Review 未发现 P0/P1/P2 blocker。

### 残余边界与提交

- 本阶段无未关闭的 P0-P2 blocker。受害者专用 Montage、双人完成屏障、Release 后非致死 Launch/Knockback 与其他处决表现由下一开放 `TODO-05A1-C：Release Outcomes` 单独冻结和验证。
- 提交只包含本计划 12 个阶段路径与 Main 收口文档 `plan.md`、`ROADMAP.md`、`ROADMAP-archive.md`、`ARCHITECTURE.md`、`README.md`；明确排除 `AGENTS.md`、所有 `Content/**`、`Config/Automation/Presets/1.json` 及其他用户 WIP。
