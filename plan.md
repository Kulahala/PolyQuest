# TODO-02B2: Locked Locomotion Facing And Death Retarget v1

## Plan State

- Status: Completed after user-confirmed Automation and PIE validation.
- Baseline: `a9ec82b` (`[Docs] 完成锁定阶段文档收尾 / Close Lock-On Stage Documentation`).
- Objective: extend the completed B1 local lock so grounded ordinary locomotion, Guard, and non-Root-Motion Parry continuously face the valid target; Sprint remains free-run; a confirmed locked-target death makes one deterministic clockwise replacement attempt.
- Accepted validation adjustment: the native Player `RotationRate.Yaw` default is `800 degrees/second`; B2 retains no new editable tuning field.
- Current `Content/**`, maps, authored Blueprint/UMG/input/GA/GE/Montage/AnimBP assets, Config/project files, and every other worktree change are user-owned WIP. Preserve them. The user confirmed `BP_Enemy_Goblin.WeaponMesh` with `Collision Enabled = No Collision`; native `ABaseCharacter::BeginPlay` remains the runtime enforcement for the fixed display mesh's `ECC_Camera = Ignore` contract.

~~~text
Outer: ue-stage-workflow
Primary: ue5-cpp-gameplay
Support: ue5-debug-validation
Route reason: B2 changes Player yaw ownership, Sprint exception behavior, and local lock lifecycle while preserving the existing fixed-camera and GAS contracts.
~~~

~~~text
Plan explorers: 0
Implementation executors: 0
Complex Executor: none
Main parallel work: none
Reason: Player rotation, lock lifecycle, Sprint state, public target query, and action ownership are Main-only integration territory under AGENTS.md.
~~~

## Locked Product Contract

1. B2 builds only on completed `TODO-02B1`. It retains one Player-owned local weak `AEnemyCharacter` lock, B1 strict-viewport eligibility, cursor acquisition, wheel cycling, existing Health-bar highlight, fixed oblique camera, camera-relative movement input, no replication, no new Gameplay Tag, and no generic targeting framework.
2. With a valid lock, Player Idle/Walk/Run and Guard continuously face the target's planar actor-relative direction. Parry also faces the target during its own `MOVE_None` lock only when it has no Root Motion. Rotation uses the native `CharacterMovement.RotationRate.Yaw = 800 degrees/second` default as a fixed maximum; B2 adds no new authorable speed property.
3. Sprint is the explicit free-run exception: while `State.Movement.Sprinting` is active, the target lock, highlight, and camera remain intact but `bOrientRotationToMovement` returns to the existing movement-facing behavior. When Sprint ends, B2 resumes lock-facing against the same target without reacquiring it.
4. Attack, Dodge, Small/Big/Launch HitReaction, active Bow aiming, Player dead/stunned state, and any `ACharacter::HasAnyRootMotion()` route own yaw. B2 neither calls `SetActorRotation` nor leaves movement-facing enabled in a way that can compete with those paths. After their existing cleanup returns Player to an eligible state, lock-facing resumes.
5. B2 caches the locked target's last valid screen candidate (screen position, clockwise angle, Player-screen distance, and stable Actor path key). Explicit acquisition/cycling and successful Tick validation update that cache. Tick validates only the current lock and never iterates other Enemies except one confirmed-death replacement attempt.
6. A current target is eligible for automatic replacement only when `AEnemyCharacter::IsDead()` is true. B2 builds one B1 candidate list, applies the existing clockwise comparator, and selects the first candidate strictly after the cached dead-target record, wrapping to the beginning. Tied angles retain the existing distance/path-key order. No candidate or no valid cached anchor clears the lock.
7. Destruction, missing ASC/team validity, invulnerability, projection failure, strict viewport exit, Player death, and explicit Middle Mouse clear still clear the lock without auto-retarget. B2 does not modify `AEnemyCharacter`, Bow, Projectile/Homing, Camera, Input, UI, Config, StateTree, or authored assets.

## Approved Native Surface

- Update `Source/PolyQuest/Public|Private/Character/Player/PlayerCharacter.*`.
  - Refine private lock validation into a disposition that distinguishes valid, death-retargeted, and cleared states; retain `GetLockedTarget()` as the existing C++-only read and add no Blueprint-facing gameplay API.
  - Cache the selected and later validated screen candidate; use a non-scanning helper to refresh that cache from the current target.
  - Extend Tick to validate the current lock, apply the legal death replacement once, synchronize `bOrientRotationToMovement`, and apply fixed-rate planar lock-facing only while eligible.
  - Preserve B1 one-shot action and Dodge-facing functions unchanged. Extend the existing rotation-mode helper so ordinary locked locomotion/Guard/eligible Parry uses lock-facing, Sprint stays movement-facing, and action/Root-Motion paths remain protected.
- Update `Source/PolyQuest/Public|Private/Character/Player/PlayerLockOnTargeting.*` with one pure C++ static successor lookup that accepts an already-cached candidate anchor and returns the next clockwise index using the existing comparator. It owns no World scan, Actor lifecycle, Tick, UI, Tag, or asset logic.
- Update `Source/PolyQuest/Private/Tests/PlayerLockOnAutomationTests.cpp` only. Reuse transient World fixtures and test-only seams; do not load or mutate authored assets.

## Validation Matrix

### Static and Automation

1. Extend `PolyQuest.Player.LockOn` with strict clockwise successor/wrap/tie tests; the `800 degrees/second` Idle fixed-rate turn; Guard and `MOVE_None` Parry facing; Sprint free-run then resume; Attack, Dodge, Small/Big/Launch reaction, Bow requester, and transient Root Motion yaw protection; death handoff/highlight transfer; no-candidate clear; and non-death no-retarget regressions.
2. Read final callers/callees, run Rider error-level inspections on touched C++, run `git diff --check`, and use code-review-graph only as stale-coverage supplemental evidence if its built SHA remains behind the implementation baseline. Do not invoke UBT, UAT, packaging, or an Editor build without user delegation.

### User Compile and PIE

1. Manually compile `PolyQuestEditor`.
2. In PIE verify target-relative forward/back/strafe/diagonal locomotion, the `800 degrees/second` idle-facing feel, Guard/Parry facing, Sprint free-run and resume, action/Dodge/Root-Motion/HitReaction yaw ownership, deterministic dead-target clockwise handoff, no-candidate clear, non-death clear without replacement, fixed camera stability, and the B1 Bow mouse-facing/Release target-assist/Homing regression. Confirm `BP_Enemy_Goblin.WeaponMesh` uses `No Collision` and no longer shortens the SpringArm.

## Review, Closeout, And Commit Boundary

1. After Automation, user compile, and PIE evidence, Main performs the current AGENTS.md default single defect-first fresh review; an executor self-review is strict only if one is later assigned.
2. On accepted closeout, synchronize README, ARCHITECTURE, ROADMAP, and this plan. Mark B2 completed and retain `TODO-03B-3` as the sole Bow locked-target preference owner.
3. The default commit contains only the approved B2 C++, Automation, and documentation. Exclude all `Content/**`, Config, Input/Widget/AnimBP, maps, project files, generated folders, and unrelated user WIP unless a later explicit stable closure is approved.

## Closeout Record

- **Implemented:** Player-owned lock Tick now applies fixed-rate target-facing only for eligible grounded locomotion, Guard, and non-Root-Motion `MOVE_None` Parry. The native yaw default is `800 degrees/second`. Sprint retains the same lock/highlight while using movement-facing; Attack, Dodge, Small/Big/Launch reaction, Bow, Root Motion, death, and Stunned retain yaw ownership.
- **Death handoff:** The Player stores the last valid screen candidate for the current target. A confirmed death performs one strict-viewport clockwise successor query, transfers the existing Health-bar highlight, and otherwise clears. Every non-death invalidation remains clear-only.
- **Automation repair:** The first B2 Automation run exposed an abstract `NewObject<UObject>` test fixture at `PlayerLockOnAutomationTests.cpp:356`; it was replaced by the existing concrete `FollowCamera` requester with the required complete Camera include. The user then confirmed `PolyQuest.Player.LockOn` Automation succeeds.
- **User validation:** The user confirmed focused PIE for locked locomotion/Guard/Parry, Sprint free-run/resume, action/Root-Motion protection, death handoff, non-death clear, Bow regression, the `800 degrees/second` retune, and `BP_Enemy_Goblin.WeaponMesh` `No Collision` camera behavior.
- **Fresh review:** Main performed one defect-first source review against baseline `a9ec82b`; no P0-P2 finding. `git diff --check` passed. CodeGraph was used for the current direct call chain. `code-review-graph` was built at `a9ec82b` with no daemon running, so its change report was stale-coverage supplemental evidence only; direct diff/source/tests controlled the conclusion.
- **Debt handoff:** No new B2 debt remains. `TODO-03B-3` is the sole owner of later Bow locked-target preference; `TODO-07A2` remains the existing owner of Enemy-bar idle visibility policy. Neither is partially implemented here.
- **Commit boundary:** Include only the five B2 C++ files, `PlayerLockOnAutomationTests.cpp`, `README.md`, `ARCHITECTURE.md`, `ROADMAP.md`, and `plan.md`. Exclude all `Content/**`, Config, map, Input/Widget/AnimBP, project-file, generated, and unrelated user WIP.
