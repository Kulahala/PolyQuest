# PolyQuest Roadmap

## Purpose And Scope

PolyQuest is a UE 5.8 C++ GAS-first, single-player stylized action RPG. It rebuilds the proven player-facing contracts from the UE 5.7 `Test` project, rather than performing an in-place copy of its FSM implementation, assets, or save data.

The previous `Test` project remains the FSM behavior reference and validation baseline. PolyQuest reuses proven gameplay contracts after deliberate GAS redesign, but does not retain its C++ state machine, save schema, authored assets, or Marketplace content wholesale.

## Current State

- UE 5.8 Third Person C++ template created.
- Official Unreal MCP and VibeUE-enhanced editor access verified read-only.
- `GameplayAbilities` is available in the editor; game-module GAS dependencies and product GAS code do not exist yet.
- External resource packs are held as raw, untracked source packages under `Content/Assets/`; none has been imported, selected as the production Skeleton baseline, or integrated into PolyQuest gameplay.

## Done Milestones

- [x] `TODO-00A: Repository And Documentation Bootstrap v1`
  - Established the UE 5.8 template baseline, Git LFS, generated-output ignores, project documentation, and verified official Unreal MCP/VibeUE read routes.
  - Committed as `f64fbd2`; no GAS gameplay code, production asset integration, or template retirement was included.

## Milestones

### Foundation

- [ ] `TODO-00B: GAS Core Contract v1`
  - Add required game-module GAS dependencies.
  - Decide and implement Player ASC ownership, initial AttributeSet lifecycle, and a minimal config-first Gameplay Tag taxonomy; promote native tag constants only where the C++ contract needs them.
  - Use user-led learning mode: the user authors the initial tags and designated GAS exercises with stepwise guidance and focused read-only checks.
  - Prove editor/build integration before gameplay abilities.

### Player Combat Vertical Slice

- [ ] `TODO-01A: Player Ability Lifecycle And Light Attack v1`
  - Prove `Input -> GameplayAbility -> CommitAbility -> Montage/Notify -> damage path -> EndAbility` with temporary template assets.
  - Recreate only the relevant Test behavior contract; do not port the Test action FSM or combo implementation.

- [ ] `TODO-01B: First Stylized Player Asset Integration v1`
  - Select and integrate one approved Polygon player/weapon set from `Content/Assets/`, then validate Skeleton, sockets, AnimBP, Montage, root motion, and the existing light-attack Ability path.
  - Replace no broader asset set until this compatibility slice passes PIE.

- [ ] `TODO-01C: Dodge, Interruption, And Stamina v1`
  - Establish tag/effect-based action blocking, cancellation, stamina cost, exhaustion, and teardown.

### Combat And World Expansion

- [ ] `TODO-02A: Enemy GAS Combat And Stance Break v1`
  - Add the first enemy ASC, damage/poise flow, a narrow stance-break contract, and StateTree-driven high-level enemy intent.

- [ ] `TODO-02B: Defensive Combat v1`
  - Add block/parry only after damage, interruption, and enemy state contracts are stable.

- [ ] `TODO-03A: Equipment, Pickup, And Checkpoint v1`
  - Rebuild only the player-loop data that the new game actually needs; do not migrate old SaveGame identifiers by default.

- [ ] `TODO-04A: Encounter, Fog Gate, And One-Time Rewards v1`
  - Build a new encounter persistence loop after the new save/checkpoint boundary is verified.

- [ ] `TODO-05A: Critical And Backstab v1`
  - Rebuild Front Critical and Backstab as GAS abilities after normal attack, stance break, reservation, animation, and teardown paths are proven.

- [ ] `TODO-06A: First Stylized Level And Boss Route v1`
  - Integrate the validated systems into a deliberate level route and boss encounter.

## Accepted Technical Direction

- **Test migration boundary:** migrate verified player-facing behavior and acceptance cases, not the old FSM, `EActionState`, save identifiers, or authored asset topology.
- **Gameplay authority:** GAS owns ability activation, costs, blocking/cancellation tags, combat state, damage, poise, hit reaction, and death. Animation owns timing and presentation; it is not a second gameplay state source.
- **Enemy AI:** StateTree is the default high-level behavior brain for both ordinary enemies and Bosses. It owns patrol, alert, chase, combat intent, return-home, phase selection, and high-level transitions; it requests GAS abilities rather than directly applying combat mutations.
- **Bosses:** Bosses use a separate StateTree and combat data profile, not a different AI framework merely because they do not patrol. Encounter, fog-gate, persistence, and rewards remain outside the Boss StateTree.
- **No parallel FSM:** do not retain an enemy HFSM or `EEnemyState` as a competing runtime truth beside StateTree and GAS tags.
- **Behavior Tree escalation gate:** do not introduce Behavior Trees unless concrete production behavior requires deeply nested reactive arbitration, dynamic subtrees, or broad concurrent services that remain unclear and untestable in StateTree.
- **External resources:** `Content/Assets/` is a raw source reservoir. A resource becomes an imported product integration only when an approved stage names it and validates its exact Skeleton, socket, animation, or gameplay surface.

## Recommendations

- **Template retirement:** retain generated template variants until PolyQuest owns an equivalent technical slice. Delete unused variants only after the replacement opens and plays correctly in PIE, with a focused source/asset commit.
- **Behavior Trees:** keep Behavior Tree tooling available but unadopted. Re-evaluate it only at the documented escalation gate; do not split ordinary enemies and Bosses across AI frameworks preemptively.
- **Motion Warping:** add product-level Motion Warping only when the selected stylized animation set contains a concrete root-motion alignment requirement.

## Known Risks And Validation Debt

- The live editor exposes `GameplayAbilities`, while `PolyQuest.Build.cs` currently lacks the GAS runtime modules. `TODO-00B` owns the first C++ dependency and compile verification.
- The generated template contains multiple sample gameplay variants. Extending them directly would blur sample behavior with PolyQuest product behavior; `TODO-00A` records them as reference-only until explicit retirement.

## Stage Completion Standard

A roadmap stage is complete only after its approved plan, focused source/asset validation, user-owned compilation/PIE evidence where applicable, review, documentation synchronization, and explicit commit approval are complete.
