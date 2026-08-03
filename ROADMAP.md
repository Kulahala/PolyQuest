# PolyQuest Roadmap

## Purpose And Scope

PolyQuest is a UE 5.8 C++ GAS-first, single-player stylized action RPG. It is a new implementation, not an in-place conversion of the UE 5.7 FSM project.

The previous `Test` project remains a separate FSM reference and validation baseline. PolyQuest may reuse proven gameplay contracts after deliberate redesign, but it does not copy its C++ state machine, save schema, authored assets, or Marketplace content wholesale.

## Current State

- UE 5.8 Third Person C++ template created.
- Official Unreal MCP and VibeUE-enhanced editor access verified read-only.
- `GameplayAbilities` is available in the editor; game-module GAS dependencies and product GAS code do not exist yet.
- New stylized Polygon/Fab assets have not been imported or selected as the production Skeleton baseline.

## Milestones

### Foundation

- [ ] `TODO-00A: Repository And Documentation Bootstrap v1`
  - Establish Git/LFS, project documentation, editor-tool routing, and a clean template baseline.
  - No gameplay code, asset migration, or GAS module integration.

- [ ] `TODO-00B: GAS Core Contract v1`
  - Add required game-module GAS dependencies.
  - Decide and implement Player ASC ownership, initial AttributeSet lifecycle, and a minimal native gameplay-tag taxonomy.
  - Prove editor/build integration before gameplay abilities.

### Player Combat Vertical Slice

- [ ] `TODO-01A: Player Ability Lifecycle And Light Attack v1`
  - Prove `Input -> GameplayAbility -> CommitAbility -> Montage/Notify -> damage path -> EndAbility` with temporary template assets.
  - Do not port the Test action FSM or combo system.

- [ ] `TODO-01B: First Stylized Player Asset Integration v1`
  - Import one approved Polygon player/weapon set and validate Skeleton, sockets, AnimBP, Montage, root motion, and the existing light-attack Ability path.
  - Replace no broader asset set until this compatibility slice passes PIE.

- [ ] `TODO-01C: Dodge, Interruption, And Stamina v1`
  - Establish tag/effect-based action blocking, cancellation, stamina cost, exhaustion, and teardown.

### Combat And World Expansion

- [ ] `TODO-02A: Enemy GAS Combat And Stance Break v1`
  - Add the first enemy ASC, damage/poise flow, and a narrow stance-break contract.

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

## Recommendations

- **Template retirement:** retain generated template variants until PolyQuest owns an equivalent technical slice. Delete unused variants only after the replacement opens and plays correctly in PIE, with a focused source/asset commit.
- **StateTree:** retain the enabled UE 5.8 StateTree tooling as an evaluated option, not a requirement. Adopt it for enemy AI only after the first enemy behavior needs reusable authored decision flow.
- **Motion Warping:** add product-level Motion Warping only when the selected stylized animation set contains a concrete root-motion alignment requirement.

## Known Risks And Validation Debt

- The live editor exposes `GameplayAbilities`, while `PolyQuest.Build.cs` currently lacks the GAS runtime modules. `TODO-00B` owns the first C++ dependency and compile verification.
- The generated template contains multiple sample gameplay variants. Extending them directly would blur sample behavior with PolyQuest product behavior; `TODO-00A` records them as reference-only until explicit retirement.

## Stage Completion Standard

A roadmap stage is complete only after its approved plan, focused source/asset validation, user-owned compilation/PIE evidence where applicable, review, documentation synchronization, and explicit commit approval are complete.
