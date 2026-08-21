# TODO-01C3: Player Authored Action Cancel Windows, Bow Action Windows, And Continuous Dodge Chaining v1

## 计划状态与前置

- Plan state: Slice A CLOSED，2026-08-21；Slice B NOT STARTED and remains the current next slice.
- Baseline: `4a59212` - `[Feature] 玩家弓箭目标辅助与有限追踪 (Player Bow Target Assist And Limited Homing)`。
- 本计划替换已完成的 `TODO-03B-2` 计划记录。`TODO-03B-2` 的 Bow 瞄准、目标辅助、追踪、投射物生命周期和伤害投递边界均保持不变。
- `ROADMAP.md`、`ARCHITECTURE.md` 与 `README.md` 已在 Slice A 的用户 PIE/Automation 与 Main 复核后同步：Charging 不再无条件放行 Dodge，实际 Rate Notify 是 `UAnimNotifyState_MontageRateWindow`；Dodge recovery RateWindow 与连续链仍明确留在未开始的 Slice B。
- 当前工作树包含大量用户拥有的 `Content/**`、`Config/Automation/**`、`Config/Tests/**`、`.zcode/**`、`PolyQuest.uproject` 及 `PlayerCharacter.h` WIP。本阶段不得回滚、暂存或修改这些无关变更。

~~~
Outer: ue-stage-workflow
Primary: ue5-cpp-gameplay
Support: ue5-debug-validation
Route reason: 本阶段改变 GAS 动作取消授权、Montage Event 生命周期、Dodge 重激活和 Bow 的 Draw/Hold/Release 阶段语义；需要以源码契约、Automation 和用户 PIE 分层验证。
~~~

~~~
Plan explorers: 0
Implementation executors: 1
Complex Executor: one scoped lifecycle-sensitive implementation
Main parallel work: 在 Executor 实施期间准备不重叠的 Editor/PIE 验证清单、接收用户证据并在交付后独立审查精确 diff。
Reason: 所有改动共同作用于同一 GAS 动作生命周期，拆给多个写入者会破坏取消与 EndAbility 收敛；用户指定 Gemini 按已接受计划执行，Main 保留架构、集成、复核、文档与提交所有权。
~~~

## 目标

建立一个作者可控的玩家主动动作取消契约：Bow 的 Draw、Hold、Release/Recovery，现有 Charged Attack 的 Hold，以及 Dodge 自己的落地/恢复阶段，默认均不可被玩家主动打断；只有当前 Montage 的 `UAnimNotifyState_ActionDodgeCancelWindow` 发出的匹配 Begin/End 事件才临时开放取消。

完成后，普通 Bow 可在任意作者选择的 Draw/Hold/Release 片段放置窗口以允许 Dodge/Guard/Parry；高风险 Bow 或 Charged Attack 可以不放前摇窗口，成为不可取消的玻璃大炮动作。Dodge 也可仅在自身恢复窗口内连续触发下一次 Dodge，且每次仍独立完成体力 preflight、消耗、无敌帧和 `EndAbility()` 清理。

## 已接受运行时契约

### 1. `CancelWindow` 是唯一的玩家主动取消许可

1. `State.Action.Charging` 只表示正在 Draw/Hold/蓄力，不再是 `UDodgeAbility` 的激活豁免或取消许可。
2. 活跃的 `State.Action.Attacking` 或 `State.Action.Dodging` 存在时，Dodge 只有在 ASC 当前拥有 `State.Action.CanCancel.Dodge` 时才可通过 `CanActivateAbility()`。
3. 现有 `UAnimNotifyState_ActionDodgeCancelWindow` 仍是唯一的作者化来源。它的 Begin/End 事件必须来自当前 Avatar、当前活跃 Montage；错误 Actor、错误 Montage、过期 Ability 或 End 后事件均忽略。
4. 为保持现有 Light/Charged/Sprint/Melee Skill 语义，一个合法窗口同时授予/移除 `State.Action.CanCancel.Dodge` 与 `State.Action.CanCancel.Defense`。因此 Bow 或 Charged 的窗口允许 Dodge、Guard 与 Parry；未放窗口则三者均不允许。
5. 本阶段不引入 Allow-Dodge-but-Deny-Defense 的掩码、第二种 Notify、全局输入缓冲或通用动作状态机。只有真实动作需要不同取消权限时，才另立阶段扩展语义。

### 2. 正常输入、强制中断与投射物互不混淆

1. `Input.PrimaryAttack` Released 是 Bow 的正常 Release 请求，不是取消；即使当前没有 `CancelWindow`，Quick Tap 仍必须经 `DrawReady -> Release` 生成最多一支箭。
2. `Event.Input.Canceled`、Montage 启动失败、Montage 自然完成/中断等现有失败或清理路径继续直接收敛到该 Ability 的 `EndAbility()`；它们不等待作者化窗口。
3. 箭已经由合法 `Event.Attack.Bow.Release` 生成后，取消 Bow Recovery 不会销毁、重搜、换追或改变该 Projectile。
4. 玩家死亡/重载对已激活 Ability 的统一终结路径仍由 `TODO-03D` 所有；本阶段不得把它误报为已完成。

### 3. 统一的是 GAS 取消入口，不是全局 `Montage_Stop`

```text
当前 Montage 的 CancelWindow Begin/End
  -> 当前 Ability 临时写入/移除 CanCancel 状态 Tag
  -> 后继 Dodge / Guard / Parry 先完成自己的 CanActivate 与 Commit preflight
  -> ASC.CancelAbilities(...) 请求取消合法的当前动作
  -> 被取消动作自己的 EndAbility() 停止自己的 Montage、结束 Task、恢复 Rate、移除 Loose Tag/Effect/Requester
```

- 禁止在 `APlayerCharacter`、Notify 或新的全局管理器中直接停止他人的 Montage。
- `UBowDrawFireAbility` 已具备 `Ability.Attack.Primary`，会继续被现有 Dodge/Guard/Parry 的 GAS 取消目标集合覆盖；本阶段不为单一 Bow 用例抽取基类、接口或新的泛化 Ability Tag。
- 每个 Ability 继续拥有自身的资源清理。Bow 尤其必须由自己的 `EndAbility()` 注销 Bow Aim requester、结束事件 Task、移除动态 Charging/Cancel 状态并恢复 Rate；不能由 Dodge 代管。

### 4. Bow 阶段与 RateWindow

1. 从成功启动 Bow Montage 起到 `TriggerRelease()` 前，Bow 动态持有 `State.Action.Charging`；`TriggerRelease()` 在跳转 Release Section 前清除它，`EndAbility()` 兜底清除它。构造函数必须同时移除该 Tag 的 `AbilityTags.AddTag(...)` 与 `ActivationOwnedTags.AddTag(...)`，使 CDO 完全不静态声明 Charging。
2. `UBowDrawFireAbility` 监听现有 `Event.Action.CancelWindow.Dodge.Begin/End` 和 `Event.Action.RateWindow.Begin/End`。两个事件都必须通过 Avatar 和当前 Bow Montage 身份校验。
3. CancelWindow 可以被作者放在 Draw、Hold、Release 或 Recovery 的任意非重叠时段；代码不再按 BowState 硬编码“哪一段可取消”。
4. `UAnimNotifyState_MontageRateWindow` 是项目中已有的唯一 Rate Notify。Bow 与 Dodge 接收其正 `EventMagnitude` 作为当前 Montage 的播放倍率；重复 Begin、重叠窗口、无效倍率和错误 Montage 均 fail-closed。匹配 End 与每个 `EndAbility()` 路径必须恢复 `1.0f`。
5. RateWindow 只调整当前 Montage 的播放速率和其已存在的动画时序；不创建额外 Root Motion、Tick 写入、伤害窗口或输入状态。

### 5. Charged 与连续 Dodge

1. `UChargedAttackAbility` 的 CancelWindow Begin 不再要求 `bReleaseStarted`。Charged Hold 是否可 Dodge/Defense 完全由其 Montage 是否作者化放置窗口决定；其既有 `Charging` Tag 继续只是状态语义。
2. `UDodgeAbility` 自身新增 CancelWindow 与 RateWindow 监听、身份校验、局部授权标记和统一清理。Dodge Montage 的恢复段可作者化放置 CancelWindow；无窗口时重复 Dodge 被拒绝。
3. 为支持同一 `InstancedPerActor` Dodge 在恢复窗口内再触发，先在 UE 5.8 `UGameplayAbility` 源码/头文件中确认 `bRetriggerInstancedAbility` 的可用性与 preflight 顺序。确认后，Dodge 显式启用该引擎重激活路径，并移除会在 `Super::CanActivateAbility()` 前无条件拒绝自身的 `State.Action.Dodging` 静态 blocker；改由自身 `CanActivateAbility()` 在“正在 Dodge 且无 CanCancel”时拒绝。`ActivateAbility()` 的首段必须重新建立本次实例生命周期：复位 `bEndAbilityRequested`、无敌 Effect Handle、Cancel/Rate 状态和 Animation 引用；旧 Task 指针必须已经由前一段 `EndAbility()` 结束并置空，绝不直接覆盖仍有效的 Task 指针。
4. 不允许在 `APlayerCharacter` 中先手动结束旧 Dodge 再请求新 Dodge。新 Dodge 必须先通过正常 activation/cost preflight；用户 PIE 必须证明体力不足的重复输入不会截断正在进行的旧 Dodge。
5. 若 UE 5.8 的实际重激活语义不能保证上述顺序或无法让旧实例经 `EndAbility()` 收敛，Executor 必须停止，不得改用手写 Montage restart 或 Character 侧补丁，并将 Engine 证据交回 Main 决策。

## 实施切片与文件边界

### Slice A - 作者化取消许可与 Bow/Charged 对齐

**主要运行时问题：** 不同作者化窗口能否精确决定 Bow Draw/Hold/Release 和 Charged Hold 的主动取消，而 Quick Tap、Release 生成和已有投射物不回归？

**允许修改：**

- `Source/PolyQuest/Public/AbilitySystem/Abilities/BowDrawFireAbility.h`
- `Source/PolyQuest/Private/AbilitySystem/Abilities/BowDrawFireAbility.cpp`
- `Source/PolyQuest/Public/AbilitySystem/Abilities/DodgeAbility.h`
- `Source/PolyQuest/Private/AbilitySystem/Abilities/DodgeAbility.cpp`
- `Source/PolyQuest/Private/AbilitySystem/Abilities/ChargedAttackAbility.cpp`
- `Source/PolyQuest/Private/Tests/ProjectileLifecycleAutomationTests.cpp`
- `Source/PolyQuest/Private/Tests/PlayerActionWindowAutomationTests.cpp` (new)

**实施顺序：**

1. 在 `UDodgeAbility::CanActivateAbility()` 移除 `State.Action.Charging` 放行；保留所有既有地面、死亡、眩晕、精力和攻击取消校验。同步从 `ActivateAbility()` 移除 `bWasCharging` 及其参与的 Light/Charged/Sprint 取消分支，取消目标只能由已验证的 `bCanCancelAttack` 决定。为之后自身重激活预留明确的 `State.Action.Dodging` + `CanCancel.Dodge` 分支，不接受模糊的 `!bIsAttacking` 绕过。
2. 在 Bow 中添加仅私有的 CancelWindow/RateWindow Event Task、当前 Montage 身份校验、局部 `bDodgeCancelable`、动态 `SetCharging(...)` 和 Rate baseline restore。仅在成功确认 Montage 活跃后开始 Charging；在 Release 跳转前和任意 End 路径清除。
3. Bow 的合法 CancelWindow Begin/End 按现有项目行为同时维护 Dodge/Defense CanCancel Loose Tags；所有 Task 创建失败、错误事件、重复事件和 teardown 均 fail-closed。
4. 移除 Charged 的 `bReleaseStarted` CancelWindow Begin 限制，使其也遵循同一作者化许可；不改变 Charged HoldReady、伤害、Poise、Trace 或 Release handoff。
5. 更新旧 `ProjectileLifecycle` 断言：Bow CDO 不再静态拥有 `State.Action.Charging`；新增测试必须验证动态持有/清理的行为而不是把它重新写回 CDO。

**Slice A 非目标：** 不修改 Projectile、Target Assist、Homing、WeaponEquipment、PlayerCharacter 输入、Light/Sprint/Melee Skill 既有窗口、Guard/Parry 生命周期、Gameplay Tag 配置或任何 `Content/**`。

### Slice A 收尾记录（已完成）

- 实现完成：`UDodgeAbility` 移除了 `Charging` 取消豁免和由其派生的攻击取消路径；Bow 将 Charging 改为成功启动 Montage 后才添加的动态 Loose Tag，并拥有匹配 CancelWindow/RateWindow 的 Task、身份校验和 `EndAbility()` 清理；Charged Hold 只锁存 HoldReady 时已打开的合法窗口，暂停诱发的伪 End 不清理该许可，恢复播放后的真实 End 与所有 `EndAbility()` 路径仍会清理。
- 已修复的 Main Fresh Review 问题：共享 Notify 发送方恢复把真实 `Animation` 写入 `OptionalObject`，不再把旧 Notify 错归属给当前 Montage；Bow 对 Montage 内 Sequence 的身份回退有真实 `UAnimComposite` fixture 覆盖，外来 Sequence 被拒绝。
- 验证：用户确认 Focused PIE 通过；Automation 成功包括 `PolyQuest.Player.ActionWindows`、`PolyQuest.Projectile.Lifecycle`、`PolyQuest.Projectile.TargetAssist`、`PolyQuest.Equipment.TransactionMatrix`、`PolyQuest.Melee.TraceSourceGeometry`、`PolyQuest.Enemy.AttackSetSelection` 与 `PolyQuest.Enemy.CombatSpacing`。这不是 Main 运行的编译或 PIE 证据。
- 复核：Main 正常 Fresh Review 无 P0-P2；Luna 不可用，已完成并明确标注 Main adversarial fallback。它验证暂停锁存不能给无窗口 Hold 授权、恢复后的真实 End 仍移除许可、Bow 在 Release/所有 End 路径移除动态 Charging、发送方保持源 Animation，以及 Dodge 不会因 Charging 单独放行。
- 静态证据：CodeGraph 已覆盖当前 C++ 调用链与直接依赖；`code-review-graph` 按 `4a59212` 做增量更新，但当前 C++ 图返回 `0 nodes / 0 edges`，因此其风险提示仅作覆盖不足提示，最终结论基于直接 diff、当前源码、配置 Tag 与用户运行时证据。`git diff --check` 通过。
- 债务交接：`ROADMAP.md` 的 `TODO-01C3` 父项保留为未完成，Slice B 唯一拥有 Dodge recovery RateWindow、连续重激活和 UE 5.8 retrigger preflight 顺序门禁；玩家死亡/重载的统一终结仍由 `TODO-03D` 所有。本 Slice A 没有未登记的接受风险。
- Main delegation record: Plan explorers: 0; Implementation executors: Gemini (approved Slice A only); Complex Executor: none; Main parallel work: architecture, review, documentation, staging, and commit. Reason: all shared GAS lifecycle and public-action contracts remain Main-owned.

### Slice B - Dodge Recovery、Rate 与连续重激活

**主要运行时问题：** Dodge 能否只在其作者化恢复窗口内安全地开始下一段 Dodge，并让所有旧实例资源经原有 `EndAbility()` 清理？

**允许修改：** Slice A 列出的 `DodgeAbility.h/.cpp` 与 `PlayerActionWindowAutomationTests.cpp`；如需补充现有 Bow 自动化断言，仅限 `ProjectileLifecycleAutomationTests.cpp`。不得扩展到其他源文件。

**实施顺序：**

1. 在确认 UE 5.8 引擎 API 后，为 Dodge 启用受控的 `InstancedPerActor` retrigger；保留 `State.Action.Dodging` 为实际动作状态，删除仅导致 Super 提前拒绝的静态 self-blocker，并用 `CanActivateAbility()` 的 CanCancel 条件取代它。重激活后 `ActivateAbility()` 必须显式复位 `bEndAbilityRequested`、Invulnerability Handle、`bDodgeCancelable`、`bRateWindowApplied`、Bound Anim/Active Montage 等瞬态状态；每个旧 Task 必须先在旧 `EndAbility()` 中 `EndTask()` 并置空，不能以赋空覆盖悬挂 Task。
2. 增加 Dodge 的 CancelWindow Begin/End 与 RateWindow Begin/End Task。所有回调必须验证当前 Avatar 和 `ActiveMontage`；合法窗口才维护当前实例拥有的 CanCancel tags，匹配 End 或 `EndAbility()` 必须移除；Rate 始终在 Montage stop 前恢复 `1.0f`。
3. 保持 Dodge 的无敌 GE、Root Motion Montage、动作朝向、Guard cancel、Stamina Commit 和现有 Montage End delegate 的所有权不变。重激活引起的旧实例结束必须复用 `EndAbility()`，不能复制清理代码或留下 `State.Status.Invulnerable`。
4. 先完成自动化正负向覆盖、静态检查，再交由用户执行 Compile/Editor/PIE Gate A；Gate A 通过后才允许在 Dodge Montage 中作者化恢复窗口并执行连续翻滚 Gate B。

**Slice B 非目标：** 不实现移动输入取消、Light Attack 取消、Potion、按键缓冲、Dodge/Sprint 物理输入重做、网络复制、全局 Action Manager 或任何新的 Action Window 类型。

## Automation 与静态验证要求

### 新建 `PolyQuest.Player.ActionWindows`

新增 `PlayerActionWindowAutomationTests.cpp`，使用临时 World、`APlayerCharacter`、ASC 和最小 Mock Montage。允许在 `#if WITH_DEV_AUTOMATION_TESTS` 下为 Bow/Dodge/Charged 提供极窄的测试注入器；生产路径不得因测试而暴露 Blueprint/Public API。

至少覆盖：

1. 已注册的 CancelWindow/RateWindow/CanCancel/Charging tags 可解析；Bow CDO 仍有 `Ability.Attack.Primary` 和攻击状态，但不再静态拥有 Charging。
2. Bow 在动态 Draw/Hold 状态拥有 Charging；进入 Release 或任何 End 清理后不再拥有。Charging 自身不能使 Dodge `CanActivateAbility()` 成功。
3. Bow 的错误 Avatar、错误 Montage、过期状态、重复 Begin/End 均不能写入或泄漏 CanCancel tags；正确 Begin/End 同时写入/移除现有 Dodge/Defense tags。
4. Charged 的合法 Hold 阶段 CancelWindow 能打开取消许可；无窗口仍不许可。保留其有效 Release-time Trace 行为的既有覆盖。
5. Dodge 在攻击或自身 Dodge 状态下：无 CanCancel 拒绝，有匹配 CanCancel 才接受；Dodge 的错误/过期 Notify 事件不得影响其他 Active Montage。
6. 重复/无效 Rate Begin、错误 Montage、匹配 End 与 `EndAbility()` 的 Rate baseline/局部状态清理；不把 Automation 的静态代理当作真实 Root Motion 或 Montage 视觉证明。

### 既有回归与静态检查

- 更新并运行 `PolyQuest.Projectile.Lifecycle`，保留 Bow Quick Tap、Release identity 和单 Projectile 生命周期覆盖。
- 运行新增 `PolyQuest.Player.ActionWindows`，以及现有 `PolyQuest.Projectile.TargetAssist`、`PolyQuest.Equipment.TransactionMatrix`、`PolyQuest.Melee.TraceSourceGeometry`、`PolyQuest.Enemy.AttackSetSelection`、`PolyQuest.Enemy.CombatSpacing`。
- Executor 在请求用户 Compile 前须读取最终变更及直接调用方，核对 `Config/Tags/PolyQuestGameplayTags.ini` 的现存 Tag 名称，执行 `git diff --check`。不得调用 UBT、Build.bat、UAT、Packaging 或修改 Editor。
- CodeGraph 覆盖 C++ 调用链；CodeGraph/code-review-graph 不能证明 Montage Notify 资产放置、Root Motion、输入手感或 PIE 视觉。

## 用户拥有的 Editor、Compile 与 PIE Gate

### Editor 作者化与读回

1. 在 `UDodgeAbility` 实际引用的 Dodge Montage 恢复/落地段放置 `ActionDodgeCancelWindow`。窗口以外不得形成连续翻滚；现有 `DodgeInvulnerability` 的位置不因本阶段被随意移动。
2. 在 Bow Montage 中按想要的玩法放置 `ActionDodgeCancelWindow`：普通 Bow 可在 Draw/Hold/Release 任意段开放；玻璃大炮 Bow 的不可取消前摇不得放置窗口。确认任何窗口的 Begin/End 都来自同一 Bow Montage。
3. 在 Charged Montage 的 Hold 中只在希望允许取消的位置放置同一 Notify；未放置的 Hold 必须保持不可取消。
4. 若需要调整速度，使用已存在的 `Montage Rate Window`，`RateMultiplier > 0` 且同一 Montage 不重叠。它可以作者化用于 Draw、Hold、Release 或 Dodge Recovery；不创建 `ActionRateWindow` 类。
5. 读回每个触及的 Montage、GA 默认值和对应 Notify 类型；资产、Blueprint、AnimBP、Input、Map、DataAsset 的本地变更仍由用户保有，不纳入本阶段源码提交。

### 手动 Compile

- 用户在 Visual Studio 2022 编译 `PolyQuestEditor`（Development Editor），并报告首个编译错误或成功加载结果。Main/Executor 均不得代替用户启动构建。

### Scene01 PIE / Standalone 验证

1. Bow Draw、Hold、Release 三段各验证一次：窗口外按 Dodge 不会截断 Bow；窗口内按 Dodge 会经 Bow `EndAbility()` 截断，且箭在 Release Notify 前绝不生成。
2. Quick Tap 仍走正常 DrawReady 后的最小 Release，不因没有 CancelWindow 而卡在 Hold，也不生成两支箭。
3. Release Notify 后的 Bow Recovery：窗口外不能 Dodge/Guard/Parry，窗口内可以；已生成箭继续其既有直飞/追踪，不被 Bow 收招取消影响。
4. Charged Hold：未放窗口不可 Dodge；放窗口后可取消。确认 Charged Release、Trace、伤害和体力成本没有回归。
5. Dodge：无恢复窗口的重复输入不重启；恢复窗口内连续两次以上 Dodge，每次各扣一次 Stamina、各拥有独立无敌窗口、朝向连续；体力不足时下一次请求失败且当前 Dodge 不被提前截断。
6. 在 Bow 或 Dodge 的 RateWindow 内观察播放速率变化；正常结束、Dodge/Guard/Parry 取消、输入取消后再次播放同 Montage 时均回到 `1.0f`，没有遗留 CanCancel、Charging、Dodging、Movement/Jump Block 或 Invulnerable tag。

## 严格复核与文档收尾

1. Gemini 完成实现后只提交严格实现自审：按批准文件面、直接调用方、负向事件、旧实例 teardown、成本/重激活顺序和测试缺口报告，不得把自审称为独立 Fresh Review，不得写文档或提交。
2. Main 进行独立正常 Fresh Review；阶段关闭时如 `gpt-5.6-luna / xhigh` 可用，按既定流程进行只读独立 Reviewer；不可用则明确记录 Main adversarial fallback。
3. Main 在用户 Compile/PIE 证据、Automation 成功和最终复核后更新：
   - `ARCHITECTURE.md`：稳定的作者化取消、动态 Charging、Rate 与 Ability-owned cleanup 边界；
   - `ROADMAP.md`：修正 `TODO-01C3` 的无条件 Charging/错误 Notify 名称，并记录完成事实；
   - `README.md`：只更新真实完成状态；
   - `plan.md`：保留本阶段验证、复核、债务和提交边界直到下一阶段替换。

## 债务、非目标与停止条件

- `TODO-03D` 仍拥有玩家死亡/重载时取消所有活跃 Ability 的终结契约；本阶段不扩大到 Death Reload。
- 现有窗口同时开放 Dodge/Defense。需要按动作独立区分 Dodge、Guard、Parry、Potion 或攻击取消时，必须先有真实玩家需求与单独接受的语义设计；本阶段不提前制作 mask/priority 系统。
- Light Attack、移动输入、Potion、网络、投射物、Target Assist、Homing、敌人 AI、装备事务、存档和任何 Content 迁移均非目标。
- 若 `bRetriggerInstancedAbility` 的 UE 5.8 实际行为与“先通过 successor preflight，再令旧 Dodge 经 EndAbility() 收敛”不一致，或同一实例无法在不破坏 Task/GE 清理的情况下重激活，停止实施 Slice B 并把源码/复现证据交回 Main；不得以 Character 侧手动 Montage restart 代替。

## 提交边界

- Slice A 提交仅包括：`BowDrawFireAbility.h/.cpp`、`ChargedAttackAbility.h/.cpp`、`DodgeAbility.h/.cpp`、`ProjectileLifecycleAutomationTests.cpp`、新增 `PlayerActionWindowAutomationTests.cpp`，以及 Main 更新后的 `README.md`、`ARCHITECTURE.md`、`ROADMAP.md`、`plan.md`。当前计划不需要 `Config/Tags/PolyQuestGameplayTags.ini` 变更，因为所需 Tag 已静态核对存在。
- 明确排除：所有 `Content/**`、`Config/Automation/**`、`Config/Tests/**`、`.zcode/**`、`PolyQuest.uproject`、`PlayerCharacter.h` 的当前 WIP、Map/Blueprint/AnimBP/Montage/DataAsset/Input/导入资源及其他无关用户变更。
- Slice A 已有用户确认的 PIE/Automation、Main Fresh/Adversarial Review 与债务交接；本次提交按该已授权边界精确暂存。不得把用户的 Automation/PIE 反馈夸大为 Main 执行的手动编译证据，禁止 `git add -A`。
