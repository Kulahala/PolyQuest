# TODO-02C3L: Four-Directional Small/Big Hit Reactions v1

## Plan State

- Status: Completed / Closed. The C3L source/test WIP removed the obsolete generic fallback contract after the user intentionally cleared the four legacy single-Montage fields.
- Baseline: `47d627c` (`[Feature] 完成战斗命中反馈 (Complete Combat Impact Feedback)`).
- Documentation closeout: Main synchronized `README.md`, `ARCHITECTURE.md`, `ROADMAP.md`, and this closeout record. No stage file is staged or committed by this plan.
- Objective: make living Player and Enemy Small/Big reactions select an attacker-relative Front, Back, Left, or Right authored Montage from one complete required set, with no legacy single-Montage fallback.
- Player-facing success: from any cardinal attack direction, the victim visibly plays the matching directional Small or Big reaction; diagonal attacks resolve deterministically to a nearest cardinal reaction; invalid impact data safely skips the reaction instead of playing a wrong directional asset; no Player/Enemy makes a preparatory turn toward the attacker before a Big reaction.
- Scope decision: C3L is four-direction only. Eight-direction presentation is not prebuilt or partially generalized; it requires a separate accepted stage after compatible diagonal assets exist and focused PIE proves four-direction mapping visually inadequate.

## Route And Delegation

Outer: `ue-stage-workflow`
Primary: `ue5-cpp-gameplay`
Support: none
Route reason: this is a bounded native GAS presentation-selection change. It consumes the existing Health-event Context and changes only Montage choice; authority, damage, tags, movement, and authored assets retain their current owners.

Plan explorers: 0
Implementation executors: 1 (Gemini, only after plan review and explicit Main handoff)
Complex Executor: none
Main parallel work: none
Reason: the four Ability integrations and one shared pure selector form a small, coherent source/test slice. Documentation, asset authoring, validation interpretation, review, staging, and commit remain Main/User-owned.

Main owns the C3L contract, public/reflection boundary, shared resolver interpretation, documentation, acceptance, staging, and commit. Gemini may write only the approved C++/test paths below. It must stop and return evidence before touching an unlisted path, public API, GameplayTag, Config, Build.cs, asset, Blueprint, test fixture, or lifecycle rule.

## Existing Evidence

1. `FHitReactionImpactResolver::ResolveImpactDirection()` already returns a finite, normalized target-local planar `Target -> Attacker` direction from the same `FGameplayEventData::ContextHandle` forwarded by Player and Enemy Health delegates. Instigator location takes priority over `ImpactNormal`; invalid input returns `FVector::ZeroVector`.
2. The user intentionally cleared the four legacy serialized single-Montage fields after the original C3L PIE/Automation pass to prepare their removal. Their continued source-level validation is therefore a current authoring/runtime mismatch, not a compatibility requirement. `AM_SmallHitReaction_F` and `AM_BigHitReaction_F` are retained user-owned Front directional Montages, not generic fallbacks.
3. Static asset inventory contains four candidate source sequences for each tier under `Content/_Animations/Weapon/LightSword/`: `A_Hit_[F/B/L/R]_React_Sword` and `A_Hit_[F/B/L/R]_Stagger_RootMotion_Sword`. This is file-inventory evidence only; Editor preview, Skeleton compatibility, Montage configuration, and PIE behavior remain user validation.
4. Small is an overlay route with no movement/input cancellation. Big is a grounded full-body Root Motion interrupt route. Launch has its independent C3I/C3J target-facing and CharacterMovement trajectory contract.

## Frozen Runtime Contract

### Direction Semantics

1. Direction names refer to the attacker's position in the target's local frame, never to the desired movement/displacement direction.
2. The shared four-way selector receives only a local planar vector and uses these exact sectors:
   - `Abs(X) >= Abs(Y)`: `X >= 0` selects Front; otherwise Back.
   - `Abs(X) < Abs(Y)`: `Y >= 0` selects Right; otherwise Left.
   - Exact 45-degree ties therefore use the X axis: Front for X-positive front diagonals and Back for X-negative back diagonals. The implementation must not change this tie-break rule.
3. The selector must first copy `FVector(Local.X, Local.Y, 0.0f)` and use only that planar value for finite checks, near-zero rejection, absolute-value comparison, and sector selection. Z is always ignored.
4. A complete Front/Back/Left/Right Montage set is mandatory. Zero, near-zero, NaN, Inf, missing `TriggerEventData`, a missing target, an incomplete set, or a null selected member produces no Montage selection. The Ability follows its existing immediate activation-failure cleanup without creating a Montage task; it must not substitute Front, a legacy field, or any other direction.
5. Direction is evaluated once during the synchronous Ability activation. No `FGameplayEventData`, Context, Actor, or pointer into effect data survives that call.

### Ownership And Non-Goals

1. Small remains `InstancedPerActor`, `ServerOnly`, non-interrupting, and overlay-only. It gains no movement/input block, cancellation, yaw, or CharacterMovement write.
2. Big remains its existing grounded, full-body Root Motion route. It receives the selected Montage before task creation, but retains its existing action cancellation, velocity stop, ledge safety, Falling teardown, and unified EndAbility behavior.
3. Big must not call `SetActorRotation`, change Controller rotation, change AI Focus, inject movement, alter Root Motion, or use C3I/C3J smoothing. Its current local Root Motion remains relative to the current actor facing.
4. Launch is completely excluded: no change to its snapshot, turn task, commit Notify, velocity, takeoff, airborne, LandingRecovery, or AI yaw arbitration.
5. No new GameplayTag, event, GE, damage route, GameplayCue, DataAsset, generic directional framework, input route, Config/Build.cs change, physics behavior, or asset migration is approved.

## Approved C++ And Test Slice

### Private Four-Way Selector

Amend these existing C3L private C++-only WIP files in place:

1. `Source/PolyQuest/Private/Combat/Reaction/HitReactionFourWayMontageSelector.h`
2. `Source/PolyQuest/Private/Combat/Reaction/HitReactionFourWayMontageSelector.cpp`

They define a narrow non-reflected `FHitReactionFourWayMontageSet` containing non-owning `UAnimMontage*` Front/Back/Left/Right candidates, `IsComplete()`, and `FHitReactionFourWayMontageSelector::SelectFromLocalAttackerDirection(...)`.

- The selector takes only a local direction and the four-way set. It first planarizes to XY and returns the selected candidate only when the set is complete and that planar direction is finite/non-zero; otherwise it returns `nullptr`. It has no fallback parameter.
- It does not receive a World, ASC, GameplayEffect, Actor, DataAsset, or Blueprint value; it only implements four-sector classification and pointer selection.
- It is private to the runtime module, has no `UCLASS`, `USTRUCT`, `UENUM`, generated header, `POLYQUEST_API`, Blueprint exposure, logging, allocation, Tick, or mutable state.
- It calls no transform/movement API. `FHitReactionImpactResolver` remains the existing sole owner of Context-to-local-direction resolution.

### Ability Authoring Surface And Activation

Modify only these public/private pairs:

1. `Source/PolyQuest/Public/AbilitySystem/Abilities/PlayerSmallHitReactionAbility.h`
2. `Source/PolyQuest/Private/AbilitySystem/Abilities/PlayerSmallHitReactionAbility.cpp`
3. `Source/PolyQuest/Public/AbilitySystem/Abilities/EnemySmallHitReactionAbility.h`
4. `Source/PolyQuest/Private/AbilitySystem/Abilities/EnemySmallHitReactionAbility.cpp`
5. `Source/PolyQuest/Public/AbilitySystem/Abilities/PlayerBigHitReactionAbility.h`
6. `Source/PolyQuest/Private/AbilitySystem/Abilities/PlayerBigHitReactionAbility.cpp`
7. `Source/PolyQuest/Public/AbilitySystem/Abilities/EnemyHitReactionAbility.h`
8. `Source/PolyQuest/Private/AbilitySystem/Abilities/EnemyHitReactionAbility.cpp`

For each Ability:

1. Remove the legacy single-Montage UPROPERTY from every Ability header and all runtime references to it:
   - Small: `SmallHitReactionMontage`.
   - Player Big: `BigHitReactionMontage`.
   - Enemy Big: `HitReactionMontage`.
   Do not rename, modify, or delete any Content asset while removing these native fields.
2. Retain the four existing `EditDefaultsOnly, BlueprintReadOnly` private `TObjectPtr<UAnimMontage>` directional fields with `AllowPrivateAccess`, and revise their Chinese authoring ToolTips where needed. Every ToolTip must explicitly state "攻击者位于受击者本地[前/后/左/右]方时播放", state that the name does not describe victim displacement, and state that all four fields are required together. Use these exact naming families:
   - Small: `FrontSmallHitReactionMontage`, `BackSmallHitReactionMontage`, `LeftSmallHitReactionMontage`, `RightSmallHitReactionMontage`.
   - Player Big: `FrontBigHitReactionMontage`, `BackBigHitReactionMontage`, `LeftBigHitReactionMontage`, `RightBigHitReactionMontage`.
   - Enemy Big: `FrontHitReactionMontage`, `BackHitReactionMontage`, `LeftHitReactionMontage`, `RightHitReactionMontage`.
3. `ValidateActivationSetup()` must require all four directional fields through the private complete-set check. Missing authoring configuration fails activation clearly before task construction; partial configuration is invalid and has no degraded behavior.
4. In `UPlayerSmallHitReactionAbility::ActivateAbility` and `UEnemySmallHitReactionAbility::ActivateAbility`, retain the named fourth parameter `const FGameplayEventData* TriggerEventData`. Do not alter the virtual signature.
5. After basic Avatar/ASC/AnimInstance preflight and before `UAbilityTask_PlayMontageAndWait` construction, resolve the local direction with `FHitReactionImpactResolver::ResolveImpactDirection(*TriggerEventData, Avatar)` when an event exists. Build the private four-way set from this Ability's UPROPERTY fields, then select the actual Montage through the private selector.
6. Create the task and set `ActiveMontage` only after a non-null selection. A failed selection follows the existing immediate activation-failure cleanup with no task construction. Startup validation and diagnostics must refer to `ActiveMontage.Get()`; in particular, the failed-start Warning must print `*GetNameSafe(ActiveMontage.Get())` rather than a removed legacy member.
7. In `UPlayerBigHitReactionAbility` and `UEnemyHitReactionAbility`, preserve the existing pre-task `ImpactDirectionSnapshot` write from the same resolved value. Do not add snapshots to Small.
8. Preserve every other lifecycle ordering, delegate binding, cancellation path, movement restoration rule, tag count, and StateTree/AI interaction unchanged.

### Execution Order

1. Amend the existing private selector to remove its fallback parameter and verify its includes remain private to the runtime module.
2. Remove the four legacy UPROPERTY declarations and their runtime references, then make the existing sixteen directional properties a complete mandatory authoring set through validation and ToolTips.
3. Integrate no-fallback selection in the two Small Abilities, then in the two Big Abilities; preserve each Big snapshot before task creation as part of that same edit.
4. Add the pure selector coverage to `HitReactionAutomationTests.cpp` and retain all pre-existing resolver/event tests.
5. Run static checks, inspect the scoped diff, and return the prescribed evidence without editing assets, documentation, staging, or committing.

### Native Automation

Modify only `Source/PolyQuest/Private/Tests/HitReactionAutomationTests.cpp`.

1. Include the new private selector and construct transient Front/Back/Left/Right objects with `NewObject<UAnimMontage>(GetTransientPackage())`; do not load or depend on Content assets.
2. Extend the existing impact-resolver section to cover selector behavior:
   - four cardinals;
   - exact four diagonal ties under the frozen X-axis rule;
   - samples immediately on both sides of each boundary;
   - unnormalized planar input and non-zero Z;
   - incomplete sets and a missing selected member;
   - zero, near-zero, NaN, and Inf input returning `nullptr`.
3. Assert pointer identity for each valid selected dummy Montage. Assert `IsComplete()` rejects every incomplete set and the selector returns `nullptr` for incomplete or invalid input; no test may expect a fallback. Existing Context/instigator/ImpactNormal resolver tests remain intact and prove the preceding Context-to-local-direction contract.
4. Do not add test seams, Blueprint test assets, test-only production fields, or a world/animation-playback fixture. Actual slot, Skeleton, Root Motion, and visual Montage selection remain Editor/PIE gates.

## User-Owned Asset Authoring

1. No asset change is authorized in this amendment. The user's existing `AM_SmallHitReaction_F` and `AM_BigHitReaction_F` are the Front entries of their respective directional sets; they are not deleted, renamed, or reassigned by C++.
2. The user owns verification that all four directional fields are assigned in each GA. Small retains its current overlay Slot/blend contract, while Big retains its current full-body Slot and Root Motion contract; no Montage may turn the Actor manually.
3. The four GA assets remain the authoring surface:
   - `Content/_Abilities/Player/HitReaction/GA_PlayerSmallHitReaction.uasset`
   - `Content/_Abilities/Player/HitReaction/GA_PlayerBigHitReaction.uasset`
   - `Content/_Abilities/Enemy/HitReaction/GA_EnemySmallHitReaction.uasset`
   - `Content/_Abilities/Enemy/HitReaction/GA_EnemyBigHitReaction.uasset`
4. The old single default fields must no longer appear after recompilation. Do not solve a missing/invalid directional assignment through C++ asset-path logic, retargeting automation, another generic fallback, or a Front substitution.

## Validation Matrix

### Static / Executor Evidence

1. Read final diffs and direct callers/callees; confirm all changes stay inside the eleven approved source/test files.
2. Run Rider `get_file_problems` or `lint_files` on all touched C++ files; resolve newly introduced diagnostics.
3. Run `git diff --check` and report the exact changed paths.
4. Do not invoke UBT, Rider build, Editor compilation, PIE, live Editor writes, asset saves, staging, or commit.

### User Gates

1. Compile `PolyQuestEditor (Development Editor)` in Visual Studio 2022.
2. In Editor, read back each GA's four required directional fields, confirm the removed legacy single field no longer appears, then verify Montage slot configuration and Root Motion configuration.
3. Run `PolyQuest.Combat.HitReaction`, `PolyQuest.Enemy.RootMotionFacing`, `PolyQuest.Combat.LaunchFacingSmoothing`, and `PolyQuest.Combat.HitFeedback`; then run the wider existing combat/reaction regression matrix if the focused suite is clean.
4. In `Scene01`, test Player and Enemy as victims from front/back/left/right plus all four diagonals for Small and Big:
   - Small selects the correct overlay reaction, retains movement/action behavior, and does not rotate the Actor.
   - Big selects the correct full-body reaction, has no pre-turn, preserves grounded Root Motion behavior, avoids ledge/falling regressions, and restores existing control/AI state at completion.
   - Launch still smooth-turns only through C3J and launches away from the attacker; it shows no C3L montage-selection regression.

## Documentation, Debt, And Commit Boundary

1. After user validation and Main fresh review, Main updates `ARCHITECTURE.md`, `ROADMAP.md`, `README.md`, and this plan's closeout record. No executor edits project documentation.
2. C3L has no accepted new runtime debt. The removal of the obsolete generic fallback is part of this stage, not a deferred migration. The conditional eight-direction follow-up is already canonically recorded in `ROADMAP.md`; it opens only with compatible diagonal assets plus PIE evidence that four-way mapping is visually inadequate.
3. The intended source commit contains only the eleven C++/test files and Main-approved documentation. All `Content/**` assets, Blueprints, GA/GE, Montages, AnimBPs, maps, Config, project files, and unrelated WIP remain excluded unless the user explicitly approves a stable asset closure.
4. No staging or commit occurs until the user explicitly authorizes it.

## Executor Stop Conditions

Stop and return evidence to Main instead of expanding scope if any of the following is required:

1. A new GameplayTag, event, Context field, GE, Config value, Build.cs dependency, input route, or public Blueprint callable API.
2. A change to Launch, AI yaw/Focus, CharacterMovement ownership, Root Motion policy, damage dispatch, C3K feedback, or existing reaction cancellation/teardown contract.
3. Any Content asset creation, import, retarget, reparent, save, or Blueprint graph edit.
4. Any additional production/test file beyond the whitelist, including a test-only Ability or fixture seam.
5. A missing, invalid, or incompatible required directional Montage for either fixture; report the authoring gap rather than retaining a generic fallback or substituting another direction.

## Closeout Record

- Implemented surface: the approved eleven C++/test files only. A private non-reflected `FHitReactionFourWayMontageSet` and selector own one complete Front/Back/Left/Right selection contract. The four legacy single-Montage UPROPERTY fields and fallback parameter are gone; no Content asset, Blueprint, Tag, Config, Build.cs, GameplayCue, damage route, Launch route, AI yaw policy, or C3K feedback behavior changed.
- Runtime contract: valid target-local attacker directions choose one cardinal Montage with X-axis priority on exact diagonal ties. Incomplete configuration, zero/near-zero, NaN, and Inf input fail closed before a Montage task can be created. Small remains a non-interrupting overlay; Big keeps existing grounded Root Motion, cancellation, ledge/falling cleanup, and no-pre-turn ownership.
- Validation: the user confirmed focused Editor readback/PIE and the combat/reaction Automation matrix. The final `PolyQuest.Combat.HitReaction` Editor log completed with `Success` after the P3 repair timestamps; its new selector coverage includes cardinals, ties, both boundary sides, XY projection, near-zero, non-finite directions, and every incomplete set.
- Static evidence: `git diff --check` passed. Rider inspection reported no Errors; existing project weak warnings remain, and the new selector has one non-blocking `AbsY` if-init style suggestion with no behavior or contract impact.
- Review: Main's defect-first review identified the duplicated complete-set rule and missing near-zero test; both were repaired. The final Main review and an independent `gpt-5.6-luna / xhigh` Fresh Reviewer found no P0-P2 or actionable introduced defect. The code-review graph baseline matched `47d627c` for tracked files; direct source review covered the two untracked selector files.
- Debt handoff: no new runtime debt is accepted. The existing conditional eight-direction follow-up remains canonical in `ROADMAP.md`: open it only when compatible diagonal assets exist and focused PIE demonstrates that nearest-cardinal presentation is visually inadequate.
- Commit boundary: a later source/docs commit may include only these eleven C++/test paths plus Main-owned `plan.md`, `ROADMAP.md`, `ARCHITECTURE.md`, and `README.md`. All `Content/**`, Config, project, map, Blueprint, AnimBP, Montage, GA/GE, and unrelated WIP remain excluded. No staging or commit has occurred.
