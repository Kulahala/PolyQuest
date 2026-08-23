# TODO-03H3: Automation Fixture Signal Hygiene v1

## Plan State

- Status: Complete. The user confirmed `PolyQuestEditor` compilation and all ten existing Automation suites through the Editor front-end; H3 has no PIE gate.
- Baseline: `7381cb7` (`[Fix] 修复自动化夹具日志并完成运行时健康审查`).
- Objective: remove missing-fixture configuration noise from successful paths in the existing ten native Automation suites. This stage does not change runtime gameplay, GAS failure safeguards, authored assets, Config, log levels, or the test-suite count.
- Preserve the current uncommitted `ROADMAP.md` H3 scheduling change and all other user-owned WIP. In particular, exclude `Content/**`, Config, maps, Blueprints, Input, AnimBP, GA/GE/Montage assets, `.uproject`, generated output, and `.zcode/`.

```text
Outer: ue-stage-workflow
Primary: ue5-debug-validation
Support: ue5-cpp-gameplay
Route reason: distinguish missing native-fixture configuration signals from intentional negative coverage. The only implementation surface is private Automation construction, test-only configuration, and assertions.
```

```text
Plan explorers: 0
Implementation executors: 0
Complex Executor: none
Main parallel work: none
Reason: shared Player/Enemy BeginPlay, ASC, equipment, and test lifecycle contracts are Main-only integration territory; no parallel exploration materially reduces risk.
```

## Approved Contract

1. Add a private `FCombatAutomationFixture` under `Source/PolyQuest/Private/Tests/`. It owns deferred Player/Enemy spawn, pre-`FinishSpawning()` setup, and a one-time BeginPlay guard. It is not runtime or Blueprint API.
2. Add `UTestStaminaRegenGE`: an `Infinite` test-only GameplayEffect with no numerical modifier, so a native Player fixture gets a valid active regen handle without pretending to test production stamina recovery.
3. Under `WITH_DEV_AUTOMATION_TESTS`, add only the narrow fixture hooks required to atomically configure Player startup and prove its regen effect applied; configure an empty legal `UCombatLoadoutDefinition`, a transient valid unarmed `UMeleeWeaponDefinition`, the existing Hero mesh and `Weapon_R` socket, and `UTestStaminaRegenGE` before Player BeginPlay.
4. Under `WITH_DEV_AUTOMATION_TESTS`, add a narrow passive Enemy fixture hook that sets `AutoPossessAI=Disabled`, disables death ragdoll, and supplies `UTestPoiseRecoveryGE` before Enemy BeginPlay. It must not fabricate AttackSet, AIProfile, StateTree, Physics Asset, or Ragdoll coverage.
5. Migrate direct Player/Enemy `DispatchBeginPlay()` fixture paths only in `HitReactionAutomationTests.cpp`, `MeleeTraceSourceComponentAutomationTests.cpp`, `PlayerLockOnAutomationTests.cpp`, `ProjectileLifecycleAutomationTests.cpp`, `ProjectileTargetAssistAutomationTests.cpp`, and `VitalHudAutomationTests.cpp`. Do not perform cosmetic migrations in the other four suites.
6. Do not suppress, lower, or reinterpret logs. No `AddExpectedError`, log-category changes, runtime bypasses, new gameplay feature, authored asset, Config edit, or eleventh Automation suite is in scope.

## Warning Ledger Contract

After the migration, successful paths in the ten-suite matrix must no longer emit incidental warnings for null Player loadout, missing default Player weapon, missing Player stamina regen GE, Enemy invalid AttackSet/AIProfile/Poise recovery, headless Enemy missing Physics Asset ragdoll, or invalid Bow AbilitySpecHandle.

The following remain deliberate negative-test signals and must be mapped at closeout rather than suppressed: Equipment preflight/active-swap/rollback, invalid static blade socket or geometry, invalid multi-tier hit-reaction tag, and no Stance Break Ability accepting an event followed by Poise recovery. The Vital HUD headless-null Widget path remains legal, and `No game viewport was found` must not reappear.

## Execution Order

1. Read the current Player/Enemy startup paths and the six test callers with CodeGraph/direct source. Verify the fixture injection happens before `FinishSpawning()` even for Worlds already in play.
2. Add the test-only GE, shared fixture, and minimal Player/Enemy `WITH_DEV_AUTOMATION_TESTS` hooks.
3. Replace only the six approved direct startup paths, preserving their test-specific setup and negative assertions.
4. Read the final call chains and use Rider error-level inspection where available, `git diff --check`, and code-review-graph as supplementary impact evidence.
5. Ask the user to compile `PolyQuestEditor` and run the existing ten suites with raw logs. H3 has no Editor authoring or PIE gate.
6. After accepted validation, perform Main's single defect-first fresh review, synchronize `README.md`, `ARCHITECTURE.md`, `ROADMAP.md`, and `plan.md`, then wait for explicit commit approval.

## Validation Matrix

- `PolyQuest.Equipment.TransactionMatrix`
- `PolyQuest.Melee.TraceSourceGeometry`
- `PolyQuest.Player.ActionWindows`
- `PolyQuest.Combat.HitReaction`
- `PolyQuest.Enemy.AttackSetSelection`
- `PolyQuest.Enemy.CombatSpacing`
- `PolyQuest.UI.VitalHUD`
- `PolyQuest.Player.LockOn`
- `PolyQuest.Projectile.Lifecycle`
- `PolyQuest.Projectile.TargetAssist`

Acceptance requires that every remaining Warning in these successful runs maps to a named deliberate negative assertion in the H3 warning ledger. Any unclassified successful-path warning blocks H3 closeout.

## Current Validation Record

1. User-confirmed Automation success: `PolyQuest.Equipment.TransactionMatrix`, `PolyQuest.Melee.TraceSourceGeometry`, `PolyQuest.Player.ActionWindows`, `PolyQuest.Combat.HitReaction`, `PolyQuest.Enemy.AttackSetSelection`, `PolyQuest.Enemy.CombatSpacing`, `PolyQuest.UI.VitalHUD`, `PolyQuest.Player.LockOn`, `PolyQuest.Projectile.Lifecycle`, and `PolyQuest.Projectile.TargetAssist`.
2. H3 success-path fixture noise is absent from the supplied raw logs: null Player Loadout, missing Player weapon or Stamina Regen GE, Enemy invalid AttackSet/AIProfile/Poise configuration, headless Ragdoll Physics Asset, invalid Bow AbilitySpecHandle, and `No game viewport was found`.
3. Remaining messages map to asserted negative coverage: invalid multi-tier reaction tags; Stance Break rejection with Poise recovery; equipment preflight, active-swap and rollback injections; and invalid/missing static trace geometry.
4. Main's required single defect-first fresh review found no P0-P2 issue. Rider error-level inspection is clean across all fourteen H3 C++ files and `git diff --check` passes. code-review-graph remains supplemental only because its Unreal Automation/new-file coverage is incomplete.
5. User-confirmed compilation: the H3 source was compiled into the `PolyQuestEditor` session before the ten Automation runs. H3 has no Editor authoring or PIE validation gate.

## Closeout

1. `TODO-03H3` is complete. `ROADMAP.md` records the durable test-only startup ownership and removes the resolved signal-hygiene debt; its next approved stage is `TODO-03A6B`.
2. No H3-specific runtime or validation debt remains. The warning ledger deliberately retains only asserted negative paths: invalid reaction tags, rejected Stance Break fallback with Poise restore, equipment preflight/active-swap/rollback, and static Trace fail-closed geometry.
3. Main's single defect-first fresh review found no P0-P2. code-review-graph was supplemental only because Unreal Automation macro and newly added-file coverage is incomplete; direct source/caller review, Rider error-level inspection, `git diff --check`, user compilation, and the ten-suite matrix are the acceptance evidence.

## Commit Boundary

Default commit scope: the private test fixture, test GE, necessary `WITH_DEV_AUTOMATION_TESTS` header/source surfaces, six migrated Automation files, and the four project documents. Do not stage any user-owned WIP or authored asset/config closure.
