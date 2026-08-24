# TODO-07B1: Combat Hit Feedback v1

## Plan State

- Status: Complete.
- Baseline: `feeb378` (`[Docs] 安排基础战斗反馈阶段`).
- Objective: add short visual hit feedback for actual nonlethal Health damage without changing damage delivery, Guard/Parry consumption, Hit Reaction classification, movement, camera ownership, or asset topology.
- Preserve all user-owned WIP. This stage does not write or stage `Content/**`, Config, maps, Blueprints, Input, AnimBPs, Niagara, GA/GE/Montage assets, project files, or generated output.

```text
Outer: ue-stage-workflow
Primary: ue5-cpp-gameplay
Support: ue5-debug-validation, game-feel
Route reason: B1 modifies existing Health attribute callbacks, Character EndPlay, Mesh Overlay lifetime, and local PlayerCameraManager feedback without changing the combat resolver.
```

```text
Plan explorers: 0
Implementation executors: 1
Complex Executor: none
Main parallel work: plan ownership, evidence accounting, and final fresh review only
Reason: Gemini reviews the plan, executes the complete approved C++/Automation slice, and performs a strict implementation self-review. Main owns scope, plan, user validation interpretation, documentation, staging, and the separate final fresh review.
```

## Implementation Evidence

- **Process correction:** the user clarified the executor split after implementation had started. Main had already written the B1 runtime integration while Gemini implemented only the Automation slice; this is a recorded process deviation and must not be described as Gemini having executed the complete B1 slice. Before final fresh review, Gemini must now strict-review the complete approved B1 diff and make only justified in-scope repairs. Future stages use the corrected split from the start.
- Gemini's read-only review was accepted: the Health callbacks invoke feedback after real nonlethal Health-GE/death checks but before Stunned or reaction-tier suppression; deferred fixtures inject their test Overlay and Player Shake before `FinishSpawning()`; Headless Shake coverage uses a possessed Controller explicitly marked local rather than a viewport or Engine-private active-shake list.
- Main integrated the runtime Overlay/Shake lifecycle and reviewed the test slice. The native test Shake uses `FObjectInitializer::SetDefaultSubobjectClass` for its persistent Pattern; it does not call `NewObject` through `ChangeRootShakePattern()` while constructing a UObject.
- Static preflight completed against the approved B1 C++ surface: direct caller/callee reads, CodeGraph, code-review-graph impact against `feeb378` (graph-built SHA matches the baseline), Rider error-level inspection, and scoped `git diff --check`. Rider reports no current error-level issue. Graph test-gap labels remain supplemental because the new Automation file is untracked until the eventual approved commit; direct test source is the coverage evidence.
- The first user `PolyQuest.Combat.HitFeedback` run failed after the new suite created its transient World: it omitted the `World->BeginPlay()` transition used by the established Exhaustion fixture, and its single large world tick was not a stable TimerManager frame pump. Its temporary local `APlayerController` also exposed eight missing InputAction warnings. Gemini's test-only repair now begins the World before spawning fixtures, advances Timer time in `0.05s` frame steps with `GFrameCounter` progression, and injects one transient `UInputAction` through the existing `WITH_DEV_AUTOMATION_TESTS` startup fixture before `FinishSpawning()`. The runtime Overlay/Shake path is unchanged.
- User-confirmed evidence: focused PIE passed, and the repaired `PolyQuest.Combat.HitFeedback` Automation plus all eleven named regression suites passed in the Editor front-end. The remaining logs are intentional negative assertions: invalid multi-tier Reaction Tags, no accepted Stance Break fallback, equipment preflight/rollback/active-swap rejection, and invalid static Trace geometry. This is user runtime/visual evidence, not an Agent asset readback or a separately supplied manual compile claim.
- Main's single defect-first fresh review against `feeb378` found no P0-P2 source defect in the approved B1 diff. Direct source, CodeGraph, Rider error-level inspection, and scoped `git diff --check` were reviewed; code-review-graph coverage labels remain supplemental because its baseline index cannot prove the untracked new Automation coverage. No error-memory MCP endpoint was available.
- The full B1 Automation matrix and focused PIE gate are complete. `README.md`, `ARCHITECTURE.md`, `ROADMAP.md`, this plan, and the repository delegation boundary are synchronized; B2 remains the next accepted stage. The default commit still excludes all user-owned `Content/**`, Config, maps, Blueprints, Input, AnimBPs, Niagara assets, project files, generated output, and unrelated WIP.

## Approved Runtime Contract

1. `ABaseCharacter` owns one protected, non-Blueprint-callable global Mesh Overlay lifecycle. It exposes only inherited `EditDefaultsOnly` authoring fields `HitFeedbackOverlayMaterial` and `HitFeedbackOverlayDurationSeconds`; the duration defaults to `0.10s`.
2. The helper uses only `GetOverlayMaterial()` and global `SetOverlayMaterial()`. It never replaces a normal material slot or changes `MaterialSlotsOverlayMaterial`.
3. On the first flash, cache the current global Overlay and apply the authored flash material. A repeat hit refreshes the one Timer. If another system replaces the Overlay while the flash is active, preserve that newer external Overlay as the value to restore after the refreshed flash.
4. On Timer expiry or `EndPlay`, restore the cached Overlay only when the Mesh still displays B1's active flash material. If another system has already changed it, leave that value untouched. Clear the Timer and transient cache through the same idempotent helper.
5. Missing Overlay material or non-positive duration is a once-per-actor authoring warning followed by a no-op. Player Camera Shake remains an independent channel and may still run.
6. Player and Enemy invoke the Overlay only after their existing Health delegates establish a real nonlethal GameplayEffect-driven decrease: `NewValue > 0`, `NewValue < OldValue`, valid `GEModData`, and no Dead state/teardown. The call is before Hit Reaction tier classification and before Stunned/Poise-Broken reaction suppression.
7. Therefore nonlethal Health damage with no reaction tag, an invalid multi-tier tag, Stunned state, or Enemy Poise Broken state still flashes. Healing, direct attribute-base changes, Poise-only changes, lethal Health damage, dead targets, and teardown do not flash.
8. Guard/Parry remain naturally excluded: both current Resolver paths consume a successful defense before applying the Health GameplayEffect. No B1 branch is added to Guard, Parry, melee tracing, or projectile hit resolution.
9. `APlayerCharacter` gains one `EditDefaultsOnly` `HitFeedbackCameraShakeClass`. On a valid local `APlayerController` with `PlayerCameraManager`, it calls `StartCameraShake(Class, 1.0f)`. It never moves the Pawn, CameraBoom, fixed camera transform, OS window, or FOV.
10. The authored Camera Shake must have `bSingleInstance=true`; UE 5.8 then restarts the same class instance on repeat hits instead of stacking it. Missing local controller/manager is a transient no-op; a missing authored Shake class on a valid local Player is a once-per-actor configuration warning.
11. This is single-player local presentation. No replication, Gameplay Tags, input, new damage route, Hit Stop, sound, Niagara, weapon trail, or generic feedback framework is introduced.

## Approved Source And Test Surface

- Main-only runtime integration:
  - `Source/PolyQuest/Public/Character/BaseCharacter.h` and `Private/Character/BaseCharacter.cpp`: shared Overlay authoring fields, transient cache/Timer, protected apply/clear helpers, EndPlay cleanup, and narrow test-only configuration/state hooks.
  - `Source/PolyQuest/Public/Character/Player/PlayerCharacter.h` and `Private/Character/Player/PlayerCharacter.cpp`: Player Shake class, local Camera Manager trigger, one-time configuration signal, test-only observability, and correctly placed Health callback call.
  - `Source/PolyQuest/Private/Character/Enemy/EnemyCharacter.cpp`: correctly placed Overlay trigger only; no Enemy Camera Shake and no Enemy header/public API expansion.
- Gemini-only Automation implementation after Main freezes the above interfaces:
  - `Source/PolyQuest/Private/Tests/CombatAutomationFixture.cpp` and any required test-only fixture declaration: inject a valid Engine/test Overlay into every deferred Player/Enemy fixture and the Player test Shake class into every Player fixture so H3 clean-success signal rules remain intact.
  - `Source/PolyQuest/Private/Tests/TestHitFeedbackCameraShake.h/.cpp`: native Automation-only `UCameraShakeBase` subclass with `bSingleInstance=true` plus a private Engine-native test Pattern that stays active across the second start, so instance reuse can be observed without Engine private state or an `EngineCameras` module dependency.
  - `Source/PolyQuest/Private/Tests/CombatHitFeedbackAutomationTests.cpp`: new `PolyQuest.Combat.HitFeedback` suite using the H3 fixture, a possessed `APlayerController`, real `UTestProjectileDamageGE` health changes, and no Content assets.
- The dedicated suite must cover Player/Enemy nonlethal flash, Player Shake invocation/single-instance reuse through a narrow test-only observation point rather than Engine private shake lists, no-tag and invalid-tag independence, healing/direct-base/lethal/dead rejection, repeated-hit Timer refresh, initial Overlay restoration, external Overlay preservation, and active-flash destroy/EndPlay cleanup.
- Existing `PolyQuest.Player.ActionWindows` remains the Guard/Parry regression proof; this stage does not manufacture a second defense test route.

## User-Owned Editor Work

1. Create one local shared `M_HitFlash_Red`, preferably under `Content/_Feedback/Materials/`: Surface, Translucent, Unlit, red Emissive/Opacity Overlay. Start at roughly `0.65` opacity; tune only after PIE.
2. Create `CS_PlayerHit` under `Content/_Feedback/Camera/` as a `DefaultCameraShakeBase` / `CameraShakeBase` child. The UE 5.8 EngineCameras plugin is enabled by default; do not add a project plugin or module dependency. Set `bSingleInstance=true`; use a brief low-amplitude noise pattern, roughly `0.12s`, with no FOV change.
3. In `BP_Player` and `BP_Enemy_Goblin`, assign the same inherited Overlay material and `0.10s` duration. In `BP_Player` only, assign `CS_PlayerHit`.
4. Read back that the Player and Enemy Meshes have no per-slot Overlay configuration that would override the global B1 Overlay. These assets remain user-owned WIP and are excluded from the default commit.
5. Manually compile `PolyQuestEditor`, run Automation from the Editor front-end, and perform focused Scene01 PIE validation.

## Validation Matrix

- New Automation: `PolyQuest.Combat.HitFeedback`.
- Regression Automation: `PolyQuest.Equipment.TransactionMatrix`, `PolyQuest.Melee.TraceSourceGeometry`, `PolyQuest.Player.ActionWindows`, `PolyQuest.Combat.HitReaction`, `PolyQuest.Enemy.AttackSetSelection`, `PolyQuest.Enemy.CombatSpacing`, `PolyQuest.UI.VitalHUD`, `PolyQuest.Player.Exhaustion`, `PolyQuest.Player.LockOn`, `PolyQuest.Projectile.Lifecycle`, and `PolyQuest.Projectile.TargetAssist`.
- Successful paths must retain H3's clean fixture signal contract: no missing Loadout, weapon, stamina, AI, Poise, ragdoll, Hit Feedback, or invalid AbilitySpec configuration warnings. Existing deliberate negative-test signals remain mapped to their assertions.
- PIE: verify Player melee/projectile damage flashes red and shakes briefly; Enemy nonlethal damage flashes without changing the Player camera; rapid hits reset cleanly without permanent red or increasing shake amplitude; Guard/Parry neither flash nor shake; lethal damage does not steal death presentation; LockOn/HUD/camera collision/Bow/Hit Reactions remain unchanged.
- Static before user handoff: read changed callers/callees; Rider `lint_files` with project-relative paths, `rootFolder=E:/GameDevelop/PolyQuest`, and `timeout=60000`; CodeGraph; code-review-graph diff impact; `git diff --check`. Agents do not call UBT, UAT, packaging, or Editor compilation.

## Review, Documentation, And Commit Boundary

- Gemini first performs a strict read-only plan review and reports only concrete P0-P2 blockers, lifecycle risks, or missing tests. Main accepts/rejects suggestions and freezes the whole approved slice before Gemini implements it. Gemini then conducts a strict self-review; it is not an independent fresh review.
- User validation and Main's single defect-first fresh review are complete. The four project documents and `AGENTS.md` now record the stable B1 contract, evidence, next stage, and the corrected Main-plan/Gemini-execution boundary. `TODO-07B2` remains next work.
- Default commit includes only approved B1 C++, Automation, and the four project documents. It excludes all `Content/**`, Config, maps, Blueprints, Input, AnimBPs, Niagara assets, project files, generated output, and unrelated user WIP.
