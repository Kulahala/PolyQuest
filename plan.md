# TODO-03H4: Combat Delivery And Presentation Health Review v1

## Plan State

- Status: Completed read-first health review; no production repair was required and the closeout commit is documentation-only.
- Baseline: `905c2e4` (`[Feature] 完成敌人根运动朝向与目标记忆`).
- Objective: audit the completed combat-delivery and presentation boundaries before `TODO-03C: Ranged Enemy v1`, then either close confirmed healthy contracts without implementation changes or isolate each real blocker into a minimal approved repair slice.
- This stage adds no player loop, weapon family, VFX system, asset migration, DataAsset field, Gameplay Tag, Input route, StateTree topology, generic framework, or authored Content change by default.
- Preserve all current user WIP. `.gitignore`, `Config/**`, `Content/**`, maps, Blueprints, AnimBPs, GA/GE/Montage assets, `.uproject`, generated files, and unrelated source changes are not review findings or commit candidates unless a direct confirmed defect requires a separate Main scope decision.

## Route And Delegation

Outer: `ue-stage-workflow`
Primary: `ue5-debug-validation`
Support: none initially
Route reason: H4 is an evidence-led integration audit across already-completed native contracts. It must distinguish an observed gameplay defect from expected negative-test logging, authored-asset WIP, and speculative future scaling.

Plan explorers: 0
Implementation executors: 0
Complex Executor: none
Main parallel work: none
Reason: target/trace/trail/projectile/equipment/AI teardown boundaries overlap through shared Controller, AbilityTask, GAS, and presentation lifecycles. A broad audit has no safe independent implementation slice; Main owns the review conclusion and any later repair scope.

## Review Surface And Primary Runtime Questions

### 1. Delivery ownership and stale-callback safety

- Review `UAbilityTask_MeleeTraceWindow`, `UMeleeTraceSourceComponent`, `UMeleeWeaponTrailComponent`, relevant AnimNotify payload routing, and the multi-source Trace/Trail Automation suites.
- Confirm one attack window remains the sole owner of endpoint sampling, shared target deduplication, and source-keyed trail requester cleanup; stale Notify/Task teardown must not close a successor window or trail.
- Confirm invalid/degenerate endpoints fail closed before hit delivery and visual updates, without leaving active requesters/components behind.

### 2. Equipment composition and active-action teardown

- Review `UWeaponEquipmentComponent`, weapon definitions, Guard/Bow entry points, and transaction/Action Window/Mobile Bow Automation coverage.
- Confirm an equipment transaction cannot leave a display, Trace source, granted spec, prepared slot, defense state, or effect handle from a displaced composition after a successful swap, rollback, death, or active-action rejection.
- Preserve MainHand locomotion facts, committed OffHand Shield presentation, current Bow contract, and all existing GAS authority; do not redesign equipment.

### 3. Bow, projectile, and presentation terminal paths

- Review `UBowDrawFireAbility`, `ACombatProjectile`, `UProjectileDefinition`, targeting/homing helpers, and Lifecycle/TargetAssist/FlightTrail Automation coverage.
- Confirm Bow's exact MoveSpeed effect handle, aim requester, cancel windows, montage/event tasks, and projectile spawn boundary converge safely through `EndAbility()`.
- Confirm projectile collision/damage delivery remains immediate and exactly once while Flight Trail visual completion, timeout, external destruction, and world teardown remain bounded and cannot re-enable movement, homing, collision, or a second hit.

### 4. Enemy combat presentation and target continuity

- Review `AEnemyAIController`, Enemy combat StateTree-facing call sites, hit-reaction/Root Motion handoff, Reposition move completion, and RootMotionFacing/CombatTargetRetention/CombatSpacing Automation coverage.
- Confirm StateTree selects intent only; Controller owns CurrentTarget, Focus, Home leash, tactical pace, and Root Motion yaw handoff. Retained target loss must yield one deterministic TargetLost path, and late perception/teardown must not restore a stale target.
- Preserve the completed 140-degree initial Sight rule, bounded retention, fixed v1 `300` tactical pace, and no Search/Hearing/LastKnownLocation behavior.

### 5. Automation signal and evidence integrity

- Classify current Automation logs as either expected negative assertion evidence, unexpected success-path signal noise, or a real product warning.
- Inspect test fixtures only where they prove the production route. Do not turn a renderer/headless limitation, expected fail-closed warning, or user-owned asset absence into a production defect.
- Do not claim manual compilation, Editor readback, PIE, visual, or asset validation unless the user separately supplies that evidence for this stage.

## Approved Initial Files

Read-only initial review surface:

- `Source/PolyQuest/Public|Private/AbilitySystem/Tasks/AbilityTask_MeleeTraceWindow.*`
- `Source/PolyQuest/Public|Private/Combat/Melee/MeleeTraceSourceComponent.*`
- `Source/PolyQuest/Private/Combat/Melee/MeleeWeaponTrailComponent.*`
- `Source/PolyQuest/Public|Private/Combat/Equipment/WeaponEquipmentComponent.*` and directly read weapon-definition contracts
- `Source/PolyQuest/Public|Private/AbilitySystem/Abilities/BowDrawFireAbility.*`
- `Source/PolyQuest/Public|Private/Combat/Projectile/CombatProjectile.*`, `ProjectileDefinition.*`, and direct targeting helpers
- `Source/PolyQuest/Public|Private/AI/EnemyAIController.*` plus direct StateTree task/condition callers
- Directly related Automation files: `MeleeMultiTraceSourceAutomationTests.cpp`, `MeleeWeaponTrailAutomationTests.cpp`, `WeaponEquipmentComponentAutomationTests.cpp`, `PlayerMobileBowAutomationTests.cpp`, `ProjectileLifecycleAutomationTests.cpp`, `ProjectileTargetAssistAutomationTests.cpp`, `ProjectileFlightTrailAutomationTests.cpp`, `EnemyRootMotionFacingAutomationTests.cpp`, `EnemyCombatTargetRetentionAutomationTests.cpp`, and only required fixtures/test helpers.
- `AGENTS.md`, `ROADMAP.md`, `ARCHITECTURE.md`, and this active `plan.md` for contract/debt comparison.

Any source, Config, asset, Blueprint, map, StateTree asset, public API, Tag, Input route, or Build.cs modification is a stop condition until Main records a narrow repair decision. H4 itself has no authorized production-code edits at plan acceptance.

## Audit Order And Evidence Rules

1. Freeze the current baseline and preserve unrelated WIP; use CodeGraph before raw source search and code-review-graph only as supplemental change/impact evidence.
2. Trace each boundary from entry event to cleanup, including cancellation, interrupted montage/task callbacks, invalid data, actor teardown, and same-frame successor paths.
3. Compare native source, directly relevant tests, current Roadmap debt, and user-provided Automation/PIE evidence. Source/static inspection is not runtime or visual proof.
4. Run targeted Rider diagnostics and `git diff --check` only if H4 creates or approves a code repair. Do not invoke UBT, UAT, packaging, or Editor writes.
5. Classify findings: P0-P2 block closeout and require a minimal repair plan; a P3 becomes Roadmap debt only when it has an affected boundary, current evidence, actual impact, closure trigger, and owning stage/release gate. Pure style observations and optional ideas remain out of the debt register.
6. If no blocker remains, Main performs the stage Fresh Review, records stable facts in project documentation, and waits for explicit commit approval. If any repair changes runtime code, the user reruns the affected Automation and focused PIE route before that closeout.

## Validation Matrix

### Existing user evidence to interpret, not rerun by default

- Current Editor Automation matrix: 18 suites, including the trace, trail, equipment, mobile Bow, projectile, and Enemy AI suites.
- Focused Scene01 PIE evidence from their owning completed stages, including weapon trails, arrow flight trail, mobile Bow, Shield presentation, multi-source melee, and Enemy Root-Motion-facing/target retention.

### Only if H4 confirms and repairs a defect

1. User compiles the affected `PolyQuestEditor` target or otherwise explicitly reports the current-code build result.
2. User reruns the exact affected Automation suite plus the full current matrix.
3. User performs a focused Scene01 PIE repro/verification that distinguishes the original first-bad transition from the repaired outcome.
4. Main completes a delta Fresh Review before documentation/commit.

## Existing Debt Handoff

- `TODO-03H4B` owns the header-hygiene candidates as isolated no-behavior micro-slices. `MeleeWeaponDefinition.h` may stop including `CombatLoadoutDefinition.h`, but its `.cpp` must include it directly because `IsValidWeaponDefinition()` calls `AssociatedLoadout->IsRouteTableValid()`; this is a Public-header dependency move, not removal of the C++ dependency. The four `GameplayAbilityTypes.h` candidates require individual self-sufficiency review and a clean compile; a direct public/reflected type use is valid reason to retain the include.
- The Controller-owned fixed `300` Reposition pace remains a conditional future AI debt, not a slimming task. Trigger migration to `UEnemyAIProfile` authored pace plus a dedicated Enemy MoveSpeed GameplayEffect only if a second Enemy needs a distinct pace or Enemy MoveSpeed effects become real; never reuse Player `GE_Walk_MoveSpeed`.
- Definition-wide Niagara System and Trace Radius remain the accepted symmetric-fist v1 contract. Per-source overrides are an adoption condition for a real asymmetric source, not an approved P3 cleanup or pre-authorized field addition.
- `EnemyCombatSpacingTests.cpp` contains local state-machine simulations for some Approach/Reposition cases instead of driving the production Controller route. This is a P3 test-fidelity debt, not a current behavior defect. Its owner is `TODO-03C` only if that stage changes shared Approach/Reposition logic; then add a production `AEnemyAIController` / StateTree-facing fixture before accepting the change.
- The prior AI3/AI3B protected-override Rider notice is not an accepted debt: no direct caller or gameplay impact was found, and H4 must not add a cleanup item merely to silence an IDE style preference.

## Closeout And Commit Boundary

- With no repair, the eventual commit contains only H4 documentation updates. With a confirmed repair, commit scope is limited to the explicitly approved source/test/document paths for that repair; never absorb user WIP as a health-review side effect.
- `ARCHITECTURE.md` receives only stable implemented ownership facts. `ROADMAP.md` receives only completed H4 evidence and genuine unresolved debt with a closure trigger. `README.md` changes only if its public summary materially changes.

## Main Fresh Review And Closeout

### Findings

- P0-P2: none found in the approved delivery, equipment, Bow/projectile, and Enemy Controller review surface.
- P3, documentation/evidence correction: the initial executor-style report counted seventeen `*AutomationTests.cpp` files, but the repository contains eighteen Automation macro declarations; `EnemyCombatSpacingTests.cpp` was omitted by that glob. This is an audit-coverage correction, not a failed test or a product regression.
- P3, validation fidelity: the omitted Combat Spacing suite uses local simulated state machines for parts of Approach/Reposition. The direct Root-Motion-facing and target-retention routes retain their existing coverage; only a future shared AI-spacing change must add the production-path fixture described above.

### Evidence Accounting

- Main used source/static inspection, CodeGraph call-path reads, direct test inspection, and a documentation-only code-review-graph check. The graph was built at an older SHA than `905c2e4`, so its zero-impact result was not treated as source-review coverage.
- Existing user evidence interpreted by H4: eighteen Editor Automation suites and focused Scene01 PIE validation from the owning completed stages. H4 made no source or asset change, so it did not request or run a new compile, Automation pass, Editor readback, or PIE session.

### Documentation And Commit Result

- `ROADMAP.md` records H4 as complete, preserves only the concrete P3 closure conditions above, and keeps the accepted order `TODO-03H4A` -> `TODO-03H4B` -> `TODO-03C`.
- `ARCHITECTURE.md` and `README.md` remain unchanged because this audit introduced no stable runtime contract or public-facing feature.
- Commit scope: `plan.md` and `ROADMAP.md` only. All `.gitignore`, `Config/**`, `Content/**`, Blueprint, map, animation, and other user WIP remains excluded.
