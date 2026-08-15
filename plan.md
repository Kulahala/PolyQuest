# plan.md - AI Agent Collaboration File

## Rules

1. Read this file before continuing a PolyQuest stage.
2. Keep feedback, active plan, validation evidence, and unresolved blockers scoped to the current stage.
3. The implementation author cannot mark a stage as review-approved.
4. Retain the completed plan and closeout record until the next accepted stage deliberately replaces it.
5. Durable architecture belongs in `ARCHITECTURE.md`; accepted future direction and validation debt belong in `ROADMAP.md`.

---

## Active Stage: TODO-02C3B - Enemy Hit Reaction And Safe Interrupt v1

Baseline: `6c119d3 [Feature] 敌人布娃娃死亡表现 (Enemy Ragdoll Death Presentation)`.

### Objective

Establish the first non-lethal, damage-semantic hard-interrupt path for the Goblin:

- Only `GE_ChargedAttack_Damage` carries `Data.Reaction.Interrupt`, so only it can request an enemy Hit Reaction.
- Light and Sprint Attack continue to deal damage without interrupting an active enemy attack. Any small additive flinch remains owned by `TODO-07B`.
- `State.Status.Dead` remains terminal and wins before a reaction can be requested or recover movement.

The route is target-side and event driven:

```text
Charged Damage GE Asset Tag
  -> AEnemyCharacter Health change callback
  -> Event.Reaction.Enemy.Hit on the target ASC
  -> UEnemyHitReactionAbility
  -> verified reaction Montage start
  -> explicit cancellation of Ability.Attack.Enemy.Melee
  -> existing enemy melee EndAbility cleanup
```

There is no second damage, melee trace, AI state machine, Poise, hit-direction, reaction queue, Root Motion, or Reaction DataAsset path in this stage.

### Route And Ownership

```text
Outer: ue-stage-workflow
Primary: ue5-cpp-gameplay
Support: ue5-blueprint-workflow, ue5-state-tree-ai, ue5-debug-validation
Route reason: target-side GameplayEffect reception, GAS Ability lifecycle, safe attack interruption, and StateTree waiting share one enemy lifecycle boundary.
```

```text
Plan explorers: 0
Implementation executors: 0
Complex Executor: none
Main parallel work: none
Reason: Attribute/ASC, Gameplay Tag, Ability cancellation, Controller, and StateTree behavior are shared Main-only lifecycle contracts. Splitting them would increase integration risk, and the configured Luna reviewer/runtime is unavailable.
```

- Main owns C++, Gameplay Tag configuration, plan maintenance, static checks, review, document closeout, staging, and commit boundary.
- The user owns mutable Editor authoring, manual `PolyQuestEditor` compilation, Scene01 PIE/visual validation, and final commit approval.
- No live Editor write is authorized in this stage. The Editor endpoint may listen, but no callable Unreal MCP toolset is injected in this session; asset conditions below are user gates, not Editor readback evidence.

### Approved Native Changes

#### Gameplay Tags

Add these project tags in `Config/Tags/PolyQuestGameplayTags.ini`:

- `Ability.Reaction.Enemy.Hit`
- `Event.Reaction.Enemy.Hit`
- `State.Action.HitReacting`
- `Data.Reaction.Interrupt`

#### Target-Side Reaction Gate

`AEnemyCharacter::OnHealthAttributeChanged()` keeps the existing `Health <= 0 -> State.Status.Dead` path first. It sends `Event.Reaction.Enemy.Hit` to its own ASC only when all of these are true:

1. This is authoritative execution.
2. Health actually decreased and remains above zero.
3. The enemy is not dead.
4. `FOnAttributeChangeData::GEModData` is valid.
5. The applied `FGameplayEffectSpec` Asset Tags contain exactly `Data.Reaction.Interrupt`.

Healing, unchanged Health, direct/non-GE changes, rejected same-team or invulnerable hits, missed traces, and lethal damage do not request a reaction. `FMeleeHitResolver` stays unchanged: it owns GE delivery and target-side code decides whether the accepted effect has reaction semantics.

#### UEnemyHitReactionAbility

Add `UEnemyHitReactionAbility` as an `InstancedPerActor`, `ServerOnly`, Gameplay-Event-triggered ability:

- Ability Tag: `Ability.Reaction.Enemy.Hit`.
- Trigger: `Event.Reaction.Enemy.Hit` with `GameplayEvent` source.
- Owned active Tag: `State.Action.HitReacting`.
- Blocked Tags: `State.Status.Dead`, `State.Status.Stunned`, and `State.Action.HitReacting`.
- One authored `EditDefaultsOnly` field: `HitReactionMontage`.
- No Cost, Cooldown, trace, damage, queue, random selection, direction, Root Motion policy, or Reaction DataAsset.

It validates ASC, `AEnemyCharacter`, non-dead state, required tags, AnimInstance, and authored Montage. It reuses the enemy melee Montage identity filter and one guarded `EndAbility()` cleanup path. Only after `Montage_IsActive()` confirms the reaction actually started does it stop/disable CharacterMovement and explicitly call `CancelAbilities()` for `Ability.Attack.Enemy.Melee`.

Do not use the `CancelAbilitiesWithTag` property. UE 5.8 performs that cancellation during `PreActivate()`, before authored Montage startup is verified. Explicit post-start cancellation preserves an active enemy attack when the reaction Montage is invalid and still lets `UEnemyMeleeAbility::EndAbility()` clean its Trace Window, Montage, State Tag, task, and existing cooldown.

`EndAbility()` is idempotent. It unregisters the animation delegate, stops only its active Montage, ends its task, and restores `MOVE_Walking` only if this ability actually locked movement and the enemy is still alive and not tearing down. Death and teardown never restore movement.

#### Enemy Melee, Controller, And StateTree Contract

- `UEnemyMeleeAbility` adds `State.Action.HitReacting` to its activation blockers and validates that tag.
- `AEnemyAIController` exposes `IsEnemyHitReactionActive()` from the controlled enemy ASC. `TryRequestMeleeAttack()` rejects while it is active.
- `FEnemyStateTreeTask_RequestMeleeAttack` checks the reaction state at the start of Enter and Tick, returning `Running` rather than retrying an attack. After reaction ends, it retains its current semantics: an already observed attack completes after its tag clears; otherwise cooldown still waits; range failure still lets StateTree return to Chase.
- Do not change the authored `Patrol -> Alert -> Chase -> Combat -> Return` topology, Controller navigation ownership, Attack Profile, WeaponMesh, collision, Physics Asset, AnimBP, or StateTree serialized task layout.

### Native File Boundary

Add:

- `Source/PolyQuest/Public/AbilitySystem/Abilities/EnemyHitReactionAbility.h`
- `Source/PolyQuest/Private/AbilitySystem/Abilities/EnemyHitReactionAbility.cpp`

Modify only:

- `Source/PolyQuest/Public/Character/Enemy/EnemyCharacter.h`
- `Source/PolyQuest/Private/Character/Enemy/EnemyCharacter.cpp`
- `Source/PolyQuest/Public/AI/EnemyAIController.h`
- `Source/PolyQuest/Private/AI/EnemyAIController.cpp`
- `Source/PolyQuest/Public/AbilitySystem/Abilities/EnemyMeleeAbility.h`
- `Source/PolyQuest/Private/AbilitySystem/Abilities/EnemyMeleeAbility.cpp`
- `Source/PolyQuest/Private/AI/StateTree/EnemyStateTreeTasks.cpp`
- `Config/Tags/PolyQuestGameplayTags.ini`
- `plan.md`

Do not modify `FMeleeHitResolver`, `CharacterAttributeSet`, Build.cs, project settings, `ARCHITECTURE.md`, or `ROADMAP.md` until user validation and review support closeout.

### User-Owned Editor Gate

1. In `GE_ChargedAttack_Damage`, add UE 5.8's **Asset Tags (on Gameplay Effect)** component and set `Data.Reaction.Interrupt`. Do not place it in Granted Tags.
2. Do not add this tag to Light or Sprint damage GameplayEffects.
3. Create `GA_EnemyHitReaction`, parent class `UEnemyHitReactionAbility`, and add it to `BP_Enemy_Goblin` `StartupAbilities`.
4. Assign a verified `SKEL_Character_Dungeon`-compatible, full-body, in-place, non-Root-Motion reaction Montage using `DefaultGroup.DefaultSlot`.
5. The reaction Montage must not contain `AttackTraceWindow`, damage, Health, Gameplay Tag, StateTree, movement, or collision behavior.
6. Do not edit the StateTree asset, Attack Profile, collision, WeaponMesh, Physics Asset, or AnimBP for this stage.

### Validation Matrix

#### Main Static Gate

- Re-read the final Health callback, reaction Ability, melee Ability, Controller, StateTree task, resolver call boundary, and Tag configuration.
- Use CodeGraph for source call paths. Use code-review-graph only as a supplemental impact check; its `6df6dde` index is older than the `6c119d3` baseline, so direct source/diff review remains primary.
- Inspect for inherited-member-shadowing locals before asking for a build, specifically the previously observed C4458 pattern.
- Run `git diff --check`.
- Do not run UBT, Visual Studio, Live Coding, Editor write operations, or PIE.

#### User Compile And PIE Gate

By explicit user acceptance, C3B closes on focused Scene01 reaction-animation playback rather than the broader original matrix below. The accepted authored reaction clip currently carries Root Motion, while C3B intentionally calls `DisableMovement()` after playback starts, so it is accepted only as an in-place presentation fixture. This is not compile evidence, root-motion displacement evidence, or a replacement for the C3C root-motion adoption gate.

The retained checks below are regression reference for a later reaction-policy change; they are not blockers for this accepted C3B closeout.

- Compile `PolyQuestEditor` manually.
- Light/Sprint non-lethal hits only reduce Health; they do not trigger reaction.
- Charged non-lethal hits during Idle and Chase play one reaction, freeze movement, then restore navigation.
- Charged hits during enemy attack, including an open Trace Window, stop the old attack Montage and clear `State.Action.Attacking`, Trace Window, and task without residual damage.
- Repeated Charged hits during reaction do not replay, queue, or stack it. Afterward the enemy still obeys the existing attack cooldown.
- Lethal Charged damage goes directly to C2/C3D Dead presentation; it does not first play reaction or restore movement.
- Same-team, invulnerable, and missed hits do not trigger reaction. Sight loss, Return, PIE stop, and Actor teardown leave no `State.Action.HitReacting` or reaction Montage.
- Re-run regression checks for player attack, enemy attack, same-team rejection, invulnerability rejection, and C3D ragdoll/AnimBP fallback.

### Closeout And Commit Boundary

After the user-accepted focused playback gate and strict review:

- `ARCHITECTURE.md` records the stable GE Asset Tag -> target event -> reaction Ability -> post-start attack-cancel contract, plus StateTree Combat waiting during reaction.
- `ROADMAP.md` moves `TODO-02C3B` to Done. `TODO-02C3C` remains the sole owner of Poise/stance break; `TODO-07B` owns non-interrupting light flinch presentation.
- This plan remains as the most-recent-stage record until the next accepted plan replaces it.

Native commit candidates are the new reaction Ability pair, the exact Enemy Character/Controller/Melee/StateTree C++ hunks, Gameplay Tag config, and exact documentation hunks. Explicitly exclude all `Content/**`, including GA, GE, Montage, Blueprint, AnimBP, StateTree, map, imported assets, External Actors, generated directories, and unrelated existing `ROADMAP.md` WIP. A native commit must not claim a clean checkout recreates the authored reaction fixture.

### Current Status

- C3D compile and PIE evidence were user-confirmed before this stage.
- C3B native implementation is complete: target-side GE Asset Tag reception, `UEnemyHitReactionAbility`, enemy melee blocking, Controller reaction query, and StateTree Combat waiting are in the approved native boundary.
- Main static preflight completed: final source/caller reads through CodeGraph where indexed, direct readback for the new untracked Ability pair, Gameplay Tag cross-check, C4458-style local-name scan, memory MCP query, and `git diff --check`. The memory query found no matching reusable entry.
- `code-review-graph` reported a high generic impact surface but its graph was built at `6df6dde`, behind `6c119d3`; it did not cover the untracked reaction Ability pair. It is recorded only as a stale-coverage supplement, not as correctness or runtime proof.
- The user explicitly accepted focused Scene01 reaction-animation playback as the C3B runtime gate. The currently authored Root Motion clip remains visually in place because this Ability deliberately locks CharacterMovement after the Montage starts; no root-motion displacement, direction tier, Poise, or broad reaction regression is claimed by that acceptance.
- C3B strict review is complete: Main normal review plus a separately performed Main adversarial fallback found no P0-P2 C++/GAS/Tag/StateTree blocker. `gpt-5.6-luna / xhigh` remained unavailable, so no independent review is claimed.
- Debt handoff: `TODO-02C3C` owns a root-motion Big Reaction only after it defines a deliberate movement policy, navigation/collision behavior, interruption/death teardown, and focused PIE coverage. `TODO-07B` owns non-interrupting Small Reaction presentation.
