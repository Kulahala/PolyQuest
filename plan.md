# TODO-03B-4: Player Mobile Bow Draw v1

## Plan State

- Status: Completed. This record preserves the accepted B4 contract and closeout evidence; it is not a claim that mutable authored assets are reproducible from the focused source/test commit.
- Baseline: `51a17c7` (`[Feature] 完成近战武器拖尾 (Complete Melee Weapon Trail)`).
- Objective: allow the Player to move through the complete active Bow Ability lifecycle: `Draw -> Hold -> Release -> Recovery/EndAbility`, while one configured authored MoveSpeed GameplayEffect owns the whole lifecycle's pace. Fixed top-down Bow presentation remains base locomotion plus an upper-body Montage layer.
- Preserve every unrelated user WIP. Do not modify, stage, move, delete, infer behavior from, or include `Content/**`, Config, maps, Blueprints, input, AnimBPs, GA/GE/Montage assets, `.uproject`, generated output, or imported-resource changes unless the user later grants a separate explicit closure.

```text
Outer: ue-stage-workflow
Primary: ue5-cpp-gameplay
Support: ue5-blueprint-workflow, ue5-debug-validation
Route reason: the stage changes one GAS Ability's owned tags, exact MoveSpeed GE Handle lifetime, and Sprint handoff; user-owned Montage/AnimBP authoring closes the visual result.
```

```text
Plan explorers: 0
Implementation executors: 1 (Gemini only after its read-only plan review is accepted and the user explicitly authorizes execution)
Complex Executor: Gemini external executor for one frozen Bow Ability lifecycle slice
Main parallel work: none
Reason: Bow's ASC Tag, Sprint, MoveSpeed Handle, EndAbility, and Automation contracts form one lifecycle-sensitive integration. Main owns the plan, contracts, documentation, validation interpretation, fresh review, staging, and commit; Gemini may write only the frozen source/test slice.
```

## Evidence And Decisions

- `UBowDrawFireAbility` currently owns `State.Action.Attacking`, `State.Input.Block.Movement`, and `State.Input.Block.Jump`. `APlayerCharacter::DoMove()` rejects movement only through the matching movement-block tag.
- Existing `State.Action.Attacking` already keeps Sprint unavailable through `APlayerCharacter::CanAttemptSprint()` and `USprintAbility`; removing Bow's movement block must not relax the Sprint gate.
- Existing Bow activation registers a mouse-plane aim requester, which owns horizontal Capsule yaw. Valid B2 lock state remains only a B3 Release-time target preference; it must not replace Bow mouse-facing.
- `ABaseCharacter` already owns the `MoveSpeed` Attribute-to-`CharacterMovement.MaxWalkSpeed` delegate. The stage must not write `MaxWalkSpeed` directly.
- The Mobile Bow slowdown is a required authored, Infinite, non-periodic MoveSpeed GameplayEffect. Its exact multiplier is mutable Content tuning, not a native or architectural contract. `UTestMobileBowMoveSpeedGE` uses a controlled native multiplier solely to prove exact-handle lifetime and Attribute-to-CharacterMovement synchronization; it does not assert the current authored balance value.
- The user confirmed fixed top-down presentation should use an upper-body overlay rather than Aim Offset. Candidate Bow resources exist under `Content/ArcherAnimsetPro`, but their skeleton compatibility and Root Motion settings are Editor-owned facts, not established by this plan.
- No Internet lookup or old `E:\GameDevelop\Test` implementation is required. Current PolyQuest source provides the required GAS, input, Sprint, aim-facing, and movement boundaries.

## Frozen Runtime Contract

1. In `UBowDrawFireAbility`, remove only the `State.Input.Block.Movement` contribution from `ActivationOwnedTags`. Retain `State.Action.Attacking` and `State.Input.Block.Jump`; retain every ActivationBlockedTag, AbilityTag, input event, target-assist, Dodge-window, rate-window, projectile, and aim-facing contract. Do not remove the tag from project config or from any other Ability.

2. Add one authored field to `UBowDrawFireAbility`:

   ```cpp
   UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bow|Movement")
   TSubclassOf<UGameplayEffect> MobileBowMoveSpeedGameplayEffectClass;
   ```

   Add one private `FActiveGameplayEffectHandle` plus private start/clear helpers. The public reflected surface changes only by this authoring property; no Blueprint callable runtime API is added.

3. Treat the configured Mobile Bow GE as mandatory setup. Include a null-class preflight with the existing Bow setup validation. After `MontageTask` has actually started and the current Montage is confirmed active, execute one common mobile-movement start helper:

   - Resolve the configured GE CDO and apply it to the current ASC at `GetAbilityLevel()`.
   - If no CDO or no valid Handle is obtained, log a clear `LogPolyQuest` Warning and converge through the existing cancelled `EndAbility()` path. Never silently leave Bow at base movement speed.
   - Only after Bow's own GE applied successfully, call `PlayerCharacter->CancelSprintAbility()`. This prevents `Sprint -> Bow` from retaining the Sprint effect while preserving Sprint when a misconfigured Bow fails to start.

4. The same Bow Handle stays active through Drawing, Holding, Releasing, and the authored Release/Recovery tail. `OnDrawReadyEvent`, early primary release, `TriggerRelease`, Release Notify, and rate/cancel-window events must neither reapply nor remove it. No new `Recovery` enum state, section-driven GE switching, timer, or second Bow state machine is allowed.

5. `EndAbility()` must call one idempotent exact-handle clear helper before it tears down tasks. That helper removes only the Handle applied by this Bow instance from the current ASC, then invalidates it even if the ASC is unavailable. It covers normal Montage completion/blend-out, input cancel, existing Dodge cancellation, existing action/hit interruption, and any other route that already reaches Bow `EndAbility()`; it must not remove an external MoveSpeed source using the same GE class.

6. Do not introduce a Player terminal-cancellation route. Current Player death does not yet cancel active Bow/other Ability instances; that accepted debt remains owned by `TODO-03D`. This stage proves correct cleanup once `EndAbility()` is reached, not unimplemented Player death teardown.

7. Do not change `APlayerCharacter`, input mappings, Sprint Ability source, AttributeSet, Gameplay Tag config, replication, camera, lock-on, projectile target selection, gameplay costs, GameplayCue routing, root-motion policy, or movement-component configuration. Bow remains single-player/server-only as it is today.

## Approved Source And Test Surface

**Contract owner: Main. Implementation writer: Gemini only after explicit execution authorization.** Any need to modify an unlisted source/header/public API, Gameplay Tag, Input route, Config, `.uproject`, or asset is a stop condition requiring Main evidence review.

- `Source/PolyQuest/Public/AbilitySystem/Abilities/BowDrawFireAbility.h`
  - Contract owner: Main. Implementation writer: Gemini.
  - Add only the authored GE class, private Handle/helper declarations, required forward declaration/include, and narrow `WITH_DEV_AUTOMATION_TESTS` configuration/observation accessors that call the same production start/clear path.

- `Source/PolyQuest/Private/AbilitySystem/Abilities/BowDrawFireAbility.cpp`
  - Contract owner: Main. Implementation writer: Gemini.
  - Remove only the Bow movement owned-tag contribution; validate, apply, cancel Sprint, and clear the exact Handle in the order above. Keep Montage, event, Projectile, target-assist, aim-requester, and cancellation logic otherwise unchanged.

- `Source/PolyQuest/Private/Tests/TestMobileBowMoveSpeedGE.h/.cpp` (new)
  - Contract owner: Main. Implementation writer: Gemini.
  - Define a native-only, Infinite, non-periodic controlled MoveSpeed GE. It must not load or reference `Content/**`, and its test multiplier must not be described as the source of truth for authored balance.

- `Source/PolyQuest/Private/Tests/TestMobileBowSprintAbility.h/.cpp` (new)
  - Contract owner: Main. Implementation writer: Gemini.
  - Define only the minimal active Ability carrying the real `Ability.Movement.Sprint` and `State.Movement.Sprinting` contracts necessary to verify `CancelSprintAbility()` cancellation. It is not a replacement for production `USprintAbility`, has no asset/config dependency, and is test-only.

- `Source/PolyQuest/Private/Tests/PlayerMobileBowAutomationTests.cpp` (new)
  - Contract owner: Main. Implementation writer: Gemini.
  - Add the fourteenth suite, `PolyQuest.Player.MobileBow`, using the existing H3 deferred-spawn Player fixture. Do not modify fixture defaults, create a Content dependency, or create a second movement implementation.

No `PlayerCharacter`, `SprintAbility`, `CombatAutomationFixture`, `PlayerActionWindowAutomationTests`, `ProjectileLifecycleAutomationTests`, Build.cs, Config, or Content path is approved for modification in this stage.

## Automation Contract

`PolyQuest.Player.MobileBow` must use the real `UBowDrawFireAbility` instance and the production start/clear helpers through a narrow `WITH_DEV_AUTOMATION_TESTS` seam. It does not need a renderable AnimInstance or production Montage asset merely to prove GE ownership; actual Montage startup and visuals remain Editor/PIE evidence.

It must cover all of the following without `AddExpectedError`, lowered log levels, or a test-only runtime fallback:

- The Bow CDO owns `State.Action.Attacking` and `State.Input.Block.Jump`, but not `State.Input.Block.Movement`.
- With a base `MoveSpeed` of `500`, one successful mobile-Bow start produces a valid Bow Handle, the controlled fixture's expected slowed MoveSpeed, and matching `CharacterMovement.MaxWalkSpeed` through the existing delegate.
- A genuinely active test-only Sprint request is cancelled by the successful Bow start. Bow's retained Attacking contract remains the reason a new production Sprint cannot start; this stage does not rewrite Sprint preflight.
- The exact same Handle persists while test state traverses Draw, Hold, Release, and Recovery/active-Releasing representation; no duplicate active effect is added.
- Normal and cancelled `EndAbility()` each remove Bow's own Handle and restore base speed. If an independent MoveSpeed GE was already active, its resolved pre-Bow speed and its active Handle survive Bow teardown.
- All test setup injects `UTestMobileBowMoveSpeedGE` before invoking the start helper, so H3's clean successful-path signal remains intact.

Run the new suite plus the existing thirteen-suite matrix through the Unreal Editor Automation front end:

`PolyQuest.Equipment.TransactionMatrix`, `PolyQuest.Melee.TraceSourceGeometry`, `PolyQuest.Melee.WeaponTrail`, `PolyQuest.Player.ActionWindows`, `PolyQuest.Combat.HitReaction`, `PolyQuest.Enemy.AttackSetSelection`, `PolyQuest.Enemy.CombatSpacing`, `PolyQuest.UI.VitalHUD`, `PolyQuest.Player.Exhaustion`, `PolyQuest.Player.LockOn`, `PolyQuest.Projectile.Lifecycle`, `PolyQuest.Projectile.TargetAssist`, and `PolyQuest.Combat.HitFeedback`.

All fourteen suites must succeed. Existing retained warnings may only be those in the H3 intentional-negative ledger: invalid multi-tier reaction tags, no Stance Break Ability fallback, equipment preflight/rollback, and invalid static trace geometry. B4 adds no successful-path missing-GE, missing-fixture, or invalid-spec warning.

## User-Owned Editor Authoring

After source static preflight, the user owns the following Editor work and readback:

1. In `GA_Player_Bow_DrawFire`, assign `MobileBowMoveSpeedGameplayEffectClass` to the selected authored MoveSpeed GameplayEffect; confirm it is `Infinite`, has no Periodic execution, and uses the currently intended MoveSpeed modifier. The exact multiplier remains asset-owned tuning and must be read back from the Editor when it changes.

2. In `AM_Bow_Shoot`, author the continuous `Draw -> Hold(loop) -> Release -> Recovery -> End` route. The full active Montage belongs to the currently validated `DefaultGroup.UpperBody` Slot route; do not put Bow back into a full-body `DefaultSlot` track.

3. Choose compatible in-place Bow Draw/Hold/Release source sequences from `Content/ArcherAnimsetPro`. Confirm the source sequences and Montage do not generate Root Motion. Do not use Force Root Lock to hide Root Motion, and do not use RootMotion-directory Bow assets as a `CharacterMovement` walking substitute.

4. In `ABP_Player_Dungeon`, preserve existing Bow base locomotion and the existing pure-base-pose Stride Warping position. Feed that pose into the current upper-body Slot, layer the Slot output from `spine_01` with the existing Mesh Space Rotation Blend setup, then keep the existing Reaction Overlay last:

   ```text
   Bow base locomotion / Stride Warping
       -> DefaultGroup.UpperBody Bow Slot
       -> Layered Blend per Bone (spine_01)
       -> existing Reaction Overlay
   ```

   Do not add Aim Offset, a dedicated Bow Aim BlendSpace, a second equipment-state bool, or a new Slot Group. The active Slot controls presentation; Bow's existing mouse-plane yaw controls aim-facing.

5. If the existing Bow base locomotion cannot visibly support forward/backward/left/right movement while the upper body stays aimed, stop the asset closure and report the missing strafe/in-place asset evidence. Do not compensate with native movement changes, Root Motion, or a misleading upper-body blend.

These mutable assets stay outside the default source/test/document commit.

## Static Checks, User Validation, And Closeout

Before user validation, Gemini/Main must read the final changed Ability and direct callers/callees, run CodeGraph, use code-review-graph only as supplementary diff/impact evidence when its index covers the baseline, run Rider error-level inspection on every touched C++ path, and run `git diff --check`. These are static checks only and are not compile, Editor, PIE, or visual proof.

User validation:

- Manually compile `PolyQuestEditor`.
- Run the fourteen Automation suites above from the Editor front end and preserve the raw result/log excerpt.
- In `Scene01`, equip Bow and verify: Sprint then Bow starts at the configured authored pace; Draw/Hold/Release/Recovery all retain that pace and allow camera-relative eight-direction movement; cursor-facing remains stable; Jump and new Sprint are blocked; Release-time lock/target assist and projectile behavior are unchanged.
- Verify the upper body draws/holds/releases while the lower body walks, no Root Motion translates the Character, no torso twist or foot-slide regression is introduced, and Reaction Overlay remains visible.
- Verify normal release, input cancel, and an existing valid Dodge cancel restore the non-Bow speed. Do not claim Player death teardown is supported until `TODO-03D` supplies its terminal route.

After user-confirmed compile, Automation, and PIE evidence, Main performs one defect-first fresh review. Then Main updates `README.md`, `ARCHITECTURE.md`, `ROADMAP.md`, and this `plan.md` with only verified results. `ROADMAP.md` must update B4 wording from Draw/Hold-only movement to the complete active Bow lifecycle; its existing `TODO-03D` terminal-Ability debt remains canonical.

## Commit Boundary

Default commit includes only the two Bow Ability files, the four new test helper files, `PlayerMobileBowAutomationTests.cpp`, the scoped `MeleeWeaponTrailAutomationTests.cpp` Unity-build local-helper rename, and the four project documents after verified closeout. Explicitly exclude all `Content/**`, Config, map, Blueprint, GA/GE/Montage, AnimBP, input, `.uproject`, generated output, and unrelated WIP. No commit occurs until the user explicitly approves it.

## Closeout Record

- Runtime ownership: `UBowDrawFireAbility` no longer owns `State.Input.Block.Movement`, but retains `State.Action.Attacking` and `State.Input.Block.Jump`. After its Montage is confirmed active, it applies one exact handle from the mandatory authored `MobileBowMoveSpeedGameplayEffectClass`, then cancels only an active Sprint. The same handle remains through Draw, Hold, Release, and Recovery; every existing path reaching `EndAbility()` removes only that handle before task, aim-requester, scoped-tag, and Montage cleanup. No direct `MaxWalkSpeed` write, new Bow state machine, input route, targeting behavior, or terminal-death route was added.
- Automation: `PolyQuest.Player.MobileBow` uses a real ASC-hosted Bow Ability plus native-only controlled MoveSpeed and Sprint fixtures. It covers Tag ownership, successful and missing-GE start behavior, MoveSpeed-to-CharacterMovement synchronization, Sprint cancellation, handle idempotency across Bow states, normal/cancelled `EndAbility()` cleanup on independent instances, and survival of an external MoveSpeed handle. The controlled fixture validates lifecycle mechanics only; it does not duplicate mutable GE balance tuning from `Content/**`. The committed `MeleeWeaponTrailAutomationTests.cpp` change only renames its local world-cleanup type to avoid a Unity-build collision; it changes no trail test behavior.
- User evidence: the user confirmed focused PIE behavior and the fourteen-suite Unreal Editor Automation matrix as Success after the final repair. The authored GE, Montage, AnimBP, Blueprint, and input settings remain user-owned local `Content/**` WIP and are excluded from this commit.
- Static and fresh review: Main re-read the final Ability lifecycle, `CancelSprintAbility()` caller boundary, test fixture, and direct cleanup paths; CodeGraph and code-review-graph supplied supplemental source/impact context, while direct source review covered UE Automation macro and untracked-file gaps. Rider lint reported no Error. `git diff --check` reported no whitespace defect. Main's defect-first fresh review found no P0-P2.
- Tuning source of truth: the local authored Mobile Bow GE was retuned after the original x0.6 plan assumption. This closeout intentionally records no durable numeric multiplier; current pace is read from the authored asset and verified in focused PIE, while the native fixture remains an isolated control value.
- Scope: the approved commit contains only B4 C++, Automation, this closeout record, and synchronized `README.md`, `ARCHITECTURE.md`, and `ROADMAP.md`. All `Content/**`, Config, maps, Blueprints, GA/GE/Montage, AnimBP, input, `.uproject`, generated output, imported resources, and unrelated user WIP stay out.
