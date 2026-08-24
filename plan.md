# TODO-01I: Player Exhaustion Recovery And Walk-Pace v1

## Plan State

- Status: Complete.
- Baseline: `51a9b16` (`[Docs] 完成持盾防御表现文档收尾`).
- Objective: replace AttributeSet's immediate Stamina-to-Exhausted toggle with one Player-owned exhaustion lifecycle. Stamina continues to regenerate normally, but the Player remains action-locked until both the original three-second buffer has elapsed and Stamina is positive.
- Preserve all user-owned WIP. This stage does not write or stage `Content/**`, Config, maps, Blueprints, Input, AnimBPs, GA/GE/Montage assets, project files, or generated output.

```text
Outer: ue-stage-workflow
Primary: ue5-cpp-gameplay
Support: ue5-debug-validation
Route reason: Player ASC attribute delegates, Gameplay Tags, persistent GameplayEffect handles, movement speed, and Ability activation gates change together.
```

```text
Plan explorers: 0
Implementation executors: 0
Complex Executor: none
Main parallel work: none
Reason: AttributeSet, Player lifecycle, shared tags, and Ability activation contracts are Main-only integration territory.
```

## Approved Runtime Contract

1. `APlayerCharacter` owns an authority-only exhaustion lifecycle. The first Stamina value at or below zero writes the Player's loose `State.Status.Exhausted`, applies one independent MoveSpeed GameplayEffect handle, and starts one fixed `3.0s` timer.
2. Exhaustion clears only when that original timer has elapsed and the current Stamina is strictly positive. A positive value before expiry remains action-locked; a timer expiry at zero remains action-locked until the next positive value.
3. An additional zero transition while Exhaustion is active never resets the timer. A later zero after a successful clear begins a new cycle.
4. `ExhaustionMoveSpeedGameplayEffectClass` is a Player-authored reference to the existing `GE_Guard_MoveSpeed`: Infinite, non-periodic, `MoveSpeed` multiplicative `0.7`. Exhaustion owns its own handle and never writes `CharacterMovement.MaxWalkSpeed` directly.
5. A committed Attack, Dodge, Bow, or Parry keeps its existing Montage/Root Motion/task cleanup. Sprint retains its own zero-Stamina stop and release-to-restart gate; Guard retains its Guard Break route. Guard Break does not clear the intended Exhaustion lifecycle.
6. Death-tag receipt, EndPlay, ASC rebind, and failed/restarted state clean the timer, exact loose tag contribution, and correct ASC-owned GameplayEffect handle through one idempotent path.
7. `UCharacterAttributeSet` continues to clamp Stamina but no longer writes `State.Status.Exhausted` for every shared AttributeSet holder. `UPlayerParryAbility` gains the Exhausted activation blocker; `UJumpAbility` loses it. Guard Break and Player reactions remain available.
8. Jump is a zero-cost movement exception, not a generic Stamina action: it retains its authored Cost GE and Regen Delay GE references for the existing ability setup contract, but bypasses the shared positive-Stamina gate and does not apply the Regen Delay GE on end. It therefore neither pauses ordinary Stamina recovery nor clears the Player-owned Exhaustion timer, loose tag, or slow before their existing conditions are met.

## Approved Source And Test Surface

- `Source/PolyQuest/Private/AbilitySystem/CharacterAttributeSet.cpp`: remove the generic exhaustion loose-tag side effect.
- `Source/PolyQuest/Public/Character/Player/PlayerCharacter.h` and `Private/Character/Player/PlayerCharacter.cpp`: add the private delegate, Timer, exact GE-handle cleanup, state helpers, editor-configurable GE class, and narrow automation fixture hooks.
- `Source/PolyQuest/Private/AbilitySystem/Abilities/JumpAbility.cpp`, `StaminaActionAbility.cpp`, and `PlayerParryAbility.cpp`: correct the two changed activation gates and make zero-cost Jump opt out of the shared Regen Delay application without changing other Stamina actions.
- `Source/PolyQuest/Private/Tests/`: add `UTestExhaustionMoveSpeedGE`, `UTestJumpExhaustionAbility` and its temporary GE inputs, extend `FCombatAutomationFixture` with the native authored reference, and add `PolyQuest.Player.Exhaustion` coverage without Content assets.
- Regression repair discovered by the required matrix: `AEnemyCharacter` must not retain a callback-local `FGameplayEffectSpec*` across GameplayEffect applications. Replace it with a Definition plus EffectContext snapshot, require that the current effect has executed its own Poise modifier, and extend `HitReactionAutomationTests.cpp` for different-transaction rejection with both fresh and reused Context paths.

## User-Owned Editor Work

1. In `BP_Player`, assign the new `ExhaustionMoveSpeedGameplayEffectClass` to the existing `GE_Guard_MoveSpeed`; read back Infinite duration, no Periodic execution, and `MoveSpeed x 0.7`.
2. In the existing Jump Cost GE, set the Stamina modifier magnitude to `0`, but retain both the Cost GE and Stamina Regen Delay GE references on the Jump ability.
3. Compile `PolyQuestEditor` manually, run the Automation matrix from the Editor front-end, then validate focused Scene01 PIE behavior.

## Validation Matrix

- New `PolyQuest.Player.Exhaustion`: zero entry; 70-percent MoveSpeed and CharacterMovement synchronization; positive-before-expiry retention; expiry-at-zero retention; no timer extension; fresh post-clear cycle; Death/EndPlay cleanup; Parry block; and a real zero-cost Jump activation that preserves the Exhausted tag/timer and leaves its observable delay effect unapplied.
- Existing regression suites: `PolyQuest.Equipment.TransactionMatrix`, `PolyQuest.Melee.TraceSourceGeometry`, `PolyQuest.Player.ActionWindows`, `PolyQuest.Combat.HitReaction`, `PolyQuest.Enemy.AttackSetSelection`, `PolyQuest.Enemy.CombatSpacing`, `PolyQuest.UI.VitalHUD`, `PolyQuest.Player.LockOn`, `PolyQuest.Projectile.Lifecycle`, and `PolyQuest.Projectile.TargetAssist`.
- PIE: Exhausted permits ordinary movement and zero-cost Jump, blocks new combat/defense/Bow/Sprint starts, does not pause recovery, remains locked before three seconds, preserves committed-action cleanup, retains the Sprint physical-release gate, and leaves no speed residue after Guard Break or recovery.

## Non-Goals And Closeout

- No new Gameplay Tag, input route, generic movement system, Player death implementation, UI, replication, asset migration, or generic equipment query.
- `State.Action.Guarding.Shield` continues to mean active Shield Guard, not equipped Shield. A future accepted backpack/equipment UI stage may add a narrow `UWeaponEquipmentComponent` query based on the OffHand DefenseProfile; it must not replace this active-action tag for Guard locomotion.

## Completion Record

- User-confirmed Editor work: `BP_Player` references the existing Infinite, non-periodic `GE_Guard_MoveSpeed` for Exhaustion at `MoveSpeed x0.7`; Jump retains its Cost and Regen Delay references with a zero Stamina Cost magnitude.
- User-confirmed validation: manual `PolyQuestEditor` compilation, focused `Scene01` PIE, new `PolyQuest.Player.Exhaustion`, and the ten existing regression suites all passed from the Editor Automation front-end. The remaining logs are H3's classified negative assertions for multi-tier reaction tags, Stance Break fallback, equipment transaction rejection/rollback, and static trace geometry.
- Main defect-first fresh review against `51a9b16` found no P0-P2 in the Player exhaustion/Jump lifecycle, shared stamina-action cleanup, Enemy Poise source correlation, or Automation fixture boundary. CodeGraph and code-review-graph were read against the matching baseline; graph test-gap labels were checked against the direct Automation source and user run. Rider MCP was unavailable during the final review due to its local HTTP stream failure, so no new final IDE inspection is claimed.
- Debt handoff: no current 01I blocker or unowned validation debt remains. The future equipment-display query is deliberately not prebuilt; it is a conditional design decision for a later accepted backpack/equipment UI stage, not a substitute for active Shield Guard state.
- Commit boundary remains limited to approved `Source/PolyQuest/**`, the new/updated Automation files, and these four documents. `Content/**`, Config, maps, Blueprints, Input, AnimBPs, authored GA/GE/Montage assets, project files, generated output, and unrelated user WIP remain excluded pending explicit approval.
