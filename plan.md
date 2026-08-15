# plan.md — AI Agent Collaboration File

## Rules

1. Read this file before continuing a PolyQuest stage.
2. Keep feedback, active plan, validation evidence, and unresolved blockers scoped to the current stage.
3. The implementation author cannot mark a stage as review-approved.
4. After a stage is complete and review-approved, retain its final plan and closeout record below this header until the next accepted stage deliberately replaces it.
5. Durable architecture belongs in `ARCHITECTURE.md`; accepted future direction and validation debt belong in `ROADMAP.md`.

---

## Last Completed Stage: TODO-02C2 - First Enemy Death And Teardown v1

Baseline: `165c8d4`.

### Objective

Establish the first Goblin's terminal enemy-only outcome. When Health reaches `0`, native code adds `State.Status.Dead`, stops StateTree, movement, Controller focus, active GAS work, Montage, and Trace Window, then rejects future shared melee delivery from or to that enemy. Player death, reactions, Poise, rewards, drops, respawn, ragdoll, and collision policy remain out of scope.

### Route And Ownership

```text
Outer: ue-stage-workflow
Primary: ue5-cpp-gameplay
Support: ue5-state-tree-ai, ue5-blueprint-workflow, unreal-mcp
Route reason: Health, ASC Tag ownership, Ability teardown, Controller/StateTree stop, and the shared resolver must form one terminal lifecycle.

Plan explorers: 0
Implementation executors: 0
Complex Executor: none
Main parallel work: none
Reason: AttributeSet, ASC, Gameplay Tag, Controller, and shared combat delivery are Main-only integration territory. The user owns Editor authoring, manual PolyQuestEditor compilation, and Scene01 PIE validation.
```

### Approved Runtime Scope

- `UCharacterAttributeSet` clamps Health to `[0, MaxHealth]`; when the target ASC already owns `State.Status.Dead`, it preserves Health at `0`. It does not decide which character enters death.
- `AEnemyCharacter` binds Health and Dead Tag events after Base ActorInfo initialization. Health at or below zero sets the Dead loose-tag count to exactly one. The Dead Tag callback promotes any legal Dead source to that same terminal loose count, then owns idempotent teardown including Controller cleanup, `CancelAllAbilities()`, and disabling CharacterMovement. `IsDead()` reads the ASC Tag; its private guard prevents duplicate teardown only.
- `AEnemyAIController` stops StateTree, path movement, target, and focus through one controlled-enemy death entry point. Possession, perception, alert, attack requests, cooldown, and target validity fail closed while its pawn is dead. The authored StateTree topology does not change.
- `FMeleeHitResolver` rejects a request when either source or target ASC owns `State.Status.Dead`, while retaining its existing self, team, ASC, and invulnerability guards. `UEnemyMeleeAbility` remains unchanged; Controller cooldown ignores a post-teardown Ability callback.
- No new Gameplay Tag, GameplayEffect, Ability, DataAsset, Build.cs dependency, StateTree node, Blackboard, Behavior Tree, animation path, or asset reference is introduced.

### User-Owned Editor Gate

- Confirm `BP_Enemy_Goblin` remains an `AEnemyCharacter`, its Profile/Startup Ability remain valid, and the existing player damage GameplayEffect actually modifies enemy Health.
- C2 does not author a death animation or AnimBP terminal state. The current accepted visual result is the native stopped terminal pose; `TODO-02C3` owns the future `State.Status.Dead`-driven one-way death presentation, which may read but never mutate Health, Tags, StateTree, or Controller state.
- The current `http://127.0.0.1:8050/mcp` read route is unavailable. Asset/Blueprint expectations are user authoring gates, not current Editor readback evidence.

### Validation, Review, And Closeout Record

- Main completed final source/caller/callee inspection, CodeGraph, a refreshed code-review-graph impact pass, Gameplay Tag configuration verification, and `git diff --check`. The graph's high-risk/test-gap classification was treated as coverage guidance, not as a runtime finding; direct review found no unresolved P0-P2 C++/GAS/Tag/StateTree blocker. `gpt-5.6-luna / xhigh` was unavailable, so strict review used a separately labelled Main adversarial fallback rather than an independent result.
- User confirmed `PolyQuestEditor` compilation and Scene01 PIE. The reported terminal behavior is: after enemy Health reaches zero the enemy no longer acts; later attacks leave Health at zero and do not drive it negative. No UBT, Visual Studio, PIE, or Editor write was run by Main.
- Debt handoff: `TODO-02C3` owns enemy reactions, Poise, stance break, and the `State.Status.Dead`-driven authored terminal presentation; its AnimBP may read the Tag but must not mutate Health, Tags, StateTree, or Controller. `TODO-03D` owns player death, reload, and revival. No new placeholder TODO is created.
- Commit scope: `CharacterAttributeSet.cpp`, `EnemyCharacter.h/.cpp`, `EnemyAIController.h/.cpp`, `MeleeHitResolver.cpp`, plus exact TODO-02C2 `ARCHITECTURE.md` / `ROADMAP.md` / `plan.md` hunks. All `Content/**`, config, `.uproject`, `AGENTS.md`, imported resources, plugins, and generated output remain excluded; this is not a clean-checkout authored death fixture.
