# TODO-03A5: Ordinary Enemy Weapon Presets And Weighted Attack Sets v1

## Status

- **Plan state:** COMPLETED - Passed Verification, Independent Review, and Debt Handoff. Ready for explicit commit approval.
- **Baseline:** `5f7555d [Feature] 副手盾牌与组合防御装配 (Off-Hand Shield And Composite Defense Loadout)`.
- **Prerequisites:** `TODO-02A`, `TODO-02D3`, `TODO-03A1` through `TODO-03A4`, and `TODO-03A3B` are closed. The existing first enemy endpoint, StateTree controller lifecycle, native enemy melee Ability, shared Trace Window/resolver, Hyper Armor window, and fixed-component trace fallback are the required foundation.
- **Primary runtime question:** can an ordinary enemy use one immutable authored Attack Set to approach to an authored combat distance, choose one currently valid attack by pure weight, and execute it through the existing GAS Ability lifecycle without adding a second AI, damage, trace, weapon-equipment, or mutable DataAsset state path?

## Summary

`TODO-03A5` replaces the first enemy's one direct `UEnemyAttackProfile` reference with a finite `UEnemyAttackSet`. The set is static authored data. `AEnemyAIController` continues to own target acquisition, StateTree lifetime, the one approach/engagement range, and attack cooldown. `UEnemyMeleeAbility` chooses and snapshots one Profile only after activation has been admitted.

The first realized preset is `BP_Enemy_Goblin_Axe`, a configuration-only child of the current Goblin Blueprint (or assigned directly on `BP_Enemy_Goblin`). It retains the existing fixed `WeaponMesh`, `BladeTraceBase`, and `BladeTraceTip` display/trace fixture and uses an Axe Attack Set containing two distinct, Editor-verified Goblin attacks. No Sword preset is required in this slice; the same native contract will be reused only after compatible Sword assets are actually verified.

```text
Outer: ue-stage-workflow
Primary: ue5-cpp-gameplay
Support: ue5-state-tree-ai, ue5-blueprint-workflow, ue5-debug-validation
Route reason: Attack Set data, Controller approach/cooldown ownership, EnemyMeleeAbility selection/snapshot, and StateTree attack requests are one enemy-combat lifecycle contract.
```

```text
Plan explorers: 0
Implementation executors: 0
Complex Executor: none
Main parallel work: none
Reason: EnemyCharacter, Controller, Ability, and StateTree share one selection and teardown boundary. Splitting writers would create duplicate ownership of the selected Profile or cooldown.
```

## Locked Decisions

1. **Attack Set type:** add a narrow `UEnemyAttackSet` DataAsset. Do not reuse the player-only `UWeaponDefinition` hierarchy and do not add `UEnemyWeaponPresetDefinition`.
2. **Entry shape:** `FEnemyAttackSetEntry` contains only one `UEnemyAttackProfile` reference and one positive `SelectionWeight`. The Profile remains the complete static configuration owner for Montage, Damage GE, `AttackRange`, cooldown, and Guard Stamina Damage.
3. **Two-layer range contract:** `UEnemyAttackSet::EngagementRange` is the Controller/StateTree chase-stop and attack-request range. A Profile's existing `AttackRange` is its maximum eligible selection range. Both checks use the existing 2D actor-center distance convention.
4. **Axe behavior example:** when `EngagementRange` is close range, an Axe attack with `AttackRange = 250` does not repeatedly fire while the player holds 180 cm distance; the Controller chases to the Set's closer engagement distance first. Once inside, all Profiles whose `AttackRange` covers the current distance participate in weighted selection.
5. **Selection owner:** `UEnemyMeleeAbility` selects one Profile during `ActivateAbility()`, not in the Controller and not in StateTree. It snapshots that Profile's execution values for the active Ability lifetime.
6. **Random policy:** selection is pure weighted random. Immediate repeats are legal; this version deliberately has no previous-attack memory, combo queue, minimum-range bands, utility scoring, opener rule, or tactical repositioning layer.
7. **Deterministic test seam:** the Set's pure selection helper accepts a caller-supplied normalized selection fraction. Runtime passes a fresh random fraction; Automation passes fixed fractions. The DataAsset itself does not own an RNG or mutable selection history.
8. **Validity policy:** an Attack Set fails closed for an empty list, null or invalid Profile, duplicate Profile reference, non-positive/non-finite weight, non-positive `EngagementRange`, or an `EngagementRange` that no entry can reach. A valid general Set may contain one entry, but the Axe authoring gate requires two distinct valid entries to prove weighting.
9. **Migration policy:** replace `AEnemyCharacter::AttackProfile` with `AttackSet` in this slice. Do not leave an old-profile compatibility fallback or two runtime branches. The existing `DA_EnemyAttackProfile_GoblinAxe` asset remains and becomes an entry in the new Set.
10. **Preset boundary:** `BP_Enemy_Goblin_Axe` is an authored presentation/configuration preset only. It may select its Attack Set and fixed weapon display components; it must not acquire player pickups, use `UWeaponEquipmentComponent`, dynamically switch weapons, own inventory, or become a separate gameplay authority.
11. **Cooldown and teardown:** Controller cooldown begins exactly as it does now, only after the selected Montage actually started and the active Ability has ended. A failed selection or failed Montage start starts no cooldown. Existing Trace, Hyper Armor, C3B Hit Reaction, C3C Stance Break, death, and `EndAbility()` cleanup remain their current owners.
12. **No premature long-range AI:** a future enemy that truly needs a long-range opener, minimum-distance attacks, strafing, or tactical spacing receives a dedicated behavior stage. This slice makes farther engagement an explicit Set authoring decision rather than inferring it from the largest Profile range.

## Current Source Contract And Gap

- `UEnemyAttackProfile` is currently a valid-or-invalid immutable DataAsset containing one Montage, one damage effect, `AttackRange`, cooldown, and Guard Stamina Damage. It explicitly has no selector, queue, random table, or weapon-switching behavior.
- `AEnemyCharacter` currently exposes one `AttackProfile`. `AEnemyAIController::OnPossess()` validates it, caches its `AttackRange` into `MeleeRange`, and starts StateTree. `TryRequestMeleeAttack()` only activates `Ability.Attack.Enemy.Melee`; it does not play a Montage or mutate combat state.
- `UEnemyMeleeAbility` currently reads the direct Profile, snapshots Montage/effect/cooldown/Guard Stamina Damage, creates the existing Montage/Trace/Hyper Armor tasks, and starts the Controller cooldown only after a confirmed attack began and later ends.
- `FEnemyStateTreeTask_RequestMeleeAttack` only waits for target/range/cooldown and requests the GAS Ability. It never chooses an attack or owns a Montage.
- `UMeleeTraceSourceComponent` currently resolves the enemy's fixed component-name fallback (`WeaponMesh`, `BladeTraceBase`, `BladeTraceTip`) when no player equipment markers are present. It remains a legal enemy trace provider. Its removal is still exclusively `TODO-06C` and requires source plus Editor Reference Viewer evidence of zero users.

The missing contract is one immutable Attack Set with an explicit engagement range and a pure weighted selection helper. The implementation must not solve this by putting selected state in a DataAsset, by adding a second controller state machine, or by making a Player weapon definition pretend to be an Enemy weapon definition.

## Runtime Implementation

### 1. `UEnemyAttackSet`

Add `Source/PolyQuest/Public/Combat/Enemy/EnemyAttackSet.h` and `Source/PolyQuest/Private/Combat/Enemy/EnemyAttackSet.cpp`.

- Declare a BlueprintType `FEnemyAttackSetEntry` with `AttackProfile` and `SelectionWeight` only.
- Declare a BlueprintType `UEnemyAttackSet : UDataAsset` with authorable positive `EngagementRange` and `Entries`.
- Expose narrow native accessors for `EngagementRange`, validation with a human-readable failure reason, and pure profile selection for a supplied 2D distance plus normalized random fraction.
- Validation must inspect every entry rather than silently skip malformed data. Duplicate references are invalid; changing probability must be done through the one entry's weight.
- Selection must return no Profile when the Set is invalid, the target lies outside `EngagementRange`, no Profile reaches the current distance, or the supplied random fraction is invalid. It sums only currently eligible positive weights, then uses cumulative weighted selection.
- Do not add Gameplay Tags, Blueprint condition callbacks, random seeds, timers, Target references, ASC references, current profile fields, cooldown fields, or weapon display data to the Set.

### 2. Enemy Character And Controller Migration

In `AEnemyCharacter`, replace the direct `AttackProfile` property/accessor with `AttackSet` / `GetAttackSet()`. Do not modify the existing death, Poise recovery, team, ASC, startup-ability, fixed display, or trace-source lifecycle.

In `AEnemyAIController`:

- Rename the validity concept/API from Profile to Attack Set and update all native callers and logs accordingly.
- During `OnPossess()`, validate the possessed enemy's Set before StateTree starts. Cache only `AttackSet.EngagementRange` into the existing `MeleeRange` field, resetting it during unpossess/death as today.
- Add one narrow native helper that retrieves the current valid target's 2D distance. `IsCombatTargetInMeleeRange()` must use that helper and the cached engagement range so the Controller and Ability share one distance convention.
- Keep Controller ownership of target, focus, perception, StateTree start/stop, `MeleeAttackCooldownEndTime`, and cooldown timing. It must not choose, cache, or pass a selected Profile to the Ability.
- An invalid Set remains fail-visible: log once with the validation reason and do not start StateTree. A target outside `EngagementRange` remains a normal chase condition, not an error.

### 3. `UEnemyMeleeAbility` Selection And Snapshot

- Update `CanActivateAbility()` and setup validation to require a valid Attack Set, valid Controller target, active-range admission, AnimInstance, tags, and existing GAS state gates. They must not roll random numbers or select a Profile.
- At the start of `ActivateAbility()`, reset all current active fields, retrieve the current target distance through the Controller, and ask the Attack Set to select a Profile using a fresh runtime random fraction.
- Store the selected Profile in a transient GC-tracked `ActiveAttackProfile` field, then snapshot its Montage, Damage GE, cooldown, and Guard Stamina Damage before creating any AbilityTasks or committing the Ability.
- If selection fails because the target moved or no entry is eligible, end through the existing unified failure path before `CommitAbility()`. Do not create tasks, set attack state, apply cooldown, or leave a selected pointer behind.
- Retain current Montage identity filtering, Trace Window delivery, Hyper Armor Begin/End filtering, and confirmed-start behavior. `EndAbility()` must clear `ActiveAttackProfile` alongside the existing active snapshot fields after using the already-snapshotted cooldown value when `bAttackStarted` is true.
- StateTree task and condition topology remain unchanged. They continue to ask Controller questions and request the one enemy melee Ability; no Blueprint StateTree authoring is required for weighted selection.

### 4. Focused Native Automation

Add `Source/PolyQuest/Private/Tests/EnemyAttackSetAutomationTests.cpp` with `PolyQuest.Enemy.AttackSetSelection` under the existing Editor automation convention.

- Build test Profiles and Sets as transient native objects. Follow the existing automation style for private authored fields through reflection; do not add public production setters or require local `Content/**` fixtures.
- Assert rejection for empty Sets, null/invalid entries, duplicate Profiles, zero/negative weights, and an engagement range that no Profile can cover.
- Assert target distance above `EngagementRange` produces no selection, Profile ranges filter candidates, and fixed random fractions select the expected cumulative-weight entry at lower/middle/upper boundaries.
- Assert a single valid entry remains legal for the general data type. The two-entry requirement belongs only to this stage's authoring and PIE evidence.

## Editor Authoring Gate

After the user compiles `PolyQuestEditor` successfully:

1. Create `DA_EnemyAttackSet_GoblinAxe` under the existing enemy combat data location.
2. Add the existing `DA_EnemyAttackProfile_GoblinAxe` and one new, distinct `DA_EnemyAttackProfile_GoblinAxe_*` entry. Each must use a different attack Montage confirmed by Editor to be compatible with the Goblin Skeleton, have a valid damage GE, positive `AttackRange`, non-negative cooldown, and non-negative Guard Stamina Damage.
3. Configure both positive weights and an explicit `EngagementRange` no greater than at least one entry's `AttackRange`. For the intended close-combat Goblin behavior, set a close engagement range so the AI walks in before it can select either attack.
4. Create `BP_Enemy_Goblin_Axe` as a configuration-only child of the current `BP_Enemy_Goblin`. Assign its Attack Set and preserve the existing `WeaponMesh`, `BladeTraceBase`, `BladeTraceTip`, trace settings, `GA_EnemyMelee`, Hit Reaction, Stance Break, AnimBP, and StateTree configuration.
5. On each new attack Montage, author a valid existing `AttackTraceWindow`. Preserve or add the existing `Enemy Hyper Armor` NotifyState only where that individual attack genuinely needs it; it is not mandatory for every profile.
6. Replace the Scene01 Goblin test instance with `BP_Enemy_Goblin_Axe`, save affected assets, and read back that no Blueprint has a missing/unknown enemy attack property. The old single Profile asset is retained as a Set entry; do not delete imported assets or unused WIP.
7. Do not create `BP_Enemy_Goblin_Sword` in this stage. It becomes a later configuration-only preset after its display mesh, fixed markers, and at least two usable Sword attacks are actually verified.

All Editor assets remain local authoring WIP and are excluded from the focused native/documentation commit unless the user later gives explicit asset-commit approval.

## Validation And Review Record

### Main Static Gate

- Re-read `UEnemyAttackSet`, `AEnemyCharacter`, `AEnemyAIController`, `UEnemyMeleeAbility`, and `UEnemyAttackProfile`.
- Verified pure fail-closed validation on `IsAttackSetValid` (rejecting empty list, non-positive/non-finite engagement range, null/invalid profiles, non-positive/non-finite weights, duplicate profile references, and unreachable engagement ranges).
- Verified deterministic weighted selection on `SelectAttackProfile` (distance filtering, bounds checks, cumulative weights).
- Verified `AEnemyAIController` caches `EngagementRange` into `MeleeRange` and validates `AttackSet` on `OnPossess()`, with `TryGetCurrentTargetDistance2D` providing 2D actor distance.
- Verified `UEnemyMeleeAbility::ActivateAbility()` selects and snapshots Profile dynamically and cleans up in `EndAbility()`.
- Verified zero remaining `AttackProfile` property/accessor paths in `AEnemyCharacter` and `AEnemyAIController`.
- `git diff --check` passed cleanly with 0 whitespace errors.

### User Compile, Automation, And PIE Evidence

- **Compilation**: Compiled `PolyQuestEditor` with Live Coding (`Ctrl+Alt+F11`) with success.
- **Automation Test 1**: `PolyQuest.Enemy.AttackSetSelection` passed 100% with `Success` (covering validation edge cases and cumulative selection).
- **Automation Test 2**: `PolyQuest.Equipment.TransactionMatrix` passed 100% with `Success` (regression guard confirming player equipment transactions intact).
- **Asset Configuration**: Authored `DA_EnemyAttackProfile_GoblinAxe_02` (distinct montage with NotifyState `AttackTraceWindow`) and `DA_EnemyAttackSet_GoblinAxe` (`EngagementRange = 180.0cm`, 2 entries with weights `1.0`).
- **Scene01 PIE**: Confirmed Goblin chases to engagement range and randomly executes both axe attack montages at melee range.

### Independent Review Record

- **Reviewer**: Fresh Independent Reviewer (Codex / Fresh Review pass).
- **Mode**: Strict Defect-First Code Review (Pass 1 Normal Review + Pass 2 Adversarial Defense/Audit).
- **Outcome**: 2 P2s and 1 P3 identified and repaired; 0 P0/P1s.
  - **P2 (Fixed)**: Replaced stale single-Profile descriptions in `ARCHITECTURE.md` (lines 170-175, 198) and `EnemyAIController.h` (line 110) comment with explicit `UEnemyAttackSet` and `EngagementRange` contracts.
  - **P2 (Fixed)**: Added `FMath::IsFinite` checks across `UEnemyAttackProfile::IsValidAttackProfile`, `UEnemyAttackSet::IsAttackSetValid` (total weight sum overflow), `UEnemyAttackSet::SelectAttackProfile` (total eligible weight), and `AEnemyAIController::StartMeleeAttackCooldown`.
  - **P3 (Fixed)**: Added explicit automation test assertions in `EnemyAttackSetAutomationTests.cpp` covering `+INF`, `NaN`, and total weight overflow on Profile fields, EngagementRange, SelectionWeight, and target distance / random fraction.

### Debt Handoff

- Static enemy weapon geometry convergence (consuming `UMeleeWeaponDefinition` geometry directly for trace markers and display) is owned by `TODO-03A5B`.
- Fixed-component trace marker fallback removal remains owned by `TODO-06C`.
- All authored `Content/**` assets (`DA_EnemyAttackProfile_GoblinAxe_02`, `DA_EnemyAttackSet_GoblinAxe`, `BP_Enemy_Goblin`, maps, montages) remain local authoring WIP and are excluded from the native/documentation commit.

## Commit Boundary

Candidate native/documentation paths are limited to:

```text
Source/PolyQuest/Public/Combat/Enemy/EnemyAttackSet.h
Source/PolyQuest/Private/Combat/Enemy/EnemyAttackSet.cpp
Source/PolyQuest/Public/Combat/Enemy/EnemyAttackProfile.h      (only if comments/API need the clarified contract)
Source/PolyQuest/Private/Combat/Enemy/EnemyAttackProfile.cpp  (only if validation support needs it)
Source/PolyQuest/Public/Character/Enemy/EnemyCharacter.h
Source/PolyQuest/Private/Character/Enemy/EnemyCharacter.cpp
Source/PolyQuest/Public/AI/EnemyAIController.h
Source/PolyQuest/Private/AI/EnemyAIController.cpp
Source/PolyQuest/Public/AbilitySystem/Abilities/EnemyMeleeAbility.h
Source/PolyQuest/Private/AbilitySystem/Abilities/EnemyMeleeAbility.cpp
Source/PolyQuest/Private/Tests/EnemyAttackSetAutomationTests.cpp
ARCHITECTURE.md
ROADMAP.md
plan.md
```

Explicitly exclude all `Content/**`, `Config/**`, `.uproject`, maps, StateTree assets, AnimBPs, GA/GE assets, Montages, imported content, `.zcode/`, `Config/Tests/`, generated directories, and unrelated user WIP. Do not use `git add -A`.
