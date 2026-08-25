# TODO-02C4: Enemy Death Ragdoll Impact v1

## Plan State

- Status: Completed. Gemini implemented the frozen Enemy lifecycle slice; Main repaired the review-found test-vacuity gap, completed the delta Fresh Review, and the user authorized documentation closeout and commit after confirming the current PIE and Automation gates.
- Baseline: `218df1b` (`[Docs] 完成作者提示元数据 (Authoring Tooltip Metadata)`).
- Objective: make a lethal Enemy hit produce one readable attacker-away ragdoll motion instead of the current zero-horizontal-momentum collapse, while retaining the existing Dead-tag teardown as the sole gameplay authority.
- Player-facing scope: every current lethal Health GameplayEffect with a valid current attacker-direction context—melee, projectile, or future damage that follows the same GAS route—may contribute one ragdoll velocity change. Damage with no usable current context safely keeps the existing natural ragdoll fall.
- Preserve all current user WIP. `Config/**`, `Content/**`, maps, Blueprints, AnimBPs, GA/GE/Montage assets, `.uproject`, generated files, and unrelated source changes are neither implementation nor commit candidates unless the user explicitly expands the scope.

## Route And Delegation

Outer: `ue-stage-workflow`
Primary: `ue5-cpp-gameplay`
Support: `physics-tuning`, `ue5-debug-validation`
Route reason: this is one native Enemy lifecycle change spanning a GAS Attribute callback, terminal teardown, SkeletalMesh physics startup, data-authored presentation values, and a deterministic non-Chaos Automation boundary.

Plan explorers: 0
Implementation executors: 1 (Gemini only after its read-only plan review is accepted and the user explicitly authorizes execution)
Complex Executor: one scoped lifecycle-sensitive implementation
Main parallel work: none
Reason: `OnHealthAttributeChanged -> SetDeadState -> HandleDeath -> StartDeathRagdoll` is one synchronous shared lifecycle. Splitting ownership would increase the risk of stale context, duplicate impulse, or teardown regressions. Main retains architecture, the frozen contract, documentation, validation interpretation, staging, and commit ownership.

## Frozen Runtime And Authoring Contract

### 1. Enemy-owned authoring surface

Contract owner: Main. Implementation writer: Gemini. The only product Header/API surface change is private authored state on `AEnemyCharacter`; no new public Blueprint function, Gameplay Tag, GameplayEffect, DataAsset, module dependency, or Config route is allowed.

- Add private `EditDefaultsOnly`, `BlueprintReadOnly`, `AllowPrivateAccess` fields under `Combat|Enemy|Death`, each with concise Chinese ToolTips:
  - `DeathRagdollImpulseBoneName`: `FName`, default `NAME_None`. It must name one actual simulated Physics Asset body when directional force is wanted; `NAME_None` is a legal silent no-directional-force default and must never fall back to the root body.
  - `DeathRagdollHorizontalVelocityChange`: `float`, default `2000.0f`, `ClampMin = 0`, `Units = CentimetersPerSecond`.
  - `DeathRagdollUpwardVelocityChange`: `float`, default `500.0f`, `ClampMin = 0`, same velocity unit.
- These are velocity-change values, not mass-dependent force values: the runtime must use `AddImpulse(..., bVelChange = true)`. The naming must not call either field `ImpulseStrength`.
- The initial generic `450 / 180 cm/s` estimate was replaced by the current `2000 / 500 cm/s` authored baseline after user-confirmed PIE showed that a single constrained Pelvis body did not produce readable whole-body motion at the lower setting. These are fallback CDO values, not a guarantee that an existing Blueprint override changes automatically.
- The fields live on the Enemy Character CDO rather than `UEnemyAIProfile`, `UEnemyAttackProfile`, or a GameplayEffect: bone selection and ragdoll feel belong to the mesh/Physics Asset presentation archetype, not AI movement or the damage formula.
- The native CDO deliberately supplies no default bone. The user configures a verified physical bone on `BP_Enemy_Goblin` after an Editor/Physics Asset readback; do not assume or hard-code `pelvis` without that evidence.

### 2. Lethal-context capture and terminal consumption

- Add one private non-reflected value cache, `FVector PendingDeathRagdollVelocityChange`, initialized to zero. It may hold only a finite velocity vector; it must never retain `FGameplayEffectSpec*`, `FGameplayEffectModCallbackData*`, an Effect Context pointer, or an earlier hit's source.
- In `AEnemyCharacter::OnHealthAttributeChanged`, inside the existing `NewValue <= 0` branch and before the first `SetDeadState()` call:
  1. only when this is the first terminal transition (`!IsDead()`), clear the pending vector;
  2. require a real Health decrease and current `ChangeData.GEModData`;
  3. read only the current `EffectSpec.GetContext()` and use `FHitReactionImpactResolver::ResolveImpactDirectionFromContext()` unchanged;
  4. require a finite current Enemy planar yaw, then convert its documented target-local `Target -> Attacker` planar direction to a finite world-space `Attacker -> Target` direction and build horizontal attacker-away velocity plus the configured upward velocity;
  5. retain the result only when the direction is nonzero/finite, horizontal velocity change is strictly positive, upward velocity change is finite and nonnegative, and the full vector is finite.
- Do not use `TryBuildLaunchVelocity()`: its contract requires a positive vertical speed and owns launch-reaction semantics. C4 may legitimately use zero upward change and owns death-ragdoll presentation only.
- `SetDeadState()` remains immediately after this capture. The existing Dead-tag callback remains the only caller of terminal teardown; no death Ability, reaction tier gate, direct Health write, Root Motion route, or new damage path is introduced.
- Do not remove `HandleDeath()`'s existing `StopMovementImmediately()` or `DisableMovement()`. They clear navigation/CharacterMovement state; ragdoll receives its independent velocity change only after simulation has been enabled.

### 3. Ragdoll injection and fail-closed behavior

- `StartDeathRagdoll()` consumes and clears `PendingDeathRagdollVelocityChange` on every entry before any early return, so disabled ragdoll, missing Physics Asset, repeated calls, and teardown can never reuse a stale candidate.
- Preserve the existing terminal setup order: disable fixed weapon display collision; disable Capsule collision/overlaps; apply the `Ragdoll` profile; enable mesh simulation; wake rigid bodies.
- After physics is active, apply exactly one `SkeletalMesh->AddImpulse(ConsumedVelocityChange, DeathRagdollImpulseBoneName, true)` only if all of the following are true: ragdoll is enabled, not previously started, SkeletalMesh and Physics Asset are valid, the consumed vector is valid/nonzero, the authored bone name is non-None, `GetBodyInstance(BoneName)` returns a body, and the SkeletalMesh reports that named body as simulating physics.
- A configured non-None bone that is missing or not simulated is a configuration fault: preserve the normal ragdoll without directional force and log one `LogPolyQuest` Warning per Enemy. `NAME_None`, missing current effect context, zero direction, zero/invalid velocity settings, disabled ragdoll, or teardown are normal no-impact paths and must not log.
- Keep the existing missing Physics Asset warning/fallback behavior. Do not add corpse lifetime, destruction delay, Physics Asset filesystem edits, collision-profile redesign, animation replacement, player death, or a general impulse framework.

### 4. Test-only seam and Automation surface

- Add only narrow `#if WITH_DEV_AUTOMATION_TESTS` readback/configuration helpers on `AEnemyCharacter` for this stage: the most recent lethal candidate, the current pending value, and capture/consume counts. They must be unavailable in non-test builds and must not bypass production Health, Dead-tag, or ragdoll code paths.
- The tests may set death-ragdoll authoring values through a test-only helper, but must not alter the shared fixture's default passive-enemy rule that disables real ragdoll. No test-only fake Physics Asset, body simulation bypass, production log suppression, or direct Health-callback/Attribute mutation to simulate damage or death is permitted. The existing fixture-style numeric Health setup is allowed only to establish the lethal baseline before a real GameplayEffect Spec delivers the damage.
- Add `Source/PolyQuest/Private/Tests/EnemyDeathRagdollAutomationTests.cpp` as `PolyQuest.Enemy.DeathRagdoll`, reusing `FCombatAutomationFixture` and the existing native Health damage GE/spec route. Do not add a new test GE only for this stage unless the existing test GE cannot express a lethal Health decrease.
- Required cases:
  - a valid current lethal context captures exactly the configured attacker-away horizontal velocity and upward component;
  - a prior nonlethal hit from attacker A cannot influence a later lethal hit from attacker B;
  - invalid/empty current context, zero direction, invalid velocity configuration, and direct Dead-tag receipt yield no valid candidate;
  - terminal processing consumes/clears the value once, and repeated Health/Dead callbacks do not capture or consume again;
  - the normal no-asset/no-ragdoll fixture route remains silent and does not affect living Enemy hit reaction, Poise, AI, or damage delivery.
- Automation proves callback ownership, current-context isolation, velocity construction, and once-only consumption. It does not claim real Chaos displacement, rigid-body tumble quality, or Physics Asset correctness; those are explicit user PIE gates.

## Approved Paths, Execution Order, And Stop Conditions

### Approved implementation paths

1. `Source/PolyQuest/Public/Character/Enemy/EnemyCharacter.h`
2. `Source/PolyQuest/Private/Character/Enemy/EnemyCharacter.cpp`
3. `Source/PolyQuest/Private/Tests/EnemyDeathRagdollAutomationTests.cpp`

`FHitReactionImpactResolver`, `FCombatAutomationFixture`, `Build.cs`, `Config/**`, Blueprint/AnimBP/Physics Asset files, project documentation, staging, and commits are prohibited implementation targets. If the executor believes any unlisted file, public API, Tag, input route, asset write, Physics Asset edit, or lifecycle policy is necessary, it must stop and report concrete source evidence for a Main decision.

### Execution order

1. Gemini performs a read-only plan review over the three approved paths plus the direct `FHitReactionImpactResolver` and existing Health-damage test route. It reports P0-P2 findings, real gaps, non-blocking recommendations, and Automation/Editor feasibility only; it does not edit, compile, invoke the Editor, stage, or commit.
2. After the user accepts that review, Gemini implements only the frozen C++/test slice. It rereads the plan and preserves every existing Dead, Hit Reaction, Poise, AI, movement, and ragdoll fallback contract.
3. Gemini runs Rider `get_file_problems` or `lint_files` for touched C++ files plus `git diff --check`; it returns changed paths, static evidence, unrun user gates, strict self-review findings, and remaining risks. It must not claim compile, Editor, PIE, or visual evidence.
4. The user performs the Editor authoring/readback, manual `PolyQuestEditor (Development Editor)` compile, Automation, and PIE gates below. Gemini does not write `.uasset` files or live Editor state.
5. Main performs the separate defect-first Fresh Review after user evidence, then synchronizes documentation and prepares a scoped commit only with explicit user authorization.

## Validation Matrix

### Static gate before user handoff

- Read the final diff and the direct lethal/death call chain; verify the context is transformed and copied before `SetDeadState()`, and the vector is consumed before every `StartDeathRagdoll()` early return.
- Verify no callback-local pointer survives the Health delegate, no shared resolver semantic changes, no new warning occurs for normal missing-context/no-impact paths, and no test helper escapes `WITH_DEV_AUTOMATION_TESTS`.
- Rider reports zero Errors for the touched C++ paths; `git diff --check` passes. CodeGraph/code-review-graph are supplemental source/impact evidence only, not runtime proof.

### User-owned Editor, compile, and runtime gates

1. In `BP_Enemy_Goblin`, inspect the actually assigned SkeletalMesh and Physics Asset. Set `DeathRagdollImpulseBoneName` to one verified physical body, keep ragdoll enabled, and confirm the two velocity-change fields expose the intended units/tooltips. A serialized Blueprint value (for example the previous `750 / 280`) overrides the Native `2000 / 500` fallback and must be changed explicitly. Do not assume a bone name from documentation.
2. Tune Physics Asset damping, constraints, and collision in Unreal Editor only. No generic damping/constraint number is a frozen product constant in this stage.
3. Compile `PolyQuestEditor (Development Editor)` and report the exact result.
4. Run the full Editor Automation matrix, including the new `PolyQuest.Enemy.DeathRagdoll` suite and the existing Hit Reaction, Projectile Lifecycle, Melee, Equipment, and Enemy AI regressions. Expected successful paths must be free of unhandled `LogPolyQuest` warnings.
5. In `Scene01`, kill the Goblin from front, rear, and side with both a melee hit and an arrow. Confirm each corpse initially moves away from the actual attacker, gets only a light upward lift, then naturally slides/tumbles without jitter, explosion, persistent collision interference, or a living-Enemy behavior regression.
6. Temporarily configure an invalid bone in Editor, confirm the Enemy still enters ordinary ragdoll safely with a single configuration warning and no directional launch, then restore the verified bone before closeout. Also confirm a missing/disabled ragdoll configuration preserves its pre-C4 fallback.

## Non-Goals, Documentation, And Commit Boundary

- Out of scope: per-reaction-tier death force, mass-scaled force, damage scaling, GameplayCue, death montage selection, Physics Asset asset changes by source control, corpse persistence/recovery, root-motion changes, player death, AI redesign, and `TODO-03H4B` slimming work.
- `ARCHITECTURE.md` receives only the stable implemented ownership/lifecycle after compile, Automation, PIE, and Fresh Review evidence. `ROADMAP.md` moves C4 to Done only after the same gates; no unproven Physics tuning values go into architecture or roadmap text.
- Default commit scope after accepted closeout: the three approved C++ paths plus Main-owned project documentation. All authored `Content/**`, including `BP_Enemy_Goblin` and any Physics Asset adjustments, remain user WIP and excluded unless the user separately approves a stable asset closure and its LFS pointer verification.

## Closeout Record — 2026-08-26

- **Delivered surface:** `AEnemyCharacter` now captures one finite attacker-away velocity-change vector from the current lethal Health GameplayEffect Context before writing Dead, consumes that value before every ragdoll early return, and applies it once only to an explicitly authored simulated Physics Asset body after ragdoll physics starts. It adds no new Tag, death Ability, damage route, Root Motion route, corpse lifecycle, or Physics Asset source-file change.
- **Authoring result:** the Native fallback is `2000 / 500 cm/s`; user Editor readback confirms `BP_Enemy_Goblin` currently enables ragdoll, selects `Pelvis`, and explicitly uses `20.0 / 5.0 m/s`. Those Blueprint/Physics Asset changes remain user-owned `Content/**` WIP and are excluded from this commit.
- **Main delta Fresh Review:** no P0/P1/P2 remains after the new Automation suite was hardened to assert its fail-closed setup rather than silently skipping it. `NAME_None` remains a silent no-force route; a non-None invalid/non-simulated bone retains ordinary ragdoll and logs once. The test-only observation seam remains under `WITH_DEV_AUTOMATION_TESTS`.
- **Static evidence:** CodeGraph/direct source review confirmed the current-context capture and one-time consumption chain; Rider error-only inspection found zero errors; `git diff --check` passed. Warning-level style suggestions are not behavior findings. The code-review graph index was older than the review baseline, so direct source/diff review remained the primary evidence.
- **User evidence:** the user confirmed focused `Scene01` PIE and all nineteen named Editor Automation suites, including `PolyQuest.Enemy.DeathRagdoll`, passed. This record does not invent a separate build-log result beyond that user-provided runtime evidence.
- **Documentation/commit boundary:** `ARCHITECTURE.md` and `ROADMAP.md` now record the stable implemented ownership and completion. `README.md` remains unchanged because this internal combat-presentation slice does not alter the public project overview. The scoped commit includes only the three approved C++ paths and these three Main-owned documents.
