# plan.md - AI Agent Collaboration File

## Rules

1. Read this file before continuing a PolyQuest stage.
2. Keep feedback, active plan, validation evidence, and unresolved blockers scoped to the current stage.
3. The implementation author cannot mark a stage as review-approved.
4. Retain the completed plan and closeout record until the next accepted stage deliberately replaces it.
5. Durable architecture belongs in `ARCHITECTURE.md`; accepted future direction and validation debt belong in `ROADMAP.md`.

---

## Active Stage: TODO-03A - Equipment And Combat Loadout Foundation v1

Baseline: `5852c8d [Feature] Montage 速率窗口与动作时序 (Montage Rate Window And Action Timing)`.

### Objective

Establish the first product equipment boundary after the fixed straight-sword trace has proven itself: an authored melee-weapon definition, one runtime equipment component owning the equipped state and its spawned components, an equip/swap transaction that covers display attachment, trace markers, Ability grants/revocations, and Loadout selection, and the replacement of the player-side fixed-name trace lookup and C3D collision guard with equipment-owned truth. `UAbilityTask_MeleeTraceWindow` and `FMeleeHitResolver` are reused unchanged. No world pickup, checkpoint, SaveGame, inventory grid, or generic backpack; a loadout change affects future input routing and grants only and never becomes a second combat state machine or a GAS replacement.

Locked values (user-locked contract):

- New `UMeleeWeaponDefinition` (DataAsset, `Combat/Equipment/`): `WeaponMesh` (StaticMesh), `AttachSocketName` (default `Weapon_R`), `DisplayLocationOffset`/`DisplayRotationOffset`, `BladeBaseMarkerRelativeLocation`/`BladeTipMarkerRelativeLocation` (authored in weapon-mesh local space; markers are runtime-spawned non-colliding SceneComponents), `TraceRadius` and `BladeSubdivisions` (the weapon's sweep shape travels with the weapon), `GrantedWeaponAbilities` (ability classes granted while equipped), and one optional `AssociatedLoadout` (`UCombatLoadoutDefinition`) activated on equip. `TraceChannel` stays on `UMeleeTraceSourceComponent` as the project-level collision policy that no single weapon may override.
- `UMeleeTraceSourceComponent` keeps its current fixed-fixture parameters as the default for actors without an equipment component (the Goblin path unchanged). Its trace-shape getters resolve by ownership: when the owner has a `UWeaponEquipmentComponent` with a weapon equipped, `GetTraceRadius()`/`GetBladeSubdivisions()` read the equipped definition; otherwise they return the component's authored fixture values. The component discovers the equipment owner through `FindComponentByClass<UWeaponEquipmentComponent>()` - `UMeleeTraceSourceComponent` never hard-depends on `APlayerCharacter`, and `TryGetBladeEndpoints` uses the same optional-component query for marker resolution with the fixed-name lookup as the fallback.
- New `UWeaponEquipmentComponent` (ActorComponent created by `APlayerCharacter` only in v1; never hoisted into `ABaseCharacter` for speculative reuse - when an enemy truly needs dynamic equipment, add the same component to it explicitly then): owns the currently equipped definition, the spawned display mesh, the two marker components, and the granted-ability spec handles. It exposes exactly one public operation, `EquipWeapon(Definition)` (`BlueprintCallable`), for future pickup/UI owners and PIE transaction testing. There is no public unequip: teardown is a private helper used by a validated swap and by `EndPlay`.
- v1 has no sustainable bare-handed state. `DefaultEquippedWeapon` is a required configuration (a missing default weapon is a fail-visible configuration error, not gameplay); at runtime the player always holds one valid weapon, and a swap is always the synchronous transaction "validated new weapon replaces the old weapon". A legal no-weapon state is deferred until a fists loadout or pickup/inventory rules define it; "attacking prints the trace configuration warning" is not an acceptable gameplay state.
- Transaction contract: `bool EquipWeapon(UMeleeWeaponDefinition* Definition)` returns success. The full preflight covers the DataAsset self-check plus the runtime prerequisites - the owner mesh exists and actually owns `AttachSocketName` (`DoesSocketExist`), the ASC is available with ability-granting authority, and every candidate ability class passes the duplicate gate: it is not a `StartupAbilities` class (queried through a new C++-only, non-Blueprint narrow helper `APlayerCharacter::IsStartupAbilityClass(TSubclassOf<UGameplayAbility>) const` that does not modify `ABaseCharacter`) and no already-existing candidate spec lives on the ASC outside this component's `GrantedAbilitySpecHandles` (specs the current weapon itself granted and is about to revoke in the swap pass this gate). No mutation happens before the preflight passes. Immediately after preflight and before teardown, the transaction snapshots the old definition and the current `ActiveCombatLoadout` (via a narrow C++ getter); the post-preflight steps - display component creation, marker creation, `GiveAbility`, and `SetActiveCombatLoadout` - must fully succeed, and any failure routes through a private **gate-free** restore helper that rebuilds the old weapon, its ability handles, and the old loadout before returning `false`. Only a failed restore logs an explicit fatal configuration error; the player is never knowingly left without a weapon.
- Loadout semantics: `AssociatedLoadout == nullptr` keeps the current active loadout untouched; only a non-null loadout that passed `IsRouteTableValid()` (checked in the definition validation) activates `SetActiveCombatLoadout` on successful equip and participates in the snapshot/restore contract.
- Combat-state arbitration: the equipment transaction refuses to run while the ASC owns any of `State.Action.Attacking`, `State.Action.Guarding`, `State.Action.Parrying`, or `State.Action.Dodging` (one focused warning, no partial state). `State.Action.Charging` is deliberately not added: the Charged ability owns `State.Action.Attacking` from activation, so it is already covered. The swap never cancels active abilities and never touches Stamina/Health/Guard state; it only governs future grants, routing, display, and trace samples.
- Collision policy single source of truth: the equipment component spawns display meshes with `NoCollision` + no overlap events by construction, so the player no longer relies on `ABaseCharacter::DisableFixedWeaponDisplayCollision`. That fixed-name guard remains for the enemy-side authored fixture and stays untouched this stage.
- Trace provider: `UMeleeTraceSourceComponent::TryGetBladeEndpoints` first asks the owner's `UWeaponEquipmentComponent` for its live markers; with no component or nothing equipped it falls back to the existing fixed-name resolution, which remains the enemy path. The ability-facing `GetMeleeTraceSource()` contract, `UAbilityTask_MeleeTraceWindow`, and `FMeleeHitResolver` are unchanged.
- Ability grants are tracked by source: only handles recorded by the equipment component are revoked on swap or teardown; `StartupAbilities` (Dodge, Guard, Parry, movement) stay authored on the character. Weapon-specific ability classes migrate to the weapon definition's `GrantedWeaponAbilities` as an Editor-side authoring move, with no native `StartupAbilities` change.
- Tags: none new. No new trace/damage path, no StateTree or enemy change, no asset-path hardcoding (the default socket name is a DataAsset default, not a lookup key).

### Native Contract (revision 3)

- `UMeleeWeaponDefinition` performs `IsValidDefinition(FString& OutReason)` validation (non-null mesh, non-none socket, distinct non-coincident marker locations, positive `TraceRadius`, `BladeSubdivisions` within the existing 1-8 clamp, non-null ability entries, no duplicate ability class within the weapon list, and a null-or-route-valid loadout) consumed by the equipment transaction; it stores authored data only and no runtime state.
- `UWeaponEquipmentComponent` holds `CurrentWeapon`, `EquippedDisplayComponent`, `EquippedBladeBaseMarker`, `EquippedBladeTipMarker`, `GrantedAbilitySpecHandles`, and a one-shot swap-refusal warning flag. Its single public operation `bool EquipWeapon(Definition)` runs the combat-state gate, the full preflight (including `IsStartupAbilityClass` and the foreign-spec duplicate gate), the pre-teardown snapshot, the old-weapon teardown, and the new-weapon application with the gate-free snapshot restore on any post-preflight failure; re-equipping the same definition is a no-op success returning `true`. `EndPlay`/component destruction runs the same private teardown so PIE stop and actor destruction leak nothing. No `UnequipWeapon` exists as a public or BlueprintCallable operation.
- `APlayerCharacter` creates the component in its constructor, exposes the required `DefaultEquippedWeapon`, adds the C++-only `IsStartupAbilityClass(...)` helper and a narrow C++ `GetActiveCombatLoadout()` getter, and equips the default weapon once during `BeginPlay` (after `SetActiveCombatLoadout(InitialCombatLoadout)`). A missing or invalid default weapon is logged as a fail-visible configuration error and leaves the player without melee capability, never a fallback weapon guess. Combat input routing, defense dispatch, and all existing player contracts are untouched.
- `ABaseCharacter` is not modified: the enemy path, `DisableFixedWeaponDisplayCollision`, and `GetMeleeTraceSource()` stay as they are.

### User-Owned Editor Gate

1. Create `DA_MeleeWeapon_Sword` from `UMeleeWeaponDefinition`: assign the current sword mesh, `Weapon_R`, blade-base/tip relative locations matching today's authored marker positions, the current trace radius/subdivisions as the weapon's sweep shape, the weapon-specific attack ability classes moved out of `BP_Player.StartupAbilities`, and the existing straight-sword loadout as `AssociatedLoadout`.
2. Set `BP_Player.DefaultEquippedWeapon` to the new DataAsset and remove the authored `WeaponMesh`/`BladeTraceBase`/`BladeTraceTip` components from `BP_Player` (their role is now runtime-spawned by the equipment component).
3. Keep `BP_Enemy_Goblin`'s authored weapon fixture untouched; its trace and ragdoll behavior must be unchanged.
4. PIE transaction testing uses only the existing `BlueprintCallable` `EquipWeapon` bound through a user-local temporary Blueprint key/widget plus one temporary second valid weapon definition that must have a different `AssociatedLoadout` and at least one different trace-shape value (`TraceRadius` or `BladeSubdivisions`); a different mesh is optional presentation. The A -> B -> A route exercises ability-handle recycling, loadout switching, display/marker recreation, the weapon-specific sweep shape, same-weapon re-equip no-op, and in-combat swap refusal. No native console command or Exec entry is added; the temporary debug assets and bindings are not committed. The invalid-ASC prerequisite is a Main static fail-closed check in source, not a manually reproducible PIE item.

### Validation Matrix

Main static gate: final source and direct caller/callee reads (`MeleeTraceSourceComponent`, `WeaponEquipmentComponent`, `PlayerCharacter`, one attack ability caller), CodeGraph, C4458 scan, `git diff --check`, and confirmation that `UAbilityTask_MeleeTraceWindow` and `FMeleeHitResolver` are byte-identical. Main does not run UBT, Editor writes, or PIE.

User compile and Scene01 PIE gate:

- Compile `PolyQuestEditor`; confirm the default weapon spawns at the same visual location and marker positions as the retired authored components, and Light/Charged/Sprint Attack hits, Poise, hit reaction, Stance Break, and death behave exactly as before.
- Verify the A -> B -> A swap transaction through the temporary debug entry: weapon-specific abilities are granted/revoked with no residual spec (startup abilities like Dodge/Guard/Parry are never revoked), the loadout switches with each weapon, display and markers are recreated per weapon, the equipped weapon's sweep shape is used, and re-equipping the same weapon is a no-op.
- Verify swap refusal while attacking/guarding/parrying/dodging (one warning, no partial state) and that a wrong socket or duplicate ability configuration fails closed before any teardown of the old weapon; the invalid-ASC branch is verified as a static source-level fail-closed check, not as a PIE reproduction.
- Verify the enemy fixture regression (trace, ragdoll, weapon display) and the full combat smoke (D1/D2 defense, C3B/C3C, rate windows).

### Documentation And Commit Boundary

After user compile/PIE and review: `ARCHITECTURE.md` records the equipment DataAsset/component/transaction contract and retires the player-side fixed-fixture wording (the enemy fixture wording stays), `README.md` status advances to the equipment foundation, `ROADMAP.md` marks only `TODO-03A` done; this file receives the closeout.

Native candidate paths: new `Combat/Equipment/MeleeWeaponDefinition.h`, new `Combat/Equipment/WeaponEquipmentComponent.h`, new `Combat/Equipment/WeaponEquipmentComponent.cpp`, `Combat/Melee/MeleeTraceSourceComponent.h`, `Combat/Melee/MeleeTraceSourceComponent.cpp`, `Character/Player/PlayerCharacter.h`, `Character/Player/PlayerCharacter.cpp`, and exact `README.md`/`ARCHITECTURE.md`/`ROADMAP.md`/`plan.md` hunks. Exclude every `Content/**` item, DataAsset, Blueprint, debug widget, map, generated directory, and unrelated WIP. No commit occurs without explicit user approval.

### Route And Ownership

```text
Outer: ue-stage-workflow
Primary: ue5-cpp-gameplay
Support: ue5-blueprint-workflow, unreal-enhanced-input
Route reason: The stage spans a DataAsset contract, a component lifecycle owning spawned subobjects, ASC grant/revocation bookkeeping, loadout switching, and the shared trace provider - all Main-only integration territory.
```

```text
Plan explorers: 0 (direct reads of the involved files sufficed)
Implementation executors: 0
Complex Executor: none
Main parallel work: none
Reason: One transaction owner and one trace provider hold the whole lifecycle; concurrent writers would race grant bookkeeping and marker state.
```

Main owns native source, static checks, review, documentation, staging, and commit boundaries. The user owns DataAsset/Blueprint authoring, Editor readback, manual `PolyQuestEditor` compilation, Scene01 PIE validation, and commit approval.

### Non-Goals

No world pickup, checkpoint, SaveGame, inventory grid, or backpack; no enemy equipment change; no second melee delivery, trace, or damage path; no Loadout-runtime-state machine; no SkeletalMesh-weapon or multi-weapon-socket support beyond the authored single melee slot; no public unequip or sustainable bare-handed state; no native console/Exec debug entries.

### Closeout Record (2026-08-16)

- Implementation: `UMeleeWeaponDefinition` now keeps the authored melee display, local marker, sweep-shape, Ability-grant, and optional Loadout data. The player-only `UWeaponEquipmentComponent` owns the runtime display/markers/spec handles and its validated `EquipWeapon` transaction. The player trace source consumes that equipment state first; the unchanged fixed-name path remains the first enemy fixture fallback.
- User validation: the user confirmed the post-repair manual `PolyQuestEditor` compile and Scene01 PIE equipment route, including normal weapon switching and combat use. Main did not run UBT, Editor, or PIE.
- Review: Main's strict review found the active Primary arbitration swap gap and the hidden foreign duplicate-spec gap; both repairs are present in the final source. A direct delta review and the user's GLM review report found no remaining P0/P1/P2 issue. The requested fresh `gpt-5.6-luna` reviewer returned HTTP 503, so no Luna independent-review result is claimed.
- Static evidence: direct final-source/caller reads and CodeGraph covered the component, Player, trace source, Primary Ability, Trace Window, and resolver boundary; `git diff --check` passes. Code-review-graph remains supplemental only and does not establish authored-asset or runtime coverage.
- Debt handoff: `ROADMAP.md` now records the deliberate v1 limit (player-only single StaticMesh slot, no public unequip/bare-handed state, no pickup/inventory/enemy equipment) and requires a reviewed design contract before any broader weapon category or representation adopts this foundation.
- Commit scope: the two new equipment headers, equipment component implementation, Player and trace-source changes, and exact `README.md`/`ARCHITECTURE.md`/`ROADMAP.md`/`plan.md` updates. Every `Content/**` item, DataAsset, Blueprint, debug binding, map, project setting, generated directory, and unrelated WIP remains excluded.

---

## Previous Stage Record: TODO-02F (durable record lives in ROADMAP Done Milestones)

- 02F delivered the Montage rate-window contract (NotifyState + Light/Charged/Sprint Attack integration with baseline restore across all teardown paths) with user-confirmed compile and PIE; the committed-cost atomicity debt stays open in ROADMAP per the revised disposition (no-refund is direction, not closure). Committed as `5852c8d`.
