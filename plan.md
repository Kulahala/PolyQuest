# TODO-03AI3B: Enemy Combat Target Retention v1

## Plan State

- Status: Completed. TODO-03AI3 and this blocking retention repair have passed their user-confirmed validation gates and Main's initial plus delta Fresh Reviews; this file remains as the completed-stage handoff record until the next accepted plan replaces it.
- Baseline: 7b78187dfedd6af9f98c8240e234203683a8a40d ([Feature] 完成投射物飞行拖尾).
- The uncommitted TODO-03AI3 Controller/header/test work already in the worktree is approved active-stage work, not unrelated WIP; preserve it.
- Objective: after an Enemy acquires the Player as combat target, retain that target through a temporary negative sight stimulus caused by the Player moving behind it, while still clearing deterministically when the Player leaves the existing combat-memory distance or the Enemy breaks its Home leash.
- Frozen v1 decisions:
  - Initial discovery remains the existing 140-degree sight cone. Passive Enemies do not become globally 360-degree aware.
  - Post-acquisition memory retains the current target only while it is valid, within existing LoseSightRadius in 2D, and the Enemy remains within existing Home LeashRadius.
  - Existing LoseSightRadius is the v1 memory distance. Do not add a DataAsset field, config value, grace timer, or Gameplay Tag.
  - Distance escape, Home-leash break, target invalidation/destruction, explicit clear, Enemy death, and UnPossess use the existing one-time ClearCurrentTarget(true) route.
  - No Search state, LastKnownLocation, Hearing/Damage sense, target retargeting, StateTree topology, animation, or perception-asset edit is included.
- Preserve all unrelated user WIP. Do not modify, stage, move, delete, or infer product behavior from .gitignore, Config/**, Content/**, maps, Blueprints, AnimBPs, GA/GE/Montage assets, .uproject, generated output, or any unlisted source file.

## Route And Delegation

Outer: ue-stage-workflow
Primary: ue5-cpp-gameplay
Support: ue5-debug-validation
Route reason: this repair owns one Controller-held combat-target memory flag, its perception-to-StateTree event boundary, and a bounded revalidation loop. It does not alter perception assets, StateTree structure, or GAS ownership.

Plan explorers: 0
Implementation executors: 1 (Gemini only after its read-only plan review is accepted and the user explicitly authorizes execution)
Complex Executor: one scoped lifecycle-sensitive C++ slice
Main parallel work: none
Reason: negative perception, target ownership, Root-Motion-facing integration, periodic validation, and TargetLost cleanup are one Controller lifecycle. Main owns contract, stage status, validation interpretation, Fresh Review, documents, staging, and commit; Gemini writes only the frozen source/test slice.

## Evidence And Design Decision

- AEnemyAIController::HandleTargetPerceptionUpdated currently calls ClearCurrentTarget(true) immediately whenever a negative FAIStimulus belongs to CurrentTarget. ClearCurrentTarget stops Reposition, clears Gameplay Focus, resets the Root-Motion handoff, and sends Event.AI.Target.Lost; the observed StateTree return-home behavior follows from that existing event path.
- The exact negative stimulus source can be sight-cone loss, occlusion, or distance. PeripheralVisionHalfAngleDegrees = 70 makes the described launch-then-walk-behind case highly plausible, but this repair fixes the proven immediate-clear behavior rather than assuming one visual cause.
- LoseSightRadius already belongs to Controller sight hysteresis and is configured into UAISenseConfig_Sight. Reusing it as bounded post-acquisition memory avoids a speculative second radius.
- TODO-03AI3 clears Controller Gameplay Focus during active Root Motion without clearing CurrentTarget. Retention must preserve that target through sight loss so the Root-Motion handoff can resume safely.

## Frozen Runtime Contract

### 1. Bounded Combat Target Retention

In Source/PolyQuest/Public/AI/EnemyAIController.h and Source/PolyQuest/Private/AI/EnemyAIController.cpp:

1. Add one private non-reflected boolean meaning current target has lost visual sight but remains retained by combat memory. Add a protected Tick(float DeltaSeconds) override and only private helpers needed to evaluate retention eligibility, process negative perception, and revalidate memory.
2. Positive perception of a valid Player continues through SetCurrentTarget. It clears the lost-visual retention flag and retains existing focus and Root-Motion deferral behavior.
3. A negative perception of the current Player target:
   - retains target and sets the lost-visual flag only when HasValidCombatTarget, a valid possessed Pawn, FVector::Dist2D(Pawn, CurrentTarget) <= LoseSightRadius, and !IsExceedingLeash all hold;
   - does not clear focus, stop navigation, send TargetLost, restart StateTree, or change Root-Motion yaw ownership in that retained case;
   - otherwise calls unchanged ClearCurrentTarget(true) exactly once.
   Negative perception for a non-current actor remains a no-op.
4. Before Super::Tick(DeltaSeconds), revalidate only while the lost-visual flag is active. If target is invalid, no longer within LoseSightRadius, or Enemy now exceeds LeashRadius, call ClearCurrentTarget(true). If it remains valid, do nothing. This prevents permanent retained aggro after the Player runs away.
5. ClearCurrentTarget, OnPossess, OnUnPossess, and existing Enemy-death cleanup reset the lost-visual flag. Do not add a second TargetLost event or duplicate StateTree transition.
6. Preserve initial 140-degree acquisition, SightRadius, LoseSightRadius, perception affiliation, CurrentTarget ownership, Home leash calculation, TargetAcquired/TargetLost tags, StateTree topology, Approach/Reposition geometry, GAS, complete TODO-03AI3 Root-Motion-facing handoff, and the current fixed 300 Reposition pace.

### 2. Test-Only Surface

Under WITH_DEV_AUTOMATION_TESTS, add only narrow wrappers/inspectors that invoke or observe the real production retention path:

- submit a Player perception-result boolean through the same private helper used by HandleTargetPerceptionUpdated;
- invoke the real retained-target revalidation path;
- read whether current target is retained without visual sight.

They must not directly set retention state, target pointer, focus, range, leash result, or StateTree event. Existing TODO-03AI3 test-only wrappers remain unchanged unless a compilation-required declaration grouping is unavoidable.

## Approved Source And Test Surface

Contract owner: Main. Implementation writer: Gemini only after explicit execution authorization. Any need to touch an unlisted file, public production API, AEnemyCharacter, AIProfile, Config, Gameplay Tag, Input route, Ability, StateTree, asset, map, Blueprint, AnimBP, Montage, document, or current Root-Motion-facing test requires a Main scope decision.

- Source/PolyQuest/Public/AI/EnemyAIController.h
  - Add only protected Tick declaration, private retention state/helpers, and frozen test-only wrapper/inspector declarations.
  - Preserve existing public APIs, CDO data, perception setup, Root-Motion-facing/pace members, and reflection visibility.
- Source/PolyQuest/Private/AI/EnemyAIController.cpp
  - Modify only Tick (new), HandleTargetPerceptionUpdated, SetCurrentTarget, ClearCurrentTarget, OnPossess, OnUnPossess, and minimal private retention helpers.
  - Do not change UpdateControlRotation, TryRequestCooldownReposition, StopCooldownReposition, OnMoveCompleted, ConfigureSight, StateTree start/stop, target-selection rules, or existing logging except where direct retention behavior requires no log.
- Add Source/PolyQuest/Private/Tests/EnemyCombatTargetRetentionAutomationTests.cpp with Automation name PolyQuest.Enemy.CombatTargetRetention.
  - Reuse the transient valid Enemy Controller setup proven by EnemyRootMotionFacingAutomationTests: valid transient AttackSet/Profile, passive Enemy fixture, real Controller possession, and Player fixture.
  - Do not modify CombatAutomationFixture, use Content assets, require NavMesh, or depend on renderer/viewport.

## Native Automation Contract

PolyQuest.Enemy.CombatTargetRetention must prove:

1. A front-side positive perception acquires the Player through real production target route.
2. A negative perception of that Player while inside LoseSightRadius and Home leash retains CurrentTarget, marks lost-visual memory, and survives revalidation.
3. A later positive perception clears only lost-visual memory and retains the same target.
4. After an in-range negative perception, moving Player beyond LoseSightRadius then revalidating clears target through existing TargetLost route.
5. After an in-range negative perception, moving Enemy beyond Home leash then revalidating clears target through existing TargetLost route.
6. Negative perception for another Player never affects current target.
7. UnPossess and explicit clear reset retained-memory without a stale later restoration.
8. With active transient Root Motion, an in-range negative perception preserves CurrentTarget; existing TODO-03AI3 Controller update still owns yaw and does not reintroduce focus during Root Motion.

Run this new suite, PolyQuest.Enemy.RootMotionFacing, PolyQuest.Enemy.CombatSpacing, PolyQuest.Combat.HitReaction, and complete current Automation matrix. The full matrix must remain green.

## Execution Order

1. Add minimal Controller retained-memory state, bounded eligibility helper, and Tick revalidation before modifying perception behavior.
2. Route positive/negative perception and existing clear/possess teardown through frozen state transitions.
3. Add isolated headless Automation suite and only named test seams.
4. Gemini rereads final diff and direct callers/callees, runs Rider errors-only inspection on three touched C++ files using solution-relative paths, and runs git diff --check. It must not compile, enter PIE, edit assets, update documents, stage, or commit.
5. User manually compiles PolyQuestEditor, runs targeted/full Automation, and validates PIE. Main interprets evidence and performs separate Fresh Review across TODO-03AI3 plus this repair.

## User-Owned Editor And PIE Contract

1. Do not alter PeripheralVisionHalfAngleDegrees, SightRadius, LoseSightRadius, StateTree, or authored assets. Confirm current values only by readback.
2. Manually compile PolyQuestEditor.
3. In Scene01, approach a passive Enemy from behind before it acquires Player: existing initial sight-cone behavior must remain; no new global 360-degree rule.
4. Acquire combat from front, launch/hit Enemy, then move behind while inside current LoseSightRadius and Home leash: it must retain combat, not return Home, and retain TODO-03AI3 Root-Motion-facing behavior.
5. From retained behind-target state, run beyond LoseSightRadius, then separately make Enemy exceed Home leash: each must do normal one-time disengage/return-home.
6. Report Editor readback, compilation, focused/full Automation, and PIE evidence separately.

## Non-Goals, Documentation, And Commit Boundary

- No global 360-degree detection, dynamic peripheral-vision change, Search/Alert state, LastKnownLocation, hearing/damage sense, target grace timer, target swap, AIProfile field, GE_Walk_MoveSpeed reuse, Enemy MoveSpeed GE, DataAsset migration, StateTree edit, animation edit, GameplayCue, new Tag, ranged Enemy behavior, or persistence work is included.
- Current fixed 300 Reposition pace remains a validated v1 implementation. TODO-03H4 owns future health check: migrate to UEnemyAIProfile-authored pace plus dedicated Enemy MoveSpeed effect only if a second Enemy needs another pace or Enemy MoveSpeed GameplayEffects become real.
- After TODO-03AI3B validation and Main Fresh Review, Main alone closes both slices in plan.md, marks Roadmap accurately, updates ARCHITECTURE.md, and updates README.md only if its public evidence summary needs it.
- Default eventual commit scope is approved TODO-03AI3 Controller/header/test files, this new Automation suite, and completed documents. Exclude Content/**, Config/**, maps, Blueprints, AnimBPs, GA/GE/Montage assets, imported resources, .uproject, generated folders, and all unrelated user WIP unless user separately approves stable closure.

## Closeout Record

- **Implemented contract:** `AEnemyAIController` remains the sole owner of Enemy perception, `CurrentTarget`, Gameplay Focus, Home leash, tactical Reposition pace, and StateTree target events. Active Root Motion clears Gameplay Focus and returns before Controller yaw/control-rotation writes; after Root Motion ends, a valid target restores focus and Pawn yaw turns only through the bounded `800 degrees/second` recovery handoff. Cooldown Reposition temporarily captures and overrides only the controlled Enemy's `MaxWalkSpeed` to `300`, then restores the exact prior value on completion, failure, interruption, UnPossess, or death.
- **Retention contract:** initial discovery remains the existing 140-degree Sight cone. A negative Sight stimulus for the current Player retains the target only inside `LoseSightRadius` and Home leash; the Controller's pre-`Super::Tick` revalidation clears through the existing one-time `TargetLost` route once either bound fails. Positive Sight clears only the retained-without-sight flag. `SetCurrentTarget()` now fails closed without a possessed Pawn, so a late positive perception callback after `UnPossess()` cannot restore a stale target into a later possession.
- **Validation evidence:** the user confirmed focused Scene01 PIE for the Root-Motion-facing and retained-target player loop, then confirmed the complete 18-suite Editor Automation matrix after the late-perception safety repair, including `PolyQuest.Enemy.RootMotionFacing` and `PolyQuest.Enemy.CombatTargetRetention`. A separate manually logged `PolyQuestEditor (Development Editor)` build is not claimed; the recorded Editor Automation run is runtime evidence for the loaded current test code.
- **Review:** Main's first defect-first Fresh Review found one P1: a late positive perception after `UnPossess()` could recreate `CurrentTarget` and bypass fresh Sight acquisition on a later possession. The central no-Pawn guard and the `UnPossess -> late positive -> re-Possess -> fresh positive` production-path regression were added. Main delta Fresh Review found no remaining P0/P1/P2. Rider errors-only inspection of the repaired Controller and retention test returned zero errors; scoped `git diff --check` passed. The Rider public-to-protected override notice is an accepted non-runtime style observation with no direct caller, not a Roadmap debt.
- **Debt handoff:** no new accepted debt was created. `TODO-03H4` remains the canonical owner of the already-recorded future interaction between the fixed Controller-owned `300` Reposition override and any future Enemy MoveSpeed GameplayEffect/data-authored pace requirement.
- **Commit boundary:** include only `EnemyAIController.h/.cpp`, `EnemyRootMotionFacingAutomationTests.cpp`, `EnemyCombatTargetRetentionAutomationTests.cpp`, and the synchronized project documents. Exclude all `Content/**`, `Config/**`, `.uproject`, generated files, and unrelated worktree changes.
