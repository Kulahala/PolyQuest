# plan.md — AI Agent Collaboration File

## Rules

1. Read this file before continuing a PolyQuest stage.
2. Keep feedback, active plan, validation evidence, and unresolved blockers scoped to the current stage.
3. The implementation author cannot mark a stage as review-approved.
4. After a stage is complete and review-approved, retain its final plan and closeout record below this header until the next accepted stage deliberately replaces it.
5. Durable architecture belongs in `ARCHITECTURE.md`; accepted future direction and validation debt belong in `ROADMAP.md`.

---

## Last Completed Stage: TODO-02C1 - First Enemy Attack Profile And Reach Contract v1

Baseline: `21fc5838a78f4032f5eace48a3591302f7a6a836`.

### Objective

Give the first Goblin axe enemy one authored, non-random attack Profile: one Montage, one damage GameplayEffect, a `180cm` actor-center two-dimensional reach, and a fixed `1.0s` cooldown that starts only after an actually started attack Montage reaches Ability cleanup. This stage does not implement death, hit reaction, Poise, stance break, special attacks, or player reactions.

### Route And Ownership

```text
Outer: ue-stage-workflow
Primary: ue5-cpp-gameplay
Support: ue5-state-tree-ai, ue5-blueprint-workflow, unreal-mcp
Route reason: The Profile, Controller cooldown, StateTree wait semantics, and GAS Ability teardown share one runtime contract.

Plan explorers: 0
Implementation executors: 0
Complex Executor: none
Main parallel work: none
Reason: Attack Profile, Controller cooldown, StateTree waiting, and GAS lifecycle are one Main-owned integration boundary. The user owns Editor authoring, manual PolyQuestEditor compilation, and Scene01 PIE validation.
```

### Approved Runtime Scope

- Add `UEnemyAttackProfile` under `Combat/Enemy/`. It owns only `AttackMontage`, `DamageGameplayEffectClass`, `AttackRange` (default `180.0f`), and `CooldownAfterAttack` (default `1.0f`), plus native validation requiring a Montage, GameplayEffect, positive range, and non-negative cooldown.
- `AEnemyCharacter` owns one static authored `AttackProfile` CDO reference and exposes it read-only. There is no Profile list, weighting, random selection, attack queue, or runtime weapon switching.
- `UEnemyMeleeAbility` resolves the Avatar Profile during activation and snapshots the active Montage, damage GameplayEffect, and cooldown. Existing Trace Window semantics, active-Montage Notify identity checks, shared `UAbilityTask_MeleeTraceWindow`, `FMeleeHitResolver`, and unified `EndAbility()` cleanup remain intact. A cooldown is reported to the Controller only after `Montage_IsActive()` confirms the configured Montage actually started.
- `AEnemyAIController` validates the possessed Pawn Profile before it starts StateTree logic. An invalid Profile logs one clear warning and leaves AI logic stopped. The Profile range populates the read-only runtime `MeleeRange`; Controller target range uses `FVector::DistSquared2D` between actor centers. The Controller owns cooldown expiration; target loss or reacquisition does not reset it.
- `FEnemyStateTreeTask_RequestMeleeAttack` fails when the target is invalid or outside the Profile range, stays `Running` during cooldown without retrying activation, observes the ASC-owned attacking tag, then succeeds only after that observed tag clears. Invalid configuration or an Ability that never actually starts fails without retaining a false attack state.
- Do not add Gameplay Tags, GameplayEffects, Build.cs dependencies, Blackboard, Behavior Tree, HFSM, a second melee resolver, or a StateTree cooldown state/branch.

### User-Owned Editor Gate

- Before compiling the new native class, record the current `GA_EnemyMelee` Montage and damage GameplayEffect references; its retired native fields are no longer a durable source after reload. Then create `Content/_DataAssets/Combat/DA_EnemyAttackProfile_GoblinAxe`, assign those references, set range `180`, cooldown `1.0`, and assign it to `BP_Enemy_Goblin`. `GA_EnemyMelee` remains the Startup Ability but no longer owns the Montage or GameplayEffect runtime source.
- Confirm the Profile Montage has `AttackTraceWindow`, enemy `WeaponMesh/BladeTraceBase/BladeTraceTip` are valid, and the Capsule responds `Block` to `MeleeTrace`.
- In `ST_Enemy_Goblin_Melee`, bind Chase `TargetActor` to the Controller target and `AcceptableRadius` to runtime `MeleeRange`; disable both agent/goal reach-radius additions and keep moving-goal tracking enabled. Keep the existing Combat task and do not add a cooldown StateTree branch.
- The Unreal MCP endpoint `http://127.0.0.1:8050/mcp` was unreachable during planning. These are authoring/readback requirements, not current Editor evidence.

### Validation And Review Record

- Main completed CodeGraph, direct source plus caller/callee inspection, refreshed code-review-graph impact inspection, and `git diff --check`. The graph does not cover the new untracked `EnemyAttackProfile.*` files, so Main reviewed those sources directly.
- Live Unreal Editor MCP readback confirmed the authored `ST_Enemy_Goblin_Melee` Root, Alert, Chase, Combat, and Return transitions plus the Chase/Return `Move To` bindings and reach-test contracts. This is Editor readback evidence, not PIE evidence.
- The user confirmed `PolyQuestEditor` compilation and Scene01 PIE for Sight/Chase, the `180cm` boundary, post-attack approximately `1.0s` cooldown, Montage interruption, target loss/reacquisition, bidirectional hits, same-team and invulnerability rejection, and Trace cleanup.
- Main normal review and a separately labelled Main adversarial fallback found no P0-P2 C++/GAS/Tag/StateTree blocker. `gpt-5.6-luna / xhigh` was unavailable, so no independent-review result is claimed.

### Closeout And Commit Boundary

- `TODO-02C1` closed the authored Chase reach-radius debt: the runtime and Chase arrival rule now share the Profile-owned exact 2D actor-center range, and its Editor readback plus boundary-distance PIE evidence are recorded above. No new placeholder TODO was created.
- The native commit includes only `EnemyAttackProfile.h/.cpp`, `EnemyCharacter.h`, `EnemyAIController.h/.cpp`, `EnemyMeleeAbility.h/.cpp`, `EnemyStateTreeTasks.cpp`, and exact TODO-02C1 documentation hunks.
- All `Content/**` remains excluded, including `BP_Enemy_Goblin`, `BP_EnemyAIController`, `ST_Enemy_Goblin_Melee`, `GA_EnemyMelee`, `GE_EnemyMelee_Damage`, Montages, AnimBPs, maps, input, imported resources, External Actors, and generated directories. `DefaultEditor.ini`, `DefaultGame.ini`, `.uproject`, `AGENTS.md`, Tags/config, and unrelated documentation WIP are also excluded. This native commit does not claim a clean checkout reproduces the authored enemy fixture.
