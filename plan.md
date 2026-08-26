# TODO-02C3I: Launch Reaction Facing Ownership And Presentation v1

## Plan State

- Status: Completed. Gemini implemented the accepted ten-file source/test slice; the user confirmed the complete Editor Automation matrix and focused `Scene01` PIE, and Main's defect-first Fresh Review found no P0-P2.
- Baseline: `b8f99ae` (`[Docs] 同步 H4B 完成状态 (Synchronize H4B Completion State)`) plus the current Main-owned uncommitted `ROADMAP.md` entry that inserts `TODO-02C3I` before `TODO-03C`.
- Objective: correct the existing Player/Enemy Launch presentation so a victim faces the valid attacker at `Event.Reaction.Launch.Commit` while `CharacterMovement` continues launching the victim away from that attacker. The behavior is a one-frame reaction-facing ownership handoff, not a new knockback mechanic.
- Player-facing success: when struck from front, back, left, or right, Player and Enemy face the attacker and travel backward relative to that new facing. Launch range, horizontal/vertical speed, damage, Poise, landing recovery, input blocks, and existing cancellation semantics remain unchanged.
- Preserve all user WIP. `.gitignore`, `Config/**`, `Content/**`, `.uproject`, maps, Blueprints, AnimBPs, Gameplay Ability/Effect/Montage assets, imported resources, generated folders, and unrelated source are not implementation or commit candidates.

## Route And Delegation

Outer: `ue-stage-workflow`
Primary: `ue5-cpp-gameplay`
Support: `ue5-state-tree-ai`, `ue5-debug-validation`
Route reason: this is a shared native GAS Launch lifecycle and AI Controller yaw-arbitration correction. Animation assets supply the visual pose, but native code owns the authoritative one-time facing and world launch vector.

Plan explorers: 0
Implementation executors: 1 (Gemini, only after its plan review is accepted and the user explicitly authorizes execution)
Complex Executor: one scoped lifecycle-sensitive implementation
Main parallel work: none
Reason: the resolver snapshot, two Ability Commit paths, and Enemy Controller gate are one atomic behavior contract; multiple writers would increase lifecycle drift risk.

## Frozen Runtime Contract

### Scope and non-goals

1. This stage applies only to `UPlayerLaunchReactionAbility` and `UEnemyLaunchReactionAbility`. `Small`, `Big`, `Death`, ragdoll, eight-direction reaction selection, Motion Warping, mesh-only offsets, continuous attacker tracking, new Tags, DataAsset fields, input routes, and StateTree edits are prohibited.
2. The selected visual timing is an instantaneous one-time Actor Yaw snap at the accepted `Event.Reaction.Launch.Commit`, not an interpolated Takeoff turn, Tick, timer, curve, or authorable duration.
3. The existing finite target-local `Target -> Attacker` direction stays canonical. Its World interpretation must use a frozen pre-turn Actor Yaw; no code may derive launch velocity from Actor rotation after the snap, Lock-On state, AI Focus, or a later target location.
4. This remains single-player/server-only behavior. It adds no replication, RPC, GameplayCue, or networking/prediction contract.

### Shared resolver contract

Contract owner: Main. Implementation writer: Gemini.

- In `HitReactionImpactResolver.h/.cpp`, add one non-reflected pure static API:

  ```cpp
  static bool TryBuildLaunchFacingAndVelocity(
      const FVector& LocalAttackerDirection,
      float ImpactReferenceYaw,
      float HorizontalSpeed,
      float VerticalSpeed,
      float& OutFacingYaw,
      FVector& OutLaunchVelocity);
  ```

- It receives the existing local `Target -> Attacker` vector and a finite pre-turn target Yaw. On success, `OutFacingYaw` points at the attacker in world space and `OutLaunchVelocity` points away from the attacker with the supplied vertical component.
- On any zero, non-finite, or invalid input, it resets `OutFacingYaw` and `OutLaunchVelocity`, returns `false`, and emits no log. `ResolveImpactDirection*` remains unchanged. Preserve the public signature and behavior of `TryBuildLaunchVelocity`, but make it a thin wrapper that forwards to `TryBuildLaunchFacingAndVelocity` with a local ignored-facing output; no second normalization/rotation implementation may remain.

### Launch Ability lifecycle contract

Contract owner: Main. Implementation writer: Gemini.

- In both Launch Ability headers, add only one private non-reflected `float ImpactReferenceYawSnapshot`. Do not add Blueprint API, DataAsset fields, Tags, or generic test hooks.
- In each `ActivateAbility`, reset both snapshots first. After existing activation validation but before `MontageTask->ReadyForActivation()`, capture the current Actor Yaw and resolve `ImpactDirectionSnapshot` from the trigger event. Remove the current post-`ReadyForActivation()` direction resolution. This accommodates a zero-time Commit Notify without allowing montage-start failure to rotate the actor.
- In each `OnLaunchCommitEventReceived`, call `TryBuildLaunchFacingAndVelocity` before mutating phase, montage, movement, or actor rotation. On failure, retain the existing warning/end path and leave the Actor transform untouched.
- On success, retain existing Commit lifecycle: set Commit/phase, pause the active Takeoff Montage, preserve the Actor's current Pitch/Roll while writing only the resolved Yaw, then call the existing `LaunchCharacter` with the resolved velocity. In `EndAbility`, reset both snapshots again on every natural, cancellation, and setup-failure exit. Landing recovery, fall validation, cancellation, Dodge cancel, and Enemy deferred Stance Break ownership remain local and unchanged.

### Enemy Controller yaw contract

Contract owner: Main. Implementation writer: Gemini.

- `State.Action.HitReacting` is intentionally too broad for this stage: both Enemy Big and Enemy Launch own it. Add one **private, non-reflected** `EnemyLaunchReactionAbilityTag` initialized to `Ability.Reaction.Enemy.Launch` and one private `IsEnemyLaunchReactionActive() const` helper. It must query matching Ability Specs through `GetActivatableGameplayAbilitySpecsByAllMatchingTags(..., false)` and return true only when a matching non-null Spec is active; it must not infer active Launch from the shared reaction state tag.
- In `AEnemyAIController::UpdateControlRotation`, retain the existing Root Motion branch first. Immediately after it and before `bWasRootMotionActive` recovery, return only when `IsEnemyLaunchReactionActive()` is true.
- The new branch must not clear or set Focus, reset Root-Motion handoff fields, stop movement, change StateTree state, mutate targets, or alter `IsEnemyHitReactionActive()` and its existing StateTree/combat gating role. It simply prevents Controller yaw writes while active Enemy Launch owns the Actor's snapped Launch yaw.
- When that active Launch Spec ends, the existing normal or Root-Motion handoff logic resumes naturally. The required header change stays private and non-reflected: no Blueprint/public API, test-only production seam, new Gameplay Tag, Config, or asset is permitted.

## Approved Paths, Execution Order, And Stop Conditions

### Approved implementation paths

1. `Source/PolyQuest/Public/Combat/Reaction/HitReactionImpactResolver.h`
2. `Source/PolyQuest/Private/Combat/Reaction/HitReactionImpactResolver.cpp`
3. `Source/PolyQuest/Public/AbilitySystem/Abilities/PlayerLaunchReactionAbility.h`
4. `Source/PolyQuest/Private/AbilitySystem/Abilities/PlayerLaunchReactionAbility.cpp`
5. `Source/PolyQuest/Public/AbilitySystem/Abilities/EnemyLaunchReactionAbility.h`
6. `Source/PolyQuest/Private/AbilitySystem/Abilities/EnemyLaunchReactionAbility.cpp`
7. `Source/PolyQuest/Public/AI/EnemyAIController.h`
8. `Source/PolyQuest/Private/AI/EnemyAIController.cpp`
9. `Source/PolyQuest/Private/Tests/HitReactionAutomationTests.cpp`
10. `Source/PolyQuest/Private/Tests/EnemyRootMotionFacingAutomationTests.cpp`

`ROADMAP.md`, `plan.md`, `ARCHITECTURE.md`, `README.md`, all Config, all assets, Build.cs, StateTree tasks/assets, Character classes, hit classifiers, damage delivery, and staging/commits are prohibited Gemini targets. The sole `EnemyAIController.h` change is the frozen private non-reflected tag/member-helper declaration above; no public/reflected API is allowed. Any need for an otherwise unlisted public/reflected API, Tag, asset edit, Config change, test-only production seam, or lifecycle rule is a stop condition requiring Main source evidence and a new scope decision.

### Execution order

1. Gemini reviews this plan read-only against the exact paths, Resolver call chain, Launch Commit timing, Controller yaw path, and existing Automation fixtures. It returns P0-P2 findings, blocking gaps, non-blocking recommendations, and Automation feasibility without editing, compiling, Editor access, staging, or committing.
2. After acceptance, Gemini performs the single indivisible source/test slice. It must not broaden the stage to Big/Small reactions or authored assets.
3. Gemini runs Rider diagnostics on every touched C++ file and `git diff --check`, then returns changed paths, static evidence, unrun user gates, strict self-review findings, and remaining risks.
4. The user performs compile, Editor readback, focused Automation, and `Scene01` PIE. Main then performs the required defect-first Fresh Review before documentation closeout and any explicit commit approval.

## Validation Matrix

### Native Automation

Extend existing suites only; do not add production accessors or Content-dependent test assets.

- `PolyQuest.Combat.HitReaction`: extend the Resolver matrix for front/back/left/right attacker vectors at reference Yaw `0`, plus rotated `45`, `90`, and `180` degree reference cases. Every valid case must prove `OutFacingYaw` points to the attacker and `OutLaunchVelocity` points away using the same pre-turn reference Yaw. Compare angle results with `FMath::FindDeltaAngleDegrees`, including the `+180/-180` boundary. Zero/NaN/Inf direction, Yaw, and speed must fail closed with both outputs reset. Retain and exercise the forwarding `TryBuildLaunchVelocity` regressions.
- `PolyQuest.Enemy.RootMotionFacing`: with a possessed Enemy, valid target, and existing Gameplay Focus, first add only the existing `State.Action.HitReacting` loose tag, set a non-target Yaw, and prove ordinary Controller target-facing still occurs. This locks the exclusion of Enemy Big from the new gate. Then grant a real `UEnemyLaunchReactionAbility` Spec, set that test-local Spec's `ActiveCount` to one only within the fixture, and assert `Spec->IsActive()` before calling the existing production Controller update helper. While that exact active Launch Spec exists, assert Yaw and Focus remain untouched; reset `ActiveCount` to zero before cleanup, call the helper again, and assert normal target-facing resumes. This is a controlled Spec-state test of Controller arbitration, not a content-dependent Launch-Montage activation test; it adds no production seam. Existing Root Motion priority, focus clearing, and bounded post-Root-Motion recovery assertions must remain intact.

### Static gate

- Use CodeGraph for Resolver callers and the Controller branch, plus code-review-graph as supplemental diff-impact evidence when its index matches the review baseline; otherwise record direct source/diff review as the primary evidence.
- Run Rider `get_file_problems` or `lint_files` across all touched C++ files, inspect every final diff, and run `git diff --check`.
- Confirm no changes to Config/Tags, input, DataAsset schema, Blueprint/API exposure, module dependencies, or assets.

### User-owned Editor and runtime gate

1. Compile `PolyQuestEditor (Development Editor)` and report the exact result.
2. Run the complete Editor Automation matrix, with `PolyQuest.Combat.HitReaction` and `PolyQuest.Enemy.RootMotionFacing` mandatory.
3. In the Player and Enemy Launch GA defaults, inspect the assigned Takeoff/Landing Montages: actor forward axis must match the chosen backward-flight silhouette, and no active Root Yaw after Commit may overwrite the native snap. This is readback/authoring evidence, not a source test.
4. In `Scene01`, trigger Player and Enemy Launch from front/back/left/right. Verify one Commit-frame face-to-attacker snap, backward travel away from attacker, valid landing recovery, Player input/cancel behavior, Enemy AI yaw resumption, and no Stance Break regression. If a Montage still overwrites yaw after Commit, stop and return the asset readback; do not silently alter the asset or C++ contract.

## Execution, Validation, And Review Record

- Gemini changed exactly the ten approved source/test files. The resolver now derives launch-facing Yaw and attacker-away velocity from one frozen impact pair; both Launch Abilities capture it before `ReadyForActivation()`, use it only at Commit, and clear it through `EndAbility()`. The Enemy Controller yields yaw only to an active `Ability.Reaction.Enemy.Launch` Spec, preserving the existing Big-reaction and Root-Motion boundaries.
- Static evidence: the Main and executor diff reviews found the approved ten-file surface only; Rider error-only inspection reported zero errors across all touched C++ files; `git diff --check` passed with only normal CRLF conversion notices.
- User evidence: the pasted Editor log reports all twenty Automation suites as `Success`, including `PolyQuest.Combat.HitReaction` and `PolyQuest.Enemy.RootMotionFacing`; the user also confirmed focused `Scene01` PIE. No separate Development Editor build log is claimed in this closeout.
- Main Fresh Review found no P0-P2. The retained runtime risk is authored Takeoff/Landing Montage Root Yaw: the current PIE route is accepted, but any future Montage replacement must verify it does not overwrite the native Commit-facing write.

## Debt Handoff And Commit Boundary

- No unresolved blocker, failed validation, or accepted corrective follow-up remains from C3I. The optional smoothing idea is a Roadmap Recommendation with an explicit visual-adoption trigger, not current-stage debt and not a prerequisite for `TODO-03C`.
- One scoped commit may include only the ten approved source/test paths and Main-owned `plan.md`, `ROADMAP.md`, `ARCHITECTURE.md`, and `README.md` closeout changes. It excludes all existing `.gitignore`, Config, Content, `.uproject`, map, Blueprint, AnimBP, GA/GE, Montage, and imported-resource WIP.
