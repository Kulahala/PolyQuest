# TODO-03A6D: Multi-Source Melee Trace And Per-Window Selection v1

## Plan State

- Status: Completed. The user confirmed the post-review `PolyQuestEditor` Automation matrix and focused PIE; Main fresh review's right-only/left-only NotifyState regression passed in the final `PolyQuest.Melee.MultiTraceSource` run.
- Baseline: `4f2f43d` (`[Feature] 完成副手表现与盾牌防御动作解耦 (Complete OffHand Presentation And Shield Guard Decoupling)`).
- Objective: replace the Player's singular MainHand Blade Base/Tip assumption with a bounded authored multi-source contract selected by each `UAnimNotifyState_AttackTraceWindow`. The first playable closure is Unarmed `RightFist` and `LeftFist`; one window may select either hand or both hands without creating duplicate damage delivery.
- Execution & Validation:
  - C++ contracts implemented: `FOwnerMeshMeleeTraceSource`, `UWeaponEquipmentComponent` source-keyed marker maps, `UAnimNotifyState_AttackTraceWindow` per-window `TraceSourceNames` array with `OptionalObject2` event routing, `UAbilityTask_MeleeTraceWindow` all-or-nothing multi-source Phase 1/2/3 lifecycle with single-window damage deduplication, `UMeleeWeaponTrailComponent` source-keyed transient child Niagara components.
  - Native Automation: post-review 15/15 automation suites passed (Success), including the expanded right-only/left-only coverage in `PolyQuest.Melee.MultiTraceSource`.
  - User PIE: verified right punch, left punch, and dual-source damage deduplication and trail rendering in `Scene01`.
- Preserve all unrelated WIP. Do not modify, stage, move, delete, or infer product behavior from `.gitignore`, `Config/**`, `Content/**`, maps, Blueprints, AnimBPs, GA/GE/Montage assets, `.uproject`, or generated output. The sole test-only exception is `Source/PolyQuest/Private/Tests/PlayerExhaustionAutomationTests.cpp`: its two local movement constants move into `RunTest()` to avoid colliding with the same anonymous-namespace names in `PlayerMobileBowAutomationTests.cpp` when UE Unity Build groups the translation units. It changes no assertion or product behavior and belongs to this source/test commit.

```text
Outer: ue-stage-workflow
Primary: ue5-cpp-gameplay
Support: ue5-debug-validation, unreal-niagara
Route reason: this stage changes one data-driven equipment contract, the shared AbilityTask trace lifecycle, gameplay-event payload routing, and source-keyed Niagara ownership while preserving the existing GAS resolver authority.
```

```text
Plan explorers: 0
Implementation executors: 1 (Gemini only after its read-only plan review is accepted and the user explicitly authorizes execution)
Complex Executor: one scoped lifecycle-sensitive C++ slice
Main parallel work: none
Reason: named marker construction, task-wide hit dedupe, Notify identity, and Niagara requester teardown are one integrated lifecycle. Main owns the public contracts, plan, documentation, validation interpretation, fresh review, staging, and commit; Gemini may implement only the frozen source/test slice.
```

## Evidence And Decisions

- `UAbilityTask_MeleeTraceWindow` currently owns exactly one previous Base/Tip pair and one task-local `DeliveredTargets` set. Creating one Task per fist would give each fist its own dedupe set and can damage the same target twice in one authored window. The implementation therefore keeps exactly one Task and one shared delivery set per active window.
- `FGameplayEventData::OptionalObject` already carries the source `UAnimSequenceBase` and is used by all five melee abilities to reject stale Montage events. UE 5.8 provides `OptionalObject2`; `UAnimNotifyState_AttackTraceWindow` will place itself there without repurposing `OptionalObject`.
- `UMeleeTraceSourceComponent` currently resolves only singular Player markers or a singular static/legacy Enemy path. `UWeaponEquipmentComponent` owns the transient Player marker lifetime, so named owner-mesh markers belong there rather than in a new Actor or a Tick-driven character system.
- `UMeleeWeaponTrailComponent` has one requester and one Niagara endpoint pair. A multi-source Task would otherwise overwrite its own first hand. The user chose to reuse the current white Niagara System for both fists, so the component must own per-source requester state and additional child components.
- Existing `PolyQuest.Build.cs` already has a private `Niagara` dependency. This stage must not change module dependencies, Gameplay Tags, input, ASC ownership, `FMeleeHitResolver`, damage GameplayEffects, or `ABaseCharacter` default-subobject topology.

## Frozen Runtime Contract

### 1. Authored source data and equipment markers

1. Add `FOwnerMeshMeleeTraceSource` in `MeleeWeaponDefinition.h` with only:
   - `FName TraceSourceName`
   - `FName OwnerMeshSocketName`
   - `FVector BladeBaseMarkerRelativeLocation`
   - `FVector BladeTipMarkerRelativeLocation`

2. Add these `UMeleeWeaponDefinition` fields:
   - `TArray<FOwnerMeshMeleeTraceSource> OwnerMeshTraceSources`
   - `FName DefaultOwnerMeshTraceSourceName`
   - native `TryResolveTraceSourceName(FName RequestedSourceName, FName& OutResolvedSourceName) const`.

3. These fields are valid only with `bUseOwnerMeshSocketForTrace == true`:
   - an empty array retains the existing one-pair legacy route and resolves only `NAME_None`;
   - a nonempty array requires a non-None default that names exactly one entry;
   - every entry requires a non-None unique name and socket, finite non-coincident Base/Tip offsets, and no duplicated source name;
   - a non-owner-mesh weapon rejects a nonempty profile array or a non-None default source name.

4. `UWeaponEquipmentComponent` replaces its singular transient marker members with two transient source-keyed maps, one for Base and one for Tip. It keeps the existing unnamed `TryGetBladeMarkers(OutBase, OutTip)` wrapper and adds the exact named overload `TryGetBladeMarkers(FName TraceSourceName, OutBase, OutTip)`.

5. On an owner-mesh weapon with profiles, `RunPreflight` verifies every named Socket exists on the actual Player mesh before replacement. `ApplyComposition` creates every new marker pair into temporary maps before granting abilities, then publishes the maps only with the successful composition. Failure destroys all temporary markers and follows the existing rollback route. `TeardownEquippedWeapons` destroys every pair and resets both maps.

6. The old singular marker route remains the `NAME_None` entry for Sword, Heavy Sword, legacy Unarmed data, static Enemy fixtures, and any source list left empty. A requested unknown named source must never fall back to `NAME_None` or another hand.

### 2. Per-window source selection and AbilityTask lifecycle

1. Add `TraceSourceNames` as an `EditAnywhere, BlueprintReadOnly` array on `UAnimNotifyState_AttackTraceWindow`, with a native read-only getter. The getter returns the authored array unchanged; only the Task normalizes an empty array into one default-source request. It is authored per NotifyState placement:
   - empty array: one resolved default source;
   - nonempty array: each name is explicit, non-None, and unique;
   - `RightFist`, `LeftFist`, or `[RightFist, LeftFist]` selects the intended contact set.

2. The existing event helper gains an optional `OptionalObject2` argument. Only `UAnimNotifyState_AttackTraceWindow` passes `this`; all unrelated action-window events remain unchanged. `OptionalObject` continues to hold the Animation/Montage identity.

3. Light Attack, Charged Attack, Sprint Attack, Player Melee Skill, and Enemy Melee retain their existing active-Montage identity checks. A Trace Begin/End event must additionally carry a valid `UAnimNotifyState_AttackTraceWindow` in `OptionalObject2`; an unrelated raw event is ignored.

4. Each ability stores the active NotifyState identity only while its trace Task is open. A Begin starts a task with that NotifyState's source array. An End closes only the task started by the same NotifyState. Ability normal completion, cancellation, interruption, and teardown still call the existing unconditional close path.

5. Add source-list overloads for both `UAbilityTask_MeleeTraceWindow::OpenMeleeTraceWindow` factory forms, each taking a final `const TArray<FName>& InTraceSourceNames`. Retain the two existing no-source-list factories as forwarding compatibility overloads with an empty list, while all new Notify-driven paths use the explicit source-list overload. No source data is read from Gameplay Tags, the weapon class name, `bUnarmed`, or input state.

6. At `Activate`, the Task resolves and samples every selected source before setting `bWindowOpen` or starting any Trail. At every `TickTask`, it captures all current source endpoints before it updates Niagara or calls a single Sweep. If any selected source is unknown, duplicated, non-finite, missing, or coincident, it ends the entire Task before delivery for that frame.

7. The Task retains one `FTraceSourceSample` per resolved source, each with previous Base/Tip points. It loops those samples in a deterministic array order but shares its current `DeliveredTargets` set across all sweeps. A target enters that set only after `FMeleeHitResolver::TryResolveHit` succeeds. Separate authored windows create separate Tasks and may hit again as before.

8. `UMeleeTraceSourceComponent` gains name-aware resolution and endpoint APIs while retaining the current unnamed wrappers. Player equipment supports its resolved named profiles. Static-definition and legacy Enemy paths support only the default name and fail closed for a requested named source. Trace radius, channel, and blade subdivisions remain definition-wide values in this v1.

### 3. Source-keyed Niagara ownership

1. `UMeleeWeaponTrailComponent` updates its `StartTrail`, `UpdateTrail`, and `EndTrail` calls to include a resolved source key. `NAME_None` continues to use the existing root `UMeleeWeaponTrailComponent`, preserving all current one-source asset assignment and behavior.

2. A named source uses a lazily created, registered `UNiagaraComponent` attached to the owning Character root. It copies the root component's configured Niagara System, disables auto activation, auto management, and auto destroy, and receives the existing `User.BladeBase` and `User.BladeTip` parameters.

3. Store lazily created child components in a `UPROPERTY(Transient)` source-keyed map; requester records remain weak. Each source has its own weak requester token. The old Task can end only the key it owns, so stale teardown cannot stop a successor on the same source or the other hand. `EndPlay` explicitly deactivates and destroys every child before resetting source state; component/owner teardown remains idempotent.

4. Check for the root Niagara System before creating a child. With no asset, Start/Update/End are silent visual no-ops, create no child component, retain no requester, and never alter hit delivery. The user-selected policy is that `RightFist` and `LeftFist` use the current white Niagara System when one is configured; no per-fist system field is added.

## Approved Source And Test Surface

**Contract owner: Main. Implementation writer: Gemini only after explicit execution authorization.** Any need to touch an unlisted public surface, Config, Build.cs, Gameplay Tag, Input route, Player/Enemy gameplay rule, asset, map, Blueprint, AnimBP, Montage, or documentation is a stop condition requiring a Main decision.

### Shared contracts, Main-owned and Gemini-writable only as frozen below

- `Source/PolyQuest/Public/Combat/Equipment/MeleeWeaponDefinition.h`
- `Source/PolyQuest/Private/Combat/Equipment/MeleeWeaponDefinition.cpp`
  - Add only `FOwnerMeshMeleeTraceSource`, the two profile fields, and profile validation/name resolution. Preserve all current display-mesh trace socket, TraceRadius, BladeSubdivisions, loadout, defense, and base weapon validation behavior.

- `Source/PolyQuest/Public/Combat/Equipment/WeaponEquipmentComponent.h`
- `Source/PolyQuest/Private/Combat/Equipment/WeaponEquipmentComponent.cpp`
  - Replace only the marker storage/query implementation described above. Preserve current equip, world-pickup, prepared-slot, granted-handle, DefenseProfile, swap refusal, and rollback contracts.

- `Source/PolyQuest/Public/Combat/Melee/MeleeTraceSourceComponent.h`
- `Source/PolyQuest/Private/Combat/Melee/MeleeTraceSourceComponent.cpp`
  - Add named source normalization and endpoint resolution only. Preserve current static mesh geometry validation, collision channel, trace-radius, subdivisions, warning policy, and legacy fixture behavior.

- `Source/PolyQuest/Public/AbilitySystem/Tasks/AbilityTask_MeleeTraceWindow.h`
- `Source/PolyQuest/Private/AbilitySystem/Tasks/AbilityTask_MeleeTraceWindow.cpp`
  - Implement exactly one multi-source Task with shared hit dedupe and all-or-nothing sampling. Preserve `FMeleeHitResolver` as the only delivery route and all existing SetByCaller/GuardStamina request data.

- `Source/PolyQuest/Private/Combat/Melee/MeleeWeaponTrailComponent.h`
- `Source/PolyQuest/Private/Combat/Melee/MeleeWeaponTrailComponent.cpp`
  - Implement only source-keyed VFX requester/child lifecycle. Do not rebase the component away from `UNiagaraComponent`, alter `ABaseCharacter`, create a VFX subsystem, or add a GameplayCue.

- `Source/PolyQuest/Public/Animation/Combat/AnimNotifyState_ActionWindows.h`
- `Source/PolyQuest/Private/Animation/Combat/AnimNotifyState_ActionWindows.cpp`
  - Add the per-window array and `OptionalObject2` routing only. Do not change any other Notify semantic, Event Tag, or EventMagnitude behavior.

- `Source/PolyQuest/Public/AbilitySystem/Abilities/LightAttackAbility.h/.cpp`
- `Source/PolyQuest/Public/AbilitySystem/Abilities/ChargedAttackAbility.h/.cpp`
- `Source/PolyQuest/Public/AbilitySystem/Abilities/SprintAttackAbility.h/.cpp`
- `Source/PolyQuest/Public/AbilitySystem/Abilities/PlayerMeleeSkillAbility.h/.cpp`
- `Source/PolyQuest/Public/AbilitySystem/Abilities/EnemyMeleeAbility.h/.cpp`
  - Change only Trace Begin/End extraction, Notify identity tracking, and the private trace-open call to pass source arrays. Do not alter activation checks, Cost, damage values, Montage ownership, tags, movement, cancellation, or any other action window.

### Test-only surface

- Add `Source/PolyQuest/Private/Tests/MeleeMultiTraceSourceAutomationTests.cpp` with `PolyQuest.Melee.MultiTraceSource`.
- Update `Source/PolyQuest/Private/Tests/TestMeleeTrailAbility.h/.cpp` only under `WITH_DEV_AUTOMATION_TESTS` with a helper that closes its default test Task and opens a real replacement Task using an explicit source-name list. It must not mock source resolution or hit delivery.
- Under `WITH_DEV_AUTOMATION_TESTS`, make Trail observation source-keyed as well: independent requester, active flag, endpoint snapshot, and call counts per name. Retain the current no-argument observation accessors as `NAME_None` forwarding helpers so existing single-source tests stay narrow.
- Update `PlayerExhaustionAutomationTests.cpp` only to scope `BaseMoveSpeed` and `ExhaustedMoveSpeed` inside `RunTest()` for Unity Build collision isolation from `PlayerMobileBowAutomationTests.cpp`; do not change its fixtures, assertions, or gameplay contract. Update `MeleeWeaponTrailAutomationTests.cpp`, `MeleeTraceSourceComponentAutomationTests.cpp`, and `WeaponEquipmentComponentAutomationTests.cpp` only when their existing assertions or direct API calls need migration or a narrow regression assertion. Do not alter `CombatAutomationFixture`, global test defaults, or unrelated suites.

## Automation And Validation Contract

### Native Automation

`PolyQuest.Melee.MultiTraceSource` uses the existing deferred Player fixture and re-equips a transient owner-mesh melee definition. The fixture's known valid `Weapon_R` Socket may be used for both test profiles with deliberately different local offsets, so native tests prove the source contract without mutating a loaded skeletal asset. Actual left/right skeletal placement remains a user Editor gate.

The suite must cover all of these without `AddExpectedError`, lowered log levels, mocked resolver delivery, Content Niagara assets, GPU simulation, or a viewport:

1. A valid two-profile composition creates and resolves `RightFist` and `LeftFist`; an empty request resolves the configured `RightFist` default. The old unnamed query still resolves the default legacy path.
2. A right-only and left-only real Task hit only the target on their corresponding prepared geometry.
3. One dual-source Task can contact the same valid target through both source sweeps but applies exactly one successful delivery in that authored window.
4. Unknown source, duplicate requested source, invalid profile, and active coincident endpoint fail closed. The active Task closes, every source-keyed trail request ends, and the failed tick causes no partial delivery.
5. Both named sources can be tracked simultaneously with the same Task requester. A stale first Task ending after a second Task claims `RightFist` cannot stop the successor or `LeftFist`.
6. The no-Niagara-asset Player path creates no child component/requester and continues to trace and resolve normally. Existing default sword and Enemy source paths retain their current behavior.

Run the new suite plus all current fourteen Automation suites through the Unreal Editor front end, for fifteen successful suites total. At minimum inspect the regressions `PolyQuest.Melee.TraceSourceGeometry`, `PolyQuest.Melee.WeaponTrail`, and `PolyQuest.Equipment.TransactionMatrix`; the remaining suites must also remain green.

### User-Owned Editor Authoring And PIE

1. On the Skeleton actually used by `BP_Player`, create and save `Trace_RightFist` and `Trace_LeftFist` sockets at the fist contact regions. These names are authored data, not native hard-coded socket names.
2. In `DA_Weapon_Unarmed`, retain `bUseOwnerMeshSocketForTrace`, add the `RightFist` and `LeftFist` profiles, assign the two sockets, copy the current right-hand offsets to `RightFist`, tune the left-hand offsets, and set `DefaultOwnerMeshTraceSourceName = RightFist`.
3. In each Unarmed Montage's `UAnimNotifyState_AttackTraceWindow`, enter `RightFist`, `LeftFist`, or both based on the actual contact timing. The expected authoring targets are `AM_LightAttack01_Hand`, `AM_LightAttack02_Hand`, `AM_LightAttack03_Hand`, `AM_Unarmed_Charged`, and `AM_Unarmed_SprintAttack`. Keep all sword, heavy weapon, and Enemy Trace Window arrays empty.
4. Confirm `BP_Player` still assigns its existing white `NS_MeleeWeaponTrail` to `MeleeWeaponTrail`; do not create a new Blueprint Actor or per-hand Blueprint component.
5. Manually compile `PolyQuestEditor`. In `Scene01`, verify right-only punch, left-only punch, a controlled two-hand window, normal weapon trace, Enemy trace, interrupted attack cleanup, and no visible stale trail. Report Editor readback, compile, PIE, and Automation evidence separately.

## Non-Goals, Debt, And Commit Boundary

- Do not implement dual-wield attacks, offhand damage, shield hit tracing, separate fist Niagara assets, profile-specific radius/subdivisions, a general inventory system, GameplayCues, new Gameplay Tags, replication, or a second damage path. Future dual wield may reuse this source-name contract only after a separately accepted stage.
- The live Unreal MCP endpoint was unavailable during planning, and Rider's offline asset index did not expose the Unarmed DataAsset values. The exact socket transform and each Montage window's hand assignment therefore remain user Editor evidence, not source-confirmed facts.
- After user validation, Gemini provides an implementation self-review only. Main performs the separate defect-first fresh review, then updates `README.md`, `ARCHITECTURE.md`, `ROADMAP.md`, and this closeout record. Any confirmed unresolved risk must be recorded in `ROADMAP.md` with an owning stage and closure trigger.
- Default commit scope is this stage's C++/Automation/docs only. Exclude all `Content/**`, Config, maps, Blueprint/AnimBP/GA/GE/Montage assets, imported resources, `.uproject`, generated folders, and unrelated WIP unless the user separately approves a stable asset closure.

## Closeout Record

- User validation: the post-review `PolyQuest.Melee.MultiTraceSource` and the remaining fourteen Automation suites all passed. The retained invalid-source, duplicate-source, and coincident-endpoint logs are intentional fail-closed negative coverage. The user also confirmed focused Scene01 PIE for right punch, left punch, dual-source deduplication, and trail rendering.
- Main fresh review: no P0/P1 production defect was found. One P2 test-contract gap was repaired: Case 5 now executes both right-only and left-only NotifyState-selected real Tasks, asserts that the opposite source remains inactive, and verifies one delivery per separate authored window. The final targeted and full Automation matrix passed after that repair.
- Deferred debt: the include-hygiene audit and conditional per-source Niagara/radius expansion are recorded under `TODO-03H4` with concrete adoption conditions. They are not current runtime defects.
- Commit scope: include this stage's C++/Automation source plus `README.md`, `ARCHITECTURE.md`, `ROADMAP.md`, and this plan. Exclude every `Content/**` asset, Config, map, Blueprint/AnimBP/GA/GE/Montage asset, imported resource, `.uproject`, generated folder, and unrelated user WIP.
