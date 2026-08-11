# PolyQuest Architecture

## Current Verified State

PolyQuest is a UE 5.8 Windows C++ project created from the Third Person template. The only runtime module is `PolyQuest`.

The game module currently declares these public dependencies:

```text
Core, CoreUObject, Engine, InputCore, EnhancedInput, AIModule,
StateTreeModule, GameplayStateTreeModule, GameplayAbilities,
GameplayTags, GameplayTasks, UMG, Slate
```

The live UE 5.8 editor resolves the GameplayAbilities plugin, and the module links the GAS runtime dependencies. PolyQuest has completed its first single-player GAS foundation: the active player exposes an ASC, a four-attribute AttributeSet, and a project-owned Gameplay Tag config source.

`TODO-01A` adds the first locally compiled and PIE-verified player ability path: LMB requests a native light-attack ability, the ability commits an authored stamina cost, a Montage Notify emits a semantic Gameplay Event, one pawn sweep finds a target character, and an authored damage GameplayEffect is applied through the target ASC. The local fixture has passed the user-owned recovery, repeated-input, cost, no-target, and target-damage checks.

`TODO-01C` extends that local fixture with a ground-only, camera-relative Root Motion Dodge. A shared Stamina-action lifecycle permits a positive remainder to overdraw to zero, gates later actions through exhaustion, delays periodic recovery after committed actions, and uses semantic NotifyState events for attack cancellation and Dodge invulnerability timing.

`TODO-01D` extends the light-attack fixture with a data-driven two-entry linear combo. The active ability owns one buffered `Input.PrimaryAttack`, identity-filtered semantic animation events, per-entry cost and hit consumption, recovery Dodge cancellation, and one teardown path across both Montages.

This commit intentionally keeps mutable authoring assets out of version control: the GameplayAbility Blueprint, GameplayEffects, Montage, AnimBP, `BP_Player`, and input assets remain local development WIP. Selected meshes, Skeleton/material dependencies, and animation sequences are a stable source-asset baseline, but this commit alone is not a clone-ready reproduction of the local PIE fixture.

## Product Entry And Template Retirement

`/Game/Maps/Scene01` is the product prototype map. `Config/DefaultEngine.ini` sets it as both the game default map and the editor startup map.

The active player route is `BP_GameMode -> BP_Player -> APlayerCharacter -> ABaseCharacter`, while `BP_PlayerController -> APolyQuestPlayerController` owns desktop mapping-context installation. `APolyQuestGameMode` and `APolyQuestPlayerController` remain the same reflected `/Script/PolyQuest` Blueprint roots after their source moved to `Framework/`.

`APolyQuestPlayerController` adds each authored `DefaultMappingContexts` entry only for a local desktop player. `BP_PlayerController` supplies `IMC_Default` and `IMC_MouseLook`; the retired mobile touch widget, forced-touch setting, and mobile-excluded context path are not part of the product route.

`APolyQuestCharacter`, the `ThirdPerson` map/Blueprint closure, and the generated `Variant_Combat`, `Variant_Platforming`, and `Variant_SideScrolling` source/content closures have been retired. New product gameplay is introduced through documented PolyQuest stages rather than by extending a generated template variant.

## GAS Core Contract

### Actor And Attribute Ownership

- `ABaseCharacter` owns one `UAbilitySystemComponent` and one `UCharacterAttributeSet` default subobject.
- The AttributeSet is registered exactly once through `AddAttributeSetSubobject(...)` during character construction. Its current fields are `Health`, `MaxHealth`, `Stamina`, and `MaxStamina`, initialized to `100.0f`.
- `UCharacterAttributeSet` clamps both current and base Stamina to `[0, MaxStamina]`. After a Stamina GameplayEffect executes, it adds the loose `State.Status.Exhausted` tag at zero and removes it after recovery; Attributes themselves remain the source of truth.
- In the current single-player boundary, `OwnerActor == AvatarActor == ABaseCharacter`. `BeginPlay()` initializes actor info with `InitAbilityActorInfo(this, this)` so an unpossessed test target can receive a GameplayEffect. `PossessedBy()` initializes it again after the superclass possession path.
- `StartupAbilities` is a Blueprint-configured list on `ABaseCharacter`. Authority grants each class during possession only when `FindAbilitySpecFromClass()` confirms it is not already present, so a repeated possession path cannot duplicate a spec.
- `EndPlay()` calls `CancelAllAbilities()` before character teardown. This is the character-level teardown entry for active Montages, AbilityTasks, and owned ability tags.
- The player-specific camera, movement, look, and jump input layer belongs to `APlayerCharacter`; the base character has no unconditional tick or player input contract.

### Runtime Routing And Input

- `BP_GameMode` is the active default GameMode and selects `BP_Player` as the default Pawn and `BP_PlayerController` as the PlayerController.
- `APolyQuestPlayerController` installs the Blueprint-authored desktop `DefaultMappingContexts` for local players; the current controller Blueprint supplies `IMC_Default` and `IMC_MouseLook`.
- `APlayerCharacter` binds `PrimaryAttackAction`, `AimAction`, and four fixed `AbilitySlotActions`. Every combat-input `Started` records held time, sends `Event.Input.Pressed` with the actual `Input.*` tag in `FGameplayEventData::InstigatorTags`, then resolves the active Combat Loadout through the ASC. `Completed` sends `Released`; `Canceled` sends `Canceled`; both clear the held state. The input layer never plays a Montage, spends Stamina, traces, or mutates an Attribute directly.
- `DA_CombatLoadout_StraightSword` currently maps `Input.PrimaryAttack -> Ability.Attack.Light`; absent Aim and Slot routes intentionally express an input event without activating an Ability. A Loadout change affects future input starts only and never grants, revokes, or cancels Abilities. Future equipment owns the corresponding Ability-grant policy separately.
- `Event.Input.Pressed`, `Event.Input.Released`, and `Event.Input.Canceled` are semantic delivery events for already active Abilities using `WaitGameplayEvent`; they are not general `AbilityTriggers`. A future event-triggered Ability must use a dedicated outer event tag or validate the payload's input intent before activation, because the generic outer event alone does not distinguish Primary, Aim, and Slot input.
- Current validation is keyboard/mouse-only by explicit scope decision. Gamepad Right Shoulder and Left Trigger mappings are deferred rather than treated as verified controller support.
- `APlayerCharacter` binds `DodgeAction` on `Started` and only requests `Ability.Dodge`. It caches the latest movement input so Dodge can derive one camera-relative world direction, suppresses new translation and Jump starts while `State.Action.Dodging` is present, and keeps camera look available. Jump release always calls `StopJumping()` so a pre-Dodge UE jump request cannot remain latched after the roll.

### Light Attack Ability Lifecycle

- `ULightAttackAbility` is `InstancedPerActor` and `ServerOnly` within the current single-player boundary. It owns `Ability.Attack.Light`, owns `State.Action.Attacking` while active, and is blocked by attacking, dodging, dead, exhausted, and stunned state tags.
- Before the initial `CommitAbility()`, it validates its ASC, animation instance, inherited Cost/Damage/regen-delay GameplayEffect classes, required event tags, and a nonempty `UComboChainDataAsset` containing unique non-null complete Montages. A missing required configuration logs a warning and ends without applying cost, starting tasks, or recording hit state.
- `UComboChainDataAsset` stores ordered Montage references only. It holds no active entry, buffered input, target, cost state, or other mutable runtime state; `ULightAttackAbility` owns those values for the entire chain.
- The initial entry commits once through `CommitAbility()`. A continuation first passes `CheckCost()` and then commits only its Cost through `CommitAbilityCost()`, so each accepted entry spends Stamina once while the shared Stamina-action base applies regeneration delay when the whole Ability ends.
- Persistent `UAbilityTask_WaitGameplayEvent` listeners receive hit, Dodge-cancel, primary-input, Combo InputWindow, and Combo BranchWindow semantics across the chain. `UAbilityTask_PlayMontageAndWait` starts the selected entry, while an identity-filtered `UAnimInstance::OnMontageEnded` callback owns natural completion or interruption; an end event from a replaced Montage cannot end its successor.
- `UAnimNotify_LightAttackHit` and the combat action-window NotifyStates send semantic events from an ASC-capable mesh owner and attach their source animation in `FGameplayEventData::OptionalObject`. The Ability accepts only events from its current entry Montage. It permits one buffered `Input.PrimaryAttack`: an early input is consumed when the BranchWindow opens, while an input during an open BranchWindow continues immediately.
- The first accepted hit event for each entry is consumed by a per-entry guard. The ability performs one forward sphere sweep on `ECC_Pawn`, ignores its avatar, selects the nearest other `ABaseCharacter`, creates the damage spec from the source ASC, and applies it to the target ASC. There is no team filter, weapon collision, multi-hit window, generic hit resolver, or direct `Health` write in this slice.
- Natural completion, interruption, Dodge cancellation, character teardown, and configuration failure converge through `EndAbility()`. Cleanup removes the Montage delegate, ends all tasks, removes the scoped Dodge-cancel tag, clears entry/buffer state, and prevents duplicate cleanup.
- Future direct task-level `ExternalCancel()` callers must define whether they also stop the Montage and must still converge through ability cleanup. There is no current caller; this is a conditional cancellation-contract requirement for later Stun or explicit interruption work.

### Stamina Actions And Dodge Lifecycle

- `UStaminaActionAbility` is the narrow base for Stamina-consuming actions. It permits activation only while current Stamina is positive, allows the authored cost GameplayEffect to consume the remaining amount, and lets the AttributeSet clamp the result to zero. After a successfully committed action ends, it applies the authored regeneration-delay effect through the owner ASC.
- `ULightAttackAbility` now derives from that base and listens for semantic Dodge-cancel window Begin/End events. It owns `State.Action.CanCancel.Dodge` only while its active Montage window permits an interruption, then removes that loose tag from every normal, cancelled, interrupted, and teardown path.
- `UDodgeAbility` is `InstancedPerActor` and `ServerOnly` in the current single-player boundary. It requires grounded movement, rejects dead, stunned, exhausted, or already-Dodging states, and may interrupt a light attack only when the attack owns `State.Action.CanCancel.Dodge`. It validates its authored cost, recovery-delay, invulnerability effect, Montage, and tags before committing; only after commit does it cancel the eligible attack and begin the Root Motion Montage task.
- The Dodge ability listens for semantic invulnerability Begin/End events and owns the active invulnerability-effect handle. Montage completion, interruption, cancellation, `EndPlay()`, or a late event all converge through `EndAbility()`, which ends tasks and removes that effect.
- `UAnimNotifyState_ActionDodgeCancelWindow` and `UAnimNotifyState_DodgeInvulnerability` are separate reflected types in one combat-action-window source group. They require only an ASC-capable mesh owner and send Gameplay Events; they do not mutate Attributes, Gameplay Tags, or ability state directly.

### Stylized Player Presentation

- The local player fixture keeps `SK_Character_Hero_Knight_Male` on `SKEL_Character_Dungeon`. `Weapon_R` is a socket below `hand_r`; `BP_Player` attaches the display-only `WeaponMesh` there with `SM_Wep_Ornate_Sword_02`.
- The weapon is presentation only in this slice: it has no gameplay collision, overlap, physics, equipment state, or trace ownership. The existing ability-owned forward sphere sweep remains the only light-attack hit delivery path.
- `AM_Light_Attack01_Sword` plays the retargeted root-motion sequence `Anim_SAS_V2_ComboAttack02_01_Root` through `DefaultGroup.DefaultSlot`. `ABP_Player_Dungeon` uses `Root Motion from Montages Only`, so the Montage drives the Character's tested forward movement without a new movement component or Motion Warping contract.
- The retargeted Sequence contains no light-attack Notify. The Montage contains one `UAnimNotify_LightAttackHit`, which emits the existing semantic event; the ability still owns the one-hit guard, sweep, cost, and target GameplayEffect.
- The Socket, player Blueprint, AnimBP, Montage, GA/GE, input assets, and retargeting assets remain deliberately local mutable authoring WIP. The stable direct sword and retargeted Sequence assets can be versioned separately, but this subset does not recreate the local playable fixture from a clean checkout.

### Gameplay Tags

- Project tags are config-authored in `Config/Tags/PolyQuestGameplayTags.ini`; there is no native tag singleton or Blueprint tag library in this stage.
- The approved leaf tags are `Ability.Attack.Light`, `Ability.Dodge`, `Event.Attack.Light.Hit`, `Event.Attack.Light.Combo.InputWindow.Begin`, `Event.Attack.Light.Combo.InputWindow.End`, `Event.Attack.Light.Combo.BranchWindow.Begin`, `Event.Attack.Light.Combo.BranchWindow.End`, `Event.Action.CancelWindow.Dodge.Begin`, `Event.Action.CancelWindow.Dodge.End`, `Event.Dodge.Invulnerability.Begin`, `Event.Dodge.Invulnerability.End`, `Event.Input.Canceled`, `Event.Input.Pressed`, `Event.Input.Released`, `Input.AbilitySlot.1` through `.4`, `Input.Aim`, `Input.Dodge`, `Input.PrimaryAttack`, `State.Action.Attacking`, `State.Action.CanCancel.Dodge`, `State.Action.Dodging`, `State.Resource.Stamina.RegenBlocked`, `State.Status.Dead`, `State.Status.Exhausted`, `State.Status.Invulnerable`, and `State.Status.Stunned`.
- Plugin and native test tag sources remain engine/plugin-owned and are not part of the PolyQuest taxonomy.

## Not Yet Established

The following remain future stage contracts:

- Enemy ASC topology and StateTree-to-GAS intent requests.
- Charged/sprint attacks, player death, GameplayCues, generic hit-resolution, and nonlinear or multi-weapon combo-extension contracts.
- Team filtering, weapon collision, multi-hit windows, persistence ownership, and multiplayer/PlayerState ownership.
- Weapon equipment, Ability-grant/revocation, multi-weapon Loadout switching, additional Skeleton/animation, and Motion Warping topology.

These decisions belong to their owning roadmap stages; they are not implied by the TODO-00B foundation.
