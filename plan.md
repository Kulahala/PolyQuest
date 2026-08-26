# TODO-02C3J: Launch Facing Smoothing v1

## Plan State

- Status: Completed closeout. This document replaces the completed C3I plan and remains the most-recent-stage record until a later accepted plan replaces it. Main Fresh Review found one Public-header/Private-header boundary P2; the narrow header-path repair passed Main delta review.
- Baseline: `0139372` (`[Feature] 完成击飞受击朝向所有权 (Complete Launch Reaction Facing Ownership)`) plus Main-owned, uncommitted `ROADMAP.md` scheduling `TODO-02C3J` before `TODO-03C`.
- Objective: replace C3I's one-frame Player/Enemy Launch-facing snap with a short, fixed-rate pre-launch turn. The victim must still face the attacker and travel away from it, but the actor rotates visibly during Takeoff and only leaves the ground once the exact target yaw is reached.
- User decisions: begin turning during Takeoff; default Player/Enemy rate is `1440.0 degrees/second`; if the authored Commit Notify arrives before the turn completes, pause Takeoff, finish the remaining ground turn at that same rate, then issue the existing launch once. A 180 degree residual turn is therefore at most approximately `0.125s`.
- Source/asset evidence: both authored Launch GAs reference `AM_LaunchTakeoff`; its source sequence is `A_KnockDown_Begin_RootMotion_Sword`, whose current on-disk metadata reported `bEnableRootMotion=False` and `SequenceLength=0.700000`. This remained static asset evidence; the user subsequently confirmed the focused PIE route.
- Player-facing success: side/back impacts show a smooth, finite Takeoff turn rather than a yaw snap; every valid Launch still faces the attacker exactly at departure, moves away with the unchanged frozen velocity, holds the existing air pose, lands/recoveries normally, and never rotates again after becoming airborne.
- Preserve all user WIP. `.gitignore`, `Config/**`, `Content/**`, `.uproject`, maps, Blueprints, AnimBPs, Gameplay Ability/Effect/Montage assets, imported resources, generated folders, and unrelated source are not implementation or commit candidates.

## Route And Delegation

Outer: `ue-stage-workflow`
Primary: `ue5-cpp-gameplay`
Support: `ue5-debug-validation`
Route reason: C3J changes a shared native GAS temporal lifecycle. The active Launch Ability must retain yaw and launch ownership while the existing Player/Enemy arbitration paths continue to suppress competing writers.

Plan explorers: 0
Implementation executors: 1 (Gemini, completed the accepted source/test slice; documentation, staging, and commit ownership remained with Main)
Complex Executor: one scoped lifecycle-sensitive implementation
Main parallel work: none
Reason: the frozen snapshot, asynchronous yaw task, Commit arbitration, and exactly-once `LaunchCharacter` call are one atomic Player/Enemy contract. Splitting writers would make cancellation and callback ordering harder to verify.

## Frozen Runtime Contract

### Scope and non-goals

1. C3J applies only to `UPlayerLaunchReactionAbility` and `UEnemyLaunchReactionAbility`, through one narrow internal smoothing state and one AbilityTask. It is a user-priority presentation refinement after passing C3I validation, not a C3I defect repair.
2. Do not change hit classification, `FHitReactionImpactResolver`, damage, Poise, launch horizontal/vertical speed, landing recovery, Player Dodge cancellation, Enemy deferred Stance Break, Tags, Input, StateTree, AI target ownership, Controller code, Character code, Motion Warping, network behavior, Config, or authored assets.
3. `ImpactReferenceYawSnapshot` remains the canonical frozen pre-turn yaw and direction-conversion basis. Target yaw and launch velocity are calculated exactly once from that snapshot plus C3I's local `Target -> Attacker` direction. No later Actor rotation, target position, Lock-On state, AI Focus, or controller direction may recompute either value.
4. `CharacterMovement::RotationRate`, Player locomotion rotation, Controller yaw, and Motion Warping are not C3J turn owners. Player `State.Action.HitReacting` suppression and the existing active Enemy Launch-Spec controller gate already prevent their normal writers while the Ability is active; do not alter them.
5. Root Motion has higher priority. The new task must never write yaw while `ACharacter::HasAnyRootMotion()` is true, and `CommitFrozenLaunch()` must also reject active Root Motion. Treat that as a fail-closed task failure: end the Launch through the existing cancellation path with no `LaunchCharacter` call, rather than waiting indefinitely or fighting the animation. The current selected Takeoff sequence is explicitly non-Root-Motion.

### Internal frozen-state contract

Contract owner: Main. Implementation writer: Gemini.

- Add non-reflected `FLaunchFacingSmoothingState` under `Public/AbilitySystem/Abilities/` because the two Public Launch Ability headers hold it by value. It remains a C++-only internal lifecycle primitive, not a general turn framework or a Blueprint/reflection surface.
- It owns only `StartYaw`, `TargetYaw`, `LaunchVelocity`, `bHasFrozenLaunch`, `bCommitReceived`, `bTurnCompleted`, and `bLaunchIssued`.
- `TryFreeze(...)` resets first, delegates the mathematics to the unchanged `FHitReactionImpactResolver::TryBuildLaunchFacingAndVelocity`, and stores the successful target yaw/velocity. Invalid input leaves the state reset and returns false.
- `TryReceiveCommit()` accepts only the first Commit on a valid frozen state. `MarkTurnCompleted()` records one valid completion. `TryConsumeLaunchVelocity(OutVelocity)` succeeds only after both accepted Commit and turn completion, copies the frozen velocity, marks `bLaunchIssued`, and can never succeed again. Every failure resets the out parameter to `FVector::ZeroVector`.
- `Reset()` is idempotent and is called at every activation start and every `EndAbility()` exit. The helper neither accesses Actors nor calls `LaunchCharacter`.

### Facing AbilityTask contract

Contract owner: Main. Implementation writer: Gemini.

- Add `UAbilityTask_TurnToFacing` under `AbilitySystem/Tasks` as a C++-only, Ability-owned tick task. Its narrow factory receives the owning Ability, `ACharacter`, frozen start yaw, frozen target yaw, and positive degrees-per-second rate. Declare native `DECLARE_MULTICAST_DELEGATE` completion/failure delegates and bind the two Launch Abilities with `AddUObject`; do not use dynamic delegates, `BlueprintAssignable`, `UFUNCTION` callback plumbing, or any other reflected callback surface.
- Validate Actor, finite Start/Target Yaw, positive finite rate, and finite Tick delta. The task uses `FMath::FixedTurn` at the supplied rate, preserves the Actor's current Pitch/Roll on every write, and uses `FMath::Abs(FMath::FindDeltaAngleDegrees(CurrentYaw, TargetYaw))` for every tolerance/final-step decision so `+180/-180` follows the correct shortest arc. It snaps only the final sub-tolerance remainder to the exact frozen target yaw, then broadcasts completion once and ends.
- If the target is already within the existing `0.01 degree` threshold, complete synchronously after setting the exact target yaw. Root Motion, destruction, invalid input, or invalid Tick state broadcast failure once and end without any further transform write. `OnDestroy()` only releases state; it must not synthesize success/failure after the owning Ability has cancelled it.
- The task must set `bTickingTask = true`; it must use no TimerManager, component Tick, Character Tick, global delegate, GameplayCue, Tag, or asset reference.

### Player and Enemy Launch lifecycle contract

Contract owner: Main. Implementation writer: Gemini.

- In both Launch Ability headers add a private `EditDefaultsOnly` `FacingTurnRateDegreesPerSecond = 1440.0f`, clamped non-negative with a concise Chinese Tooltip stating that it is the Takeoff pre-launch turn rate. It is the only new authorable field; it is inherited by the current data-only GAs, so no `.uasset` edit is part of this stage.
- Add only private transient/internal members: `FacingTurnTask`, `FLaunchFacingSmoothingState`, and the `TurningToLaunch` phase needed between `Takeoff` and `AwaitingAirborne`. No Blueprint callable function, Config/Tag field, public production test hook, or data asset schema is allowed.
- In `ActivateAbility`, preserve C3I validation, event listener setup, Montage task creation, Cost/Commit, and existing cancellation ordering. After the bound Character, Takeoff Montage, phase, and C3I impact snapshots are set, call `TryFreeze(...)` before `MontageTask->ReadyForActivation()`. After Takeoff is confirmed active and prior action/Root Motion owners are cancelled, create/bind/activate `FacingTurnTask`; the frozen state accepts a zero-time Commit Notify until that task reports turn completion.
- If freezing fails, do not create a turn task and preserve the current C3I malformed-context behavior: Takeoff may begin, but the first valid Commit logs/ends fail-closed with no transform or launch. If task construction or later task execution fails, use the existing `EndFromMontage(true)` route; do not invent a new cleanup path.
- During normal Takeoff, the task turns toward the frozen target. Completion before Commit only records `bTurnCompleted`; it does not launch early.
- `OnLaunchCommitEventReceived` must first retain all C3I identity/active-Montage checks. It then accepts exactly one frozen-state Commit, switches to `TurningToLaunch`, and pauses `TakeoffMontage`. Immediately afterward it must call `CommitFrozenLaunch()` when `SmoothingState.IsTurnCompleted()` is already true, covering the valid `TurnCompleted -> Commit` order. It must not directly call `SetActorRotation` or `LaunchCharacter`.
- Add one private `CommitFrozenLaunch()` in each Ability. It runs only in `TurningToLaunch`, checks the existing Avatar/ASC/Movement/Montage state plus no Root Motion, asks the smoothing state to consume the velocity, marks `AwaitingAirborne`, then performs the single existing `LaunchCharacter(LaunchVelocity, true, true)` call and preserves all existing falling-grace handling. A duplicate Commit, duplicate task completion, failed state consumption, cancellation, death, or interrupted Montage must never invoke it twice.
- `OnFacingTurnCompleted` clears its task pointer, marks the shared state complete, and calls `CommitFrozenLaunch()` only if Commit was already received. `OnFacingTurnFailed` clears its task pointer and converges through `EndFromMontage(true)`.
- Update Takeoff Montage-end handling so uncommitted Takeoff and unexpected Montage termination during `TurningToLaunch` both end through the current route. In both `OnMovementModeChanged` implementations, treat `TurningToLaunch` exactly like `Takeoff`: any `IsFalling()` result before `bLaunchIssued` is an unexpected pre-launch fall and must call `EndFromMontage(true)` without issuing a launch. Keep existing LandingRecovery completion behavior unchanged.
- `EndAbility()` ends any live facing task before clearing the shared state, then retains all current delegate removal, montage shutdown, ledge restoration, Player Dodge cleanup, and Enemy Stance Break deferral disposition. No callback after teardown may mutate Actor rotation or issue a launch.

## Approved Paths, Execution Order, And Stop Conditions

### Approved implementation paths

1. `Source/PolyQuest/Public/AbilitySystem/Abilities/LaunchFacingSmoothingState.h`
2. `Source/PolyQuest/Private/AbilitySystem/Abilities/LaunchFacingSmoothingState.cpp`
3. `Source/PolyQuest/Public/AbilitySystem/Tasks/AbilityTask_TurnToFacing.h`
4. `Source/PolyQuest/Private/AbilitySystem/Tasks/AbilityTask_TurnToFacing.cpp`
5. `Source/PolyQuest/Public/AbilitySystem/Abilities/PlayerLaunchReactionAbility.h`
6. `Source/PolyQuest/Private/AbilitySystem/Abilities/PlayerLaunchReactionAbility.cpp`
7. `Source/PolyQuest/Public/AbilitySystem/Abilities/EnemyLaunchReactionAbility.h`
8. `Source/PolyQuest/Private/AbilitySystem/Abilities/EnemyLaunchReactionAbility.cpp`
9. `Source/PolyQuest/Private/Tests/TestLaunchFacingSmoothingAbility.h`
10. `Source/PolyQuest/Private/Tests/TestLaunchFacingSmoothingAbility.cpp`
11. `Source/PolyQuest/Private/Tests/LaunchFacingSmoothingAutomationTests.cpp`
12. `Source/PolyQuest/Private/Tests/HitReactionAutomationTests.cpp`

`ROADMAP.md`, `plan.md`, `ARCHITECTURE.md`, `README.md`, all Config, all assets, Build.cs, `EnemyAIController`, all Character classes, `HitReactionImpactResolver`, StateTree tasks/assets, damage delivery, staging, and commits are prohibited Gemini targets. `UAbilityTask_TurnToFacing` is the sole accepted new native type; it must remain C++-only and use no reflected Blueprint callable API. Any need for a new Gameplay Tag, Config field, asset edit, Character/Controller modification, resolver alteration, public Blueprint/API surface, generic rotation framework, test-only production seam, or changed Root Motion policy is a stop condition requiring Main evidence and a new scope decision.

### Execution order

1. Gemini reviews this plan read-only against the current C3I Ability lifecycle, actual `AM_LaunchTakeoff`/source-sequence metadata, `ACharacter::HasAnyRootMotion()` availability, existing AbilityTask conventions, and the proposed Automation fixture. It returns P0-P2 findings, blocking gaps, non-blocking recommendations, and Automation feasibility without editing, compiling, Editor access, staging, or committing.
2. After review acceptance, Gemini implements the one indivisible source/test slice. It must not tune or save Launch GAs/Montages/AnimBPs, and it must not alter C3I's controller gate or resolver.
3. Gemini runs Rider diagnostics across every touched C++ file, reads the final diff and direct caller/callee boundaries, and runs `git diff --check`. It returns changed paths, static evidence, unrun user gates, strict self-review findings, and remaining risks.
4. The user performs Editor readback, manual Development Editor compilation, full Automation, and focused `Scene01` PIE. Main then performs the required defect-first Fresh Review before documentation closeout and any explicit commit approval.

## Validation Matrix

### Native Automation

Add `PolyQuest.Combat.LaunchFacingSmoothing` without Content-dependent Montage/Blueprint fixtures and without production test seams.

- `FLaunchFacingSmoothingState`: valid cardinal/vector-derived freezes preserve StartYaw, TargetYaw, and attacker-away velocity; invalid direction/Yaw/speed leaves it reset; completion before Commit and Commit before completion cannot consume velocity; after both events one consume returns the exact frozen velocity; repeated Commit, completion, or consume cannot issue another launch token; `Reset()` restores the initial state.
- `UTestLaunchFacingSmoothingAbility`: a LocalOnly, InstancedPerActor test host drives a real ASC-owned `UAbilityTask_TurnToFacing` against a fixture Player. World ticks prove `0 -> 180` progresses at `1440 degrees/second` (for example, `72 degrees` after `0.05s`), reaches exact `180` after the remaining duration, broadcasts completion once, and never moves again on later ticks. The same matrix covers the `+180/-180` shortest-arc boundary. A mid-turn ability end proves no later transform write or completion. Zero/negative/NaN rate and NaN target yaw prove one failure and no actor yaw mutation.
- The test host may use `FLaunchFacingSmoothingState` to verify completion/Commit ordering and the exactly-once consume protocol, but it must not imitate or introduce a second Player/Enemy launch implementation. Production `LaunchCharacter` integration remains covered by source review and user PIE because the native automation fixture intentionally has no authored AnimInstance/Montage route.
- Extend `PolyQuest.Combat.HitReaction` CDO checks to assert Player and Enemy default `FacingTurnRateDegreesPerSecond` are finite positive `1440.0f`. Retain all C3I Resolver/fail-closed tests unchanged.
- Run `PolyQuest.Enemy.RootMotionFacing` unchanged as mandatory regression proof: the existing active Enemy Launch-Spec gate must preserve yaw/Focus while C3J's early turn is active and resume normal target-facing after the Spec ends.

### Static gate

- Use CodeGraph for both Launch Ability activation/Commit/End paths, the new task, and `UpdateControlRotation`; use code-review-graph only as supplemental diff-impact evidence when its index matches the review baseline. Otherwise record direct source/diff review as primary evidence.
- Run Rider `get_file_problems` or `lint_files` on every added/modified C++ path; inspect final headers/includes and all direct callbacks; run `git diff --check`.
- Confirm no Config/Tag/Input/DataAsset/API/module/asset/Controller/Character/Resolver modification, no dynamic/reflected Task delegate surface, and no `LaunchCharacter` call outside the new per-Ability `CommitFrozenLaunch()` helper.

### User-owned Editor and runtime gate

1. Compile `PolyQuestEditor (Development Editor)` and report the exact result.
2. Run the complete Editor Automation matrix, with `PolyQuest.Combat.LaunchFacingSmoothing`, `PolyQuest.Combat.HitReaction`, and `PolyQuest.Enemy.RootMotionFacing` mandatory.
3. Read back both current Launch GAs: confirm they inherit `FacingTurnRateDegreesPerSecond = 1440.0`, retain existing `LaunchHorizontalSpeed = 800`, retain their current Landing Montage, and do not gain shadow Blueprint variables. Read back `AM_LaunchTakeoff`: confirm `Reaction Launch Commit` remains the departure Notify and its selected Sequence does not enable Root Motion. Do not edit an asset as part of this check.
4. In `Scene01`, trigger Player and Enemy Launch from front, back, left, and right. Confirm Takeoff begins turning immediately; when Commit arrives early, the paused montage holds while residual yaw completes; departure happens once at exact attacker-facing yaw; velocity remains attacker-away; airborne yaw does not fight; landing recovery, Player Dodge cancellation, Enemy Focus/AI resumption, and Enemy Stance Break deferral remain correct.
5. If a Montage writes yaw during the turn or flight, or if active Root Motion is observed, stop and return the exact Editor asset/readback. Do not compensate by changing Controller, `RotationRate`, Motion Warping, or source timing inside this stage.

## Debt Handoff And Commit Boundary

- C3J creates no accepted follow-up by default. A future selected Root-Motion Launch asset or a request for eased/curve-driven turn profiles requires a separate stage; it must not weaken the current fixed-rate, Ability-owned, fail-closed ownership rule.
- After accepted validation and Main Fresh Review, `ROADMAP.md`, `ARCHITECTURE.md`, and `README.md` are synchronized only with implemented stable behavior and recorded evidence. Retain this C3J plan/closeout record until the next accepted stage replaces it.
- One scoped commit may include only the twelve approved source/test paths and Main-owned documentation closeout paths. It excludes all current `.gitignore`, Config, Content, `.uproject`, map, Blueprint, AnimBP, GA/GE, Montage, Physics Asset, imported-resource, and unrelated WIP.

## Closeout Record

- Implementation: Player and Enemy Launch now share the non-reflected `FLaunchFacingSmoothingState` and C++-only `UAbilityTask_TurnToFacing`. The frozen launch velocity is consumed once only after both the authored Commit event and the finite pre-launch turn have completed; Root Motion, invalid task state, cancellation, and premature Falling fail closed through the existing Ability cleanup.
- Boundary repair: the state header moved from `Private/` to `Public/` and gained `POLYQUEST_API`, because both exported Public Ability headers store it by value. No Blueprint/reflection API, Tag, Config, asset, Controller, Character, Resolver, or gameplay-contract expansion was introduced.
- Main review: the initial defect-first Fresh Review found the Public-header/Private-header P2. After the repair, Main delta review found no P0-P3 code defect. Rider reported zero errors, and `git diff --check` passed.
- User validation: the user confirmed `PolyQuest.Combat.LaunchFacingSmoothing`, `PolyQuest.Combat.HitReaction`, and the complete combat/reaction regression matrix passed, plus focused Scene01 PIE covering smooth turn, physical launch, and landing recovery. This record does not claim a separately logged Development Editor build result.
- Debt handoff: no new accepted debt. A future Root-Motion Launch asset or eased/curve-driven turn profile remains a separate stage and must preserve the fixed-rate Ability-owned fail-closed ownership contract.
