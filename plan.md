# TODO-03AI: Enemy Combat AI Architecture And Behavior Boundaries v1

## Status

- **Plan state:** CLOSED - Architecture and behavior-boundary gate completed; no runtime implementation was authorized in this slice.
- **Baseline:** `ccec2bb` (TODO-03A5B closed; the worktree also contains user-owned Content WIP and the uncommitted `AGENTS.md` handoff-rule update).
- **Prerequisites:** `TODO-03A5B` is closed. `TODO-03A4B` remains conditional and is not required while all authored Guard behavior is full absorption.
- **Primary question:** what durable contracts must separate StateTree intent, Controller navigation/target ownership, GAS action execution, ordinary melee spacing, ranged behavior, elite presets, Boss phases, and encounter coordination before another enemy behavior is implemented?
- **Review:** Gemini completed the requested read-only plan review with no P0-P3 findings or blocking open points; Main accepted the plan boundary and the follow-up slice order.

This is a planning and boundary gate. It does not add or modify C++, Gameplay Tags, StateTree assets, Blueprints, DataAssets, Montages, maps, or AI behavior.

```text
Outer: ue-stage-workflow
Primary: ue5-state-tree-ai
Support: ue5-architecture, ue5-debug-validation
Route reason: the stage defines StateTree shape, Controller/GAS ownership, enemy data boundaries, and future validation gates without implementing a second AI framework.
```

```text
Plan explorers: 0
Implementation executors: 0
Complex Executor: none
Main parallel work: none
Reason: this is a cross-system planning gate owned by Main; splitting the ownership and transition decisions would create conflicting contracts.
```

## Current Facts And Scope

- `AEnemyAIController` currently owns the single Player target, Focus, Home location, Sight configuration, StateTree lifetime, AttackSet validation, melee engagement range, and post-attack cooldown.
- The current StateTree has the five high-level intents `Patrol`, `Alert`, `Chase`, `Combat`, and `Return`. Native conditions query the Controller; native tasks request GAS abilities and do not mutate Health, Poise, damage, or a second AI state.
- `UEnemyAttackSet` and `UEnemyAttackProfile` currently describe weighted melee attacks, attack reach, Montage, damage GameplayEffect, Guard stamina damage, and cooldown. `UEnemyMeleeAbility` selects and snapshots one melee profile for an activation.
- The current implementation and local fixture are a melee Goblin path. No ranged enemy, elite behavior, Boss phase logic, group attack coordination, or cooldown repositioning is implemented by this stage.

## Locked Architecture Contracts

### Ownership

- StateTree owns high-level intent and transitions only. It must not become an HFSM, Blackboard replacement, damage source, or direct Montage/GameplayEffect driver.
- `AEnemyAIController` owns target validity, perception, Focus, navigation requests, Home/Leash decisions, and cooldown observation. Runtime target, movement request, and cooldown state never live in a DataAsset.
- GAS Abilities and GameplayEffects own attack execution, costs, damage, hit reaction, Stance Break, Hyper Armor, death, and teardown. Controller/StateTree wait on ASC tags and do not cancel or mutate combat state directly.
- Ordinary enemy presets own an immutable `UEnemyAIProfile` reference in addition to `UEnemyAttackSet`. `UEnemyAttackSet` remains attack-selection data; `UEnemyAIProfile` remains spatial/target-behavior data. The profile is authored data only and has no active target, timer, or movement state.

### Ordinary Melee

- The first follow-up implementation uses a bounded `Combat` subtree with `Attack`, `Reposition`, and `Wait` states while retaining the existing five top-level intents.
- After a successfully started attack ends, and while its Controller cooldown is active, the enemy may issue one finite lateral/reposition navigation request near the current engagement radius.
- Reposition must stop or fail closed when the enemy is Dead, Stunned, HitReacting, tearing down, has no valid target, exceeds Leash, or cannot project a valid navigation point. Navigation failure leaves the actor at its current location and enters a wait path; it does not invent a fallback damage or movement path.
- The existing `EngagementRange` remains AttackSet-owned. The profile may configure bounded reposition distance, acceptance tolerance, retry delay, and LeashRadius, but it does not select attacks or alter Trace/Resolver semantics.
- No continuous orbit, random attack-memory system, group token, or second melee ability is introduced by the first spacing slice.

### Leash And Target Loss

- `UEnemyAIProfile` defines a finite hard `LeashRadius` greater than the home acceptance radius and suitable for the enemy preset.
- Target death, invalidation, perception loss, or exceeding Leash clears the Controller target and Focus, stops movement, and routes to the existing `Return` intent. Infinite pursuit is forbidden.
- The current single-player target contract remains one Controller-owned Player target. Multi-target scoring, faction attitude, and multiplayer relation logic are out of scope.

### Ranged Enemies

- A future ranged enemy does **not** become ranged by changing `UEnemyAIProfile` alone. The profile can supply preferred/retreat distances only after a ranged behavior path exists.
- Ranged behavior requires a dedicated StateTree combat branch/task, Controller range predicates and navigation requests, and a Ranged GameplayAbility/Projectile contract. The Ability owns draw/fire/commit and target snapshot; the travelling projectile owns homing, target loss, and hit delivery.
- The ranged path reuses the Controller's target ownership and shared team/damage contracts. It does not reuse melee-only `IsCombatTargetInMeleeRange`, `TryRequestMeleeAttack`, `UEnemyMeleeAbility`, or melee Trace Window assumptions.
- Player Bow/projectile contracts remain owned by `TODO-03B`; the first ranged enemy implementation remains `TODO-03C` and cannot be pulled into this planning gate.

### Elite, Boss, And Encounter Boundaries

- Elite enemies reuse the ordinary Controller/StateTree/GAS framework and differ through AI Profile, AttackSet, and Ability composition. No elite-only AI framework or parallel FSM is planned.
- Bosses receive a separate StateTree and combat-profile contract for phase selection, telegraphing, phase interruption, death, and completion handoff, but still reuse the established GAS, target, damage, and teardown foundations. Concrete Boss Phase 1/2 implementation belongs to `TODO-06B`.
- Encounter-level attack slots, reservation, rotation, crowd spacing, and multi-enemy coordination belong to `TODO-04A`. This stage defines no global coordinator or attack token API.
- Encounter persistence, rewards, fog gates, and completion state remain outside the Boss StateTree and follow `TODO-03D`/`TODO-04A` ownership.
- Behavior Trees remain unadopted. Reconsider them only if a documented StateTree escalation condition is proven: deeply nested reactive arbitration, dynamic subtrees, or broad concurrent services that cannot remain testable in StateTree.

## Planning Work And Handoff

1. Record an ownership matrix for StateTree, Controller, `UEnemyAIProfile`, AttackSet/Profile, GAS, projectile delivery, encounter coordination, and Boss completion.
2. Record the ordinary melee transition contract (`Combat -> Attack/Reposition/Wait`), Leash/target-loss exits, interruption/death rules, and navigation-failure behavior without adding implementation code.
3. Record the ranged prerequisite chain and explicitly state that an AI Profile-only edit cannot implement approach/retreat for a ranged enemy.
4. Record the dependency order: ordinary melee spacing implementation -> player Bow/projectile (`TODO-03B`) -> ranged enemy (`TODO-03C`), with `TODO-04A` for coordination and `TODO-06B` for the first Boss.
5. Keep `ROADMAP.md` as the durable milestone owner. Do not update `ARCHITECTURE.md` with unimplemented runtime contracts until a follow-up implementation passes compile, Editor readback, Automation, and PIE gates.

This stage has no implementation executor. If a later implementation slice is assigned to Gemini, Main must provide a handoff prompt containing the absolute cwd `E:\GameDevelop\PolyQuest`, baseline `ccec2bb`, active plan and approved path list, `ue-stage-workflow` plus the concrete Skill (`ue5-state-tree-ai` and/or `ue5-cpp-gameplay`), execution order, non-goals, user-owned Editor/compile/PIE gates, evidence requirements, strict self-review boundary, stop conditions, and the prohibition on scope expansion, destructive changes, and committing.

## Validation And Closeout

- Static gate: re-read the current Controller, StateTree conditions/tasks, EnemyCharacter, AttackSet/Profile, and architecture/roadmap entries; confirm no proposed owner duplicates an implemented source of truth.
- Plan gate: every behavior has an owner, entry/exit conditions, failure policy, non-goal, future stage, and validation trigger. No open architecture decision is left to an implementation executor.
- Review gate: the independent read-only Gemini review returned `READY` with `No findings`; this stage has no runtime implementation to compile or execute.
- No C++ compile, Editor readback, Automation, or PIE result is claimed for this planning-only stage.
- Preserve all user-authored `Content/**`, Blueprint, StateTree, Montage, map, Config, and project WIP. No asset deletion or migration is part of this plan.

## Closeout And Next Slice

- `TODO-03AI` is complete as a planning gate only. The next implementation-ready plan must be a separate bounded `TODO-03AI1: Ordinary Melee Cooldown Spacing And Reposition v1` slice; it is not implemented or authorized here.
- `TODO-03H1` is intentionally scheduled after that implementation slice so its health review has a real behavior delta to inspect.

## Accepted Deferred Risks

- Exact lateral direction selection and numeric spacing tuning are implementation-slice decisions constrained by the bounded navigation contract; they are not global AI state.
- Ranged preferred/retreat ranges remain dormant until a Ranged Ability/Projectile and StateTree branch exist; adding fields alone is not a valid implementation.
- Partial Guard outcomes remain owned by `TODO-03A4B` and are not folded into enemy AI planning.
