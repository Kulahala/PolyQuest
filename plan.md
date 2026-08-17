# TODO-03A4: Off-Hand Shield And Composite Defense Loadout v1

## Status

- **Plan state:** CLOSED - implementation, repaired automation coverage, reviews, and documentation closeout are recorded; explicit commit approval remains pending.
- **Baseline:** `62cab5d [Feature] 武器扫掠 Socket 作者化与校准 (Melee Sweep Socket Authoring And Calibration)`.
- **Prerequisites:** `TODO-03A1`, `TODO-03A2`, `TODO-03A3`, and `TODO-03A3B` are closed. The current player equipment transaction, concrete `UOffHandWeaponDefinition`, `UDefenseProfileDefinition`, equipment-owned ability grants, world pickup/drop-swap path, and socket-authored main-hand sweep path are the required foundation.
- **Primary runtime question:** can an equipped OffHand Shield override only the current Guard/Parry ability selection through the existing equipment component, while the MainHand continues to own Primary/Charged/Sprint attacks and all existing GAS cancellation, Guard Break, held-input, rollback, and cleanup contracts remain intact?

## Summary

`TODO-03A4` adds the first usable OffHand Shield to the established MainHand + OffHand composition. The shield is an authored `UOffHandWeaponDefinition` carrying a pure `UDefenseProfileDefinition`; it does not become a second combat component, a trace source, a damage actor, or a new generic ability framework.

The runtime path remains:

```text
Input.Guard / Input.Parry
    -> APlayerCharacter existing combat-input delivery
    -> UWeaponEquipmentComponent effective defense tag resolution
       OffHand DefenseProfile -> MainHand DefenseProfile -> current generic default
    -> ASC activates exactly one matching granted/default Guard or Parry Ability
    -> existing UPlayerGuardAbility / UPlayerParryAbility lifecycle and cleanup
```

The Shield-specific Blueprint GAs inherit the existing native Guard/Parry lifecycle. They differ only in authored Montage/presentation and their exact Shield identity tags. Existing generic tags remain explicitly present so old cancellation and input-resume contracts continue to see them.

```text
Outer: ue-stage-workflow
Primary: ue5-cpp-gameplay
Support: ue5-blueprint-workflow, ue5-debug-validation
Route reason: Defense Profile validation, component-owned grants, input routing, and Guard/Parry teardown are one GAS/equipment lifecycle boundary.
```

```text
Plan explorers: 0
Implementation executors: 0
Complex Executor: none
Main parallel work: none
Reason: the preflight, grant set, profile resolver, rollback, and existing Defense Ability cancellation contracts share one transaction boundary. Splitting writers would create duplicate ownership of the same live AbilitySpecs and tags.
```

The user may have Gemini review this draft and later implement the accepted C++ slice, but the route record remains zero child agents for this plan. Main retains plan acceptance, integration, validation interpretation, documentation, staging, and commit ownership.

## Locked Decisions

1. **Shield definition type:** retain the existing narrow `UOffHandWeaponDefinition`. It validates only `HandSlot == OffHand`; there is no second `UShieldWeaponDefinition`, trace payload, damage data, runtime state, or additional equipment component. `UWeaponDefinition` stays abstract.
2. **Profile data boundary:** retain `UDefenseProfileDefinition` as a pure Guard/Parry input-to-Ability-Tag DataAsset. This stage introduces no second reflected profile type. A profile stores no Montage, GE, Stamina, collision, damage, active state, or AbilitySpec handle.
3. **Shield identity tags:** add exactly these two Gameplay Tags:
   - `Ability.Defense.Guard.Shield`
   - `Ability.Defense.Parry.Shield`
4. **Shield Ability form:** create no new native Shield Ability. `GA_PlayerShieldGuard` is a Blueprint child of `UPlayerGuardAbility`; `GA_PlayerShieldParry` is a Blueprint child of `UPlayerParryAbility`.
5. **Required Ability tags:** each Shield GA explicitly carries both its legacy parent category and its exact Shield identity:
   - Shield Guard: `Ability.Defense.Guard` and `Ability.Defense.Guard.Shield`.
   - Shield Parry: `Ability.Defense.Parry` and `Ability.Defense.Parry.Shield`.
   The parent tags preserve generic Guard lookup, `UPlayerGuardBreakAbility` cancellation, Guard/Parry cancellation from other combat actions, and the existing held-RMB resume route. The specific tags make the active profile unambiguous.
6. **Existing defaults stay startup-owned:** `GA_Guard_Sowrd`, `GA_PlayerParry`, and Guard Break remain in `BP_Player.StartupAbilities`. Do not migrate them into weapon grants and do not change `APlayerCharacter` or `UPlayerGuardBreakAbility` for Shield routing.
7. **MainHand combat stays MainHand-owned:** Shield grants only its Guard/Parry GAs. It does not replace the active MainHand Primary/Charged/Sprint chain, MainHand trace sources, current Combat Loadout, or base attack Montage selection.
8. **Shield slots are deferred:** `DA_Weapon_Shield` has an empty candidate list and empty prepared `1-4` layout in v1. Shield skill selection at a rest site remains exclusively `TODO-03D1`; this stage does not create permanent Shield `1-4` grants.
9. **Mechanical reuse:** Shield Guard/Parry reuse the existing native ability contracts and current Guard/Parry GameplayEffect configuration. The only v1 presentation change is authorship of Shield-compatible Montages: Guard needs a held loop, and Parry uses the existing `Player Parry Window` NotifyState.
10. **No unrelated presentation or combat expansion:** no weapon-aware locomotion, shield collision/hit trace/damage, bash, block-angle rewrite, new GE, new input asset, `BP_Weapon`, Bow behavior, inventory, Rest-site UI, AnimBP topology, physics asset, map, or enemy change belongs to this stage.

## Current Source Contract And Gap

Static source inspection at the baseline establishes these facts:

- `UWeaponEquipmentComponent::ResolveDefenseAbilityTag()` already resolves the current OffHand `DefenseProfile` first, then the current MainHand profile, then `Ability.Defense.Guard` / `Ability.Defense.Parry` defaults.
- `RunPreflight()` runs before any equipment teardown and already composes both hand definitions' `BaseGrantedActions`, rejects duplicate Ability classes, and validates the prospective transaction before existing handles, display components, markers, loadout, or current definitions change.
- `UPlayerGuardAbility` and `UPlayerParryAbility` are reusable native lifecycle bases. `UPlayerGuardBreakAbility` continues to cancel the generic Guard category, which is why the Shield children must retain the generic parent tag explicitly.

The missing contract is not a second profile resolver. It is preflight proof that a non-default authored Defense Profile actually corresponds to exactly one correctly tagged Guard class and one correctly tagged Parry class in the prospective profile provider's own base grant set.

## Runtime Implementation

### 1. Gameplay Tag Registration

In `Config/Tags/PolyQuestGameplayTags.ini`, add only:

```text
Ability.Defense.Guard.Shield
Ability.Defense.Parry.Shield
```

Do not rename `Input.Guard`, `Input.Parry`, `Ability.Defense.Guard`, `Ability.Defense.Parry`, Guard Break tags, or existing event/state tags. The new tags identify the selected Shield behavior; they do not create a new physical input intent.

### 2. `UDefenseProfileDefinition` Validity

Keep `UDefenseProfileDefinition` data-only. Tighten its existing `IsProfileValid()` predicate so a present profile requires two valid, distinct tags. It remains a small local validity check only; it must not reference Blueprint GAs, Montages, GEs, an ASC, an equipped actor, or mutable equipment state.

An absent `DefenseProfile` remains legal and continues to select the existing generic default tags. A non-null but invalid profile is an authored error and must fail preflight rather than being silently ignored or falling back.

### 3. Prospective Defense-Profile Preflight

Extend `UWeaponEquipmentComponent::RunPreflight()` before teardown or handle mutation. Determine the prospective profile provider from the proposed composition:

```text
NewOffHand->DefenseProfile when present
    else NewMainHand->DefenseProfile when present
    else no profile provider (existing generic-default path)
```

For an explicit provider, fail closed unless all of the following hold:

1. The `UDefenseProfileDefinition` is valid, including distinct Guard and Parry profile tags.
2. The provider's `BaseGrantedActions` contains exactly one Guard candidate whose Ability CDO has both the exact profile Guard tag and the exact generic `Ability.Defense.Guard` tag.
3. The same provider's `BaseGrantedActions` contains exactly one Parry candidate whose Ability CDO has both the exact profile Parry tag and the exact generic `Ability.Defense.Parry` tag.
4. Guard and Parry resolve to two different Ability classes.
5. Missing classes, null CDOs, a class missing either required tag, more than one class for either mapping, one class satisfying both mappings, invalid profile tags, and duplicated profile tags all reject the prospective composition.

Use exact Gameplay Tag checks for this validation. Parent hierarchy alone is insufficient: the Shield profile must select `Ability.Defense.Guard.Shield` or `Ability.Defense.Parry.Shield` deliberately, while the generic tags remain compatibility categories on the same CDOs.

Keep the existing broad composition validation and duplicate-class rejection. The new rule validates only the effective profile provider's own `BaseGrantedActions`; a Sword MainHand may not accidentally satisfy a malformed Shield profile, and an OffHand profile must not borrow a default Startup Ability to pass preflight.

No profile means no new validation or grant requirement: the existing default Guard/Parry fallback stays behaviorally unchanged. A valid explicit profile continues through the existing grant/apply path; no separate Shield grant, resolver, or post-apply repair path is added.

### 4. Existing Input And Ability Lifecycle Remain Intact

Do not modify `APlayerCharacter`, `UPlayerGuardAbility`, `UPlayerParryAbility`, or `UPlayerGuardBreakAbility` for tag routing. After a successful Shield composition, existing input delivery asks the equipment component for the effective tag and activates the existing component-granted Shield GA. The Shield GAs then execute the inherited lifecycle unchanged.

In particular, preserve all of these current contracts:

- `State.Action.Guarding`, `State.Action.Parrying`, Stamina, movement, damage absorption, parry timing, collision/trace, and `EndAbility()` cleanup retain their existing owners.
- Guard Break continues to cancel the generic Guard category; therefore a Shield Guard cannot survive a Guard Break merely because it has a more specific identity tag.
- Held RMB resumes through the established path using the **current** effective profile after Guard Break, interruption, release/cancel, drop-swap, or a successful TwoHanded/OffHand composition change.
- Existing attack/Dodge/Defense cancellation tags continue to see the Shield GAs through their explicit generic parent tags.
- A valid Shield Profile must not cause both a default startup Guard/Parry and a Shield Guard/Parry to activate for one input.

### 5. Native Scope And Expected Paths

Expected native/config changes are limited to:

```text
Config/Tags/PolyQuestGameplayTags.ini
Source/PolyQuest/Public/Combat/Equipment/DefenseProfileDefinition.h
Source/PolyQuest/Public/Combat/Equipment/WeaponEquipmentComponent.h
Source/PolyQuest/Private/Combat/Equipment/WeaponEquipmentComponent.cpp
Source/PolyQuest/Private/Tests/WeaponEquipmentComponentAutomationTests.cpp
```

`WeaponEquipmentComponent.h` may expose only a narrow private/helper or test-only seam needed by the existing local automation fixture. Do not broaden it into public profile editing, weapon inventories, generic interaction, or an alternate combat authority. `UOffHandWeaponDefinition` already exists and is not expected to change unless implementation discovers a concrete defect in its current `HandSlot == OffHand` validation.

## User-Owned Editor Authoring Gate

Do these asset steps only after the native source compiles and the new tags are visible in the Editor. All `Content/**` work remains local authoring WIP and is excluded from the native/documentation commit.

1. Create `DA_Defense_Shield` as `UDefenseProfileDefinition`.
   - `GuardAbilityTag = Ability.Defense.Guard.Shield`.
   - `ParryAbilityTag = Ability.Defense.Parry.Shield`.
   - Confirm the two values are valid and distinct.
2. Complete the existing `Content/_DataAssets/Weapon/DA_Weapon_Shield` as `UOffHandWeaponDefinition`.
   - Keep `HandSlot = OffHand`.
   - Set `DefenseProfile = DA_Defense_Shield`.
   - Set `BaseGrantedActions` to exactly `GA_PlayerShieldGuard` and `GA_PlayerShieldParry`.
   - Keep candidate/reusable/exclusive and prepared-slot arrays empty for v1.
   - Preserve its existing display mesh/socket/offset authoring unless a concrete Shield presentation defect is found. Do not add trace markers or damage data.
3. Create `GA_PlayerShieldGuard` as a Blueprint child of `UPlayerGuardAbility`.
   - In `AbilityTags`, explicitly retain `Ability.Defense.Guard` and add `Ability.Defense.Guard.Shield`.
   - Assign a Shield-compatible Guard Montage with an authored held loop.
   - Reuse the current Guard cost/move-speed/regen/delay configuration required by the inherited ability. Do not create a new Shield-only GE in this stage.
4. Create `GA_PlayerShieldParry` as a Blueprint child of `UPlayerParryAbility`.
   - In `AbilityTags`, explicitly retain `Ability.Defense.Parry` and add `Ability.Defense.Parry.Shield`.
   - Assign a Shield-compatible Parry Montage and place the existing `Player Parry Window` NotifyState only over the intended parry frames.
   - Reuse the inherited/current Parry configuration; no Shield-only cost, cooldown, damage, or reaction contract is introduced.
5. Configure one local `BP_WorldWeaponPickup` fixture to reference `DA_Weapon_Shield`. This is map/presentation authoring only; `AWorldWeaponPickup` and its C++ transaction remain the sole pickup authority.
6. Read the assets back before PIE. Confirm Blueprint parent classes, both exact tag pairs, profile linkage, only two Shield base grants, empty Shield `1-4` arrays, Montages, and no Missing/Unknown Notify or class references.

Do not change `BP_Player.StartupAbilities`, default `GA_Guard_Sowrd`, `GA_PlayerParry`, Guard Break, `GA_PrimaryAttack`, input mappings, `DA_Weapon_Sword`, `DA_Weapon_Unarmed`, `DA_Weapon_TwoHandedFixture`, `DA_CombatLoadout_*`, AnimBP topology, maps, physics, or collision settings as routine Shield setup.

## Validation Matrix

### Main Static Gate

Before requesting user validation:

1. Re-read the final `UDefenseProfileDefinition`, `UWeaponEquipmentComponent`, current input resolver path, Guard/Parry/Guard Break direct cancellation callers, and the automation fixture.
2. Use CodeGraph for the component/profile/direct-caller graph. Use `code-review-graph` as supplemental diff/impact evidence when its index covers the baseline; if stale, record the stale-coverage fallback and review the direct source/diff instead.
3. Cross-check both new tags in configuration, preflight, test fixture, and the two authored-asset requirements. Confirm no code path treats the new specific tags as a replacement for their generic parent categories.
4. Scan added locals and helpers for inherited-member shadowing/C4458 exposure, null CDO handling, and profile/provider confusion.
5. Run `git diff --check` on the approved source/config/documentation paths.
6. Do not run UBT, Visual Studio, Unreal Editor, PIE, or mutate assets. Those gates remain user-owned.

### Native Automation Gate

Extend `PolyQuest.Equipment.TransactionMatrix` using the real local Shield fixture and the existing real Player skeleton/socket fixture. The test must cover, at minimum:

| Case | Required assertion |
| --- | --- |
| Valid Sword + Shield composition | Preflight/application succeeds; Shield Guard and Parry are component-owned base grants; effective Guard/Parry resolution returns the exact Shield tags; MainHand attack grants and current MainHand remain intact. |
| Malformed explicit profile | Null/invalid/duplicate profile tags, no matching provider base action, a class without its generic parent tag, duplicate matches, or one class matching both mappings all fail preflight before any component-owned handle, display, marker, loadout, or current definition changes. |
| Default fallback | A no-profile composition preserves the existing generic Guard/Parry resolution and does not require Shield classes. |
| Shield removal / TwoHanded change | Removing the OffHand through a legal TwoHanded transaction clears Shield grants and resolves the resulting composition's applicable MainHand/default profile without stale Shield handles or tags. |
| TwoHanded -> Shield normalization | Picking an OffHand Shield while holding the existing TwoHanded fixture composes `UnarmedFallbackDefinition + Shield`, drops the TwoHanded definition through the existing transaction, and resolves Shield Defense tags. |
| Atomic regressions | Existing direct equip, world pickup, rollback, projected drop, FormerOwner cooldown, socket-sweep, and active-combat swap-refusal assertions still pass. |

The test must exercise ordinary current behavior, not a mock ASC or a parallel Shield-only transaction. It is local-asset-dependent evidence and must not be presented as clean-checkout fixture proof while the authored asset WIP is excluded from the commit.

### User Compile And Editor Readback

1. Compile `PolyQuestEditor` manually.
2. Run `PolyQuest.Equipment.TransactionMatrix` and report the named result.
3. In the Editor, read back `DA_Defense_Shield`, `DA_Weapon_Shield`, `GA_PlayerShieldGuard`, `GA_PlayerShieldParry`, and the Shield world-pickup fixture. Confirm the exact authoring checklist above and that no asset resolves to Missing/Unknown classes or NotifyStates.

### Scene01 PIE Gate

Use a Sword main hand, the authored Shield pickup, an enemy able to attack, and the existing TwoHanded fixture where applicable.

1. **Sword baseline:** without Shield, RMB and Q still use the existing default Guard/Parry behavior. Light/Charged/Sprint attacks remain Sword-owned; no Shield `1-4` action appears.
2. **Shield override:** pick up Shield while using Sword. RMB plays only Shield Guard and Q plays only Shield Parry; neither input double-activates the old default and Shield ability. Movement, front-angle blocking, Stamina, and existing Guard/Parry recovery behave as before.
3. **Parry timing:** an enemy hit inside the Shield Parry NotifyState succeeds through the existing Parry path; the same hit outside the authored window does not gain Parry behavior.
4. **Guard Break and held-input recovery:** block until the existing Guard Break path occurs. Shield Guard ends with the normal cleanup, Guard Break behaves normally, and a still-held RMB can only resume through the existing input path using the current Shield profile once legal. Physical Released/Canceled remains a clean stop.
5. **Cancellation and teardown:** attack/Dodge/defense cancellation, Guard Break, death, and montage interruption leave no stale `State.Action.Guarding`, `State.Action.Parrying`, component-owned Shield spec, Trace task, or montage state. Existing generic cancellation behavior must still affect Shield GAs.
6. **Composite changes:** Sword + Shield -> TwoHanded clears Shield, drops it through the existing world transaction, and falls back to the legal resulting defense profile/default. TwoHanded -> Shield switches the MainHand to Unarmed fallback, equips Shield, drops TwoHanded, and uses the Shield profile. The replaced item stays visible on the ground under the established former-owner rule.
7. **Regression:** unarmed, Sword, current world pickup/drop-swap, malformed pickup rejection, socket-authored Sword sweep, enemy damage, death, no-hit, same-team rejection, invulnerability rejection, and current Guard/Parry input behavior remain intact.

Compile, Automation, Editor readback, PIE, input, and visual checks are separate evidence. A static review or successful test compile is not a substitute for the user-confirmed Scene01 behavior.

## Implementation, Review, And Closeout Record

### Implemented Contract

- Registered `Ability.Defense.Guard.Shield` and `Ability.Defense.Parry.Shield`.
- `UDefenseProfileDefinition::IsProfileValid()` now rejects identical Guard/Parry tags. `UWeaponEquipmentComponent::RunPreflight()` selects the prospective OffHand profile first, then MainHand, and fail-closes an explicit profile unless its provider's own `BaseGrantedActions` supplies exactly one distinct Guard class and one distinct Parry class. Each matching CDO must carry both the exact profile tag and the explicit generic parent tag.
- The existing input resolver and native Guard/Parry/Guard Break ability lifecycle were not changed. A valid Shield profile only changes the selected defense tag and grants its two abilities through the existing component-owned grant set; MainHand attacks, trace delivery, damage, and Shield quick slots stay out of scope.
- The display component now ignores collision channels explicitly. This keeps a displayed Shield presentation-only and prevents it from becoming a second collision participant.

### P3 Automation Repair

Codex's fresh review found two non-blocking automation gaps after the initial implementation: the matrix did not prove that the selected Shield Guard/Parry classes were current component-owned grants, and it did not prove that a Sword + Shield -> TwoHanded -> Shield sequence clears stale defense Specs before re-granting the Shield pair.

- Added the `WITH_DEV_AUTOMATION_TESTS`-only `VerifyGrantedAbilityBinding()` helper. It checks an expected class against the component's current granted handles and ASC Specs without changing runtime behavior.
- Extended `PolyQuest.Equipment.TransactionMatrix` to assert the valid Sword + Shield grants, exact Shield Guard/Parry resolution, clearing both Shield handles and ASC Specs after the TwoHanded transition, and re-granting them after the TwoHanded -> Shield normalization. The fixture-only `UCombatLoadoutDefinition::AddTestInputAbilityRoute()` supplies the MainHand primary route used by that composed test setup.

### Evidence And Review Boundary

- **User-confirmed Automation:** after the repair, `PolyQuest.Equipment.TransactionMatrix` reported `Success`. Its logged TwoHanded/OffHand rejection, coincident-socket rejection, apply/drop rollback, and active-combat swap refusal lines are intentional negative-path assertions.
- **User-observed presentation:** Shield Guard was exercised far enough to expose a side-on upper-body versus default locomotion twist while moving. That is accepted presentation debt, not evidence of a failed defense transaction; `TODO-07B` owns the focused weapon-aware locomotion/facing solution.
- **Gemini review:** the user reported Gemini's strict review passed before the final P3 coverage repair.
- **Codex fresh review:** direct source/diff review, CodeGraph call-path inspection, and the final automation delta found no P0/P1/P2 issue after the two P3 assertions were added. `code-review-graph` was built at `7093c302e47d460580ae22e4b5e6be90bfb6752e`, behind this stage baseline `62cab5d46e2d7dbef0278254a6015b91ff6d1932`; its change summary was treated only as stale-coverage guidance, with direct source/diff review authoritative.
- **Not claimed:** this closeout does not claim a separate final `PolyQuestEditor` compile, final Editor asset readback, or complete Scene01 PIE regression run after the P3 repair beyond the evidence stated above.

### Candidate Commit Boundary

After explicit user approval, stage only the approved native/config/documentation paths that actually changed:

```text
Config/Tags/PolyQuestGameplayTags.ini
Source/PolyQuest/Public/Combat/Equipment/DefenseProfileDefinition.h
Source/PolyQuest/Public/Combat/Equipment/WeaponEquipmentComponent.h
Source/PolyQuest/Private/Combat/Equipment/WeaponEquipmentComponent.cpp
Source/PolyQuest/Public/Combat/Input/CombatLoadoutDefinition.h
Source/PolyQuest/Private/Tests/WeaponEquipmentComponentAutomationTests.cpp
ARCHITECTURE.md
ROADMAP.md
plan.md
```

Explicitly exclude all `Content/**`, including Shield DataAssets, Shield GA Blueprints, Shield Montages, NotifyState placements, `BP_WorldWeaponPickup` fixtures, maps, AnimBPs, input assets, imported resources, `.uproject`, generated directories, `Config/Tests/`, `.zcode/`, and all unrelated user WIP. No new native Shield GA, `BP_Weapon`, player/enemy character subclass, inventory, Bow, or world-interaction framework is part of this commit.
