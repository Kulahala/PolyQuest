# plan.md - AI Agent Collaboration File

## Rules

1. Read this file before continuing a PolyQuest stage.
2. Keep feedback, active plan, validation evidence, and unresolved blockers scoped to the current stage.
3. The implementation author cannot mark a stage as review-approved.
4. Retain the completed plan and closeout record until the next accepted stage deliberately replaces it.
5. Durable architecture belongs in `ARCHITECTURE.md`; accepted future direction and validation debt belong in `ROADMAP.md`.

---

## Most Recent Stage: TODO-03A2 - Player Main-Hand And Unarmed Combat v1 (Completed; closeout recorded 2026-08-17)

Baseline: `7093c30 [Feature] 装备域与手持槽位合同 (Equipment Domain And Hand Slots)`.

Revision history: Revision 1 applied the four codex rulings (explicit owner-socket trace flag with bidirectional fail-closed validation, staged `GrantedWeaponAbilities` migration with Editor enumeration as the hard gate, the `CanSwapNow` granted-handle active check, a fully designated first `1-4` skill). Revision 2 settled the skill class (`UPlayerMeleeSkillAbility`, identity tags authored on the GA/GE assets, never hardcoded) and the montage-confirmed single-commit ordering. Revision 3 retires `UCombatActionDefinition` per the codex source/asset re-review: the type only wrapped `AbilityClass`, `ActionIntentTag` has no consumer, and each GA is already the sole behavior-and-configuration carrier of its combat action. Weapon definitions hold ability classes directly; the component works by ability class; no replacement action DataAsset type is introduced. Revision 4 settles four refinements: `ReusableCombatActions` and `ExclusiveCombatActions` are compat-retained candidate groupings merged into one runtime candidate union with no exclusivity rule in v1 (their source comments/Editor tooltips must not imply unimplemented behavior; real exclusive selection belongs to TODO-03D1), `DefaultPreparedActions` rejects duplicate non-null ability classes at validation (the layout fill's defensive skip is not the only defense), the staged migration reads legacy grants only until the atomic cutover with `BaseGrantedActions` never merged during Phases 1/2, and the ROADMAP decision sync splits into pre-implementation future-direction alignment versus post-validation done/debt closure with no early `ARCHITECTURE.md` or `README.md` edits. Revision 5 narrows the playable validation fixture set to Sword plus Unarmed: `DA_MeleeWeapon_Axe` and `DA_CombatLoadout_Axe` remain deferred WIP (no DefaultEquippedWeapon/debug-swap routing, no `BaseGrantedActions` requirement, outside the PIE matrix, re-authored under the direct GA-class contract by a future Axe authoring stage); the `BaseGrantedActions` enumeration/readback gate covers every currently routable/equippable validation fixture instead of every historical `UMeleeWeaponDefinition`; and before the legacy-field deletion the user records Axe's old `GrantedWeaponAbilities` list once, disposed of as old WIP per the accepted ruling.

Final native state (2026-08-17): `bUseOwnerMeshSocketForTrace`, the owner-socket marker branch, the `CanSwapNow` granted-handle scan, class-based `BaseGrantedActions`/candidate/default arrays, class-based prepared identities and exact-handle validation, `UPlayerMeleeSkillAbility`, and the three skill vocabulary tags are implemented. `GrantedWeaponAbilities` and `CombatActionDefinition.h` are removed; a Source/Content binary scan found no residual references.

### Objective

Make `DA_Weapon_Unarmed` a complete legal MainHand fallback that fights through the existing stable input/GAS lifecycle (LMB combo, Charged Attack, Sprint Attack, Guard, Parry, `1-4`), migrate the first Sword onto the class-based equipment grant source with `DA_MeleeWeapon_Axe` deferred as WIP, deliver the first real prepared-slot skill, and close the `GrantedWeaponAbilities` removal debt through its agreed hard gates. The TODO-03A/03A1 transaction, marker, trace, and resolver results are preserved; the single melee delivery path is unchanged. World pickup, Shield override, enemy presets, and Bow remain later stages.

### Locked Goals (user-locked)

1. `DA_Weapon_Unarmed` is a full-combat MainHand fallback with no dummy StaticMesh; its melee contact points come from the character SkeletalMesh's socket/bone-relative source.
2. Unarmed and equipped weapons share the existing stable input/GAS lifecycle: LMB combo, Charged, Sprint Attack, Guard, Parry, and `1-4` are never silently removed and never get a second state machine.
3. The first Sword migrates to its formal MainHand definition; `DA_MeleeWeapon_Axe` and `DA_CombatLoadout_Axe` stay deferred WIP - not routed as DefaultEquippedWeapon or debug-swap targets, not re-authored this stage - and a future Axe authoring stage re-authors them under the direct GA-class contract; Sword/Katana may later share GA/combo assets. No Katana asset work in this stage.
4. GAs keep owning Montage, GE, Cost, Damage, Timing, Tags, and Cleanup; nothing relocates that configuration.
5. The selected weapon/action configuration is fixed for the duration of an active action; equipment cannot rewrite it underneath.
6. The `GrantedWeaponAbilities` migration completes through the staged sequence below; the Editor enumeration and readback of every currently routable/equippable validation fixture is the hard removal gate, and the binary Content scan is auxiliary evidence only.
7. `UCombatActionDefinition` and every CA_* asset contract retire: `UWeaponDefinition`'s `BaseGrantedActions` / `ReusableCombatActions` / `ExclusiveCombatActions` / `DefaultPreparedActions` hold `TArray<TSubclassOf<UGameplayAbility>>` directly, the component's prepared identity / keep-if-compatible / preflight / apply / exact-handle triple validation all work by ability class, and no replacement Skill DataAsset type is introduced. `ReusableCombatActions` and `ExclusiveCombatActions` are compat-retained authoring groupings merged into one runtime candidate union; no exclusivity rule is implemented in v1, and real exclusive selection belongs to TODO-03D1.

### Material Design Decisions (Main rulings with rationale)

1. **Unarmed contact source: an explicit flag, not implicit null-mesh.** `UMeleeWeaponDefinition` gains `UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) bool bUseOwnerMeshSocketForTrace = false`. `DA_Weapon_Unarmed` sets it true with `WeaponMesh = null`, `HandSlot = MainHandOneHanded`, `AttachSocketName` on the character skeleton (for example `hand_r`), and the two blade-marker offsets authored in that socket's local space plus its own `TraceRadius`/`BladeSubdivisions`. `IsValidWeaponDefinition` is bidirectionally fail-closed so exactly two legal shapes exist:
   - `bUseOwnerMeshSocketForTrace == true && WeaponMesh` is rejected: an owner-socket trace source cannot also carry a display mesh (ambiguous contact source).
   - `bUseOwnerMeshSocketForTrace == false && !WeaponMesh` is rejected with today's "WeaponMesh is not assigned" reason (display weapons keep the unchanged contract).
   - Flag true keeps the existing distinct-marker, radius, and subdivision checks; the socket itself is verified at runtime by the existing `DoesSocketExist` preflight, which is character-skeleton-specific and cannot live on the DataAsset.
   - `ApplyComposition` branches on the flag: false keeps today's display-attached markers; true attaches the two component-owned marker components directly to `OwnerMesh` at `AttachSocketName` with the authored relative offsets (no display is spawned for a null mesh, as today). `TryGetBladeMarkers`, `GetEquippedMainHandMelee`, `GetTraceRadius`, and `GetBladeSubdivisions` work verbatim, so `UMeleeTraceSourceComponent`, `UAbilityTask_MeleeTraceWindow`, and `FMeleeHitResolver` are untouched and no second damage/trace/resolver path exists. One Verbose log marks the owner-socket attach for the PIE route.
2. **Action identity is the ability class; the CA wrapper layer retires.** `UWeaponDefinition`'s four action arrays become direct `TArray<TSubclassOf<UGameplayAbility>>`:
   - `BaseGrantedActions` is the always-granted-while-equipped chain (per weapon family: Light, Charged, Sprint Attack GAs). The 03A1-locked dual-grant model survives - base grants and prepared-slot grants apply together and any duplicate class between or within them fails preflight closed - and the base source migrates from the melee-only legacy field to the shared base-class field, clearing the removal debt instead of freezing it.
   - `ReusableCombatActions` / `ExclusiveCombatActions` are the `1-4` candidate pool; `DefaultPreparedActions` is the authored initial layout (at most four; null entries are legal no-op slots). The two candidate lists are compat-retained authoring groupings only: runtime merges them into one candidate union and implements no exclusivity rule in v1 - the field comments (Editor tooltips) state this explicitly so the names do not imply unimplemented behavior, and real exclusive selection rules belong to TODO-03D1.
   - Rationale for the retirement: `UCombatActionDefinition` had no independent data responsibility - it wrapped `AbilityClass` and an `ActionIntentTag` with zero consumers - while each GA already is the unique carrier of its action's behavior and configuration. The wrapper added an asset layer and authoring steps without adding truth. The deliberate tradeoff: a per-action metadata indirection (display names, future UI data) no longer exists; such a layer may return only with a real consumer requirement, not speculatively.
   - `IsValidWeaponDefinition` validates one class-level discipline: no null classes, no duplicates within or across the reusable/exclusive union, `BaseGrantedActions` disjoint from and deduplicated within itself, `DefaultPreparedActions` at most four with non-null entries inside the candidate union **and with duplicate non-null classes rejected at validation** - `ComputeKeepIfCompatibleLayout`'s skip of already-present defaults stays as defense in depth, never as the only line of defense. Type changes on the three 03A1-committed fields are data-safe: the committed Sword/Axe assets hold empty lists; any partially authored WIP CA references inside the lists are dropped on load and the Editor readback confirms the lists contain only newly authored GA classes.
   - Route boundaries are unchanged: the Base Input Profile (`AssociatedLoadout`) stays tag-only (`Input.PrimaryAttack` route + Sprint Attack tag); the current `GA_PrimaryAttack` spec is granted through the MainHand `BaseGrantedActions` source that those tags activate; `1-4` keep exact-handle activation from candidates/defaults; Guard/Parry stay startup GAs behind the Effective Defense Profile defaults - Unarmed therefore needs zero defense-specific work.
3. **Unarmed equip route.** Unarmed is reached only through `DefaultEquippedWeapon` authoring and the existing debug swap bindings calling `EquipWeapon`. No unequip API, no auto-fallback, no world pickup/drop/Former-owner gate (all TODO-03A3), no rest-site editing (TODO-03D1).
4. **Activation snapshot is enforced by composition plus an explicit granted-handle gate.** (a) Montage/GE/combo/damage configuration is GA-CDO- and per-spec-owned and immutable under equip; (b) `CanSwapNow` keeps the existing Attacking/Guarding/Parrying/Dodging tag gates and the active Primary arbitration spec check, and additionally scans every handle in the component's own `GrantedAbilitySpecHandles` for an active spec - so any active prepared-slot skill (which need not hold any of the four state tags) also blocks a swap; (c) the trace task is ability-scoped and re-queries markers per tick, markers only change inside gated transactions, and a hypothetical post-tag-removal race fails closed (missing markers -> no trace, one warning) - the strict review re-verifies this ordering; (d) `TryActivatePreparedSlot` triple-validates the binding on every call (valid handle, current grant of this component, and the spec's ability class matches the slot's class). This stage adds no snapshot copying; it documents the contract and closes the skill-activation swap hole with (b).

### First Real 1-4 Skill: Designated Contract (no placeholder)

`UPlayerMeleeSkillAbility : UStaminaActionAbility` (new narrow native class, `AbilitySystem/Abilities/PlayerMeleeSkillAbility.h/.cpp`), following the validated `USprintAttackAbility` single-shot skeleton minus its Sprint gate. First authored instance: `GA_Skill_Whirlwind` (Sword Whirlwind, 旋风斩) - a spin attack whose montage animates a wide blade sweep the existing marker trace resolves naturally. It is registered on the Sword by ability class directly: `ReusableCombatActions` += `GA_Skill_Whirlwind`, `DefaultPreparedActions` slot 1 = `GA_Skill_Whirlwind`.

- **Identity (authored on assets, not in the native class):** `GA_Skill_Whirlwind` carries `Ability.Skill.Whirlwind` in its Ability Tags; its Cooldown GE grants `Cooldown.Skill.Whirlwind` through its Granted Tags (the 02D2 pattern). Both tags are new registered vocabulary (`PolyQuestGameplayTags.ini`); `Ability.Attack.Skill.Whirlwind` is not added. The reusable native class hardcodes no Whirlwind or skill-identity tag - it references only the shared state/event/input tags - so future skill instances configure identity entirely through their GA and GE assets. No `Ability.Attack.*` tag means the Primary/Light/Charged/Sprint arbitration can never activate or suppress the skill through a tag route.
- **Activation:** only through `TryActivatePreparedSlot`'s exact spec handle (`1` with the skill prepared). `CanActivateAbility`: grounded, and none of Attacking/Dodging/Parrying/Charging/Dead/Exhausted/Stunned active (Guarding is not blocked: like every existing attack, the skill cancels Guard on confirmation and preserves the resume path); Stamina through the shared `CheckCost`. One press is one activation attempt; there is no hold/release arbitration.
- **Commit ordering (single commit, montage-confirmed):** exactly one `CommitAbility` (Stamina cost + Cooldown GE) per activation. The Montage task alone may start and confirm pre-commit: the commit executes only after the tracked `ActiveMontage` identity is verified with `Montage_IsActive` true - not at key press, and not merely after calling the task's activation. The six `WaitGameplayEvent` window tasks (trace/dodge-cancel/rate) call `ReadyForActivation` only after the commit succeeds. With no valid montage (null/failed to start, or the active-montage check fails) or a failed commit, the ability commits nothing - zero Stamina cost, zero Cooldown - leaves no trace window, no cancel tag, no rate window, and no Guard cancel, stops any started montage, and ends through the converged `EndAbility` with a fail-visible warning. Ending the never-armed window tasks is engine-safe (`UGameplayTask::EndTask` and `WaitGameplayEvent::OnDestroy` both guard un-activated state).
- **Side effects gated behind the commit:** `State.Action.Attacking` plus the input-block tags, the six window tasks' activation, any Guard cancel, and facing begin only after the successful commit; a failed commit applies none of them and must not have cancelled Guard or opened a trace. Tags are removed in the converged `EndAbility`; authored rate windows restore the baseline rate on every teardown path.
- **Damage contract:** `GE_Skill_Whirlwind_Damage` is a fixed-modifier Damage GameplayEffect (no SetByCaller; the trace window passes an empty SetByCaller tag exactly like Light/SprintAttack), delivered only through the same `UAbilityTask_MeleeTraceWindow` and `FMeleeHitResolver` with per-window target dedup. `CanActivateAbility` requires `DamageGameplayEffectClass`, so this class claims no null-damage-GE utility variant; a non-damage skill is future work that must change that requirement first.
- **Cancellation:** montage natural end ends normally; Dodge inside the cancel window, death, stun, and teardown converge through the same `EndAbility` cleanup (task end, tag removal, rate restore). Input release after the press does nothing.

### GrantedWeaponAbilities / UCombatActionDefinition Retirement (completed)

- The class-based cutover is atomic in the runtime transaction: preflight and apply read `BaseGrantedActions` from both hand definitions, merge the two candidate groups only for prepared slots, and grant every selected class through component-owned handles. There is no legacy/mixed grant source.
- The user completed the routable Sword/Unarmed readback, the former `GrantedWeaponAbilities` data was cleared/resaved, and the final static Source/Content binary scan found zero `GrantedWeaponAbilities` or `CombatActionDefinition` references. `DA_MeleeWeapon_Axe` remains deferred authoring WIP and starts directly from the new class contract when that weapon is taken up.
- `CombatActionDefinition.h` and the unreferenced CA_* WIP contract are retired. Each GA remains the sole source of its Montage, GameplayEffects, timing, cost, damage, tags, and cleanup configuration.

### Native Contract (draft)

- `WeaponDefinition.h`: the four action arrays as `TArray<TSubclassOf<UGameplayAbility>>` with one class-level validation discipline (non-null, no duplicates within or across the reusable/exclusive union, `BaseGrantedActions` disjoint and deduplicated, `DefaultPreparedActions` at most four with candidate-only entries and duplicate non-null classes rejected); the Reusable/Exclusive field comments (Editor tooltips) state the candidate-union grouping with no v1 exclusivity rule; the `CombatActionDefinition.h` include is removed. No other base-class change.
- `MeleeWeaponDefinition.h`: `bUseOwnerMeshSocketForTrace` with the bidirectional validation; gated deletion of `GrantedWeaponAbilities` per the staged retirement; comments updated to the class-based BaseGrantedActions source.
- `WeaponEquipmentComponent.h/.cpp`: `PreparedSlotClasses` (`Transient`, `TArray<TSubclassOf<UGameplayAbility>>`) index-aligned with `PreparedSlotHandles`; `ComputeKeepIfCompatibleLayout` keeps an old class only when it is still in the new composition's candidate class union and fills remaining slots from `DefaultPreparedActions` classes; preflight and apply collect/grant by class from both slots' `BaseGrantedActions` (base type, no melee cast needed from cutover onward) and each prepared class; `TryActivatePreparedSlot` triple-validates that the handle is valid, a current grant, and that the spec's ability matches the slot's class; the `CanSwapNow` granted-handle active scan and the owner-socket marker branch are retained.
- `AbilitySystem/Abilities/PlayerMeleeSkillAbility.h/.cpp`: the designated skill class above; one narrow reflected ability type with no generic skill framework, no data-driven dispatch, and no skill-identity tag hardcoded.
- `Config/Tags/PolyQuestGameplayTags.ini`: `Ability.Skill.Melee`, `Ability.Skill.Whirlwind`, and `Cooldown.Skill.Whirlwind`.
- `Combat/Equipment/CombatActionDefinition.h`: removed during the rework phase on explicit user authorization (CA_* assets already deleted unreferenced, zero Content references, zero Source references); no interim dead class remains.
- `PlayerCharacter`, `ABaseCharacter`, enemy classes, `UMeleeTraceSourceComponent`, `UAbilityTask_MeleeTraceWindow`, `FMeleeHitResolver`: no changes. Any diff there must be justified in review, not assumed.
- `DodgeAbility`, `PlayerGuardAbility`, `PlayerParryAbility`, `PlayerGuardBreakAbility`: updated cancel sets and validation to include `Ability.Skill.Melee`.

### User-Owned Editor Gate

1. Author `GA_Skill_Whirlwind` (BP on `UPlayerMeleeSkillAbility`, Ability Tags = `Ability.Skill.Whirlwind`) with its montage, `GE_Skill_Whirlwind_Damage` (fixed-modifier Damage GE, no SetByCaller), `GE_Skill_Whirlwind_Cost` (duplicated from the Light Attack cost GE and then tuned; the shared Light cost GE is never modified), and a Cooldown GE with Granted Tags `Cooldown.Skill.Whirlwind` (fixed duration, no SetByCaller). The Whirlwind montage's trace/dodge-cancel/rate window notifies must not begin at frame 0 - leave at least one normal animation frame after montage start, because the window tasks arm only after the commit and a frame-0 window would be silently missed.
2. Register the skill on the Sword by ability class: `ReusableCombatActions` += `GA_Skill_Whirlwind`; `DefaultPreparedActions` slot 1 = `GA_Skill_Whirlwind` (other slots may stay empty).
3. Author `DA_Weapon_Unarmed`: `bUseOwnerMeshSocketForTrace = true`, null mesh, hand socket, socket-local marker pair (v1 single-pair constraint: the pair follows the one anchored hand socket - see the open point on alternating-hand fidelity), radius/subdivisions, `BaseGrantedActions` = the punch Light/Charged/SprintAttack GA classes, `AssociatedLoadout = DA_CombatLoadout_StraightSword` (settled reuse).
4. Migrate `DA_MeleeWeapon_Sword` to class-based `BaseGrantedActions`; `DA_MeleeWeapon_Axe` and `DA_CombatLoadout_Axe` stay deferred WIP - not a DefaultEquippedWeapon or debug-swap target, no `BaseGrantedActions` authoring this stage; complete the stage-2 enumeration readback across the routable fixtures, including the dropped-CA-entries confirmation and a one-time note of Axe's legacy `GrantedWeaponAbilities` contents for the disposition record.
5. Final phase only: after the zero-referencer confirmation, delete the four CA_* WIP assets.
6. Readback after each compile round: the staged-retirement gates above plus `BP_Player.DefaultEquippedWeapon` still the Sword for the regression route; the Unarmed PIE route may temporarily point it at `DA_Weapon_Unarmed` or use the existing debug swap bindings (not committed).
7. No enemy, map, StateTree, AnimBP, or input-mapping edits.

### Validation Matrix

Main static gate: final source/caller reads, CodeGraph, Gameplay Tag cross-check (including the two new tags), class-level duplicate/disjoint validation review, `git diff --check`, and the auxiliary Content scans. Main does not run UBT, Editor writes, or PIE.

User compile and Scene01 PIE gates:

- Compile rounds per the staged retirement: rework, cutover, final deletion - each with its authoring/resave/readback steps.
- Sword regression, byte-equivalent to 03A1: display/markers, LMB combo, Charged, Sprint Attack, Guard/Parry through the default Effective Defense Profile, rate windows, Poise/hit-reaction enemy route, swap refusal, same-weapon no-op.
- Unarmed route: no weapon display spawns; punch LMB combo registers hits from the hand-contact sweep; Charged punch; Sprint Attack punch; Guard/Parry work; the Verbose owner-socket log appears; swapping Unarmed<->Sword through debug bindings rebuilds markers/display correctly and keep-if-compatible rebuilds the prepared layout by class.
- Skill route: `1` activates Whirlwind through the exact handle with the Stamina deduction and cooldown applying only after the montage is confirmed active; a second press inside the cooldown does not reactivate; an active skill blocks weapon swap (the handle gate); Dodge inside the cancel window cancels into Dodge with converged cleanup; the sweep damages the enemy through the shared trace; an empty slot stays a no-op. An unconfigured skill variant (missing Montage or Cooldown GE) is rejected cleanly by CanActivateAbility before activation with zero Stamina cost, zero Cooldown, no action tags, and no Guard disturbance; a configured but unstartable Montage variant reaches ActivateAbility's active-montage check, aborts with the Warning log, and leaves zero side effects.
- Failure-injection paths beyond static evidence remain out of scope here (see automation boundary).

### Automated Testing Boundary

This stage ships no automation; the equipment transaction matrix's automatable part becomes a required TODO-03A3 plan item, recorded in `ROADMAP.md` under 03A3: a native automation-test slice running against a real Player/skeletal-mesh fixture with the authored sockets (not a pure DataAsset mock), covering EquipWeapon preflight rejections (TwoHanded/OffHand both directions, duplicate class grants, StartupAbilities collision, foreign specs), class-based keep-if-compatible layout transitions, identity-based restore under an injected apply failure, and prepared-slot triple validation. Resolver-precedence coverage is not part of this requirement. Injection-based closure of the committed-cost debt stays governed by its existing ROADMAP entry and is not relaxed by this stage.

### Documentation And Commit Boundary (documentation complete; commit pending explicit approval)

Documentation sync runs in two deliberate steps:

1. **Pre-implementation future-direction alignment (after this plan's approval, before the rework lands):** `ROADMAP.md` only - revise every `UCombatActionDefinition` dependency in TODO-03A2/03A4/03A5/03D1 to the ability-class vocabulary (including the candidate-union note that real exclusive selection belongs to TODO-03D1), add the retirement note to the 03A1 Done record, and attach the 03A3 automation requirement (with the real Player/skeletal-socket fixture condition). `ARCHITECTURE.md` and `README.md` are not touched in this step.
2. **Post-validation closeout (completed 2026-08-17):** `ARCHITECTURE.md` now records the owner-socket contact-source contract, the class-based action/grant contract (retiring the action-reference type), the prepared-skill lifecycle, and the activation-snapshot/gate guarantee; `ROADMAP.md` marks 03A2 done and carries the remaining Unarmed fidelity release gate; `README.md` advances its status line; this file contains the closeout. No commit is created until explicit approval.

Native candidate paths: `Combat/Equipment/WeaponDefinition.h`, `Combat/Equipment/MeleeWeaponDefinition.h`, `Combat/Equipment/WeaponEquipmentComponent.h`, `Combat/Equipment/WeaponEquipmentComponent.cpp`, `AbilitySystem/Abilities/PlayerMeleeSkillAbility.h/.cpp`, `Config/Tags/PolyQuestGameplayTags.ini`, deletion of `Combat/Equipment/CombatActionDefinition.h` in the final phase, and exact `README.md`/`ARCHITECTURE.md`/`ROADMAP.md`/`plan.md` hunks. Exclude every other `Content/**` item (GA/GE/Montage/DA authoring stays local WIP unless the user explicitly approves a stable closure), generated directories, and unrelated WIP. No commit without explicit approval.

### Route And Ownership

```text
Outer: ue-stage-workflow
Primary: ue5-cpp-gameplay
Support: ue5-blueprint-workflow
Route reason: The stage reworks the equipment action identity to ability classes, adds the owner-socket trace source, and delivers one narrow GAS skill class inside existing contracts - Main-only integration territory; the user owns the GA/GE/DA authoring gates.
```

```text
Plan explorers: 0 (direct reads: equipment domain, PlayerCharacter input chain, trace source/task, Primary/Light/Charged/Sprint/Guard/Parry ability contracts, loadout profile, tag taxonomy, engine task/anim-instance signatures, Content referencer scans)
Implementation executors: 0
Complex Executor: none
Main parallel work: none
Reason: One grant-source migration and one marker-source branch sit inside a single transaction owner; any second writer races grant bookkeeping and marker state.
```

Main owns native source, static checks, review, documentation, staging, and commit boundaries. The user owns GA/GE/DA authoring, Editor readback and enumeration, the three manual `PolyQuestEditor` compiles, Scene01 PIE validation, CA_* asset deletion, and commit approval.

### Non-Goals

No world pickup/drop-swap/Former-owner gate (03A3); no Shield Defense Profile override or shield skills (03A4); no enemy Attack Sets/presets (03A5); no Bow, inventory, persistence, or rest-site work; no replacement action DataAsset or per-action metadata layer without a real consumer; no generic weapon framework, no generic skill framework or data-driven skill dispatch, and no second damage/trace/resolver path; no `BP_Player_Sword`/`BP_Player_Unarmed` gameplay Blueprint classes; no Katana migration; no non-damage skill variant in this stage; no moving Montage/GE/damage/timing configuration out of GAs; no unequip or bare-handed fallback state beyond the Unarmed definition itself; no CoreRedirects or UPROPERTY renames; no mid-combat slot rearrangement.

### Active Review And Repair Record (TODO-03A2 Review Rework)

- **Ability.Skill.Melee 类别标签职责**：作为“可由近战动作取消门禁统一取消”的通用类别标签（添加至 `PolyQuestGameplayTags.ini` 并在 `UPlayerMeleeSkillAbility` 构造函数注册到 `AbilityTags`）。具体技能身份标签（如 `Ability.Skill.Whirlwind`）由 GA 资产作者化，不与类别标签混淆。
- **P1 修复理由与实现**：
  - `DodgeAbility.cpp`：仅在 `bCanCancelAttack` 分支中将 `Ability.Skill.Melee` 纳入 `AbilityTagsToCancel`；保留 `bWasCharging` 的蓄力取消语义，防止 Dodge 在非取消窗口提前取消技能。
  - `PlayerGuardAbility.cpp` / `PlayerParryAbility.cpp`：在既有 `DefenseCancelableStateTag` 取消门禁内统一取消 `CancelableMeleeAbilityTags`（重命名自 `AttackAbilityTags`，包含 4 个 `Ability.Attack.*` 及 `Ability.Skill.Melee`），并同步将 `ValidateActivationSetup` 校验数量更新为 5。
  - `PlayerGuardBreakAbility.cpp`：`AbilitiesToCancel` 纳入 `Ability.Skill.Melee`，并将 `ValidateActivationSetup` 校验数量更新为 7；仅在破防 Montage 确认 active 后执行取消。
  - 单一收敛出口：所有取消均汇聚于 `PlayerMeleeSkillAbility::EndAbility` 统一收敛（关闭 Trace Window、移除 CanCancel tags、恢复 PlayRate、清理 Attacking 及 Input Block tags），不建立第二套技能状态或取消框架。
- **P2 修复理由与实现**：
  - `UPlayerMeleeSkillAbility` 保留 `MeleeSkillAbilityTag` 成员，并在 `PostLoad()` 与 Editor `PostCDOCompiled()` 将有效的 `Ability.Skill.Melee` 重新加入 Ability CDO 的 `AbilityTags`。`CanActivateAbility` / `ActivateAbility` 只验证字典 Tag 本身有效，不再以 `AbilityTags.HasTagExact(...)` 作为会被派生蓝图默认值覆盖的自我激活门禁；具体 `Ability.Skill.Whirlwind` 身份 Tag 仍由 GA 资产作者化。
  - `UPlayerMeleeSkillAbility` 的 `CanActivateAbility` 与 `ActivateAbility` 显式校验 `CooldownGameplayEffectClass` 非空；失败日志明确要求 Cost/Cooldown/Damage/Regen 均为必填配置。
  - `DodgeAbility.cpp` 延迟 `Ability.Skill.Melee` 取消时序：在 `MontageTask->ReadyForActivation()` 并确认 `Montage_IsActive` 后才执行技能取消，避免 Dodge 起播失败时误杀已在活动的技能；保留既有 Primary/Light/Charged/Sprint 历史取消时序。
- **P3 计划与验证断言修正**：
  - 明确缺失 `SkillMontage` 或 `CooldownGameplayEffectClass` 是由 `CanActivateAbility` 拒绝（未激活且零副作用），而非 `ActivateAbility` 的 Warning 日志；只有非空但起播失败的 Montage 才会进入 `ActivateAbility` 的 Warning 路径。
  - 破防断言规范为“受控 Gameplay Event / 静态取消合同验证”，即通过向角色发送 `Event.Reaction.Player.GuardBreak` 验证破防 Montage 启动后正确取消运行中技能。
- **验证证据与范围：** 用户完成了本地 Editor 编译/重载及 `GA_Skill_Whirlwind`、其 Cost/Cooldown/Damage 配置、`DA_Weapon_Unarmed` 与 Sword 定义的资产读回。最终 Scene01 PIE 确认覆盖 Sword/Unarmed 夹具路线及 `1` 槽旋风斩的正常播放。静态复核覆盖 Dodge/Guard/Parry/GuardBreak 的取消接线与 Dodge 失败起播时序；本次收尾不宣称已完成故障注入套件、打包、手柄验证或完整交替手出拳覆盖。

### Closeout And Debt Handoff

- **User validation:** the user confirmed the final local Editor readback for `GA_Skill_Whirlwind` (native `Ability.Skill.Melee` plus authored `Ability.Skill.Whirlwind`, Cost, and Cooldown) and the Scene01 PIE route, including normal Whirlwind playback from `1`. Earlier stage gates cover the Sword and Unarmed base paths; this closeout does not claim a new automation suite, packaged build, controller route, or clean-checkout authored fixture.
- **Static/review evidence:** Main re-read the source/input/equipment/trace lifecycle with CodeGraph, checked tag registration and `git diff --check`, and found no remaining P0-P2 blocker. The code-review graph remained supplemental because its built baseline was `7093c30` and the new skill files were untracked. Gemini supplied a second independent delta review after the slot-activation repair; its conclusion was accepted after correcting the documentation wording about CDO tag ownership.
- **Debt handoff:** the v1 single owner-socket marker pair does not prove alternating-hand punch coverage. `ROADMAP.md` now carries the concrete release gate: before an opposite-hand Unarmed attack ships, constrain the animation to the anchored contact hand and validate it, or approve a dedicated per-hand marker/active-hand selection slice. No second trace/resolver path is accepted as an incidental repair.
- **Commit boundary:** documentation is synchronized, but no files are staged or committed by this closeout. A later explicit approval must stage only the approved native/config/documentation paths and exclude all `Content/**`, generated output, and unrelated user WIP.

---

## Previous Stage Record: TODO-03A1 (durable record lives in ROADMAP Done Milestones)

- 03A1 delivered the equipment-domain contract (UWeaponDefinition base, UCombatActionDefinition, UDefenseProfileDefinition, hand-slot rules, keep-if-compatible prepared layout, exact-handle activation, single input resolver, Defense Profile chain) with five review repairs, user-confirmed compile/readback/PIE including the post-repair gate, and the honest multi-slot-evidence limits. `GrantedWeaponAbilities` removal was handed to 03A2. Committed as `7093c30`. (Revision 3 note: the UCombatActionDefinition action-reference type is being retired by TODO-03A2's class-based contract.)

## Previous Stage Record: TODO-03A (durable record lives in ROADMAP Done Milestones)

- 03A delivered the player-only single-slot equipment foundation (definition DataAsset, validated rollback transaction, runtime markers, weapon sweep shape, player-first trace resolution with the enemy fixed fallback), with the Primary-arbitration and foreign-spec review repairs, user-confirmed compile/PIE, and the deliberate-v1-limit debt handoff to the now-accepted TODO-03A1-03A5 direction. Committed as `601ad38`.
