# TODO-03A5B: Static Enemy Weapon Geometry Binding And Display Collision v1

## Status

- **Plan state:** CLOSED - Implementation, transient fixture test refactor, all targeted Automation reruns, Main final fresh/adversarial delta review, documentation synchronization, and explicit commit approval complete.
- **Baseline:** `41c6699` (TODO-03A5 completed; current workspace also contains user-owned Content and project WIP).
- **Prerequisites:** `TODO-03A5` and `TODO-03A3B` are closed. `TODO-03AI` remains a later planning gate for new enemy behavior and is not part of this slice.
- **Primary runtime question:** can a fixed enemy display consume the same authored melee geometry as the player while preserving the existing enemy trace fallback and preventing display collision and character body occlusion from affecting the top-down camera or damage path?

## Summary

The existing enemy `WeaponMesh` remains a Blueprint-owned display component attached to the enemy skeleton. `UMeleeTraceSourceComponent` gains an optional static `UMeleeWeaponDefinition` reference and resolves blade endpoints from the displayed Static Mesh sockets. The definition is used only as a geometry provider on the enemy path; player equipment transactions and enemy GAS remain separate.

The source priority is:

```text
player runtime equipment markers -> valid static enemy definition/socket source -> legacy fixed BladeTraceBase/BladeTraceTip fallback
```

An absent static definition keeps the legacy fallback working. A configured but invalid static definition fails closed, warns once, and never silently falls back to stale markers.
`ABaseCharacter` configures `CapsuleComponent`, `SkeletalMeshComponent`, and `WeaponMesh` to ignore `ECC_Camera` (with `WeaponMesh` also set to `NoCollision` and overlap generation disabled) to ensure top-down SpringArm camera view distance remains stable without shrinking or zooming when character bodies or weapons occlude the view.

```text
Outer: ue-stage-workflow
Primary: ue5-cpp-gameplay
Support: ue5-blueprint-workflow, ue5-debug-validation
Route reason: TraceSource, shared melee geometry validation, and fixed display collision are one lifecycle and failure-policy boundary.
```

```text
Plan explorers: 0
Implementation executors: 0
Complex Executor: none
Main parallel work: none
Reason: the shared TraceSource priority and display lifecycle must be implemented and reviewed by one owner; splitting it risks duplicate fallback or collision semantics.
```

## Locked Decisions And Non-Goals

1. Put the optional static reference on `UMeleeTraceSourceComponent`:
   `TObjectPtr<UMeleeWeaponDefinition> StaticMeleeWeaponDefinition`.
2. Reuse `UMeleeWeaponDefinition`; do not create an enemy weapon definition subclass, a `WeaponComponent` base class, an enemy weapon Actor, or a second combat framework.
3. The static path reads only `WeaponMesh`, `BladeBaseSocketName`, `BladeTipSocketName`, `TraceRadius`, and `BladeSubdivisions`. It must not read or execute player `BaseGrantedActions`, loadout, defense, hand-slot, pickup, inventory, or equipment-transaction fields.
4. Do not attach `UWeaponEquipmentComponent` to enemies. Do not modify `UEnemyMeleeAbility`, Attack Set selection, StateTree topology, Controller cooldown, Notify timing, `UAbilityTask_MeleeTraceWindow`, or `FMeleeHitResolver`.
5. StaticMesh collision is presentation policy only. It is never a damage, overlap, or melee-hit path.
6. Keep `BladeTraceBase` and `BladeTraceTip` during migration. Their removal remains owned by `TODO-06C`, after source and Editor Reference Viewer prove zero users.

## Runtime Implementation

### `UMeleeWeaponDefinition`

Add a narrow geometry-only validation entry point in the existing header/source, such as:

```cpp
bool IsValidStaticMeshTraceGeometry(FString& OutReason) const;
```

It must not call full player equipment validation. It rejects a missing `WeaponMesh`, `bUseOwnerMeshSocketForTrace == true`, incomplete or duplicate socket names, missing sockets, coincident or non-finite socket locations, non-finite/non-positive `TraceRadius`, and `BladeSubdivisions` outside `1..8`.

### `UMeleeTraceSourceComponent`

Add the editor-visible static definition reference and private helpers needed to:

- locate the configured `WeaponMesh` component;
- require it to be a `UStaticMeshComponent`;
- require its Static Mesh to equal the definition's `WeaponMesh`;
- read Base/Tip with `GetSocketLocation()`;
- return the definition's radius and subdivision values;
- warn once and return false for every configured-static failure.

If the static reference is null, retain the existing component-name resolver and attachment checks unchanged. If the static reference is non-null but invalid, stop the trace rather than using the legacy components. A valid player equipment marker remains higher priority than the static path.

### Fixed display collision

Extend `ABaseCharacter::DisableFixedWeaponDisplayCollision()` so a fixed component named `WeaponMesh` explicitly has:

- `NoCollision`;
- all channels ignored, including explicit `ECC_Camera` ignore;
- overlap generation disabled.

This preserves the existing player display contract and prevents fixed enemy weapons from retracting or obstructing the SpringArm camera.

## Native Automation

Add `Source/PolyQuest/Private/Tests/MeleeTraceSourceComponentAutomationTests.cpp` with test name:

```text
PolyQuest.Melee.TraceSourceGeometry
```

Use transient definitions/components and the existing automation conventions; do not add public production setters or require new Content fixtures. Cover:

- valid Socket pair resolves distinct world endpoints;
- radius/subdivision values come from the static definition;
- missing display component, wrong component type, or mesh mismatch fails closed;
- missing/incomplete/coincident/invalid sockets fail closed;
- configured-static failure does not fall back to old markers;
- null static definition preserves old Marker fallback;
- fixed display is `NoCollision`, has no overlap generation, and ignores `ECC_Camera`;
- `PolyQuest.Equipment.TransactionMatrix` remains a required regression test for player dynamic-marker priority.

## Editor Authoring Gate

After the user compiles `PolyQuestEditor`:

1. On the actual enemy weapon Static Mesh, confirm or author the complete `Trace_Base` and `Trace_Tip` Socket pair.
2. Configure the existing `DA_Weapon_Axe` (no enemy-only duplicate) with the pair, radius, and subdivisions.
3. Set `StaticMeleeWeaponDefinition` on the Goblin `MeleeTraceSource` component.
4. Confirm the enemy `WeaponMesh` Static Mesh exactly matches the DataAsset mesh and remains attached to the enemy skeleton with its authored visual offset.
5. Keep the legacy `BladeTraceBase`/`BladeTraceTip` components for fallback testing.
6. Confirm no enemy received `UWeaponEquipmentComponent`, pickup logic, or player input/loadout settings.
7. Preserve `GA_EnemyMelee`, Attack Set, Montages, `AttackTraceWindow`, Hyper Armor, AnimBP, Skeleton, StateTree, and map topology.
8. Read back that no Blueprint or DataAsset has Missing/Unknown references.

## Validation And Review Record

### Main Static Gate

- Re-read `UMeleeWeaponDefinition`, `UMeleeTraceSourceComponent`, `ABaseCharacter`, and `UAbilityTask_MeleeTraceWindow`.
- Verified pure fail-closed validation on `IsValidStaticMeshTraceGeometry` (rejecting empty mesh, `bUseOwnerMeshSocketForTrace`, missing/duplicate/unfound sockets, non-finite/coincident socket locations, non-positive/non-finite radius, invalid subdivisions).
- Verified three-tier resolution in `UMeleeTraceSourceComponent`: (1) player dynamic markers, (2) `StaticMeleeWeaponDefinition` with strict fail-closed on any invalid condition, (3) legacy fallback only when `StaticMeleeWeaponDefinition == nullptr`.
- Verified camera collision policy: `ABaseCharacter` sets `ECC_Camera = ECR_Ignore` on `CapsuleComponent` and `SkeletalMeshComponent` in constructor and `BeginPlay()`, and `DisableFixedWeaponDisplayCollision` explicitly sets `NoCollision`, ignores all channels including `ECC_Camera`, and disables overlap events on `WeaponMesh`.
- Refactored `PolyQuest.Melee.TraceSourceGeometry` to use transient `UStaticMesh`/`UStaticMeshSocket` fixtures (removing uncommitted Content asset dependency) and real `FinishSpawning` actor lifecycle (eliminating non-derived downcasting).
- `git diff --check` passed cleanly with 0 whitespace errors.

### User Compile, Automation, And PIE Evidence

- **Compilation**: Compiled `PolyQuestEditor` with Live Coding (`Ctrl+Alt+F11`) with success.
- **Automation Test 1**: `PolyQuest.Melee.TraceSourceGeometry` passed with `Success` (covers static geometry extraction, fail-closed branches, legacy fallback preservation, and display/body camera collision policy). Its invalid-static-definition warning is an expected negative branch; the spawned enemy fixture also emits its existing invalid Poise-recovery warning because the test does not author a combat recovery effect.
- **Automation Test 2**: `PolyQuest.Enemy.AttackSetSelection` passed 100% with `Success` (regression guard confirming enemy attack selection intact).
- **Automation Test 3**: `PolyQuest.Equipment.TransactionMatrix` passed 100% with `Success` (regression guard confirming player equipment transactions intact).
- **Asset Configuration**: Authored `Trace_Base` and `Trace_Tip` sockets on `SM_Wep_Axe_01`, configured `DA_Weapon_Axe`, and bound `StaticMeleeWeaponDefinition = DA_Weapon_Axe` on `BP_Enemy_Goblin`.
- **Scene01 PIE**: Confirmed Goblin axe attacks accurately hit player and apply Health/Poise damage; confirmed enemy weapon and character body no longer obstruct or push the SpringArm camera.

### Review Record

- **Gemini implementation self-review / strict review**: Verified fail-closed boundary, priority resolution, camera collision policy, transient test fixtures, lifecycle safety, and all requested transient display/socket/radius boundary tests in `PolyQuest.Melee.TraceSourceGeometry`.
- **Main Fresh / Adversarial Final Delta Review (Codex)**: No P0/P1/P2 defect found. The five display/socket branches and the `NaN`/`+Inf` `TraceRadius` assertions now pass with transient fixtures and no Content dependency. The invalid-static-definition and invalid-Poise-recovery messages remain expected negative/test-fixture warnings; the latter is non-blocking log noise.
- **External Independent Review Note**: Luna returned 503 during this session and is not claimed as passed.

### Debt Handoff

- Fixed-component trace marker fallback (`BladeTraceBase` / `BladeTraceTip`) removal remains owned by `TODO-06C`.
- Enemy Combat AI architecture and behavior boundaries (repositioning, ranged behavior, elite variants, boss boundaries) belong to `TODO-03AI`.
- Weapon and combat architecture health and lean review belongs to `TODO-03H1`.
- All authored `Content/**` assets (`SM_Wep_Axe_01`, `DA_Weapon_Axe`, `BP_Enemy_Goblin`, maps, montages) remain local authoring WIP and are excluded from the native/documentation commit.

## Closeout Record

- `ARCHITECTURE.md` records the player dynamic-marker versus enemy static-socket contract, explicit display collision policy, and legacy fallback ownership.
- `ROADMAP.md` marks `TODO-03A5B` Done, retains `TODO-06C` as the evidence-gated fallback cleanup owner, and preserves the `TODO-03AI -> TODO-03H1` order.
- This `plan.md` retains the implementation, validation, review, and debt-handoff evidence until the next accepted stage replaces it.
- `AGENTS.md` records the standing Main/Gemini role split and the narrow single-file exception used for low-risk documentation or test repairs.

Candidate native/documentation paths:

```text
Source/PolyQuest/Public/Combat/Equipment/MeleeWeaponDefinition.h
Source/PolyQuest/Private/Combat/Equipment/MeleeWeaponDefinition.cpp
Source/PolyQuest/Public/Combat/Melee/MeleeTraceSourceComponent.h
Source/PolyQuest/Private/Combat/Melee/MeleeTraceSourceComponent.cpp
Source/PolyQuest/Private/Character/BaseCharacter.cpp
Source/PolyQuest/Private/Tests/MeleeTraceSourceComponentAutomationTests.cpp
ARCHITECTURE.md
ROADMAP.md
plan.md
AGENTS.md
```

Exclude all `Content/**`, `Config/**`, `.uproject`, Blueprints, Montages, AnimBPs, StateTrees, maps, GA/GE assets, imported/generated files, and unrelated user WIP. Do not use `git add -A`.
