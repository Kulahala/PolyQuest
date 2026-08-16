# plan.md - AI Agent Collaboration File

## Rules

1. Read this file before continuing a PolyQuest stage.
2. Keep feedback, active plan, validation evidence, and unresolved blockers scoped to the current stage.
3. The implementation author cannot mark a stage as review-approved.
4. Retain the completed plan and closeout record until the next accepted stage deliberately replaces it.
5. Durable architecture belongs in `ARCHITECTURE.md`; accepted future direction and validation debt belong in `ROADMAP.md`.

---

## Active Stage: TODO-02D3 - Enemy Notify-Timed Hyper Armor v1

Baseline: `ed7b9e0 [Feature] 定时弹反与敌人韧性反制 (Timed Parry And Enemy Poise Counter)`.

### Objective

为首个 Goblin 的现有 `UEnemyMeleeAbility` 增加由攻击 Montage NotifyState 驱动的单一、连续、非重叠 Hyper Armor 窗口。窗口只覆盖作者化的 `AttackTraceWindow`，不覆盖前摇或收招。窗口内 `Data.Reaction.Interrupt` 仍正常结算 Health/Poise，但 C3B `EnemyHitReaction` 被 GAS 阻止；C3C Stance Break、死亡 teardown、攻击中断和 Actor teardown 仍拥有更高优先级并清理窗口。

### Locked Runtime Contract

- 在 `Config/Tags/PolyQuestGameplayTags.ini` 新增且只新增 `State.Status.HyperArmor`、`Event.Attack.HyperArmor.Begin`、`Event.Attack.HyperArmor.End`。
- 在现有 `Animation/Combat/AnimNotifyState_ActionWindows.*` 增加 `UAnimNotifyState_EnemyHyperArmor`。`NotifyBegin`/`NotifyEnd` 只通过既有 `SendGameplayEvent` helper 发送对应事件，并把当前 Animation 放入 `OptionalObject`；不直接写 Health、Poise、Ability、Movement、Collision 或 AI。
- `UEnemyMeleeAbility` 监听两个事件；事件必须在 Ability 未结束、`bAttackStarted` 为真，且 `Instigator`、`Target`、`OptionalObject` 分别精确匹配当前敌人和 `ActiveMontage` 时才接受。Begin 只接受一次并用 `SetLooseGameplayTagCount(State.Status.HyperArmor, 1)` 写入 loose tag；End 设为 `0`。统一 `EndAbility()` 先清理 Hyper Armor，再停止 Montage、Trace Task 和事件 Task，任何中断、死亡、Stance Break、无效 Montage 或拆除都不能残留 Tag。
- Hyper Armor 不进入 `ActivationOwnedTags`，不延长攻击生命周期、不改攻击冷却、不改 Trace 语义；没有 Notify 时攻击保持现有 C3B 行为。
- `UEnemyHitReactionAbility` 将 `State.Status.HyperArmor` 加入 `ActivationBlockedTags`，并在 `ValidateActivationSetup()` 要求该 Tag 有效。霸体期间事件不排队、不延迟补播。
- 不修改 `EnemyStanceBreakAbility`、`EnemyCharacter`、`AEnemyAIController`、StateTree、Resolver、Attack Profile、Damage GE、Build.cs 或任何资产路径。

### Route And Ownership

```text
Outer: ue-stage-workflow
Primary: ue5-cpp-gameplay
Support: ue5-blueprint-workflow, ue5-debug-validation
Route reason: Hyper Armor loose-tag ownership, Montage event identity, EnemyMeleeAbility teardown, and C3B/C3C/death priority share one GAS lifecycle boundary.
```

```text
Plan explorers: 0
Implementation executors: 0
Complex Executor: none
Main parallel work: none
Reason: EnemyMeleeAbility, HitReaction, StanceBreak, and death cleanup share one lifecycle; concurrent writers would race the same tag ownership.
```

Main owns native source/config, static checks, review, documentation, staging and commit boundaries. The user owns Content authoring, Editor readback, manual `PolyQuestEditor` compilation, Scene01 PIE and final commit approval.

### User-Owned Editor Gate

在当前 `BP_Enemy_Goblin` 实际使用的攻击 Montage 中放置一个 `Enemy Hyper Armor` NotifyState，并让 Begin/End 与现有 `AttackTraceWindow` 完全重合。每个 Montage 只保留一个连续窗口；不新增 GA/GE，不替换 Montage、Skeleton、WeaponMesh、BladeTrace 或现有 Trace Notify，不修改 StateTree、AnimBP、Physics Asset、碰撞、地图或 Root Motion 设置。

### Validation And Commit Boundary

Main 只做最终源码/调用方审读、CodeGraph、Tag 交叉核对、C4458 遮蔽扫描和 `git diff --check`，不运行 UBT、Visual Studio、Editor 或 PIE。用户编译并验证窗口内外 Charged Interrupt、Health/Poise、C3C/Dead 优先级和所有 teardown 无残留。通过用户验证和 review 后，才同步 `ARCHITECTURE.md`、`ROADMAP.md`、本文件的 D3 closeout；候选提交仅包含 D3 白名单源码、Tag 配置和精确文档 hunk，排除全部 `Content/**` 与其他 WIP。

### Implementation And Closeout Record (2026-08-16)

#### Native Implementation

- Added the three D3 tags and `UAnimNotifyState_EnemyHyperArmor` to the existing combat action-window source group. The NotifyState only emits Begin/End Gameplay Events with the current Animation in `OptionalObject`.
- `UEnemyMeleeAbility` owns two persistent event tasks, exact current-Montage/actor identity filtering, one `bHyperArmorActive` duplicate guard, and the precise loose-tag count. It clears Hyper Armor before every Montage, Trace Window, event-task, and cooldown teardown continuation.
- `UEnemyHitReactionAbility` blocks only while `State.Status.HyperArmor` exists. C3C Stance Break, Death, Controller, StateTree, Attack Profile, and the shared Resolver were intentionally left unchanged.

#### User Validation Evidence

- The user confirmed `PolyQuestEditor` compilation and the authored Scene01 PIE route after placing one continuous `Enemy Hyper Armor` NotifyState over the existing Attack Trace Window. Main did not run UBT, Editor, or PIE.

#### Review Record

- Main normal review found no P0-P2 defect after direct source/caller review, CodeGraph, tag cross-check, C4458 scan, and `git diff --check`.
- `gpt-5.6-luna / xhigh` was unavailable. Main therefore completed a separate adversarial fallback over Notify/Trace same-frame ordering, duplicate events, identity filtering, C3B blocking, C3C/Dead priority, and every EnemyMelee teardown route; no P0-P2 defect was found. This is not an independent review.
- `code-review-graph` was consulted only as stale supplemental evidence: its graph is built on `be3fdfd`, behind the `ed7b9e0` D3 baseline. Direct source and user PIE evidence are the coverage basis.

#### Documentation And Commit Boundary

- `ARCHITECTURE.md` records the stable NotifyState -> EnemyMelee loose tag -> C3B block contract, including C3C/Dead precedence and the v1 one-window limit. `ROADMAP.md` moves D3 into Done Milestones with no new debt entry.
- The approved commit set is exactly the D3 Tag config, EnemyMelee, EnemyHitReaction, action-window NotifyState, and these documentation hunks. All `Content/**`, `.uproject`, generated files, and unrelated user WIP remain excluded; this source/config commit does not recreate the authored Hyper Armor fixture from a clean checkout.

---

## Previous Stage Record: TODO-02D2 - Timed Parry And Enemy Poise Counter v1

Baseline: `be3fdfd [Feature] 定向防御与玩家破防 (Directional Guard And Player Guard Break)`.

### Objective

Deliver the first timed Parry as an independent Q-triggered ability: `IA_Parry -> Input.Parry -> active Combat Loadout -> Ability.Defense.Parry`. A confirmed full-body, in-place, movement-locked Parry Montage pays an overdraft-allowed cost at startup, opens one authored NotifyState Parry window, and a resolver-validated front contact during that window is consumed and countered with authored enemy Poise damage through the existing C3C stance-break path. Guard, Guard Break, enemy Poise tuning, StateTree, and the shared melee delivery path remain D1/C3C behavior except for the narrow arbitration entries this slice owns. `TODO-02D3` Hyper Armor remains the next independent pending slice.

Locked values (user-accepted contract, revision 3):

- Input: physical Q maps to `IA_Parry`; routing is the existing `HeldCombatInput -> Event.Input.* -> CombatLoadoutDefinition` route with `Input.Parry`. Releasing or canceling Q only clears held input records; it never interrupts a started Parry. No second input state machine; no buffering of Q during non-cancelable attacks.
- `UParryAbility` derives from `UStaminaActionAbility` and explicitly sets `InstancingPolicy = InstancedPerActor` and `NetExecutionPolicy = ServerOnly` in its own constructor - these policies are never inherited implicitly. It owns `Ability.Defense.Parry` and `State.Action.Parrying` and uses one compatible full-body, in-place Parry Montage on `DefaultGroup.DefaultSlot`.
- Cost: `-15` Stamina through the authored Instant `GE_Parry_Cost` as the ability Cost, committed at startup through the new `CommitStaminaCostOnly(...)` helper with the inherited overdraft contract - any positive Stamina may start, the cost may consume the remainder down to zero. Full `15` is not required.
- Cooldown: `0.5s` authored Duration `GE_Parry_Cooldown` assigned as `CooldownGameplayEffectClass`; the GE grants `Cooldown.Parry` through its **Granted Tags** (Asset Tags do not feed `GetCooldownTags`/`CheckCooldown` and must not be the blocking source). `CheckCooldown` in `CanActivateAbility` is the single rejection path. The ability calls the engine-native `CommitAbilityCooldown(...)` exactly once, only from `EndAbility()` consuming the `bMontageCompletedNaturally` latch; failure, death, teardown, and interruption never set the latch and never commit the cooldown.
- Parry counter: the player's ASC applies the authored Instant `GE_Parry_PoiseDamage` to the attacker's ASC with `Data.Poise.Parry = -100` (SetByCaller). Enemy `MaxPoise` stays unchanged, so one Parry fully depletes the first Goblin and routes it through the existing C3C deferred Stance Break; native code never grants Stunned directly.
- Parry window: authored `UAnimNotifyState_ParryWindow` interval, initially `0.15s`, Editor-tunable on the Montage; native code stores no duration. The window's ASC-observable state is the loose `State.Action.ParryActive` tag; native queries use the ability instance's `IsParryActive()`.
- Movement: v1 Parry is movement- and jump-locked. Only after `Montage_IsActive()` confirms startup does it `StopMovementImmediately()` + `DisableMovement()` (the character enters `MOVE_None`). Its single `EndAbility()` restores `MOVE_Walking` only when `bMovementLockedByParry` is true and the current movement mode is still `MOVE_None`; it never overwrites `MOVE_Falling`. Camera control is untouched.
- Exclusivity: while `State.Action.Parrying` is active, Guard, Dodge, Jump, Primary, Light, Charged, and Sprint Attack cannot activate, and Sprint is canceled and cannot restart. Only an external transition into `MOVE_Falling` cancels Parry; the Parry's own `DisableMovement` (`MOVE_None`) is not airborne and must not cancel it or clear the Guard resume qualification.
- Guard interaction: Parry may activate while Guarding; only after its own Montage is confirmed active does it cancel the live Guard and the currently defense-cancelable attack. If that cancellation hit a live Guard while RMB stays held, exactly one Guard retry is attempted through the existing `ResumeGuardAfterAttack()` on the tick after the `State.Action.Parrying` tag is removed; `UParryAbility::EndAbility()` never requests Guard directly. A neutral Parry, or RMB newly pressed during Parry, creates no input buffer.
- Completion: both a successful Parry and a whiff play the full authored Montage, end naturally, and enter the cooldown; there is no early cut into a Recovery section. `0.15s` window and `0.5s` cooldown are this stage's confirmed, Editor-adjustable initial values.
- Stamina recovery: the inherited base contract applies the existing regeneration-delay effect exactly once when the committed Parry action ends; a successful Parry contact applies no additional regeneration-delay refresh.

### Mandated Semantics (required by ROADMAP before implementation)

- **Parry cooldown:** blocking is purely GAS-native - `CooldownGameplayEffectClass` + Granted `Cooldown.Parry` tag + `CheckCooldown`. Commit timing is decoupled from cost: startup commits only the Cost through `CommitStaminaCostOnly(...)`; only an identity-matched Montage-ended delegate callback reporting not-interrupted sets `bMontageCompletedNaturally`, and `EndAbility()` consumes that latch once to call the engine-native `CommitAbilityCooldown(...)`. `EndAbility`'s own `bWasCancelled` parameter alone never infers natural completion. Death, teardown, external cancellation, and startup failure never set the latch.
- **False start:** a whiffed Parry pays the full cost and, after its Montage completes naturally, the cooldown; no further penalty and no refund. The window simply expires on its Notify end event.
- **Montage failure:** a Parry whose Montage fails to start cancels nothing, locks nothing, opens no window, and never commits the cooldown. Its already-committed cost follows the standing project-wide committed-cost atomicity debt tracked for `TODO-02F`.
- **Enemy high Poise:** `-100` fully depletes the first Goblin (`MaxPoise 100`), so its next-tick C3C Stance Break is the expected route. A future higher-Poise enemy only takes partial depletion, finishes its swing, and recovers through the existing delayed Poise recovery; no immediate enemy-facing reaction is owned here.
- **Attack teardown:** resolution stays synchronous inside the resolver call. An enemy attack cancelled before contact closes its Trace Window and never reaches the Parry entry; the resolver already rejects dead sources, and a missing attacker ASC skips only the counter while the Parry still consumes the hit.
- **Player-state cleanup:** the window flag, its loose `State.Action.ParryActive` tag, Montage delegate, tasks, the movement lock, the natural-completion latch, and the Guard-resume qualification converge through one `EndAbility()`; the cooldown GE expires through its own Duration lifecycle. Dead, Stunned, and teardown reuse the existing cleanup semantics.

### Route And Ownership

```text
Outer: ue-stage-workflow
Primary: ue5-cpp-gameplay
Support: unreal-enhanced-input, ue5-blueprint-workflow, ue5-debug-validation
Route reason: The slice touches held input routing, the shared Stamina-action cost/commit contract, the native Cooldown mechanism, NotifyState event identity, resolver defense-dispatch order, Guard arbitration, and the enemy Poise lifecycle; every point is Main-only integration territory.
```

```text
Plan explorers: 0
Implementation executors: 0
Complex Executor: none
Main parallel work: none
Reason: The Parry window flag, cost/cooldown commit split, movement lock, Guard cancellation, and resume hand-off share one lifecycle boundary; concurrent writers would race the same arbitration state.
```

Main owns native source, tags, static checks, review, documentation, staging, and commit boundaries. The user owns all `Content/**` authoring, Editor readback, manual `PolyQuestEditor` compilation, Scene01 PIE validation, and commit approval. No live Unreal Editor MCP query or write is claimed for this slice.

### Approved Native Contract (revision 3)

#### Tags

Add exactly: `Input.Parry`, `Ability.Defense.Parry`, `State.Action.Parrying`, `State.Action.ParryActive`, `Event.Defense.Parry.Window.Begin`, `Event.Defense.Parry.Window.End`, `Data.Poise.Parry`, and `Cooldown.Parry` (Granted Tag of `GE_Parry_Cooldown`; the blocking source for `CheckCooldown`).

#### StaminaActionAbility Minimal Helper

- `UStaminaActionAbility` gains one minimal protected helper: `CommitStaminaCostOnly(...)`. It reuses the existing `CheckCost` and the engine's `CommitAbilityCost`, and on success writes the existing `bCostCommitted` flag so the base `EndAbility()` still applies the one-shot Stamina regen delay exactly once.
- The existing `CommitAbility` override stays byte-identical. No global flag, no broadened virtual, and no behavior change for `ULightAttackAbility`, `UDodgeAbility`, `UChargedAttackAbility`, `USprintAttackAbility`, or `UJumpAbility`.

#### Parry Window Notify

- New `UAnimNotifyState_ParryWindow` joins the existing combat action-window group (`Animation/Combat/AnimNotifyState_ActionWindows.*`). `NotifyBegin`/`NotifyEnd` send `Event.Defense.Parry.Window.Begin/End` with the source animation attached for identity filtering, exactly like the Dodge Cancel and Trace Window NotifyStates. It mutates nothing itself.

#### UParryAbility Lifecycle

- Startup validation follows the D1 pattern: ASC, grounded living player, AnimInstance, Montage, Cost/Cooldown/Counter effects, and all required tags. While attacking it activates only through the existing `State.Action.CanCancel.Defense` window, identical to D1 Guard.
- Order: create tasks -> commit the Cost only through `CommitStaminaCostOnly(...)` -> bind the Montage-ended delegate -> `ReadyForActivation` -> confirm `Montage_IsActive`. Only after confirmation: stop/disable CharacterMovement into `MOVE_None` (jump/movement lock), cancel Sprint, cancel the live Guard, and cancel the currently defense-cancelable attack through the existing tag-scoped `CancelAbilities` route. A failed startup cancels nothing, locks nothing, and commits no cooldown.
- Two persistent `UAbilityTask_WaitGameplayEvent` listeners receive the window events, accepted only when the payload identity-matches the active Parry Montage. Window-begin adds the loose `State.Action.ParryActive` tag and arms `bParryWindowOpen`; window-end and every `EndAbility()` path remove the tag and clear the flag.
- `IsParryActive()` (`bParryWindowOpen && !bEndAbilityRequested`) is the narrow native query for the defense dispatch entry. A window contact calls `TryParryMeleeHit(AttackingActor)`: valid active parry plus the D1 `120`-degree front-arc check, then apply `GE_Parry_PoiseDamage` to the attacker ASC through `MakeOutgoingSpec` + `SetSetByCallerMagnitude(Data.Poise.Parry, -ParryPoiseDamage)` + `ApplyGameplayEffectSpecToSelf` (skip only on a missing attacker ASC), and return true. It never touches Health, Guard Stamina, or the regeneration delay. Success does not cut the Montage; playback continues to natural completion.
- `OnActiveMontageEnded` sets `bMontageCompletedNaturally` only when the callback's Montage identity-matches the active Parry Montage and reports not-interrupted, then routes through the single `EndAbility()` path.
- `EndAbility()` is the single cleanup and the sole cooldown commit point: remove delegate/tasks, stop any still-active Montage, remove the `ParryActive` loose tag, restore `MOVE_Walking` only when `bMovementLockedByParry` is true and the current mode is still `MOVE_None` (never overwrite `MOVE_Falling`), consume `bMontageCompletedNaturally` once to call the engine-native `CommitAbilityCooldown(...)`, and clear the latch. It never requests Guard directly; Q release/cancel events are never listened to for ending.

#### Defense Dispatch, Exclusivity, And Guard Arbitration

- `APlayerCharacter` gains `ParryAction` binding (Started/Completed/Canceled on the existing combat-input route; release only clears held records) and `TryResolveIncomingDefense(AttackingActor, GuardStaminaDamage)`: it checks the active Parry first and falls back to the unchanged `TryGuardIncomingMeleeHit`. `FMeleeHitResolver` calls this one renamed entry; no second trace, damage, or resolver path is added.
- Exclusivity entries are one blocked-tag line per ability constructor: `UPlayerGuardAbility`, `UDodgeAbility`, `UJumpAbility`, `UPrimaryAttackAbility`, `ULightAttackAbility`, `UChargedAttackAbility`, and `USprintAttackAbility` each add `State.Action.Parrying` to `ActivationBlockedTags`. `APlayerCharacter::CanAttemptSprint` adds the same tag, and the sprint-relevant tag listener registers `State.Action.Parrying` with Guard-equivalent cancel/wait semantics (the Parry Ability itself owns the post-confirmation Sprint cancellation; the listener does not double-cancel).
- `OnMovementModeChanged` cancels an active Parry only when the new mode is `MOVE_Falling`. The Parry's own `DisableMovement` (`MOVE_None`) is not an airborne event, must not cancel the Parry, and must not clear the Guard resume qualification. An external Falling transition cancels the ability and clears that qualification under the existing D1 airborne rule.
- The Guard-resume retry reuses the D1 qualification flag and the existing `ResumeGuardAfterAttack()`, triggered solely by the removal of the `State.Action.Parrying` tag on the next tick. Dead/Stunned arrivals keep clearing the qualification through the existing listener branches.
- Q pressed during a non-cancelable attack records only held input state, exactly like D1 RMB; there is no Parry resume, no buffered Parry, and no automatic retry.

### User-Owned Editor Gate

1. Create Digital `IA_Parry`; assign it to `BP_Player.ParryAction`; map Q to it in `IMC_Default`.
2. Add `Input.Parry -> Ability.Defense.Parry` to the current player Combat Loadout route.
3. Create `GA_PlayerParry` from `UParryAbility` and add it to `BP_Player.StartupAbilities`.
4. Create one compatible full-body, in-place Parry Montage on `DefaultGroup.DefaultSlot` with one `UAnimNotifyState_ParryWindow` track of `0.15s` early in the Montage. No Guard Montage, AnimBP locomotion, enemy asset, StateTree, or map edit.
5. Create `GE_Parry_Cost` (Instant, Stamina Additive `-15`), `GE_Parry_Cooldown` (Duration `0.5s`, **Granted Tags** containing exactly `Cooldown.Parry`, no other grants), and `GE_Parry_PoiseDamage` (Instant, Poise SetByCaller `Data.Poise.Parry`); assign them on `GA_PlayerParry` together with the inherited Stamina regen-delay reference.
6. Do not change enemy `MaxPoise`, `DA_EnemyAttackProfile_GoblinAxe`, weapon collision, Physics Asset, imported resources, or Root Motion settings for D2.

### Validation Matrix

Main static gate: final source and direct caller/callee reads, CodeGraph, Gameplay Tag cross-check, C4458 scan, `git diff --check`; code-review-graph only as supplemental coverage if its index matches. Main does not run UBT, Editor writes, or PIE.

User compile and Scene01 PIE gate:

- Compile `PolyQuestEditor` after authoring the D2 asset references.
- Verify Q Parry starts only when grounded and legal (or through an eligible Defense Cancel Window during an attack), pays `-15` once at startup through `CommitStaminaCostOnly` - including the overdraft case where positive Stamina below `15` still starts and consumes the remainder - and never activates while Parrying, Stunned, Dodging, or Guard-Broken.
- Verify movement and jump lock only after the Montage starts (character enters `MOVE_None`), walking restoration on completion only while still in `MOVE_None`, no overwrite of an external `MOVE_Falling`, and untouched camera control.
- Verify one front-arc contact inside the `0.15s` window: no Health loss, no Guard Stamina loss, the attacker's Poise drops by `100`, and the first Goblin then routes through the existing C3C Stance Break on the next tick with no direct Stun grant.
- Verify the Stamina contract: the existing regeneration delay refreshes exactly once when the committed Parry action ends, and a successful Parry contact adds no extra refresh.
- Verify whiff and success both play the full Montage, end naturally, and commit `GE_Parry_Cooldown` exactly once; Q inside the `0.5s` cooldown is rejected by `CheckCooldown` while Guard/Dodge/Jump/attacks stay unaffected.
- Verify startup failure: a Parry whose Montage fails to start cancels no Guard, no attack, locks nothing, opens no window, and never commits the cooldown.
- Verify exclusivity: while Parrying, Q-adjacent inputs cannot start Guard, Dodge, Jump, Primary/Light/Charged/Sprint Attack, or Sprint; an external fall into `MOVE_Falling` cancels Parry while the Parry's own `MOVE_None` lock does not; releasing Q mid-Parry does not interrupt it.
- Verify arbitration: Parry during a Defense Cancel Window cancels that attack only after the Parry Montage starts; Q during a non-cancelable attack does nothing later; a Parry started while Guarding cancels that Guard, and a continuously held RMB restores Guard exactly once on the tick after the Parrying tag clears - including after an airborne Parry cancellation, where the failed `ResumeGuardAfterAttack()` attempt clears the eligibility by itself; a neutral Parry or RMB newly pressed during Parry never buffers Guard.
- Verify boundary: a contact just after window-end, or a rear/out-of-arc contact during the window, falls through to the unchanged D1 Guard deduction path.
- Verify teardown: death, stun, airborne, PIE stop, and repeated contacts within one Trace Window leave no stale `ParryActive` tag, window flag, movement lock, natural-completion latch, cooldown GE, or resume qualification.
- Re-run the D1 matrix (guard arc, rear damage, zero-Stamina Guard Break, attack cancel/resume, Sprint exclusivity) plus C3B/C3C, invulnerability, and death regressions.

### Documentation And Commit Boundary

After user compile/PIE and review: document only the durable Parry input/ability/cost-cooldown/counter, defense-dispatch, and exclusivity contract in `ARCHITECTURE.md`, mark only `TODO-02D2` done, preserve `TODO-02D3` as pending, and remove this draft's DRAFT marker with the accepted record.

Native candidate paths: `PlayerCharacter.h`, `PlayerCharacter.cpp`, `PlayerGuardAbility.cpp` (+ `.h` if the blocked-tag entry requires it), new `PlayerParryAbility.h`, new `PlayerParryAbility.cpp`, `StaminaActionAbility.h`, `StaminaActionAbility.cpp` (the `CommitStaminaCostOnly` helper only), `DodgeAbility.cpp`, `JumpAbility.cpp`, `PrimaryAttackAbility.cpp`, `LightAttackAbility.cpp`, `ChargedAttackAbility.cpp`, `SprintAttackAbility.cpp` (one blocked-tag line each), `MeleeHitResolver.h`, `MeleeHitResolver.cpp` (defense-entry rename), `Animation/Combat/AnimNotifyState_ActionWindows.h`, `Animation/Combat/AnimNotifyState_ActionWindows.cpp`, `Config/Tags/PolyQuestGameplayTags.ini`, and exact `ARCHITECTURE.md`/`ROADMAP.md`/`plan.md` hunks. Exclude every `Content/**` item, GA/GE, Montage, AnimBP, Blueprint, input asset, map, generated directory, and unrelated WIP. No commit occurs without explicit user approval.

### Implementation And First Review Record (2026-08-16)

#### Implementation Notes

- The accepted revision-3 contract was implemented on baseline `be3fdfd` across the exact candidate path list: the `CommitStaminaCostOnly(...)` helper (existing `CommitAbility` untouched), new `UPlayerParryAbility` (explicit `InstancedPerActor`/`ServerOnly`, cost-only startup commit, `bMontageCompletedNaturally` latch consumed once into the engine-native `CommitAbilityCooldown(..., false)`), the `TryResolveIncomingDefense` Parry-first dispatch entry in `APlayerCharacter` (resolver call-site rename only), one `State.Action.Parrying` blocked-tag line in each of Guard/Dodge/Jump/Primary/Light/Charged/Sprint Attack, `CanAttemptSprint` plus the sprint-relevant listener parity, external `MOVE_Falling` cancellation that clears Guard resume eligibility while Parry's own `MOVE_None` lock preserves it, the movement-lock restore guarded to this ability's own lock, and the `UAnimNotifyState_ParryWindow` NotifyState in the existing action-window group. Eight tags were added to the config (the seven contract tags plus `Cooldown.Parry`).
- Three implementation-time corrections were made before the user compile: the engine's `CommitAbilityCooldown` requires a mandatory `ForceCooldown` argument (verified against the UE 5.8 header, fixed to `false`); `OnParryWindowBegin` initially queried `IsParryActive()`, which could never be true before the window opened and would have permanently disabled the window (removed - the montage identity filter already rejects ended abilities); and the first compile failed because `UCharacterMovementComponent` exposes no public `GetMovementMode()` - both call sites now use engine-verified `IsFalling()`/`IsMovingOnGround()` combinations with identical semantics, and the movement-restore branch additionally gained the missing Dead-tag check to match the Guard Break pattern.

#### User Validation Evidence

- The user confirmed `PolyQuestEditor` compilation (after the movement-API repair) and that the authored Scene01 PIE route passed. Main did not run UBT, Editor, or PIE.

#### Main First Review (normal pass; codex owns the second pass)

Scope: the full native/config diff against `be3fdfd` plus direct callers/callees, read from the final on-disk state.

Result: one P1 and one P3; all other attack surfaces closed (cost/cooldown commit split, Granted-Tag cooldown blocking verified in engine source, window open/close pairing, seven-way exclusivity, movement lock/restore semantics, airborne cancellation, Q-release semantics, EndAbility reentrancy, and the D1/C3C regression surface).

- P1 (repaired): when a Parry activated through an attack's Defense Cancel Window, the Attacking-tag removal callback consumed the Guard resume qualification one tick early - `ResumeGuardAfterAttack` cleared it as a "failed retry" while `State.Action.Parrying` was still active, so Guard never resumed after the Parry, violating locked contract item 8 in that combined scenario. Repair: `ResumeGuardAfterAttack` now returns without consuming the qualification while `State.Action.Parrying` is present; the Parrying-tag removal re-triggers the retry. The D1-only path is unaffected because the tag cannot exist without a granted Parry ability.
- P3 (repaired): the Attacking and Parrying resume branches in `OnSprintRelevantTagChanged` were structurally identical and are now one merged condition.
- Remaining validation gap: the combined P1 scenario (held RMB + attack cancel window + Q + Parry completion) should get one focused PIE confirmation after this repair's recompile; `GA_PlayerParry` reference omissions remain a covered Editor-gate self-check item rather than a native defect.

#### Fresh Review Repair Record (2026-08-16)

- P1 repaired: `OnMovementModeChanged()` now distinguishes Parry's own `MOVE_None` lock from an external `MOVE_Falling`. A confirmed Parry preserves the held-RMB Guard recovery qualification through its own movement lock; an actual fall still cancels Parry and clears the Guard recovery path under the existing D1 airborne rule.
- P2 repaired: `UPlayerParryAbility::ValidateActivationSetup()` now fails closed when the inherited Cost or Cooldown GameplayEffect is missing, or Parry Poise damage is non-positive. `TryParryMeleeHit()` likewise rejects a non-positive value, and the authored CDO range now starts at `0.01` to match that runtime contract.

#### Closeout Record (2026-08-16)

- The user confirmed post-repair `PolyQuestEditor` compilation and the authored Scene01 PIE route. Main did not run UBT, Editor, or PIE.
- Main's repair delta review re-read the final Parry code and direct callers/callees; it found no P0-P2 issue. The local `code-review-graph` index remains at `be3fdfd`, so direct-source review is the coverage basis rather than graph zero-results. No fresh independent Reviewer result is claimed.
- The committed-cost playback atomicity risk, now including Parry's cost-before-Montage-confirmation path, is handed off to `TODO-02F` in `ROADMAP.md`; stable Parry ownership and D2 Done status are synchronized there and in `ARCHITECTURE.md`.
- The approved candidate set is ready for an explicit-path commit. All `Content/**`, GA/GE, Montage, AnimBP, Blueprint, input, map, imported resources, and other user WIP remain excluded; this source/config/documentation change is not a clean-checkout Parry fixture.

---

## Previous Stage Record: TODO-02D1 (superseded by the D2 record above; durable record lives in ROADMAP Done Milestones)

- D1 completed directional Guard and player Guard Break on baseline `c897342`; the user confirmed `PolyQuestEditor` compilation and Scene01 PIE.
- Main review found no P0-P2 defect; the user-run GLM second review repaired Trace-Request indentation noise and added an unaccepted-Guard-Break-event warning while preserving absorbed-hit behavior. Final static evidence: source/call-chain reads, CodeGraph, tag cross-check, C4458 scan, `git diff --check`.
- Committed as `be3fdfd` with the approved native/config path list recorded in git; all `Content/**` authoring WIP remains excluded and is not a clean-checkout fixture.
