# TODO-02C3E: Hit Reaction Tier Classification And Small Reaction v1

## Plan State

- Status: completed and verified on 2026-08-22.
- Baseline: `aeacbe5` (`[Feature] 武器感知普通移动表现 (Weapon-Aware Locomotion Presentation)`).
- Objective: replace the one-off enemy interrupt marker with one fail-closed, Damage-GameplayEffect-authored reaction category and ship the first Player/Enemy Small reaction without changing combat interruption, movement, damage, Poise, or input behavior.

~~~text
Outer: ue-stage-workflow
Primary: ue5-cpp-gameplay
Support: ue5-blueprint-workflow, ue5-debug-validation
Route reason: this stage changes the shared GAS damage-reaction classification boundary, target-side Health callbacks, native GameplayAbility teardown, and user-owned Montage/AnimBP authoring.
~~~

~~~text
Plan explorers: 0
Implementation executors: 1 (Gemini, user-coordinated bounded executor)
Complex Executor: none
Main parallel work: accept the contract, inspect the implementation/self-review, perform fresh and adversarial review, close documents, stage, and commit.
Reason: the reaction categories, Gameplay Tags, target-side GAS lifecycle, and Player/Enemy integration are one coupled contract. No parallel writer may split it.
~~~

## Locked Product Contract

1. Every successfully applied ordinary damage GameplayEffect may carry exactly one `AssetTag`: `Data.Reaction.Small`, `Data.Reaction.Big`, or `Data.Reaction.Launch`. No reaction tag is a legal `None` result. More than one category is invalid and fail-closed: health damage remains applied, but no reaction event is sent.
2. Classification comes only from the applied `FGameplayEffectSpec` AssetTags. Do not infer it from damage magnitude, WeaponDefinition, AttackRange, Poise amount, Montage, projectile type, or visual effects.
3. C3E implements only `Small` presentation. It is non-interrupting, non-stacking, has no displacement, does not stop movement, cancel an Ability, alter Health/Poise/Stamina, mutate input state, or own `State.Action.HitReacting`.
4. The existing full-body `UEnemyHitReactionAbility` becomes the retained Enemy `Big` route. Keep its C++ class and `GA_EnemyHitReaction` asset name for serialized-asset compatibility; remap only its internal Ability/Event Tags to `Ability.Reaction.Enemy.Big` and `Event.Reaction.Enemy.Big`.
5. `Big` against the Player and all physical `Launch` behavior remain intentionally unimplemented in C3E. Those categories are legal authored data but are no-ops on an unsupported target until `TODO-02C3F` / `TODO-02C3G`.
6. `State.Status.Stunned` remains the existing Stance Break / Guard Break state. Do not rename it, fold it into reaction categories, or change its movement, cancellation, recovery, or priority semantics. A Stance Break or Guard Break cancels its target's active Small reaction; Small activation is blocked while Stunned. `State.Action.HitReacting` remains Enemy Big-only.
7. Hyper Armor continues to block Enemy Big only. It does not suppress Small, because Small has no combat interruption effect.

## Runtime And Public Contract

### Reaction classifier and Tags

- Add a narrow shared native classifier `FHitReactionClassifier` under `Combat/Reaction/`, with a non-reflected `EHitReactionTier` of `None`, `Small`, `Big`, `Launch`, and `Invalid`, plus a static pure classifier whose only input is `const FGameplayTagContainer&`. It is the only code allowed to interpret reaction AssetTags.
- `FHitReactionClassifier` is context-free: it must not receive or query an Actor, ASC, World, UObject, or DataAsset, and it must not log. It recognizes only exact reaction tags. The target-side Health dispatcher owns the single concise `Invalid` warning because it alone has useful target and applied-effect context.
- Register the following exact Gameplay Tags in `Config/Tags/PolyQuestGameplayTags.ini`:
  - `Data.Reaction.Small`, `Data.Reaction.Big`, `Data.Reaction.Launch`.
  - `Ability.Reaction.Player.Small`, `Event.Reaction.Player.Small`.
  - `Ability.Reaction.Enemy.Small`, `Event.Reaction.Enemy.Small`.
  - `Ability.Reaction.Enemy.Big`, `Event.Reaction.Enemy.Big`.
  - `State.Action.SmallHitReacting` as an internal GAS-owned reactivation guard only. It must not be a child of `State.Action.HitReacting`, otherwise existing Big blockers would incorrectly treat Small as interruption.
- Remove `Data.Reaction.Interrupt`, `Ability.Reaction.Enemy.Hit`, and `Event.Reaction.Enemy.Hit` only after all native and user-authored references have migrated and Reference Viewer/Tag search report zero remaining references. Do not add redirects or legacy aliases.

### Target-side dispatch and priority

- Keep `FMeleeHitResolver` and `FCombatProjectileHitResolver` unchanged except for any compile-only include adjustment proven necessary. Both remain sole normal damage-delivery paths and must not dispatch reaction events themselves.
- Extend `AEnemyCharacter::OnHealthAttributeChanged` and add the matching Player health delegate lifecycle in `APlayerCharacter`:
  - `APlayerCharacter` adds `PossessedBy(AController*)` and follows the existing guarded delegate pattern: bind after `Super::BeginPlay()` and after `Super::PossessedBy()`, unbind before `Super::EndPlay()`, retain the bound ASC weakly, and remove the exact Health delegate before any ASC replacement/rebind. Repeated BeginPlay/Possess paths must not double-bind.
  - For an authoritative, non-teardown callback, use one fixed priority order: lethal Health first (Enemy preserves its current Dead path; Player adds no death implementation and only sends no reaction), then non-damage Health changes, missing `GEModData`, existing Dead/Stunned state, Enemy Poise Broken state, classifier result, and target-specific event dispatch. `GEModData` must never prevent the existing Enemy lethal branch from running.
  - Preserve enemy Poise/stance-break precedence: a Poise-broken Enemy does not start Small; if Stance Break starts after an already active Small, `UEnemyStanceBreakAbility` cancels it before its own movement lock.
  - Build `FGameplayEventData` from the applied EffectSpec context: instigator, target, and positive Health loss magnitude. Do not store mutable hit state on the GE or DataAsset.
  - Dispatch Player/Enemy Small only for `Small`; dispatch Enemy Big only for `Big`; treat `None`, Player Big, and Launch as legal C3E no-ops.
  - Invalid multi-category data logs a concise warning naming the target and effect, then sends no event.

### Small reaction abilities

- Add `UPlayerSmallHitReactionAbility` and `UEnemySmallHitReactionAbility` as separate `InstancedPerActor`, `ServerOnly` native abilities. Do not create a generic Player/Enemy reaction base class in v1.
- Both abilities use their own Gameplay Event trigger, carry their own AbilityTag, own `State.Action.SmallHitReacting` while active, and block on that tag, `State.Status.Dead`, and `State.Status.Stunned`.
- Both validate ASC, living target, AnimInstance, valid tag set, and a configured Small Montage before `CommitAbility`/playback. Missing setup or failure to start the Montage fails closed with no gameplay cancellation or movement mutation.
- Both follow the existing `UEnemyHitReactionAbility` playback pattern exactly: `UAbilityTask_PlayMontageAndWait` starts the montage, while one manually bound `UAnimInstance::OnMontageEnded` callback identity-filters `ActiveMontage` and converges through guarded `EndAbility()` cleanup. It handles synchronous zero-length/startup failure, removes the delegate, and ends the task. Do not additionally bind `OnCompleted`, `OnBlendOut`, `OnInterrupted`, or `OnCancelled`; duplicate lifecycle entry points are forbidden.
- Neither ability adds movement/input/attack blocking tags, calls `CancelAbilities`, touches CharacterMovement, or consumes a cost. The reactivation tag prevents visual stacking only.
- Update `UEnemyStanceBreakAbility` to replace the retired `Ability.Reaction.Enemy.Hit` cancellation tag with `Ability.Reaction.Enemy.Big` and add `Ability.Reaction.Enemy.Small` before its own Stunned movement lock. Update `UPlayerGuardBreakAbility` to add `Ability.Reaction.Player.Small` before its existing Stunned path. No other Guard/Parry/Dodge behavior changes.

### Existing Enemy Big migration

- Keep `UEnemyHitReactionAbility` full-body and interruption-owned: it still stops movement, owns `State.Action.HitReacting`, cancels Enemy Melee only after its Montage actually starts, and restores movement only when neither Dead nor Stunned owns it.
- Replace all internal old Hit identifiers with Enemy Big identifiers, including `UEnemyStanceBreakAbility` cancellation tags and `AEnemyCharacter` event dispatch.
- `Data.Reaction.Interrupt` migrates as follows: `GE_Unarmed_ChargedAttack_Unarmed_Damage` and `GE_Sword_ChargedAttack_Damage` become `Data.Reaction.Big`. Do not classify other attacks as Big merely because they are visually strong.

## Authoring Gate

### GameplayEffect classification

- Add `Data.Reaction.Small` to `GE_Light_Unarmed_Damage`, `GE_Sword_LightAttack_Damage`, and `GE_EnemyMelee_Damage`.
- Migrate the two confirmed charged damage effects above from `Data.Reaction.Interrupt` to `Data.Reaction.Big`.
- Keep Bow, Sprint Attack, Whirlwind, Parry, Guard, Poise recovery, and every unlisted damage effect unclassified in C3E. They deal their current damage but create no reaction.

### Shared Small Montage and AnimBP

- First inspect `BP_Player` and `BP_Enemy_Goblin` mesh Skeletons in Unreal Editor. Player and Goblin may share one new `AM_SmallHitReaction` only if the Skeleton asset is exactly the same.
- If the Skeleton differs, author target-specific Small Montages instead. C++ must only receive the Montage through each GA Blueprint property and must never hardcode an asset path.
- The Small Montage must have Root Motion disabled and no root displacement. It plays on a dedicated `ReactionOverlayGroup.ReactionOverlay` Slot, not the default/full-body attack group.
- In both `ABP_Player_Dungeon` and `ABP_Enemy_Goblin`, insert the same `ReactionOverlay` Slot through the existing layered blend from `spine_01`. The base locomotion/attack pose remains the lower layer; no new attack, locomotion, or trace graph is introduced.
- Keep existing `AM_HitReaction` attached to Enemy Big; it is not the Small asset. Add Player Small and Enemy Small GA Blueprints to `BP_Player` and `BP_Enemy_Goblin` `StartupAbilities`; retain `GA_EnemyHitReaction` as Enemy Big.

## Approved Source Surface

- `Config/Tags/PolyQuestGameplayTags.ini`.
- New `Source/PolyQuest/Public|Private/Combat/Reaction/*` classifier pair.
- `Source/PolyQuest/Public|Private/AbilitySystem/Abilities/PlayerSmallHitReactionAbility.*`.
- `Source/PolyQuest/Public|Private/AbilitySystem/Abilities/EnemySmallHitReactionAbility.*`.
- Existing Enemy Big / Stance Break / Player Guard Break Ability files, `PlayerCharacter.*`, `EnemyCharacter.*`, and one new focused automation test file under `Source/PolyQuest/Private/Tests/`.
- User-owned `Content/**`: Small Montage(s), Player/Enemy Small GA Blueprints, the five specified GE tag edits (three Small classifications plus two Big migrations), two AnimBPs, and character Blueprint StartupAbilities. These are authoring/validation inputs, not an implicit commit closure.

## Explicit Non-Goals

- No Player Big reaction, planar knockback, hyper-armor priority expansion, launch/airborne/landing, ragdoll change, death implementation, Player terminal-ability cleanup, camera shake, VFX/SFX, damage formula, Poise formula, weapon-specific reaction tables, or generic reaction framework.
- No Resolver-side event dispatch, no action FSM, no direct attribute mutation, no timeline/timer-based reaction state, no ability inheritance tree, and no asset import/retargeting by the implementation executor.
- No change to the existing `State.Status.Stunned` / Stance Break / Guard Break mechanics beyond their narrow Small-cancellation integration.

## Automation And Validation

### Native automation

- Add `PolyQuest.Combat.HitReaction` with transient tags/effects/fixtures to prove:
  - exact classification for `None`, `Small`, `Big`, `Launch`, and invalid multi-category AssetTags;
  - ordinary Player and Enemy Health loss dispatches only their expected Small event;
  - unsupported Player Big and Launch are legal no-ops; invalid classification is no event;
  - Small ability CDO tags show no movement/input/attack block, no `State.Action.HitReacting`, and use `State.Action.SmallHitReacting` only for self-reentry;
  - Stunned/Dead block Small and Stance Break/Guard Break cancellation containers include the correct target Small tag;
  - charged Damage GE migration dispatches Enemy Big through the existing path.
- Use a transient tagged Damage GE and an event-observer fixture to exercise target-side Health dispatch, including lethal, existing Dead, Stunned, Enemy Poise-Broken, invalid multi-category, and unsupported-tier no-event branches.
- Run the new test plus `PolyQuest.Player.ActionWindows`, `PolyQuest.Melee.TraceSourceGeometry`, `PolyQuest.Projectile.Lifecycle`, and `PolyQuest.Enemy.AttackSetSelection`. Expected negative-fixture warnings remain warnings, not failures.

### User-owned Editor and PIE gate

1. Confirm shared-vs-separate Skeleton decision, Small Montage Root Motion disabled, `ReactionOverlayGroup.ReactionOverlay` slot, and `spine_01` layered application in both AnimBPs.
2. Confirm every listed GE contains exactly the planned reaction tag and `Data.Reaction.Interrupt` has zero references before deletion.
3. Confirm Player and Goblin StartupAbilities contain their Small GA; Enemy retains its Big GA with the existing `AM_HitReaction`.
4. Manually compile `PolyQuestEditor`.
5. In Scene01 PIE, verify Player Light -> Goblin and Goblin normal melee -> Player both produce visible Small reaction while movement and active attack continue; rapid repeated hits do not stack or restart it.
6. Verify Player Charged -> Goblin still runs full Enemy Big interruption; Guard, Parry, Dodge invulnerability, enemy Stance Break, Player Guard Break, lethal damage, and dead actors never produce an erroneous Small overlay.
7. Record Skeleton result, asset paths, automation result, compile result, and PIE observations before review/closeout.

## Execution And Review Gates

1. Gemini first performs a read-only plan review. It must stop and report rather than broaden the stage if the shared Skeleton, existing `spine_01` layering, or Slot Group cannot satisfy the authoring gate.
2. After plan acceptance, Gemini implements only the approved source/config/test surface, performs its strict self-review, and does not commit, stage files, modify `.uasset` manually, import assets, run UBT, or claim Editor/PIE evidence.
3. Main performs a fresh defect-first review after Gemini's self-review. `gpt-5.6-luna / xhigh` remains unavailable, so any required second pass is recorded as a Main adversarial fallback rather than an independent Fresh Reviewer.
4. Documentation closeout updates `ARCHITECTURE.md`, `ROADMAP.md`, `README.md`, and this plan only after user-confirmed compile/automation/PIE evidence. Commit excludes all unrelated user WIP, especially `Content/**`, non-tag `Config/**`, `.zcode/**`, `PolyQuest.uproject`, and `PlayerCharacter.h` changes outside this approved diff.

## Closeout Record (TODO-02C3E)

- **C++ Implementation**:
  - `Source/PolyQuest/Public/Combat/Reaction/HitReactionClassifier.h` and `Source/PolyQuest/Private/Combat/Reaction/HitReactionClassifier.cpp`: Pure, static classifier evaluating `Data.Reaction.Small`, `Data.Reaction.Big`, `Data.Reaction.Launch`, and `Invalid` (fail-closed).
  - `Source/PolyQuest/Public/AbilitySystem/Abilities/PlayerSmallHitReactionAbility.h|cpp`: Native Player Small reaction ability.
  - `Source/PolyQuest/Public/AbilitySystem/Abilities/EnemySmallHitReactionAbility.h|cpp`: Native Enemy Small reaction ability.
  - `Source/PolyQuest/Public/Character/Player/PlayerCharacter.h|cpp` & `EnemyCharacter.h|cpp`: Bound authoritative `OnHealthAttributeChanged` delegates to dispatch classified reaction events.
  - `Source/PolyQuest/Public/AbilitySystem/Abilities/EnemyHitReactionAbility.h|cpp`: Re-mapped tags to Enemy Big (`Ability.Reaction.Enemy.Big`, `Event.Reaction.Enemy.Big`).
  - `Source/PolyQuest/Public/AbilitySystem/Abilities/EnemyStanceBreakAbility.cpp` & `PlayerGuardBreakAbility.cpp`: Explicitly cancel active Small reactions before Stunned locks.
  - `Source/PolyQuest/Private/Tests/HitReactionAutomationTests.cpp`: Added 8 test sections verifying classification, CDO tag rules, Health dispatch, lethal guards, and negative branches.
- **Asset Authoring (User Confirmed)**:
  - `AM_SmallHitReaction`: Root motion disabled, slot configured to `ReactionOverlayGroup.ReactionOverlay`.
  - `GA_PlayerSmallHitReaction` and `GA_EnemySmallHitReaction` added to `BP_Player` and `BP_Enemy_Goblin` `StartupAbilities`.
  - GE Asset Tags authored with `Data.Reaction.Small` (`GE_Sword_LightAttack_Damage`, `GE_Light_Unarmed_Damage`, `GE_EnemyMelee_Damage`).
  - `ABP_Player_Dungeon` and `ABP_Enemy_Goblin` configured with two-stage decoupled `Layered blend per bone (spine_01)` overlay pipeline.
- **Validation**:
  - Automation Tests: 8/8 suites passed (`PolyQuest.Combat.HitReaction` and all regression tests 100% success).
  - Visual PIE: User verified Light Attack Small hit reaction overlay without interrupting movement or attack flow, and confirmed Charged Attack Big interruption.
- **Review Remediation (Codex Fresh Review)**:
  - P2 (1): `HitReactionAutomationTests.cpp` negative branch Health isolation: Fixed continuous damage accumulation draining Health to 0. Added per-test-branch Health restoration (`Health = 100.0f`), and added dedicated negative branch coverage for `None`, `Invalid`, `Stunned`, `Poise Broken`, `Already Dead`, and `Lethal` on both Player and Enemy.
  - P2 (2): `ARCHITECTURE.md` stale C3B tag references: Updated lines 215-238 to replace legacy `Data.Reaction.Interrupt` / `Event.Reaction.Enemy.Hit` / `Ability.Reaction.Enemy.Hit` with the C3E `Data.Reaction.Big` / `Event.Reaction.Enemy.Big` / `Ability.Reaction.Enemy.Big` contracts and `FHitReactionClassifier`.
- **Debt Handoff & Next Steps**:
  - `TODO-02C3F` (Big Hit Interruption and Knockback) owns Player Big hit reaction, planar knockback vector derivation, and AI recovery.
  - `TODO-02C3G` (Launch and Landing) owns physical airborne trajectory, landing presentation, and recovery.
  - Authored `.uasset` content remains local development WIP.
