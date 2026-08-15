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
- `TODO-01F` has completed the native Sprint, Jump, Sprint Jump air-speed, and optional Sprint Attack lifecycle. Its authored GA/GE, Montage, retargeted Sequence, Blueprint, AnimBP, Loadout, input, and map assets remain deliberately local WIP; the dedicated Sprint locomotion loop is deferred to `TODO-07B`.
- `TODO-01G` has completed a health review of the player-combat foundation. It repaired synchronous Montage-end reentry in Light and Charged startup and aligned Dodge with active-Montage identity filtering and unified teardown.
- `TODO-01H` has completed the native shared melee-delivery foundation for Light, Charged, and Sprint Attack. The fixed `WeaponMesh` / `BladeTraceBase` / `BladeTraceTip` lookup remains a v1 fixture; mutable authored combat assets remain local WIP and are not a clean-checkout fixture.
- `TODO-02A` has completed the first enemy native AI and melee-intent foundation. The configured local Scene01 fixture has user-confirmed PIE and visual evidence; its mutable Blueprint, StateTree, Gameplay Ability/Effect, Montage, AnimBP, marker, collision, map, and imported-asset setup remains local WIP.
- `TODO-02B` has completed the fixed-world oblique Perspective camera and planar action-facing foundation. The user confirmed the focused Scene01 PIE and visual route, including the accepted position-lag response to empty-space Root Motion camera motion. Authored input, Blueprint, and Scene01 presentation assets remain local WIP.
- `TODO-02C1` has completed the first enemy's single-attack Profile, exact 2D actor-center reach, and post-attack cooldown contract. The user confirmed compile and focused Scene01 PIE; the authored DataAsset, StateTree, Blueprint, Gameplay Ability/Effect, Montage, AnimBP, and map fixture remain local WIP.
- `TODO-02C2` through `TODO-02C3B` have completed the enemy terminal, death-presentation, ragdoll, and first charged-damage hard-interrupt slices. The C3B runtime acceptance is focused reaction-animation playback with authored Root Motion held in place; `TODO-02C3C` owns the next Poise/Stance Break contract, while the related authored Content remains local WIP.
- External source packages remain under `Content/Assets/`. Imported content is not a production integration merely because it is present locally; skeleton, weapon, socket, animation, and presentation decisions still require their named stage validation.

## Test-To-PolyQuest Migration Contract Inventory

The old `Test` project is evidence for player-facing behavior, not a source tree to copy. The following contracts were implemented and manually validated there; PolyQuest will rebuild them through its own GAS, StateTree, data, asset, and save boundaries.

| Verified Test contract | PolyQuest reconstruction boundary | Owning stage |
| --- | --- | --- |
| Focused player profile plus small attack/action/reaction DataAssets | A PolyQuest character/ability manifest grants abilities and references focused authored data. It does not become a second runtime state machine. | `TODO-01A`, `TODO-01B` |
| Shared physical input intent across weapon and loadout combat styles, including press/hold/release semantics | A shared Gameplay Mapping Context emits stable input intent; GAS and the active weapon/loadout route that intent to the current melee, charged, ranged, or selected spell ability. `TODO-01C2` adds the deliberately adopted single-key Dodge/Sprint short-hold arbitration without making physical input a second combat state source. | `TODO-01C1`, `TODO-01C2` |
| One complete `Montage + EntrySection` per linear combo entry, with local damage, stamina, poise, and optional warp data | A GAS-owned linear combo-chain asset. Each entry remains a complete authored attack module; runtime continuation state remains inside the active attack ability. | `TODO-01D` |
| One LMB pre-input during `ComboWindow`; early and late continuation during `ComboBranchWindow` | The active light-attack ability owns one continuation buffer. Notify states emit semantic begin/end events only; there is no global input buffer. | `TODO-01D` |
| Recovery `CancelWindow` permits Dodge, Parry, Block, or Potion only after each action's own preflight | A scoped cancellation opportunity exposed by the active attack ability. Higher-priority actions remain immediate ability requests, not buffered actions. | `TODO-01C`, `TODO-01D`, `TODO-02D`, `TODO-03E` |
| Weapon-motion trace window, swept trace, team filtering, one-hit protection, and centralized hit resolution | A single GAS damage path driven by semantic attack timing and a validated hit resolver. A Trace Window continuously sweeps between prior/current world-space Blade Base/Tip samples; animation timing never directly owns Health or Poise mutation. | `TODO-01A`, `TODO-01H` |
| Hold-to-charge, release attack, stamina exhaustion, and interruption cleanup | A dedicated charged-attack ability consumes the existing hold/release intent after Combo continuation arbitration; do not fold it into the linear combo asset. | `TODO-01C`, `TODO-01E` |
| Sprint state and Sprint Attack | Establish and validate a real Sprint movement/input/stamina contract before a Sprint Attack may consume it. | `TODO-01F` |
| Over-shoulder lock-on, target switching, camera/facing ownership, and free-run exception | Do not migrate this Test-specific camera contract. PolyQuest uses an elevated oblique-perspective camera with camera-relative movement and an explicit planar action-facing rule; reconsider optional target assist only if focused visual validation proves it is necessary for combat readability. | `TODO-02B` |
| Enemy attack-entry DataAssets, poise/stance break, hit/death safety, melee/ranged delivery | StateTree selects high-level intent; enemy GameplayAbilities execute attacks and GameplayEffects own combat mutation. | `TODO-02A`, `TODO-02C1`, `TODO-02C2`, `TODO-02C3A`, `TODO-02C3D`, `TODO-02C3B`, `TODO-02C3C`, `TODO-03C` |
| Checkpoint, item ownership, transient Gold, fixed rewards, clear persistence, fog gate, and one-time defeat behavior | New PolyQuest persistence contracts with new stable IDs and validation fixtures. No old SaveGame schema or identifiers transfer. | `TODO-03A`, `TODO-03D` through `TODO-04A` |
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

- [x] `TODO-01F: Sprint Foundation And Sprint Attack v1`
  - Added the `MoveSpeed` Attribute, one idempotent CharacterMovement synchronization delegate, a continuous ground Sprint, Stamina-costed Jump, Sprint Jump air-speed cleanup, and Loadout-owned Sprint Attack routing. Sprint is ASC-tag truth rather than an input-layer Boolean; a Sprint Attack commits its cost before ending Sprint, plays one Root Motion Montage, consumes one identity-filtered Sprint Hit event, and permits Dodge only during its authored recovery cancel window.
  - The user confirmed `PolyQuestEditor` compilation and Scene01 PIE coverage for Sprint start/stop/cost/exhaustion/re-entry, normal and Sprint Jump behavior, Sprint Attack hit/cost/damage, and the repaired Exhausted-tag recovery path. The current BlendSpace presentation is accepted temporarily; a dedicated Sprint Loop remains a separate `TODO-07B` presentation task.
  - Main normal review repaired repeated loose `State.Status.Exhausted` additions that could otherwise leave action abilities blocked after Stamina recovered. Main adversarial fallback found no additional confirmed source blocker. The requested fresh `gpt-5.6-luna / xhigh` Reviewer could not start because the provider returned HTTP 503, so no independent-review result is claimed.
  - GA/GE, Montage, Sequence, Blueprint, AnimBP, Loadout, input, map, and imported assets remain local mutable WIP. The focused commit contains only native source, Gameplay Tag config, and documentation.
- [x] `TODO-01G: Player Combat Foundation Health Review v1`
  - Audited `TODO-01A` through `TODO-01F` input, Loadout, Gameplay Tag, Stamina, interruption, Root Motion, Montage/Notify identity, and teardown contracts without changing the provisional actor-forward Sphere Sweep delivery. It established shared weapon motion traces and hit resolution as the mandatory prerequisite before the first enemy.
  - Repaired synchronous Montage-end reentry in Light and Charged startup. Dodge now filters completion and invulnerability events by its active Montage, retains persistent filtered event listeners, and clears its effect, delegate, tasks, and Montage through `EndAbility()`.
  - The user confirmed the requested Scene01 keyboard/mouse integrated PIE regression. The requested fresh `gpt-5.6-luna / xhigh` Reviewer could not start because the provider returned HTTP 503 with no available channel, so the second pass was a Main adversarial fallback; no independent review is claimed. Unreal Editor MCP readback was unavailable in this session, so the authored fixture remains local WIP rather than independently read-back or clean-checkout evidence.
  - The committed-cost playback atomicity, charged same-frame threshold ordering, controller hardware, task-level `ExternalCancel()`, and authored-fixture reproducibility debts remain tracked under `Known Risks And Validation Debt` with closure triggers.

- [x] `TODO-01H: Weapon Motion Trace And Hit Resolver Foundation v1`
  - Replaced the three provisional actor-forward sphere sweeps with one shared Trace Window delivery path. Light, Charged, and Sprint Attack retain their own state, Cost, cancellation, attack data, and active-Montage identity checks; animation sends only Trace Window Begin/End timing.
  - Each open window continuously sweeps the prior-to-current Blade Base/Tip path on `MeleeTrace`, applies an authored GameplayEffect through the shared GAS resolver, and records a target only after successful delivery so a target can receive at most one accepted hit per window. The resolver rejects self, the same minimal combat team, missing target ASC, and `State.Status.Invulnerable`.
  - `WeaponMesh` / `BladeTraceBase` / `BladeTraceTip` are fixed v1 fixture names, not an equipment architecture. `TODO-03A` must replace them with an equipped-weapon trace-sample provider or authored weapon data while reusing the Trace Window task and shared resolver.
  - The user confirmed `PolyQuestEditor` compilation and Scene01 PIE validation. Main review plus an independent read-only source review found no P0-P2 C++/Gameplay Tag/config blocker. Blueprint, Montage, GameplayEffect, AnimBP, input, map, collision-fixture, and marker authoring remain deliberately excluded mutable WIP; this native shared-hit foundation is not a clean-checkout reproduction of the complete combat fixture.

- [x] `TODO-02A: First Enemy GAS Combat And StateTree Intent v1`
  - Replaced the provisional integer team identifier with exact `Team.Player` and `Team.Enemy` tags. Invalid or equal tags fail closed in the shared melee resolver. The Player registers as a Sight source; `AEnemyCharacter` inherits the existing ASC and trace fixture while `AEnemyAIController` owns Sight, target, focus, home, and StateTree lifecycle.
  - The first native StateTree contract selects Patrol, Alert, Chase, Combat, and Return intent. It requests one `UEnemyMeleeAbility` and observes its ASC-owned action tag; StateTree does not directly play Montages, apply damage, modify Health/Poise, or establish another enemy FSM. The enemy Ability identity-filters Trace Window events and reuses `UAbilityTask_MeleeTraceWindow` and `FMeleeHitResolver`.
  - The user confirmed the configured Scene01 PIE and visual route after the authored enemy fixture was completed. Main normal review and a Main adversarial fallback found no P0-P2 C++/Gameplay Tag/config blocker. A fresh `gpt-5.6-luna / xhigh` Reviewer was unavailable, so no independent-review result is claimed.
  - The focused commit intentionally excludes all mutable authored fixture WIP: Content assets, Blueprints, StateTree, GA/GE, Montage, AnimBP, marker/collision tuning, map, input, project settings, and imported resources. It is not a clean-checkout reproduction of the local combat fixture.

- [x] `TODO-02B: Oblique Perspective Camera And Combat Facing v1`
  - Replaced the free over-the-shoulder orbit with a fixed-world elevated oblique Perspective camera. Player movement resolves from the CameraBoom's world yaw, while native Look bindings are intentionally absent and authored Look assets remain mutable WIP.
  - Light, Charged, Sprint Attack, and Dodge each apply one planar action-facing yaw at startup. Existing ASC tag observation suppresses ordinary movement-facing while `State.Action.Attacking` or `State.Action.Dodging` is present, then restores it after both tags clear; Root Motion is not forcibly overwritten every frame.
  - The user confirmed the focused Scene01 PIE and visual route, including the `2000cm` position-lag configuration's empty-space Root Motion camera behavior. Main completed source/caller reads, UE 5.8 API/tag-order verification, CodeGraph, code-review-graph structural analysis, error-memory lookup, `git diff --check`, and a Main adversarial fallback with no confirmed P0-P2 native blocker. The requested fresh `gpt-5.6-luna / xhigh` Reviewer returned HTTP 503, so no independent-review result is claimed.
  - The focused native commit excludes all mutable authored WIP: `Content/**`, input mappings, Blueprints, maps, animation, Montage, material, imported resources, and generated directories. By explicit user approval it also includes the small project-policy/configuration set `AGENTS.md`, `Config/DefaultEditor.ini`, `Config/DefaultGame.ini`, and `PolyQuest.uproject`; it is still not a clean-checkout recreation of the local presentation fixture. `TODO-07B` remains the sole accepted owner for any later occlusion or presentation retune shown necessary by visual evidence.

- [x] `TODO-02C1: First Enemy Attack Profile And Reach Contract v1`
  - `UEnemyAttackProfile` is the static authored input for one Montage, one damage GameplayEffect, a positive attack range, and a non-negative post-attack cooldown. It is not a selector, queue, random table, or weapon-switching system.
  - The Controller validates the Profile before it starts StateTree logic, caches its only 2D actor-center reach as runtime `MeleeRange`, and owns cooldown expiration after an actually started attack cleans up. The enemy Ability snapshots Profile data and reuses the shared Trace Window/Resolver; the StateTree task observes the action tag and cooldown rather than creating a second combat state.
  - Live Editor MCP readback confirmed the Root/Alert/Chase/Combat/Return topology and Chase reach binding. The user confirmed `PolyQuestEditor` compilation and focused Scene01 PIE, including the reach boundary, cooldown, interruption, target loss/reacquisition, hit rejection, and Trace cleanup.
  - Main normal review and Main adversarial fallback found no P0-P2 C++/GAS/Tag/StateTree blocker. `gpt-5.6-luna / xhigh` was unavailable, so no independent review is claimed. All authored fixture assets remain excluded mutable WIP; this native commit is not a clean-checkout fixture.

- [x] `TODO-02C2: First Enemy Death And Teardown v1`
  - Established the first enemy-only terminal path: terminal Health is clamped to zero, `AEnemyCharacter` promotes zero Health or another legal Dead source to an exact loose `State.Status.Dead` tag, then one idempotent teardown stops the Controller, StateTree, movement, and active GAS work.
  - The shared melee resolver now rejects a dead source or target, so a same-frame residual Trace cannot damage after the terminal Tag is in place. Native C2 leaves the Actor/Capsule and current pose intact; it does not reference authored death-animation assets.
  - The user confirmed `PolyQuestEditor` compilation and Scene01 PIE: after Health reaches zero the enemy no longer acts, and later attacks leave Health at zero rather than below zero. Main normal review and Main adversarial fallback found no unresolved P0-P2 C++/GAS/Tag/StateTree blocker. `gpt-5.6-luna / xhigh` remained unavailable, so no independent review is claimed.
  - All authored Blueprints, AnimBPs, GameplayEffects, Montages, StateTree assets, maps, and imported resources remain excluded mutable WIP. `TODO-02C3A`, `TODO-02C3D`, and `TODO-02C3B` are completed presentation/reaction slices; `TODO-02C3C` owns the remaining Poise, stance-break, and conditional Big Reaction/Root Motion work; `TODO-03D` owns player death, reload, and revival.

- [x] `TODO-02C3A: Enemy Death Presentation v1`
  - `ABP_Enemy_Goblin` now reads the native ASC-backed `AEnemyCharacter::IsDead()` query into a presentation-only `bIsDead` cache and enters a one-way `Dead` state. It does not mutate Health, Gameplay Tags, StateTree, Controller, movement, collision, or Ability state.
  - `To Land` is the authored alias for `Fall Loop` and `Jump`; `To Falling` is the authored alias for `Land` and `Locomotion`. Their `bIsDead == true` transitions cover those source states without duplicate direct death routes, and `Dead` has no exit transition.
  - The user confirmed the AnimBP compiled and the Scene01 PIE death presentation: the enemy reaches the terminal pose and does not revive or resume action. The compatible AnimBP, sequences, Blueprints, map, and imported assets remain excluded mutable WIP; C3A has no native source/config candidate.

- [x] `TODO-02C3D: Enemy Ragdoll Death Presentation v1`
  - After the existing C2 terminal teardown, `AEnemyCharacter` validates the Mesh Physics Asset, disables the inherited Capsule collision/overlaps, applies the engine `Ragdoll` profile, begins skeletal simulation, and wakes bodies. The ASC-owned Dead Tag remains the only gameplay survival state; disabled ragdoll or a missing Physics Asset retains the existing C3A terminal AnimBP fallback.
  - The fixed-v1 `WeaponMesh` display fixture is now visual-only at runtime: `ABaseCharacter` disables its collision and overlap generation at `BeginPlay()`, and the death path repeats that idempotent rule before Mesh physics starts. This removes the fixture from SpringArm and ragdoll physical interaction without changing the marker-driven Trace Window or shared resolver.
  - The user confirmed recompilation and the focused C3D test route passed. Main normal review and a separately performed Main adversarial fallback found no P0-P2 native/GAS blocker. `gpt-5.6-luna / xhigh` remained unavailable, so no independent review is claimed. All Physics Asset, Blueprint, AnimBP, map, animation, and imported Content remain excluded mutable WIP.

- [x] `TODO-02C3B: Enemy Hit Reaction And Safe Interrupt v1`
  - Established the first target-side hard-interrupt route: a living enemy accepts only a Health-reducing GameplayEffect whose Asset Tags include exact `Data.Reaction.Interrupt`, then sends `Event.Reaction.Enemy.Hit` to its own ASC. `UEnemyHitReactionAbility` owns the active reaction tag and Montage lifecycle; shared melee delivery remains unchanged.
  - Only an actually started reaction Montage locks movement and cancels `Ability.Attack.Enemy.Melee`, so the existing melee Ability still closes its Trace Window, clears `State.Action.Attacking`, and begins its established cooldown. Controller and StateTree Combat read `State.Action.HitReacting` and wait rather than requesting another attack; Dead teardown remains higher priority.
  - By explicit user acceptance, C3B closes on focused Scene01 reaction-animation playback. The accepted authored clip currently contains Root Motion, but C3B intentionally suppresses actor displacement through `DisableMovement()`; this is an in-place presentation fixture, not root-motion behavior evidence. `TODO-02C3C` owns any future root-motion reaction adoption.
  - Main normal review and Main adversarial fallback found no P0-P2 C++/GAS/Tag/StateTree blocker. `gpt-5.6-luna / xhigh` was unavailable, so no independent review is claimed. All GA/GE, Montage, Blueprint, AnimBP, StateTree, map, imported resources, and other `Content/**` remain excluded mutable WIP; this native commit is not a clean-checkout reaction fixture.

- [x] `TODO-01C2: Shared Dodge/Sprint Input Arbitration v1`
  - `APlayerCharacter` now temporally arbitrates one physical `DodgeSprintAction`: a keyboard Shift release before the authorable `0.15s` threshold requests the existing Dodge Ability, while reaching the threshold resolves the same held press to Sprint intent and never adds a late Dodge on release.
  - Input arbitration owns only press timing. The existing Sprint Ability remains the authority for MoveSpeed, Stamina drain, Gameplay Tags, and cleanup; action, movement-block, and airborne transitions cancel only an active Sprint, then existing movement/tag/landing paths may retry the resolved hold when conditions recover. Canceled input, physical release, teardown, and the existing post-exhaustion release gate retain their respective cleanup semantics.
  - The user confirmed the `0.15s` keyboard Scene01 PIE route. Main normal review and Main adversarial fallback found no P0-P2 C++/GAS/Enhanced Input lifecycle blocker; `gpt-5.6-luna / xhigh` was unavailable, so no independent review is claimed. `Content/**` input actions, mappings, Blueprints, and all other authored combat WIP remain excluded from the native commit.

## Milestones

### First Enemy, Camera, And Combat Presentation

- [ ] `TODO-02C3C: Enemy Poise And Stance Break v1`
  - Add enemy Poise/MaxPoise ownership, GameplayEffect-driven depletion, stance-break tagging, recovery, and the ability/StateTree gates that consume the proven reaction contract.
  - Keep Poise mutation on the target ASC and authored GameplayEffects; animation supplies timing/presentation, while StateTree continues to express intent rather than owning Poise or Health. Death remains the C2 terminal path and must win over any active break/recovery state.
  - C3B remains one in-place hard-interrupt reaction selected by Charged damage semantics; C3C owns the first real Big Reaction tier after Poise proves when it is warranted. Use discrete full-body Front/Back/Left/Right Montages when compatible direction assets exist; do not use a 1D BlendSpace for circular impact direction or introduce a generic Reaction DataAsset before more than one validated tier/direction set needs it.
  - A root-motion Big Reaction is conditional, not an asset-side toggle: adopt it only when the selected compatible assets demonstrate a visible gameplay need. Its accepted plan must define Montage-only root-motion extraction, navigation intent while movement is under reaction control, collision and ledge behavior, interruption/death teardown, and recovery to normal AI movement. C3B keeps `DisableMovement()` and does not silently support root-motion displacement.
  - Validate repeated Poise damage, break during attack and recovery, reset/recovery, death at zero Health, and cleanup on interruption. Do not add directional block, parry, guard break, hyper armor, or multi-enemy coordination here.

- [ ] `TODO-02D: Defensive Combat And Hyper Armor v1`
  - Add directional block, timed parry, guard break, and notify-timed hyper armor only after `TODO-02C1` through `TODO-02C3C` have proven the enemy hit resolver, Poise, and action-cancellation contracts. Any player-facing reaction needed by those mechanics receives its own accepted slice rather than expanding an enemy milestone implicitly.
  - Parry, Dodge, Block, and Potion remain immediate preflight actions during a combo CancelWindow; none becomes a global pre-input buffer.

- [ ] `TODO-02E: Bidirectional Melee Combat Health And Lean Review v1`
  - After the first enemy, oblique camera/combat-facing, `TODO-02C1` through `TODO-02C3C`, and defensive actions are proven, audit player/enemy melee damage delivery, any adopted target-assist behavior, team filtering, cancellation/death teardown, StateTree-to-GAS intent boundaries, and camera/facing interactions.
  - Add no combat feature. Repair blockers here; assign non-blocking findings to the owning combat or player-loop milestone with evidence and a concrete closure trigger.
  - A lean pass is limited to redundant input paths, expired debug fixtures, redirects, and assets/configuration proven to have zero referencers or an explicit replacement. It is not authority to remove Marketplace or authored Content by directory.

### Player Loadout, Rewards, And Encounters

- [ ] `TODO-03A: Equipment And Combat Loadout Foundation v1`
  - Establish the first product equipment boundary after the fixed straight-sword trace has proven itself: item definition/instance ownership, equip and unequip transaction, display/socket attachment, Loadout selection, and the Ability grant/revocation policy for a changed weapon.
  - Do not add world pickup, checkpoint, SaveGame, inventory grids, or a generic backpack. A loadout change affects future input routing and grants only; it never becomes a second combat-state machine or a replacement for GAS.
  - Replace the v1 fixed `WeaponMesh` / `BladeTraceBase` / `BladeTraceTip` name lookup with an equipped-weapon trace-sample provider or authored weapon data. Weapon changes may replace that provider, but must reuse `UAbilityTask_MeleeTraceWindow` and `FMeleeHitResolver` rather than creating a second melee delivery path.
  - Make the weapon component or equipment data the single source of truth for equipped-weapon collision policy, replacing C3D's fixed-name `WeaponMesh` runtime guard. It must distinguish display, physical interaction, and gameplay trace needs so visual weapons cannot accidentally block the SpringArm or corpse physics, while melee remains marker-driven through the existing Trace Window and resolver.

- [ ] `TODO-03B: Player Bow And Projectile v1`
  - Rebuild Bow aiming, prepared-arrow presentation, socket-authoritative projectile delivery, and projectile-specific GAS damage delivery after equipment/loadout ownership is proven.
  - Add aim offsets only when the selected assets expose a demonstrated vertical-aim mismatch. Do not add enemy AI or a second projectile/damage framework in this stage.

- [ ] `TODO-03C: Ranged Enemy v1`
  - Add one StateTree-driven ranged enemy after the player Bow and projectile damage path are proven. It reuses the existing enemy ASC/StateTree and shared target/team/damage contracts rather than introducing another AI framework.

- [ ] `TODO-03D: Checkpoint, Rest, Death Reload, And Save Foundation v1`
  - Establish new PolyQuest checkpoint/rest, death reload, and SaveGame ownership before durable pickups or rewards. Define stable Save IDs, a single transaction owner, failed-save behavior, and a reload-validation fixture.
  - Do not migrate old SaveGame identifiers, old world actor names, or raw actor references.

- [ ] `TODO-03E: Potion And Consumable Loop v1`
  - Add the first healing consumable after ownership, save, action interruption, and cancellation contracts exist.
  - Preserve notify-timed healing and interruption semantics where the chosen animation needs them; do not add a backpack or general consumable framework without a player-loop need.

- [ ] `TODO-03F: Pickup, World Drops, Fixed Rewards, And Reward Persistence v1`
  - Rebuild transient enemy currency drops separately from persistent fixed-item rewards and one-time elite defeat state.
  - Each durable reward requires a stable authored ID, one transaction owner, failed-save behavior, and a reload-validation fixture.

- [ ] `TODO-04A: Encounter, Fog Gate, And Clear Persistence v1`
  - Build a new encounter clear-persistence loop, fog boundary, and participant ownership after the new save/checkpoint boundary is verified.
  - Fixed rewards remain the independent `TODO-03F` special-enemy contract; an encounter does not create a second reward or one-time-defeat system.
  - Add multi-enemy attack coordination only if the first real encounter proves that ordinary StateTree/GAS spacing cannot preserve readable combat.

- [ ] `TODO-04B: Persistence And Encounter Health Review v1`
  - Audit the completed player-loop and encounter path before punish abilities: stable Save IDs, transaction ownership, failed-save behavior, reload/re-entry, reward delivery, encounter cleanup, fog-gate state, and persistence boundaries between actors, subsystems, and authored data.
  - Do not add inventory, encounter, or combat features. Fix confirmed data-integrity or lifecycle blockers; record every remaining accepted risk under its owning milestone or `Known Risks And Validation Debt` with evidence and a closure trigger.
  - This is a persistence-safety review, not a broad asset-cleanup pass. Any removal still requires a proven zero-reference or replacement path.

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

- [ ] `TODO-06C: Route Health And Lean Review v1`
  - Before polish, exercise the integrated checkpoint-to-completion route and audit cross-system ownership, dead references, temporary test fixtures, duplicate or obsolete configuration/assets, teardown after interruption/death/reload, and drift between implementation, authored data, and project documentation.
  - Add no new player loop or combat mechanic. Fix confirmed route blockers in this stage; put any remaining accepted risk in its concrete owning milestone or `Known Risks And Validation Debt` with evidence and a release-gate closure condition.
  - Lean removal remains evidence-led: source/config inspection plus Asset Registry/Reference Viewer must show zero referencers or an explicit replacement. Keep high-frequency authored WIP outside a cleanup commit unless its stable dependency closure is separately approved.

- [ ] `TODO-07A: HUD And Combat Observability v1`
  - Add only the HUD and debug/observability surfaces needed to make the validated player, enemy, Stamina, Poise, target, and damage loop legible.
  - This is not a vehicle for new combat mechanics or a generic UI framework.

- [ ] `TODO-07B: Feedback, Presentation Retune, And Demo Polish v1`
  - Add the VFX/SFX, camera, navigation readability, and presentation feedback needed to make the validated loop readable and presentable.
  - Add the first Small Reaction only as a non-interrupting, non-stacking upper-body additive flinch. It must not cancel melee, modify Health/Poise, or become a second combat-state source. Start with discrete authored presentation; only introduce a 2D additive BlendSpace from the target-local forward/right impact vector if focused visual validation shows that continuous direction materially improves readability. Small Reaction does not own root-motion displacement.
  - After the core player-combat framework and selected weapon animation set are stable, run one focused combat-presentation retune over provisional Montages: final source-animation selection, per-weapon Montage composition, root-motion feel, and manually authored segment rates.
  - Re-author `AttackTraceWindow`, Combo Input/Branch, Dodge Cancel, and Dodge Invulnerability timing against the final motions while retaining the proven one-buffer, Branch consumption, one-hit-per-entry, and ability-owned gameplay contracts.
  - If that retune needs runtime playback-rate control, create a separate approved RateWindow lifecycle stage. It must restore rates on natural completion, interruption, cancellation, combo handoff, `EndAbility()`, and EndPlay; temporary authored segment rates are not that system.
  - Re-run focused PIE visual and teardown checks for contact readability, root-motion continuity, hit/cancel timing, and stale tags/tasks after interruption.

- [ ] `TODO-07C: Full Route Regression v1`
  - Establish the repeatable manual validation matrix for New Game, Continue, checkpoint/rest, ordinary enemy reset, cleared encounter restore, equipment and consumables, drops/fixed rewards, player/enemy ranged combat, punish abilities, Boss completion, and completion Continue.
  - Fix regressions in previously completed systems only; this stage does not defer their original implementation or add new gameplay systems.

## Accepted Technical Direction

- **Test migration boundary:** migrate verified player-facing behavior and acceptance cases, not the old FSM, `EActionState`, save identifiers, or authored asset topology.
- **Gameplay authority:** GAS owns ability activation, costs, blocking/cancellation tags, combat state, damage, poise, hit reaction, and death. Animation owns timing and presentation; it is not a second gameplay state source.
- **Input intent:** Physical controls express stable player intent; the active weapon/loadout and GAS determine the concrete ability. Mapping Contexts change for control modes, not for profession labels or weapon inventory alone.
- **Camera and facing:** PolyQuest uses an elevated oblique-perspective follow camera, not a Souls-style over-the-shoulder orbit. Movement is camera-relative; ordinary locomotion faces movement, while an active attack or Dodge retains a resolved planar action-facing direction. Hard lock-on, manual target switching, and a sprint free-run exception are not baseline systems; optional target assist requires a dedicated adoption decision after visual evidence shows the oblique combat loop needs it.
- **Data ownership:** a PolyQuest character/ability manifest composes focused authored assets. Combo entries, action settings, enemy attack profiles, and reaction data are modular tuning inputs; none stores mutable gameplay state or replaces ability/effect ownership.
- **Combo contract:** one active light-attack ability owns at most one buffered LMB continuation. `ComboWindow` accepts it, `ComboBranchWindow` consumes early input or retries late input, and `CancelWindow` exposes only a scoped immediate-cancel opportunity for preflight-valid actions.
- **Montage lifetime:** a successor combo entry must explicitly reject stale completion/interruption events from a prior entry. Natural completion, cancellation, hit/death teardown, invalid timing events, and asset failure converge through one ability cleanup path.
- **Combat timing:** weapon collision, potion heal, projectile release, parry active frames, hyper armor, combo windows, and cancellation windows are semantic animation events. Their receiver validates the active ability and current target/context before applying gameplay. Light, Charged, and Sprint Attack use shared `AttackTraceWindow` Begin/End events; the retired action-specific one-shot hit events are not a second damage path.
- **Enemy AI:** StateTree is the default high-level behavior brain for both ordinary enemies and Bosses. It owns patrol, alert, chase, combat intent, return-home, phase selection, and high-level transitions; it requests GAS abilities rather than directly applying combat mutations.
- **Bosses:** Bosses use a separate StateTree and combat data profile, not a different AI framework merely because they do not patrol. Encounter, fog-gate, persistence, and rewards remain outside the Boss StateTree.
- **No parallel FSM:** do not retain an enemy HFSM or `EEnemyState` as a competing runtime truth beside StateTree and GAS tags.
- **Behavior Tree escalation gate:** do not introduce Behavior Trees unless concrete production behavior requires deeply nested reactive arbitration, dynamic subtrees, or broad concurrent services that remain unclear and untestable in StateTree.
- **External resources:** `Content/Assets/` is a raw source reservoir. A resource becomes an imported product integration only when an approved stage names it and validates its exact Skeleton, socket, animation, or gameplay surface.

## Recommendations

- **Behavior Trees:** keep Behavior Tree tooling available but unadopted. Re-evaluate it only at the documented escalation gate; do not split ordinary enemies and Bosses across AI frameworks preemptively.
- **Motion Warping:** add product-level Motion Warping only when the selected stylized animation set contains a concrete root-motion alignment requirement.
- **Shared Dodge/Sprint key:** `TODO-01C2` establishes one temporal physical-input route: a short release requests Dodge and a `0.15s` hold resolves to Sprint intent. Do not rebind independent legacy `IA_Dodge` and `IA_Sprint` actions to the same key, because their independent Started callbacks would race rather than share the arbiter's release/cancel semantics.

## Known Risks And Validation Debt

- `TODO-01H` user compile/PIE evidence covers the shared trace behavior before the source-only relocation of `UAnimNotify_ChargedAttackHoldReady` into `AnimNotifyState_ActionWindows.*`, which removes the retired `AnimNotify_Attack.*` pair. Before treating that relocation as runtime-validated, compile `PolyQuestEditor` and run a focused Scene01 Charged HoldReady smoke; record that result here. This remains accepted validation debt, not a new feature stage.
- `UAbilityTask_PlayMontageAndWait::ExternalCancel()` can end a task without an explicit guarantee that the Montage stops. Static inspection found no current PolyQuest caller. When `TODO-01C` or a later Stun/explicit-interrupt path needs task-level cancellation, it must define montage-stop behavior, converge through `EndAbility()`, and add focused PIE coverage before relying on that path.
- `TODO-01A` deliberately withholds its mutable authoring assets from this commit. A curated stable asset baseline is required before the local fixture can be reproduced from a clean checkout; this is a versioning boundary, not a failure of the local compile/PIE validation.
- `TODO-01B` intentionally skips the sword/sequence Reference Viewer dependency-closure audit by user decision. Its direct stable-asset commit may therefore omit material or Skeleton dependencies and must not be treated as a clean-checkout asset baseline; rerun the closure audit before promoting it as one.
- `TODO-01C1` has accepted keyboard/mouse PIE evidence only. Right Shoulder and Left Trigger mapping behavior and physical controller operation are not verified controller support. Before presenting controller support, adding controller-specific UX, or producing a controller-facing build, read back the intended Mapping Context entries and pass focused hardware validation for every intended controller action; until then, do not claim gamepad support.
- `TODO-01C2` has accepted keyboard Shift Scene01 PIE only. Controller parity was deliberately not authored or tested; before assigning the shared Dodge/Sprint route to a controller input or claiming controller support, read back the Mapping Context and pass focused hardware coverage for threshold release, Canceled input, exhaustion release-gate, and temporary action/airborne blockers.
- Light entry/continuation, Dodge, and Sprint Attack can commit a Cost before authored Montage startup is confirmed; Charged can commit before its release/resume path is known usable. Dodge can also cancel an eligible prior action after Commit but before its own startup is confirmed. A rare playback failure after valid preflight can therefore consume Stamina, end the Ability, or prematurely interrupt the prior action without an automatic recovery contract. The normal AnimInstance/Slot path has user PIE evidence, but failure injection is unverified. Before defining a final combat asset baseline or adding runtime playback-rate control, define the atomicity, refund, and interruption semantics and add focused failure-injection coverage for every affected action.
- `TODO-01E` has manual threshold validation, including the repaired normal-release-before-delay route, but no deterministic same-frame input/timer injection test. Before changing input-event dispatch order, introducing prediction/networking, or relying on frame-exact charge thresholds, add a deterministic automated fixture or controlled logging harness that exercises both callback orders and verifies exactly one Light or Charged activation.

## Deferred TODOs

- **Deferred Sprint Loop Presentation (`TODO-07B`):** `TODO-01F` validates Sprint speed, Stamina, cancellation, and Sprint Jump independently of a dedicated loop. The current locomotion BlendSpace remains intentional temporary presentation while Sprint is active. When the selected locomotion set is stable, retarget and validate an In-Place Dungeon Knight Sprint Loop, let `ABP_Player_Dungeon` read `State.Movement.Sprinting` only for presentation, and run focused PIE checks for cadence, foot sliding, Root Motion isolation, and transitions back to ordinary locomotion. Do not describe a dedicated Sprint Loop as integrated before that work passes.

## Stage Completion Standard

A roadmap stage is complete only after its approved plan, focused source/asset validation, user-owned compilation/PIE evidence where applicable, review, documentation synchronization, and explicit commit approval are complete.
