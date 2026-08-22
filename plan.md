# TODO-02C3F: Big Hit Interruption And Grounded Root Motion Knockback v1

## Plan State

- Status: completed and validated on 2026-08-23.
- Baseline: `7365248` (`[Feature] 受击层级分类与轻受击反应 (Hit Reaction Tiers And Small Reactions)`).
- Objective: complete the Player and Enemy `Data.Reaction.Big` route with full action interruption, one Montage-owned grounded Root Motion knockback, collision/ledge safety, and deterministic Player Big PIE coverage without implementing Launch or eight-direction presentation.

~~~text
Outer: ue-stage-workflow
Primary: ue5-cpp-gameplay
Support: ue5-blueprint-workflow, ue5-debug-validation
Route reason: C3F extends target-side GAS Big dispatch, native GameplayAbility cleanup, player input blocking, Root Motion ownership, and user-authored enemy attack fixtures.
~~~

~~~text
Plan explorers: 0
Implementation executors: 1 (Gemini, user-coordinated bounded executor)
Complex Executor: none
Main parallel work: inspect the implementation/self-review, perform fresh and adversarial review, close documentation, stage, and commit.
Reason: Gameplay Tags, Health dispatch, Ability lifecycle, player input gating, and CharacterMovement Root Motion are one coupled contract. No parallel writer may split them.
~~~

## Locked Product Contract

1. `Data.Reaction.Big` is a non-lethal, grounded, full-body interruption for both Player and Enemy. It starts only when the target is alive, not Stunned, not Hyper Armor, not already Big-reacting, and `CharacterMovement` is moving on ground. Damage still applies when Big is rejected.
2. Big starts its Montage first. Native GAS applies its `ActivationOwnedTags` and `BlockAbilitiesWithTag` for the Big ability lifetime as part of activation; a startup failure must immediately remove those normal GAS-owned tags through `EndAbility()`. The irreversible mutations -- `CancelAbilities`, current-velocity stop, ledge-rule override, controller/path stop, and MovementMode delegate binding -- may occur only after `Montage_IsActive()` confirms the authored montage has started. Montage/task startup failure cancels no pre-existing action and changes no CharacterMovement setting.
3. The Big Montage is the sole displacement owner. It contains planar, local-space backward Root Motion only: no `AddImpulse`, `LaunchCharacter`, `FRootMotionSource`, Motion Warping, `DisableMovement()`, vertical translation, or root rotation. C3F deliberately accepts that side/back hits still move relative to the target's current facing.
4. After confirmed montage start, Big snapshots `UCharacterMovementComponent::bCanWalkOffLedges`, temporarily sets it to `false`, and binds the CharacterMovement `MovementModeChangedDelegate`. CharacterMovement and Capsule collision own wall/ledge truncation; no corrective teleport or secondary force is allowed. If the new mode is `MOVE_Falling`, the delegate enters the same guarded `EndAbility()` teardown path. Teardown removes the exact delegate before restoring the saved ledge value, and restores nothing that the ability did not first override. Launch owns all intentional airborne motion later.
5. A successful Guard/Parry or Dodge invulnerability consumes the contact before Health damage and never reaches Big. Once a Player Health Big is accepted, it force-cancels Light, Charged, Sprint Attack, melee Skill, Primary arbitration/Bow, Dodge, Sprint, Jump, Guard, Parry, and Player Small. `Ability.Attack.Primary` covers both `UPrimaryAttackAbility` and `UBowDrawFireAbility` because both own that exact tag.
6. `State.Action.HitReacting` becomes the shared active-Big state on each target's own ASC. Stance Break and Guard Break remain higher-priority Stunned routes and cancel their target's Big. A new Big while Big is already active is rejected; it does not restart, refresh, or stack.
7. On successful Big activation, a pure helper snapshots a target-local planar direction that always means `target -> attacker`: use the finite, non-zero Effect Context `FHitResult::ImpactNormal` first, then fall back to finite, non-zero `InstigatorLocation - TargetLocation`. It projects to XY, normalizes, converts through the target actor rotation, and returns `FVector::ZeroVector` for every invalid/missing/zero input. C3F does not use this snapshot to align Root Motion or select animations; an invalid snapshot must never suppress the fixed-local Big montage. It is the single future source for eight-direction Small/Big presentation.

## Native Implementation

### Tags, dispatch, and impact snapshot

- Add `Ability.Reaction.Player.Big` and `Event.Reaction.Player.Big` to `Config/Tags/PolyQuestGameplayTags.ini`. After the confirmed Unarmed Charged asset migration and zero-reference scan, retire `Data.Reaction.Interrupt`, `Ability.Reaction.Enemy.Hit`, and `Event.Reaction.Enemy.Hit`; do not change `Data.Reaction.Small` / `Data.Reaction.Launch` behavior.
- Add `FHitReactionImpactResolver` under `Combat/Reaction/`. It is a non-reflected static helper that resolves target-local planar attacker direction from `FGameplayEventData`: first valid Effect Context hit normal, then valid Instigator-to-Target planar fallback, otherwise invalid/zero. It owns no Actor, ASC, state, logging, or asset lookup.
- Extend `APlayerCharacter::OnHealthAttributeChanged` so `EHitReactionTier::Big` dispatches `Event.Reaction.Player.Big`; retain C3E's health/dead/stunned/invalid filters and all Launch/None behavior. Enemy Big dispatch remains unchanged.

### Player Big ability

- Add `UPlayerBigHitReactionAbility` as an `InstancedPerActor`, `ServerOnly` native GA with one authored `BigHitReactionMontage` property and the existing one-delegate / one-`EndAbility()` teardown style.
- It owns `State.Action.HitReacting`, `State.Input.Block.Movement`, and `State.Input.Block.Jump`; it blocks activation while Dead, Stunned, Hyper Armor, already Big-reacting, or airborne.
- Its `BlockAbilitiesWithTag` and `AbilitiesToCancel` contain exactly the same action set: `Ability.Attack.Primary`, `Ability.Attack.Light`, `Ability.Attack.Charged`, `Ability.Attack.Sprint`, `Ability.Skill.Melee`, `Ability.Dodge`, `Ability.Movement.Sprint`, `Ability.Movement.Jump`, `Ability.Defense.Guard`, `Ability.Defense.Parry`, and `Ability.Reaction.Player.Small`. The former blocks new activation for the Big ability lifetime; the latter is invoked only after confirmed Montage start. Do not edit every existing Player action GA.
- After confirmed Montage playback, cache the optional direction snapshot, stop current velocity, snapshot/disable ledge walk-off, bind the MovementMode delegate, and let Montage Root Motion drive the capsule. Every completion, interruption, `MOVE_Falling` transition, cancellation, death/teardown, and invalid runtime path removes delegates/tasks, stops only this Montage, restores the prior ledge setting only when owned, and drops owned tags. The delegate must be identity-safe and cannot re-enter cleanup after teardown begins.

### Enemy Big migration

- Keep `UEnemyHitReactionAbility` and `GA_EnemyHitReaction` names for serialized asset compatibility. Replace its in-place `DisableMovement()` path with the same grounded Root Motion ownership: only after confirmed Montage start, call `AAIController::StopMovement()` when available and `StopMovementImmediately()`, preserve `MOVE_Walking`, snapshot/disable ledge walk-off, bind the MovementMode delegate, cache the optional direction snapshot, and restore only state owned by this ability. Remove `bMovementLockedByReaction`, `DisableMovement()`, and the old end-path `SetMovementMode(MOVE_Walking)`; StateTree resumes its normal decisions after `State.Action.HitReacting` is removed.
- Continue cancelling Enemy Melee only after Montage start; additionally cancel Enemy Small. Existing StateTree/AI `State.Action.HitReacting` waiting remains the only AI recovery route, so no new StateTree state or controller framework is added.
- `UPlayerGuardBreakAbility` adds a named `PlayerBigHitReactionAbilityTag` to its cancellation container. Its preflight must require that tag to be valid and the container to contain exactly nine unique entries (the current eight plus Player Big); update the CDO test accordingly. `UEnemyStanceBreakAbility` already cancels Enemy Big and remains otherwise unchanged.

## User-Owned Authoring Gate

1. Confirm the shared Player/Goblin Skeleton route. Reuse the existing Enemy Big montage only if it has valid planar backward Root Motion; otherwise author one compatible shared Big montage. It must have Root Motion enabled, no Z translation, no root rotation, and use the established full-body reaction Slot.
2. Set both AnimBPs to `Root Motion from Montages Only` for this route. Do not alter the C3E Small `ReactionOverlay` layer.
3. Create `GA_PlayerBigHitReaction` from `UPlayerBigHitReactionAbility`, assign the Big Montage, and add it to `BP_Player` StartupAbilities. Keep the Enemy Big GA on the same compatible montage.
4. Copy the ordinary enemy damage GE into a separate Big GE with identical numeric damage/guard values but exactly `Data.Reaction.Big`, not `Data.Reaction.Small`.
5. Create one dedicated enemy Big AttackProfile and a deterministic Scene01 test AttackSet containing that profile; assign it only to a focused test Goblin so the ordinary enemy Small route remains available for regression.

## Approved Native Surface

- `Config/Tags/PolyQuestGameplayTags.ini`: `Ability.Reaction.Player.Big` and `Event.Reaction.Player.Big` additions plus the zero-reference retirement of `Data.Reaction.Interrupt`, `Ability.Reaction.Enemy.Hit`, and `Event.Reaction.Enemy.Hit`.
- New `Source/PolyQuest/Public|Private/Combat/Reaction/HitReactionImpactResolver.*` and `Source/PolyQuest/Public|Private/AbilitySystem/Abilities/PlayerBigHitReactionAbility.*`.
- Existing `Source/PolyQuest/Public|Private/AbilitySystem/Abilities/EnemyHitReactionAbility.*`, `PlayerGuardBreakAbility.*`, `Source/PolyQuest/Public|Private/Character/Player/PlayerCharacter.*`, and `Source/PolyQuest/Private/Tests/HitReactionAutomationTests.cpp`.
- No `APlayerCharacter::OnMovementModeChanged` special case, StateTree asset/code change, CharacterMovement subclass, Build.cs change, or asset-file edit is in the native executor scope.

## Automation And Validation

### Native automation

- Extend `PolyQuest.Combat.HitReaction` to prove Player Big event dispatch, retained Enemy Big dispatch, Player/Enemy Big CDO tags, Player cancellation/blocking containers, Guard Break's Player Big entry and exact nine-entry preflight contract, Stance Break precedence, and no regression in Small/Launch/Invalid behavior.
- Add pure `FHitReactionImpactResolver` coverage for `ImpactNormal` priority, target-to-attacker direction consistency with the Instigator fallback, non-finite/zero failure, and target-local conversion. The helper's invalid result must not suppress C3F's fixed-local Root Motion Big.
- Keep the existing C3E dead, stunned, Poise-broken, Hyper Armor, and lethal no-event cases; add grounded-vs-airborne Big acceptance coverage where a controlled native fixture can prove it without authored assets.
- Run `PolyQuest.Combat.HitReaction` plus the existing Player ActionWindows, Enemy AttackSetSelection, Enemy CombatSpacing, Equipment TransactionMatrix, Melee TraceSourceGeometry, Projectile Lifecycle, and Projectile TargetAssist suites. Expected negative-fixture warnings remain warnings.

### User compile and PIE gate

1. Manually compile `PolyQuestEditor`.
2. In Scene01, verify the dedicated Enemy Big test Goblin sends Player Big: current combat action ends, movement/jump/input actions stay blocked during the full-body Root Motion montage, collision/ledge stops prevent falling, and all input returns after completion. Force a safe loss of ground while Big is active: Big must end once, remove all tags/input locks, and restore the pre-existing ledge setting without changing the movement mode manually.
3. Verify Player Charged Big against Enemy: Enemy Melee cancels after Big visibly starts, AI issues no new attack/reposition while `State.Action.HitReacting` exists, and resumes normally after cleanup.
4. Verify normal Enemy Small and Player Light Small still overlay without interruption; Guard/Parry/Dodge invulnerability still prevent Health Big; Stance Break/Guard Break preempt Big; repeated Big does not stack.
5. Verify a Big hit while Player is Falling only reduces Health and creates no Montage, Big tag, displacement, or stale input lock. Record the accepted temporary side/back visual limitation: local backward Root Motion is not impact-aligned until a later directional presentation slice.

## Explicit Non-Goals And Closeout

- No eight-direction selection, direction DataAsset, Small direction work, Launch, airborne trajectory, landing, generic reaction base, weapon-specific reaction tables, Motion Warping, root-motion source, damage balancing, or new AI framework.
- No change to existing player death/terminal-action debt, Guard/Parry resolution, Dodge invulnerability semantics, Poise formula, projectile delivery, or ordinary enemy attack-selection rules.
- After user-confirmed compile, automation, PIE, Gemini self-review, Main fresh review, and Main adversarial fallback while Luna remains unavailable, update `ARCHITECTURE.md`, `README.md`, `ROADMAP.md`, and this plan. `ROADMAP.md` must record the accepted non-directional Big limitation and its closure trigger: eight compatible directional assets plus focused direction visual validation.
- The eventual commit includes only approved C++/tag/test/document changes. It excludes all `Content/**`, `.uasset`, `.umap`, Blueprints, Montages, AnimBPs, DataAssets, Scene01, input assets, `.zcode/**`, `PolyQuest.uproject`, and unrelated user WIP.

## Execution And Review Gates

1. Gemini reads this accepted plan and the live source, then implements only the Approved Native Surface. It may add the two exact tags, but must not change other tag taxonomy, assets, StateTree, input routing, Build.cs, documentation, staging, or commits.
2. The user owns the Big Root Motion Montage, AnimBP root-motion mode, Player Big GA/StartupAbility, Big Damage GE, dedicated Enemy Big AttackProfile/AttackSet, Scene01 fixture, manual `PolyQuestEditor` compile, Automation run, and PIE/visual evidence. No executor manually edits `.uasset` or `.umap`, imports/retargets assets, launches Editor, runs UBT, or claims those results.
3. Before handoff, Gemini runs scoped source/static checks permitted by its environment, performs a two-pass implementation self-review (normal plus adversarial), and reports changed paths, exact static evidence, unverified gates, known limitations, and any required user authoring action. Its self-review is not an independent fresh review.
4. Main performs a fresh defect-first review after the user reports compile/Automation/PIE results. With `gpt-5.6-luna` unavailable, the second required pass is explicitly a Main adversarial fallback. Documentation closeout and a scoped commit require the user to request them after those gates pass.

## Closeout Record

- **Native implementation:** Added `UPlayerBigHitReactionAbility` and `FHitReactionImpactResolver`; Player Health Big now dispatches `Event.Reaction.Player.Big`. `UEnemyHitReactionAbility` now shares grounded Root Motion ownership rather than disabling movement, and both Big abilities bind Falling cleanup only after a valid Montage starts.
- **Priority and cleanup:** Player Big blocks/cancels the approved combat matrix, including `Ability.Attack.Primary` for Bow and Primary arbitration. Guard Break now cancels Player Big through its validated nine-tag container; Stance Break remains the higher-priority Enemy route. Enemy AI continues to wait on `State.Action.HitReacting`.
- **Legacy Tag closure:** The user migrated `GE_Unarmed_ChargedAttack_Unarmed_Damage` from `Data.Reaction.Interrupt` to `Data.Reaction.Big`. Rider offline asset scans confirmed zero remaining legacy Interrupt/Enemy.Hit references before the three old config tags were removed. Automation now asserts conflicting ImpactNormal/fallback priority plus `NaN` and `Inf` fail-closed behavior.
- **User-confirmed validation:** `PolyQuest.Combat.HitReaction` Automation completed with `Success`; its null Loadout, invalid AttackSet/Poise, invalid multi-tier, and missing ragdoll fixture messages are expected negative-path logs. The user confirmed Scene01 PIE for Player/Enemy Big behavior and the migrated Unarmed Charged Big route.
- **Review:** Main fresh review found the stale Unarmed Charged `Interrupt` P2 and resolver-test P3 gap; both were remediated. `gpt-5.6-luna / xhigh` was unavailable, so the second review pass was a Main adversarial fallback. `code-review-graph` transport was unavailable; direct source, CodeGraph, Rider lint, asset tag queries, and diff checks supplied the static evidence.
- **Debt handoff:** Big currently uses authored local-backward Root Motion independent of hit angle. `ROADMAP.md` records `TODO-07B` as the owner: close only after eight compatible directional Small/Big assets, an accepted bounded selector, and focused Player/Enemy visual validation.
- **Commit boundary:** Include only approved native source, `Config/Tags/PolyQuestGameplayTags.ini`, focused automation, and synchronized documentation. Exclude all `Content/**`, `.uasset`, `.umap`, Blueprints, Montages, AnimBPs, DataAssets, Scene01, input assets, `.zcode/**`, `PolyQuest.uproject`, `AGENTS.md`, and unrelated user WIP.
