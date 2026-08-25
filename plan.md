# TODO-03A6C: OffHand Presentation And Shield Guard Locomotion Decoupling v1

## Plan State

- Status: Completed. The user confirmed the post-cleanup Editor compilation/readback, fourteen-suite Automation matrix, and focused PIE; Main completed one defect-first fresh review with no P0-P2. The user has approved documentation closeout and this scoped commit.
- Baseline: `cade92a54fcf7a968d4e13be77279f94223a40eb` (`[Feature] 完成可移动玩家拉弓 (Complete Mobile Player Bow Draw)`).
- Objective: separate MainHand base locomotion, durable Shield-equipped presentation, and active Shield Guard presentation so Shield Guard can be selected without encoding every MainHand/OffHand pair as a new locomotion mode.
- Preserve every unrelated user WIP. Do not modify, stage, move, delete, infer behavior from, or include `Content/**`, Config, maps, Blueprints, input, AnimBPs, GA/GE/Montage assets, `.uproject`, generated output, or imported-resource changes unless the user later explicitly approves their stable closure.

```text
Outer: ue-stage-workflow
Primary: ue5-cpp-gameplay
Support: ue5-blueprint-workflow, ue5-debug-validation
Route reason: this stage changes one committed equipment-state query and one MainHand-only locomotion query, while the user-owned AnimBP consumes those facts with the established Shield Guard Gameplay Tag.
```

```text
Plan explorers: 0
Implementation executors: 1 (Gemini only after its read-only plan review is accepted and the user explicitly authorizes execution)
Complex Executor: Gemini external executor for one frozen Equipment query and Automation slice
Main parallel work: none
Reason: committed OffHand state, public BlueprintPure queries, asset-facing Guard-state consumption, and transaction regression coverage are one integrated contract. Main owns the plan, contract decisions, documentation, validation interpretation, fresh review, staging, and commit; Gemini may write only the frozen source/test slice.
```

## Evidence And Decisions

- The first 03A6C implementation has already made `UWeaponEquipmentComponent::GetResolvedLocomotionMode()` MainHand-only, but the residual `RequiredMainHandLocomotionMode` / `CompositionLocomotionMode` fields still participate in `UOffHandWeaponDefinition::IsValidWeaponDefinition()`. `UWeaponEquipmentComponent::RunPreflight()` calls that validation for every prospective OffHand, so these are live equipment-preflight dependencies rather than harmless unused metadata.
- The user has saved the actual Shield DataAsset with both residual fields set together to `Default`, while retaining `Provides Shield Presentation = true`, `Locomotion Mode = Default`, `Hand Slot = Off Hand`, its Defense Profile, and its granted actions. That prepares a narrow source cleanup without changing current authored gameplay behavior.
- `PolyQuest.Equipment.TransactionMatrix` still assigns and tests the residual pair, so enum removal, OffHand validation removal, and matrix migration must happen as one atomic source/test slice.
- The current Player's `WeaponEquipment` component is `VisibleAnywhere, BlueprintReadOnly`, so `ABP_Player_Dungeon` can consume a new component query without a new Player API, delegate, Tick, or replicated state.
- Offline Rider CDO readback confirms `GA_PlayerShieldGuard` overrides its `ActivationOwnedTags` to exactly `State.Action.Guarding.Shield`; `GA_Guard_Sowrd` has no such override. The child tag hierarchically satisfies existing generic `State.Action.Guarding` checks, so Guard gameplay authority does not require a native change.
- `Ability.Defense.Guard.Shield` identifies the defensive ability route supplied by a Defense Profile. It is not evidence that a Shield is currently equipped. Conversely, generic `State.Action.Guarding` cannot choose the Shield-only locomotion because single-sword Guard also owns it.
- The existing full-body `BS_Shield_Walk_Run` is the accepted active Shield Guard branch. It must remain distinct from the single-sword upper-body Guard route because the Shield stance includes torso, hips, and lower-body orientation.

## Frozen Runtime Contract

1. Add `bProvidesShieldPresentation` to `UOffHandWeaponDefinition` as an `EditDefaultsOnly, BlueprintReadOnly` authoring field in `Weapon|Presentation`, defaulting to `false`. It means only: this committed OffHand should expose the v1 Shield presentation fact. It must not inspect Ability Tags, ASC state, DefenseProfile, inventory, or UI.

2. Add `UWeaponEquipmentComponent::HasShieldEquipped()` as one `BlueprintPure` public query. It returns `true` only when the committed `CurrentOffHandWeapon` is a valid `UOffHandWeaponDefinition` whose `bProvidesShieldPresentation` is true. It has no cache, delegate, Tick, tag mutation, equipment mutation, or runtime state of its own.

3. Make `GetResolvedLocomotionMode()` MainHand-only. It still returns `Default` for a missing or invalid MainHand, otherwise the validated MainHand `Default` / `LightSword` / `HeavySword` / `Bow` mode. It must no longer inspect `CurrentOffHandWeapon` or return `SwordShield` for any equipment composition.

4. Complete the residual composition cleanup in this stage. Remove `EWeaponLocomotionMode::SwordShield`, `UOffHandWeaponDefinition::RequiredMainHandLocomotionMode`, and `UOffHandWeaponDefinition::CompositionLocomotionMode`. Preserve the serialized numeric identity of Bow by declaring `EWeaponLocomotionMode::Bow = 4`; ordinal `3` has no valid enumerator and must fail closed in `UWeaponDefinition::IsValidWeaponDefinition()`. Do not introduce a deprecated alias, enum redirect, or a new composition field.

5. `UOffHandWeaponDefinition::IsValidWeaponDefinition()` continues to require an OffHand slot, `LocomotionMode == Default`, and a WeaponMesh, but no longer validates any MainHand/OffHand locomotion pair. Its error text must not refer to composition overrides. This changes no DefenseProfile, BaseGrantedActions, transaction, pickup/drop, or committed-state rule.

6. Do not add a Gameplay Tag or change `UPlayerGuardAbility`, Guard/Parry routing, DefenseProfile precedence, Guard Arc, Stamina, Guard Break, MoveSpeed, B2 lock-facing, input, replication, equipment transaction, pickup/drop, Bow, Dodge, attack, or hit-reaction logic.

7. Shield Guard selection remains a presentation conjunction owned by AnimBP:

```text
bUseShieldGuardLocomotion = HasShieldEquipped()
    && ASC has matching State.Action.Guarding.Shield
```

The exact Shield child tag is the required authored state. A generic Guard tag, `Ability.Defense.Guard.Shield`, a MainHand mode, or merely a left-hand mesh must not replace either half of this conjunction.

## Approved Source And Test Surface

**Contract owner: Main. Implementation writer: Gemini only after explicit execution authorization.** A need to modify an unlisted header/public surface, Player/Guard Ability source, Gameplay Tag, Input route, Config, Build.cs, `.uproject`, or asset is a stop condition requiring Main review.

- `Source/PolyQuest/Public/Combat/Equipment/OffHandWeaponDefinition.h`
  - Contract owner: Main. Implementation writer: Gemini.
  - Retain `bProvidesShieldPresentation`; remove only the two residual composition fields and their validation. Keep `HandSlot`, DefenseProfile ownership, required base `LocomotionMode == Default`, mesh validation, and every unrelated authored property unchanged.

- `Source/PolyQuest/Public/Combat/Equipment/WeaponDefinition.h`
  - Contract owner: Main. Implementation writer: Gemini.
  - Remove only the `SwordShield` enum member, make `Bow = 4` explicit, update the enum description, and collapse the base validation to a generic invalid-enum failure. Do not reorder or renumber `Default`, `LightSword`, `HeavySword`, or `Bow`; do not alter any other DataAsset field or validation rule.

- `Source/PolyQuest/Public/Combat/Equipment/WeaponEquipmentComponent.h`
  - Contract owner: Main. Implementation writer: Gemini.
  - This file contains the already-authorized first 03A6C query change. The supplemental cleanup must not edit it.

- `Source/PolyQuest/Private/Combat/Equipment/WeaponEquipmentComponent.cpp`
  - Contract owner: Main. Implementation writer: Gemini.
  - This file contains the already-authorized first 03A6C query change. The supplemental cleanup must not edit it.

- `Source/PolyQuest/Private/Tests/WeaponEquipmentComponentAutomationTests.cpp`
  - Contract owner: Main. Implementation writer: Gemini.
  - Extend the existing `PolyQuest.Equipment.TransactionMatrix`; do not create a Content-independent replacement fixture or modify global fixture defaults.
  - Remove all residual-pair setup and composition validation cases. Add only a raw ordinal-`3` fail-closed assertion and an assertion that `Bow` retains underlying numeric value `4`, while retaining the existing Shield-presentation, rollback, and Guard CDO Tag coverage. Do not add a `UPlayerGuardAbility` test accessor or change production Guard source solely for test access.

No `PlayerCharacter`, `PlayerGuardAbility`, `DefenseProfileDefinition`, `MeleeWeaponDefinition`, `WeaponEquipmentComponent.h/.cpp`, input, Gameplay Tag config, Build.cs, Config, or Content path is approved for modification by this supplemental cleanup. `PlayerExhaustionAutomationTests.cpp` is unrelated user-owned WIP and must remain untouched and unstaged.

## Automation Contract

Extend the existing transaction matrix with these assertions, without `AddExpectedError`, lowered log levels, or a test-only product fallback:

- A committed OffHand configured with `bProvidesShieldPresentation = true` makes `HasShieldEquipped()` true; no OffHand and an otherwise valid generic OffHand with the default false field return false.
- Unarmed + Shield resolves `Default` plus true Shield presentation; one-handed Sword + Shield resolves `LightSword` plus true Shield presentation; no valid runtime/asset path can select a composition locomotion mode.
- `static_cast<EWeaponLocomotionMode>(3)` fails `UWeaponDefinition::IsValidWeaponDefinition()` with the generic invalid-enum path, and `static_cast<uint8>(EWeaponLocomotionMode::Bow)` remains `4`. This protects existing serialized Bow assets while proving removed SwordShield data cannot silently become a valid mode.
- Equipping a TwoHanded Bow/Heavy weapon clears the OffHand and returns false; world-pickup Apply and Drop rollback restore both the MainHand-only locomotion result and the prior Shield-presentation fact with no drift.
- `GA_PlayerShieldGuard` CDO has the exact child `State.Action.Guarding.Shield` and hierarchically matches generic `State.Action.Guarding`; `GA_Guard_Sowrd` does not have the exact Shield child and retains the generic Guard route. This is authored-asset static evidence, not a PIE assertion.

Run the resulting `PolyQuest.Equipment.TransactionMatrix` plus the remaining thirteen suites through the Unreal Editor Automation front end:

`PolyQuest.Melee.TraceSourceGeometry`, `PolyQuest.Melee.WeaponTrail`, `PolyQuest.Player.ActionWindows`, `PolyQuest.Player.MobileBow`, `PolyQuest.Combat.HitReaction`, `PolyQuest.Enemy.AttackSetSelection`, `PolyQuest.Enemy.CombatSpacing`, `PolyQuest.UI.VitalHUD`, `PolyQuest.Player.Exhaustion`, `PolyQuest.Player.LockOn`, `PolyQuest.Projectile.Lifecycle`, `PolyQuest.Projectile.TargetAssist`, and `PolyQuest.Combat.HitFeedback`.

All fourteen suites must succeed. Retained logs may only be established intentional-negative coverage; 03A6C adds no success-path configuration warning.

## User-Owned Editor Authoring

After static preflight, the user owns every asset change and readback:

1. Completed preparation evidence: on the actual Shield `UOffHandWeaponDefinition`, the user set both residual fields together to `Default` and saved, while retaining `bProvidesShieldPresentation = true`. After source compilation, reopen it and confirm both residual fields are absent; keep the Shield presentation flag, base `LocomotionMode = Default`, Hand Slot, Defense Profile, and granted actions unchanged.

2. In `ABP_Player_Dungeon` Event Graph, preserve the existing cached `WeaponEquipment` and ASC acquisition route. Update the cached ordinary locomotion enum from the component's now MainHand-only `GetResolvedLocomotionMode()`, add `bHasShieldEquipped` from `HasShieldEquipped()`, and derive `bUseShieldGuardLocomotion` from that bool plus the existing ASC tag query for `State.Action.Guarding.Shield`. Do not create a parallel Equipment boolean, tag, or Blueprint-side inventory check.

3. After source compilation, open `ABP_Player_Dungeon`, refresh/reconstruct every `Blend Poses by EWeaponLocomotionMode` node, and inspect the labelled enum pins rather than raw `BlendPose_N` array indices. Remove the obsolete `SwordShield` branch/reference. Confirm the remaining `Default`, `LightSword`, `HeavySword`, and `Bow` branches point to their correct existing assets, especially the Bow branch. Select full-body `BS_Shield_Walk_Run` only when `bUseShieldGuardLocomotion` is true. Route that result through the established final Reaction Overlay so Small Hit remains visible. The Shield Guard branch bypasses the single-sword `DefaultGroup.UpperBody` Guard visual branch; ordinary Sword/Shield locomotion does not acquire a new passive overlay.

4. Preserve Bow's independent aim branch, the current Stride Warping placement on pure base locomotion, and all Root Motion policy. Do not add an Aim Offset, new Slot, passive shield hold layer, shield-back stowage, or a new Guard BlendSpace.

## Static Checks, User Validation, And Closeout

- Before handoff, read the final query and direct transaction callers; use CodeGraph and file-scoped code-review-graph as supplemental evidence; run Rider Error-level inspection on all touched C++ paths and `git diff --check`. These are static checks only. The graph does not prove Blueprint branches or GameplayTag runtime ownership, so direct Automation/Editor readback remains required.
- The user manually compiles `PolyQuestEditor`, runs the fourteen Automation suites, and records the result.
- Editor readback confirms the actual Shield DataAsset no longer exposes the two removed fields, the Shield-presentation flag and DefenseProfile remain intact, the two Guard Blueprint CDO tag containers remain correct, and every `Blend Poses by EWeaponLocomotionMode` node has exactly the current labelled branches with Bow correctly wired. Do not delete `BS_SwordShield_Walk_Run` in this stage; after Reference Viewer proves it has zero referencers, asset deletion belongs to a separate approved Content-cleanup decision.
- In `Scene01`, validate Unarmed + Shield and Sword + Shield ordinary locomotion retain their respective MainHand base routes; Shield Guard for both uses the full-body Shield BlendSpace; single-sword Guard stays upper-body only; Bow/Heavy clears Shield presentation; and release, attack/Dodge cancellation, Guard Break, Small Hit, death, and B2 locked movement leave no stale Shield Guard pose or yaw regression.
- After user-confirmed compile, Automation, Editor readback, and PIE evidence, Main performs one defect-first fresh review. Then Main updates `README.md`, `ARCHITECTURE.md`, `ROADMAP.md`, and this `plan.md` with verified results. The closeout must record that the enum and OffHand composition fields were removed, that Bow keeps serialized value `4`, and that any now-unreferenced legacy Content asset still requires its own evidence-led cleanup approval.

## Commit Boundary

This user-approved commit includes only `WeaponDefinition.h`, `OffHandWeaponDefinition.h`, the already-approved `WeaponEquipmentComponent.h/.cpp`, `WeaponEquipmentComponentAutomationTests.cpp`, and the four synchronized project documents. It explicitly excludes every `Content/**` asset, Config, map, Blueprint, GA/GE/Montage, AnimBP, input, `.uproject`, generated output, imported resource, `PlayerExhaustionAutomationTests.cpp`, and unrelated user WIP.

## Closeout Record

- Runtime contract: `EWeaponLocomotionMode` now exposes only `Default = 0`, `LightSword = 1`, `HeavySword = 2`, and `Bow = 4`. The removed ordinal `3` fails `UWeaponDefinition::IsValidWeaponDefinition()` together with every other invalid enum value, preserving the serialized Bow value without a deprecated alias or enum redirect. `UOffHandWeaponDefinition` retains required OffHand slot, base `LocomotionMode == Default`, display-mesh validation, and `bProvidesShieldPresentation`, but no MainHand/OffHand composition fields or preflight rule.
- Presentation contract: `GetResolvedLocomotionMode()` reads only committed MainHand base locomotion. `HasShieldEquipped()` independently reads only the committed OffHand presentation flag. `ABP_Player_Dungeon` combines the latter with exact active `State.Action.Guarding.Shield` for full-body `BS_Shield_Walk_Run`; generic Guard and the Shield Guard ability tag are not equipped-Shield signals. The ordinary enum branch set is `Default`, `LightSword`, `HeavySword`, and `Bow`.
- Automation and review: `PolyQuest.Equipment.TransactionMatrix` covers the ordinal-`3` failure, preserved Bow value, Shield/non-Shield OffHand presentation, TwoHanded clearing, and Apply/Drop rollback. The user confirmed all fourteen Editor Automation suites and focused Scene01 PIE after the final cleanup. Main re-read the enum, OffHand validation, preflight/commit path, transaction matrix, direct callers, and final diff; CodeGraph, code-review-graph, Rider inspection, and `git diff --check` supplied supplemental static evidence. Main found no P0-P2.
- Asset boundary: the user removed the obsolete `SwordShield` AnimBP branch but retained `BS_SwordShield_Walk_Run` in local `Content/**`. It is not a current runtime selection path and is deliberately excluded from this commit. Any deletion requires a separately approved, Reference Viewer-backed Content cleanup.
- Scope: no Player/Guard Ability, DefenseProfile, Gameplay Tag, input, transaction, pickup/drop, Bow, Stamina, B2 facing, Config, Blueprint, AnimBP source asset, map, or project-file contract changed outside the approved 03A6C surface. The unrelated `PlayerExhaustionAutomationTests.cpp` modification remains uncommitted user WIP.
