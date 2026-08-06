# PolyQuest Roadmap

## Purpose And Scope

PolyQuest is a UE 5.8 C++ GAS-first, single-player stylized action RPG. It rebuilds the proven player-facing contracts from the UE 5.7 `Test` project, rather than performing an in-place copy of its FSM implementation, assets, or save data.

The previous `Test` project remains the FSM behavior reference and validation baseline. PolyQuest reuses proven gameplay contracts after deliberate GAS redesign, but does not retain its C++ state machine, save schema, authored assets, or Marketplace content wholesale.

## Current State

- UE 5.8 Third Person C++ template created.
- Official Unreal MCP and VibeUE-enhanced editor access verified read-only.
- `GameplayAbilities` is available in the editor; `TODO-00B` has added the game-module GAS dependencies, established the player ASC/AttributeSet contract, and registered the initial project Gameplay Tags.
- External resource packs are held as raw, untracked source packages under `Content/Assets/`; none has been imported, selected as the production Skeleton baseline, or integrated into PolyQuest gameplay.

## Test-To-PolyQuest Migration Contract Inventory

The old `Test` project is evidence for player-facing behavior, not a source tree to copy. The following contracts were implemented and manually validated there; PolyQuest will rebuild them through its own GAS, StateTree, data, asset, and save boundaries.

| Verified Test contract | PolyQuest reconstruction boundary | Owning stage |
| --- | --- | --- |
| Focused player profile plus small attack/action/reaction DataAssets | A PolyQuest character/ability manifest grants abilities and references focused authored data. It does not become a second runtime state machine. | `TODO-01A`, `TODO-01B` |
| One complete `Montage + EntrySection` per linear combo entry, with local damage, stamina, poise, and optional warp data | A GAS-owned linear combo-chain asset. Each entry remains a complete authored attack module; runtime continuation state remains inside the active attack ability. | `TODO-01D` |
| One LMB pre-input during `ComboWindow`; early and late continuation during `ComboBranchWindow` | The active light-attack ability owns one continuation buffer. Notify states emit semantic begin/end events only; there is no global input buffer. | `TODO-01D` |
| Recovery `CancelWindow` permits Dodge, Parry, Block, or Potion only after each action's own preflight | A scoped cancellation opportunity exposed by the active attack ability. Higher-priority actions remain immediate ability requests, not buffered actions. | `TODO-01C`, `TODO-01D`, `TODO-02D`, `TODO-03B` |
| Weapon-collision notify state, swept trace, team filtering, one-hit protection, and centralized hit resolution | A single GAS damage path driven by semantic attack timing and a validated hit resolver. Animation timing never directly owns health or poise mutation. | `TODO-01A`, `TODO-02B` |
| Sprint attack, hold-to-charge, release attack, stamina exhaustion, and interruption cleanup | Separate player abilities with explicit input priority and effect/tag lifecycle; do not fold them into the linear combo asset. | `TODO-01C`, `TODO-01E` |
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
  - Added the public GAS module dependencies, established character-owned ASC and AttributeSet initialization on possession, and registered the nine config-first project Gameplay Tags.
  - The user compiled `PolyQuestEditor`, verified the active `BP_Player`/`BP_PlayerController` route in PIE, confirmed the four attributes through `ShowDebug AbilitySystem`, and validated movement, look, and jump. No ability, effect, cue, replication, or combat behavior was added.

## Milestones

### Player Combat Vertical Slice

- [ ] `TODO-01A: Player Ability Lifecycle And One-Hit Attack v1`
  - Prove `Input -> GameplayAbility -> CommitAbility -> Montage/semantic Notify -> validated hit path -> GameplayEffect -> EndAbility` with temporary template assets.
  - Establish one explicit cleanup route for natural finish, interruption, invalid notify, and teardown. Do not introduce combo, charge, sprint, or production asset assumptions yet.

- [ ] `TODO-00C: Template Cleanup v1`
  - Run after `TODO-01A` has completed its temporary-template compile/PIE proof, so the first ability does not lose its validation fixtures.
  - Use Editor Reference Viewer/Asset Registry evidence to retire the old `ThirdPerson` map and Blueprint parent chain, then remove only closed sets of template source/assets with no remaining references.
  - Remove root template classes only after `BP_Player`, `BP_GameMode`, and `BP_PlayerController` no longer depend on them; remove `Variant_Combat`, `Variant_Platforming`, and `Variant_SideScrolling` source only together with their retired assets and Build.cs include paths.
  - Keep `ABaseCharacter`, `APlayerCharacter`, `UCharacterAttributeSet`, project GAS config, and product-owned Blueprints. Use a focused cleanup commit and a fresh compile/PIE startup check.

- [ ] `TODO-01B: First Stylized Player Asset Integration v1`
  - Select and integrate one approved Polygon player/weapon set from `Content/Assets/`, then validate Skeleton, sockets, AnimBP, Montage, root motion, and the existing light-attack Ability path.
  - Replace no broader asset set until this compatibility slice passes PIE.

- [ ] `TODO-01C: Dodge, Stamina, And Action Interruption v1`
  - Establish tag/effect-based action blocking, cancellation, stamina cost, deliberate overdraft/exhaustion policy, dodge direction, invulnerability timing, and teardown.
  - The first action-cancellation contract must work before attacks expose a recovery cancel window.

- [ ] `TODO-01D: Data-Driven Combo, Branch Window, And Recovery Cancel v1`
  - Rebuild the proven linear per-entry combo design: one complete authored `Entry -> Recovery -> End` Montage per chain entry and one focused combo DataAsset per weapon/ability set.
  - Preserve one-LMB buffering, early/late BranchWindow continuation, recovery CancelWindow behavior, cross-Montage stale-callback safety, and failure-safe cleanup through the active light-attack ability.
  - Do not create a branching combo graph, a universal input buffer, disconnected wind-up/strike/recovery assets, or a parallel action FSM.

- [ ] `TODO-01E: Charged And Sprint Attack v1`
  - Add hold-to-charge/release and sprint-attack abilities after the basic attack, stamina, cancellation, and authored combo boundaries are stable.
  - Keep the proven intent order explicit: an eligible combo continuation owns LMB first; otherwise sprint and hold/charge rules decide the new attack. These attacks are not extra combo entries by default.

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

- [ ] `TODO-07B: Full Route Regression v1`
  - Establish the repeatable manual validation matrix for New Game, Continue, checkpoint/rest, ordinary enemy reset, cleared encounter restore, equipment and consumables, drops/fixed rewards, player/enemy ranged combat, punish abilities, Boss completion, and completion Continue.
  - Fix regressions in previously completed systems only; this stage does not defer their original implementation or add new gameplay systems.

## Accepted Technical Direction

- **Test migration boundary:** migrate verified player-facing behavior and acceptance cases, not the old FSM, `EActionState`, save identifiers, or authored asset topology.
- **Gameplay authority:** GAS owns ability activation, costs, blocking/cancellation tags, combat state, damage, poise, hit reaction, and death. Animation owns timing and presentation; it is not a second gameplay state source.
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

- The player GAS foundation is complete; the next validation boundary is the first real ability in `TODO-01A`, which must introduce its own grant, activation, cost, timing, hit, and cleanup contracts without expanding TODO-00B.
- The generated template contains multiple sample gameplay variants. Extending them directly would blur sample behavior with PolyQuest product behavior; `TODO-00A` records them as reference-only until explicit retirement.

## Stage Completion Standard

A roadmap stage is complete only after its approved plan, focused source/asset validation, user-owned compilation/PIE evidence where applicable, review, documentation synchronization, and explicit commit approval are complete.
