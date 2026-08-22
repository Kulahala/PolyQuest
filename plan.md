# TODO-03A6A: Weapon-Aware Ordinary Locomotion Presentation v1 (Completed)

## Plan State

- Status: completed and closed on 2026-08-22.
- Baseline: `df088e3`.
- Outcome: the player ordinary-locomotion presentation now resolves five stable modes: `Default`, `LightSword`, `HeavySword`, `SwordShield`, and `Bow`.

~~~text
Outer: ue-stage-workflow
Primary: ue5-cpp-gameplay
Support: ue5-blueprint-workflow
Route reason: the runtime change is a narrow equipment-owned C++ query and immutable DataAsset contract; the AnimBP consumes that result for presentation only.
~~~

~~~text
Plan explorers: 0
Implementation executors: 1 (Gemini, user-coordinated bounded executor)
Complex Executor: none
Main parallel work: accept the contract, inspect the implementation/self-review, perform fresh and adversarial review, close documents, stage, and commit.
Reason: the public equipment API and its transaction/rollback boundary are shared integration territory. A second writer would not reduce risk.
~~~

## Delivered Contract

### Runtime ownership

1. `EWeaponLocomotionMode` is a reflected `uint8` enum with exactly `Default`, `LightSword`, `HeavySword`, `SwordShield`, and `Bow`.
2. `UWeaponDefinition::LocomotionMode` is the immutable main-hand base family. Valid base values are `Default`, `LightSword`, `HeavySword`, and `Bow`; `SwordShield` is reserved for an OffHand composition and invalid enum values fail closed.
3. `UOffHandWeaponDefinition` owns only composition data. Its inherited base `LocomotionMode` must be `Default`; its two composition fields must be either both `Default` (no override) or exactly `LightSword -> SwordShield`. Half-configurations and all other pairs fail definition validation.
4. `UBowWeaponDefinition` authors and validates `LocomotionMode == Bow` together with its existing TwoHanded Bow contract.
5. `UWeaponEquipmentComponent::GetResolvedLocomotionMode()` is the sole public presentation query. It reads the successfully committed hand definitions and has no cache, Tick, Delegate, equipment notification, Gameplay Tag mutation, or second equipment/action state.
6. Resolution is deterministic: an absent main hand or invalid base-mode enum returns `Default`; a `LightSword` main hand plus a matching Shield returns `SwordShield`; every other OffHand leaves the validated main-hand base mode unchanged.
7. Existing `EquipWeapon` / `TryEquipWorldPickup` transaction, ability grants, prepared slots, dropped pickups, and rollback behavior remain untouched. Because the query reads committed definitions, Apply/Drop rollback automatically restores the prior locomotion mode without a separate visual rollback path.

### Presentation ownership

- `ABP_Player_Dungeon` consumes the pure query only in ordinary Idle and Walk/Run presentation. It retains the established Character, `GroundSpeed`, `Direction`, `ShouldMove`, falling, Foot IK, and Montage-Slot paths.
- Default/Unarmed, LightSword, HeavySword, SwordShield, and Bow each use their authored ordinary-locomotion route with a `0.15s` visual blend. Attack, Bow Draw/Hold/Release, Dodge, hit, death, Guard Break, Guard rules, movement speed, and GAS lifecycle are outside this slice.
- The user-owned authored asset work selected the compatible sequences/BlendSpaces and batch-set `Force Root Lock = True` / disabled root motion for 192 ordinary-locomotion sequences. These `Content/**` assets remain local authoring WIP and are deliberately excluded from the source/documentation commit.

## Source And Test Surface

### Included source and tests

- `Source/PolyQuest/Public/Combat/Equipment/WeaponDefinition.h`
- `Source/PolyQuest/Public/Combat/Equipment/OffHandWeaponDefinition.h`
- `Source/PolyQuest/Public/Combat/Equipment/WeaponEquipmentComponent.h`
- `Source/PolyQuest/Private/Combat/Equipment/WeaponEquipmentComponent.cpp`
- `Source/PolyQuest/Private/Combat/Equipment/BowWeaponDefinition.cpp`
- `Source/PolyQuest/Private/Tests/WeaponEquipmentComponentAutomationTests.cpp`

### Automation coverage

`PolyQuest.Equipment.TransactionMatrix` Section 13 uses real transient definitions and the normal pickup transaction path to cover:

- invalid base `SwordShield` and invalid enum values;
- OffHand half-configurations, invalid pairs, and non-`Default` inherited base mode;
- `Default`, `LightSword`, `SwordShield`, `Bow`, and `HeavySword` resolution;
- TwoHanded-to-Shield normalization returning `Default`, not `SwordShield`;
- Apply and Drop failure rollback preserving the prior resolved mode.

## Validation Evidence

- User-confirmed automation: the relevant seven-suite Automation matrix passed, including `PolyQuest.Equipment.TransactionMatrix`; the expected warnings are intentional negative fixtures for invalid composition and rollback paths, not test failures.
- User-confirmed Scene01 PIE / visual route: Unarmed, LightSword, HeavySword, SwordShield, and Bow ordinary locomotion switch smoothly at `0.15s`, with no T-pose or residual previous-weapon pose observed.
- Static closeout: final direct source/test read, CodeGraph call-path inspection, code-review-graph change/impact supplement, and `git diff --check` were run by Main. These are static evidence only and do not replace the user-owned Automation or PIE proof.

## Review And Repair Record

1. Gemini completed its implementation self-review.
2. Main fresh review found one P2: an `UOffHandWeaponDefinition` could author a non-`Default` inherited base `LocomotionMode`; preflight accepted it while the resolver ignored it. The repair rejects that authoring state in `UOffHandWeaponDefinition::IsValidWeaponDefinition()` and adds the Section 13 negative test.
3. Main delta review of the repair found no remaining P0-P2 issue in the approved source surface.
4. `gpt-5.6-luna / xhigh` was unavailable. The required second pass was therefore a clearly labelled Main adversarial fallback, not an independent Fresh Reviewer result.

## Debt Handoff

- `TODO-03A6B` owns only moving Guard posture, turn/facing alignment, and presentation readability while `State.Action.Guarding` is active. It does not reopen the existing Guard/Parry GAS, Stamina, damage, or defense-resolution contracts.
- `TODO-03C` may proceed after this 03A6A closeout; 03A6B is a separate presentation slice rather than its gameplay prerequisite.
- No additional accepted risk was created by this stage.

## Commit Boundary

The stage commit contains only the six C++/Automation files above plus `README.md`, `ARCHITECTURE.md`, `ROADMAP.md`, and this closeout record. It excludes all user-owned `Content/**`, `Config/**`, `.zcode/**`, `PolyQuest.uproject`, `PlayerCharacter.h`, maps, Blueprints, AnimBPs, BlendSpaces, Montages, DataAssets, input assets, imported resources, and any other local WIP.
