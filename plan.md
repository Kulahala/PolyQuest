# TODO-03B-5: Bow Hold Locomotion Presentation v1

## Plan State

- Status: Completed / user-validated / ready for documentation closeout and commit.
- Baseline: `4144e15` (`[Docs] Schedule Bow Hold Locomotion Stage`). Preserve the existing user-owned `Content/**` WIP and the separate unstaged `AGENTS.md` review-policy wording change.
- Objective: keep Bow locomotion selected for the active Draw -> Hold -> Release action while making the UpperBody Montage bypass Hold-only, so the authored Draw and Release portions of `AM_Bow_Shoot` remain visible during movement.
- Player-facing success: a moving player visibly draws the bow, uses the Bow locomotion route while the Bow action is active, suppresses the UpperBody layer only during Hold, and visibly releases the arrow. Early release, cancellation, interruption, and failed activation leave no residual Charging state.

## Route And Delegation

Outer: `ue-stage-workflow`
Primary: `ue5-cpp-gameplay`
Support: `ue5-blueprint-workflow`
Route reason: the runtime fix is a narrow existing-GAS-tag lifecycle adjustment in one Ability, paired with one user-authored AnimBP condition change. It does not require a new gameplay framework, input path, tag taxonomy, or animation system.

Plan explorers: 0
Implementation executors: 1 (Gemini; completed the frozen native/test slice)
Complex Executor: none
Main parallel work: none
Reason: the allowed native slice is small and its lifecycle contract is frozen. Gemini can implement the exact private C++/test edits, while Main retains the shared tag contract, asset boundary, acceptance, documentation, staging, and commit ownership.

### Ownership And Handoff Boundary

- Contract owner: Main. `State.Action.Attacking`, `State.Action.Charging`, Bow phase semantics, input/cancel ownership, asset routing contract, test acceptance, and documentation remain Main-owned.
- Implementation writer: Gemini. It may modify only the three approved native/test files below, and only the explicitly named functions/tests.
- User-owned Editor work: `ABP_Player_Dungeon`, `AM_Bow_Shoot` readback, all Blueprint/Montage saves, manual `PolyQuestEditor` compilation, Automation execution, and PIE.
- No executor may modify `AGENTS.md`, `ROADMAP.md`, `ARCHITECTURE.md`, `README.md`, `plan.md`, `Config/**`, `Content/**`, `PlayerCharacter`, projectile code, or any public header.

## Existing Evidence

1. `UBowDrawFireAbility` is `InstancedPerActor` and ServerOnly. It owns `State.Action.Attacking` through `ActivationOwnedTags`, so that tag lasts for the entire active Bow Ability, not merely its Hold phase.
2. The existing Ability validates `Draw`, `Hold`, and `Release` Montage sections and creates the semantic `Event.Attack.Bow.DrawReady` / `Event.Attack.Bow.Release` listeners before starting `AM_Bow_Shoot` at `Draw`.
3. The final Ability applies the mobile MoveSpeed GE and enters `Drawing` without Charging. A valid `OnDrawReadyEvent()` changes to `Holding` and applies Charging; `TriggerRelease()` removes Charging before entering `Releasing` and jumping to `Release`; `EndAbility()` remains idempotent.
4. User Editor readback established two independent authored booleans in `ABP_Player_Dungeon`: `Is Bow Aiming` is `ResolvedLocomotionMode == Bow && State.Action.Attacking` and selects the Bow locomotion BlendSpace; `Is Bow Holding` is `ResolvedLocomotionMode == Bow && State.Action.Charging` and participates in the UpperBody `AllowBlend` gate. The prior single predicate caused Draw and Release presentation to be suppressed.
5. `State.Action.Charging`, `Event.Attack.Bow.DrawReady`, and `Event.Attack.Bow.Release` are already valid project tags. No Config edit is needed.
6. Existing native fixtures already expose Bow state, `TestOnDrawReadyEvent`, `TestOnInputReleased`, `TestSetCharging`, and ASC tag state. `ProjectileLifecycleAutomationTests` already constructs a valid mock Bow Montage and exercises DrawReady/InputReleased event identity gates; `PlayerMobileBowAutomationTests` already proves one MoveSpeed GE handle persists through representative Draw/Hold/Release states and is cleaned by both normal and cancelled `EndAbility()` paths.

## Frozen Runtime Contract

### Phase-To-Tag And AnimBP Meaning

| Bow phase | `State.Action.Attacking` | `State.Action.Charging` | `Is Bow Aiming` (locomotion) | `Is Bow Holding` (layer gate) | UpperBody `AllowBlend` |
| --- | --- | --- | --- | --- | --- |
| Drawing | Present | Absent | True in Bow mode | False | True |
| Holding after valid DrawReady | Present | Present | True in Bow mode | True | False |
| Releasing | Present | Absent | True in Bow mode | False | True |
| Inactive, cancelled, interrupted, failed | Absent after GAS cleanup | Absent | False | False | Normal non-Bow route |

1. `State.Action.Attacking` remains the full Draw -> Hold -> Release action-arbitration tag and continues to drive `Is Bow Aiming`/Bow locomotion in Bow mode. It must not become a Hold-only tag and no cancellation/block matrix changes are approved.
2. `State.Action.Charging` is the existing dynamic Bow Hold presentation gate and drives `Is Bow Holding`; it is not added during Drawing, and no new GameplayTag or tag hierarchy is introduced.
3. In `OnDrawReadyEvent()`, after the existing active-Montage/identity validation succeeds, set `BowState` to `Holding`. If `bReleaseRequested` is already true, call the existing `TriggerRelease()` immediately and return without adding Charging. Otherwise call `SetCharging(true)` exactly once.
4. Normal Holding input release continues through the existing `TriggerRelease()` path, which removes Charging before `BowState = Releasing` and before the Montage Section jump. `OnReleaseAnimEvent()` and projectile spawning are unchanged.
5. `EndAbility()` remains the one idempotent terminal cleanup owner. It must still remove only this Bow Ability's MoveSpeed effect handle, remove Charging when present, clear cancellation/rate state, unregister the aim requester, stop/end its tasks, and reset phase state.
6. The mobile MoveSpeed GE and `RegisterBowAimRequester()` retain their entire active-Ability lifetimes. This stage changes presentation gating only; it does not change move eligibility, aim facing, target assist, projectile direction/snapshot, Dodge windows, Jump block, or Sprint cancellation.

### AnimBP Contract

1. In `ABP_Player_Dungeon`, retain `Is Bow Aiming = (ResolvedLocomotionMode == Bow && HasMatchingGameplayTag(State.Action.Attacking))` for Bow locomotion selection through Draw, Hold, and Release.
2. Use the separate `Is Bow Holding = (ResolvedLocomotionMode == Bow && HasMatchingGameplayTag(State.Action.Charging))` only for the Hold presentation gate.
3. Keep the authored `AllowBlend` relation `NOT (Is Shield Guarding OR Is Bow Holding)`. Draw and Release therefore keep the established UpperBody Montage route, while Holding alone bypasses that layer and presents `BS_Bow_Aim_Walk_Run` full body.
4. Do not create a new AnimBP state machine, Montage-time polling, section-name inference, duplicate BlendSpace, or a third Bow locomotion/state boolean.
5. `AM_Bow_Shoot` and `BS_Bow_Aim_Walk_Run` are validation surfaces, not native source edits. The user confirmed `DrawReady` at the Draw -> Hold boundary and the Release event in the Release segment before accepting the asset side.

## Approved Native And Asset Slice

### Gemini C++ / Test Whitelist

1. `Source/PolyQuest/Private/AbilitySystem/Abilities/BowDrawFireAbility.cpp`
   - Contract owner: Main; implementation writer: Gemini.
   - Allowed functions: `ActivateAbility()` and `OnDrawReadyEvent()` only.
   - Remove the post-MoveSpeed `SetCharging(true)` from activation. Add the Hold-only call in the valid DrawReady path after the early-release branch. Do not modify `SetCharging()`, `TriggerRelease()`, `EndAbility()`, public API, tags, task creation, input routing, or asset references.

2. `Source/PolyQuest/Private/Tests/ProjectileLifecycleAutomationTests.cpp`
   - Contract owner: Main; implementation writer: Gemini.
   - Extend the existing Bow event-gate section using its current transient Montage, Player/ASC, and existing test helpers. Explicitly set the event-test Bow instance's current ActorInfo before tag assertions rather than relying on whether the ASC supplied a primary instance. Assert invalid DrawReady inputs retain Drawing and no Charging; valid DrawReady produces Holding plus Charging; normal Holding release produces Releasing with Charging removed.
   - The Early Release case must use a fresh `NewObject<UBowDrawFireAbility>` rooted on the current test Player, set its current ActorInfo and mock BowMontage, put it in Drawing, then send valid InputReleased followed by valid DrawReady. Assert Releasing plus an absent Charging tag. Do not reuse a preceding event-case instance because `bReleaseRequested` is private state reset only by the real activation/terminal lifecycle.
   - Do not add a production/test-only API or new fixture class.

3. `Source/PolyQuest/Private/Tests/PlayerMobileBowAutomationTests.cpp`
   - Contract owner: Main; implementation writer: Gemini.
   - Add the existing Charging tag to the test's resolved-tag preflight. Before each of the normal and cancelled `EndAbility()` calls in Section 6, use existing test helpers to establish Charging, assert it is present, then assert the same ASC no longer has Charging immediately after that exact terminal route. Preserve all MoveSpeed handle persistence and external-GE survival assertions.

4. `Source/PolyQuest/Private/Character/Player/PlayerCharacter.cpp`
   - Contract owner: Main; implementation writer: Main under the user's explicit adjacent-presentation authorization.
   - The only accepted change is the camera follow tuning `CameraLagSpeed: 18.0f -> 8.0f` and `CameraLagMaxDistance: 75.0f -> 0.0f`. It is documented as an independent camera presentation adjustment, not part of Bow GAS ownership or phase semantics. No other Player, camera, input, or facing behavior is in scope.

### User-Owned Assets

1. `Content/BP/Characters/Player/Animations/ABP_Player_Dungeon.uasset`
   - User-authored result: retain `Is Bow Aiming = (Bow locomotion mode && State.Action.Attacking)` for locomotion, add/retain `Is Bow Holding = (Bow locomotion mode && State.Action.Charging)` for the Hold-only layer gate, and keep `AllowBlend = NOT (Is Shield Guarding OR Is Bow Holding)`.

2. `Content/BP/Montages/Bow/AM_Bow_Shoot.uasset`
   - Readback only. Confirm the existing `Draw`, `Hold`, and `Release` sections plus semantic DrawReady/Release notifies. Do not retime, replace, retarget, or otherwise edit it unless that confirmation exposes a separate asset defect for Main scope review.

3. `Content/_Animations/Weapon/Bow/Locomotion/BS_Bow_Aim_Walk_Run.uasset`
   - Readback only. No sample, speed, blend, or axis adjustment belongs to this stage.

## Execution Order

1. Gemini reads the current three whitelisted C++/test files and verifies the frozen calls/fixtures before editing.
2. Move the Charging onset from activation to valid non-early-release DrawReady, then re-read the complete Ability lifecycle to prove release and terminal cleanup remain unmodified.
3. Extend `ProjectileLifecycle` phase/event assertions and `PlayerMobileBow` terminal-cleanup assertions without adding headers, tags, or fixtures.
4. Gemini performs Rider inspection and scoped static checks, returns changed paths plus self-review evidence, and stops without touching assets, docs, staging, or commits.
5. User compiles native code, authors the two AnimBP booleans and `AllowBlend` relation, confirms the Montage event placement, runs focused Automation, and performs PIE. The user also confirmed the adjacent camera tuning through Editor readback.
6. After accepted validation, Main performs one independent defect-first fresh review under the project review override, synchronizes documentation, and waits for explicit commit approval. A strict/adversarial review is additional work only if the user explicitly requests it.

## Validation Matrix

### Gemini Static Evidence

1. Read the final three-file diff and direct `UBowDrawFireAbility` callers/callees.
2. Run Rider `get_file_problems` or `lint_files` for both touched test files and `BowDrawFireAbility.cpp`; resolve newly introduced errors or warnings.
3. Run `git diff --check` and report exact changed paths. Do not run UBT, Visual Studio/Rider compilation, live Editor operations, PIE, staging, or commits.

### User Compile And Editor Readback

1. Compile `PolyQuestEditor (Development Editor)` in Visual Studio 2022.
2. In `AM_Bow_Shoot`, confirm valid `Draw`, `Hold`, and `Release` sections; verify `Event.Attack.Bow.DrawReady` fires at Draw completion and `Event.Attack.Bow.Release` remains in Release.
3. In `ABP_Player_Dungeon`, compile the AnimBP and confirm `Is Bow Aiming == (Bow locomotion mode && State.Action.Attacking)`, `Is Bow Holding == (Bow locomotion mode && State.Action.Charging)`, and `AllowBlend == NOT (Is Shield Guarding OR Is Bow Holding)`.
4. Confirm the user-read-back `CameraBoom` values are `CameraLagSpeed = 8.0f` and `CameraLagMaxDistance = 0.0f`; this is an adjacent presentation tuning check only.

### Automation And PIE

1. Run `PolyQuest.Projectile.Lifecycle`, `PolyQuest.Player.MobileBow`, and `PolyQuest.Player.ActionWindows`.
2. Regress `PolyQuest.Projectile.TargetAssist` and `PolyQuest.Player.LockOn`, because their Bow-facing/Release snapshot contracts are deliberately unchanged.
3. In Scene01, validate:
   - hold LMB while moving: Draw pose plays first, then Hold uses Bow Aim locomotion;
   - normal release from Hold: Release pose is visible and arrow delivery remains correct;
   - early release during Draw: Draw completes into immediate Release without a visible/stale Aim locomotion phase;
   - cancel/interruption: no residual Charging tag, Bow Aim locomotion, MoveSpeed effect, or aim requester;
   - locked and unlocked target-assist shots preserve their current direction/target snapshot behavior.

## Documentation, Debt, And Commit Boundary

1. User confirmed the focused Bow Automation matrix and Scene01 PIE; Main's independent defect-first fresh review found no P0-P2 or actionable P3 defect. Main has updated this plan's closeout record, marked `TODO-03B-5` done in `ROADMAP.md`, and synchronized `ARCHITECTURE.md`/`README.md` only for stable, implemented behavior.
2. No new debt is accepted. If the DrawReady notify is absent, misplaced, or incompatible with the current montage, that is an authoring blocker for this stage, not permission to infer phases from Montage time or introduce a second runtime state source.
3. The source/docs commit includes the three native/test files, the explicitly authorized `PlayerCharacter.cpp` camera tuning, and Main-owned documentation. `ABP_Player_Dungeon.uasset`, all other `Content/**` assets, Config, project files, maps, Blueprints, Montages, GA/GE, and unrelated WIP remain excluded by default. The current `AGENTS.md` policy change is a separate documentation change and is not included.
4. The user has explicitly authorized staging and commit after this closeout.

## Gemini Stop Conditions

Return evidence to Main without expanding scope if any of the following is needed:

1. A public header/test seam, new GameplayTag, Config edit, GameplayEffect, input route, PlayerCharacter edit, or any change outside the three native/test files.
2. A modification to `SetCharging()`, `TriggerRelease()`, `EndAbility()`, target assist, projectile spawning, aim requester lifetime, CharacterMovement, or cancellation ownership.
3. Any asset save, Montage/BlendSpace retime, AnimBP graph edit, import, retarget, Blueprint change, staging, commit, compilation, or PIE operation.
4. A fixture limitation that makes the required state assertions impossible without adding a new production/test type; report it for Main scope decision rather than working around it.
5. A missing or invalid DrawReady/Release authored event, section, or Editor routing condition; report the exact asset gap for user/Main resolution.

## Closeout Record

- Implemented surface: `BowDrawFireAbility.cpp` now adds `State.Action.Charging` only after a valid DrawReady event, keeps the existing `State.Action.Attacking` lifetime tag, and preserves the existing Release/EndAbility cleanup. `ProjectileLifecycleAutomationTests.cpp` and `PlayerMobileBowAutomationTests.cpp` cover invalid event gates, normal Hold release, early release, and exact Charging cleanup. `PlayerCharacter.cpp` contains only the separately authorized CameraBoom lag tuning.
- Runtime/AnimBP contract: `Is Bow Aiming` remains the Attack-lifetime Bow locomotion selector; `Is Bow Holding` is the Hold-only Charging selector; `AllowBlend` bypasses the UpperBody layer only for Bow Holding (or the existing Shield Guard route). Draw and Release keep their authored UpperBody Montage presentation while movement remains available.
- Validation: the user confirmed focused Automation and Scene01 PIE. Editor readback confirmed the two Boolean wiring and camera values (`8.0f` lag speed, `0.0f` max distance); no separate Main compilation claim is made here. Main's independent defect-first fresh review found no P0-P2 or actionable P3 defect. Main static evidence includes scoped CodeGraph/code-review-graph review, Rider diagnostics previously returning no errors on touched C++, and `git diff --check`.
- Asset boundary: `ABP_Player_Dungeon.uasset` and other authored Content remain local mutable WIP and are intentionally excluded from the source/docs commit; the commit does not claim clean-checkout reproduction of the Editor fixture.
- Debt handoff: no new Bow runtime debt accepted. A single active Bow Ability still owns its existing target-assist, projectile, Action Window, and cleanup contracts; camera tuning is independent presentation data.
- Commit boundary: stage source (`BowDrawFireAbility.cpp`, the two Bow test files, and authorized `PlayerCharacter.cpp`) plus `plan.md`, `ROADMAP.md`, `ARCHITECTURE.md`, and `README.md`; preserve `AGENTS.md`, all Content/Config/project WIP, and unrelated changes. No asset, Config, or map files are staged.
