# TODO-03A3B: Melee Weapon Sweep Socket Authoring And Calibration v1

## Status

- **Plan state:** CLOSED - implementation, documentation, user validation, repair, and review gates are complete; user approved the focused commit.
- **Baseline:** `451cc73 [Feature] 世界武器拾取与原子掉落交换 (World Weapon Pickup And Atomic Drop-Swap)`.
- **Prerequisite evidence:** `TODO-03A3` is closed. The user confirmed `PolyQuest.Equipment.TransactionMatrix` Success and the Scene01 direct world-pickup/drop-swap route. The current player melee path already owns exactly one runtime Base/Tip marker pair through `UWeaponEquipmentComponent`; `UMeleeTraceSourceComponent`, `UAbilityTask_MeleeTraceWindow`, and `FMeleeHitResolver` consume that pair without knowing how it was authored.
- **Completed evidence in 03A3B:**
  - C++ implemented: `UMeleeWeaponDefinition` trace socket properties and fail-closed all-or-nothing validation against `WeaponMesh->FindSocket`, including distinct Socket names with non-coincident `RelativeLocation` values; `UWeaponEquipmentComponent::ApplyComposition` `SnapToTargetNotIncludingScale` socket attachment; `WeaponEquipmentComponentAutomationTests` socket assertions.
  - Editor authoring verified via live MCP readback: `SM_Wep_Ornate_Sword_02` has `Trace_Base` and `Trace_Tip`; `DA_Weapon_Sword` has `BladeBaseSocketName = Trace_Base`, `BladeTipSocketName = Trace_Tip`.
  - Automation verified: After the coincident-location repair, the user executed `PolyQuest.Equipment.TransactionMatrix` reporting `Success`, including the socket-mode assertions and preserved prior composition on rejection.
  - Review: Gemini completed the requested strict review. Codex's initial fresh review found the coincident-location Socket P2; after repair and the user Automation rerun, Codex delta fresh review found no P0/P1/P2. `code-review-graph` was built at `7093c30`, behind this stage baseline `451cc73`, so direct source/diff inspection was the authoritative coverage fallback.
- **Problem:** displayed `UMeleeWeaponDefinition` assets currently author `BladeBaseMarkerRelativeLocation` and `BladeTipMarkerRelativeLocation` as raw vectors. Those values are hard to calibrate because the author cannot position them in the weapon mesh viewport.

## Objective

For a displayed StaticMesh melee weapon that opts into Socket authoring, make its existing runtime Blade Base/Tip marker pair attach to two named Static Mesh sockets. The current Sword fixture becomes the first migrated asset, so the blade-root and blade-tip sweep endpoints can be visually adjusted in the Static Mesh Editor while all damage delivery remains on the existing Trace Window -> `UMeleeTraceSourceComponent` -> `UAbilityTask_MeleeTraceWindow` -> `FMeleeHitResolver` path.

The primary runtime question is not whether a weapon can own combat collision. It is whether the existing component-owned marker pair can resolve from an authored Static Mesh socket pair, fail before a swap when the authored pair is invalid, and remain correct after the current legal equipment transaction rebuilds the display.

## Route And Delegation

```text
Outer: ue-stage-workflow
Primary: ue5-cpp-gameplay
Support: ue5-blueprint-workflow, ue5-debug-validation
Route reason: UMeleeWeaponDefinition owns the authored data contract; UWeaponEquipmentComponent creates the transient display/marker pair; the user-owned Static Mesh and DataAsset edits need exact Editor readback and the existing automation/PIE paths need focused proof.
```

```text
Plan explorers: 0
Implementation executors: 0
Complex Executor: none
Main parallel work: none
Reason: Socket data validation, equipment preflight, transient marker attachment, the local asset fixture, and transaction regression all meet at one composition lifecycle. Splitting writers would duplicate ownership or make the test fixture drift.
```

After this plan is accepted, Gemini may implement the bounded native/test slice. The user remains the sole owner of Static Mesh/DataAsset authoring, manual `PolyQuestEditor` compilation, Automation execution, Scene01 PIE, and final commit approval. Gemini performs the requested strict review after user validation; Codex performs a separate fresh review before documentation closeout.

## Fixed Decisions

| ID | Decision | Consequence |
| --- | --- | --- |
| D1 | Add `BladeBaseSocketName` and `BladeTipSocketName` to `UMeleeWeaponDefinition`; their values, not a hard-coded C++ convention, select a displayed weapon's Static Mesh trace sockets. | Future Sword/Katana/Axe assets can use different socket names if needed. The current Sword fixture uses the authoring convention `Trace_Base` and `Trace_Tip`; test constants verify that fixture only. |
| D2 | Socket mode is an all-or-nothing pair: both names are `None` for existing relative-marker mode, or both are non-`None`, distinct, resolvable through `WeaponMesh->FindSocket`, and non-coincident in local `RelativeLocation`. | A half-configured pair, duplicate names, missing mesh socket, or spatially coincident endpoint pair fails `IsValidWeaponDefinition()` and therefore the shared equipment preflight before handles, displays, markers, or world pickups mutate. |
| D3 | Socket mode is only for displayed StaticMesh melee definitions. `bUseOwnerMeshSocketForTrace` remains the explicit Unarmed branch and requires both new mesh-socket names to be `None`. | `DA_Weapon_Unarmed` continues to use its owner SkeletalMesh `AttachSocketName` plus the current relative Base/Tip values. No second hand-contact or Trace path is introduced. |
| D4 | In socket mode, the component attaches the existing transient marker components directly below the already-positioned `UStaticMeshComponent` at the authored socket names, using the socket transform and no DataAsset marker offset. | `DisplayLocationOffset` and `DisplayRotationOffset` still position the entire weapon. The old marker vectors are ignored in socket mode, so a visual socket adjustment is the sole endpoint adjustment. |
| D5 | Preserve the current raw-vector marker mode for non-adopted displayed melee assets during the staged migration; do not add a new boolean, asset redirect, or automatic conversion. | Sword is the only required migration in 03A3B. `DA_Weapon_Axe`, `DA_Weapon_TwoHandedFixture`, debug fixtures, and future meshes remain valid in legacy mode until their own adoption is approved. The eventual removal condition is documented below rather than silently changing their behavior. |
| D6 | Extend the existing local-asset-dependent `PolyQuest.Equipment.TransactionMatrix` instead of creating a parallel combat test world or a mock Trace/Resolver test. | The suite already loads the real Hero and Sword meshes, exercises `RunPreflight`, direct equip, world pickup, rollback, and `TryGetBladeMarkers`. It will gain the specific socket assertions while retaining its existing transaction coverage. |
| D7 | `BP_Weapon` is not introduced. Static Mesh sockets are an authoring aid, not combat authority. | No weapon Actor-owned hit collision, overlap damage, physics interaction, alternate trace task, alternate resolver, new Gameplay Tag, or Blueprint EventGraph combat logic is permitted. |

## Existing Call Path And Ownership

```text
DA_Weapon_Sword
  BladeBaseSocketName / BladeTipSocketName
          |
          v
UMeleeWeaponDefinition::IsValidWeaponDefinition
          |
          v
UWeaponEquipmentComponent::RunPreflight
          |
          v
UWeaponEquipmentComponent::ApplyComposition
  transient UStaticMeshComponent -> transient Base/Tip USceneComponents
          |
          v
UMeleeTraceSourceComponent::TryGetBladeEndpoints
          |
          v
UAbilityTask_MeleeTraceWindow -> FMeleeHitResolver
```

| Owner | Responsibilities in 03A3B | Explicitly not responsible for |
| --- | --- | --- |
| `UMeleeWeaponDefinition` | Stores the two optional Static Mesh socket names, declares whether the displayed definition uses a complete socket pair, and validates the pair against its `WeaponMesh`. | Equipped state, marker components, damage, collision, hit delivery, sockets on the character Skeleton, or a generic weapon framework. |
| `UWeaponEquipmentComponent` | After shared preflight has accepted the definition, creates the same transient Base/Tip marker objects and attaches them to the display mesh at the named sockets. It retains the existing Unarmed owner-SkeletalMesh branch and legacy displayed-vector branch. | Reading raw mesh geometry, editing Static Mesh assets, changing Trace Window behavior, or supplying fallback endpoints after an invalid socket pair. |
| `UMeleeTraceSourceComponent`, `UAbilityTask_MeleeTraceWindow`, `FMeleeHitResolver` | Continue consuming the component's live marker locations, sampling movement, and resolving the current hit contract. | Knowing socket names, reading DataAssets, spawning display components, or adding a second melee delivery path. |
| User-authored `SM_Wep_Ornate_Sword_02` and `DA_Weapon_Sword` | Own the visual socket locations and their DataAsset references. | Native equipment transaction logic, GAS grants, damage, animation timing, or commit ownership. |

## Native Implementation

### 1. `UMeleeWeaponDefinition` Data And Validation

Modify `Source/PolyQuest/Public/Combat/Equipment/MeleeWeaponDefinition.h` and add `Source/PolyQuest/Private/Combat/Equipment/MeleeWeaponDefinition.cpp`.

- Add `FName BladeBaseSocketName` and `FName BladeTipSocketName` under a dedicated `Weapon|Trace Sockets` category. Their default is `NAME_None`; C++ must not default them to `Trace_Base`/`Trace_Tip`, because that would silently force every existing displayed DataAsset into socket mode before it has authored sockets.
- Add one narrow non-UFUNCTION query, `UsesDisplayMeshTraceSockets() const`, which is true only when the definition is not an Unarmed owner-mesh source and both names are non-`None`. This is a data-mode helper, not runtime state.
- Move the current inline `IsValidWeaponDefinition()` implementation into the new `.cpp` so it can include `Engine/StaticMesh.h` and use the UE 5.8 `UStaticMesh::FindSocket(FName)` API without widening public includes.
- Preserve existing base validation, WeaponMesh/Unarmed exclusivity, trace radius/subdivision rules, and `AssociatedLoadout` validation. Extend the validation in this order:
  1. If `bUseOwnerMeshSocketForTrace` is true, reject a non-null `WeaponMesh` as today and reject either Static Mesh socket name being set. Continue requiring distinct relative Base/Tip positions for this Unarmed path.
  2. For a displayed melee definition, continue requiring a non-null `WeaponMesh`.
  3. If exactly one socket name is set, reject the incomplete pair.
  4. If both names are set, reject equal names, either `WeaponMesh->FindSocket(...) == nullptr`, or socket `RelativeLocation` values equal within `KINDA_SMALL_NUMBER`. In this valid socket mode, do not require the legacy relative vectors to differ because they are intentionally unused.
  5. If both names are `None`, preserve the existing distinct-relative-vector validation for non-adopted displayed definitions.
- Validation must remain pure and fail closed. It must not create components, mutate the mesh, repair missing sockets, substitute `AttachSocketName`, or fall back from a requested socket pair to raw vector offsets.

### 2. Existing Marker Creation Path

Modify only `Source/PolyQuest/Private/Combat/Equipment/WeaponEquipmentComponent.cpp`.

- Keep `RunPreflight()` as the common failure boundary used by `EquipWeapon`, `TryEquipWorldPickup`, and the existing automation helper. The new definition validation must run there before `TeardownEquippedWeapons()`.
- Keep `SpawnDisplay()` and all display placement behavior unchanged: the weapon still attaches to the character `AttachSocketName`, then uses its existing display location/rotation offsets.
- In `ApplyComposition()`, retain the current Unarmed `bUseOwnerMeshSocketForTrace` branch unchanged.
- In the displayed-melee branch, select one of two modes after `NewMainHandDisplay` exists:
  - **Socket mode:** create the same transient Base/Tip `USceneComponent`s, attach each to `NewMainHandDisplay` with `FAttachmentTransformRules::SnapToTargetNotIncludingScale` and its corresponding authored socket name, and do not call `SetRelativeLocation` from the legacy marker vectors.
  - **Legacy vector mode:** keep the existing attachment below `NewMainHandDisplay` plus `BladeBaseMarkerRelativeLocation`/`BladeTipMarkerRelativeLocation` exactly as it behaves today.
- Do not modify component teardown, ability grants, prepared slots, active Loadout selection, world-pickup staging, rollback, input routing, or `TryGetBladeMarkers()`. Both valid displayed modes expose the same two marker components to the existing trace source.
- Do not touch `UMeleeTraceSourceComponent`, `UAbilityTask_MeleeTraceWindow`, `FMeleeHitResolver`, Gameplay Tags, Character code, World Pickup code, Ability code, collision channels, or Build.cs.

### 3. Automation Coverage

Modify `Source/PolyQuest/Private/Tests/WeaponEquipmentComponentAutomationTests.cpp`; retain the existing `PolyQuest.Equipment.TransactionMatrix` test name and all 11 current transaction sections.

- Define test-only fixture constants for `Trace_Base` and `Trace_Tip`, then assert that the already loaded real `/Game/PolygonDungeons/Meshes/Weapons/SM_Wep_Ornate_Sword_02` resolves both through `FindSocket`.
- Configure the existing transient `SwordDef` and `TwoHandedDef` with that complete socket pair instead of raw vector endpoints. They continue using the same Sword mesh and all existing direct-equipment/world-pickup/rollback matrix paths exercise the socket mode.
- After a successful Sword equip, use the existing public `TryGetBladeMarkers()` API to assert that both markers exist, resolve to distinct world locations, and report the expected `GetAttachSocketName()` values. Do not inspect private component fields or introduce test-only runtime behavior.
- Add focused fail-closed preflight cases with an otherwise valid transient melee definition:
  - exactly one socket name set;
  - both names set but one name absent from `SwordMesh`.
  - two distinct Socket names whose `RelativeLocation` values coincide.
  Each must fail through `TestDirectPreflight()` before changing the currently equipped Sword/OffHand/Loadout or component-owned Spec handles; the coincident-location case must also prove direct `EquipWeapon()` preserves the currently equipped TwoHanded composition.
- Retain the existing Unarmed fixture with both new names unset and preserve the established TwoHanded -> Shield -> Unarmed transaction assertions. This is the narrow regression proof that socket mode did not reinterpret the owner-SkeletalMesh contact source.
- This test remains local-asset-dependent because its real Sword mesh must contain the two user-authored sockets. It is not evidence that a clean checkout without the user-owned Static Mesh/DataAsset WIP can run the same fixture.

## User-Owned Editor Authoring Gate

Do this only after the native source compiles and the new DataAsset fields appear in the Editor. These asset edits are local WIP and are explicitly excluded from the native/documentation commit.

1. Create a restore point for the two target assets. Do not bulk-edit `Content/Assets/`, imported animation packages, enemy meshes, or unrelated weapons.
2. Open `/Game/PolygonDungeons/Meshes/Weapons/SM_Wep_Ornate_Sword_02` in the Static Mesh Editor. Add exactly two Static Mesh sockets:
   - `Trace_Base`: place it at the cutting blade root immediately above the hilt, not at the hand/character attachment socket.
   - `Trace_Tip`: place it at the actual blade tip.
   Socket rotation and scale are not consumed by v1 sweep spheres; author their locations carefully and leave any cosmetic orientation conventional.
3. Save `SM_Wep_Ornate_Sword_02`, then open `Content/_DataAssets/Weapon/DA_Weapon_Sword`.
4. Keep `WeaponMesh` pointing at the same Sword mesh and `bUseOwnerMeshSocketForTrace = false`. Set `BladeBaseSocketName = Trace_Base` and `BladeTipSocketName = Trace_Tip`.
5. Do not tune the legacy Base/Tip FVector fields for this Sword afterward. They are ignored once the complete socket pair is configured. Leave `TraceRadius`, `BladeSubdivisions`, display offsets, action classes, Defense Profile, and Loadout unchanged unless a separate approved tuning issue appears.
6. Read back and save both assets. Confirm no Missing/Unknown Socket warning, the two Socket locations are visibly at blade root/tip, and `DA_Weapon_Sword` shows both names exactly.
7. Do not edit `DA_Weapon_Unarmed`; do not create/migrate `DA_Weapon_Axe`, Katana, Bow, Staff, Shield, enemy definitions, or a `BP_Weapon` in this stage. `DA_Weapon_TwoHandedFixture` may remain legacy-vector mode even though the test uses the same mesh in a transient socket-mode definition.

## Validation Matrix

### Main Static Gate

- Re-read final `UMeleeWeaponDefinition`, `UWeaponEquipmentComponent`, `WeaponEquipmentComponentAutomationTests`, and their direct callers/callees. Confirm the source uses the actual UE 5.8 `UStaticMesh::FindSocket` API and does not accidentally use character SkeletalMesh sockets for displayed weapons.
- Use CodeGraph for `IsValidWeaponDefinition` -> `RunPreflight` -> `ApplyComposition` -> `TryGetBladeMarkers` -> `UMeleeTraceSourceComponent` -> Trace Window, then use `code-review-graph` only as supplemental diff/impact evidence. If its built revision is not the review baseline, record stale-coverage fallback and rely on direct source/diff inspection.
- Search for `BladeBaseMarkerRelativeLocation`, `BladeTipMarkerRelativeLocation`, `BladeBaseSocketName`, and `BladeTipSocketName` to confirm exactly the intended dual-mode branches, no C4458 shadowing, and no unwanted Trace/Resolver edits.
- Run `git diff --check`. Main does not invoke UBT, Visual Studio, Unreal Editor, Automation, or PIE.

### User Compile, Editor, And Automation Gate

- Compile `PolyQuestEditor` after the native source change. Report the first compile error verbatim if it fails.
- Complete the Editor readback above. Confirm the actual Sword mesh has `Trace_Base` and `Trace_Tip`, and the actual Sword DataAsset uses that complete pair; this is Editor evidence, not a source-only claim.
- Run `PolyQuest.Equipment.TransactionMatrix`. It must report `Success`, including the new real-Sword socket existence, marker attachment, incomplete-pair rejection, missing-socket rejection, coincident-location rejection with prior-composition preservation, and existing 03A3 transaction/rollback assertions.

### Scene01 PIE Gate

- Start with the migrated Sword equipped. Strike a valid front target during the existing authored Trace Window: damage/Poise must still land once per Trace Window entry, and the sweep should visibly follow the authored blade root-to-tip span.
- Perform a legal idle equipment transaction using the existing world-pickup route to a distinct valid MainHand fixture, then legally return to `DA_Weapon_Sword`. Verify the rebuilt Sword still has normal display placement and its Trace Window still damages a valid target. Do not use a same-definition pickup because it is intentionally a non-consuming rejection.
- Swing into empty space and against a normal existing rejection case (same team or Invulnerable, whichever is already available in Scene01): no false damage delivery must appear. This proves the visual endpoint change did not create a second hit route.
- Run one focused Unarmed regression after a legal return to `DA_Weapon_Unarmed` through an existing debug route or a temporary `BP_WorldWeaponPickup` configured with that definition: the existing punch trace remains on the owner-mesh path and no displayed Sword markers remain. This does not close the separate alternating-hand fidelity debt.
- Re-run the current Light, Charged, Sprint, Guard, Parry, Dodge, and pickup/drop smoke only to the extent they share the equipment/trace transaction. No animation retune is required by this stage.

## Review And Closeout

1. Gemini reviews this plan before implementation.
2. After the user completes compile, Editor readback, Automation, and PIE gates, Gemini performs the requested strict review: defect-first pass plus adversarial pass, scoped to this stage's approved diff and direct dependencies.
3. Codex performs a separate fresh review after Gemini's review/repairs and the affected validation has been repeated. Findings remain defect-first; an absence of findings is not presented as build or PIE proof.
4. Only after all gates pass, update:
   - `ARCHITECTURE.md` with the stable dual authored-source contract: displayed socket-mode weapons validate a named Static Mesh socket pair and attach the existing transient markers there; Unarmed remains owner-SkeletalMesh relative-marker contact; Trace/Resolver ownership stays unchanged.
   - `ROADMAP.md` by moving `TODO-03A3B` to Done, recording the actual Sword Editor/Automation/PIE evidence, and retaining the migration debt below.
   - `plan.md` with implementation, validation, review, and closeout evidence until the next accepted stage replaces it.

### Closeout Record

- Gemini completed its requested strict review. Codex's initial fresh review found that two distinct socket names at the same local location were accepted by preflight and then rejected only by the runtime trace path (P2).
- The repair compares `UStaticMeshSocket::RelativeLocation` values within `KINDA_SMALL_NUMBER` in `UMeleeWeaponDefinition::IsValidWeaponDefinition()` and extends `PolyQuest.Equipment.TransactionMatrix` to reject the fixture through both direct preflight and direct equipment while preserving the prior TwoHanded composition.
- The user reran `PolyQuest.Equipment.TransactionMatrix` with `Success`. Codex's post-repair delta fresh review found no P0/P1/P2. No new UBT, Editor, or PIE run is asserted by Codex after this repair.
- `code-review-graph` was stale at `7093c30` versus the `451cc73` stage baseline, so direct source, diff, CodeGraph, and user-provided Automation evidence were used for the final coverage boundary.

### Accepted Migration Debt

The legacy displayed-weapon vector mode remains intentionally available only for melee definitions whose two new socket names are both unset. Current evidence is that 03A3B migrates Sword only; Axe, Katana, and future displayed mesh weapons have not yet been adopted or visually calibrated. The impact is authoring ergonomics, not a second runtime damage path. Each future mesh weapon adoption must either author a complete socket pair or deliberately stay in legacy mode with a recorded reason. Remove the legacy relative-marker branch only after an Asset Registry/Reference Viewer plus source audit proves every shipped displayed melee definition uses a complete socket pair; the owning lean-removal gate is `TODO-06C`, not this stage.

## Commit Boundary

After explicit user approval, the native/documentation candidate may include only:

- `Source/PolyQuest/Public/Combat/Equipment/MeleeWeaponDefinition.h`
- `Source/PolyQuest/Private/Combat/Equipment/MeleeWeaponDefinition.cpp` (new)
- `Source/PolyQuest/Private/Combat/Equipment/WeaponEquipmentComponent.cpp`
- `Source/PolyQuest/Private/Tests/WeaponEquipmentComponentAutomationTests.cpp`
- Exact `ARCHITECTURE.md`, `ROADMAP.md`, and `plan.md` closeout hunks.

Explicitly exclude all `Content/**`, including the user-authored `SM_Wep_Ornate_Sword_02.uasset` and `DA_Weapon_Sword.uasset`; all GA/GE/Montage/AnimBP/Blueprint/Input/Map work; `.uproject`; generated directories; `.zcode/`; `Config/Tests/Tags.ini`; and unrelated user WIP. The source commit must not claim that it recreates the local socket-authored fixture from a clean checkout.
