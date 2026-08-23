# TODO-02B1: Screen-Space Player Lock-On And Target Cycling v1

## Plan State

- Status: Implementation complete; documentation closeout pending one Bow PIE regression gate.
- Baseline: `615b7a1` (`[Feature] 完成基础生命 HUD 与敌人血条 / Core Vital HUD And Enemy Bars`).
- Objective: add one local, Player-owned screen-space lock target selected by middle mouse and cycled by the mouse wheel, with a gold highlight on the target's existing overhead Health bar.
- Current `Content/**`, maps, authored Blueprint/UMG/input/GA/GE/Montage/AnimBP assets, Config/project files, and every other worktree change are user-owned WIP. Preserve them. This stage may write only the explicitly named `BP_Player`, `IMC_Default`, and `WBP_EnemyVitalBar` assets through live Unreal Editor MCP after a focused readback and recovery point; all other asset work remains excluded.

~~~text
Outer: ue-stage-workflow
Primary: ue5-cpp-gameplay
Support: unreal-enhanced-input, ue5-ui-umg-slate, ue5-debug-validation
Route reason: this stage adds local Player input, target lifecycle, one-shot action-facing, and a narrow UMG presentation bridge while preserving fixed-camera and GAS ownership.
~~~

~~~text
Plan explorers: 0
Implementation executors: 0
Complex Executor: none
Main parallel work: none
Reason: Player input/action-facing, public C++ target queries, and lifecycle integration are Main-only territory under AGENTS.md. No child owns these cross-cutting contracts.
~~~

## Locked Product Contract

1. `APlayerCharacter` owns one local `TWeakObjectPtr<AEnemyCharacter>` lock. It adds no Gameplay Tag, GAS Ability, replication, auto-retargeting, generic targeting framework, camera orbit/recentering, free-look, or continuous rotation control.
2. A candidate must be an `AEnemyCharacter`, pass the existing `FCombatProjectileTargeting::IsValidTargetCandidate()` hostile/living/non-invulnerable/ASC predicate, and use its existing upper-torso `GetTargetAimPoint()`. The lock route must not call the Bow query `TryFindBestTargetCandidate()` and must not use Bow overscan, distance, angle, height, pitch, or visibility-trace filters.
3. Candidate aim points must be in front of the active local camera, project to finite screen coordinates, and satisfy strict bounds `0 < X < ViewportWidth`, `0 < Y < ViewportHeight`. Projection, viewport, mouse, ASC, or target failure is fail-closed.
4. With no valid lock, Middle Mouse selects the valid candidate nearest to the mouse screen position. Exact distance ties resolve by clockwise order, Player-screen distance, then Actor path name. No candidate is a no-op.
5. With a valid lock, Middle Mouse clears it. `IA_TargetCycle` positive input moves one candidate clockwise around the projected Player position; negative input moves one candidate counter-clockwise. Sort by ascending `atan2(DeltaY, DeltaX)` in screen coordinates, starting at screen-right; exact angle ties resolve by Player-screen distance, then Actor path name. Wheel input with no valid lock is a no-op.
6. Tick validates only the current lock and never scans candidates. Source/target death, destruction, missing ASC/team validity, invulnerability, projection failure, or leaving the strict viewport clears the weak reference and the old highlight with no automatic replacement. A later explicit Middle Mouse press may acquire anew.
7. Lock affects only a legal action start. Light, Charged, Sprint Attack, and Player Melee Skill face a valid lock once, otherwise retain legacy facing. A directional Dodge uses camera-relative movement direction whenever input is nonzero (screen-left remains screen-left); a no-input Dodge faces the valid lock, otherwise retains actor-forward fallback. Existing active Montages, Root Motion, ordinary movement, Guard/Parry, camera, Bow pointer facing, Release-time Bow assistance, projectile collision, and Homing remain unchanged.
8. The existing Enemy bar is the only lock presentation. It gains a gold `TargetHighlightImage` frame while locked; there is no separate reticle, target marker, scale change, fade, damage text, UI input ownership, or `TODO-07A2` visibility policy.

## Approved Native Surface

### Player lock and input

- Add `Source/PolyQuest/Public|Private/Character/Player/PlayerLockOnTargeting.{h,cpp}` as a stage-specific, pure screen-candidate helper. It owns strict viewport checks, clockwise ordering, mouse-nearest selection, and deterministic tie comparison over supplied candidate records. It owns no World scan, Actor state, GameplayTag, Tick, UI, or asset path.
- Update `Source/PolyQuest/Public|Private/Character/Player/PlayerCharacter.*`.
  - Add `LockOnAction` (`UInputAction*`) and `TargetCycleAction` (`UInputAction*`) editable inputs, plus `HandleLockOnStarted()` and `HandleTargetCycleTriggered()`.
  - Add one C++-only nullable `GetLockedTarget()` query for the later B3 Release-time preference. Do not call it from Bow in B1 and do not expose a Blueprint gameplay API.
  - Build candidates only for explicit acquire/cycle input using `TActorIterator<AEnemyCharacter>`, the shared projectile eligibility/aim-point helpers, and a local Controller projection adapter. A Tick path validates only `LockedTarget` and never chooses another actor.
  - Keep `GetActionWorldDirection()` unchanged. Add a lock-aware attack-facing entry that falls back to legacy facing, and a Dodge-facing entry that preserves a nonzero `CurrentMoveInput` before using the lock with no input. Update only Light, Charged, Sprint Attack, Player Melee Skill, and Dodge call sites required by that contract.
  - Clear the lock and previous highlighter during EndPlay, invalidation, and target changes. Missing local Controller/viewport/input configuration must warn or fail closed without changing gameplay state.
- Update `Source/PolyQuest/Private/AbilitySystem/Abilities/DodgeAbility.cpp` only to use the new Dodge-facing entry after its existing successful commit/cancellation flow. Do not alter Cost, CancelWindow, invulnerability, Montage, or Root Motion behavior.

### Enemy UI bridge

- Update `Source/PolyQuest/Public|Private/Character/Enemy/EnemyCharacter.*` with one C++-only `SetPlayerLockOnHighlighted(bool)` bridge. It caches the requested state, forwards it to a valid typed Health-bar Widget, reapplies it after the Widget is resolved, and clears it on death/EndPlay. It must not add a delegate, timer, target cache, gameplay mutation, or change to Health/Poise/death behavior.
- Update `Source/PolyQuest/Public|Private/UI/EnemyHealthBarWidget.*` with `SetLockOnHighlighted(bool)` and optional `TargetHighlightImage` (`BindWidgetOptional`). It sets only visibility and remains null-safe in Headless Automation or an incompletely authored Widget.
- Do not modify `FCombatProjectileTargeting`, `BowDrawFireAbility`, projectile classes, Gameplay Tags/config, `PolyQuest.Build.cs`, `APolyQuestPlayerController`, AttributeSet, GAS abilities other than the one Dodge-facing call site, AI, StateTree, maps, or mutable authored combat assets.

### Focused Automation

- Add `Source/PolyQuest/Private/Tests/PlayerLockOnAutomationTests.cpp` under `PolyQuest.Player.LockOn`. Use transient World fixtures and only narrow `WITH_DEV_AUTOMATION_TESTS` seams when existing C++-only behavior cannot otherwise be observed.
- Cover strict bounds/camera-front rejection, finite projection failure, mouse-nearest and deterministic tie-break, clockwise/counter-clockwise cycling, no-lock wheel no-op, invalid/dead/invulnerable/destructed/off-screen target clear without replacement, old/new Enemy highlight handoff, and the three facing routes: attack lock, Dodge with camera-relative move input, and no-input Dodge lock fallback.
- Extend the existing `VitalHudAutomationTests.cpp` only if needed to inject and assert `TargetHighlightImage`; do not load a Widget Blueprint or mutate Content assets in Automation.

## User-Authorized Live Editor Work

Before any write, Main must use live Unreal MCP serially: verify the Editor is not in PIE, read the current three assets, and save a recovery point. On a failed tool result, stop and report rather than guessing.

1. Create `/Game/Input/Actions/IA_LockOn` as Boolean, map Middle Mouse Button in the existing `/Game/Input/IMC_Default`; create `/Game/Input/Actions/IA_TargetCycle` as Axis1D, map Mouse Wheel Axis in the same context without Hold/Down triggers.
2. On `/Game/BP/Characters/Player/BP_Player`, assign `LockOnAction = IA_LockOn` and `TargetCycleAction = IA_TargetCycle`. Do not change `BP_test`, `BP_PlayerController`, `IMC_MouseLook`, cursor settings, camera, map, or unrelated input/action fields.
3. On `/Game/_UI/HUD/Vitals/WBP_EnemyVitalBar`, add a top-layer non-hit-testable `Image` named exactly `TargetHighlightImage`, frame it identically to the current bar, tint it gold, and set default Visibility to `Collapsed`. Preserve `HealthProgressBar`, geometry, and all other Widget behavior.
4. Read back the created/modified assets and save them. Asset changes remain user WIP and are excluded from the default stage commit unless the user later approves a stable asset closure.

## Validation, Review, And Closeout

### Static and Automation gate

1. Read final caller/callee and public API paths through CodeGraph, inspect required tags/config only to confirm B1 adds none, run Rider error-level lint on all touched C++, run `PolyQuest.Player.LockOn` plus `PolyQuest.UI.VitalHUD` and affected projectile/combat regressions, then run `git diff --check`. Do not invoke UBT/UAT/Rider builds without explicit user permission.
2. Static, Automation, Rider, and Editor readback are not PIE or visual proof.

### User compile and PIE gate

1. Manually compile `PolyQuestEditor`.
2. In PIE verify no-target acquire, mouse-nearest acquire, Middle Mouse clear/reacquire, positive/negative Wheel order, strict viewport loss, death/destruction clear, gold bar-frame transfer, locked attack/skill facing, screen-left Dodge, no-input lock Dodge, unchanged fixed camera/movement/cursor/Game-and-UI behavior, and unchanged Bow pointer-facing/target-assist/Homing.

### Review, documentation, and commit boundary

1. After user-confirmed Automation, compile, Editor readback, and PIE, Main performs normal defect-first review plus Main adversarial fallback while `gpt-5.6-luna / xhigh` remains unavailable. Do not call that fallback independent review.
2. On accepted closeout, update README, ARCHITECTURE, ROADMAP, and this plan. Mark B1 done; retain B3 as the only Bow locked-target preference owner and 07A2 as the Enemy-bar visibility-policy owner.
3. The default commit includes only approved C++/Automation/docs. Exclude all `Content/**`, input/Widget Blueprints, maps, Config/project WIP, generated folders, and unrelated changes unless a later explicit stable asset closure is approved.

## Current Evidence And Remaining Closeout Gate

- User-confirmed: manual `PolyQuestEditor` compilation; Editor setup that produces the Middle Mouse lock and gold existing Enemy-bar frame; PIE checks for no target, cursor-nearest acquisition, Middle Mouse clear/reacquire, both wheel directions, non-death invalidation clear, action-facing, unchanged fixed camera/ordinary movement, and both directional/no-input Dodge behavior.
- User-confirmed: `PolyQuest.Player.LockOn` Automation now succeeds. Its transient fixture warnings about missing Enemy AttackSet, Poise recovery configuration, Player loadout, and ragdoll Physics Asset are intentional incomplete-fixture diagnostics; the test result is `Success`.
- Main single fresh review found no P0-P2 source defect. CodeGraph caller/callee reads, code-review-graph supplemental context, Rider error-level lint on the touched C++ surface, Rider project Errors, and `git diff --check` found no source or whitespace blocker. Graph-reported global test gaps are not treated as an independent test result.
- Remaining closeout gate: user PIE must confirm that, while an Enemy is locked, Bow Draw/Hold remains mouse-directed and Bow Release preserves the existing no-lock target-assist/Homing behavior. This stage does not give Bow the lock target; that remains exclusively `TODO-03B-3`. Until this visual regression is confirmed, do not mark B1 done or update README/ARCHITECTURE as completed.
