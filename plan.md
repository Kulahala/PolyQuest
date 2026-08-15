# plan.md - AI Agent Collaboration File

## Rules

1. Read this file before continuing a PolyQuest stage.
2. Keep feedback, active plan, validation evidence, and unresolved blockers scoped to the current stage.
3. The implementation author cannot mark a stage as review-approved.
4. Retain the completed plan and closeout record until the next accepted stage deliberately replaces it.
5. Durable architecture belongs in `ARCHITECTURE.md`; accepted future direction and validation debt belong in `ROADMAP.md`.

---

## Active Stage: TODO-02D1 - Directional Guard And Player Guard Break v1

Baseline: `c897342 [Feature] 敌人韧性与姿态破坏 (Enemy Poise And Stance Break)`.

### Objective

Deliver the first player defensive slice only: held RMB routes through the active Combat Loadout to a grounded directional Guard, a valid enemy melee contact in the front 120-degree arc spends authored Guard Stamina instead of Health, and a zero-Stamina successful block starts one player Guard Break presentation. `TODO-02D2` Parry and `TODO-02D3` Enemy Hyper Armor remain blocked on this slice's user compile and Scene01 PIE evidence.

Locked values:

- Guard arc: 120 degrees total, using the player's current horizontal forward vector and the attacker's actor-center direction.
- Guard movement: 70 percent of normal `MoveSpeed` through a Duration GameplayEffect.
- Guard Stamina regeneration multiplier: 0.7 through a Duration GameplayEffect and the new ASC-owned `StaminaRegenRateMultiplier` attribute.
- First Goblin `GuardStaminaDamage`: 25, authored in its existing `UEnemyAttackProfile`.
- A zero-Stamina guard still absorbs the resolving hit once, then dispatches Guard Break.

### Route And Ownership

```text
Outer: ue-stage-workflow
Primary: ue5-cpp-gameplay
Support: unreal-enhanced-input, ue5-blueprint-workflow, ue5-debug-validation
Route reason: Player input intent, ASC attributes/tags, Ability teardown, enemy Profile data, and the shared melee resolver form one lifecycle-sensitive defensive contract.
```

```text
Plan explorers: 0
Implementation executors: 0
Complex Executor: none
Main parallel work: none
Reason: The shared ASC/AttributeSet, input lifecycle, Gameplay Tags, and resolver are Main-only integration territory. Parallel writers would create races across the same action state.
```

Main owns native source, tags, static checks, documentation, review, staging, and commit boundaries. The user owns all `Content/**` authoring, Editor readback, manual `PolyQuestEditor` compilation, Scene01 PIE/visual validation, and commit approval. No live Unreal Editor MCP query or write is claimed for this slice.

### Approved Native Contract

#### Input, Guard State, And Attack Arbitration

- Add `Input.Guard`, `Ability.Defense.Guard`, `Ability.Reaction.Player.GuardBreak`, `Event.Reaction.Player.GuardBreak`, `State.Action.Guarding`, `State.Action.CanCancel.Defense`, and `Data.Stamina.GuardDamage` to the project tag config.
- `APlayerCharacter` adds an authored `GuardAction`. Its Started/Completed/Canceled callbacks use the existing `HeldCombatInput -> Event.Input.* -> CombatLoadoutDefinition` route with `Input.Guard`; it does not gain a second input state machine. `AimAction` and `Input.Aim` remain for future Bow authoring, while the user removes RMB from its Mapping Context entry.
- `UPlayerGuardAbility` is `InstancedPerActor` and `ServerOnly`. It requires ground movement, held Guard input, positive Stamina, and no Dead, Exhausted, Stunned, Dodging, or current Guard. It may start while attacking only if the existing active action owns `State.Action.CanCancel.Defense`; it owns `State.Action.Guarding` but neither blocks movement nor forces combat-facing.
- The Guard Ability starts its input listeners and Montage first. It applies its move-speed and regeneration-multiplier Duration GameplayEffects, cancels Sprint, and cancels an eligible active attack only after `Montage_IsActive()` confirms the authored Guard Montage actually started. A failed Guard Montage cannot spend Stamina, cancel an attack, or block an incoming hit.
- Light, Charged, and Sprint Attack reuse the existing Dodge Cancel Notify timing. Their existing scoped helper owns both `State.Action.CanCancel.Dodge` and the new `State.Action.CanCancel.Defense` for the same active-Montage interval, then removes both in every teardown path. The serialized Notify class and authored assets retain their existing identity.
- Light, Charged, and Sprint Attack cancel a live Guard only after their own Montage confirms active. `APlayerCharacter` records a one-shot resume qualification only when it actually canceled Guard while RMB was already held; after all active attacking tags clear, it retries Guard on the next tick. Releasing/canceling RMB, Guard Break, death, stun, Dodge, air transition, teardown, or a failed retry clears that qualification. Pressing RMB during an attack outside an eligible cancel window never creates a global input buffer.
- Dodge also cancels Guard only after its own Montage confirms active, without setting attack-resume qualification. Leaving the ground cancels Guard. This is a narrow exclusivity safeguard so an active ground Guard cannot survive into a Dodge or airborne state.
- Sprint cannot activate while `State.Action.Guarding` is active. Guard activation and every blocking player state use the existing ASC tag listener to cancel an already active Sprint; normal Sprint recovery behavior otherwise remains unchanged.

#### Guard Resolution And Guard Break

- `UCharacterAttributeSet` gains ASC-owned `StaminaRegenRateMultiplier`, default `1.0`, clamped nonnegative in final/base/GameplayEffect mutation paths. The existing periodic Stamina Regen GameplayEffect remains user-authored and must read this multiplier; native code does not introduce a second regeneration timer.
- `UEnemyAttackProfile` gains only `GuardStaminaDamage`, default `25`, nonnegative, and included in native Profile validity. `UEnemyMeleeAbility` snapshots it with Montage/GE/Cooldown and passes it through the existing Trace Window `FMeleeHitRequest`; player attacks retain the zero default.
- After existing self/team/Dead/Invulnerable validation and before constructing the original damage spec, `FMeleeHitResolver` calls the Player's narrow Guard entry. The active Guard Ability accepts only a valid active guard, a grounded player, a front-arc attacker, and valid authored Guard effects.
- A successful Guard creates the authored Instant `GE_PlayerGuard_StaminaCost`, writes negative `Data.Stamina.GuardDamage`, applies it to the player ASC, and refreshes the existing authored Stamina Regen Delay. The resolver reports that target as successfully resolved, so the Trace Window records it and cannot deduct Stamina repeatedly during the same swing. It does not create or apply the original damage GameplayEffect.
- If the successful deduction reaches zero, Guard ends and emits `Event.Reaction.Player.GuardBreak`. `UPlayerGuardBreakAbility` is `InstancedPerActor`, `ServerOnly`, gameplay-event-triggered, owns the existing `State.Status.Stunned`, and has no Cost or second Stamina path. Only after its Guard Break Montage is active does it stop/disable movement, clear Guard resume eligibility, require a physical RMB release before later Guard, and explicitly cancel Guard, Sprint, and attack abilities.
- Guard Break cleanup has one `EndAbility()` path: stop Montage/tasks/delegate, restore `MOVE_Walking` only for a live non-destroying player it locked, and never refill Stamina. Existing regen delay/recovery remains the sole Stamina recovery contract.

### User-Owned Editor Gate

1. Create Digital `IA_Guard`; assign it to `BP_Player.GuardAction`; map RMB to it in `IMC_Default`; remove the RMB mapping from `IA_Aim`. Retain `IA_Aim`, `Input.Aim`, and all unrelated input/Content WIP.
2. Add `Input.Guard -> Ability.Defense.Guard` to the current player Combat Loadout route.
3. Create `GA_PlayerGuard` from `UPlayerGuardAbility` and `GA_PlayerGuardBreak` from `UPlayerGuardBreakAbility`; add both to `BP_Player.StartupAbilities`.
4. Create an in-place UpperBody Guard Montage and a compatible full-body, in-place Guard Break Montage. Add `DefaultGroup.UpperBody` and blend the Guard Slot over the final `MainStates` locomotion pose with `Layered Blend Per Bone` from the first torso bone; keep the existing full-body `DefaultGroup.DefaultSlot` after that layer so attack, Dodge, and Guard Break Montages take visual priority. Guard presentation must remain active while RMB is held and must have a reliable release/cancel exit; neither Montage owns Health, Stamina, tags, movement, or collision logic.
5. Create the authored Guard move-speed, Guard regeneration-multiplier, and Guard Stamina-cost GameplayEffects. The two first effects are Duration effects with 0.7 values; the cost is Instant and reads `Data.Stamina.GuardDamage`. Update the already configured periodic Stamina Regen GameplayEffect to multiply its normal recovery by `StaminaRegenRateMultiplier`; retain its existing RegenBlocked/Delay tag semantics.
6. Add `GuardStaminaDamage = 25` to `DA_EnemyAttackProfile_GoblinAxe`. Confirm the existing enemy melee Montage, trace window, weapon markers, collision response, Startup Ability, StateTree, and current Profile references remain intact.
7. Do not edit StateTree topology, enemy AnimBP, player AnimBP locomotion, weapon collision, Physics Asset, map, imported resources, or Root Motion settings for D1.

### Validation Matrix

Main static gate: final source and direct caller/callee reads, CodeGraph, Gameplay Tag cross-check, C4458 inherited-member-shadow scan, `git diff --check`, and code-review-graph only as supplemental impact coverage if its index matches or is explicitly marked stale. Main does not run UBT, Editor writes, or PIE.

User compile and Scene01 PIE gate:

- Compile `PolyQuestEditor` after authoring the D1 asset references.
- Verify RMB Guard begins only when grounded and legal; ordinary movement remains camera-relative and faces movement at 70 percent speed; Shift cannot start or retain Sprint while Guarding.
- Verify a front enemy melee contact within the 120-degree arc consumes exactly one authored Stamina amount per Trace Window, does not reduce Health, and refreshes the existing Stamina recovery delay. Verify the rear/out-of-arc contact follows the original Health-damage route.
- Verify guard regeneration operates at 70 percent of the normal configured periodic rate while holding Guard, then restores the normal rate when Guard ends; no extra native timer or lingering effect survives release, cancel, Montage failure, death, PIE stop, or teardown.
- Verify a successful zero-Stamina block absorbs that one hit, starts Guard Break, removes Guard/Sprint/attack states, freezes movement only after the Guard Break Montage starts, restores walking at completion, leaves Stamina to normal delayed recovery, and requires RMB release before Guard may restart.
- Verify Light/Charged/Sprint Attack only lower Guard after their own Montages start; a pre-held RMB resumes Guard one frame after the attack ends, while RMB first pressed during a non-cancelable attack does not buffer. Verify an eligible current Cancel Window accepts immediate Guard and Guard only cancels that attack after its own Montage starts.
- Verify Dodge and airborne transitions cannot leave Guard active. Re-run enemy attack/trace, C3B/C3C interruption and death, invulnerability, same-team rejection, Sprint/Dodge/Light/Charged/Sprint Attack, fixed camera/facing, and ragdoll regressions.

### Documentation And Commit Boundary

Do not update `ARCHITECTURE.md` or mark D1 complete before the user compile/PIE gate and strict review. After they pass, document only the durable Guard/Resolver/Guard Break contract, mark only `TODO-02D1` done, preserve D2/D3 as pending, and retain this plan plus its validation/review record until the next accepted slice replaces it.

Native candidate paths: `PlayerCharacter.*`, `CharacterAttributeSet.*`, new `PlayerGuardAbility.*`, new `PlayerGuardBreakAbility.*`, `DodgeAbility.*`, `LightAttackAbility.*`, `ChargedAttackAbility.*`, `SprintAttackAbility.*`, `EnemyAttackProfile.*`, `EnemyMeleeAbility.*`, `AbilityTask_MeleeTraceWindow.*`, `MeleeHitResolver.*`, `Config/Tags/PolyQuestGameplayTags.ini`, and exact `ROADMAP.md`/`plan.md` hunks. Exclude every `Content/**` item, Blueprint, GA/GE, Montage, input asset, map, generated directory, and unrelated documentation WIP. No commit occurs without explicit user approval.

### Review And Closeout Record (2026-08-16)

- The user confirmed that `PolyQuestEditor` compiled and the D1 Scene01 PIE route passed. This is user-provided build and runtime evidence; Main did not run UBT, Visual Studio, Unreal Editor, or PIE.
- Main's first normal code review found no P0-P2 defect in the approved D1 native surface. The user then obtained a second GLM review: it removed an accidental extra indentation level from the `FMeleeHitRequest` population block and added a Warning when a zero-Stamina Guard Break event has no accepting Ability. Both repairs preserve the intended resolving-hit behavior.
- Final static evidence is the focused source/call-chain read, CodeGraph, Gameplay Tag cross-check, a C4458 inherited-member-shadow scan, and `git diff --check`. The available local memory registry had no matching persistent C4458 record; no memory MCP tool was injected for a live query or write.
- The accepted presentation/authoring fixture remains outside this commit: all `Content/**`, GA/GE, Montage, AnimBP, Blueprint, Input Action/Mapping Context, map, imported resource, and generated-directory changes are intentionally excluded. This source/config/documentation commit does not reproduce a playable Guard fixture from a clean checkout.

### Approved Commit Boundary

```text
Config/Tags/PolyQuestGameplayTags.ini
Source/PolyQuest/Public/Character/Player/PlayerCharacter.h
Source/PolyQuest/Private/Character/Player/PlayerCharacter.cpp
Source/PolyQuest/Public/AbilitySystem/CharacterAttributeSet.h
Source/PolyQuest/Private/AbilitySystem/CharacterAttributeSet.cpp
Source/PolyQuest/Public/AbilitySystem/Abilities/PlayerGuardAbility.h
Source/PolyQuest/Private/AbilitySystem/Abilities/PlayerGuardAbility.cpp
Source/PolyQuest/Public/AbilitySystem/Abilities/PlayerGuardBreakAbility.h
Source/PolyQuest/Private/AbilitySystem/Abilities/PlayerGuardBreakAbility.cpp
Source/PolyQuest/Private/AbilitySystem/Abilities/DodgeAbility.cpp
Source/PolyQuest/Public/AbilitySystem/Abilities/LightAttackAbility.h
Source/PolyQuest/Private/AbilitySystem/Abilities/LightAttackAbility.cpp
Source/PolyQuest/Public/AbilitySystem/Abilities/ChargedAttackAbility.h
Source/PolyQuest/Private/AbilitySystem/Abilities/ChargedAttackAbility.cpp
Source/PolyQuest/Public/AbilitySystem/Abilities/SprintAttackAbility.h
Source/PolyQuest/Private/AbilitySystem/Abilities/SprintAttackAbility.cpp
Source/PolyQuest/Public/Combat/Enemy/EnemyAttackProfile.h
Source/PolyQuest/Private/Combat/Enemy/EnemyAttackProfile.cpp
Source/PolyQuest/Public/AbilitySystem/Abilities/EnemyMeleeAbility.h
Source/PolyQuest/Private/AbilitySystem/Abilities/EnemyMeleeAbility.cpp
Source/PolyQuest/Public/AbilitySystem/Tasks/AbilityTask_MeleeTraceWindow.h
Source/PolyQuest/Private/AbilitySystem/Tasks/AbilityTask_MeleeTraceWindow.cpp
Source/PolyQuest/Public/Combat/Melee/MeleeHitResolver.h
Source/PolyQuest/Private/Combat/Melee/MeleeHitResolver.cpp
ARCHITECTURE.md
ROADMAP.md (D1-related precise hunks only)
plan.md
```
