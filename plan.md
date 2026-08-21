# TODO-01C3 Slice B: Dodge Recovery Rate Window And Continuous Retrigger Chaining v1

## 计划状态与前置

- Plan state: COMPLETED, 2026-08-22. Slice A closed in `2cbff1b` (`[Feature] 玩家作者化动作取消窗口 Slice A (Player Authored Action Cancel Windows Slice A)`); Slice B is closed by the pending focused commit.
- Baseline: `2cbff1b`.
- 本计划有意替换 Slice A 的收尾记录，Slice A 的稳定实现事实已经进入 `README.md`、`ARCHITECTURE.md`、`ROADMAP.md` 与提交历史。
- 当前工作树仍包含用户拥有的大量 `Content/**`、`Config/Automation/**`、`Config/Tests/**`、`.zcode/**`、`PolyQuest.uproject` 与 `PlayerCharacter.h` WIP。本阶段不得回滚、暂存或修改这些无关变更。
- 已完成 UE 5.8 源码门禁：`D:\UE\UE_5.8\Engine\Plugins\Runtime\GameplayAbilities\Source\GameplayAbilities\Private\AbilitySystemComponent_Abilities.cpp` 的 `InternalTryActivateAbility()` 先在 1817 行调用 `CanActivateAbility()`，再在 1831-1845 行为已激活的 `InstancedPerActor` 且 `bRetriggerInstancedAbility` 为 true 的 Ability 调用旧实例 `EndAbility()`。`UGameplayAbility::CanActivateAbility()` 在 `GameplayAbility.cpp:518` 执行 `CheckCost()`；`UStaminaActionAbility::CheckCost()` 要求当前 Stamina 大于零。因此体力不足会在旧 Dodge 结束前被拒绝，满足本 Slice 的前置顺序契约。
- 该引擎证据不消除既有的 post-commit 原子性债务：`CommitAbility()` 在旧实例结束后仍会再次检查成本。该一般性风险已由 `ROADMAP.md` 的既有 post-commit playback-failure 项拥有，本 Slice 不扩大到资源事务重做，也不得宣称完全原子化。

~~~
Outer: ue-stage-workflow
Primary: ue5-cpp-gameplay
Support: ue5-debug-validation
Route reason: 本 Slice 修改一个 InstancedPerActor GAS Ability 的重激活、动画事件和 EndAbility 生命周期；需要以 UE 5.8 引擎实际调用顺序、最小 C++ 变更和可观察的 PIE 行为共同定义边界。
~~~

~~~
Plan explorers: 0
Implementation executors: Gemini (one bounded implementation executor)
Complex Executor: one scoped lifecycle-sensitive implementation
Main parallel work: architecture guardrails, final fresh/adversarial review, documentation closeout, staging, and commit preparation after executor handoff
Reason: Main has already fixed the action-cancellation policy, engine retrigger order, allowed paths, non-goals, and stop conditions. Gemini may implement only this bounded `UDodgeAbility` plus Automation slice; it may not revise contracts, alter input routing, expand behavior, write documents, stage, or commit.
~~~

## 目标

让玩家仅在 Dodge Montage 的作者化恢复窗口内启动下一段 Dodge。连续 Dodge 必须由 UE 的 `bRetriggerInstancedAbility` 路径收敛旧实例，旧实例仍使用自己的 `EndAbility()` 清理无敌 GE、Montage delegate、Task、临时取消标签与播放速率；现有 `APlayerCharacter::RequestDodgeAbility()` 保持不变。

完成后，恢复窗口外的第二次短按仍不会重启当前 Dodge；恢复窗口内可连续触发下一 Dodge；Stamina 为零时第二次请求失败且当前 Dodge 继续自然播放。Dodge Recovery 可以通过已有 `UAnimNotifyState_MontageRateWindow` 改变当前 Montage 速率，且任意结束路径回到 `1.0f`。

## 已接受运行时契约

### 1. 重激活顺序与许可

1. `UDodgeAbility` 保持 `InstancedPerActor` 与单机 `ServerOnly`，并在其 native CDO 上启用 `bRetriggerInstancedAbility`。
2. 从 `ActivationBlockedTags` 移除 `State.Action.Dodging`，因为它会在 `Super::CanActivateAbility()` 阶段抢先拒绝重激活；`State.Action.Dodging` 本身仍由每段 Dodge 的 `ActivationOwnedTags` 真实拥有。
3. `UDodgeAbility::CanActivateAbility()` 在通过 `Super`、地面与既有终态门禁后，显式读取 `State.Action.Attacking`、`State.Action.Dodging` 与 `State.Action.CanCancel.Dodge`：任一前两者存在时，只有当前 `CanCancel.Dodge` 才放行；两者都不存在时保持现有普通 Dodge 放行。不得用 `!bIsAttacking` 绕开活跃 Dodge。
4. 引擎先做该完整 preflight（含 `UStaminaActionAbility::CheckCost()`），成功后才结束旧实例。因此 Stamina 为零的连续请求不结束旧 Dodge；通过 preflight 的请求可由引擎调用旧实例 `EndAbility()`，之后重用同一 Ability 实例开始新段。
5. 不修改 `APlayerCharacter::RequestDodgeAbility()`、物理输入、计时器或 Montage 手工 restart；它已有的 `TryActivateAbilitiesByTag(Ability.Dodge)` 是唯一输入入口。

### 2. Dodge Recovery Window 与 Rate Window

1. `UDodgeAbility` 添加当前 Dodge Montage 专属的 `UAbilityTask_WaitGameplayEvent` 监听：`Event.Action.CancelWindow.Dodge.Begin/End` 与 `Event.Action.RateWindow.Begin/End`。所有回调继续用当前 Avatar、`ActiveMontage` 或该 Montage 内部 Sequence 的身份校验；错误 Actor、外来 Animation、过期实例、重复 Begin/End、非正倍率均 fail-closed。
2. 匹配 Dodge CancelWindow 仅由 Dodge 自己添加/移除 `State.Action.CanCancel.Dodge`。本 Slice 不添加 `State.Action.CanCancel.Defense`，也不改 Guard、Parry、Light Attack 或移动的 `State.Action.Dodging` blocker；Dodge Recovery 目前只开放连续 Dodge，不承诺从 Dodge 恢复直接转 Guard、Parry、轻击或移动。
3. 匹配 RateWindow Begin 只在未应用倍率、动画实例有效且当前 Dodge Montage 活跃时设置 `Montage_SetPlayRate`；匹配 End 和任一 `EndAbility()` 路径都在停止 Montage 前恢复 `1.0f`。RateWindow 不可重叠、不可叠乘。
4. 重激活的旧 `EndAbility()` 必须依次清除无敌 GE、Dodge Cancel tag、Rate baseline、Montage delegate 与全部旧 Task；不能只将指针赋空。新的 `ActivateAbility()` 只在旧清理后重新建立本段临时状态，明确复位 `bEndAbilityRequested`、无敌 Handle、Cancel/Rate 标志、`BoundAnimInstance` 与 `ActiveMontage`。

### 3. 保留边界

1. Slice A 的 Bow/Charged 作者化 CancelWindow、动态 Charging、暂停锁存、投射物与目标辅助不改动。
2. 既有 Dodge 无敌 Notify、Root Motion、动作朝向、Guard cancel、Stamina 成本和 regen-delay 所有权不迁移。
3. 不实现按键缓冲、长按改造、Dodge-to-Light/Guard/Parry、Potion、Sprint 物理重做、网络预测、全局动作管理器、第二种 Action Window 或 Content 资产迁移。
4. 不把用户 PIE 的动画表现、输入手感或资产放置替换成 Automation 结论；Automation 只能覆盖原生状态与事件契约。

## 实施边界与顺序

### 允许修改

- `Source/PolyQuest/Public/AbilitySystem/Abilities/DodgeAbility.h`
- `Source/PolyQuest/Private/AbilitySystem/Abilities/DodgeAbility.cpp`
- `Source/PolyQuest/Private/Tests/PlayerActionWindowAutomationTests.cpp`
- 收尾时仅由 Main 更新：`README.md`、`ARCHITECTURE.md`、`ROADMAP.md`、`plan.md`

### 明确禁止修改

- `Source/PolyQuest/Public/Character/Player/PlayerCharacter.h`、`PlayerCharacter.cpp`、任何其他 Ability、Gameplay Tag Config、Build.cs、`PolyQuest.uproject`、`Config/**`、`Content/**`、Blueprint、Montage、AnimBP、DataAsset、Input、Map 与导入资源。

### 实施顺序

1. 在 `UDodgeAbility` native CDO 启用 retrigger，新增独立 `DodgingStateTag`，移除自我 static blocker，并将 `CanActivateAbility()` 收敛为“攻击或 Dodge 活跃均需 `CanCancel.Dodge`”的显式判定。
2. 为 Dodge 增加私有 CancelWindow/RateWindow Task、局部 `bDodgeCancelable` / `bRateWindowApplied`、匹配回调、`SetDodgeCancelable(...)` 与 `RestoreBaselineMontageRate()`；复用当前 Sequence-aware Montage 身份校验，不改共享 Notify 发送方。
3. 在 `ActivateAbility()` 与 `EndAbility()` 以旧实例 cleanup 为前提完成状态重置与 Task 收敛。严禁覆盖仍有效的 Task 指针、手工终止前一段 Montage，或在 Commit 前取消旧 Dodge。
4. 扩展 `PolyQuest.Player.ActionWindows`：验证 native CDO retrigger 与无 self-blocker；攻击/活跃 Dodge 在无窗口时拒绝、有匹配 `CanCancel.Dodge` 时才接受；无效/外来/重复 CancelWindow 与 RateWindow 不改状态；匹配 End 和 `EndAbility()` 清理标签与速率状态。若需要测试注入器，只能放在 `#if WITH_DEV_AUTOMATION_TESTS` 下，不能扩大生产 Public API。
5. 在请求用户验证前执行最终源码复读、`git diff --check`、现有 Tag 静态核对与 Main 轻量自审。不得启动 UBT、Build.bat、UAT、Packaging 或 Editor。

## 用户拥有的验证门

### Editor 读回

1. 确认实际使用的 Dodge GA 未覆盖 native `bRetriggerInstancedAbility = true`。
2. 仅在 Dodge Montage 的落地/恢复段放置现有 `UAnimNotifyState_ActionDodgeCancelWindow`；窗口外不应产生连续 Dodge。保留既有 `UAnimNotifyState_DodgeInvulnerability` 的位置与时长。
3. 如需调速，在同一恢复段放置现有 `UAnimNotifyState_MontageRateWindow`，`RateMultiplier > 0` 且不与另一 RateWindow 重叠。读回 Notify 类型、区间与 GA 默认值。

### 手动 Compile

- 用户在 Visual Studio 2022 编译 `PolyQuestEditor`（Development Editor），并报告首个错误或成功加载结果。Main 不代替用户启动构建。

### Scene01 PIE

1. 无恢复 CancelWindow 时，短按 Dodge 不会重启当前 Dodge。
2. 在恢复窗口内连续触发至少两次 Dodge：每段独立扣一次 Stamina、独立进入自己的无敌 Notify、朝向连续、旧段不会留 `Invulnerable`、`CanCancel.Dodge`、Movement/Jump Block 或 Rate 残留。
3. Stamina 为零时，在恢复窗口内尝试下一 Dodge：请求失败，当前段不提前停止，且不产生新的无敌/成本。
4. RateWindow 内确认恢复播放速率变化；正常结束、重激活、外部取消和下一次开始同 Montage 后均回到 `1.0f`。
5. 确认 Dodge Recovery 本阶段只连向下一 Dodge；Guard、Parry、轻击和移动不因该窗口获得新行为。

## 自动化与静态验证

- 更新 `PolyQuest.Player.ActionWindows`，并回归运行 `PolyQuest.Projectile.Lifecycle`、`PolyQuest.Projectile.TargetAssist`、`PolyQuest.Equipment.TransactionMatrix`、`PolyQuest.Melee.TraceSourceGeometry`、`PolyQuest.Enemy.AttackSetSelection`、`PolyQuest.Enemy.CombatSpacing`。
- 所有日志中的既有负向 fixture Warning 与实际测试失败分开记录；只有 `Success` 是 Automation 通过证据。
- CodeGraph 用于直接 C++ 调用链；code-review-graph 只作变更/影响补充。两者都不能证明 GA 默认值、Notify 放置、Root Motion、实际输入与 PIE 视觉。

## 停止条件与债务交接

- 若实际 `TryActivateAbilitiesByTag(Ability.Dodge)` 在 C++ preflight 已通过后仍因 Blueprint CDO 覆盖、实例重用、Task/GE 清理或引擎行为导致旧段不能经 `EndAbility()` 收敛，停止实现并带最小复现证据回到 Main；不得改为 Character 侧手动停 Montage。
- 若用户需要 Dodge Recovery 直接转 Guard、Parry、Light Attack 或移动，先单独定义各后继 Ability 的 static blocker、Cost 顺序与输入语义；这不是本 Slice 的隐含行为。
- `TODO-03D` 继续拥有死亡/重载取消所有活跃 Ability 的终结契约。
- `ROADMAP.md` 已有的 post-commit 成本/播放失败原子性债务继续保留；本 Slice 只保证引擎 preflight 中 Stamina 不足不会终止旧 Dodge。

## 文档与提交边界

- 本 Slice 在用户 Compile/PIE、Automation 成功、Main 正常与对抗性复核、债务交接及用户明确批准后，再由 Main 更新稳定文档并创建独立提交。
- 预期提交仅包含上述 Dodge 源码、动作窗口测试及收尾文档；明确排除所有用户 `Content/**`、`Config/**`、`.zcode/**`、`PolyQuest.uproject` 与 `PlayerCharacter.h` WIP。禁止 `git add -A`。

## 收尾记录（已完成，2026-08-22）

- 实现：`UDodgeAbility` 启用 `InstancedPerActor` re-trigger，移除静态 self-Dodging blocker，并在攻击或 Dodge 活跃时只接受当前 `State.Action.CanCancel.Dodge`。Dodge 自己监听匹配的 CancelWindow/RateWindow，清理其无敌 GE、Loose Tag、播放速率和全部 Task。
- 引擎与根因：UE 5.8 的 successor preflight（包括 Stamina）先于旧实例 `EndAbility()`；因此 Stamina 不足不会终止旧段。PIE 日志定位到旧的全局 `UAnimInstance::OnMontageEnded` 广播会误结束新段，修复为每个 `UAbilityTask_PlayMontageAndWait` 的实例绑定回调。Main fresh review 随后发现 `OnBlendOut` 会提前停止仍 active 的 Montage；该绑定已移除，正常结束只经 `OnCompleted` 收敛。
- 验证：用户确认最终相关 Automation 通过，并确认 Scene01 PIE 通过。该记录是用户拥有的运行时证据；Main 未运行编译、Editor 或 PIE。
- 复核：Gemini 完成实现自审；Main Fresh Review 和 Main adversarial fallback 均在最终 delta 上未发现 P0-P2。Luna 不可用，未声明独立 Reviewer 结果。`git diff --check` 通过。
- 债务交接：本 Slice 没有新增已接受风险。玩家死亡/重载取消所有活跃 Ability 继续由 `TODO-03D` 负责；既有 post-commit 成本/播放失败原子性债务仍由 `ROADMAP.md` 的既有条目拥有。
- 提交范围：仅 `DodgeAbility.h/.cpp`、`PlayerActionWindowAutomationTests.cpp`、`README.md`、`ARCHITECTURE.md`、`ROADMAP.md` 与本计划；明确排除所有用户 `Content/**`、`Config/**`、`.zcode/**`、`.uproject` 和 `PlayerCharacter.h` WIP。
