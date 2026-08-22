# TODO-02C3H: Landing-Deferred Enemy Stance Break Resolution v1

## Plan State

- Status: Completed; documentation closeout and commit are approved.
- Baseline: `c834cc5` (`[Feature] 玩家击飞起身翻滚窗口 / Player Launch Recovery Dodge`).
- Objective: allow one nonlethal Player Launch hit to reduce a living Enemy's Poise to zero without dispatching Stance Break until the Enemy has naturally completed LandingRecovery and the Launch reaction state has been released.
- Current `Content/**`, maps, imported assets, authored GA/GE/Montage/AnimBP/Blueprint/input assets, `AGENTS.md`, `Config/Automation/**`, `Config/Tests/**`, and other worktree changes are user-owned WIP. Preserve and exclude them from this stage's source/document commit unless the user explicitly approves a stable closure.

~~~text
Outer: ue-stage-workflow
Primary: ue5-cpp-gameplay
Support: ue5-debug-validation
Route reason: C3H changes the Enemy-owned Poise/Launch/Stance Break lifecycle and needs explicit abnormal-path and exactly-once validation.
~~~

~~~text
Plan explorers: 0
Implementation executors: 1 (Gemini, user-coordinated bounded executor)
Complex Executor: one scoped lifecycle-sensitive implementation
Main parallel work: contract review, validation interpretation, fresh/adversarial review, documentation closeout, and commit preparation after user approval.
Reason: gpt-5.6-luna is unavailable. The Attribute, Launch, and Stance Break lifecycle is one coupled boundary; Main retains the contract, integration, review, documentation, and commit ownership.
~~~

## Locked Product Contract

1. C3H is **Enemy-only**. A valid Player Guard or Parry continues to consume an incoming Launch hit exactly as it does today. If Guard Stamina reaches zero, the current immediate `PlayerGuardBreakAbility` route remains authoritative; C3H does not add Player pending Guard Break, Player Launch/Guard coupling, or changes to Melee/Projectile defense resolution.
2. A production damage GE may now pair exactly one `Data.Reaction.Launch` tier tag with the existing Poise-delivery contract. The selected Player attack must both deal nonlethal Health damage and actually reduce Enemy Poise through the existing `Data.Poise.Charged` / SetByCaller route. Multi-tier Reaction tags remain invalid and fail closed.
3. Existing ordinary Enemy `Poise == 0` behavior remains unchanged: it schedules the current next-tick Stance Break route. C3H only takes ownership after an Enemy Launch Takeoff Montage has successfully started.
4. A Launch-active Enemy owns at most one pending Stance Break intent. No `Event.Reaction.Enemy.StanceBreak` may be dispatched during Takeoff, AwaitingAirborne, Airborne, or LandingRecovery.
5. On a matching LandingRecovery Montage's natural end only, `UEnemyLaunchReactionAbility` must finish its delegate/task/montage/ledge cleanup and call `Super::EndAbility()` before the Enemy rechecks: authority, liveness, current `Poise <= 0`, valid Poise recovery configuration, and no stale pending state. It then dispatches at most one Stance Break event.
6. If Poise becomes positive before this natural completion, clear the pending intent with no Stance Break. Death, destruction, EndPlay, invalid setup before a Launch starts, interruption, cancellation, renewed Falling, or failed landing recovery must never produce a late Stance Break.
7. If a Launch that already owns a zero-Poise intent ends abnormally, clear the intent and restore Poise through the existing authored recovery GE. Do not introduce a post-cancel ground-wait state in this version.

## Approved Native Surface

### Enemy-owned deferred result

- Update `Source/PolyQuest/Public|Private/Character/Enemy/EnemyCharacter.*`.
- Add three native-only, non-Blueprint lifecycle calls for the paired Launch ability:
  - `BeginLaunchStanceBreakDeferral()` marks an actually-started Launch, cancels any normal pending Stance Break timer, and records the one intent when Poise is already zero.
  - `CompleteLaunchStanceBreakDeferral()` runs only after natural LandingRecovery cleanup and dispatches the existing Stance Break event through a shared eligibility/dispatch helper.
  - `AbortLaunchStanceBreakDeferral()` clears the intent and, only for a valid living Enemy still at zero Poise, restores Poise through the existing recovery GE.
- Store only target-owned transient state: one active Launch-deferral flag and one pending Stance Break flag. Do not add a reaction subsystem, a new Gameplay Tag, a timer queue, a second Poise source of truth, or an Actor/ASC reference cache.
- `OnPoiseAttributeChanged()` must retain normal recovery and normal Stance Break behavior outside an active Launch. During an active Launch, a zero-Poise transition becomes the single deferred intent. Cover both modifier orders explicitly: if Poise reaches zero first, its ordinary next-tick timer remains valid only until `BeginLaunchStanceBreakDeferral()` synchronously clears it and records the intent; if Launch begins first, the later zero-Poise callback records the intent without creating that timer. A positive Poise transition during either case clears the deferred intent.
- Refactor the existing timer-dispatch validation into the smallest shared private helper so normal and deferred routes have identical alive/Poise/ASC/recovery validation, failure logging, and rejected-event Poise restoration.
- `HandleDeath()` and `EndPlay()` clear normal timers and both C3H flags without applying recovery or dispatching an event.

### Enemy Launch bridge

- Update `Source/PolyQuest/Public|Private/AbilitySystem/Abilities/EnemyLaunchReactionAbility.*`.
- Reset a private `bLandingRecoveryCompletedNaturally` flag on every activation. Call `BeginLaunchStanceBreakDeferral()` only after the Takeoff Montage has demonstrably started and before asynchronous flight/landing paths can act.
- In `OnActiveMontageEnded()`, mark natural completion only when the current phase is `LandingRecovery`, the completed montage is the configured recovery montage, and `bInterrupted == false`.
- In unified `EndAbility()`, capture the natural-completion disposition and a local `TWeakObjectPtr<AEnemyCharacter>` before cleanup; do not retain a raw Enemy pointer across `Super::EndAbility()`. Remove delegates/tasks, stop owned montages, restore ledge settings, reset transient fields, then call `Super::EndAbility()`. Afterwards reacquire and validate the weak target (`IsValid()` and not being destroyed) before invoking `CompleteLaunchStanceBreakDeferral()` for natural LandingRecovery; every other end path invokes `AbortLaunchStanceBreakDeferral()`. Complete must consume its pending intent before `HandleGameplayEvent()` so Stance Break cancellation cannot re-enter or duplicate the result.
- Do not modify Player Launch, Player Dodge C4 behavior, `UEnemyStanceBreakAbility`, `UCharacterAttributeSet`, Gameplay Tags/config, input, StateTree, or CharacterMovement rules.

### Automation fixture

- Update `Source/PolyQuest/Private/Tests/HitReactionAutomationTests.cpp`.
- Add `Source/PolyQuest/Private/Tests/TestPoiseRecoveryGE.{h,cpp}` as an isolated native `UGameplayEffect` test class, used through its CDO/`MakeOutgoingSpec` like the existing test damage GE. Its Poise modifier must consume `Data.Poise.Recovery` through SetByCaller; do not load or mutate a Content asset or an arbitrary GameplayEffect CDO.
- Add only the minimum `WITH_DEV_AUTOMATION_TESTS` accessors/configuration hooks on `AEnemyCharacter` and `UEnemyLaunchReactionAbility` needed to exercise the target-owned lifecycle and natural-versus-abort bridge. They must not expose a production Blueprint API.

## Validation Matrix

### Native/static

- `PolyQuest.Combat.HitReaction` must prove:
  - ordinary zero Poise still emits one ordinary Stance Break;
  - Poise reaches zero before Launch starts: `BeginLaunchStanceBreakDeferral()` clears the already-scheduled ordinary timer, records one intent, and the old timer produces no Stance Break;
  - Launch starts before Poise reaches zero: the later Poise callback records the same one intent and never creates an ordinary timer;
  - Launch plus zero Poise records one intent and emits neither Stance Break nor `State.Status.Stunned` before natural completion;
  - natural completion after Launch state release emits exactly one Stance Break, with `State.Action.HitReacting` absent at dispatch;
  - duplicate begin/complete calls cannot duplicate events;
  - Poise recovery during flight clears the intent;
  - abort restores Poise and emits no Stance Break;
  - death and EndPlay clear the intent with no late event;
  - a Launch that never became active does not pollute the ordinary Poise route.
- Before user validation, inspect the final caller/callee chain and tag requests, run Rider Error-level lint for the changed C++ files if available, and run `git diff --check`. Do not invoke UBT, UAT, or a Rider build without explicit user permission.

### User-owned Editor, compile, and PIE

1. Configure one selected Player launch source to use a nonlethal damage GE with only `Data.Reaction.Launch`, Health damage, and a real Enemy-Poise reduction through the existing SetByCaller data contract. Do not change assets through filesystem edits.
2. Manually compile `PolyQuestEditor`.
3. In Scene01, verify a grounded Enemy hit by that attack:
   - flies and completes LandingRecovery without Stance Break or Stunned state;
   - enters Stance Break once, only after recovery visibly completes;
   - does not Stance Break when the same Launch damage leaves Poise above zero;
   - produces no second Stance Break from repeated contacts;
   - clears without a delayed Stance Break when the Enemy dies or Launch is externally interrupted; its surviving abnormal-abort case recovers Poise rather than remaining at zero.
4. Recheck ordinary non-Launch Poise break, Player Guard/Parry absorption, Player immediate Guard Break, Player Launch C4 recovery Dodge, and Enemy non-Launch AI behavior for regressions.

## Execution, Review, And Closeout

1. Before implementation, Main provides Gemini a concrete executor prompt with this exact plan, absolute cwd, baseline, allowed source/test paths, no-goals, user-owned Editor/compile/PIE gates, static evidence requirements, strict self-review boundary, and an explicit no-commit/no-asset-mutation rule.
2. Gemini implements only the approved native/test surface and supplies changed paths, static evidence, unverified gates, and a two-pass implementation self-review. It must not edit `plan.md`, project documentation, `Content/**`, maps, GA/GE/Montage/AnimBP/Blueprint assets, tags/config, input, Player/Guard code, Editor state, staging, or commits.
3. After user-confirmed Automation and PIE, Main performs a normal defect-first review plus a clearly labelled Main adversarial fallback while `gpt-5.6-luna / xhigh` remains unavailable. Do not describe that fallback as independent review.
4. Only after validation and review, update `README.md`, `ARCHITECTURE.md`, `ROADMAP.md`, and this plan; move C3H to Done Milestones and stage only approved C++ test/document paths. The Roadmap closeout hunk must rename C3H to `Landing-Deferred Enemy Stance Break Resolution v1`, remove deferred Player Guard Break from its body, and state that Player Guard/Parry keep the immediate existing resolution rather than creating a separate placeholder TODO.

## Closeout Record

- Implemented surface: `AEnemyCharacter` now owns the active/pending Enemy Launch Stance Break state, preserves the ordinary next-tick route outside an active confirmed Launch, correlates the same GameplayEffect modifier transaction across Health/Poise order, and converges ordinary/deferred dispatch through one current-state eligibility helper. `UEnemyLaunchReactionAbility` begins deferral only after Takeoff starts and completes or aborts it only after unified cleanup. `UTestPoiseRecoveryGE` and isolated ordering fixtures cover the native recovery and same-transaction paths without touching Content assets.
- User evidence: the user confirmed `PolyQuest.Combat.HitReaction` Automation and focused PIE. No separately retained `PolyQuestEditor` build log is represented as evidence in this closeout.
- Static evidence: direct source/caller/callee/tag review, CodeGraph exploration, Rider inspection with no error-level finding, and `git diff --check` on the approved surface. code-review-graph returned `Transport closed`, so direct diff/source review is the graph-coverage fallback.
- Review: Main normal defect-first review and Main adversarial fallback found no P0-P2. `gpt-5.6-luna / xhigh` remains unavailable, so this is not presented as an independent reviewer result.
- Debt handoff: `ROADMAP.md` records the accepted abnormal-Launch fork: abort restores Poise with no Stance Break; retain-zero-Poise-and-break-after-safe-ground remains an unimplemented alternative with a focused playtest closure trigger. Player Guard/Parry and immediate Guard Break are deliberately outside C3H rather than silently deferred.
- Commit boundary: `Source/PolyQuest/Private/AbilitySystem/Abilities/EnemyLaunchReactionAbility.cpp`, `Source/PolyQuest/Public/AbilitySystem/Abilities/EnemyLaunchReactionAbility.h`, `Source/PolyQuest/Private/Character/Enemy/EnemyCharacter.cpp`, `Source/PolyQuest/Public/Character/Enemy/EnemyCharacter.h`, `Source/PolyQuest/Private/Tests/HitReactionAutomationTests.cpp`, `Source/PolyQuest/Private/Tests/TestPoiseRecoveryGE.cpp`, `Source/PolyQuest/Private/Tests/TestPoiseRecoveryGE.h`, `README.md`, `ARCHITECTURE.md`, the C3H-only `ROADMAP.md` hunks, and this `plan.md`. Exclude `Content/**`, `AGENTS.md`, project/config WIP, generated files, and every unrelated change.
