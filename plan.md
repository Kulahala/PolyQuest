# TODO-07B1A: Reaction-Tier Player Hit Camera Shake v1

## Plan State

- Status: Complete. The strict three-tier implementation, user validation, P1 lifecycle repair, and Main delta fresh review are complete.
- Baseline: `8d8e1d5` (`[Feature] 完成战斗受击反馈 (Combat Hit Feedback)`).
- Objective: extend the completed B1 Player-only Camera Shake into exactly `Small` / `Big` / `Launch` reaction-tier variants while preserving B1's red-flash, damage, reaction-event, and fixed-camera contracts.
- Preserve all user-owned WIP. This stage does not write or stage `Content/**`, Config, maps, Blueprints, Input, AnimBPs, Niagara, GA/GE/Montage assets, project files, generated output, or unrelated worktree changes.

```text
Outer: ue-stage-workflow
Primary: ue5-cpp-gameplay
Support: ue5-debug-validation, game-feel, camera-systems
Route reason: B1A changes the existing Player Health-delegate presentation selection, exact CameraShake lifetime, and the existing native Automation suite without changing the damage or reaction systems.
```

```text
Plan explorers: 0
Implementation executors: 1 (Gemini, user-assigned)
Complex Executor: Gemini external execution boundary
Main parallel work: frozen-contract ownership, validation evidence accounting, and post-validation fresh review
Reason: Player Header, Health callback, EndPlay cleanup, and Automation form one lifecycle-sensitive slice. Main owns the contract; Gemini may implement only the named frozen paths after plan review approval.
```

## Evidence And Decisions

- Local UE 5.8 headers confirm `APlayerCameraManager::StartCameraShake(...)` returns the started instance and `StopCameraShake(UCameraShakeBase*, true)` stops exactly that instance. `UCameraShakeBase::bSingleInstance` restarts the timer for a repeated class instead of stacking it.
- Current `APlayerCharacter::OnHealthAttributeChanged()` proves real nonlethal Health GE damage, triggers B1 Overlay before Stunned/reaction suppression, then classifies the exact `Data.Reaction.Small`, `Data.Reaction.Big`, and `Data.Reaction.Launch` Asset Tags.
- The user selected CameraLocal rotation-only feedback. No Camera Location offset, Roll, or FOV variation is introduced.
- Gemini's read-only plan review found no P0-P2 blocker. Its three non-blocking recommendations are adopted explicitly below: weak-reference validity guards, native test patterns that remain active until explicitly stopped, and one pre-`FinishSpawning()` fixture injection path.
- The user rejected a fourth Generic Camera Shake asset/field. The B1 generic Shake behavior is deliberately narrowed: a legal `None` tier, an `Invalid` multi-tier tag set, or a missing valid-tier class now leaves Camera Shake untouched while retaining B1 Overlay behavior and the existing Invalid-tag reaction-event warning.
- The current working tree already has the B1A scheduling diff in `ROADMAP.md`; retain it. It is Main-owned documentation and not part of Gemini's source/test handoff.

## Frozen Runtime Contract

1. Remove the retired `HitFeedbackCameraShakeClass` and its missing-class warning state. Keep exactly three non-callable `EditDefaultsOnly` Player fields under `Combat|Feedback`:
   - `SmallHitFeedbackCameraShakeClass`
   - `BigHitFeedbackCameraShakeClass`
   - `LaunchHitFeedbackCameraShakeClass`
   They are authored on `BP_Player`; no Blueprint function, Gameplay Tag, input, DataAsset, or generic feedback framework is added.
2. After the existing authority, real nonlethal Health-GE, ASC, and Dead checks, `OnHealthAttributeChanged()` must keep `TriggerHitFeedbackOverlay()` first. It then reads Asset Tags and computes `EHitReactionTier` only for feedback selection, calls the tier-aware Shake helper, then retains the existing Stunned return and Invalid-tag reaction-event warning/return order.
3. `Small`, `Big`, and `Launch` use only their matching authored class. `None` and `Invalid` do not call `StartCameraShake()` and do not clear, replace, or otherwise mutate an already-active tier Shake. A missing valid-tier class emits one Player-local configuration Warning for that tier and no-ops; it does not fall back to another tier.
4. A valid Stunned Player still receives the selected valid-tier presentation Shake, but no reaction Gameplay Event is sent. `None` and Invalid tags remain without Shake; when not Stunned Invalid retains its existing log and no reaction event, while Stunned still returns before that existing log branch. B1 Overlay behavior is unchanged for every valid nonlethal Health GE decrease.
5. `APlayerCharacter` owns only its exact started Shake instance, the originating `APlayerCameraManager`, and its class. Store both UObject references as `TWeakObjectPtr`; before `StopCameraShake(OldInstance, true)`, require both `ActiveHitFeedbackCameraManager.IsValid()` and `ActiveHitFeedbackCameraShake.IsValid()`. Otherwise perform no dereference and only reset local state. The same class calls `StartCameraShake()` again and relies on `bSingleInstance=true` to restart. A different class or CameraManager stops the exact old instance through the stored old manager before starting the new one. Never call a class-wide or global stop API.
6. `EndPlay()` clears this exact active instance before existing teardown and `Super::EndPlay()`. The clear helper is idempotent and always resets the weak references/class state, even when the manager or instance has already expired.
7. This remains Player-local presentation. Do not modify `ABaseCharacter` Overlay logic, `AEnemyCharacter`, `FHitReactionClassifier`, damage delivery, Guard/Parry, Hit Reaction Gameplay Events, Root Motion, CameraBoom, Pawn transform, FOV, or world camera behavior.

## Approved Source And Test Surface

**Contract owner: Main. Implementation writer: Gemini after explicit execution authorization.**

- `Source/PolyQuest/Public/Character/Player/PlayerCharacter.h`
  - Remove the Generic authored field/state and retain only the three tier fields, private tier-aware selection/start/clear declarations, `TWeakObjectPtr<APlayerCameraManager>` plus `TWeakObjectPtr<UCameraShakeBase>` exact active state, per-tier warning state, and minimal `WITH_DEV_AUTOMATION_TESTS` configuration/observation surface.
  - A forward declaration of `APlayerCameraManager` and non-reflected `EHitReactionTier` is allowed only if required by those private helpers.
- `Source/PolyQuest/Private/Character/Player/PlayerCharacter.cpp`
  - Implement the frozen three-tier selection, stop/restart, and EndPlay lifecycle above. Keep the existing local-controller/CameraManager guard; `None` and Invalid must resolve to null without a configuration warning.
- `Source/PolyQuest/Private/Tests/CombatAutomationFixture.cpp`
  - Before `FinishSpawning()`, make one three-class test-only configuration call for every Player fixture so H3's clean-success-path signal contract remains intact. Do not leave a per-test or post-BeginPlay injection alternative.
- `Source/PolyQuest/Private/Tests/TestHitFeedbackCameraShake.h/.cpp`
  - Retain the `UTestHitFeedbackCameraShakePattern` that remains active until `StopShakePatternImpl()` or teardown, and bind Small, Big, and Launch test classes to it. All three classes must be `bSingleInstance=true`, so Headless Automation cannot naturally expire an instance before same-tier restart or cross-tier replacement assertions.
- `Source/PolyQuest/Private/Tests/CombatHitFeedbackAutomationTests.cpp`
  - Extend the existing suite; do not create a thirteenth suite or introduce Content dependencies.

The existing singular test configuration method has only the shared fixture caller and may be replaced by one three-class test-only configuration method. No other Header/API expansion is allowed.

## Automation And User Validation

`PolyQuest.Combat.HitFeedback` must cover:

- No reaction tag and deliberate invalid multi-tier tags flash but do not start, clear, replace, or log a missing Camera Shake.
- Exact Small, Big, and Launch class selection.
- Same-tier restart returning the same active single-instance class; Small -> Big -> Launch replacement stops the previous instance (`IsActive() == false`) rather than stacking it.
- Stunned tier damage selecting the tier Shake while existing reaction suppression remains intact.
- Enemy damage not increasing the Player Shake start count.
- Active Shake cleanup on Player `EndPlay`, while retaining all B1 Overlay/Timer/external-Overlay/death/destroy regression checks.

The invalid multi-tier tag Warning remains a deliberate negative-test signal. No successful fixture path may emit missing Player Loadout, weapon, Stamina, AI, Poise, Ragdoll, missing Shake, or invalid AbilitySpec warnings.

User-owned Editor work after source validation:

1. Rename the current working `CS_PlayerHit` through the Unreal Content Browser to `CS_PlayerHit_Small`, then duplicate it to create `CS_PlayerHit_Big` and `CS_PlayerHit_Launch`. There is no `CS_PlayerHit_Generic` asset and no Generic Player field after recompilation.
2. All three authored classes use `bSingleInstance=true`, CameraLocal rotational noise only, zero Location/FOV, and zero Roll. Start with roughly `0.08s`, `0.12s`, and `0.18s` for Small/Big/Launch; tune amplitude in PIE so intensity is strictly increasing without camera motion sickness.
3. Assign the three classes on `BP_Player`. The retired Generic field will disappear after class recompilation; do not change any Enemy asset.
4. Manually compile `PolyQuestEditor`, run `PolyQuest.Combat.HitFeedback` and the eleven existing regressions: `Equipment.TransactionMatrix`, `Melee.TraceSourceGeometry`, `Player.ActionWindows`, `Combat.HitReaction`, `Enemy.AttackSetSelection`, `Enemy.CombatSpacing`, `UI.VitalHUD`, `Player.Exhaustion`, `Player.LockOn`, `Projectile.Lifecycle`, and `Projectile.TargetAssist`.
5. PIE: validate Small < Big < Launch readability, rapid cross-tier hits without accumulated amplitude, no Camera Location/FOV change, Guard/Parry and lethal damage remaining silent, and Enemy hits never shaking the Player camera.

## Review, Closeout, And Commit Boundary

- Gemini first performs a strict read-only plan review and reports only P0-P2 blockers, lifecycle risks, or missing required coverage. It must not edit until the user explicitly authorizes execution.
- After execution, Gemini performs a strict implementation self-review. It is not an independent fresh review. Main interprets user validation and performs one separate defect-first fresh review.
- Static preflight before user compile: final caller/callee reads, CodeGraph, code-review-graph impact against `8d8e1d5`, Rider error-level inspection, and `git diff --check`. Rider MCP recovered on 2026-08-24: project-relative `get_file_problems` and `lint_files` both returned zero error-level issues on the current B1 sources. Re-run the targeted inspection on the final touched files; if transport fails again, record that limitation rather than claiming inspection passed.
- After accepted validation/review, Main updates `README.md`, `ARCHITECTURE.md`, `ROADMAP.md`, and this closeout record. Default commit includes only approved B1A C++, Automation, and those four documents; it excludes all `Content/**`, Config, maps, Blueprints, Input, AnimBPs, GA/GE/Montage assets, project files, generated output, and unrelated WIP.

## Closeout Record

- Implementation: the retired Generic Player Camera Shake field and test class are removed. `APlayerCharacter` now selects only the authored `Small`, `Big`, or `Launch` class from the existing `Data.Reaction.*` classification, while legal `None` and fail-closed `Invalid` tags preserve B1's Overlay-only behavior and leave an active tier Shake untouched. Same-tier hits rely on UE 5.8 `bSingleInstance`; a valid tier change stops the exact prior instance through its originating `APlayerCameraManager` before starting the replacement.
- Lifecycle: the Player stores only weak references to its exact B1A CameraManager, instance, and class. `UnPossessed()` and `EndPlay()` share idempotent cleanup. During fresh review, the first implementation's direct `StopShake()` / `TeardownShake()` fallback after Manager expiry was rejected because it bypassed `CameraModifier` removal and pooling. The final code dereferences only when both weak references remain valid, otherwise clears local state only.
- Automation: the shared deferred-spawn fixture injects Small/Big/Launch native test classes before `FinishSpawning()`. `PolyQuest.Combat.HitFeedback` covers tier selection, same-tier reuse, cross-tier replacement, no-tag/Invalid no-op, Stunned presentation, Enemy non-participation, and teardown. `Player->Destroy()` reaches the new `UnPossessed()` path through the UE Pawn teardown chain and asserts the active Shake stops.
- User-confirmed runtime evidence: focused PIE and all twelve Automation suites passed through the Unreal Editor front end. The retained log signals are the established negative assertions for invalid multi-tier reaction tags, Stance Break fallback, equipment rejection/rollback, and invalid static Trace geometry.
- Main static/review evidence: final caller/callee inspection, CodeGraph, scoped code-review-graph impact, Rider error-level inspection, and `git diff --check` passed. Main's defect-first fresh review found the Manager-expiry P1, and its post-repair delta review found no remaining P0-P2. Graph test-gap labels are supplemental because Unreal Automation macros are not fully linked by the graph; direct test and Engine teardown reads establish the relevant coverage.
- Scope: the commit contains only B1A native source, Automation, and project documentation. The three authored Camera Shake assets, BP assignments, and all other `Content/**`/Config/map/input/AnimBP WIP remain user-owned and excluded. `TODO-07B2` remains the next accepted stage.
