# PolyQuest Roadmap

## Purpose And Scope

PolyQuest is a UE 5.8 C++ GAS-first, single-player stylized action RPG. It rebuilds the validated player-facing contracts of the UE 5.7 Test project through a new GAS/StateTree/runtime boundary; the old FSM, save schema, authored assets, and Marketplace content are reference evidence only.

This file is the active route: it records the durable dependency order, open milestones, adoption conditions, canonical validation-debt triggers, and a compact pointer to the current/next stage. Detailed stage baselines, worktree snapshots, validation receipts, and closeout detail live in plan.md. Historical stage closeouts are preserved in ROADMAP-archive.md and are not current behavior authority. Document roles and archive discipline are defined in AGENTS.md.

## Current State

> The TODO-03A7 parent-baseline wording below is a retained historical snapshot from that task's closeout. It is not the template for future stages; future detailed baselines belong in `plan.md`.

- This stage closeout is based on parent `main @ 90ad381` (the committed `TODO-07B4` stage); `TODO-03A7` is implemented and user-PIE-confirmed in the approved stage change, but has no separate manual `PolyQuestEditor` compile or Editor-readback record. Lock-On acquisition/cycle remain strict with fixed per-axis `15%` retention for an already-owned target, while Bow keeps its independent `6%` automatic Target Assist boundary.
- The product route is `/Game/Maps/Scene01` with a fixed elevated oblique camera and a desktop Controller/input boundary. `TODO-03A7B` has an implementation closeout with user-confirmed focused Automation and Scene01 PIE; no separate manual `PolyQuestEditor` compile or Motion-Warp Editor-readback record is available, so its authored-baseline validation debt remains open. `TODO-03A7C` now has a source implementation closeout for Charged release and Sprint Attack Motion-Warp adoption with the same user-confirmed focused Automation and Scene01 PIE gates; its authored GA/Montage/Notify/Root Motion readback and separate manual compile record remain open. The next planning pointer is `TODO-03A7D`; detailed stage records remain in `plan.md`.
- Mutable GameplayAbility/GameplayEffect, Montage, AnimBP, Blueprint, input, DataAsset, map, Niagara, sound, and imported Content remain user-owned local WIP. Manual compilation, Editor readback, PIE/visual checks, imported-asset decisions, packaging, and final commit approval remain separate gates.

## Current Player-Combat Sequence

Before introducing the first ranged enemy, the player-combat route is:

Implemented/PIE-confirmed: TODO-03A3E → TODO-07B4 → TODO-03A7 (entry 0 only)
Current implementation closeouts: TODO-03A7B (Light entry 0..2 lifecycle adoption) → TODO-03A7C (Charged/Sprint source adoption; authored compile/readback debt remains)
Next player-combat slices: TODO-03A7D → TODO-05A → TODO-05B → TODO-07B5 → TODO-07B6 → TODO-03C
Later optional enemy adoption: TODO-03A7E (after the first ranged-enemy gate)

`TODO-03A7` remains deliberately limited to the selected Light entry 0; `TODO-03A7B` owns the bounded Light entry 0..2 implementation and lifecycle bridge, while authored compile/readback closure remains a separate validation debt. `TODO-03A7C` owns the bounded Charged release and Sprint Attack source lifecycles, reusing the narrow evaluator/Player bridge without creating a global target-follow service; its authored opt-in/readback debt remains separate. Later Player adoption slices reuse these narrow primitives only after their own Montage and Root Motion evidence. TODO-07B4 is a deliberately early presentation gate and is technically independent of Motion Warping. The punish dependency remains `TODO-05A → TODO-05B`; the Small Reaction retrigger remains an explicit `TODO-07B5` gate because it changes GAS re-entry and Montage cleanup semantics for both Player and Enemy. `TODO-03A4B` is conditional equipment work and is not a prerequisite unless a partial-Guard weapon is intentionally authored.

## Route Constraints

Stable ownership and data-flow contracts live in `ARCHITECTURE.md`. Future stages preserve native C++/GAS authority and the existing melee/projectile delivery paths; no parallel FSM, generic action dispatcher, or second damage path is introduced.

StateTree/Controller/GAS boundaries remain explicit: StateTree selects intent, Controllers own AI target/focus/navigation state, and Abilities/GameplayEffects execute combat. Motion Warping is attack-local and optional; unsuitable authored assets may close with an evidence-backed no-adoption decision.

Player combat and the first ranged enemy share only the narrow projectile runtime/data contract. Persistence, inventory/rewards, death reload, and multiplayer remain separate product boundaries.

For older completed stages, validation evidence, and exact historical wording, see ROADMAP-archive.md and the Git history. Do not infer current behavior from archived future TODO text.

## Active Milestones

### Player Combat Closure

- The source implementation closeouts for `TODO-03A7B` (Light entry `0..2`) and `TODO-03A7C` (Charged release/Sprint Attack) are recorded in `plan.md` and `ROADMAP-archive.md`; their authored Montage/Notify/Root Motion readback and separate manual compile records remain non-blocking validation debt. The next open slice is `TODO-03A7D`.

- [ ] TODO-03A7D: Melee Skill Motion-Warp Adoption v1
  - Audit `UPlayerMeleeSkillAbility` and its actual authored skills one at a time. Each skill must prove its own target snapshot, Montage Notify/Root Motion compatibility, range/angle policy, and all interruption/death/teardown cleanup before adoption.
  - No generic skill dispatcher, automatic retarget, locomotion warp, or change to the existing skill damage route.

### Punish

- [ ] TODO-05A: Stagger Front Execution v1
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
  - Add one StateTree-driven ranged enemy only after TODO-03A3E, TODO-07B4, TODO-03A7, TODO-03A7B, TODO-03A7C, TODO-03A7D, TODO-05A, TODO-05B, TODO-07B5, and TODO-07B6 each pass their own adoption, compile, Automation, Editor/PIE, and review gates (or a Motion-Warp slice closes with an evidence-backed no-adoption decision).
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
  - TODO-07B1/07B1A, TODO-07B2, TODO-07B3, and TODO-07B4 are delivered. TODO-07B5 and TODO-07B6 are the remaining explicitly scoped children before the first ranged enemy; the umbrella owns focused presentation work, not new combat ownership.
  - Re-author final Montage composition and Notify timing only after the selected animation set is stable. Reuse the proven TODO-02F rate lifecycle; do not add overlapping playback-rate systems or use Motion Warping for locomotion.

- [ ] TODO-07B5: Small Hit Reaction Retrigger v1
  - This is the first of the two pre-03C feedback stages; its small code surface does not waive an independent GAS/Montage validation gate.
  - Allow the existing Player and Enemy Small Hit Reaction Abilities to retrigger the same `InstancedPerActor` ability so a new Small hit stops/resets the currently playing Small Montage and starts the latest selected directional Montage.
  - Enable only the engine's `bRetriggerInstancedAbility` behavior and remove only the Small ability's self-block on `State.Action.SmallHitReacting`. Retain that tag as the active-state owner, retain the Dead/Stunned activation blocks, and preserve the existing montage-task, delegate, identity, interruption, death, and teardown cleanup.
  - Keep Big/Launch reactions, movement/action arbitration, damage/Poise delivery, StateTree intent, Gameplay Tags, Config, and authored assets unchanged. This is a bounded reaction-lifecycle adjustment, not a generic retrigger framework or a cross-tier interruption rule.
  - Close only after focused Player/Enemy repeated-Small-hit Automation coverage and user-owned `PolyQuestEditor` compile plus Scene01 PIE evidence show the successor Montage owns the state without a stale prior completion ending it. Regressions for Big/Launch, Stunned, Dead, cancellation, and teardown remain in the same gate.

- [ ] TODO-07B6: Combat Feedback DataAsset Consolidation v1
  - After the current feedback behavior and `TODO-07B5` are stable, consolidate authored feedback references/tuning (camera shake, hit-stop, sound, blood, and overlay where applicable) into a deliberately scoped Combat Feedback DataAsset while preserving each existing runtime owner's authority and lifecycle.
  - Player remains the owner of its local `PlayerCameraManager` Shake lifecycle, Controller remains the owner of global hit-stop, Enemy remains the owner of impact sound/blood presentation, and `ABaseCharacter` remains the owner of the shared overlay lifecycle. The DataAsset supplies authored data; it does not become a second gameplay or damage authority.
  - The future plan must freeze the exact fields, asset paths, fallback behavior, and migration/readback boundary before implementation. Do not introduce GameplayCue, a generic feedback dispatcher, a second damage path, network policy, or incidental asset migration.
  - Close only with old/new field-equivalence Automation, explicit missing-data fail-closed coverage, curated Editor asset readback, user-owned compile, and focused Scene01 PIE evidence for Player hit, attacker impact, Parry/Guard, Enemy impact, and teardown cleanup.

## Recommendations

- Keep the current fixed 15% Lock-On retention and Bow 6% Target Assist values stable until a focused stage presents new evidence.
- Keep TODO-03A4B conditional; do not build partial Guard or generic mitigation before a real authored weapon requires it.
- Treat a missing or unsuitable Motion-Warp asset as a valid evidence-backed no-adoption outcome.
- Keep Behavior Trees unadopted unless StateTree fails a concrete production requirement.
- Use one shared interaction candidate snapshot for prompt and pickup; do not add a generic interactable framework for TODO-03A3E.

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

## Deferred TODOs

- Dedicated Sprint Loop presentation remains deferred until the selected locomotion set and MoveSpeed cadence are stable; validate foot sliding, Root Motion isolation, and transitions in focused PIE.

## Stage Completion Standard

A roadmap stage is complete only after its approved plan, focused source/asset validation, user-owned compilation/PIE evidence where applicable, review, documentation synchronization, and explicit commit approval are complete.
