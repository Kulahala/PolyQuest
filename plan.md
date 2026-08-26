# TODO-03A3D: Equipped Weapon Display Scale v1

## Plan State

- Status: Completed. The user confirmed the stage Automation and focused PIE passed; Main fresh review found no P0-P2. No separately reported Development Editor compile result is claimed.
- Baseline: `11a56d9` (`[Feature] 修复世界武器掉落落地表现 (Fix World Pickup Drop Grounding)`) plus Main-owned, unstaged `ROADMAP.md` scheduling `TODO-03A3D` before `TODO-02C3K` and `TODO-03C`.
- Objective: add one definition-authored equipped-display scale so a weapon's held StaticMesh, its display-mesh Socket-derived melee sample positions, and the Bow's launch Socket remain spatially coherent when an author changes the weapon's visual size.
- Player-facing success: an author can adjust `DisplayScale` on a Sword, Shield, or Bow DataAsset; the held mesh visibly changes size without changing its hand alignment rule, Sword trace markers still follow the scaled mesh sockets, and Bow projectile spawn origin follows the scaled launch Socket.
- User decision: "同步放大" means the equipped display geometry and its presentation-derived positions scale together. It does not mean a runtime coupling between held presentation and the separate world-pickup presentation transform.
- Preserve all user WIP. `Content/**`, `Config/**`, `.uproject`, maps, Blueprints, AnimBPs, Gameplay Ability/Effect/Montage assets, imported resources, generated folders, and unrelated source are not executor or commit candidates, except for the two explicitly adopted Shield Ability fixture assets below.

## Route And Delegation

Outer: `ue-stage-workflow`
Primary: `ue5-cpp-gameplay`
Support: none
Route reason: this is a narrow DataAsset-to-runtime-`UStaticMeshComponent` presentation contract with no GAS, Tag, input, asset-migration, or component-hierarchy change.

Plan explorers: 0
Implementation executors: 0 (Gemini unavailable; Main directly implemented the frozen three-file slice)
Complex Executor: none
Main parallel work: none
Reason: the public Definition field, display-component application, and transaction fixture form one small, coupled slice. Gemini was unavailable, so Main retained the one-writer lifecycle and integration boundary.

Main owns the public contract, implementation, integration acceptance, documentation, staging, and commit. The three-file source/test implementation below is complete; no public surface beyond the named field, transaction ownership, Tags, Input, Config, assets, Blueprint state, Build.cs, or lifecycle rules changed.

## Source Evidence And Frozen Contract

### Definition contract

Contract owner: Main. Implementation writer: Main for the named `UWeaponDefinition` field and its inline validation.

1. Add `FVector DisplayScale = FVector::OneVector` to `UWeaponDefinition` under `Weapon|Display`, with `EditDefaultsOnly`, `BlueprintReadOnly`, and a concise Chinese ToolTip: it controls only the equipped display mesh scale and its display-mesh Socket positions; `WorldPickupDisplayTransform` remains the separate unequipped presentation transform.
2. Extend `UWeaponDefinition::IsValidWeaponDefinition()` to reject every non-finite component and every component that is not strictly greater than `0.0f`. The check order must make NaN/Inf fail closed before positivity comparison.
3. `DisplayScale = FVector::OneVector` preserves all existing DataAssets until the user deliberately authors a different value. Non-uniform positive scaling is supported.
4. Do not alter `WorldPickupDisplayTransform`, its validation, or its scale semantics. Matching world-pickup and equipped size is an author choice, not a runtime multiplication or synchronization rule.

### Equipped presentation contract

Contract owner: Main. Implementation writer: Main in `UWeaponEquipmentComponent::ApplyComposition()`'s existing private `SpawnDisplay` lambda.

1. After the current attachment, `SetRelativeLocation(Definition->DisplayLocationOffset)`, and `SetRelativeRotation(Definition->DisplayRotationOffset)`, call `SetRelativeScale3D(Definition->DisplayScale)` on the new display component.
2. The one existing lambda services both MainHand and OffHand, so Sword, Shield, and Bow use exactly the same application rule. Do not add a second display component, a new test seam, or a new public accessor.
3. Do not multiply or otherwise alter `DisplayLocationOffset` or `DisplayRotationOffset`. They remain independent hand-alignment authoring controls around the same display origin.
4. Keep display-mesh melee marker attachment unchanged: Socket markers retain `FAttachmentTransformRules::SnapToTargetNotIncludingScale`; legacy display-relative markers retain their current attachment rule. In UE 5.8 that rule snaps location/rotation while preserving the marker's own world scale, and `UStaticMeshComponent::GetSocketTransform(..., RTS_World)` resolves the scaled display component's actual Socket world transform. Therefore marker position and Bow origin inherit display scale without stretching marker components or changing collision thickness.
5. Owner-SkeletalMesh trace sources, including the Unarmed body-contact route, remain attached directly to the character mesh and are intentionally unaffected. `TraceRadius` remains a collision-thickness value and is not scaled.
6. Do not modify `TryGetEquippedMainHandDisplaySocketTransform()` or `UBowDrawFireAbility::SpawnProjectile()`. The existing query is already the narrow Bow integration point and must read the scaled live display Socket without a second Bow path.

## Approved Paths And Executor Handoff

### Approved implementation paths

1. `Source/PolyQuest/Public/Combat/Equipment/WeaponDefinition.h`
2. `Source/PolyQuest/Private/Combat/Equipment/WeaponEquipmentComponent.cpp`
3. `Source/PolyQuest/Private/Tests/WeaponEquipmentComponentAutomationTests.cpp`

`WeaponEquipmentComponent.h`, `BowDrawFireAbility.cpp`, all Definition subclasses, `WorldWeaponPickup.*`, `Build.cs`, Config, Gameplay Tags, DataAssets, Blueprints, maps, and documents are prohibited Gemini targets. No asset is edited through the filesystem or live Editor route.

### Execution record and remaining gates

1. Main reviewed the frozen paths, `UBowWeaponDefinition`, the transient Bow fixture, and UE 5.8 attachment-scale semantics before implementation. `SnapToTargetNotIncludingScale` preserves marker scale while the parent display Socket world location remains scale-aware.
2. Main added the field/validation, applied it in the one existing `SpawnDisplay` lambda, and extended `PolyQuest.Equipment.TransactionMatrix` without adding a production test accessor or changing world-pickup transaction behavior.
3. Rider file analysis reported zero errors for all touched C++ files, and `git diff --check` passed for the approved scope. Code-review-graph's generic test-gap label remained supplemental metadata only; direct test-source coverage was the acceptance evidence.
4. The user confirmed the stage Automation and focused PIE gates passed. Main then completed the defect-first fresh review with no P0-P2 findings. No separate Development Editor compile result was reported.

### Existing WIP boundary

The two existing Shield Gameplay Ability fixture-path changes are adopted into this stage: the old `/Game/_Abilities/Player/Attack/Shield/` paths no longer resolve, while `Content/_Abilities/Weapon/Shield/GA_PlayerShieldGuard.uasset` and `GA_Guard_Sowrd.uasset` are the Automation-loaded current assets. Main does not edit the assets. The source test paths remain pointed at the new assets, and this closeout commits exactly those two authored fixtures with the source-path migration after a staged Git LFS pointer check.

## Validation Matrix

### Native Automation

Extend the existing `PolyQuest.Equipment.TransactionMatrix` with its current real Hero/Sword/Shield fixture and Section 13 transient Bow mesh/Socket fixture.

1. Give the transient Sword, Shield, and Bow definitions distinct finite non-unit `DisplayScale` values and deliberately keep their `WorldPickupDisplayTransform.Scale` values different.
2. After real equipment composition, find the player-owned MainHand and OffHand `UStaticMeshComponent` instances and assert their relative scales equal their respective Definitions. Assert their relative location and rotation still equal the existing Definition offsets.
3. With a scaled Sword, assert `TryGetBladeMarkers()` returns Base/Tip marker locations equal, within explicit floating-point tolerance, to the scaled `Trace_Base` and `Trace_Tip` live Socket world locations. Keep the existing distinct-endpoint assertion.
4. Equip the existing transient `UBowWeaponDefinition` with a non-unit scale. Assert `TryGetEquippedMainHandDisplaySocketTransform(Socket_Bow_Launch, ...)` resolves to the display component's scaled Socket world position, derived from the component transform and the fixture's authored local Socket location.
5. Equip Unarmed with a non-unit `DisplayScale` value and assert its trace markers remain parented to the player SkeletalMesh rather than any display component; no display mesh is created and no owner-mesh source is scaled.
6. Add valid/invalid Definition checks for zero, negative, NaN, and Inf display-scale components. Invalid definitions must fail before teardown or staging.
7. Reuse the existing re-equip and injected Apply/Drop-failure rollback paths to assert restored Sword/Shield displays use their original definition scales. Retain current world-pickup transform and rollback assertions, proving the two presentation axes remain independent.

### Static gate

- Use CodeGraph for `UWeaponDefinition::IsValidWeaponDefinition`, `ApplyComposition`, `TryGetBladeMarkers`, `TryGetEquippedMainHandDisplaySocketTransform`, `UBowDrawFireAbility::SpawnProjectile`, and the TransactionMatrix fixture. Use code-review-graph only as supplemental post-implementation evidence when its index matches the review baseline.
- Run Rider `get_file_problems` or `lint_files` on every touched C++ file, inspect final includes, and run `git diff --check`.
- Confirm no `WorldWeaponPickup`, GAS/ASC, Tag, Input, Config, Build.cs, Blueprint, DataAsset, collision, trace-radius, damage, or physics behavior changed.

### User-owned Editor, compile, and PIE gate

1. Compile `PolyQuestEditor (Development Editor)` manually and report the exact result.
2. Run `PolyQuest.Equipment.TransactionMatrix`, then the full Editor Automation matrix.
3. In the Editor, set `DisplayScale` only on the intended Sword, Shield, and Bow DataAssets. Leave `(1,1,1)` where no correction is needed. Do not use Static Mesh `Build Scale` as an alternative implementation route.
4. In `Scene01`, check Sword/Shield/Bow hand-held size and grip alignment, scaled Sword melee reach/trace behavior, Bow arrow spawn origin, repeated swap/re-equip, and the existing world-drop path. If a changed size needs grip tuning, adjust that weapon's existing `DisplayLocationOffset` separately; do not expect runtime scale to rewrite it.
5. If the world pickup should visually match the held size, author its separate `WorldPickupDisplayTransform.Scale`; verify that choice remains independent from the held `DisplayScale`.

## Documentation, Debt, And Commit Boundary

- `ARCHITECTURE.md` now records stable `DisplayScale` ownership and its deliberate separation from `WorldPickupDisplayTransform`; `ROADMAP.md` records A3D complete and the next accepted sequence as `TODO-02C3K` -> `TODO-03C`. This completed plan remains until the next accepted stage replaces it.
- No new durable debt is created. Per-weapon size and grip values are mutable authoring tuning, not a native balance constant. Physics, dynamic pickup scaling, SkeletalMesh weapon presentation, or auto-normalized imported meshes each require a separate stage.
- This commit includes only the three approved source/test paths, Main-owned documentation, and the two adopted Shield Ability fixtures. Every other current `Content/**`, Config, project, map, Blueprint, animation, and imported-resource WIP remains excluded. The shared test file is staged by explicit path only; `git add -A` is not used.
