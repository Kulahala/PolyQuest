# PolyQuest Architecture

## Current Verified State

PolyQuest is a UE 5.8 Windows C++ project created from the Third Person template. The only runtime module is `PolyQuest`.

The game module currently declares these public dependencies:

```text
Core, CoreUObject, Engine, InputCore, EnhancedInput, AIModule,
StateTreeModule, GameplayStateTreeModule, GameplayAbilities,
GameplayTags, GameplayTasks, UMG, Slate
```

The live UE 5.8 editor resolves the GameplayAbilities plugin, and the module links the GAS runtime dependencies. PolyQuest has completed its first single-player GAS foundation: the active player exposes an ASC, a five-attribute AttributeSet, and a project-owned Gameplay Tag config source.

`TODO-01A` adds the first locally compiled and PIE-verified player ability path: LMB requests a native light-attack ability, the ability commits an authored stamina cost, a Montage Notify emits a semantic Gameplay Event, one pawn sweep finds a target character, and an authored damage GameplayEffect is applied through the target ASC. The local fixture has passed the user-owned recovery, repeated-input, cost, no-target, and target-damage checks.

`TODO-01C` extends that local fixture with a ground-only, camera-relative Root Motion Dodge. A shared Stamina-action lifecycle permits a positive remainder to overdraw to zero, gates later actions through exhaustion, delays periodic recovery after committed actions, and uses semantic NotifyState events for attack cancellation and Dodge invulnerability timing.

`TODO-01D` extends the light-attack fixture with a data-driven two-entry linear combo. The active ability owns one buffered `Input.PrimaryAttack`, identity-filtered semantic animation events, per-entry cost and hit consumption, recovery Dodge cancellation, and one teardown path across both Montages.

`TODO-01E` keeps `Input.PrimaryAttack` as one hold/release route. After an active Combo has first received the input, the no-cost Primary Ability arbitrates short release to Light Attack and a held release to Charged Attack. Charged pauses its Root Motion Montage at a semantic HoldReady event, commits its Stamina cost only on normal release, resumes the same playhead, and consumes one identity-filtered hit event. Primary, Light, Charged, and Dodge own shared movement/jump input-block tags while active.

`TODO-01F` establishes ground Sprint, Stamina-costed Jump, Sprint Jump air speed, and one optional Loadout-owned Sprint Attack. `MoveSpeed` is an Attribute consumed by CharacterMovement; Sprint state is an ASC-owned tag rather than a Player boolean or a transient speed comparison. The user has compiled and PIE-validated the configured Scene01 route, while the dedicated Sprint locomotion loop remains presentation work deferred to `TODO-07B`.

`TODO-01G` health-reviewed the player-combat foundation and hardened the shared Montage lifetime contract: synchronous startup completion must not access state already cleared by `EndAbility()`, while Dodge identity-filters Montage and invulnerability events and removes its effect, delegate, tasks, and active Montage through the same cleanup path.

`TODO-01H` replaces the provisional actor-forward sweep with one native shared melee-delivery contract for Light, Charged, and Sprint Attack. The active Ability retains state, Cost, cancellation, attack-specific data, and active-Montage identity validation; `UAnimNotifyState_AttackTraceWindow` sends Begin/End timing only. `UAbilityTask_MeleeTraceWindow` owns prior/current Blade Base/Tip samples and a per-window set of successfully delivered targets, while `FMeleeHitResolver` rejects self, invalid or equal exact `Team.*` tags, missing target ASCs, and `State.Status.Invulnerable` before applying the authored GameplayEffect through GAS. `WeaponMesh`, `BladeTraceBase`, and `BladeTraceTip` are a fixed v1 source fixture rather than equipment ownership or runtime weapon switching.

`TODO-02A` establishes the first enemy native endpoint without adding a parallel AI or combat state machine. The Player is an explicit Sight source; the enemy Controller owns perception, current target, focus, home location, and StateTree lifetime; StateTree selects Patrol, Alert, Chase, Combat, and Return intent; and one server-only enemy melee Ability reuses the existing Trace Window and GAS resolver delivery path.

`TODO-02B` establishes a fixed-world elevated oblique Perspective camera rather than an over-the-shoulder orbit. `APlayerCharacter` derives movement and one-time action-facing yaw from the actual CameraBoom world yaw. ASC-owned `State.Action.Attacking` and `State.Action.Dodging` temporarily suppress ordinary movement-facing; root-motion rotation is never overwritten every frame. Native input intentionally leaves Look actions unbound, while their authored pointers remain available for future UI ownership.

Focused native source/config commits intentionally keep mutable authoring assets out of version control: the GameplayAbility Blueprint, GameplayEffects, Montage, AnimBP, `BP_Player`, and input assets remain local development WIP. Selected meshes, Skeleton/material dependencies, and animation sequences are a stable source-asset baseline, but the source/config subset alone is not a clone-ready reproduction of the local PIE fixture.

## Product Entry And Template Retirement

`/Game/Maps/Scene01` is the product prototype map. `Config/DefaultEngine.ini` sets it as both the game default map and the editor startup map.

The active player route is `BP_GameMode -> BP_Player -> APlayerCharacter -> ABaseCharacter`, while `BP_PlayerController -> APolyQuestPlayerController` owns desktop mapping-context installation. `APolyQuestGameMode` and `APolyQuestPlayerController` remain the same reflected `/Script/PolyQuest` Blueprint roots after their source moved to `Framework/`.

`APolyQuestPlayerController` adds each authored `DefaultMappingContexts` entry only for a local desktop player. The fixed-camera native route intentionally does not bind a Look action; Mapping Context asset topology remains user-owned authoring WIP. The retired mobile touch widget, forced-touch setting, and mobile-excluded context path are not part of the product route.

`APolyQuestCharacter`, the `ThirdPerson` map/Blueprint closure, and the generated `Variant_Combat`, `Variant_Platforming`, and `Variant_SideScrolling` source/content closures have been retired. New product gameplay is introduced through documented PolyQuest stages rather than by extending a generated template variant.

## GAS Core Contract

### Actor And Attribute Ownership

- `ABaseCharacter` owns one `UAbilitySystemComponent` and one `UCharacterAttributeSet` default subobject.
- The AttributeSet is registered exactly once through `AddAttributeSetSubobject(...)` during character construction. Its current fields are `Health`, `MaxHealth`, `Stamina`, `MaxStamina`, and `MoveSpeed`; the first four initialize to `100.0f` and `MoveSpeed` initializes to `500.0f`.
- `UCharacterAttributeSet` clamps both current and base Stamina to `[0, MaxStamina]`, clamps `MoveSpeed` to a nonnegative value, and sets the loose `State.Status.Exhausted` count exactly to `1` at zero or `0` after recovery. Attributes remain the source of truth, so repeated Stamina drains while clamped at zero cannot leave Exhausted latched after recovery.
- In the current single-player boundary, `OwnerActor == AvatarActor == ABaseCharacter`. `BeginPlay()` initializes actor info with `InitAbilityActorInfo(this, this)` so an unpossessed test target can receive a GameplayEffect. `PossessedBy()` initializes it again after the superclass possession path.
- `StartupAbilities` is a Blueprint-configured list on `ABaseCharacter`. Authority grants each class during possession only when `FindAbilitySpecFromClass()` confirms it is not already present, so a repeated possession path cannot duplicate a spec.
- `EndPlay()` calls `CancelAllAbilities()` before character teardown. This is the character-level teardown entry for active Montages, AbilityTasks, and owned ability tags.
- The player-specific camera, movement, look, and jump input layer belongs to `APlayerCharacter`; the base character has no unconditional tick or player input contract.

### Runtime Routing And Input

- `BP_GameMode` is the active default GameMode and selects `BP_Player` as the default Pawn and `BP_PlayerController` as the PlayerController.
- `APolyQuestPlayerController` installs Blueprint-authored desktop `DefaultMappingContexts` for local players. `APlayerCharacter` intentionally leaves `LookAction` and `MouseLookAction` unbound in the fixed-camera route; authored mapping assets remain outside the native runtime contract.
- `APlayerCharacter` binds `PrimaryAttackAction`, `AimAction`, and four fixed `AbilitySlotActions`. Every combat-input `Started` records held time, sends `Event.Input.Pressed` with the actual `Input.*` tag in `FGameplayEventData::InstigatorTags`, then resolves the active Combat Loadout through the ASC. `Completed` sends `Released`; `Canceled` sends `Canceled`; both clear the held state. The input layer never plays a Montage, spends Stamina, traces, or mutates an Attribute directly.
- `DA_CombatLoadout_StraightSword` currently maps `Input.PrimaryAttack -> Ability.Attack.Primary`; the Primary Ability then chooses Light or Charged after the Combo listener has had first access to the same Pressed event. Absent Aim and Slot routes intentionally express an input event without activating an Ability. A Loadout change affects future input starts only and never grants, revokes, or cancels Abilities. Future equipment owns the corresponding Ability-grant policy separately.
- `Event.Input.Pressed`, `Event.Input.Released`, and `Event.Input.Canceled` are semantic delivery events for already active Abilities using `WaitGameplayEvent`; they are not general `AbilityTriggers`. `Event.Attack.Charged.ReleaseHandoff` is the narrow exception: its tagged payload activates Charged when a normal release at or after the threshold arrives before the `WaitDelay` callback, so Charged can use the original held duration after Character input state has been cleared. A future event-triggered Ability must use a dedicated outer event tag or validate the payload's input intent before activation, because the generic outer event alone does not distinguish Primary, Aim, and Slot input.
- Current validation is keyboard/mouse-only by explicit scope decision. Gamepad Right Shoulder and Left Trigger mappings are deferred rather than treated as verified controller support.
- `APlayerCharacter` uses one `DodgeSprintAction` as a temporal physical-input arbiter. Its positive authorable threshold defaults to `0.15s`: release before that threshold requests the existing `Ability.Dodge`, while reaching the threshold resolves the held press to Sprint intent and never adds a late Dodge on release. Dodge still derives one camera-relative world direction from the latest movement input; Jump release still calls `StopJumping()` so a pre-action UE jump request cannot remain latched.
- The arbiter owns only press timing and held intent. It never changes MoveSpeed, Stamina, Gameplay Tags, Montages, or Ability cleanup; `UDodgeAbility` and `USprintAbility` remain the GAS owners of those contracts, and the ASC Sprint tag remains runtime truth. Attacking, Dodging, movement blocking, and leaving the ground end only the current Sprint; a resolved long hold survives those temporary blockers and existing movement, tag, and landing paths retry it when valid. Physical release, Canceled input, and teardown clear the shared input state; exhaustion still requires a physical release before Sprint can restart. Keyboard/mouse is the verified scope, while controller support remains a Roadmap validation debt.

### Camera And Action Facing

- `CameraBoom` owns a fixed-world, elevated oblique Perspective composition with SpringArm collision; it is not driven by controller look input. Movement resolves its horizontal forward/right axes from the CameraBoom's current world yaw.
- Light, Charged, Sprint Attack, and Dodge resolve and apply one horizontal facing yaw at ability startup. With no movement input, the action keeps the actor's current horizontal forward direction. Combo continuation does not resolve a new direction.
- While the ASC owns `State.Action.Attacking` or `State.Action.Dodging`, `APlayerCharacter` disables `CharacterMovement.bOrientRotationToMovement`. Once both tags are absent, ordinary locomotion-facing is restored. This does not force a per-frame yaw over Root Motion.
- Hard lock-on, manual target switching, mouse ground-projection facing, a sprint free-run exception, Motion Warping, and a generic camera framework are not part of this baseline.

### Light Attack Ability Lifecycle

- `ULightAttackAbility` is `InstancedPerActor` and `ServerOnly` within the current single-player boundary. It owns `Ability.Attack.Light`, owns `State.Action.Attacking` while active, and is blocked by attacking, dodging, dead, exhausted, and stunned state tags.
- Before the initial `CommitAbility()`, it validates its ASC, animation instance, inherited Cost/Damage/regen-delay GameplayEffect classes, required event tags, and a nonempty `UComboChainDataAsset` containing unique non-null complete Montages. A missing required configuration logs a warning and ends without applying cost, starting tasks, or recording hit state.
- `UComboChainDataAsset` stores ordered Montage references only. It holds no active entry, buffered input, target, cost state, or other mutable runtime state; `ULightAttackAbility` owns those values for the entire chain.
- The initial entry commits once through `CommitAbility()`. A continuation first passes `CheckCost()` and then commits only its Cost through `CommitAbilityCost()`, so each accepted entry spends Stamina once while the shared Stamina-action base applies regeneration delay when the whole Ability ends.
- Persistent `UAbilityTask_WaitGameplayEvent` listeners receive Trace Window, Dodge-cancel, primary-input, Combo InputWindow, and Combo BranchWindow semantics across the chain. `UAbilityTask_PlayMontageAndWait` starts the selected entry, while an identity-filtered `UAnimInstance::OnMontageEnded` callback owns natural completion or interruption; an end event from a replaced Montage cannot end its successor.
- `UAnimNotifyState_AttackTraceWindow` and the combat action-window NotifyStates send semantic events from an ASC-capable mesh owner and attach their source animation in `FGameplayEventData::OptionalObject`. The Ability accepts only events from its current entry Montage. It permits one buffered `Input.PrimaryAttack`: an early input is consumed when the BranchWindow opens, while an input during an open BranchWindow continues immediately.
- A matching Trace Window opens or closes the shared melee task. The task samples the fixed Blade Base/Tip path, and its resolver-owned GAS delivery can record a target only after successful resolution; no animation event writes `Health` or `Poise` directly.
- Natural completion, interruption, Dodge cancellation, character teardown, and configuration failure converge through `EndAbility()`. Cleanup removes the Montage delegate, ends all tasks, removes the scoped Dodge-cancel tag, clears entry/buffer state, and prevents duplicate cleanup.
- Future direct task-level `ExternalCancel()` callers must define whether they also stop the Montage and must still converge through ability cleanup. There is no current caller; this is a conditional cancellation-contract requirement for later Stun or explicit interruption work.

### Stamina Actions And Dodge Lifecycle

- `UStaminaActionAbility` is the narrow base for Stamina-consuming actions. It permits activation only while current Stamina is positive, allows the authored cost GameplayEffect to consume the remaining amount, and lets the AttributeSet clamp the result to zero. After a successfully committed action ends, it applies the authored regeneration-delay effect through the owner ASC.
- `ULightAttackAbility` now derives from that base and listens for semantic Dodge-cancel window Begin/End events. It owns `State.Action.CanCancel.Dodge` only while its active Montage window permits an interruption, then removes that loose tag from every normal, cancelled, interrupted, and teardown path.
- `UDodgeAbility` is `InstancedPerActor` and `ServerOnly` in the current single-player boundary. It requires grounded movement, rejects dead, stunned, exhausted, or already-Dodging states, and may interrupt a light attack only when the attack owns `State.Action.CanCancel.Dodge`. It validates its authored cost, recovery-delay, invulnerability effect, Montage, and tags before committing; only after commit does it cancel the eligible attack and begin the Root Motion Montage task. That same post-commit path cancels Primary during hold arbitration and Charged while `State.Action.Charging` is present; after Charged release, Dodge again requires the authored recovery cancel window.
- The Dodge ability listens for semantic invulnerability Begin/End events and owns the active invulnerability-effect handle. Montage completion, interruption, cancellation, `EndPlay()`, or a late event all converge through `EndAbility()`, which ends tasks and removes that effect.
- `UAnimNotifyState_ActionDodgeCancelWindow` and `UAnimNotifyState_DodgeInvulnerability` are separate reflected types in one combat-action-window source group. They require only an ASC-capable mesh owner and send Gameplay Events; they do not mutate Attributes, Gameplay Tags, or ability state directly.

### Primary And Charged Attack Lifecycle

- `UPrimaryAttackAbility` is `InstancedPerActor`, `ServerOnly`, and has no Cost. It owns the generic movement/jump input-block tags only while it arbitrates one held `Input.PrimaryAttack`; a `UAbilityTask_WaitDelay` at `0.2 s` and exact Released/Canceled event listeners choose the next action. A short normal release requests `Ability.Attack.Light`; Canceled ends without creating a new attack.
- If a normal release at or after `0.2 s` wins the same-frame race with the delay callback, Primary forwards the original event duration through `Event.Attack.Charged.ReleaseHandoff` rather than reading the already-cleared held-input cache. `UChargedAttackAbility` accepts only that event with the owning actor and exact `Input.PrimaryAttack` tag, then begins its release path once.
- `UChargedAttackAbility` derives from `UStaminaActionAbility`, owns `Ability.Attack.Charged` and `State.Action.Attacking`, and adds `State.Action.Charging` only before release. It validates its authored Montage/effects/tags, pauses at the matching HoldReady Notify, checks and commits Cost on normal release, calculates a `1.0x` to `1.8x` SetByCaller damage multiplier from held duration, then resumes the paused Root Motion Montage from the same playhead.
- After release, Charged accepts matching Trace Window Begin/End events only from its active Montage and opens or closes the common melee task with its existing authored GameplayEffect and `Data.Damage.Charged` multiplier. Montage identity, source actor, source animation, and `OnMontageEnded` reject stale events and converge natural completion, cancellation, Dodge, and teardown through `EndAbility()`.

### Sprint, Jump, And Sprint Attack Lifecycle

- `MoveSpeed` is the final horizontal cap owned by `UCharacterAttributeSet`. `ABaseCharacter` binds exactly one ASC attribute-change delegate after ActorInfo initialization, writes the nonnegative value to `CharacterMovement.MaxWalkSpeed`, and removes that delegate in `EndPlay()`.
- `USprintAbility` is an `InstancedPerActor`, `ServerOnly` continuous ability. While grounded Sprint input and nonzero movement remain valid, it owns `State.Movement.Sprinting` and `State.Resource.Stamina.RegenBlocked`, applies authored MoveSpeed and periodic Stamina-drain effects, and converges input release, zero movement, airborne state, action interruption, exhaustion, external cancellation, and EndPlay through cleanup. Exhaustion requires the Sprint input to be released before a new Sprint may start.
- `UJumpAbility` commits its Stamina cost before native `ACharacter::Jump()`. A Jump that began during Sprint applies a separately removable air-only MoveSpeed effect before ending ground Sprint. The air effect grants neither a Sprint tag, Stamina drain, nor regeneration block; Player removes it on landing or EndPlay. Walking off a ledge only ends Sprint and never grants this jump-specific air speed.
- `UCombatLoadoutDefinition` can supply one optional Sprint Attack Ability tag. Primary input sends its existing semantic event first, then Player requests that tag only while a real Sprint tag, grounded state, and movement input are all present; unavailable or rejected Sprint Attack falls back to `Ability.Attack.Primary`.
- `USprintAttackAbility` validates and commits its authored cost before it ends Sprint and plays its Root Motion Montage. It owns action movement/jump/regen-block tags only during its active lifetime, accepts matching active-Montage Trace Window timing, reuses the common melee task, and exposes Dodge cancellation only through the authored recovery window. Its Montage, tasks, loose tags, and delegate converge through `EndAbility()`.

### Stylized Player Presentation

- The local player fixture keeps `SK_Character_Hero_Knight_Male` on `SKEL_Character_Dungeon`. `Weapon_R` is a socket below `hand_r`; `BP_Player` attaches the display-only `WeaponMesh` there with `SM_Wep_Ornate_Sword_02`.
- The fixed v1 `WeaponMesh` hierarchy supplies only Blade Base/Tip transform samples through `UMeleeTraceSourceComponent`; it does not use weapon collision, overlaps, physics, equipment state, or runtime weapon switching as a damage path.
- `AM_Light_Attack01_Sword` plays the retargeted root-motion sequence `Anim_SAS_V2_ComboAttack02_01_Root` through `DefaultGroup.DefaultSlot`. `ABP_Player_Dungeon` uses `Root Motion from Montages Only`, so the Montage drives the Character's tested forward movement without a new movement component or Motion Warping contract.
- Attack animation uses the shared Trace Window NotifyState only for timing. Light, Charged, and Sprint Attack retain their own Cost, state, cancellation, and attack-specific effect data while sharing the same motion-trace task and resolver.
- The Socket, player Blueprint, AnimBP, Montage, GA/GE, input assets, and retargeting assets remain deliberately local mutable authoring WIP. The stable direct sword and retargeted Sequence assets can be versioned separately, but this subset does not recreate the local playable fixture from a clean checkout.

### First Enemy AI And Melee Intent

- `APlayerCharacter` registers its native `UAIPerceptionStimuliSourceComponent` for Sight in `BeginPlay()` and unregisters it during teardown. It defaults to `Team.Player`; `AEnemyCharacter` defaults to `Team.Enemy`, inherits the BaseCharacter ASC, startup-ability grant, `MeleeTrace` endpoint, and fixed trace-source fixture, and auto-possesses with `AEnemyAIController`.
- `UEnemyAttackProfile` is one static authored input for the first enemy: one Montage, one damage GameplayEffect, a positive `AttackRange`, and a non-negative `CooldownAfterAttack`. It is not a runtime attack list, selector, queue, or weapon-switching system.
- `AEnemyAIController` is the one owner of a valid Player target, Controller focus, home location, Sight configuration, `UStateTreeAIComponent` start/stop lifecycle, and cooldown expiration. Before starting StateTree logic it validates the possessed `UEnemyAttackProfile`, then caches its Profile-owned range as instance-only `MeleeRange`; perception only sends `Event.AI.Target.Acquired` or `Event.AI.Target.Lost`, never starts a Montage, applies a GameplayEffect, or mutates an Attribute.
- The authored StateTree selects `Patrol -> Alert -> Chase -> Combat -> Return`. Native conditions query the Controller; native tasks may stop stale movement or request the enemy Ability, but they do not store a second target/state, cancel the Ability, or mutate combat values. Combat observes the ASC-owned `State.Action.Attacking` tag until the active Ability ends naturally.
- `UEnemyMeleeAbility` is `InstancedPerActor` and `ServerOnly`. It validates the ASC, Controller target/range, animation setup, Profile, and Trace Window tags; it snapshots the Profile Montage, damage GameplayEffect, and cooldown only for one activation. It owns `Ability.Attack.Enemy.Melee` and active `State.Action.Attacking`; only matching active-Montage NotifyState events can open or close the shared trace task. Natural end, interruption, cancellation, invalid setup, and teardown converge through `EndAbility()`. The Controller cooldown begins only when `Montage_IsActive()` had confirmed that the Montage actually started.
- The minimal team rule uses exact `Team.*` tags: invalid or equal tags reject a hit. This is not yet a full faction, attitude, party, target-selection, or multiplayer relation system.
- `MeleeRange` is an exact Controller center-distance check. The authored Chase `FStateTreeMoveToTask` binds its acceptance radius to that value and disables both agent and goal radius additions; changing enemy dimensions or attack reach must preserve this one Profile-owned geometry rule.

#### Enemy Hit Reaction And Safe Interrupt

- A received GameplayEffect remains the only entry to enemy reaction semantics. After a server-side Health decrease that leaves the enemy alive, `AEnemyCharacter` reads the applied EffectSpec Asset Tags; only exact `Data.Reaction.Interrupt` sends `Event.Reaction.Enemy.Hit` to the target ASC. Healing, unchanged/direct Health writes, rejected resolver hits, and lethal damage do not request a reaction. The shared resolver remains delivery-only and does not select presentation.
- `UEnemyHitReactionAbility` is `InstancedPerActor`, `ServerOnly`, and Gameplay-Event triggered. It owns `Ability.Reaction.Enemy.Hit` and active `State.Action.HitReacting`, blocks Dead, Stunned, and re-entry, and owns one authored reaction Montage plus its delegate/task cleanup. It locks CharacterMovement and cancels `Ability.Attack.Enemy.Melee` only after that Montage is confirmed active; the cancelled melee Ability remains responsible for its Trace Window, Montage, action-tag, and cooldown cleanup.
- `AEnemyAIController` and the Combat StateTree task observe `State.Action.HitReacting`; they do not infer reaction state from playback or create a second AI state. Combat waits while the tag exists and does not issue another melee request. Reaction completion restores walking only when this Ability locked movement and the enemy is still alive; Dead teardown remains terminal.
- C3B deliberately establishes an in-place hard-interrupt policy. The currently accepted presentation fixture may contain authored root translation, but `DisableMovement()` suppresses actor displacement; that does not establish a root-motion reaction contract. `TODO-02C3C` owns any later root-motion reaction policy, its navigation/collision rules, and its interruption validation.

#### Enemy Death And Teardown

- `UCharacterAttributeSet` clamps Health to `[0, MaxHealth]`. Once an ASC owns `State.Status.Dead`, any later Health write remains `0`; the AttributeSet clamps values but does not decide which character enters a terminal state.
- After BaseCharacter initializes ActorInfo, `AEnemyCharacter` observes its Health and `State.Status.Dead`. Health at or below zero writes an exact loose Dead-tag count of one. Any legal source that grants the Dead Tag converges through the same idempotent teardown, which retains that loose count as the terminal state; this baseline has no revival semantic.
- The terminal teardown tells `AEnemyAIController` to stop StateTree, path movement, target, and focus; then cancels the enemy ASC's active Abilities and stops/disables CharacterMovement. Ability cancellation retains each Ability's existing `EndAbility()` cleanup route for Montage, action tag, Trace Window, and task state.
- `FMeleeHitResolver` rejects a shared melee request when either source or target ASC owns `State.Status.Dead`, in addition to its existing self, team, ASC, and invulnerability checks. Native C2 teardown does not select a death asset or write AnimBP state.
- `ABP_Enemy_Goblin` is presentation-only: it caches `AEnemyCharacter::IsDead()` in `bIsDead` and uses a one-way `Dead` state. Its `To Land` state alias covers `Fall Loop` and `Jump`; its `To Falling` alias covers `Land` and `Locomotion`; each alias enters `Dead` when `bIsDead == true`. `Dead` has no exit transition. The AnimBP does not write Health, Gameplay Tags, Ability, AI, Controller, movement, or collision state.
- After the terminal teardown, `AEnemyCharacter` can hand presentation to a compatible SkeletalMesh ragdoll when `bUseRagdollOnDeath` is enabled and a Physics Asset exists: it disables the inherited Capsule collision/overlaps, applies the engine `Ragdoll` profile, enables simulation, and wakes bodies. This is presentation only; the ASC Dead Tag and C2 teardown remain authoritative. Disabled ragdoll or a missing Physics Asset intentionally leaves the C3A AnimBP terminal state as the fallback.
- The fixed-v1 component named `WeaponMesh` is visual-only at runtime: `ABaseCharacter` disables its collision and overlap generation so it cannot block the SpringArm or participate in corpse physics. This name-based guard is temporary; `TODO-03A` owns replacing it with equipment-owned weapon collision policy while preserving marker-driven melee delivery.

#### ST_Enemy_Goblin_Melee Authored Runtime Contract

`/Game/BP/Characters/Enemy/ST_Enemy_Goblin_Melee` uses `StateTreeAIComponentSchema`, with `AIControllerClass` set to native `/Script/PolyQuest.EnemyAIController` and Context Actor Class set to `Pawn`. `BP_EnemyAIController` inherits `AEnemyAIController`; its inherited `StateTreeComponent.StateTreeRef` is this asset and automatic start remains disabled, so native `OnPossess()` starts logic only after Profile validation. Editor readback reports the asset compiled, with no root parameters, global evaluators, or global tasks. `Root` owns the five ordered leaf states `Patrol`, `Alert`, `Chase`, `Combat`, and `Return`; all six current state nodes are enabled and use `Any` task-completion mode, while every leaf currently has one completion-relevant task.

##### Root

- Transition 1: `OnEvent Event.AI.Target.Acquired -> Alert`; conditions: none. It is enabled, normal-priority, event-consuming, and has no payload.
- Transition 2: `OnEvent Event.AI.Target.Lost -> Return`; conditions: none. It is enabled, normal-priority, event-consuming, and has no payload.
- Every current transition in this asset is enabled, uses `Normal` priority, and has no transition delay. The two Root event routes additionally consume their matching event when selected.
- These are the common target-event entry routes rather than duplicate local transitions on every child state.

##### Patrol

- Uses one `StateTreeDelayTask` with `Run Forever` enabled. It has no local transition and waits for the Root target events.

##### Alert

- Runs native `Enemy Begin Alert`, which stops stale path following and retains valid Controller focus, then succeeds immediately.
- Transition 1: `OnStateSucceeded -> Combat`; conditions: `Enemy Has Valid Target` AND `Enemy Target Is In Melee Range`.
- Transition 2: `OnStateSucceeded -> Chase`; conditions: `Enemy Has Valid Target`.
- Both are enabled normal-priority transitions; their authored order preserves the in-range Combat choice before Chase.

##### Chase

- Uses `StateTreeMoveToTask` described by the live asset as `Move To AIController.Current Target`. `TargetActor` is bound to the Controller current target; `AcceptableRadius` is the Profile-derived Controller `MeleeRange` contract.
- `AllowStrafe` is disabled. `AllowPartialPath`, `TrackMovingGoal`, `RequireNavigableEndLocation`, and `ProjectGoalLocation` are enabled. Both `ReachTestIncludesAgentRadius` and `ReachTestIncludesGoalRadius` are disabled so navigation arrival uses the same exact 2D actor-center boundary as Controller and Ability range checks.
- Transition: `OnStateCompleted -> Alert`; conditions: none. It covers both Move To success and failure, after which Alert selects the next intent from current target/range conditions.

##### Combat

- Runs native `Enemy Request Melee Attack`. It fails if target/range becomes invalid, stays running through Controller cooldown, requests the GAS Ability only when eligible, observes `State.Action.Attacking`, and succeeds only after an observed attack finishes.
- Transition: `OnStateCompleted -> Alert`; conditions: none. StateTree never cancels the Ability, opens trace windows, or applies damage; those stay with GAS, NotifyState timing, and the shared resolver.

##### Return

- Uses `StateTreeMoveToTask` described by the live asset as `Move To AIController.Home Location`. `Destination` is bound to `AIController.HomeLocation`, `TargetActor` is empty, and `AcceptableRadius` is bound to `AIController.HomeAcceptanceRadius`.
- `AllowStrafe` and `TrackMovingGoal` are disabled. `AllowPartialPath`, `RequireNavigableEndLocation`, `ProjectGoalLocation`, `ReachTestIncludesAgentRadius`, and `ReachTestIncludesGoalRadius` are enabled. This is a home-arrival policy, not the exact melee-range geometry rule used by Chase.
- Transition: `OnStateCompleted -> Patrol`; conditions: none. Both successful arrival and terminal Move To failure leave the return route cleanly.

##### Maintenance Rule

- When a stage changes this tree's state topology, events, conditions, native tasks, transition trigger/order, property bindings, or behavior-changing Move To flags, update this contract in the same stage. Do not record node layout, color, panel state, transient montage tuning, or other high-frequency presentation WIP here.

### Gameplay Tags

- Project tags are config-authored in `Config/Tags/PolyQuestGameplayTags.ini`; there is no native tag singleton or Blueprint tag library in this stage.
- The approved leaf tags are `Ability.Attack.Light`, `Ability.Attack.Primary`, `Ability.Attack.Charged`, `Ability.Attack.Sprint`, `Ability.Attack.Enemy.Melee`, `Ability.Reaction.Enemy.Hit`, `Ability.Dodge`, `Ability.Movement.Jump`, `Ability.Movement.Sprint`, `Data.Damage.Charged`, `Data.Reaction.Interrupt`, `Event.AI.Target.Acquired`, `Event.AI.Target.Lost`, `Event.Reaction.Enemy.Hit`, `Event.Attack.Light.Combo.InputWindow.Begin`, `Event.Attack.Light.Combo.InputWindow.End`, `Event.Attack.Light.Combo.BranchWindow.Begin`, `Event.Attack.Light.Combo.BranchWindow.End`, `Event.Attack.Charged.HoldReady`, `Event.Attack.Charged.ReleaseHandoff`, `Event.Attack.TraceWindow.Begin`, `Event.Attack.TraceWindow.End`, `Event.Action.CancelWindow.Dodge.Begin`, `Event.Action.CancelWindow.Dodge.End`, `Event.Dodge.Invulnerability.Begin`, `Event.Dodge.Invulnerability.End`, `Event.Input.Canceled`, `Event.Input.Pressed`, `Event.Input.Released`, `Input.AbilitySlot.1` through `.4`, `Input.Aim`, `Input.Dodge`, `Input.PrimaryAttack`, `State.Action.Attacking`, `State.Action.CanCancel.Dodge`, `State.Action.Charging`, `State.Action.Dodging`, `State.Action.HitReacting`, `State.Input.Block.Movement`, `State.Input.Block.Jump`, `State.Movement.Sprinting`, `State.Resource.Stamina.RegenBlocked`, `State.Status.Dead`, `State.Status.Exhausted`, `State.Status.Invulnerable`, `State.Status.Stunned`, `Team.Player`, and `Team.Enemy`.
- Plugin and native test tag sources remain engine/plugin-owned and are not part of the PolyQuest taxonomy.

## Not Yet Established

The following remain future stage contracts:

- Multiple/weighted enemy attack selection, reaction tiers/direction, Poise, directional ragdoll impulse, ragdoll recovery/corpse-lifetime policy, and special attacks.
- Player death, GameplayCues, and nonlinear or multi-weapon combo-extension contracts.
- Tag-authored multi-faction/hostile relation semantics beyond the minimal equal-team rejection, persistence ownership, and multiplayer/PlayerState ownership.
- Weapon equipment, Ability-grant/revocation, multi-weapon Loadout switching, additional Skeleton/animation, and Motion Warping topology.

These decisions belong to their owning roadmap stages; they are not implied by the TODO-00B foundation.
