# plan.md - AI Agent Collaboration File

## Rules

1. Read this file before continuing a PolyQuest stage.
2. Keep feedback, active plan, validation evidence, and unresolved blockers scoped to the current stage.
3. The implementation author cannot mark a stage as review-approved.
4. Retain the completed plan and closeout record until the next accepted stage deliberately replaces it.
5. Durable architecture belongs in `ARCHITECTURE.md`; accepted future direction and validation debt belong in `ROADMAP.md`.

---

## Active Stage: TODO-02F - Montage Rate Window And Action Timing v1

Baseline: `f5130c3 [Docs] 修正健康审查提交记录 (Correct Health Review Commit Record)`.

### Objective

Establish the one player-combat Montage playback-rate contract before weapon/loadout expansion: a dedicated `UAnimNotifyState` authors each window's interval and target rate on the attack Montages, the identity-matched active Montage/Ability applies and owns the override, and the baseline rate restores on every completion path with no stale rate surviving teardown. This stage also revisits the committed-cost atomicity debt ROADMAP assigned to it and retains it open (see the disposition section). No duplicate/cropped Sequence, no Section-based speed control, no parallel AnimBP action state, and no global per-tick rate controller.

Locked values:

- New `UAnimNotifyState_MontageRateWindow` in the existing action-window group: an authored `RateMultiplier` (`EditInstanceOnly`, `ClampMin = "0.01"`, default `1.0`) sent through `Event.Action.RateWindow.Begin` with the rate in `FGameplayEventData::EventMagnitude` and the source animation in `OptionalObject`; `Event.Action.RateWindow.End` carries no rate. Both tags are new.
- Baseline rate is exactly `1.0`. The project has zero other rate writers (exploration-verified), so restore never remembers a previous value; `Montage_SetPlayRate(ActiveMontage, 1.0f)` is an engine-silent no-op for a non-active Montage.
- Overlapping rate windows are not supported and authoring must keep them non-overlapping: the window End event identifies only its Montage, not which of two overlapping Rate Windows ended, so overlapping behavior is undefined and rejected as an authoring error. The implementation additionally ignores later Begin events while `bRateWindowApplied` is set; that ignore is defensive, not a supported overlap semantic.
- Root Motion scales with the rate (engine-verified substep semantics); continuity, not displacement preservation, is the contract. Charged HoldReady compatibility is engine-verified (`Montage_Pause`/`Resume` never touch `PlayRate`).
- Integration scope: Light Attack (per combo entry), Charged Attack, and Sprint Attack. Dodge, Guard, Parry, Guard Break, and every enemy ability stay un-listened in v1; later adoption is an additive listener hunk, not a mechanism change.

### Committed-Cost Atomicity Disposition (revised after independent review)

- The ROADMAP debt is **retained and is not closed by this stage**. The original debt covers Light entry and continuation, Dodge, Sprint Attack, and the Charged release path; closing it would require controlled post-commit playback-failure injection and verification across all of those paths, which cannot be produced reliably in this stage.
- The failure-injection definition is now precise: a meaningful injection must be a controlled playback failure occurring **after** `CommitAbility()`/`CommitAbilityCost()` succeeded - for example a non-null Montage asset that fails `Montage_IsActive` confirmation (zero-length or otherwise unplayable). An empty/null Montage reference only exercises the pre-commit validation abort and does not reach the committed-cost path at all.
- The no-refund semantics remains the working v1 direction (a refund path would couple reverse GameplayEffects to the Exhausted loose-tag contract and add double-bookkeeping disproportionate to the rare loss), recorded here as direction only. The debt's actual closure requires the full-path injection matrix and stays under ROADMAP ownership with its existing trigger.

### Rate Window Lifecycle Contract

- Each integrated ability adds two persistent `UAbilityTask_WaitGameplayEvent` listeners for the Begin/End events, accepted only when the payload identity-matches the active Montage (the established `OptionalObject` filter).
- On an identity-matched Begin while no window is applied and the ability is not ending: `Montage_SetPlayRate(ActiveMontage, Payload.EventMagnitude)` (defensively ignored when the magnitude is not positive) and set `bRateWindowApplied`.
- One guarded helper per ability (`RestoreBaselineMontageRate()`: when applied, restore `1.0` - engine-no-op for a stopped Montage - then clear the flag) is invoked from: the identity-matched window End, every `EndAbility()` path (before the Montage stop), and Light's `StartComboEntry()` before swapping to the next entry Montage, so each combo entry starts a fresh default-rate instance.
- Downstream timing follows the new animation timeline automatically: Trace Window, Combo Input/Branch, Dodge/Defense cancel, HoldReady, and Parry window NotifyStates all fire on rate-advanced Montage position. No downstream consumer is modified.

### User-Owned Editor Gate

1. Author Rate windows on the existing attack Montages only where the design needs them (for example a slow `Entry` and a faster `Recovery` on the light chain, a `Release` rate on Charged); use non-overlapping `UAnimNotifyState_MontageRateWindow` tracks with authored `RateMultiplier` values.
2. Do not crop/duplicate Sequences, add speed Sections, touch AnimBP locomotion/state machines, enemy assets, StateTree, input, maps, or collision for this stage.
3. Optional failure-injection pass (pre-commit abort only, within this stage's scope): temporarily author an empty/null Montage reference on one integrated ability, confirm the pre-commit validation abort exits cleanly, then restore. Post-commit playback-failure injection is out of scope for this stage per the disposition above.

### Validation Matrix

Main static gate: final source and direct caller/callee reads, CodeGraph, Gameplay Tag cross-check, C4458 scan, `git diff --check`, and verification that no rate call site exists outside the guarded helper pattern. Main does not run UBT, Editor writes, or PIE.

User compile and Scene01 PIE gate:

- Compile `PolyQuestEditor` after authoring the Rate windows.
- Verify an authored window visibly changes playback speed and restores exactly `1.0` at window end; two sequential windows on one action apply and restore independently.
- Verify Light combo handoff: an entry with a window hands off at default rate with no inherited value; window events from a replaced Montage cannot affect the successor.
- Verify Charged: a window spanning the HoldReady pause applies on release-resume; a release-section window behaves normally.
- Verify teardown: Dodge cancel, Defense cancel, interruption, death, PIE stop, and (if performed) the pre-commit validation-abort injection leave no stale rate, no stuck window flag, and no leaked listener.
- Verify downstream timing follows the rated timeline and Root Motion continuity at non-`1.0` rates.
- Re-run the D1/D2 defense matrix and C3B/C3C/HyperArmor regressions as a combat-loop smoke.

### Documentation And Commit Boundary

After user compile/PIE and review: `ARCHITECTURE.md` records the rate-window contract, `ROADMAP.md` marks only `TODO-02F` done while the committed-cost debt stays open unchanged (this stage records its no-refund direction in the plan, not in the debt's closure), and this file receives the closeout.

Native candidate paths: `Animation/Combat/AnimNotifyState_ActionWindows.h`, `Animation/Combat/AnimNotifyState_ActionWindows.cpp`, `LightAttackAbility.h`, `LightAttackAbility.cpp`, `ChargedAttackAbility.h`, `ChargedAttackAbility.cpp`, `SprintAttackAbility.h`, `SprintAttackAbility.cpp`, `Config/Tags/PolyQuestGameplayTags.ini`, and exact `README.md`/`ARCHITECTURE.md`/`ROADMAP.md`/`plan.md` hunks. Exclude every `Content/**` item and unrelated WIP. No commit occurs without explicit user approval.

### Route And Ownership

```text
Outer: ue-stage-workflow
Primary: ue5-cpp-gameplay
Support: ue5-blueprint-workflow, ue5-debug-validation
Route reason: The slice spans an AnimNotifyState contract, three attack-ability lifecycles sharing one teardown pattern, engine montage-instance semantics, and a standing validation-debt ruling; all are Main-only integration territory.
```

```text
Plan explorers: 1 (read-only general-purpose: montage call-site inventory, Charged HoldReady mechanism, engine rate API semantics, payload conventions, enemy-side rate absence)
Implementation executors: 0
Complex Executor: none
Main parallel work: none
Reason: One rate flag and one restore helper per ability share a single lifecycle boundary; concurrent writers would risk divergent restore semantics.
```

Main owns native source, tags, static checks, review, documentation, staging, and commit boundaries. The user owns all `Content/**` authoring, Editor readback, manual `PolyQuestEditor` compilation, Scene01 PIE validation, and commit approval.

### Non-Goals

No duplicate/cropped Sequence or Section-based speed control, no parallel AnimBP action state, no global per-tick rate controller, no enemy-side or defense-ability integration in v1, no refund implementation for committed costs.

### Closeout Record (2026-08-16)

- Implementation: `UAnimNotifyState_MontageRateWindow` sends Begin/End semantic events with an authored positive rate magnitude and active-Montage identity. Light, Charged, and Sprint Attack own their rate listeners, change only their matched active Montage instance, and restore `1.0` on matching End and every teardown; Light restores before a Combo handoff.
- User validation: the user confirmed the authored `PolyQuestEditor` compile and Scene01 PIE rate-window route, including normal restoration and the planned combat-loop regression coverage. Main did not run UBT, Editor, or PIE.
- Review: GLM self-review reported only a harmless `bRateWindowApplied` tail-reset style asymmetry, accepted without code churn. Main's direct review found no P0/P1 source lifecycle defect, required the atomicity disposition and unsupported-overlap wording above, then confirmed the revised plan and implementation. CodeGraph and direct source reads were the primary static evidence; `code-review-graph` was stale at `be3fdfd` and was not treated as coverage.
- Debt handoff: committed-cost playback atomicity remains open in `ROADMAP.md`. The normal no-refund direction is not closure evidence; a later controlled post-commit failure matrix across every affected action remains required before networking, prediction, or frame-exact Cost accounting.
- Commit scope: the focused native/config/documentation commit contains the two Rate Window Tags, the NotifyState, the Light/Charged/Sprint Ability lifecycle changes, and exact `README.md`/`ARCHITECTURE.md`/`ROADMAP.md`/`plan.md` updates. All `Content/**`, `.uproject`, generated files, and unrelated user WIP remain excluded.

---

## Previous Stage Record: TODO-02E (durable record lives in ROADMAP Done Milestones)

- 02E audited the bidirectional melee loop (no blocker), executed the evidence-led lean pass (legacy Dodge/Sprint properties, `Input.Dodge` tag, `ProjectName`), and synchronized README/ARCHITECTURE; the user confirmed the compile, `BP_Player` re-save, and PIE smoke. A post-review documentation repair fixed next-stage and Notify-name drift. Committed as `97a53a7` plus the `f5130c3` record correction.
