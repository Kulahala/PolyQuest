# PolyQuest Architecture

## Current Verified State

PolyQuest is a UE 5.8 Windows C++ project created from the Third Person template. The only runtime module is `PolyQuest`.

The game module currently declares these public dependencies:

```text
Core, CoreUObject, Engine, InputCore, EnhancedInput, AIModule,
StateTreeModule, GameplayStateTreeModule, GameplayAbilities,
GameplayTags, GameplayTasks, UMG, Slate
```

The live UE 5.8 editor resolves the GameplayAbilities plugin, and the module links the GAS runtime dependencies. PolyQuest has completed its first single-player GAS foundation: the active player exposes an ASC, a four-attribute AttributeSet, and a project-owned Gameplay Tag config source. No gameplay ability or effect has been authored yet.

## Generated Template Boundary

`Source/PolyQuest/` contains the base Third Person classes plus generated `Variant_Combat`, `Variant_Platforming`, and `Variant_SideScrolling` sample code. Those variants are template content, not PolyQuest-owned combat, input, AI, save, or ability contracts.

`APolyQuestCharacter` and `BP_ThirdPersonCharacter` remain compatibility fixtures. The active player route is `APlayerCharacter -> ABaseCharacter -> BP_Player`, configured through `BP_GameMode` and `BP_PlayerController`. New product gameplay must be introduced through a documented PolyQuest stage rather than by extending a generated variant opportunistically.

## GAS Core Contract

### Actor And Attribute Ownership

- `ABaseCharacter` owns one `UAbilitySystemComponent` and one `UCharacterAttributeSet` default subobject.
- The AttributeSet is registered exactly once through `AddAttributeSetSubobject(...)` during character construction. Its current fields are `Health`, `MaxHealth`, `Stamina`, and `MaxStamina`, initialized to `100.0f`.
- In the current single-player boundary, `OwnerActor == AvatarActor == ABaseCharacter`. `PossessedBy()` initializes the actor info with `InitAbilityActorInfo(this, this)` after the superclass possession path.
- The player-specific camera, movement, look, and jump input layer belongs to `APlayerCharacter`; the base character has no unconditional tick or player input contract.

### Runtime Routing And Input

- `BP_GameMode` is the active default GameMode and selects `BP_Player` as the default Pawn and `BP_PlayerController` as the PlayerController.
- `APolyQuestPlayerController` installs `IMC_Default` for normal input and adds `IMC_MouseLook` only when touch controls are not active.
- The old Third Person Blueprint route remains available only as a compatibility fixture and is not the active player route.

### Gameplay Tags

- Project tags are config-authored in `Config/Tags/PolyQuestGameplayTags.ini`; there is no native tag singleton or Blueprint tag library in this stage.
- The approved leaf tags are `Ability.Attack.Light`, `Ability.Dodge`, `Input.Attack.Light`, `Input.Dodge`, `State.Action.Attacking`, `State.Action.Dodging`, `State.Status.Dead`, `State.Status.Stunned`, and `State.Status.Exhausted`.
- Plugin and native test tag sources remain engine/plugin-owned and are not part of the PolyQuest taxonomy.

## Not Yet Established

The following remain future stage contracts:

- Enemy ASC topology and StateTree-to-GAS intent requests.
- Ability activation, grants, cancellation, GameplayEffects, GameplayCues, montage timing, damage, and death contracts.
- Attribute mutation, stamina costs, persistence ownership, and multiplayer/PlayerState ownership.
- Stylized combat weapon, Skeleton, socket, animation, and Motion Warping topology.

These decisions belong to their owning roadmap stages; they are not implied by the TODO-00B foundation.
