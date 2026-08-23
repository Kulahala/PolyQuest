# TODO-03H2: Core Combat Runtime Health And Lean Review v1

## Plan State

- Status: Complete. The user confirmed manual `PolyQuestEditor` compilation, all ten focused Automation suites, and focused `Scene01` PIE after the H2 repair.
- Baseline: `7491f53` (`[Feature] 完成弓箭锁定目标优先 (Bow Locked-Target Preference)`).
- Objective: run a read-first health gate across completed Player combat, equipment, lock-on, Bow/Projectile, hit reactions, Enemy combat/AI, and vital HUD. This stage is not a new player loop, balance pass, generic refactor, asset migration, or deletion exercise.
- Current `Content/**`, Config, maps, authored Blueprints, Input, AnimBP, GA/GE/Montage, project-file changes, and other WIP are user-owned. Preserve and exclude them.

~~~text
Outer: ue-stage-workflow
Primary: ue5-debug-validation
Support: ue5-cpp-gameplay (only for a confirmed narrow C++ or Automation repair), unreal-mcp (read-only closure checks only)
Route reason: distinguish real lifecycle defects, test-fixture signal noise, and source/document/evidence drift before making the smallest justified repair.
~~~

~~~text
Plan explorers: 0
Implementation executors: 0
Complex Executor: none
Main parallel work: none
Reason: ASC, input, equipment, target, Projectile, AI, and UI lifecycle boundaries intersect. Main retains integration and documentation ownership; Gemini is an external read-only audit reviewer only.
~~~

## Approved Runtime And Audit Contract

1. Audit `EndPlay`, `EndAbility`, Montage/Timer/Delegate cleanup, ASC Attribute/Tag delegates, possession changes, death, destruction, Projectile shutdown, AI stop, and Widget teardown. A callback must not mutate state after cancellation, death, teardown, or a successor action has taken ownership.
2. Audit the existing ownership boundaries only: input/Gameplay Tags, equipment transactions and rollback, lock death handoff, Bow Release target snapshots, Projectile Homing, camera collision, Enemy weapon displays, HUD rebinding, and Enemy Health-bar hiding. Do not add a targeting, input, GAS, UI, or generic cleanup framework.
3. Read-only asset closure verification is limited to direct `Scene01` runtime references: active GameMode/Player/Controller, mappings, Player equipment/loadout, Bow/Projectile, Enemy AttackSet/Profile/Widget, and hit-reaction GA/GE/Montage. Do not inspect raw import reservoirs or unrelated WIP as an incidental audit. The live Unreal MCP endpoint was unavailable at planning time, so Editor readback remains a user-owned validation gate.
4. Compatibility code such as `BladeTraceBase` / `BladeTraceTip`, fixed weapon displays, and the retained Look route is evidence-only. It is not deleted because of age or line count; removal requires direct source/config plus Reference Viewer zero-reference or a proven replacement closure.
5. Classify every finding as: current blocker, confirmed narrow repair, intentional negative-test signal, or Roadmap debt. Every unresolved accepted item needs an owning milestone or `Known Risks And Validation Debt` entry with evidence, impact, and a closure trigger.

## Audit Result

1. The B3 `Invalid AbilitySpecHandle` warnings were confirmed as fixture-only: Release coverage constructed a transient `UBowDrawFireAbility` and supplied ActorInfo but not the equipment-granted Spec Handle. The fixture now queries the real Bow Spec after `EquipWeapon`, asserts a valid level-one handle, and injects it through `SetTestCurrentSpecHandle()` under `WITH_DEV_AUTOMATION_TESTS`. `SpawnProjectile()` remains unchanged.
2. Main's direct cross-system audit found no P0-P2 lifecycle or ownership defect in Player/Enemy teardown, Ability cancellation, ASC delegates, equipment rollback, lock and Projectile snapshots, camera collision, AI death stop, HitReaction cleanup, or vital HUD rebinding. Gemini's separate read-only audit independently reported `No P0-P2 findings`; it did not modify the worktree.
3. Warning classification: preflight/rollback, invalid trace geometry, invalid multi-tier reaction tags, and rejected Stance Break events are intentional negative coverage. The remaining Player/Enemy missing-authoring messages come from deterministic native fixtures calling `BeginPlay` without the product Blueprint configuration. They are not product-runtime failures; their high volume is recorded in `ROADMAP.md` as a dedicated signal-hygiene debt rather than silently suppressed.

## Validation And Review Evidence

1. User confirmation: manual `PolyQuestEditor` compilation; focused `Scene01` PIE covering the approved combat, lock, Bow/Projectile, camera, AI/death, reaction, and HUD paths; and `Success` for `PolyQuest.Equipment.TransactionMatrix`, `PolyQuest.Melee.TraceSourceGeometry`, `PolyQuest.Player.ActionWindows`, `PolyQuest.Combat.HitReaction`, `PolyQuest.Enemy.AttackSetSelection`, `PolyQuest.Enemy.CombatSpacing`, `PolyQuest.UI.VitalHUD`, `PolyQuest.Player.LockOn`, `PolyQuest.Projectile.Lifecycle`, and `PolyQuest.Projectile.TargetAssist`.
2. Static evidence: Rider error-level inspection found no errors in the two changed C++ files; `git diff --check` passed. CodeGraph/direct caller review is primary. code-review-graph was updated to `7491f53` and reported low impact, but its Unreal Automation macro coverage remains incomplete, so it is supplemental only.
3. Main completed the AGENTS.md-required single defect-first fresh review after accepted validation. No P0-P2 finding remains. A strict/adversarial second pass was not requested; `gpt-5.6-luna` remains unavailable and is not claimed as an independent reviewer.

## Commit Boundary

Default commit scope is the confirmed fixture repair, its Automation coverage, and the four synchronized project documents. Exclude every `Content/**` asset, Config, map, Blueprint, Input, AnimBP, GA/GE/Montage, `.uproject`, generated output, and unrelated WIP. Commit remains subject to explicit user approval.
