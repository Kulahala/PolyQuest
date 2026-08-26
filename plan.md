# TODO-03H4B: Native Slimming, Reuse, And Decoupling v1

## Plan State

- Status: Completed. Slice A passed its user-confirmed twenty-suite Automation matrix and focused `Scene01` PIE, followed by Main delta Fresh Review with no P0-P2. Slice B passed its focused static review with no P0-P2, changes only the Public/Private include boundary, and the user confirmed the required `PolyQuestEditor (Development Editor)` compile. H4B now proceeds through its two approved scoped commits.
- Baseline: `6dd8b84` (`[Feature] 敌人死亡布娃娃受击冲量 (Enemy Death Ragdoll Impact)`).
- Objective: before `TODO-03C`, remove one proven cluster of duplicated Native melee Trace Window mechanics and one proven public-header dependency, without changing gameplay behavior, GAS ownership, serialized assets, or class paths.
- Player-facing scope: Light Attack, Charged Attack, Sprint Attack, and Player Melee Skill must retain their current one-window-at-a-time, stale-Notify-safe, once-only melee delivery and terminal cleanup behavior after the shared mechanical code is reduced.
- Preserve all current user WIP. `.gitignore`, `Config/**`, `Content/**`, maps, Blueprints, AnimBPs, GA/GE/Montage assets, `.uproject`, generated files, and unrelated source changes are neither implementation nor commit candidates.

## Route And Delegation

Outer: `ue-stage-workflow`
Primary: `ue5-cpp-gameplay`
Support: `ue5-architecture`
Route reason: this is a single-module C++ lifecycle refactor with a narrow Public/Private include-boundary cleanup; it has no authored-asset, gameplay-design, or module-split scope.

Plan explorers: 0
Implementation executors: 1 (Gemini, only after its plan review is accepted and the user explicitly authorizes execution)
Complex Executor: one scoped lifecycle-sensitive implementation
Main parallel work: none
Reason: the four player Ability paths will call one shared private helper, so splitting writers would increase lifecycle drift risk. Main owns the contract, scope, validation interpretation, documentation, review, staging, and commits.

## Frozen Scope And Runtime Contract

### Slice A — private player Trace Window lifecycle helper

Contract owner: Main. Implementation writer: Gemini. This slice adds no Public API, reflected type, Gameplay Tag, Input route, DataAsset field, module dependency, or Blueprint class-path change.

1. Add a non-reflected, stateless `FMeleeTraceWindowLifecycle` under `Source/PolyQuest/Private/AbilitySystem/Abilities/` with a paired `.h/.cpp`.
2. The helper owns only these existing mechanical operations:
   - `OpenOrKeepScalar`: clear a closed retained Task, preserve an already-open Task, resolve the current Avatar's `ABaseCharacter` / `UMeleeTraceSourceComponent`, call the existing scalar `OpenMeleeTraceWindow` factory overload, call `ReadyForActivation()`, and clear a synchronously unopened Task.
   - `OpenOrKeepMagnitudes`: perform the same sequence through the existing `TMap<FGameplayTag, float>` factory overload used by Charged Attack.
   - `CloseAndClear`: reset the caller's active Trace Notify weak reference, call `EndTask()` only on its current Task, and clear that caller-owned Task pointer. Keep only this two-reference form: no Task-only overload is added because no approved call site needs it.
3. The helper receives references to each Ability's existing `TObjectPtr<UAbilityTask_MeleeTraceWindow>` and `TWeakObjectPtr<const UAnimNotifyState_AttackTraceWindow>`; it does not own UObject lifetime, cache a Character/ASC/Notify, store `FGameplayEventData`, or introduce a second runtime state source.
4. Convert only these existing `.cpp` call sites:
   - `LightAttackAbility.cpp`
   - `ChargedAttackAbility.cpp`
   - `SprintAttackAbility.cpp`
   - `PlayerMeleeSkillAbility.cpp`
5. Retain each Ability's existing private `OpenTraceWindow(...)` and `CloseTraceWindow()` wrappers. They keep the local preconditions (including Charged's map construction) and forward the mechanical Task operation to `FMeleeTraceWindowLifecycle`; callers such as `EndAbility`, `StartComboEntry`, and Notify handlers do not expand helper parameters inline.
6. Each Ability retains its own `OnTraceWindowBegin/End` payload validation, active-Montage identity check, stale-Notify comparison, `bEndAbilityRequested` gate, damage/cost logic, Cancel Window logic, input behavior, and full `EndAbility()` ownership.
7. Charged Attack retains `bReleaseStarted` and its current Damage/Poise SetByCaller map construction. The scalar callers retain their exact invalid-tag/zero-magnitude and Guard Stamina arguments. The helper must use the same Task factory overload each caller uses today.
8. `UEnemyMeleeAbility` is explicitly excluded. It has the similar pattern but lacks direct real-Ability lifecycle Automation coverage; adding AttackProfile/Enemy fixture machinery merely to include it would expand this slimming slice beyond its approved boundary.

### Slice B — proven header dependency cleanup

Contract owner: Main. Implementation writer: Gemini. This is a separate no-behavior micro-slice.

1. In `MeleeWeaponDefinition.h`, replace the direct `Combat/Input/CombatLoadoutDefinition.h` include with `class UCombatLoadoutDefinition;`.
2. In `MeleeWeaponDefinition.cpp`, add the direct `Combat/Input/CombatLoadoutDefinition.h` include before the existing `AssociatedLoadout->IsRouteTableValid()` use.
3. Do not remove `Abilities/GameplayAbilityTypes.h` from `LightAttackAbility.h`, `ChargedAttackAbility.h`, `SprintAttackAbility.h`, or `PlayerMeleeSkillAbility.h`: their reflected `UFUNCTION` declarations use `FGameplayEventData` by value, so the complete type remains required in those Public Headers. Record this as an audited retained dependency at closeout; do not create a false include-reduction diff.

## Approved Paths, Execution Order, And Stop Conditions

### Approved implementation paths

1. `Source/PolyQuest/Private/AbilitySystem/Abilities/MeleeTraceWindowLifecycle.h`
2. `Source/PolyQuest/Private/AbilitySystem/Abilities/MeleeTraceWindowLifecycle.cpp`
3. `Source/PolyQuest/Private/AbilitySystem/Abilities/LightAttackAbility.cpp`
4. `Source/PolyQuest/Private/AbilitySystem/Abilities/ChargedAttackAbility.cpp`
5. `Source/PolyQuest/Private/AbilitySystem/Abilities/SprintAttackAbility.cpp`
6. `Source/PolyQuest/Private/AbilitySystem/Abilities/PlayerMeleeSkillAbility.cpp`
7. `Source/PolyQuest/Private/Tests/MeleeTraceWindowLifecycleAutomationTests.cpp`
8. `Source/PolyQuest/Public/Combat/Equipment/MeleeWeaponDefinition.h`
9. `Source/PolyQuest/Private/Combat/Equipment/MeleeWeaponDefinition.cpp`

`UEnemyMeleeAbility`, all Public Ability Headers, `UAbilityTask_MeleeTraceWindow`, `AnimNotifyState_ActionWindows`, `Build.cs`, `Config/**`, all assets, project documentation, staging, and commits are prohibited implementation targets. If any unlisted file, public/reflected API, Tag, Input, Config, asset, base-class reparenting, or additional test seam appears necessary, Gemini must stop and return exact source evidence for a Main decision.

### Execution order

1. Gemini reviews this plan read-only against the listed paths, the direct Task factory, and existing trace Automation fixtures. It returns P0-P2 findings, blocking gaps, non-blocking recommendations, and test feasibility; it does not edit, compile, access Editor state, stage, or commit.
2. After user acceptance, Gemini implements Slice A only and runs static checks. It must preserve all current player and task lifecycle semantics.
3. The user compiles and runs the required Automation/PIE gate. Main completes a defect-first Fresh Review of Slice A before any commit.
4. Gemini implements Slice B only after Slice A is accepted. The user compiles, Main performs the focused review, and no behavior test is skipped merely because this is header hygiene.
5. Main synchronizes documentation and prepares the two scoped commits only after all gates are green and the user explicitly authorizes the commits.

## Validation Matrix

### New Automation coverage

Add `PolyQuest.Melee.TraceWindowLifecycle`, using the existing real ASC-hosted `UTestMeleeTrailAbility` and `FCombatAutomationFixture`; do not add production test accessors or change fixtures solely for this stage.

- Scalar route: first open succeeds, a second open keeps the same active Task, a closed retained Task is replaced safely, a synchronous unopened/failed activation leaves a null Task, and close clears Task/Notify ownership.
- Map route: Charged-style SetByCaller-map Task opens and closes through the same helper semantics.
- Cleanup: close releases the active Trail requester and leaves no active Trace Window Task.
- Existing `PolyQuest.Melee.MultiTraceSource` stale-Notify regression remains mandatory evidence that an old Notify End cannot close a successor window.

### Static gate before user handoff

- Read the final helper and all four caller diffs; verify every current gate and factory argument survives unchanged outside the centralized mechanical sequence.
- Use Rider `get_file_problems` or `lint_files` on all touched C++ files, then run `git diff --check`.
- Use CodeGraph for callers/callees and code-review-graph as supplemental impact evidence. If its index is stale, document direct source/diff review as the primary evidence.
- Confirm neither slice changes `Source/PolyQuest/Public/AbilitySystem/Abilities/*.h`, Gameplay Tags, Config, module dependencies, or Content assets.

### User-owned compile and runtime gate

1. Compile `PolyQuestEditor (Development Editor)` after each slice and report the exact result.
2. Run the complete Editor Automation matrix: the current nineteen suites plus the new lifecycle suite, for an expected twenty-suite result, with mandatory focused suites:
   - `PolyQuest.Melee.TraceWindowLifecycle`
   - `PolyQuest.Melee.TraceSourceGeometry`
   - `PolyQuest.Melee.MultiTraceSource`
   - `PolyQuest.Melee.WeaponTrail`
   - `PolyQuest.Player.ActionWindows`
   - `PolyQuest.Equipment.TransactionMatrix`
3. In `Scene01`, validate Light Combo, Charged release, Sprint Attack, and Player Melee Skill: each produces one valid hit window, closes cleanly on montage end/cancel, and does not leave Trail or trace state active. Also verify one ordinary Enemy melee attack remains unchanged as a regression observation.

## Documentation And Commit Boundary

- After both slices pass their gates and Fresh Review, Main updates `ROADMAP.md` and this `plan.md` with the completed H4B result, the intentional Enemy exclusion, and the retained `GameplayAbilityTypes.h` rationale.
- `ARCHITECTURE.md` and `README.md` remain unchanged: this stage creates no durable runtime ownership or public product contract.
- Keep two separate commit boundaries, each only after explicit user authorization:
  1. `[Refactor] 复用玩家近战追踪窗口生命周期 (Reuse Player Melee Trace Lifecycle)` — Slice A source/test paths plus the then-current Main-owned plan record if appropriate.
  2. `[Chore] 瘦身近战定义头文件依赖 (Slim Melee Definition Header Dependency)` — Slice B paths and final H4B documentation closeout.
- No `.gitignore`, `Config/**`, `Content/**`, `.uproject`, map, Blueprint, AnimBP, GA/GE, Montage, imported asset, or user-authored Physics/animation WIP may be staged.

## Execution And Review Record

- Slice A added the private, non-reflected `FMeleeTraceWindowLifecycle` and moved only the repeated Task open/keep/close mechanics from Light Attack, Charged Attack, Sprint Attack, and Player Melee Skill. Each Ability retained its own montage identity, stale-Notify, cancellation, damage/cost, and terminal `EndAbility()` ownership. `PolyQuest.Melee.TraceWindowLifecycle` now covers scalar/map open/keep/close behavior, active-Ability ownership, and scalar/map synchronous endpoint fail-closed cleanup.
- Slice A user evidence: the complete twenty-suite Automation matrix, including `PolyQuest.Melee.TraceWindowLifecycle`, passed; the user also confirmed focused `Scene01` PIE for the four Player melee routes and an ordinary Enemy-melee smoke check. Main delta Fresh Review found no P0-P2. The only non-blocking observation is that the test does not explicitly assert `ActiveAbility->IsActive()` after its bootstrap Task ends; its active-Ability setup and resulting lifecycle coverage are otherwise sufficient, so this is not accepted Roadmap debt.
- Slice B removed the direct `CombatLoadoutDefinition.h` include from `MeleeWeaponDefinition.h`, retained the harmless type forward declaration, and added the full include directly to `MeleeWeaponDefinition.cpp`, where `AssociatedLoadout->IsRouteTableValid()` is invoked. The four Public Ability `GameplayAbilityTypes.h` includes were audited and intentionally retained because their reflected `FGameplayEventData` by-value declarations need the complete type. Main focused Fresh Review found no P0-P2; Rider reported only pre-existing weak style suggestions and `git diff --check` passed.
- `UEnemyMeleeAbility` remains intentionally outside this extraction. Its Trace Source already uses the shared Task/source pipeline, but its Ability snapshots `ActiveDamageGameplayEffectClass` and `ActiveGuardStaminaDamage` from an AI-selected `UEnemyAttackProfile` and currently lacks direct real-Ability trace-lifecycle Automation. Future adoption is a conditional Recommendation in `ROADMAP.md`, not deferred implementation work in this stage.
- Final user gate completed: the user confirmed `PolyQuestEditor (Development Editor)` compilation after Slice B. Slice B has no runtime behavior change, so its compile is the required proof; it does not claim a repeat PIE or Automation run beyond Slice A's already accepted user evidence.

## Non-Goals

- No generic melee/attack base class, Ability reparenting, serialized asset migration, module split, public `UAbilityTask_MeleeTraceWindow` API change, `UEnemyMeleeAbility` extraction, GameplayCue, performance optimization, Niagara/Trace tuning, asset deletion, redirector cleanup, or automatic `Content/**` cleanup.
- A measured bottleneck is required before changing ticking, allocations, traces, Niagara, or component lifetime; this stage does not perform performance work.
