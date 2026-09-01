# PolyQuest Roadmap

## Purpose And Scope

PolyQuest is a UE 5.8 C++ GAS-first, single-player stylized action RPG. It rebuilds the validated player-facing contracts of the UE 5.7 Test project through a new GAS/StateTree/runtime boundary; the old FSM, save schema, authored assets, and Marketplace content are reference evidence only.

This file is the active route: it records the durable dependency order, open milestones, adoption conditions, canonical validation-debt triggers, and a compact pointer to the current/next stage. Detailed stage baselines, worktree snapshots, validation receipts, and closeout detail live in plan.md. Historical stage closeouts are preserved in ROADMAP-archive.md and are not current behavior authority. Document roles and archive discipline are defined in AGENTS.md.

## Current State

> The TODO-03A7 parent-baseline wording below is a retained historical snapshot from that task's closeout. It is not the template for future stages; future detailed baselines belong in `plan.md`.

- This stage closeout is based on parent `main @ 90ad381` (the committed `TODO-07B4` stage); `TODO-03A7` is implemented and user-PIE-confirmed in the approved stage change, but has no separate manual `PolyQuestEditor` compile or Editor-readback record. Lock-On acquisition/cycle remain strict with fixed per-axis `15%` retention for an already-owned target, while Bow keeps its independent `6%` automatic Target Assist boundary.
- The product route is `/Game/Maps/Scene01` with a fixed elevated oblique camera and a desktop Controller/input boundary. `TODO-03A7B/C/D/F`, `TODO-03H5`, `TODO-03I1/I2`, `TODO-07B5/B6/B7`, and `TODO-03I4` have implementation closeouts recorded in `ROADMAP-archive.md`; their remaining authored/readback/compile debts are tracked only where still applicable in Known Risks. `TODO-03I4` has removed the zero-reference legacy Loadout C++/test surface, while `TODO-07B7` has split the feedback profile by role. Direct weapon fields, Defense Profile, prepared handles, the equipment transaction, and typed feedback profiles are now the relevant contracts. The next open planning slice is `TODO-05A`; detailed stage records remain in `plan.md`.
- Mutable GameplayAbility/GameplayEffect, Montage, AnimBP, Blueprint, input, DataAsset, map, Niagara, sound, and imported Content remain user-owned local WIP. Manual compilation, Editor readback, PIE/visual checks, imported-asset decisions, packaging, and final commit approval remain separate gates.

## Current Player-Combat Sequence

Before introducing the first ranged enemy, the player-combat route is:

Implemented/PIE-confirmed: TODO-03A3E → TODO-07B4 → TODO-03A7 (entry 0 only)
Current implementation closeouts: TODO-03A7B (Light entry 0..2 lifecycle adoption) → TODO-03A7C (Charged/Sprint source adoption; authored compile/readback debt remains) → TODO-03A7D (Melee Skill source adoption; Whirlwind authored readback debt remains) → TODO-03A7F (explicit trigger-range contract; authored migration/readback debt remains) → TODO-03H5 (Review-Only health audit; no P0-P2 blocker) → TODO-03I1 (Primary/Sprint direct route) → TODO-03I2 (cross-weapon cancellation/tag taxonomy; Dodge E2E fixture debt remains) → TODO-07B5 (Small Hit Reaction retrigger; real ASC cross-frame fixture debt remains) → TODO-07B6 (initial Combat Feedback DataAsset consolidation) → TODO-03I4 (Legacy Combat Loadout compatibility removal) → TODO-07B7 (typed Player/Enemy feedback profiles; profile-null coverage closed)
Next player-combat slices: TODO-05A → TODO-05B → TODO-03C
Conditional future player-ranged contract: TODO-03I3 (only when a concrete Staff/Mage player route is accepted; not a hard prerequisite for TODO-05A/B or TODO-03C)
Later optional enemy adoption: TODO-03A7E (after the first ranged-enemy gate)

`TODO-03A7` remains deliberately limited to the selected Light entry 0; `TODO-03A7B/C/D/F` preserve their recorded authored-adoption/readback debts. `TODO-03H5` found no P0-P2 blocker in the bounded Motion-Warp health audit. `TODO-03I1` makes `UWeaponDefinition::PrimaryAttackAbilityTag` and optional `SprintAttackAbilityTag` the canonical MainHand Primary/Sprint route; `TODO-03I4` has now removed the former `AssociatedLoadout`/`ActiveCombatLoadout` C++ and test compatibility surface after the user-confirmed zero-reference asset precondition. `TODO-03I2` keeps cancellation and teardown selectors orthogonal from authored skill identity, and its Dodge E2E fixture debt remains tracked below. The post-I2 player-combat route is `TODO-07B5 → TODO-07B6 → TODO-03I4 → TODO-07B7 → TODO-05A → TODO-05B`; the first four are closed, and `TODO-05A` is the next open implementation slice. `TODO-03A4B` remains conditional equipment work, not a prerequisite unless a partial-Guard weapon is intentionally authored.
`TODO-03I3` is deliberately conditional: it defines the future Staff/Mage targeting and delivery contract only after a concrete authored player route and player-value question exist. It does not retroactively classify the current Bow as a generic ranged template, and it does not add a hard dependency to punish or the first ranged enemy.

## Route Constraints

Stable ownership and data-flow contracts live in `ARCHITECTURE.md`. Future stages preserve native C++/GAS authority and the existing melee/projectile delivery paths; no parallel FSM, generic action dispatcher, or second damage path is introduced.

StateTree/Controller/GAS boundaries remain explicit: StateTree selects intent, Controllers own AI target/focus/navigation state, and Abilities/GameplayEffects execute combat. Motion Warping is attack-local and optional; unsuitable authored assets may close with an evidence-backed no-adoption decision.

The current runtime has concrete weapon-family paths rather than a universal Melee/Ranged layer: melee uses contact trace/damage delivery, while Bow owns its Draw/Hold/Release and projectile/limited-target-assist path. `Input.Aim` and Bow phase events remain Bow-specific until a concrete route proves semantic equivalence; future Staff/Mage work must use the orthogonal family/delivery/target dimensions recorded in `TODO-03I3`.

Player combat and the first ranged enemy share only the narrow projectile runtime/data contract. Persistence, inventory/rewards, death reload, and multiplayer remain separate product boundaries.

For older completed stages, validation evidence, and exact historical wording, see ROADMAP-archive.md and the Git history. Do not infer current behavior from archived future TODO text.

## Active Milestones

### Player Combat Closure

- The source implementation closeouts for `TODO-03A7B` (Light entry `0..2`), `TODO-03A7C` (Charged release/Sprint Attack), and `TODO-03A7D` (Melee Skill) are recorded in `plan.md` and `ROADMAP-archive.md`; their authored Montage/Notify/Root Motion readback and separate manual compile records remain non-blocking validation debt. `TODO-03A7F` has closed the shared trigger-range source contract, and `TODO-03H5` has completed its Review-Only health audit with no P0-P2 blocker or Source change. `TODO-03I1` has closed the direct route and `TODO-03I4` has removed the zero-reference Loadout compatibility surface from Source/test; the direct route, Defense Profile, prepared handles, and equipment transaction remain the runtime contract. `TODO-03I2` has closed the bounded cross-weapon Tag taxonomy migration; its Dodge end-to-end fixture and independent compile/readback debts remain tracked below. `TODO-07B5` is closed at the source/PIE gate; its real ASC cross-frame retrigger fixture debt is tracked below. `TODO-07B6` established the initial feedback DataAsset consolidation, and `TODO-07B7` completed the typed Player/Enemy profile split, profile-null coverage, user-confirmed final compile, focused Automation, and Scene01 PIE. The next open planning slice is `TODO-05A`.

### Runtime Health And Integration

- `TODO-03H5` is closed as a Review-Only health gate: the four Player Motion-Warp consumers, shared evaluator/bridge, static snapshots, `ReadyForActivation()` re-entry, teardown, UnPossess, ASC/Tag/Input boundaries, and the unchanged `Trace -> Resolver -> Damage GE` path were audited with no P0-P2 blocker and no Source change. The A7F 3.17 exact-stop limitation remains a non-blocking P3; authored readback, independent manual compile, and real cross-frame Task evidence remain validation debt.

### Combat Input And Ability Routing

- `TODO-03I1` establishes MainHand `PrimaryAttackAbilityTag` and optional `SprintAttackAbilityTag` as the canonical Primary/Sprint route; Guard/Parry resolve through the Effective Defense Profile and prepared `1-4` use exact handles. `TODO-03I4` has now removed `UCombatLoadoutDefinition`, `AssociatedLoadout`, Player mirrors, and test compatibility assertions after the user-confirmed zero-reference asset precondition. No Loadout route participates in runtime input or equipment transactions. The detailed closure evidence is in `plan.md` and `ROADMAP-archive.md`.

- `TODO-03I2` is closed as the bounded cross-weapon Tag taxonomy migration. Four orthogonal capability/teardown tags now separate Dodge, Defense, Reaction, and UnPossess cancellation semantics; concrete `Ability.Skill.Whirlwind` identity remains authored independently; the legacy `Ability.Skill.Melee` tag is no longer registered. The stage preserved physical inputs, Bow/Charged phase events, Motion-Warp, Trace/Resolver/Damage, and GAS ownership. User-confirmed Automation and Scene01 PIE passed; the headless Dodge end-to-end fixture and independent compile/readback receipts remain validation debt. The next open implementation slice is `TODO-05A`.

### Combat Feedback Authoring

- `TODO-07B7` is delivered as the role-specific taxonomy closure for the initial TODO-07B6 consolidation: the abstract Base profile retains Overlay only, while typed Player/Enemy profiles expose only their own feedback fields. User-confirmed typed-asset migration, zero-reference legacy-asset deletion, final compile, focused Automation, and Scene01 PIE passed. The profile-null/diagnostic debt is closed; product `.uasset` changes remain user-owned WIP outside this source/document commit. The next open implementation slice is `TODO-05A`.

- [ ] TODO-03I3: Player Ranged Targeting And Delivery Contract v1 (conditional)
  - Start only when a concrete Staff/Mage player ability, authored asset route, and player-facing targeting question are accepted. This is a contract/design gate, not permission to invent a Mage implementation or to turn the current Bow path into a universal template.
  - Keep four dimensions explicit and orthogonal: weapon/equipment family (Melee, Bow, Staff, OffHand), attack delivery (contact trace, actor projectile, ground/area effect, beam/line effect), target-selection mode (locked actor, fire-time snapshot with finite tracking, ground point plus authored area shape/extent, route/line, or free direction), and physical input intent (`Primary`, `Aim`, `Guard`, `Parry`, prepared slots).
  - Define the smallest selected Staff/Mage slice against the existing GAS ownership: an actor-target projectile may reuse the immutable projectile runtime and finite fire-time snapshot only where its payload and lifecycle match; a ground/area ability must own a world-space point and shape/extent snapshot, while a beam/line ability owns a world-space direction or segment snapshot. Neither may depend on Lock-On or Bow Target Assist by analogy.
  - Preserve ability-owned snapshots, explicit world/ASC/target validity, cancellation/death/destroy cleanup, and one existing damage GameplayEffect path. No continuous target following, automatic retargeting, global targeting service, or hidden cross-weapon state is introduced.
  - Success requires a reviewed delivery/target matrix for the selected route, an explicit reuse-versus-new-contract decision, focused Automation for snapshot and teardown boundaries, user-owned compile, curated Editor readback of the named assets, and Scene01 PIE for the actual targeting interaction. Unselected modes remain documented boundaries rather than speculative code.
  - No generic Melee/Ranged enum solely for naming, no Bow Draw/Hold/Release merge by analogy, no new physical input action, no broad Ability dispatcher/framework, no Motion-Warp redesign, no second projectile hierarchy, and no damage/trace ownership change.

### Punish

- [ ] TODO-05A: Stagger Front Execution v1
  - `TODO-07B5`, `TODO-07B6`, `TODO-03I4`, and `TODO-07B7` have closed their focused reaction/feedback, compatibility-cleanup, and profile-authoring gates. This is now the next implementation slice, keeping its scope on target reservation, geometry, Montage, hit, and cleanup rather than absorbing basic combat presentation or legacy removal work.
  - Replace the old broad Front Critical wording with one front execution that is legal only while the target currently owns the existing State.Status.Stunned from a real Poise-to-zero/Stance Break transition.
  - The route does not listen for Parry success. Combo/attack and Parry GameplayEffects may reduce Poise; execution is available only if the existing Stance Break path actually leaves the Enemy Stunned. A Parry that does not empty Poise grants no execution.
  - Establish only the smallest target reservation, front-geometry, one-time alignment/optional Motion-Warp handoff, authored execution Montage, one existing damage GameplayEffect, and unified interruption/death/teardown cleanup. No new Stunned tag, pending state, or generic finisher dispatcher.

- [ ] TODO-05B: Backstab v1
  - Keep Backstab as a distinct Player action for a living target that is not State.Status.Stunned and is behind the target at the authoritative activation snapshot. Rear geometry is the contract; no stealth, alert, perception, or AI framework is added.
  - Reuse only narrow reservation/alignment/hit/cleanup primitives proven by TODO-05A where that reuse is real. A valid action applies one existing damage GameplayEffect at its authored hit event and fails closed on target loss, range loss, montage failure, cancellation, death, or teardown.
  - Stagger Backstab is intentionally out of v1. If a later design needs a rear action during Stance Break, it must be a separately accepted stage with its own player-value and validation gate.

- TODO-05C is retired from the active route. Its former front-execution slice is absorbed into TODO-05A; its former Stagger Backstab slice is explicitly not adopted.

### Ranged Gate And Conditional Equipment

- [ ] TODO-03C: Ranged Enemy v1
  - Add one StateTree-driven ranged enemy only after TODO-03A3E, TODO-07B4, TODO-03A7, TODO-03A7B, TODO-03A7C, TODO-03A7D, TODO-03A7F, TODO-03H5, TODO-03I1, TODO-03I2, TODO-07B5, TODO-07B6, TODO-03I4, TODO-07B7, TODO-05A, and TODO-05B each pass their own adoption, compile, Automation, Editor/PIE, and review gates (or a Motion-Warp slice closes with an evidence-backed no-adoption decision).
  - Reuse the existing immutable 03B projectile runtime and GAS delivery path. Add only enemy ranged Profile/Ability/StateTree timing and an AI-owned target snapshot; enemy projectiles are explicitly authored straight/non-homing by default.
  - Do not create Player Lock-On, global Target Assist, a second projectile hierarchy, or a second damage path.

- [ ] TODO-03A7E: Enemy Melee Motion-Warp Adoption v1 (post-ranged, optional)
  - Revisit `UEnemyMeleeAbility` only after the first ranged-enemy gate and a concrete enemy authored need. Keep AI/Controller/StateTree intent separate from the Ability-owned one-shot target snapshot and Root Motion lifecycle.
  - Reuse the Player evaluator only if the geometry and ownership contract is demonstrably identical; otherwise define a bounded Enemy-specific evaluator. No Player component sharing, continuous chase, AI retarget loop, or change to Enemy Trace/Resolver/Damage ownership.
  - This stage is not a prerequisite for `TODO-03C`; it requires its own authored Montage/Notify/Root Motion, Automation, compile/readback, PIE, and teardown evidence.

- [ ] TODO-03A4B: Weapon Defense Outcome Profiles v1 (conditional)
  - Before intentionally authoring a weapon with partial Guard damage, define one explicit no-defense/full-absorb/bounded-partial outcome while keeping Parry distinct and the existing Guard/Resolver/Damage GE ownership.
  - It is not a prerequisite for the current player-combat sequence while all authored Guards remain full absorb.

### Player Loop And Persistence

- [ ] TODO-03D: Checkpoint, Rest, Death Reload, And Save Foundation v1
  - Establish new PolyQuest checkpoint/rest, death reload, stable Save IDs, one transaction owner, failed-save behavior, and a terminal Player-Ability cancellation route through each Ability's EndAbility cleanup.
  - Do not migrate old SaveGame identifiers, raw actor references, or old world names.

- [ ] TODO-03D1: Rest-Site Combat Action Preparation And Loadout Persistence v1
  - At a save/checkpoint interaction, arrange compatible MainHand/OffHand ability classes or Bow projectile definitions into 1-4. UWeaponEquipmentComponent remains the runtime owner; SaveGame stores only a validated serializable selection snapshot.
  - This is focused preparation UI/persistence, not a backpack, skill tree, crafting, or mid-combat loadout system.

- [ ] TODO-03E: Potion And Consumable Loop v1
  - Add the first healing consumable after ownership, save, interruption, and cancellation contracts exist; retain notify-timed healing only where the chosen animation needs it.

- [ ] TODO-03F: World Drops, Fixed Rewards, And Reward Persistence v1
  - Separate transient currency drops from persistent fixed rewards and one-time elite defeat state. Each durable reward needs a stable authored ID, one transaction owner, failed-save behavior, and a reload fixture.

- [ ] TODO-04A: Encounter, Fog Gate, And Clear Persistence v1
  - Build the encounter clear-persistence loop, fog boundary, and participant ownership after the save/checkpoint boundary is verified. Add multi-enemy coordination only if the first real encounter proves ordinary StateTree/GAS spacing insufficient.

- [ ] TODO-04B: Persistence And Encounter Health Review v1
  - Audit Save IDs, transaction ownership, reload/re-entry, rewards, encounter cleanup, fog-gate state, and actor/subsystem/authored-data boundaries after the pre-03C punish gate. This review fixes only confirmed data-integrity/lifecycle blockers.

### Level, Boss, And Release Regression

- [ ] TODO-06A: First Stylized Level Route And Integration v1
  - Integrate checkpoint, ordinary combat, elite/fixed-reward branch, encounter, fog gate, Boss approach, and focused route validation without adding a second reward or encounter system.

- [ ] TODO-06B: First Boss Phase And Completion v1
  - Add one Boss StateTree/combat profile, Phase 1 -> transition -> Phase 2 intent, authored abilities, completion persistence, reward handoff, and return/continue behavior.

- [ ] TODO-06C: Route Health And Lean Review v1
  - Exercise the integrated route and audit ownership, dead references, temporary fixtures, obsolete configuration/assets, interruption/death/reload teardown, and documentation drift. Any removal still requires zero-reference or replacement evidence and explicit approval.

- [ ] TODO-07C: Full Route Regression v1
  - Establish repeatable New Game, Continue, checkpoint/rest, combat, equipment, consumable, reward, ranged, punish, Boss, and completion-Continue validation. Fix regressions only; do not add a new gameplay system here.

### HUD And Presentation

- [ ] TODO-07A2: Extended HUD And Combat Observability v1
  - Add only the HUD/debug surfaces needed to make validated Poise, target, damage, and later route state legible while retaining the ASC-backed read-only ownership of TODO-07A1.

- [ ] TODO-07B: Feedback, Presentation Retune, And Demo Polish v1
- TODO-07B1/07B1A, TODO-07B2, TODO-07B3, TODO-07B4, TODO-07B5, TODO-07B6, and TODO-07B7 are delivered; `TODO-03I4` has separately removed the legacy Loadout compatibility surface. The umbrella owns focused presentation work, not new combat ownership.
  - Re-author final Montage composition and Notify timing only after the selected animation set is stable. Reuse the proven TODO-02F rate lifecycle; do not add overlapping playback-rate systems or use Motion Warping for locomotion.


## Recommendations

- Keep the current fixed 15% Lock-On retention and Bow 6% Target Assist values stable until a focused stage presents new evidence.
- Keep TODO-03A4B conditional; do not build partial Guard or generic mitigation before a real authored weapon requires it.
- Treat a missing or unsuitable Motion-Warp asset as a valid evidence-backed no-adoption outcome.
- Keep Behavior Trees unadopted unless StateTree fails a concrete production requirement.
- Use one shared interaction candidate snapshot for prompt and pickup; do not add a generic interactable framework for TODO-03A3E.
- For weapon authoring reuse after TODO-03I1, keep `UWeaponDefinition` direct fields and `BaseGrantedActions` as the runtime source of truth. With the current small asset set, duplicate a validated weapon/template asset first; consider a bounded C++/Blueprint family-default stage only when multiple new weapon families repeat the same route configuration or manual drift becomes a real cost. Any defaults must remain opt-in, preserve exact direct-route and OffHand fail-closed validation, and require Editor readback; do not add global defaults or an automatic asset migration by analogy.
- Combat Feedback authoring now uses the delivered TODO-07B7 taxonomy: an abstract Base profile for shared Overlay and typed Player/Enemy profiles for role-exclusive data. Preserve character-owned runtime lifecycles and do not add further per-enemy subclasses unless a later enemy has a genuinely different feedback contract.

## Known Risks And Validation Debt

- Authored GA/GE/Montage/AnimBP/Blueprint/input/DataAsset/map fixtures remain local WIP; source/config commits are not clean-checkout reproductions. Close only with an explicitly curated asset baseline and readback.
- Cost or state may be committed before rare Montage startup failure is known for Light, Charged, Dodge, Sprint Attack, or Parry. Close with a controlled post-commit playback-failure matrix before networking or frame-exact cost claims.
- Keyboard/mouse is the accepted input evidence; controller mappings and hardware behavior are not support claims until readback plus focused hardware validation exists.
- Player terminal Ability cancellation on death remains owned by TODO-03D; current Dead tags block new activation but do not promise cancellation of an already-active Bow or other Ability.
- The legacy BladeTraceBase/BladeTraceTip fallback remains for the current enemy fixture. Remove only after all enemy fixtures migrate to StaticMeleeWeaponDefinition and Reference Viewer proves zero references.
- Bow's fixed 6% no-lock Target Assist overscan is deliberate current tuning. A second projectile family requiring different bounds must introduce a finite validated definition field rather than a global targeting setting.
- Directional Small/Big reaction expansion remains deferred until compatible assets, a bounded selector, and focused front/back/left/right PIE evidence exist.
- Enemy Launch abnormal-Poise recovery and the Parry/Trace combined exactly-once assertion remain documented follow-ups with their existing focused closure triggers.
- The retained legacy SwordShield BlendSpace asset is authoring clutter, not a runtime path; deletion requires Reference Viewer evidence and explicit approval.
- The hierarchical Primary-tag positive fixture remains conditional until a real Ability.Attack.Primary child tag is adopted.
- TODO-03A3E's native Automation coverage primarily uses transient test seams; direct overlap/delegate depth and an explicit assertion that a stale E snapshot leaves equipment unchanged remain validation debt. Close only if a future interaction regression or dedicated test-depth stage needs those paths; this is not a blocker for TODO-07B4.
- `PolyQuest.Equipment.TransactionMatrix` now contains the corrected source asset path, but still depends on a local WIP Guard asset and has no fresh compile/readback evidence. Keep the `Content/**` asset outside this stage; close in a separately approved equipment/asset baseline with user readback.
- `Config/Automation/Presets/1.json` is not a complete test manifest. The 25/25 result recorded in the archived TODO-03A3E closeout comes from the user's manual selection of all current PolyQuest suites, not from the preset; a preset refresh is optional config maintenance, not a TODO-03A3E blocker.
- TODO-07B4 source and focused Automation are confirmed, and the user has confirmed the focused Scene01 PIE route. This closeout does not contain a separate manual `PolyQuestEditor` compile record or six-field `BP_Player` Editor readback; treat the attacker-field authoring baseline as uncurated until those exact gates are read back. The debt closes before packaging or a clean authored-fixture claim and is not a blocker for the next source-only planning slice.
- TODO-03A7 source and focused Automation/Scene01 PIE are confirmed. Its remaining debt is no separately recorded manual `PolyQuestEditor` compile or Motion-Warp asset readback; the production-entry coverage debt was addressed in TODO-03A7B. Close the authored readback before claiming a clean baseline; it is not a blocker for drafting the next player-combat plan.
- TODO-03A7B source, focused Automation, and Scene01 PIE are confirmed, but no independent manual `PolyQuestEditor` compile or entry 1/2 Motion-Warp Editor readback is recorded. Keep this as a non-blocking authored-validation debt; close when the user records the compile and the named `DA_Combo_StraightSword`/Montage/AnimBP/component readback, or record an evidence-backed no-adoption result for unsuitable entries. The current 3.14 seam does not independently construct a post-`ReadyForActivation()` synchronous task end; close that coverage gap only if a deterministic seam is available without weakening production task gates.
- TODO-03A7C source, focused Automation, and Scene01 PIE are confirmed for Charged release and Sprint Attack. No independent manual `PolyQuestEditor` compile or per-Ability GA/Montage/Notify/Modifier/Root Motion Editor readback is recorded; keep this as non-blocking authored-validation debt. Close it with the named asset readback and compile, or with real evidence-backed no-adoption for an unsuitable Ability. The existing test seam does not replace production Task/Montage timing proof.
- TODO-03A7D source, focused Automation, and an earlier Scene01 PIE confirmation are recorded for Melee Skill. The post-fix round has no separate new PIE receipt, no independent manual `PolyQuestEditor` compile record, and no `GA_Skill_Whirlwind`/Montage Editor readback; keep the source slice closed but the authored adoption gate open. Close it with the named readback/compile, or with real evidence-backed no-adoption. The existing test seam does not replace production Task/Montage timing proof.
- TODO-03A7F source, focused Automation, and Scene01 PIE are confirmed for the explicit `MinTriggerDistance`/`WarpStopDistance`/`MaxTriggerDistance` contract across the four current Player consumers. No independent user `PolyQuestEditor` compile receipt or authored field/Montage/Notify/Modifier/Root Motion readback is recorded; keep the source contract closed but the intentional field-migration/adoption gate open. Close it with the relevant Editor readback and compile, or with real evidence-backed no-adoption. The 3.17 exact-stop integration assertion has a non-blocking P3 limitation because `StartComboEntry()` clears a pre-existing target before the evaluator result is observed.
- TODO-03H5 Review-Only audit completed at `a3e0f78` with no P0-P2 blocker and no Source change. `ReadyForActivation()` re-entry, Player/Enemy invalidation, the UnPossess cancellation asymmetry, and the unique Trace/Resolver/Damage ownership were checked from static source evidence; the latter asymmetry remains a design decision for `TODO-03I1/03I2`, not an H5 defect. The 3.17 exact-stop P3 and authored readback/manual-compile/cross-frame Task evidence debts remain open with their existing closure triggers.
- TODO-03I1 establishes direct MainHand Primary/Sprint routes, while TODO-03I4 removes the former `AssociatedLoadout`/`ActiveCombatLoadout` compatibility surface after the user-confirmed zero-reference asset precondition. User-confirmed affected Automation and Scene01 PIE are recorded; no separate manual `PolyQuestEditor` compile receipt is archived for I4. Close that evidence gap when an independent compile receipt is needed; it does not block TODO-07B7.
- TODO-03I2 source closeout establishes the orthogonal `Ability.Action.CancelableBy.*` and `Ability.Action.Teardown.OnUnpossess` selectors and removes the legacy `Ability.Skill.Melee` registration. User-confirmed Automation and Scene01 PIE passed, but `Debt-03I2-DodgeSkillCancelUnitTest` remains open because headless Automation lacks a real AnimInstance/Montage cancel-window fixture; close it with a dedicated integration fixture that activates Dodge and cancels a tagged skill end to end. No independent manual `PolyQuestEditor` compile or Main Reference Viewer receipt is archived for I2.
- TODO-07B5 source closeout establishes Player/Enemy Small Hit Reaction retriggering through per-round `UAbilityTask_PlayMontageAndWait` callbacks, with old-task delegate teardown before replacement and `State.Action.SmallHitReacting` retained as an owned active-state tag rather than a self-block. User-confirmed `PolyQuest.Combat.HitReaction` Automation and Scene01 PIE passed; `Debt-07B5-RealASCRetriggerE2E` remains open because headless Automation does not prove the complete `HandleGameplayEvent -> GAS retrigger -> latest TriggerEventData -> cross-frame Montage Task` chain. Close it with a valid AnimInstance/Montage integration fixture or dedicated PIE/Automation scene; no independent manual `PolyQuestEditor` compile or Editor readback receipt is archived for 07B5.

## Deferred TODOs

- Dedicated Sprint Loop presentation remains deferred until the selected locomotion set and MoveSpeed cadence are stable; validate foot sliding, Root Motion isolation, and transitions in focused PIE.

## Stage Completion Standard

A roadmap stage is complete only after its approved plan, focused source/asset validation, user-owned compilation/PIE evidence where applicable, review, documentation synchronization, and explicit commit approval are complete.
