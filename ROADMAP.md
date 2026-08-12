# PolyQuest Roadmap

## Purpose And Scope

PolyQuest is a UE 5.8 C++ GAS-first, single-player stylized action RPG. It rebuilds the proven player-facing contracts from the UE 5.7 `Test` project, rather than performing an in-place copy of its FSM implementation, assets, or save data.

The previous `Test` project remains the FSM behavior reference and validation baseline. PolyQuest reuses proven gameplay contracts after deliberate GAS redesign, but does not retain its C++ state machine, save schema, authored assets, or Marketplace content wholesale.

## Current State

- UE 5.8 Third Person C++ template created.
- Official Unreal MCP and VibeUE-enhanced editor access verified read-only.
- `GameplayAbilities` is available in the editor; `TODO-00B` added the game-module GAS dependencies, established the player ASC/AttributeSet contract, and registered the initial project Gameplay Tags.
- `TODO-01A` has completed its native C++ lifecycle and a local PIE fixture. Its selected player mesh/Skeleton/material closure and animation sequences are stable source assets; its GameplayAbility, GameplayEffects, Montage, AnimBP, `BP_Player`, and input assets remain deliberately uncommitted authoring WIP.
- `TODO-00C` replaced the active template route with `/Game/Maps/Scene01`, a desktop-only product Controller route, and a closed ThirdPerson/Variant retirement. The user recompiled `PolyQuestEditor` and replayed the existing PIE validation path after cleanup.
- `TODO-01C` has completed the first local GAS Dodge, Stamina exhaustion/recovery, attack cancellation, and NotifyState-timed invulnerability loop. Its authored GA/GE, Montage, AnimBP, Blueprint, input, retargeting, map, and test-fixture assets remain deliberately local WIP.
- `TODO-01E` has completed the native Primary short/hold arbitration and Charged Attack lifecycle. Its authored GA/GE, Montage, retargeted Sequence, Blueprint, Loadout, and input assets remain deliberately local WIP.
- External source packages remain under `Content/Assets/`. Imported content is not a production integration merely because it is present locally; skeleton, weapon, socket, animation, and presentation decisions still require their named stage validation.

## Test-To-PolyQuest Migration Contract Inventory

The old `Test` project is evidence for player-facing behavior, not a source tree to copy. The following contracts were implemented and manually validated there; PolyQuest will rebuild them through its own GAS, StateTree, data, asset, and save boundaries.

| Verified Test contract | PolyQuest reconstruction boundary | Owning stage |
| --- | --- | --- |
| Focused player profile plus small attack/action/reaction DataAssets | A PolyQuest character/ability manifest grants abilities and references focused authored data. It does not become a second runtime state machine. | `TODO-01A`, `TODO-01B` |
| Shared physical input intent across weapon and loadout combat styles, including press/hold/release semantics | A shared Gameplay Mapping Context emits stable input intent; GAS and the active weapon/loadout route that intent to the current melee, charged, ranged, or selected spell ability. | `TODO-01C1` |
| One complete `Montage + EntrySection` per linear combo entry, with local damage, stamina, poise, and optional warp data | A GAS-owned linear combo-chain asset. Each entry remains a complete authored attack module; runtime continuation state remains inside the active attack ability. | `TODO-01D` |
| One LMB pre-input during `ComboWindow`; early and late continuation during `ComboBranchWindow` | The active light-attack ability owns one continuation buffer. Notify states emit semantic begin/end events only; there is no global input buffer. | `TODO-01D` |
| Recovery `CancelWindow` permits Dodge, Parry, Block, or Potion only after each action's own preflight | A scoped cancellation opportunity exposed by the active attack ability. Higher-priority actions remain immediate ability requests, not buffered actions. | `TODO-01C`, `TODO-01D`, `TODO-02D`, `TODO-03B` |
| Weapon-collision notify state, swept trace, team filtering, one-hit protection, and centralized hit resolution | A single GAS damage path driven by semantic attack timing and a validated hit resolver. Animation timing never directly owns health or poise mutation. | `TODO-01A`, `TODO-02B` |
| Hold-to-charge, release attack, stamina exhaustion, and interruption cleanup | A dedicated charged-attack ability consumes the existing hold/release intent after Combo continuation arbitration; do not fold it into the linear combo asset. | `TODO-01C`, `TODO-01E` |
| Sprint state and Sprint Attack | Establish and validate a real Sprint movement/input/stamina contract before a Sprint Attack may consume it. | `TODO-01F` |
| Lock-on targeting, target switching, camera/facing ownership, and free-run exception | A focused target-lock and camera component boundary that consumes valid enemy targets but does not duplicate GAS combat state. | `TODO-02C` |
| Enemy attack-entry DataAssets, poise/stance break, hit/death safety, melee/ranged delivery | StateTree selects high-level intent; enemy GameplayAbilities execute attacks and GameplayEffects own combat mutation. | `TODO-02A`, `TODO-02B`, `TODO-02E` |
| Checkpoint, item ownership, transient Gold, fixed rewards, clear persistence, fog gate, and one-time defeat behavior | New PolyQuest persistence contracts with new stable IDs and validation fixtures. No old SaveGame schema or identifiers transfer. | `TODO-03A` through `TODO-04A` |
| Front critical and backstab use a valid punish target, animation timing, alignment, damage once-only, and teardown cleanup | Two separate GAS abilities after normal combat, stance break, and target reservation contracts are proven. | `TODO-05A`, `TODO-05B` |

### Migration Rules

- Preserve the player-visible contract when it is validated; redesign its runtime ownership for GAS rather than reproducing old fields, booleans, `EActionState`, or montage-delegate code.
- Animation is the timing and presentation source. Notify and NotifyState implementations send narrow semantic events to the active ability; only abilities, effects, and the hit resolver mutate gameplay.
- A completed, cancelled, or interrupted Montage callback from a replaced combo entry must never end or clear the successor entry. The GAS combo stage owns an explicit stale-callback guard and one `EndAbility` cleanup route.
- DataAssets hold authored references and tuning, never active input buffers, current combo indexes, mutable targets, or save state.
- Motion Warping remains conditional on a selected product animation set having valid root-motion alignment needs. It is not a foundation prerequisite and is not enabled merely because Test used it.
- Do not migrate a behavior merely because Test contained it. Each stage must retain its explicit player benefit, prerequisites, and fresh PolyQuest validation fixture.

## Done Milestones

- [x] `TODO-00A: Repository And Documentation Bootstrap v1`
  - Established the UE 5.8 template baseline, Git LFS, generated-output ignores, project documentation, and verified official Unreal MCP/VibeUE read routes.
  - Committed as `f64fbd2`; no GAS gameplay code, production asset integration, or template retirement was included.
- [x] `TODO-00B: GAS Core Contract v1`
  - Added the public GAS module dependencies, established character-owned ASC and AttributeSet initialization on possession, and registered the initial config-first project Gameplay Tags.
  - The user compiled `PolyQuestEditor`, verified the active `BP_Player`/`BP_PlayerController` route in PIE, confirmed the four attributes through `ShowDebug AbilitySystem`, and validated movement, look, and jump. No ability, effect, cue, replication, or combat behavior was added.
- [x] `TODO-01A: Player Ability Lifecycle And One-Hit Attack v1`
  - Established the first local player attack path: Enhanced Input requests `Ability.Attack.Light`; the ASC activates `ULightAttackAbility`; `CommitAbility()` applies the authored cost; Montage timing emits `Event.Attack.Light.Hit`; one sphere sweep applies an authored damage effect through the target ASC; all completion and teardown paths converge through `EndAbility()`.
  - The user compiled `PolyQuestEditor` and verified the local PIE fixture: movement/look/jump remain intact, a successful light attack spends stamina once, damages the target once, rejects reactivation during the active Montage, and works again after normal recovery.
  - Main review repaired premature end-on-blend-out behavior. A fresh adversarial review found no remaining P0/P1 issue. The local authored GA/GE/Montage/AnimBP/player/input configuration is intentionally excluded from this commit while it evolves, so this records validated local behavior rather than a clone-ready fixture.
- [x] `TODO-00C: Template Cleanup v1`
  - Replaced the template map route with `/Game/Maps/Scene01`, moved the retained GameMode and PlayerController source roots into `Framework/` without changing their reflected class names, and reduced the PlayerController to local desktop mapping-context setup.
  - Retired `APolyQuestCharacter`, the ThirdPerson map/Blueprint/External Actor closure, the generated Variant source/content closures, and legacy Third Person redirects after scoped Editor-reference and static evidence.
  - The user compiled `PolyQuestEditor` and verified Scene01 startup, desktop input, movement, look, jump, LMB stamina cost, and target damage. Main normal review and an adversarial fallback found no blocking issue; the requested independent Reviewer could not start because the configured service returned HTTP 503.
- [x] `TODO-01B: First Stylized Player Asset Integration v1`
  - Integrated the local Dungeon Knight presentation fixture: `SM_Wep_Ornate_Sword_02` is display-attached through `Weapon_R`, and one retargeted root-motion sword attack plays through the existing GAS light-attack path.
  - The user verified in Scene01 PIE that the sword remains attached, the Montage drives Actor movement cleanly, a single attack spends Stamina and damages once, repeated activation is blocked, no-target recovery succeeds, and movement/look/jump do not regress. The retargeted Sequence has no hit Notify; the Montage has one `Light Attack Hit` on `DefaultGroup.DefaultSlot`.
  - No C++ gameplay contract changed. Socket, Blueprint, AnimBP, Montage, GA/GE, input, and retargeting authoring assets remain local WIP; the separate stable asset subset does not recreate the playable fixture from a clean checkout.
  - Main normal review and an adversarial fallback found no source-level blocker. The requested fresh independent Reviewer could not start because the configured service returned HTTP 503.
- [x] `TODO-01C: Dodge, Stamina, And Action Interruption v1`
  - Added `UStaminaActionAbility`, Stamina clamping/exhaustion ownership, a ground-only camera-relative `UDodgeAbility`, attack recovery cancellation, NotifyState-timed invulnerability, and unified ability cleanup for Montage tasks and temporary effects.
  - The user confirmed `PolyQuestEditor` compilation and Scene01 PIE validation for Root Motion Dodge direction, positive-Stamina overdraft to zero, exhaustion/recovery, repeated input blocking, movement/jump lock with camera control, recovery-only attack cancellation, invulnerability timing, teardown cleanup, and the repaired no-delayed-jump regression.
  - Main normal review found and repaired a latched Jump release during Dodge. Main adversarial fallback found no further source-level blocker. The requested fresh `gpt-5.6-luna / xhigh` Reviewer returned HTTP 503, so no independent-review result is claimed.
  - GA/GE, Montage, AnimBP, Blueprint, input, retargeting, map, and verification assets remain local mutable WIP and are not a clean-checkout fixture.
- [x] `TODO-01C1: Combat Input Intent Routing And Hold/Release v1`
  - Added one native `UCombatLoadoutDefinition` route table and unified `PrimaryAttack`, Aim, and direct Ability Slot input handling. A `Started` event publishes the physical `Input.*` intent before resolving the active Loadout to a GAS Ability Tag; release and cancellation publish distinct events and clear held state without launching another Ability.
  - The production local `DA_CombatLoadout_StraightSword` originally routed only `Input.PrimaryAttack -> Ability.Attack.Light`; `TODO-01E` now routes it to `Ability.Attack.Primary` for short/hold arbitration. Aim and unconfigured production slots remain intentional no-ops. Keyboard/mouse is the accepted validation scope; controller evidence is tracked under `Known Risks And Validation Debt`.
  - Removed the retired `IA_Attack_Light` route and config Tag after a scoped Editor/source/asset audit. `Event.Input.*` is documented as active-Ability event delivery rather than a generic `AbilityTrigger` source. Main review found no remaining source blocker; the requested fresh `gpt-5.6-luna / xhigh` Reviewer could not start because the provider returned HTTP 503.
  - Input, Loadout, Blueprint, GA/GE, Montage, AnimBP, and map assets remain local mutable WIP and are excluded from the focused source/config/document commit.

- [x] `TODO-01D: Data-Driven Combo, Branch Window, And Recovery Cancel v1`
  - Replaced the single-Montage light-attack configuration with `UComboChainDataAsset`: each entry is one unique complete Montage, while `ULightAttackAbility` owns the active entry, one buffered `Input.PrimaryAttack`, per-entry Cost/Hit consumption, and recovery Dodge cancellation.
  - Montage identity now guards all hit/window events and end callbacks. Source animation is carried through `FGameplayEventData::OptionalObject`, so a replaced Montage cannot damage, reopen a window, or end its successor.
  - The user compiled `PolyQuestEditor` and confirmed the authored Scene01 PIE combo route. Main normal review and a Main adversarial fallback found no confirmed P0-P2 source or semantic-event blocker; the requested fresh `gpt-5.6-luna / xhigh` Reviewer did not start because the provider returned HTTP 503, so no independent-review result is claimed.
  - Combo DataAsset, GA/GE, Montages, AnimBP, Blueprint, input, map, retargeting, and presentation assets remain local mutable WIP. The focused commit contains only native source, Gameplay Tag config, and documentation.

- [x] `TODO-01E: Charged Attack v1`
  - Added a no-cost `UPrimaryAttackAbility` that preserves Combo priority, then resolves a held `Input.PrimaryAttack`: a short normal release requests Light Attack, while a held release routes to Charged Attack. `Event.Attack.Charged.ReleaseHandoff` carries the original held duration when Released arrives before the `0.2 s` delay callback, so the Character's cleared held-input cache cannot lose the Charged release.
  - Added `UChargedAttackAbility` with HoldReady pause, release-time Stamina Cost, `1.0x` through `1.8x` SetByCaller damage, one identity-filtered hit, Root Motion resume from the same playhead, and unified task/delegate/tag cleanup. Primary, Light, Charged, and Dodge use shared movement/jump input-block tags; Dodge cancels Primary during arbitration and Charged while it is charging, then returns to the authored recovery CancelWindow after release.
  - The user confirmed `PolyQuestEditor` compilation and the authored Scene01 PIE route, including the repaired threshold timing behavior. Main normal review and Main adversarial review found no confirmed P0-P2 source blocker. The requested fresh `gpt-5.6-luna / xhigh` Reviewer did not start because the provider returned HTTP 503, so no independent-review result is claimed.
  - GA/GE, Montage, Sequence, Blueprint, AnimBP, Loadout, input, map, and imported assets remain local mutable WIP. The focused commit contains only native source, Gameplay Tag config, and documentation.

## Milestones

### Player Combat Vertical Slice

- [ ] `TODO-01F: Sprint Foundation And Sprint Attack v1`
  - Establish a real Sprint input, movement, Stamina, cancellation, and validation contract before adding one Sprint Attack that requires the active Sprint state. Do not infer Sprint from a transient movement-speed check at attack time.
  - Keep Sprint Attack separate from Charged Attack and the linear Combo asset. Lock-on free-run behavior remains owned by `TODO-02C`, so this stage validates only the unlocked Sprint route and the explicit handoff points required for later lock-on work.

### First Enemy And Combat Targeting

- [ ] `TODO-02A: First Enemy GAS Combat And StateTree Intent v1`
  - Add the first enemy ASC, a small StateTree for patrol/alert/chase/combat/return intent, and one executable melee attack ability.
  - StateTree requests abilities and observes authoritative tags; it does not own health, poise, hit, death, cooldown mutation, or a second enemy FSM.

- [ ] `TODO-02B: Enemy Attack Profiles, Poise, And Stance Break v1`
  - Rebuild data-authored enemy attack selection, configurable distance/cooldown/attack presentation, hit reaction, poise depletion, stance break, death, and safe interruption around the first enemy.
  - Keep attack data separate from StateTree intent and do not import the old local HFSM.

- [ ] `TODO-02C: Lock-On And Combat Camera v1`
  - Add valid-target selection, manual target switching, facing/camera ownership, lock break rules, and the sprint free-run exception after a real enemy target exists.
  - Lock-on consumes target data and drives presentation/movement policy; it is not an alternate combat-state source.

- [ ] `TODO-02D: Defensive Combat And Hyper Armor v1`
  - Add directional block, timed parry, guard break, and notify-timed hyper armor only after the hit resolver, poise, and action cancellation contracts are stable.
  - Parry, Dodge, Block, and Potion remain immediate preflight actions during a combo CancelWindow; none becomes a global pre-input buffer.

- [ ] `TODO-02E: Player Bow, Projectile, And Ranged Enemy v1`
  - Rebuild Bow aiming, prepared-arrow presentation, socket-authoritative projectile delivery, and one StateTree-driven ranged enemy using the same GAS damage path.
  - Add aim offsets only when the selected assets expose a demonstrated vertical-aim mismatch; do not add a second enemy AI framework.

### Player Loop, Rewards, And Encounters

- [ ] `TODO-03A: Equipment, Pickup, And Checkpoint v1`
  - Rebuild only the player-loop data that the new game actually needs: item definitions/instances, world pickup transaction, equip boundary, checkpoint/rest, death reload, and a new save schema.
  - Do not migrate old SaveGame identifiers, old world actor names, or raw actor references.

- [ ] `TODO-03B: Potion And Consumable Loop v1`
  - Add the first healing consumable after ownership, save, action interruption, and cancellation contracts exist.
  - Preserve notify-timed healing and interruption semantics where the chosen animation needs them; do not add a backpack or general consumable framework without a player-loop need.

- [ ] `TODO-03C: World Drops, Fixed Rewards, And Persistence v1`
  - Rebuild transient enemy currency drops separately from persistent fixed-item rewards and one-time elite defeat state.
  - Each durable reward requires a stable authored ID, one transaction owner, failed-save behavior, and a reload-validation fixture.

- [ ] `TODO-04A: Encounter, Fog Gate, And Clear Persistence v1`
  - Build a new encounter clear-persistence loop, fog boundary, and participant ownership after the new save/checkpoint boundary is verified.
  - Fixed rewards remain the independent `TODO-03C` special-enemy contract; an encounter does not create a second reward or one-time-defeat system.
  - Add multi-enemy attack coordination only if the first real encounter proves that ordinary StateTree/GAS spacing cannot preserve readable combat.

### Punish And Level Completion

- [ ] `TODO-05A: Front Critical v1`
  - Rebuild a front-punish ability after normal attack, stance break, target reservation, root-motion/Motion-Warp validation, damage-once timing, and teardown paths are proven.
  - Do not add Backstab, a generic finisher framework, or Boss-specific critical behavior in this stage.

- [ ] `TODO-05B: Backstab v1`
  - Rebuild Backstab as a distinct ability with its own rear-geometry and target-validity contract. It may reuse only proven reservation, alignment, hit, and cleanup primitives from Front Critical.

- [ ] `TODO-06A: First Stylized Level Route And Integration v1`
  - Integrate validated systems into a deliberate level route: checkpoint, ordinary combat space, elite/fixed-reward branch, encounter, fog gate, Boss approach, and focused route validation.
  - This stage proves level composition and existing-system integration. It does not introduce the first Boss phase, completion state, or a second encounter/reward implementation.

- [ ] `TODO-06B: First Boss Phase And Completion v1`
  - Add one Boss StateTree/combat profile, Phase 1 -> transition -> Phase 2 intent, authored ability patterns, completion persistence, reward handoff, and return/continue behavior using the level route from `TODO-06A`.
  - Boss phase selection remains StateTree intent plus GAS abilities; a Boss does not justify a separate AI framework merely because it has no patrol.

- [ ] `TODO-07A: HUD, Debug, Feedback, And Demo Polish v1`
  - Add only the HUD, debug surfaces, VFX/SFX, camera, navigation readability, and presentation feedback needed to make the validated loop legible.
  - This is not a vehicle for new combat mechanics or a generic UI framework.
  - After the core player-combat framework and selected weapon animation set are stable, run one focused combat-presentation retune over provisional Montages: final source-animation selection, per-weapon Montage composition, root-motion feel, and manually authored segment rates.
  - Re-author `LightAttackHit`, Combo Input/Branch, Dodge Cancel, and Dodge Invulnerability timing against the final motions while retaining the proven one-buffer, Branch consumption, one-hit-per-entry, and ability-owned gameplay contracts.
  - If that retune needs runtime playback-rate control, create a separate approved RateWindow lifecycle stage. It must restore rates on natural completion, interruption, cancellation, combo handoff, `EndAbility()`, and EndPlay; temporary authored segment rates are not that system.
  - Re-run focused PIE visual and teardown checks for contact readability, root-motion continuity, hit/cancel timing, and stale tags/tasks after interruption.

- [ ] `TODO-07B: Full Route Regression v1`
  - Establish the repeatable manual validation matrix for New Game, Continue, checkpoint/rest, ordinary enemy reset, cleared encounter restore, equipment and consumables, drops/fixed rewards, player/enemy ranged combat, punish abilities, Boss completion, and completion Continue.
  - Fix regressions in previously completed systems only; this stage does not defer their original implementation or add new gameplay systems.

## Accepted Technical Direction

- **Test migration boundary:** migrate verified player-facing behavior and acceptance cases, not the old FSM, `EActionState`, save identifiers, or authored asset topology.
- **Gameplay authority:** GAS owns ability activation, costs, blocking/cancellation tags, combat state, damage, poise, hit reaction, and death. Animation owns timing and presentation; it is not a second gameplay state source.
- **Input intent:** Physical controls express stable player intent; the active weapon/loadout and GAS determine the concrete ability. Mapping Contexts change for control modes, not for profession labels or weapon inventory alone.
- **Data ownership:** a PolyQuest character/ability manifest composes focused authored assets. Combo entries, action settings, enemy attack profiles, and reaction data are modular tuning inputs; none stores mutable gameplay state or replaces ability/effect ownership.
- **Combo contract:** one active light-attack ability owns at most one buffered LMB continuation. `ComboWindow` accepts it, `ComboBranchWindow` consumes early input or retries late input, and `CancelWindow` exposes only a scoped immediate-cancel opportunity for preflight-valid actions.
- **Montage lifetime:** a successor combo entry must explicitly reject stale completion/interruption events from a prior entry. Natural completion, cancellation, hit/death teardown, invalid timing events, and asset failure converge through one ability cleanup path.
- **Combat timing:** weapon collision, potion heal, projectile release, parry active frames, hyper armor, combo windows, and cancellation windows are semantic animation events. Their receiver validates the active ability and current target/context before applying gameplay.
- **Enemy AI:** StateTree is the default high-level behavior brain for both ordinary enemies and Bosses. It owns patrol, alert, chase, combat intent, return-home, phase selection, and high-level transitions; it requests GAS abilities rather than directly applying combat mutations.
- **Bosses:** Bosses use a separate StateTree and combat data profile, not a different AI framework merely because they do not patrol. Encounter, fog-gate, persistence, and rewards remain outside the Boss StateTree.
- **No parallel FSM:** do not retain an enemy HFSM or `EEnemyState` as a competing runtime truth beside StateTree and GAS tags.
- **Behavior Tree escalation gate:** do not introduce Behavior Trees unless concrete production behavior requires deeply nested reactive arbitration, dynamic subtrees, or broad concurrent services that remain unclear and untestable in StateTree.
- **External resources:** `Content/Assets/` is a raw source reservoir. A resource becomes an imported product integration only when an approved stage names it and validates its exact Skeleton, socket, animation, or gameplay surface.

## Recommendations

- **Behavior Trees:** keep Behavior Tree tooling available but unadopted. Re-evaluate it only at the documented escalation gate; do not split ordinary enemies and Bosses across AI frameworks preemptively.
- **Motion Warping:** add product-level Motion Warping only when the selected stylized animation set contains a concrete root-motion alignment requirement.

## Known Risks And Validation Debt

- `UAbilityTask_PlayMontageAndWait::ExternalCancel()` can end a task without an explicit guarantee that the Montage stops. Static inspection found no current PolyQuest caller. When `TODO-01C` or a later Stun/explicit-interrupt path needs task-level cancellation, it must define montage-stop behavior, converge through `EndAbility()`, and add focused PIE coverage before relying on that path.
- `TODO-01A` deliberately withholds its mutable authoring assets from this commit. A curated stable asset baseline is required before the local fixture can be reproduced from a clean checkout; this is a versioning boundary, not a failure of the local compile/PIE validation.
- `TODO-01B` intentionally skips the sword/sequence Reference Viewer dependency-closure audit by user decision. Its direct stable-asset commit may therefore omit material or Skeleton dependencies and must not be treated as a clean-checkout asset baseline; rerun the closure audit before promoting it as one.
- `TODO-01C1` has accepted keyboard/mouse PIE evidence only. Right Shoulder and Left Trigger mapping behavior and physical controller operation are not verified controller support. Before presenting controller support, adding controller-specific UX, or producing a controller-facing build, read back the intended Mapping Context entries and pass focused hardware validation for every intended controller action; until then, do not claim gamepad support.
- `TODO-01D` spends a continuation Cost before `UAbilityTask_PlayMontageAndWait` confirms that the successor Montage actually started. A rare playback-start failure after valid preflight can therefore consume Stamina and end the Ability without an automatic refund. The normal AnimInstance/Slot path has user PIE evidence, but failure injection is unverified. Before defining a final combat asset baseline or adding runtime playback-rate control, define the atomicity or refund semantics and add focused failure-injection coverage.
- `TODO-01E` has manual threshold validation, including the repaired normal-release-before-delay route, but no deterministic same-frame input/timer injection test. Before changing input-event dispatch order, introducing prediction/networking, or relying on frame-exact charge thresholds, add a deterministic automated fixture or controlled logging harness that exercises both callback orders and verifies exactly one Light or Charged activation.

## Stage Completion Standard

A roadmap stage is complete only after its approved plan, focused source/asset validation, user-owned compilation/PIE evidence where applicable, review, documentation synchronization, and explicit commit approval are complete.
