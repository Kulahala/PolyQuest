# TODO-03H11: Enemy Small Small Knockback

> Active-slice handoff. This document replaces the completed 03H10 handoff; the completed 03H10 evidence remains in `ROADMAP-archive.md`.

## 1. Objective, Baseline, And Route

Add a short, optional grounded knockback to Enemy Small Hit Reaction. The free-space tuning target is 20 cm over 0.10 s under the continuous falloff model. CMC remains the sole owner of collision, ground, slope, ledge, and final movement resolution.

- Repository: `E:\GameDevelop\PolyQuest`
- Start HEAD: `e483cca`
- Engine baseline: `D:\UE\UE_5.8`
- At planning start, the approved source/test paths and stage documents had no uncommitted changes. Existing `Content/**`, Config, deleted/imported assets, and other user WIP remain excluded.

```text
Outer: ue-stage-workflow
Primary: ue5-cpp-gameplay
Support: ponytail:ponytail (lite)
Route reason: extend one existing GAS Ability with a native CMC root-motion task and focused Automation coverage
Implementation executor: one in-app gpt-5.6-luna / xhigh sub-agent, explicitly requested by the user
Executor route: Luna may modify only the three source/test paths below; Main owns this plan, integration, documentation, static checks, Fresh Review, and commit gate
User: CurveFloat authoring/readback, Development Editor compilation, Automation execution, Scene01 PIE, and final commit approval
```

Luna must not start the Editor, write assets, modify documents or Config, expand scope, create a second agent, or commit. Main must not stage or commit without later explicit user approval.

## 2. Objective And Non-goals

### Target objective

- Enemy Small keeps its existing ServerOnly, retriggerable, four-way Overlay Montage, no movement-lock, and no attack-cancel contract.
- With valid configuration and no competing root motion, an active grounded Enemy Small reaction moves away from the attacker through one native RMS only.
- Zero configured distance explicitly disables the displacement while retaining the Small Montage.

### Deliberate non-goals

- Do not modify Player Small, Big, Launch, Victim Execution, `FHitReactionImpactResolver`, `AEnemyAIController`, tags, Config, `.Build.cs`, network behavior, Motion Warping, or a shared movement framework.
- Do not edit or stage `GA_EnemySmallHitReaction.uasset`, any Montage, map, Blueprint, or other binary asset.
- Do not fall back to `LaunchCharacter`, per-frame Actor or Mesh movement, or a second action-state authority.

## 3. Approved Paths

### 3.1 Luna source and test whitelist

- `Source/PolyQuest/Public/AbilitySystem/Abilities/EnemySmallHitReactionAbility.h`
- `Source/PolyQuest/Private/AbilitySystem/Abilities/EnemySmallHitReactionAbility.cpp`
- `Source/PolyQuest/Private/Tests/HitReactionAutomationTests.cpp`

Allowed production symbols are `UEnemySmallHitReactionAbility::ActivateAbility()`, `EndAbility()`, and its private reflected configuration/runtime fields. Test-only additions must remain behind `WITH_DEV_AUTOMATION_TESTS`.

### 3.2 Main documentation whitelist

- `plan.md` now records this active slice.
- `ARCHITECTURE.md`, `ROADMAP.md`, and `ROADMAP-archive.md` are writable only after every applicable validation and Fresh Review gate has evidence.

### 3.3 User-owned Editor paths

- Create `/Game/_Abilities/Enemy/HitReaction/Curve_EnemySmallHitKnockbackFalloff` only in Unreal Editor.
- Set the resulting CurveFloat on the existing user-owned `/Game/_Abilities/Enemy/HitReaction/GA_EnemySmallHitReaction` only after the user performs or explicitly authorizes the Editor write.

No other path is approved.

## 4. Frozen Runtime Contract

### 4.1 Configuration and direction

- Add private `EditDefaultsOnly` settings to the existing Ability: `KnockbackDistance` default `20.0f`, `KnockbackDuration` default `0.10f`, and `KnockbackFalloffCurve` as `TObjectPtr<UCurveFloat>`.
- The CurveFloat is a fixed v1 linear contract: normalized keys `(0, 1)` and `(1, 0)`, both linear. Its continuous area is `0.5`; the source strength is `2 * KnockbackDistance / KnockbackDuration`, or `400 cm/s` with the defaults. `FRootMotionSource_ConstantForce` samples the curve once per CMC step, so the actual free-space displacement is frame-step dependent and 20 cm is a tuning target rather than a strict runtime equality. Non-linear tuning is a separate stage because it changes the displacement profile.
- Immediately after `ResolveImpactDirection()` returns the target-local `Target -> Attacker` vector, snapshot the Enemy yaw and resolve the reversed horizontal world direction into a local `FVector`. Do this before Montage task creation, `ReadyForActivation()`, Controller updates, or any later task creation. A zero/non-finite result remains zero and only disables knockback.
- Distance `<= KINDA_SMALL_NUMBER` is an intentional no-knockback configuration. Negative/non-finite distance, non-finite or non-positive duration, missing curve, invalid source direction, missing CMC, or non-grounded movement fail closed for displacement only; they do not stop the selected Montage.

### 4.2 Activation ordering and root-motion ownership

- Preserve the current Montage/Commit/`ReadyForActivation()` lifecycle and its reentry checks. Only after the Montage is confirmed active evaluate the optional knockback branch.
- Require `MovementComponent->IsMovingOnGround()`, `!EnemyCharacter->IsPlayingRootMotion()`, and `!EnemyCharacter->HasAnyRootMotion()` before creating the force task. In UE 5.8 the latter checks CMC RootMotionSources, which a playing animation may register; retain the explicit `IsPlayingRootMotion()` check instead of relying on implementation timing.
- The final action in the valid optional branch is `UAbilityTask_ApplyRootMotionConstantForce::ApplyRootMotionConstantForce()`. Its factory immediately applies RMS, so no later prerequisite may precede it.
- Use the pre-snapshotted direction, `bIsAdditive=false`, `bEnableGravity=true`, the linear CurveFloat, `ERootMotionFinishVelocityMode::SetVelocity`, `FVector::ZeroVector`, and no clamp. Do not bind root-force completion to `EndAbility()`; the existing Montage retains Ability lifetime ownership.
- Add one transient `TObjectPtr<UAbilityTask_ApplyRootMotionConstantForce>` owned by this Ability. At retrigger start and in the sole `EndAbility()` cleanup outlet, call its `EndTask()` and clear it before moving on. Never clear `CurrentRootMotion`, remove an ID not owned by this task, or call `StopMovementImmediately()`.
- Big, Launch, and Victim Execution already cancel `Ability.Reaction.Enemy.Small`; their existing cancellation must reach the same `EndAbility()` cleanup. Existing Controller root-motion focus suppression/recovery remains untouched.

### 4.3 Automation-only injection boundary

- Under `WITH_DEV_AUTOMATION_TESTS`, add the smallest required accessors: `SetTestKnockbackConfig(float Distance, float Duration, UCurveFloat* Curve)` and a read-only active root-force Task accessor if needed to prove retrigger ownership.
- Production code never creates a fallback curve. Tests must create a transient `UCurveFloat` with `NewObject<UCurveFloat>(GetTransientPackage())` and inject the two linear keys, keeping CI independent from the user-authored asset.

## 5. Automation And Static Delivery

Extend only `PolyQuest.Combat.HitReaction`, reusing its existing world/fixture patterns rather than adding a test framework or test file.

- Valid grounded activation: verify the pre-snapshotted reversed horizontal direction, nominal `20 cm / 0.10 s` configuration under the transient linear curve, active owned RMS, a controlled `0.05 s` CMC-tick displacement within `20-35 cm`, and zero horizontal velocity when the source finishes. Do not assert an exact displacement across arbitrary CMC frame steps.
- Skip paths: zero distance, invalid/missing CurveFloat configuration, zero/invalid impact direction, airborne movement, and existing RMS. In every skip path, the Small Montage contract remains intact and no owned RMS is added.
- Lifecycle: a retrigger ends only the prior owned source; a stale old task cannot remove the new source. Completion, cancellation, Big/Launch/Victim takeover, and teardown remove only the owned source while a deliberately injected foreign RMS remains.
- The executor must read final approved diffs, run `git diff --check` on the approved scope, and report a strict implementation self-review. Use Rider diagnostics only if available; do not claim compile, Automation, Editor, PIE, or visual evidence without actually receiving it.

## 6. User Validation, Review, And Closure

User gates:

- Development Editor compilation.
- `PolyQuest.Combat.HitReaction`, `PolyQuest.Enemy.RootMotionFacing`, `PolyQuest.Enemy.CombatTargetRetention`, and `PolyQuest.Combat.EnemyLaunchReactionRootMotion` Automation suites.
- Editor readback: Enemy Small GA parent and four Montage references remain correct; distance `20.0`, duration `0.10`, curve assignment, and two linear CurveFloat keys are present.
- Scene01 PIE: four directions, repeated hits, zero distance, navigation/attack recovery, existing montage/RMS conflict, walls, slopes, ledges, Big/Launch/Victim/death takeover, and HitStop. Confirm no residual slide and immediate AI pursuit recovery after root motion ends.

Main closure:

- Perform one bounded `ue-strict-review` Fresh Review of the approved diff after the executor handoff and applicable evidence.
- Record only verified stable facts in `ARCHITECTURE.md`; move 03H11 in `ROADMAP.md` and append `ROADMAP-archive.md` only after all gates pass. Any missing asset/readback/compile/PIE proof becomes one explicit `ROADMAP.md` debt with a closure trigger.
- For one root cause, allow at most one evidence-based repair and one targeted rerun. On recurrence, preserve the first evidence and stop the loop for user direction.

Ponytail lite: a constant `200 cm/s` force with no CurveFloat would be shorter, but it violates the accepted configurable linear-falloff contract. Do not add a runtime UObject fallback or a generic knockback system.

## 7. Closure Record (2026-09-14)

- Implemented only the three approved C++/test paths. The final Native contract is the optional Enemy Small `ConstantForce` route described above; no Player Small, Big, Launch, Victim, Controller, Tag, Config, Build.cs, or user-authored asset was changed.
- User-confirmed evidence: Development Editor compilation and `Success` for `PolyQuest.Combat.HitReaction`, `PolyQuest.Enemy.RootMotionFacing`, `PolyQuest.Enemy.CombatTargetRetention`, and `PolyQuest.Combat.EnemyLaunchReactionRootMotion`.
- Main completed one bounded `ue-strict-review` Fresh Review after the repair loop; the independent read-only reviewer found no P0-P2 blocker. Main static `git diff --check` passed. These are separate compile, Automation, review, and static evidence types.
- Deferred evidence is tracked only as `Debt-03H11-AuthoredValidation` in `ROADMAP.md`: itemized Enemy Small asset readback and the full Scene01 PIE matrix remain unverified. User authorized documentation closeout and this bounded source/test/document commit with that debt retained; this record does not claim authored or visual closure.
