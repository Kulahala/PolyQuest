# plan.md - AI Agent Collaboration File

## Rules

1. Read this file before continuing a PolyQuest stage.
2. Keep feedback, active plan, validation evidence, and unresolved blockers scoped to the current stage.
3. The implementation author cannot mark a stage as review-approved.
4. Retain the completed plan and closeout record until the next accepted stage deliberately replaces it.
5. Durable architecture belongs in `ARCHITECTURE.md`; accepted future direction and validation debt belong in `ROADMAP.md`.

---

## Active Stage: TODO-02C3C - Enemy Poise And Stance Break v1

Baseline: `24c1114 [Feature] 共享闪避与奔跑输入仲裁 (Shared Dodge/Sprint Input Arbitration)`.

### Objective

Give the first Goblin an ASC-owned `Poise` / `MaxPoise` contract. Player Light, Sprint Attack, and Charged damage GameplayEffects deplete Poise through the existing shared melee delivery path. Crossing from positive Poise to zero starts one server-authoritative Stance Break Ability that uses the existing `State.Status.Stunned` tag, safely interrupts enemy melee/C3B reaction only after its authored Montage starts, and restores Poise after the break completes.

Locked values:

- `MaxPoise = 100`, `Poise = 100` defaults in `UCharacterAttributeSet`.
- `GE_LightAttack_Damage` authors fixed `Poise -15`.
- `GE_SprintAttack_Damage` authors fixed `Poise -25`.
- Charged attack uses existing charge alpha to linearly send `Data.Poise.Charged` from `-25` through `-50` with its existing `Data.Damage.Charged` value.
- Partial Poise damage waits `2.0s`, then restores `10/s` in `0.1s` steps through an authored Instant recovery GameplayEffect.
- The first Stance Break is full-body and functionally in-place. Its current authored source may contain root translation, but `DisableMovement()` deliberately suppresses actor displacement; this is not functional Root Motion stance-break behavior. Directional reactions, root-motion break behavior, player Poise/reaction, Poise UI, block, parry, hyper armor, and StateTree topology changes are out of scope.

### Route And Ownership

```text
Outer: ue-stage-workflow
Primary: ue5-cpp-gameplay
Support: ue5-state-tree-ai, ue5-blueprint-workflow, ue5-debug-validation, unreal-mcp
Route reason: AttributeSet mutation, shared GameplayEffect delivery, target-side event routing, Ability teardown, and Controller/StateTree action gates share one enemy lifecycle boundary.
```

```text
Plan explorers: 0
Implementation executors: 0
Complex Executor: none
Main parallel work: none
Reason: ASC, AttributeSet, shared resolver, Gameplay Tags, and enemy lifecycle contracts are Main-only integration territory. Splitting them would create concurrent writers over the same state.
```

Main owns C++, tags, source/static checks, strict review, documentation, staging, and commit boundary. The user owns all mutable Content authoring, Editor readback, manual `PolyQuestEditor` compilation, Scene01 PIE/visual validation, and final commit approval. No current live Unreal MCP readback is claimed.

### Approved Native Contract

#### Attribute And Shared Delivery

- `UCharacterAttributeSet` gains `Poise` and `MaxPoise`; every final/base/GameplayEffect mutation clamps Poise to `[0, MaxPoise]`. AttributeSet only clamps values and never selects death or stance behavior.
- `FMeleeHitRequest`, `UAbilityTask_MeleeTraceWindow`, and `FMeleeHitResolver` accept a narrow multi-SetByCaller collection. Existing single-value callers retain their current behavior through a compatibility overload; Charged supplies both damage and Poise values through one authored Damage GE Spec.
- `UChargedAttackAbility` stores `MinimumPoiseDamage`, `MaximumPoiseDamage`, `PoiseDataTag`, and its released Poise magnitude. It validates the new values/tags, computes them from the existing charge alpha, and resets them through existing `EndAbility()` cleanup.

#### Enemy Poise Lifecycle

- `AEnemyCharacter` binds and unbinds a Poise delegate with its existing Health/Dead bindings. A positive-to-zero crossing clears pending recovery and schedules a next-tick Stance Break dispatch so all modifiers from the same GE settle before Dead is checked.
- The deferred dispatch requires authority, a live non-destroying enemy, Health above zero, a valid Stance Break event, and a valid Poise recovery configuration. It sends `Event.Reaction.Enemy.StanceBreak` once; a missing/ungiven Ability logs and restores Poise instead of leaving an unrepeatable zero state.
- Nonlethal partial Poise damage restarts one timer. The timer creates an outgoing Spec from `PoiseRecoveryGameplayEffectClass`, sets positive `Data.Poise.Recovery`, and applies it to the same ASC. It stops when full, dead, stunned, tearing down, invalid, or unable to advance Poise.
- `AEnemyCharacter` exposes only the narrow native helpers required by the new Ability: `IsPoiseBroken`, `HasValidPoiseRecoveryConfiguration`, and `RestorePoiseToMax`. They never directly write the Attribute; the authored Instant GE remains the mutation path.
- Death clears both timer paths before `CancelAllAbilities()`. Dead remains terminal and suppresses both recovery and Stance Break completion.

#### Stance Break, C3B, And AI

- Add `UEnemyStanceBreakAbility`: `InstancedPerActor`, `ServerOnly`, triggered by `Event.Reaction.Enemy.StanceBreak`, tagged `Ability.Reaction.Enemy.StanceBreak`, and owning the existing `State.Status.Stunned` for its active lifetime.
- It blocks Dead and re-entry but may preempt active `State.Action.HitReacting`. It verifies ASC, living broken enemy, valid recovery configuration, AnimInstance, required tags, and an authored `StanceBreakMontage` before starting.
- Only after `Montage_IsActive()` succeeds does it stop/disable CharacterMovement and explicitly cancel `Ability.Attack.Enemy.Melee` and `Ability.Reaction.Enemy.Hit`. Its guarded `EndAbility()` removes delegates/tasks, restores walking and full Poise only for a live non-destroying enemy, and never undoes death teardown.
- C3B `UEnemyHitReactionAbility::EndAbility()` must not restore Walking while `State.Status.Stunned` is active. Health reaction dispatch skips `Data.Reaction.Interrupt` when current Poise is already zero; the Stance Break route wins regardless of GE modifier order.
- `AEnemyAIController::IsEnemyStunned()` is ASC-tag based. Controller attack requests reject Dead, Stunned, HitReacting, invalid Profile/target/range, and cooldown. The existing StateTree melee task returns `Running` during Stunned; no StateTree asset layout, task base, instance data, transition, or navigation contract changes.

#### Tags

Add only:

- `Ability.Reaction.Enemy.StanceBreak`
- `Event.Reaction.Enemy.StanceBreak`
- `Data.Poise.Charged`
- `Data.Poise.Recovery`

Reuse `State.Status.Stunned`; do not add another break state/tag hierarchy.

### User-Owned Editor Gate

1. Add `Poise -15` to `GE_LightAttack_Damage` and `Poise -25` to `GE_SprintAttack_Damage`.
2. Add a `Poise` SetByCaller Modifier using `Data.Poise.Charged` to `GE_ChargedAttack_Damage`; retain its existing damage modifier and `Data.Reaction.Interrupt` Asset Tag.
3. Create Instant `GE_EnemyPoise_Recovery`: one `Poise` Modifier using `Data.Poise.Recovery`; it grants no state tags.
4. Set `BP_Enemy_Goblin.PoiseRecoveryGameplayEffectClass` to that GE. Create `GA_EnemyStanceBreak`, parent it to `UEnemyStanceBreakAbility`, add it to StartupAbilities, and assign one `SKEL_Character_Dungeon`-compatible full-body, functionally in-place Montage in `DefaultGroup.DefaultSlot`. The source may contain authored root translation, but this stage does not adopt functional Root Motion movement.
5. Do not edit `ST_Enemy_Goblin_Melee` topology/bindings, AnimBP state machines, WeaponMesh, collision, Physics Asset, map, or current authoring WIP for this stage.

### Validation Matrix

Main static gate: final source/caller reads, CodeGraph, stale code-review-graph only as supplemental coverage, Tag cross-check, C4458 inherited-member-shadow scan, and `git diff --check`. Main does not run UBT, Editor writes, or PIE.

User compile/PIE gate:

- Verify Light `-15`, Sprint `-25`, Charged `-25..-50` Poise delivery and two-second delayed `10/s` partial recovery.
- Verify one Stance Break at zero Poise, no requeue while Stunned, safe attack/Trace/C3B cleanup, full recovery at break end, and StateTree navigation recovery.
- Verify a same-hit lethal outcome goes directly to Dead/ragdoll or AnimBP fallback with no Stance Break recovery.
- Verify interruption, invalid presentation configuration, repeated hits, target loss, teardown, same-team rejection, invulnerability, misses, player actions, enemy combat, death, and ragdoll regressions.

### Documentation And Commit Boundary

After user compile/PIE and strict review: update only stable C3C contracts in `ARCHITECTURE.md`, move `TODO-02C3C` to Done in the exact `ROADMAP.md` hunk, preserve its conditional root-motion debt, and retain this plan as the current-most-recent handoff until a later stage replaces it.

Native candidate paths are `CharacterAttributeSet.*`, `ChargedAttackAbility.*`, new `EnemyStanceBreakAbility.*`, `EnemyCharacter.*`, `EnemyAIController.*`, `EnemyHitReactionAbility.*`, `AbilityTask_MeleeTraceWindow.*`, `MeleeHitResolver.*`, `EnemyStateTreeTasks.cpp`, `Config/Tags/PolyQuestGameplayTags.ini`, and exact documentation hunks. Exclude every `Content/**` item, project/editor config WIP, StateTree/GA/GE/Montage/Blueprint assets, maps, imports, generated directories, and unrelated `ROADMAP.md` changes. No commit occurs without explicit approval.

### Review And Closeout Record (2026-08-16)

#### Validation Evidence

- User-confirmed: manual `PolyQuestEditor` compilation passed again after the later `OnPossess()` logging repair. Earlier user confirmation also covers a focused Scene01 PIE route for Poise/Stance Break. Individual validation-matrix subcase results were not separately recorded here.

#### Main Strict Review (normal pass plus Main adversarial fallback)

Scope: the full native/config diff against baseline `24c1114` plus direct callers/callees (`BaseCharacter`, `EnemyMeleeAbility`, StateTree tasks), read in full.

Result: no P0/P1 source defect. All nine targeted attack surfaces closed with file/line and engine-source evidence:

- GE Modifier order: per-modifier execution (`GameplayEffect.cpp` `InternalExecuteMod`) makes both authored orders converge on the Stance Break route; `OnHealthAttributeChanged` gates `Data.Reaction.Interrupt` dispatch on `IsPoiseBroken()`.
- Same-hit lethal: lethal Health promotion runs before the reaction block (`EnemyCharacter.cpp` lethal block first), and `HandleDeath()` clears the pending Stance Break timer, so death always wins regardless of modifier order.
- Stance/C3B Montage preemption: `ActivationOwnedTags` are applied in `PreActivate` before `ActivateAbility` (engine `GameplayAbility.cpp`), so a same-frame interrupted C3B already sees `State.Status.Stunned`; both abilities query the tag dynamically at their `EndAbility()`, so exactly one side restores walking in every completion order.
- Deferred timer: one pending flag, cleared on death, EndPlay, and dispatch; zero-Poise re-damage is clamped to a no-change event and cannot requeue.
- Invalid Montage: movement lock and `CancelAbilities` run only after `Montage_IsActive()` confirmation; the failed-start and no-ability-accepted paths restore Poise through the authored Instant GE (engine `HandleGameplayEvent` counts successful activations only).
- Duplicate events: `Event.Reaction.Enemy.Hit`'s only sender skips while Poise is zero; the Stance Break event fires once per positive-to-zero crossing.
- StateTree reentry: `FEnemyStateTreeTask_RequestMeleeAttack` returns `Running` while Stunned and `AEnemyAIController::TryRequestMeleeAttack()` rejects Stunned.
- SetByCaller overwrite: the single-value field applies before the multi-tag map; Charged is the only map user (`Data.Damage.Charged` + `Data.Poise.Charged`), while Light/Sprint/EnemyMelee keep the single-value overload, so no tag overlaps.
- EndAbility reentry: `bEndAbilityRequested` guard plus reset in `ActivateAbility`; Poise restoration runs before `Super::EndAbility()` removes Stunned, and the resulting increase event is ignored by the decrease-only reaction handler.

Findings and repair:

- P2 (repaired): `AEnemyAIController::OnPossess()` returned silently when `HasValidPoiseRecoveryConfiguration()` failed, unlike the adjacent AttackProfile gate which logs. Repair: one-time `UE_LOG Warning` plus `bHasLoggedInvalidPoiseRecoverySetup` flag (`EnemyAIController.cpp`, `EnemyAIController.h`); behavior unchanged (fail-closed StateTree start denial).
- P3 (accepted, no change): a looping authored Stance/reaction Montage has no timeout; death cancellation is the only exit. Same accepted exposure as C3B; the Editor gate already requires a non-looping full-body in-place Montage.
- P3 (accepted, no change): with an invalid Poise recovery configuration the deferred dispatch returns without restoration; restoration is technically impossible without the authored recovery GameplayEffect, the condition is the plan-stated dispatch prerequisite, and the Controller gate plus logged warnings make the misconfiguration fail closed rather than silently wrong.

Static evidence only: baseline diff reads, full-file reads, four engine-source semantic verifications (`HandleGameplayEvent` counting, `CancelAbilities(With, Without, Ignore)`, `ActivationOwnedTags` timing, per-modifier execution), tag cross-check, C4458-style shadow scan, `git diff --check`, CodeGraph caller closure, and an empty error-memory query. The requested fresh independent reviewer channel remained unavailable, so the second pass is a labeled Main adversarial fallback, not an independent review; the user additionally scheduled a separate codex confirmation of this review and repair.

#### Documentation Synchronization

- `ARCHITECTURE.md`: added the stable Enemy Poise And Stance Break contract, updated the AttributeSet field list, Charged dual SetByCaller delivery, and the Gameplay Tag inventory; moved remaining Poise-adjacent future work (player Poise/reaction, directional tiers, root-motion break adoption) to the not-yet-established list.
- `ROADMAP.md`: moved `TODO-02C3C` to Done Milestones with the conditional root-motion Big Reaction debt preserved, updated Current State, and removed the open milestone entry.
- `plan.md`: retained this record as the current most-recent stage handoff until the next accepted stage replaces it.
