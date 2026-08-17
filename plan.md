# TODO-03A3: World Equipment Pickup And Drop-Swap v1

## Status

- **Plan state:** completed, validated, strictly reviewed, and closed.
- **Baseline:** `3f929f5 [Feature] 主手与空手战斗合同 (Main-Hand And Unarmed Combat)`.
- **Validation:** Automation test `PolyQuest.Equipment.TransactionMatrix` completed with `Success` across all 11 test cases (real fixtures, ECC_Visibility ground projection, bidirectional conflict preflight rejection, TwoHanded and Shield pickup transitions, 2-drop/1-drop enumeration, zero-leak drop cleanup, full ASC AbilitySpec and handle ownership restoration on Apply/Drop failures via `VerifyPreparedSlotBinding`).
- **Manual Scene01 validation:** The user confirmed the live pickup route: `E` equips the selected fixture, the replaced weapon remains visible on the ground near the player's previous position, and the source/drop behavior matches the transaction contract. The screenshot confirms the displaced weapon presentation in the running scene; authored `Content/**` remains user-owned WIP.
- **Strict Review:** Main defect-first review + Independent Fresh Reviewer subagent (`e5aa2404-efe1-4ff0-958c-1525870affb2`) completed with "No findings."

## Objective

Deliver the first map-facing equipment loop: pressing `E` on one nearby world weapon pickup either completes one validated direct-equipment transaction and then materializes every displaced equipped definition as a world pickup, or changes nothing.

The primary runtime question is not "can a mesh be picked up?" It is whether world interaction can cross the existing MainHand/OffHand composition boundary without losing Ability specs, prepared-slot identities, displays, trace markers, or a displaced item when any later step fails.

## Fixed Product Contract

- `E` is a local one-shot world interaction, not a Combat Loadout input and not a Gameplay Event/Gameplay Tag route.
- v1 directly equips a compatible immutable `UWeaponDefinition`; it has no inventory, backpack, persistence, random affixes, durability, pickup instance state, or `EquipmentInstance`.
- Existing `UWeaponEquipmentComponent` remains the sole runtime owner of equipped MainHand/OffHand definitions, displays, markers, weapon-granted specs, prepared slots, active loadout changes, preflight, apply, rollback, and combat-time swap refusal.
- A world pickup is a map/visual interaction actor only. It may reference an immutable weapon definition, but it may not grant abilities, modify ASC tags/specs, author damage/collision combat, store mutable equipment state, or create a second combat authority.
- The existing public `EquipWeapon(UWeaponDefinition*)` stays compatible for initial/default/debug equipment routes. A new narrow world-pickup transaction entry reuses the same composition/preflight/apply machinery; a pickup must never call `EquipWeapon` and then independently guess what to drop.
- A successful new composition is required before any displaced pickup is created. If preflight, apply, ground projection, or any deferred drop spawn fails, the source pickup remains, every provisional drop is destroyed, and the prior equipment composition is restored by definition identity.
- A same-definition world pickup is a no-op rejection: it consumes no source pickup, creates no drop, and leaves the existing composition untouched. This differs deliberately from the debug-friendly `EquipWeapon` same-slot `true` no-op.
- A spawned displaced pickup rejects its former owner for `0.5s` of world time. It has no throw impulse or initial physics simulation.
- Runtime spawned pickups are transient map state: map reload/reset destroys them; initial placed pickups return through normal map loading. No persistence contract is implied.
- Shield in this stage proves only OffHand occupancy, display, composition conflict handling, rollback, and drop-swap. It has an empty Defense Profile and no Shield action candidates. RMB/Q continue to resolve through the current MainHand fallback until `TODO-03A4`.
- The plan keeps the existing v1 Unarmed one-socket contact limitation unchanged. It does not add a second trace/resolver path or solve opposite-hand punch fidelity.

## Accepted Decisions

| ID | Decision | Implementation consequence |
| --- | --- | --- |
| D1 | Add a narrow concrete `UOffHandWeaponDefinition` for Shield-like OffHand assets. | `UWeaponDefinition` remains abstract. The subtype validates `HandSlot == OffHand`; it adds no Trace, damage, Ability, runtime state, or component. A header-only type is acceptable when its validation remains inline. |
| D2 | Do not add `UTwoHandedWeaponDefinition`. | A real `UMeleeWeaponDefinition` authored as `MainHandTwoHanded` is the 03A3 transaction fixture. Create a neutral `DA_Weapon_TwoHandedFixture` from the known-good Sword shape as user WIP; it is not an early Bow/Staff/Greatsword gameplay implementation. |
| D3 | `UWeaponEquipmentComponent` owns an explicit `UnarmedFallbackDefinition`. | It is configured on the inherited component in `BP_Player`, typed narrowly as `UMeleeWeaponDefinition`, and points to `DA_Weapon_Unarmed`. No hardcoded asset path and no inference from `DefaultEquippedWeapon`. |
| D4 | `E` chooses the nearest native QueryOnly pickup overlap. | `APlayerCharacter` obtains nearby `AWorldWeaponPickup` actors from an overlap-enabled native interaction shape, chooses smallest distance squared, then uses a stable actor-name/order tie-break. Overlap generation maintains candidates only; it never directly equips on Begin/EndOverlap. No camera/line trace and no generic `IInteractable` framework. |
| D5 | Use `ECC_Visibility` for ground projection. | The user must confirm Scene01 ground blocks Visibility. The projection starts near the player's feet, traces down `300cm`, offsets the hit by `2cm` along its normal, and fails closed when there is no valid hit. No new collision channel and no reuse of melee `GameTraceChannel1`. |
| D6 | Separate raw illegal-composition tests from public pickup behavior. | Automation directly proves `{ TwoHanded, OffHand }` preflight rejection in both construction directions. Public world pickup instead normalizes to `{ TwoHanded, null }` or `{ Unarmed, Shield }` before preflight and must succeed when all authored data is valid. |
| D7 | Use a native Player test fixture with the actual Player SkeletalMesh and real sockets. | The test does not load or mock an entire `BP_Player` and is not a pure DataAsset test. The user supplies the actual mesh path and MainHand/OffHand socket names. If that mesh remains user WIP, the suite is explicitly local-asset-dependent and not a clean-checkout fixture. |

## Existing Runtime Facts And Boundaries

- `UWeaponEquipmentComponent` currently owns the two transient hand definitions, display components, main-hand markers, granted handles, prepared classes/handles, input resolution, and the only public mutation route. `EquipWeapon` snapshots the old composition, tears it down, applies a new one, and identity-restores on a failed apply.
- `RunPreflight` currently rejects a direct `{ MainHandTwoHanded, OffHand }` composition before mutating handles. `TODO-03A3` must generalize the component's internal target-composition calculation rather than relaxing that rule.
- `UMeleeWeaponDefinition` is the compatible MainHand melee subtype because it carries Marker/Sweep fields. It remains correct for the TwoHanded fixture, but incorrect for Shield.
- `APlayerCharacter::SetupPlayerInputComponent` already binds authored Enhanced Input actions at `Started`/`Completed`/`Canceled`. `InteractAction` adds only an `ETriggerEvent::Started` binding and must not disturb held combat input, combo buffering, Primary arbitration, or `AbilitySlotActions`.
- Existing `CanSwapNow` blocks changing equipped definitions during Attack, Guard, Parry, Dodge, active weapon-owned prepared skills, and active Primary arbitration. Pickup interaction must respect that gate; it must not cancel an Ability to force an equip.

## Runtime Ownership

| Owner | Responsibilities in 03A3 | Explicitly not responsible for |
| --- | --- | --- |
| `APlayerCharacter` | Holds/binds `InteractAction`; chooses one eligible nearby pickup under D4; passes the request to the equipment component; forwards a result to optional presentation-only Blueprint hooks. | Direct ASC/spec changes, hand-slot mutation, prepared-slot mutation, drop calculation, rollback, inventory, or combat input arbitration changes. |
| `UWeaponEquipmentComponent` | Builds the complete target MainHand/OffHand pair; enforces D3/D6; runs `CanSwapNow`, full preflight, snapshots, teardown, apply, identity restore, and calculates displaced definitions. It owns the commit/rollback decision until world drops have succeeded. | Picking a map candidate, direct Blueprint state, collision/damage truth, persistence, dynamic physics, or a public generic unequip API. |
| `AWorldWeaponPickup` | Holds one immutable definition; supplies an overlap-enabled QueryOnly native interaction shape and definition-derived visual presentation; guards reentrancy through `CanInteract`; exposes a friend-only native staging method for deferred world drops; records former owner/reject deadline; destroys the source after final success; performs no compensation from `EndPlay`. | Ability grants, Gameplay Tags, Attribute writes, combat hit collision, action selection, inventory state, arbitrary Blueprint decision graphs, or autonomous calls to the old public `EquipWeapon`. |
| `UOffHandWeaponDefinition` | Concrete authored OffHand asset identity and exact slot validation. | Trace marker/sweep data, damage configuration, ability lifecycle, a Shield action system, or a second defense resolver. |
| `UMeleeWeaponDefinition` TwoHanded fixture | Existing MainHand mesh/marker/sweep/base-action shape with `HandSlot=MainHandTwoHanded`. | Bow draw/fire, projectile delivery, staff casting, special two-handed rules beyond hand occupancy. |
| `BP_WorldWeaponPickup` / DataAssets | Presentation and immutable authored references only. | Equipment transaction logic, overlap decision branches, attack collision, damage, tags, save state, or `EventGraph` ownership. |

## Target Composition And Displacement Rules

| Current composition | Incoming definition | Target composition before preflight | Displaced definitions after commit |
| --- | --- | --- | --- |
| `{ Unarmed, null }` | OneHanded MainHand | `{ Incoming, null }` | none |
| `{ MainHand A, OffHand B }` | OneHanded MainHand C | `{ C, B }` | `A` |
| `{ MainHand A, OffHand B }` | TwoHanded C | `{ C, null }` | `A`, `B` |
| `{ TwoHanded A, null }` | OneHanded MainHand B | `{ B, null }` | `A` |
| `{ TwoHanded A, null }` | OffHand Shield B | `{ UnarmedFallbackDefinition, B }` | `A` |
| `{ MainHand A, OffHand B }` | OffHand Shield C | `{ A, C }` | `B` when `B` exists and differs from `C` |
| any | same definition already in its effective target slot | no-op rejection | none; source pickup stays in world |

`UnarmedFallbackDefinition` is a legal MainHand definition but is never a displaced world item. A malformed or missing fallback rejects the transaction before teardown.

## Atomic Pickup Flow

1. **Input and candidate selection:** `IA_Interact` fires `Started`. `APlayerCharacter` gathers only currently overlapping, valid native `AWorldWeaponPickup` actors, selects one by D4, and returns immediately when none exists. It does not send combat input events or edit an ASC.
2. **Source eligibility:** the selected pickup rejects an invalid Definition, a dead/tearing-down requester, an active interaction, or its former owner while `WorldTime < RejectUntil`. It then sets only a temporary `bInteractionInProgress` guard. The source is still alive and no map/ASC state has changed.
3. **Pure target construction:** the component receives the incoming Definition through a narrow world-pickup entry point. It builds the entire target pair from current composition, hand slot, and D3. TwoHanded always clears target OffHand; picking an OffHand while a TwoHanded MainHand is current first chooses the canonical `{ UnarmedFallbackDefinition, IncomingOffHand }` target.
4. **Fail-closed preflight:** before any teardown, the component checks `CanSwapNow`, Player/ASC validity, Player SkeletalMesh sockets, every target definition, legal hand composition, candidate/prepared grants, duplicate classes, Startup Ability conflicts, and foreign Specs. A preflight failure or no-op clears `bInteractionInProgress`, preserves the source pickup, and has zero handle/display/marker/world-drop mutation.
5. **Snapshot and apply:** the component privately snapshots prior MainHand, OffHand, ActiveCombatLoadout, prepared-class identities, and the data required for a fresh-handle restore. It tears down then applies the target composition using the existing one-authority grant/display/marker path. It retains the snapshot after a successful apply instead of committing immediately.
6. **Provisional displaced drops:** only after a complete target composition exists, the component invokes the source pickup's friend-only native staging method. The pickup ground-projects near the player's feet using D5, then creates every displaced pickup using `SpawnActorDeferred`. Each provisional Actor has interaction collision disabled until every spawn and initialization succeeds. Before `FinishSpawning`, it receives the Definition, `FormerOwner=Player`, and `RejectUntil=WorldTime+0.5`.
7. **Commit:** after all provisional Actors finish spawning and enable QueryOnly interaction, the component drops its snapshot and reports success. The source pickup executes presentation-only success feedback and destroys itself. The new equipped composition is the single equipment truth; the spawned Actors are only future map entry points.
8. **Any post-apply failure:** failure of projection, deferred spawn, initialization, or finish-spawn destroys every provisional dropped Actor, then asks the component to restore the old definition identities, loadout, prepared classes, displays, markers, and freshly granted valid component-owned handles. The source pickup clears its guard, remains in the map, and reports failure. A failed restore is a fatal configuration error: source is not consumed and no provisional drop remains, but the defect blocks stage closeout.
9. **Teardown:** `EndPlay`, level reset, actor destruction, or invalid owner pointers only clear guards/references. They do not spawn replacement pickups, revive a source, or mutate an ASC after teardown.

## Public And Private API Shape

- Keep `EquipWeapon(UWeaponDefinition*)` as the current direct/debug route. Do not give it hidden world-spawn side effects.
- Add one narrow C++ world transaction entry on `UWeaponEquipmentComponent`, concretely `TryEquipWorldPickup(AWorldWeaponPickup* SourcePickup)`. It is the only world-entry mutation and is not Blueprint-callable. The component reads the source Definition, owns target construction/snapshot/apply/rollback, and calls a friend-only native staging method on the source pickup to provision displaced Actors. No public `TFunctionRef`, generic two-phase transaction token, generic unequip, inventory API, or direct Blueprint composition API is allowed.
- Keep target-pair construction, raw composition validation, snapshot lifecycle, displaced-definition calculation, and restore private to the component. Tests may access a strictly test-only seam only under `WITH_DEV_AUTOMATION_TESTS`.
- `AWorldWeaponPickup` exposes only `CanInteract`, immutable Definition/presentation properties needed by Player and Blueprint placement, and the private/friend-only staging method used by the component. Success/failure presentation may be a narrow `BlueprintImplementableEvent` or equivalent callback, but it must carry no gameplay decision back into the transaction.
- No new Gameplay Tags, GameplayEffects, Abilities, Damage GE, Trace Task, Resolver, StateTree topology, Controller logic, or Build.cs module dependencies are expected. Any claim that one is needed pauses implementation for a plan amendment.

## Implementation Sequence

1. Re-read the final approved plan, current component source/callers, `PlayerCharacter` input binding, weapon definitions, and direct test infrastructure. Confirm the current CodeGraph/code-review-graph freshness before using either as supplemental evidence.
2. Add `UOffHandWeaponDefinition` and the D3 component property/validation. Do not create a `UTwoHandedWeaponDefinition` or alter `UMeleeWeaponDefinition` semantics.
3. Refactor only the component's private composition flow enough to support a complete target pair, displaced-definition calculation, retained snapshot, friend-only post-apply staging call, and identity restore. Preserve the existing `EquipWeapon` behavior and its existing callers.
4. Add the test-only one-shot apply-failure seam and raw-composition validation access under `WITH_DEV_AUTOMATION_TESTS`; it may not leak a production debug switch. Add the Automation suite before treating rollback behavior as done.
5. Add `AWorldWeaponPickup` with no Tick: a definition-derived visible mesh, a native QueryOnly interaction shape, reentrancy/former-owner guards, deferred drop factory, provisional-spawn cleanup, and safe `EndPlay` behavior. It must not become a combat collision actor.
6. Add `InteractAction` and `Started` binding to `APlayerCharacter`. Keep selection D4 in Player and world transaction ownership in the component. Do not route `E` through held combat input, `CombatLoadoutDefinition`, or a new tag.
7. Run Main static checks. The user then compiles `PolyQuestEditor`; only after C++ types are available does the user author/read back the D1-D7 Editor fixtures.
8. Run the native Automation suite and Scene01 PIE matrix. Diagnose failures from the first bad transition only, apply the smallest source repair, and repeat the affected gate.
9. Gemini performs the user-arranged strict review after user-confirmed validation. Main performs a separate fresh review after the Gemini review/fixes; a repair requires affected validation rerun and delta review before documentation closeout.

## Candidate Native Paths

| Path | Planned responsibility |
| --- | --- |
| `Source/PolyQuest/Public/Combat/Equipment/OffHandWeaponDefinition.h` | New narrow concrete OffHand DataAsset type and exact-slot validation. A `.cpp` exists only if non-inline logic actually requires it. |
| `Source/PolyQuest/Public/Combat/Equipment/WeaponEquipmentComponent.h` | Explicit `UnarmedFallbackDefinition`, narrow world-transaction API/result types, private snapshot/composition seams, and test-only declarations. |
| `Source/PolyQuest/Private/Combat/Equipment/WeaponEquipmentComponent.cpp` | Target construction, preflight/rollback reuse, displaced-definition calculation, post-apply callback transaction, and test-only failure injection. |
| `Source/PolyQuest/Public/Combat/Equipment/WorldWeaponPickup.h` | New native world pickup Actor, immutable Definition, QueryOnly interaction surface, former-owner state, and narrow presentation/interaction API. |
| `Source/PolyQuest/Private/Combat/Equipment/WorldWeaponPickup.cpp` | Definition-derived presentation, eligibility/reentrancy, ground projection, deferred drop spawning, provisional cleanup, and EndPlay convergence. |
| `Source/PolyQuest/Public/Character/Player/PlayerCharacter.h` | `InteractAction`, one-shot interaction handler, and presentation-only result hook. |
| `Source/PolyQuest/Private/Character/Player/PlayerCharacter.cpp` | Existing-style `Started` binding and deterministic nearby-pickup selection only. |
| `Source/PolyQuest/Private/Tests/WeaponEquipmentComponentAutomationTests.cpp` | New `WITH_DEV_AUTOMATION_TESTS` fixture and 03A3 transaction coverage. |
| `ARCHITECTURE.md`, `ROADMAP.md`, `README.md`, `plan.md` | Exact closeout hunks only after implementation, user validation, reviews, and debt handoff. |

Expected unchanged native/config paths include `MeleeWeaponDefinition.h`, `Config/Tags/PolyQuestGameplayTags.ini`, `PolyQuest.Build.cs`, every Ability/GE/Trace/Resolver file, AI/StateTree, and all Enemy classes. Do not add a `TwoHandedWeaponDefinition` file.

## User-Owned Editor Work

Perform this only after the native C++ classes compile and appear in the Editor. All assets remain user-owned WIP and are excluded from the native commit unless the user later approves a stable asset closure.

1. Create digital `IA_Interact`, map exactly one E binding in the active `IMC_Default`, and assign it to inherited `BP_Player.InteractAction`. Confirm the existing Controller still installs that mapping context and no duplicate E binding consumes it.
2. Create `DA_Weapon_Shield` with parent `UOffHandWeaponDefinition`. Confirm its enforced `HandSlot=OffHand`, display mesh, exact OffHand socket, empty Defense Profile, and empty Base/Reusable/Exclusive/Prepared action lists. It is an occupancy/display fixture only.
3. Create `DA_Weapon_TwoHandedFixture` with parent `UMeleeWeaponDefinition`, `HandSlot=MainHandTwoHanded`, valid visible mesh, existing-compatible socket/markers/radius/subdivisions, and a known-valid base input/action configuration. It may reuse the known Sword behavior for the fixture, but it must not claim Bow/Staff/Greatsword gameplay or add projectile assets.
4. Read back `DA_Weapon_Unarmed` exact asset path, parent class, hand slot, owner-mesh socket, markers, and bind it explicitly to `BP_Player`'s `UnarmedFallbackDefinition` property. Do not assume the initial default weapon is Unarmed.
5. Create `BP_WorldWeaponPickup` from the native Actor. It may set visual defaults and `WeaponDefinition` on placed instances, but its EventGraph must contain no equipment, damage, collision, ability, tag, save, or candidate-selection logic. Place Sword, Shield, and TwoHanded fixture instances in Scene01.
6. Read back collision: world pickup visual mesh has no collision; native interaction shape is QueryOnly and generates overlaps for Player candidate discovery; no Begin/EndOverlap callback directly equips, no physics simulation, overlap-driven damage, melee trace, or Blueprint collision branch exists.
7. Confirm Scene01's eligible ground blocks `Visibility`. Verify the prescribed foot-near `300cm` downward projection has a valid point with the `2cm` normal offset, without blocking the Player or causing a throw.
8. Supply the Automation fixture facts: actual Player SkeletalMesh asset path, MainHand socket name, OffHand socket name, and whether the mesh is committed or user WIP. The test must use that actual mesh, not a fake socket or whole-BP mock.

## Native Automation Slice

The suite runs under `WITH_DEV_AUTOMATION_TESTS` using a native `APlayerCharacter` test fixture, real ASC, real `USkeletalMeshComponent`, and the D7 mesh/socket asset. It must not rely on a pure DataAsset mock as proof of Player socket preflight. Any WIP asset dependency is declared in output and documentation.

Test hooks are private/test-only, deterministic, one-shot, and removed from shipping behavior. They may inject exactly one new-composition apply failure or one deferred-drop failure, but may never fire again during restoration.

| Test | Required assertion |
| --- | --- |
| Raw illegal composition, both directions | A deliberately constructed `{ TwoHanded, OffHand }` target is rejected before mutation in both construction orders; no component handle, marker, display, or current-definition change. |
| Public TwoHanded conversion | `Sword + Shield + TwoHanded pickup` resolves to `{ TwoHanded, null }`; both displaced definitions are scheduled as drops only after target apply. |
| Public OffHand conversion | `{ TwoHanded, null } + Shield pickup` resolves to `{ Unarmed, Shield }`; only the former TwoHanded item is displaced. |
| Standard and Unarmed replacement | OneHanded replacement drops the old OneHanded item; Unarmed to OneHanded creates no nonexistent Unarmed drop; same-definition pickup is a non-consuming no-op. |
| Duplicate grants | Duplicate Base/Prepared or cross-slot ability classes reject before mutation. |
| Startup/foreign Spec conflict | Startup Ability collision or same-CDO foreign ASC Spec rejects while preserving current component-owned and foreign specs. |
| Keep-if-compatible layout | Compatible prepared class stays at its existing index; incompatible slots reseed MainHand then OffHand candidates only. |
| Apply failure identity restore | A one-shot target apply failure restores old MainHand, OffHand, loadout, prepared-class identities, display/markers, and valid fresh component-owned handles. Numeric handle reuse is not required. |
| Prepared-slot triple validation | Invalid handle, non-component handle, and spec-class mismatch each fail activation; only the current exact component-owned matching handle activates. |
| Ground/drop failure atomicity | No drop exists after preflight/apply/projection/deferred-spawn failure; provisional drops are removed and old composition restores. |
| Former owner and teardown | Former owner is rejected before `0.5s`, can interact after expiry, and teardown does not dereference invalid Player or emit compensation drops. |

Automation does not prove Enhanced Input hardware dispatch, visual placement quality, Shield defense behavior, Bow combat, network replication, persistence, or Resolver precedence. Those remain outside this stage.

## Validation Matrix

| Gate | Owner | Success condition | Evidence type |
| --- | --- | --- | --- |
| Main static gate | Main | Re-read final source/callers, CodeGraph C++ flow, type/slot/asset-facing validation paths, test seam isolation, no duplicate input binding, and `git diff --check`. Use code-review-graph only as supplemental impact evidence when its index is current. | Static only, not compile or PIE. |
| Manual compile | User | Compile `PolyQuestEditor` (Development Editor) and report exact first failure or success. | Compile. |
| Editor readback | User | Complete all eight authoring/readback items above, including D5 collision and D7 mesh/socket facts. | Editor readback, not PIE. |
| Native Automation | User | Run the 03A3 `PolyQuest.Equipment` suite with all listed transaction cases passing. | Automation, not visual/input proof. |
| Scene01 direct pickup | User | E equips OneHanded, Unarmed-to-OneHanded, Shield, and TwoHanded fixtures; MainHand/OffHand displays/specs/markers are coherent and no illegal pair remains. | PIE/runtime. |
| Scene01 refusal and rollback | User | E during Attack, Guard, Parry, Dodge, active Primary arbitration, or active prepared skill leaves source and old composition intact. Invalid data/failure injection likewise leaves no drop. | PIE/runtime. |
| Scene01 drop lifecycle | User | Replaced definitions land on validated ground without initial physics throw; former owner cannot immediately re-pick; after `0.5s` it can; map reload restores placed pickups and clears runtime drops. | PIE/runtime/visual. |
| Shield fallback regression | User | With Shield equipped, RMB/Q still use MainHand fallback and no Shield block/parry presentation is claimed. | PIE/runtime. |
| Existing combat regression | User | Sword/Unarmed LMB, Charged, Sprint Attack, Guard, Parry, Prepared Slot, existing debug swap refusal, enemy trace/damage, and current death/Poise contracts remain intact. | PIE/runtime. |

The planning-time no-evidence note is superseded by the user-confirmed Automation run and the focused Scene01 manual pickup/drop validation recorded above. Controller, network, package, and mutable authored-asset clean-checkout evidence remain outside this stage.

## Review And Closeout Sequence

1. Gemini performs a read-only plan review before implementation. Any material architecture objection returns to this plan; it does not become an implementation-time guess.
2. After user-confirmed compile, Editor work, Automation, and PIE, Gemini performs the user-arranged strict review against the approved source/test/document scope.
3. After Gemini's fixes and the relevant validation rerun, Main performs a separate fresh review of the final diff plus direct callers/callees. This is independent source/static review, not a claim of additional PIE evidence.
4. A real blocker is repaired before closeout. Any repair repeats the affected validation and receives delta review before documentation or commit approval.
5. Completed for this stage: `ARCHITECTURE.md` records the implemented single transaction/world-pickup ownership contract, `ROADMAP.md` marks 03A3 done and retains the explicit 03A3B sweep-authoring follow-up, and this plan retains the evidence and commit boundary until the next accepted stage replaces it.

## Debt And Commit Boundary

- Retain the existing Unarmed alternating-hand contact release gate unchanged. 03A3 neither proves nor bypasses it.
- If D7 relies on an uncommitted Player mesh/socket asset, record the exact local-only dependency and closure condition in `ROADMAP.md`; do not call the Automation suite clean-checkout coverage.
- If Scene01 does not block `Visibility`, do not add a silent alternate trace channel. Record the failed Editor fact, correct/approve collision policy, and rerun D5 validation before closing the stage.
- Any failed map-reset, ground-projection, restore, or former-owner validation gets a canonical roadmap entry with affected boundary, evidence, impact, owner, and concrete closure trigger. "Deferred" alone is not sufficient.
- Candidate commit paths are only the final approved `Source/PolyQuest/**` paths above, native test file, and exact documentation hunks. Exclude all `Content/**`, `.uproject`, maps, input assets, Blueprints, AnimBPs, GAs/GEs/Montages, imported assets, generated files, `.zcode/`, and unrelated user WIP.
- No staging or commit occurs without the user's later explicit approval. Never use `git add -A`.

## Route And Delegation Record

```text
Outer: ue-stage-workflow
Primary: ue5-cpp-gameplay
Support: unreal-enhanced-input, ue5-blueprint-workflow, ue5-debug-validation
Route reason: The stage extends a shared equipment composition transaction, adds a native world Actor, binds one Enhanced Input intent, and requires authored DataAsset/Blueprint fixtures plus rollback-oriented validation.
```

```text
Plan explorers: 0
Implementation executors: 0
Complex Executor: none
Main parallel work: none
Reason: Equipment composition, ASC grants, prepared-slot identities, rollback, world-drop commit order, and Player input are one coupled lifecycle. Parallel writers would create conflicting transaction ownership. Gemini participation is user-directed review/execution after this plan is approved, not a delegated Codex child writer.
```

## Non-Goals

- No inventory/backpack, pickup persistence, SaveGame, short/held E arbitration, item instances, random affixes, upgrades, durability, crafting, or generic interaction framework.
- No public unequip or automatic bare-handed fallback outside the explicitly configured `UnarmedFallbackDefinition` needed for the TwoHanded-to-OffHand transaction.
- No Shield defense override, Shield ability, Shield animation, Defense Profile change, or prepared Shield skill before `TODO-03A4`.
- No Bow/Staff/Greatsword gameplay, projectiles, ranged mode, or a `UTwoHandedWeaponDefinition` before `TODO-03B` has a concrete behavior need.
- No enemy pickup, enemy dynamic switching, enemy Attack Set, StateTree change, Controller change, melee trace/resolver change, damage change, Gameplay Tag change, multiplayer authority, or physics throw behavior.
- No `Content/**` mutation by Main, no asset import/delete/redirect, no map cleanup, and no clean-checkout claim for mutable authored WIP.
