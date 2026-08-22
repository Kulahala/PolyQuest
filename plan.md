# TODO-01C4: Player Reaction Recovery Dodge Cancel Windows v1

## Plan State

- Status: Complete. Documentation closeout and scoped commit approved by the user.
- Baseline: `c65c222` ([Feature] 完成击飞、空中与落地受击 / Launch Airborne Landing Reactions).
- Objective: let the player cancel only the authored middle-late window of Launch `LandingRecovery` into the existing normal-cost Dodge. The player must remain unable to Dodge during Takeoff, AwaitingAirborne, Airborne, recovery before the window, and recovery after the window.
- Current `Content/**`, maps, imported assets, authored GA/GE/Montage/AnimBP/Blueprint/input assets, `AGENTS.md`, `Config/Automation/**`, and `Config/Tests/**` are user-owned WIP. Preserve and exclude them from this stage's future source/document commit unless the user explicitly approves an asset closure.

~~~text
Outer: ue-stage-workflow
Primary: ue5-cpp-gameplay
Support: ue5-debug-validation
Route reason: C4 changes the shared GAS action-cancellation contract between Player Launch and Dodge, reuses an existing AnimNotifyState event bridge, and needs focused lifecycle and input-cost validation.
~~~

~~~text
Plan explorers: 0
Implementation executors: 1 (Gemini, user-coordinated bounded executor)
Complex Executor: one scoped lifecycle-sensitive implementation
Main parallel work: source integration review, validation interpretation, fresh/adversarial review, documentation closeout, and commit preparation after user approval.
Reason: the user explicitly assigned Gemini the exact C4 source/test slice. Main retains the accepted action-arbitration contract, public/tag integration ownership, user coordination, final review, documentation, staging, and commit ownership; Gemini may not broaden or redesign the contract.
~~~

## Locked Product Contract

1. C4 is Player-only and modifies only the existing C3G `LandingRecovery` phase. Enemy Launch, airborne Dodge, generic reaction cancellation, new input bindings, buffering, Guard/Parry cancellation, and a new action state are out of scope.
2. The user will add exactly one existing `UAnimNotifyState_ActionDodgeCancelWindow` placement to `Content/BP/Montages/Takeoff/AM_LaunchLandingRecovery`:
   - Open at roughly 55 percent of the Montage, on the first convincing mid-late get-up frame after the torso has clearly left the prone pose.
   - Close at roughly 90 percent, leaving the final settle/stand beat non-cancelable.
   - Do not place it on `AM_LaunchTakeoff` and do not add a Dodge invulnerability Notify to LandingRecovery. A successful `GA_Dodge` continues to own its normal Montage, cost, movement, and invulnerability lifecycle.
   - The shared Montage may emit the existing event on an Enemy, but Enemy Launch must not listen to or act on it.
3. A press/release outside the authored window is rejected immediately and is not buffered. Inside the window, the ordinary DodgeSprint short-press path must pass the existing grounded and Stamina preflight and commit the normal Dodge cost before it cancels Launch. C4 deliberately preserves the current normal-Dodge soft-overspend rule: any current `Stamina > 0` passes `UStaminaActionAbility::CheckCost()`, the authored cost may drain it to the AttributeSet-clamped `0`, and C4 must not add a `Stamina >= authored cost` affordability gate.
4. `State.Action.CanCancel.Dodge` remains the sole temporary cancellation permission. Player Launch owns one scoped loose-tag contribution during a valid matching recovery window and removes exactly that contribution on window end or any Launch cleanup. It must never grant `State.Action.CanCancel.Defense`.
5. Existing precedence remains: Dead, Stunned, Guard Break, Stance Break, renewed Falling, teardown, and a failed Dodge cost do not create a partial cancellation. A successful Dodge cancels only the active Player Launch through normal `EndAbility()` cleanup; C3H remains the sole future owner of post-landing deferred Guard/Stance resolution.

## Approved Native Surface

### Player Launch reaction

- Update `Source/PolyQuest/Public|Private/AbilitySystem/Abilities/PlayerLaunchReactionAbility.*`.
- Keep `Ability.Dodge` in `AbilitiesToCancel` so an incoming Launch still cancels an already active Dodge, but remove it from `BlockAbilitiesWithTag`; the latter would otherwise suppress the only legal C4 Dodge activation.
- Add the existing Dodge CancelWindow Begin/End tags and two persistent `UAbilityTask_WaitGameplayEvent` tasks. Create, bind, and activate them before Takeoff playback, then ignore all events unless the ability is in `LandingRecovery`.
- Add a private LandingRecovery Montage identity check with the established owner plus direct-Montage-or-Slot-sequence policy. It must also require the bound AnimInstance to report `LandingRecoveryMontage` active. Wrong Avatar, wrong Montage, stale Takeoff event, missing `OptionalObject`, an inactive/stopped recovery montage, duplicate Begin, End before Begin, interruption, or teardown must fail closed without changing the ASC tag count.
- Add a scoped `SetDodgeCancelable(bool)` helper and a transient owner flag. `EndAbility()` must clear the permission before ending its event tasks and before stopping its Montage; it must include both new tasks in its existing unified cleanup.
- Update activation preflight to expect 10 blocked tags and 11 cancellation tags. Add only the minimum `WITH_DEV_AUTOMATION_TESTS` hooks needed to exercise LandingRecovery window identity and cleanup without production or Blueprint exposure.

### Dodge arbitration

- Update `Source/PolyQuest/Public|Private/AbilitySystem/Abilities/DodgeAbility.*`.
- Request and validate `State.Action.HitReacting` plus `Ability.Reaction.Player.Launch` in both construction and activation setup. Extend `CanActivateAbility()` so Attacking, Dodging, or HitReacting each require `State.Action.CanCancel.Dodge`; grounded, Stamina, Dead, Stunned, Exhausted, and Parry gates stay unchanged.
- After the existing `CommitAbility()` succeeds, include `Ability.Reaction.Player.Launch` in Dodge's conditional cancellation container alongside legal attack cancellation. Do not cancel Launch before cost commit succeeds.
- Preserve UE 5.8 retrigger behavior: when the first C4 Dodge has cancelled LandingRecovery and consumed its window tag, a second rapid request during Dodge startup remains rejected until Dodge's own authored recovery window opens.

### Tests and excluded code

- Update `Source/PolyQuest/Private/Tests/PlayerActionWindowAutomationTests.cpp` and `Source/PolyQuest/Private/Tests/HitReactionAutomationTests.cpp` only.
- No change is approved for `PolyQuestGameplayTags.ini`, `APlayerCharacter`, Enemy code, StateTree, CharacterMovement, AnimNotify source, Build.cs, input mappings, GE/GA asset files, or `.uasset`/`.umap` contents through filesystem edits.

## Automation And Validation

### Native/static

- `PolyQuest.Player.ActionWindows` must prove: HitReacting without the tag rejects Dodge; a valid C4 tag permits the grounded, Stamina-positive preflight; a low positive Stamina value still permits the same normal-Dodge soft-overspend route; exact zero Stamina and Falling both reject; and pre-existing Attack/Dodge recovery chaining does not regress.
- The same suite must exercise a transient Player Launch instance with a synthetic LandingRecovery Montage: invalid Avatar/Montage/Sequence, inactive recovery Montage, and wrong phase do nothing; valid Begin grants only `CanCancel.Dodge`; duplicate Begin does not accumulate; valid End and forced `EndAbility()` remove the owned tag once.
- `PolyQuest.Combat.HitReaction` must assert Player Launch blocks exactly ten abilities without Dodge, preserves all other blocked tags, and still cancels all eleven targets including Dodge.
- Before user validation, inspect direct callers/callees and tag requests; run Rider Error-level lint for the four changed ability/test files and `git diff --check`. Use code-review-graph only as supplemental diff evidence when its service is available.

### User-owned Editor, compile, and PIE

1. Read back `GA_PlayerLaunchReaction` uses the native Player Launch class and `AM_LaunchLandingRecovery`; confirm exactly one `ActionDodgeCancelWindow` placement at the locked mid-late interval. `GA_Dodge`, its cost GE, and its normal Montage remain assigned.
2. Manually compile `PolyQuestEditor`.
3. In Scene01, use a focused Enemy Launch hit against Player and verify:
   - Dodge does not start during Takeoff, Airborne, early LandingRecovery, or after the window.
   - A normal short Dodge input inside the window costs Stamina once, stops LandingRecovery once, starts normal Dodge, and leaves no Launch HitReacting/input/ledge state behind.
   - A low but positive Stamina value follows the same normal-Dodge soft-overspend behavior and clamps to zero after its authored cost; exact zero Stamina inside the window leaves LandingRecovery intact and natural completion restores normal controls.
   - Held/repeated input cannot stack Dodge, re-open Launch permission, or let a successor Dodge erase its predecessor during startup.
   - Guard Break, external interruption, renewed Falling, and natural LandingRecovery completion each clear the window permission exactly once. Enemy Launch behavior remains unchanged.

## Execution, Review, And Closeout

1. Gemini implements only the approved native surface from baseline `c65c222`, then supplies changed paths, static evidence, unverified gates, and a two-pass implementation self-review. It must not modify plan/doc files, `Content/**`, maps, GA/GE/Montage/AnimBP/Blueprint assets, tags/config, input, Enemy code, Editor state, build state, staging, or commits.
2. The user owns Montage Notify authoring, GA/GE asset references, manual compile, and PIE evidence. The active assets remain local WIP and are not proof of a clean-checkout fixture.
3. After user-confirmed focused Automation and PIE, Main performs normal defect-first review and a separate Main adversarial fallback while `gpt-5.6-luna / xhigh` remains unavailable. Do not label that fallback as an independent fresh review.
4. Only then update `README.md`, `ARCHITECTURE.md`, `ROADMAP.md`, and this plan; move C4 to Done Milestones and retain no unowned C4 debt. A later scoped commit may include only approved C++ tests and documentation, never the user-owned assets or unrelated WIP.

## Closeout Record

- Implemented surface: Player Launch owns scoped Dodge CancelWindow Begin/End listeners and matching active-LandingRecovery identity checks; Dodge gates all HitReacting cancellation through `State.Action.CanCancel.Dodge` and cancels Player Launch only after normal `CommitAbility()` succeeds. No tags/config, Enemy behavior, input bindings, CharacterMovement behavior, or authored assets changed in the native slice.
- User evidence: the user confirmed Automation and focused PIE success. No separate manual `PolyQuestEditor` compile log was supplied for this closeout.
- Static evidence: direct source/caller/callee/tag review, CodeGraph exploration, Rider error-level lint with no errors, and `git diff --check` with no errors. `code-review-graph` calls returned `Transport closed`, so direct diff/source review was the impact-analysis fallback.
- Review: Main normal defect-first review and Main adversarial fallback found no P0-P2 or C4 repair item. `gpt-5.6-luna / xhigh` remained unavailable; this does not claim an independent reviewer result.
- Debt handoff: no unresolved accepted C4 risk remains. `TODO-02C3H` remains the canonical owner of landing-deferred Stance/Guard Break resolution; the authored Montage/GA/GE/Blueprint/AnimBP/input/map fixture remains mutable local `Content/**` WIP and is intentionally outside this commit.
- Commit boundary: only the six approved C++ source/test paths plus `README.md`, `ARCHITECTURE.md`, `ROADMAP.md`, and this `plan.md`; exclude `Content/**`, `AGENTS.md`, project/config WIP, generated files, and every unrelated change.
