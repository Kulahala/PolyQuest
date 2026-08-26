# TODO-03A3C: World Pickup Drop Grounding And Presentation v1

## Plan State

- Status: Completed. This document records the accepted A3C plan and closeout after the user confirmed the implemented pickup grounding path in PIE and Automation.
- Baseline: `6df9b78` (`[Feature] 完成击飞受击平滑转向 (Complete Launch Facing Smoothing)`) plus Main-owned, unstaged `ROADMAP.md` scheduling `TODO-03A3C` before `TODO-03C`; preserve all other user WIP.
- Objective: make every definition-backed world weapon pickup use the same authorable display transform for placed and runtime-dropped instances, then place its Root so the final rendered mesh remains exactly `2cm` clear of the traced ground along that ground's normal.
- User decision: on slopes, preserve the Definition-authored visual orientation. Only the Root location moves along the ground normal; no mesh rotation is derived from slope orientation.
- Source evidence: `AWorldWeaponPickup::ProjectLocationToGround` currently returns only `ImpactPoint + ImpactNormal * 2.0f`; `StageDisplacedDrops` then deferred-spawns with `FRotator::ZeroRotator`. `UpdateVisualMesh` only assigns `WeaponMesh`, while `UWeaponDefinition` currently holds only hand-socket display offsets. The atomic `UWeaponEquipmentComponent::TryEquipWorldPickup` transaction already rolls back the composition when staging fails and must remain its sole transaction owner.
- Player-facing success: after repeatedly swapping Sword and Shield, the displaced pickup uses its intended authored visual orientation and rests visibly above flat or sloped ground without bounce, falling physics, changed interaction range, or a temporary overlap/collision state.
- Preserve all user WIP. `Content/**`, `Config/**`, `.uproject`, maps, Blueprints, AnimBPs, Gameplay Ability/Effect/Montage assets, imported resources, generated folders, and unrelated source are not executor or commit candidates.

## Route And Delegation

Outer: `ue-stage-workflow`
Primary: `ue5-world-interaction`
Support: `ue5-cpp-gameplay`, `ue5-debug-validation`
Route reason: this is a deterministic world-interaction presentation fix at the pickup Actor / weapon Definition boundary. It must preserve the existing transactional equipment lifecycle while adding a narrow native geometry calculation and focused Automation proof.

Plan explorers: 0
Implementation executors: 1 (Gemini, after this plan's read-only review and Main acceptance)
Complex Executor: none
Main parallel work: none
Reason: the source boundary is small and already traced; a second Explorer would repeat the same call path. The executor writes only the frozen source/test slice, while Main retains public-contract, asset, documentation, staging, and integration ownership.

## Frozen Runtime And Authoring Contract

### Public Definition contract

Contract owner: Main. Implementation writer: Gemini only for the field and validation below.

1. Add `FTransform WorldPickupDisplayTransform = FTransform::Identity` to `UWeaponDefinition` as `EditDefaultsOnly, BlueprintReadOnly` under `Weapon|World Pickup`, with a concise Chinese ToolTip. It is the StaticMeshComponent-relative location, rotation, and scale for a weapon's unequipped world representation.
2. `WorldPickupDisplayTransform` is distinct from `DisplayLocationOffset` and `DisplayRotationOffset`; hand attachment data must not be reused for ground pickup presentation.
3. Extend the inline `UWeaponDefinition::IsValidWeaponDefinition` with `WorldPickupDisplayTransform.ContainsNaN()` plus explicit `FMath::IsFinite` checks for every translation, quaternion, and scale component. Reject non-finite authoring before transaction teardown; Identity remains valid, preserving existing DataAssets until the user authors a non-identity presentation.
4. No Gameplay Tag, Input, Ability, ASC, component ownership, serialization migration, new Definition subclass, or Blueprint-callable API is added.

### Pickup lifecycle contract

Contract owner: Main. Implementation writer: Gemini only for the named functions and private helper.

1. `AWorldWeaponPickup::UpdateVisualMesh()` assigns both the Definition's `WeaponMesh` and `WorldPickupDisplayTransform` to `PickupMeshComponent`. A null Definition clears the mesh and restores the component's relative transform to Identity, preventing stale presentation reuse.
2. Keep the public `ProjectLocationToGround(UWorld*, const FVector&, FVector&, const AActor*)` signature. Internally factor one private raw ground-trace helper returning `FHitResult`, so staging can retain `ImpactPoint` and `ImpactNormal`; the public wrapper retains its existing `+2cm` result for any future caller.
3. Add a private, pure C++ `FWorldPickupGrounding` under `Private/Combat/Equipment/`, with no `UCLASS`, `USTRUCT`, generated header, or reflected API. Its sole static operation is `TryComputeGroundedRootLocation(...)`; it accepts a finite local `FBox`, the final relative display `FTransform`, a finite ground impact point/normal, and the fixed `2.0f` clearance. It transforms all eight box corners, finds the minimum dot product along the normalized ground normal, and returns:
   `ImpactPoint + GroundNormal * (2.0f - MinimumProjection)`.
   The helper initializes its output to zero and returns `false` for invalid bounds, zero/non-finite normals, non-finite transforms, non-finite clearance, or non-finite output.
4. `StageDisplacedDrops` keeps its current horizontal scatter and uses zero Root rotation. For each definition it must trace ground, deferred-spawn the existing runtime class, call `InitializeDroppedPickup` so the final mesh/relative transform is available, obtain the unregistered mesh's local `FBox` through `WeaponMesh->GetBoundingBox()` (with a direct `Engine/StaticMesh.h` include), calculate the final Root location, and pass that final transform to `FinishSpawning`.
5. A valid Definition without a `WeaponMesh` retains the old traced `ImpactPoint + Normal * 2cm` Root location because it has no visible bounds. A display-bearing definition whose bounds or grounding calculation is invalid destroys the current deferred actor plus every already staged provisional drop, clears the provisional list, and returns `false` so the existing full equipment rollback runs. No per-frame Tick, physics simulation, throw impulse, bounce, CCD, extra collision, or actor subclass is introduced.
6. The existing `InteractionSphere` state, provisional `NoCollision -> QueryOnly` commit transition, `FormerOwnerRejectDuration`, `OnPickupConsumed`, and `TryEquipWorldPickup` ordering remain unchanged.

## Approved Paths And Execution Order

### Approved executor paths

1. `Source/PolyQuest/Public/Combat/Equipment/WeaponDefinition.h`
2. `Source/PolyQuest/Private/Combat/Equipment/WorldWeaponPickup.cpp`
3. `Source/PolyQuest/Private/Combat/Equipment/WorldPickupGrounding.h`
4. `Source/PolyQuest/Private/Combat/Equipment/WorldPickupGrounding.cpp`
5. `Source/PolyQuest/Private/Tests/WorldPickupGroundingAutomationTests.cpp`
6. `Source/PolyQuest/Private/Tests/WeaponEquipmentComponentAutomationTests.cpp`

`WorldWeaponPickup.h`, `WeaponEquipmentComponent.h/.cpp`, all Definition subclasses, Build.cs, Config, Gameplay Tags, map assets, Blueprints, DataAssets, and documents are prohibited Gemini targets. Needing any unlisted public surface, Blueprint/asset change, new Tag/Input/Config, transaction-order change, or physics behavior is a stop condition requiring Main evidence and a scope decision.

### Execution order

1. Gemini first reviews this plan read-only against `AWorldWeaponPickup`, `UWeaponDefinition`, the existing TransactionMatrix fixture, and the UE 5.8 StaticMesh local-bounds API. It returns P0-P2 findings, blocking gaps, non-blocking recommendations, and Automation feasibility without edits, builds, Editor access, staging, or commits.
2. After Main accepts that review, Gemini implements the six approved source/test paths only. It must preserve every existing transaction branch and use a finite, non-reflected helper rather than a test-only production hook.
3. Gemini runs Rider diagnostics on every touched C++ path, reviews its final diff and direct callers/callees, and runs `git diff --check`. Its handoff lists changed paths, static evidence, unrun user gates, strict self-review findings, and remaining risks.
4. The user performs Editor authoring/readback, manual Development Editor compilation, Automation, and PIE. Main then performs the default defect-first fresh review before documentation closeout or any commit.

## Validation Matrix

### Native Automation

Add `PolyQuest.Equipment.WorldPickupGrounding` with no Content asset or renderer dependency.

1. Identity bounds on a flat normal produce `2cm` clearance within `0.01cm` tolerance.
2. Non-identity rotation, relative translation, and non-uniform scale still leave every transformed local-bound corner on or above the `2cm` support plane within `0.01cm` tolerance.
3. A sloped normal moves only along that normal and does not alter the supplied display transform; this proves the selected stable-orientation policy without creating a map asset.
4. Invalid bounds, zero/non-finite normals, non-finite transform values, and non-finite clearance fail closed with `OutRootLocation == FVector::ZeroVector`.

Extend `PolyQuest.Equipment.TransactionMatrix` using its existing temporary World, Engine Cube floor, real Sword/Shield meshes, and transactional swaps:

1. Give transient Sword and Shield definitions distinct finite `WorldPickupDisplayTransform` values.
2. Run the existing real `TryEquipWorldPickup` displacement route and inspect spawned `UStaticMeshComponent` instances by type, not by a new production accessor.
3. Assert the dropped mesh and relative transform match the displaced Definition and that its final flat-floor lower bound is at least `2cm` above the floor within an explicit `0.01cm` floating-point tolerance.
4. Retain the existing Apply-failure, injected Drop-failure, collision staging, FormerOwner rejection, AbilitySpec rollback, and slot/locomotion regression assertions.
5. Add a transient invalid `WorldPickupDisplayTransform` Definition validation assertion; preflight must reject it before teardown or drop staging.

### Static gate

- Use CodeGraph for `UpdateVisualMesh`, `ProjectLocationToGround`, `StageDisplacedDrops`, `TryEquipWorldPickup`, `IsValidWeaponDefinition`, and both Automation suites. Use code-review-graph only as supplemental post-implementation impact evidence when its index matches the review baseline.
- Run Rider `get_file_problems` or `lint_files` for every touched file; inspect direct includes, ensure the helper stays private/non-reflected, and run `git diff --check`.
- Confirm no `UWeaponEquipmentComponent` source, GAS/ASC, Tag, Input, Config, Build.cs, Blueprint, DataAsset, map, collision policy, or physics change was made by the executor.

### User-owned Editor and runtime gate

1. Compile `PolyQuestEditor (Development Editor)` manually and report the exact result.
2. Run `PolyQuest.Equipment.WorldPickupGrounding`, `PolyQuest.Equipment.TransactionMatrix`, then the complete Editor Automation matrix.
3. In the Editor, author `WorldPickupDisplayTransform` first on `DA_Weapon_Shield` to match the intended upright shield display. Inspect `DA_Weapon_LightSword`, `DA_Weapon_HeavySword`, `DA_Weapon_Axe`, and `DA_Weapon_Bow`; leave Identity when already correct and author a transform only where visual evidence requires it. Keep `DA_Weapon_Unarmed` at Identity with no display mesh.
4. Read back `BP_WorldWeaponPickup`: it remains the single generic pickup Blueprint, with no new presentation child class or collision/physics override. Existing Scene01 pickup Actor Root transforms remain level placement only; normalize any non-identity Root rotation/scale rather than treating them as weapon presentation data.
5. In `Scene01`, repeat Sword -> Shield and Shield -> Sword swaps on flat ground. Confirm displaced items retain the Definition-authored orientation, never visually pierce the floor, remain interactable after transaction commit, honor the former-owner cooldown, and do not bounce/fall. If Scene01 already has a suitable slope, repeat once there; otherwise the sloped-normal native Automation result is the accepted slope proof and no test ramp/map asset is created.

## Documentation, Debt, And Commit Boundary

- Result: `UWeaponDefinition` now owns `WorldPickupDisplayTransform`; placed and deferred-spawned `AWorldWeaponPickup` instances apply it to their visual mesh. `FWorldPickupGrounding` projects every transformed local-bounds corner onto the normalized ground normal and returns the Root location that preserves the fixed `2cm` visual clearance. Invalid mesh bounds or transform data fail closed through the existing provisional-drop cleanup and equipment rollback path; a valid no-mesh definition preserves the former `ImpactPoint + Normal * 2cm` fallback.
- Validation: the user confirmed the A3C Automation validation and focused Scene01 PIE pickup/drop behavior passed. This closeout does not claim a new separately reported Development Editor compilation result.
- Review: Main's defect-first fresh review found no P0-P2. The only P3 was an unrelated Shield Gameplay Ability asset-path relocation in `WeaponEquipmentComponentAutomationTests.cpp`; the old local assets no longer exist and the new local assets do, so those two path edits remain unstaged user-owned Content-migration support rather than being reverted or included in this A3C commit.
- Follow-up: `TODO-03A3D` owns equipped display scale. Toss trajectories, bounce, rolling, simulated physics, pickup animation, inventory, or persistent world drops remain separate feature decisions with their own lifecycle and validation contracts.
- Commit boundary: include only the six approved A3C source/test paths and Main-owned `plan.md`, `ROADMAP.md`, and `ARCHITECTURE.md` updates. Exclude all user-owned `Content/**`, `Config/**`, `.uproject`, maps, Blueprints, DataAssets, imported assets, and the two Shield path-relocation lines unless a later migration closure explicitly adopts them.
