# PolyQuest Architecture

This document is the current Native C++ and runtime-config architecture ledger for
PolyQuest. It records stable ownership and contracts, not a stage diary or an
asset-authoring checklist.

## Scope and evidence

The current evidence boundary is Native C++ and tracked project/runtime
configuration: Source/PolyQuest, PolyQuest.uproject, Source/PolyQuest/PolyQuest.Build.cs,
Config/DefaultEngine.ini, and Config/Tags/PolyQuestGameplayTags.ini. Build,
Unreal Editor, .uasset graph,
PIE, and visual evidence are deliberately outside this ledger unless explicitly
recorded with their own evidence type.

| Label | Meaning |
| --- | --- |
| [Native] | Direct static fact from the current C++ source. |
| [Config] | Direct static fact from a tracked configuration file. |
| [Authored asset] | A referenced or expected asset contract; its actual asset data was not read back in this pass. |
| [Historical Editor readback] | A behavior-relevant authored-asset snapshot recorded by an identified earlier Editor readback; it is not fresh current-asset evidence and must be refreshed after asset edits. |
| [Not verified in this pass] | Intentionally outside the static source/config evidence boundary. |

Unless a paragraph says otherwise, class names, ownership, and lifecycle claims
are [Native]. Static inspection establishes code/config intent only; it is not
evidence of a successful build, Editor readback, PIE behavior, or visual result.

Values exposed as `UPROPERTY`, DataAsset fields, Config entries, or authored
StateTree properties are referenced by their source member name below instead of
being copied as a second tuning table. Their current values belong to the
source/config/asset that owns them; only non-tunable validation rules and
ownership constraints are architectural facts here.

## Contents

- [Runtime topology and ownership](#runtime-topology-and-ownership)
- [GAS, attributes, and character lifecycle](#gas-attributes-and-character-lifecycle)
- [Input, equipment, and world pickups](#input-equipment-and-world-pickups)
- [Melee, defense, and action abilities](#melee-defense-and-action-abilities)
- [Paired execution](#paired-execution)
- [Bow, projectile, and target assist](#bow-projectile-and-target-assist)
- [Camera, lock-on, and floor visibility](#camera-lock-on-and-floor-visibility)
- [Enemy AI and StateTree](#enemy-ai-and-statetree)
- [Reaction, death, feedback, and UI](#reaction-death-feedback-and-ui)
- [Gameplay Tag taxonomy](#gameplay-tag-taxonomy)
- [Non-goals and authored dependencies](#non-goals-and-authored-dependencies)

---

## Runtime topology and ownership

### Project baseline

The project descriptor declares UE 5.8, Windows as its target platform, and one
project `Runtime` module named PolyQuest [Config]. Product genre, visual style,
and network mode are not established by this evidence pass.

| Area | Current source/config fact |
| --- | --- |
| Public module dependencies | Core, CoreUObject, Engine, InputCore, EnhancedInput, AIModule, StateTreeModule, GameplayStateTreeModule, GameplayAbilities, GameplayTags, GameplayTasks, UMG, Slate, SlateCore |
| Private module dependencies | Niagara, MotionWarping |
| Default map | /Game/Maps/Scene01.Scene01 [Config] |
| Editor startup map | /Game/Maps/Scene01.Scene01 [Config] |
| Default GameMode reference | /Game/BP/Game/BP_GameMode.BP_GameMode_C [Config] |
| Gameplay Tag source | Config/Tags/PolyQuestGameplayTags.ini [Config] |

The map and GameMode entries are configuration references. Their placed actors,
Blueprint parent classes, graph wiring, and visual layout remain [Not verified
in this pass] unless they are independently read back from the Editor. A
Native/source scan alone also does not prove permanent retirement of legacy
template maps, Blueprints, or imported assets; that is an Editor/reference
inspection claim.

### Source anchors

| Contract | Primary Native paths |
| --- | --- |
| Character and GAS base | Source/PolyQuest/Public/Character and Source/PolyQuest/Public/AbilitySystem |
| Player input, camera, and lock-on | Source/PolyQuest/Public/Character/Player |
| Equipment and pickups | Source/PolyQuest/Public/Combat/Equipment |
| Melee and execution | Source/PolyQuest/Public/Combat/Melee, Source/PolyQuest/Public/Combat/Execution, Source/PolyQuest/Public/AbilitySystem/Abilities |
| Projectile and targeting | Source/PolyQuest/Public/Combat/Projectile |
| AI and floor visibility | Source/PolyQuest/Public/AI and Source/PolyQuest/Public/Environment |
| Feedback and UI | Source/PolyQuest/Public/Combat/Feedback, Source/PolyQuest/Public/Camera, and Source/PolyQuest/Public/UI |

### Runtime ownership map

~~~text
Input assets [Authored asset]
        |
        v
APolyQuestPlayerController ---- creates local HUD and interaction views
        |
        v
APlayerCharacter ---- UWeaponEquipmentComponent ---- UWeaponDefinition assets
        |                         |
        |                         +---- AWorldWeaponPickup transaction boundary
        |
        +---- ABaseCharacter ---- ASC + AttributeSet + melee trace/trail
        |             |
        |             +---- GameplayAbilities / GameplayEffects / GameplayTags
        |
        +---- lock-on, camera, motion warping, pickup candidate arbitration

AEnemyAIController ---- perception, navigation, tactical intent, StateTree
        |
        v
AEnemyCharacter ---- ASC, attributes, poise/death, enemy reaction dispatch
~~~

| Owner | Owns | Does not own |
| --- | --- | --- |
| ABaseCharacter | ASC, UCharacterAttributeSet, melee trace source, melee trail, common ASC lifecycle | Player input, equipment composition, AI intent |
| APlayerCharacter | Camera, lock-on state, Motion Warping, equipment component, pickup candidate selection, player exhaustion lifecycle | A second action-state machine or projectile flight state |
| AEnemyCharacter | Enemy poise transition, death pipeline, enemy reaction dispatch | Player equipment or StateTree tactical policy |
| UWeaponEquipmentComponent | Hand composition, component-granted specs, prepared slots, display components, input-to-spec resolution | World overlap arbitration and pickup prompt presentation |
| AWorldWeaponPickup | Interaction overlap surface, displayed definition reference, grounding, former-owner cooldown | Input handling, UI, GAS mutation, equipment transaction ownership |
| AEnemyAIController | Perception, target/focus, home/tactical movement, attack preparation, execution AI lock | Combat action state authority |
| Gameplay Ability / Effect | Activation, cancellation, transient gameplay state, costs and effects | Persistent parallel controller or StateTree action state |

### Authority rules

- GAS is the only combat-state authority. Abilities, Effects, Tags, Attributes,
  and AbilityTasks own activation, interruption, cost, recovery, and temporary
  restrictions.
- AEnemyAIController and StateTree select and execute tactical intent, but do
  not maintain a parallel action enum or duplicate combat-state machine.
- Presentation channels such as trails, overlays, camera shake, hit-stop, sound,
  and Niagara effects do not create another damage or attribution path.

---

## GAS, attributes, and character lifecycle

### Common character initialization

ABaseCharacter creates these default subobjects:

- UAbilitySystemComponent
- UCharacterAttributeSet
- UMeleeTraceSourceComponent
- UMeleeWeaponTrailComponent

The AttributeSet is registered through AddAttributeSetSubobject. Both
BeginPlay() and PossessedBy() initialize the ASC actor info with the character
as owner and avatar. On authority, PossessedBy() grants StartupAbilities only
after FindAbilitySpecFromClass() confirms that the class has not already been
granted.

ABaseCharacter::EndPlay() clears its common bindings/presentation state and
cancels active abilities before character teardown. Its MoveSpeed attribute
delegate updates CharacterMovement->MaxWalkSpeed; it does not introduce an
unrelated movement-state authority.

### Attribute ownership

| Attribute | Source member | AttributeSet responsibility |
| --- | --- | --- |
| Health | `Health` | Clamp to [0, MaxHealth]; keep Health at zero when the dead state is present |
| MaxHealth | `MaxHealth` | Supplies the Health upper bound; this class has no separate MaxHealth clamp |
| Poise | `Poise` | Clamp to [0, MaxPoise] |
| MaxPoise | `MaxPoise` | Supplies the Poise upper bound; this class has no separate MaxPoise clamp |
| Stamina | `Stamina` | Clamp to [0, MaxStamina] |
| MaxStamina | `MaxStamina` | Supplies the Stamina upper bound; this class has no separate MaxStamina clamp |
| StaminaRegenRateMultiplier | `StaminaRegenRateMultiplier` | Non-negative clamp |
| MoveSpeed | `MoveSpeed` | Non-negative clamp; ABaseCharacter owns CharacterMovement synchronization |

UCharacterAttributeSet initializes and clamps attributes. It does not own the
Exhausted tag lifecycle or dispatch Enemy Stance Break.

Transition ownership:

- Player stamina reaches zero: `APlayerCharacter::OnStaminaAttributeChanged` /
  `BeginExhaustion()` add the Player-owned loose `State.Status.Exhausted` tag,
  apply the configured exhaustion MoveSpeed effect, and enter the exhaustion
  lifecycle. `ExhaustionMinimumDurationSeconds` governs the minimum recovery
  gate. When any blocking action tag (`State.Action.Attacking`,
  `State.Action.Dodging`, or `State.Action.Parrying`) is present, starting that
  gate is deferred; it starts only after none of those tags remains. Normal
  recovery requires both elapsed minimum duration and positive Stamina; Dead
  state and EndPlay clear the lifecycle directly.
  `APlayerCharacter::ClearExhaustionState()` clears the recovery timer and
  pending action-deferral state, removes its own effect handle, and removes only
  the Player-owned loose tag; the effect does not own tag removal.
- Enemy poise changes: `AEnemyCharacter::OnPoiseAttributeChanged` owns poise
  recovery and Stance Break dispatch. A zero crossing is not an AttributeSet-side
  event.
- Health reaches terminal state: character-specific health/death handling owns
  the terminal path; the AttributeSet enforces the zero-health invariant after
  death.

### Lifecycle and async safety

- Ability activation, attribute cost, and temporary state are represented by
  GAS Ability/Effect/Tag lifetimes rather than controller booleans.
- Async callbacks from timers, montages, notifies, traces, or AbilityTasks must
  validate their active object, relevant actor/ASC, token/context, and current
  state before mutating gameplay state.
- ReadyForActivation() is treated as a synchronous re-entry boundary. Code
  after it may restore task state only when the ability remains active.
- Natural completion, cancellation, death, destruction, and teardown converge
  on the owning ability's idempotent EndAbility() cleanup path.

---

## Input, equipment, and world pickups

### Equipment data and runtime composition

UWeaponDefinition is an authored UDataAsset base. It describes a weapon's hand
occupancy, display attachment, locomotion mode, action candidates, default
prepared slots, defense profile, and direct attack tags. It owns no active
runtime state.

| Definition area | Stable Native contract |
| --- | --- |
| Hand slot | MainHandOneHanded, MainHandTwoHanded, or OffHand |
| Display | WeaponMesh, AttachSocketName, DisplayLocationOffset, DisplayRotationOffset, DisplayScale, and WorldPickupDisplayTransform |
| Combat actions | BaseGrantedActions, DefaultPreparedActions, ReusableCombatActions, ExclusiveCombatActions, direct primary/sprint tags, and optional DefenseProfile |
| Melee extension | Trace geometry, BladeSubdivisions, sockets/markers, owner-mesh trace sources, execution montage/range data |
| Off-hand extension | Optional defense override and shield presentation fact |
| Bow extension | Projectile definition plus authored draw/hold/release presentation references |

ReusableCombatActions and ExclusiveCombatActions are currently authoring groups
that merge into one candidate pool. The word Exclusive does not represent an
implemented runtime exclusivity rule.

UWeaponEquipmentComponent exists only on APlayerCharacter. It owns the current
hand composition, component-granted Ability specs, prepared-slot identities and
handles, display/marker components, and input intent resolution.

| Input route | Resolution rule |
| --- | --- |
| Primary and sprint attack | Current MainHand's direct Ability tags |
| Guard and parry | Effective defense profile: OffHand override, then MainHand fallback, then built-in fallback |
| Ability slots 1-4 | Exact prepared spec handle plus handle validity, component ownership, and class identity checks |

#### Weapon locomotion and shield presentation

- `EWeaponLocomotionMode` is a reflected `uint8` enum for MainHand locomotion families only: `Default = 0`, `LightSword = 1`, `HeavySword = 2`, and `Bow = 4`. The explicit Bow value preserves existing serialized Bow assets after ordinal `3` was reserved/deprecated. `UWeaponDefinition::IsValidWeaponDefinition()` accepts only those four values; an OffHand must retain its inherited base mode at `Default`.
- `UWeaponEquipmentComponent::GetResolvedLocomotionMode()` is a pure, stateless `BlueprintPure` MainHand query with zero caching, delegates, ticks, or tag mutations. It returns `Default` for a null or invalid MainHand and otherwise returns the validated MainHand family.
- `UWeaponEquipmentComponent::HasShieldEquipped()` is a separate pure query. It returns true only when the committed `CurrentOffHandWeapon` is a `UOffHandWeaponDefinition` with `bProvidesShieldPresentation == true`.
- `ABP_Player_Dungeon` [Authored asset] consumes both facts: `GetResolvedLocomotionMode()` feeds its `Default`, `LightSword`, `HeavySword`, and `Bow` locomotion branches, while full-body Shield Guard is selected only when `HasShieldEquipped()` is true and exact active `State.Action.Guarding.Shield` is present.

### Input event boundary

`APlayerCharacter::SetupPlayerInputComponent()` binds movement, jump, combat
input, four ability slots, interaction, lock-on, and target-cycle actions. The
declared `LookAction` and `MouseLookAction` are not bound in the current fixed
camera route. Physical keys, mouse buttons, wheel axes, and Mapping Context
contents are [Authored asset] and are not inferred from the Native bindings.

For a combat input intent, the Player records the press time and sends
`Event.Input.Pressed` with the exact `Input.*` tag in the event payload, then
requests the routed Ability. `Completed` and `Canceled` clear the held record
and send `Event.Input.Released` or `Event.Input.Canceled` with the measured held
duration. This layer does not play a montage, spend Stamina, write an Attribute,
or perform a trace directly.

`DodgeSprintAction` is a temporal physical-input arbiter. Its
`DodgeSprintHoldThresholdSeconds` separates a short release (Dodge) from a hold
that resolves to Sprint. The arbiter owns only press timing; the Dodge and
Sprint Abilities own GAS tags, costs, movement, and cleanup. On a non-sprint
`Input.PrimaryAttack`, the request order is Front execution, Backstab execution,
then the equipped MainHand direct route. A Sprint Attack is attempted before
that execution branch when the movement/input predicate qualifies.

### Defense profile contract

`UDefenseProfileDefinition` is tag-and-class mapping data, not a second defense
state machine. The effective lookup order is OffHand profile, MainHand profile,
then built-in Guard/Parry defaults. When a profile is authored, its Guard and
Parry tags must be valid and distinct, and the corresponding component-granted
Ability classes must carry the exact profile tags; malformed mappings fail
equipment preflight rather than borrowing an unrelated StartupAbility.

### Equipment transaction boundary

`EquipWeapon()` and `TryEquipWorldPickup()` are the equipment mutation entry
points. They use a transactional sequence:

~~~text
preflight -> snapshot -> teardown old grants/display -> apply target composition
          -> either commit new state or rollback the complete old composition
~~~

Preflight validates the current character/ASC state, definition and sockets,
composition constraints, and the relevant Ability identities. Direct
`EquipWeapon()` preserves the other hand while building its target composition;
it does not normalize a cross-slot conflict, so a two-handed MainHand with an
existing OffHand (or an OffHand with an existing two-handed MainHand) is rejected
by preflight. `TryEquipWorldPickup()` first normalizes through
`BuildTargetCompositionForIncoming()`: an incoming one-handed MainHand replaces
the MainHand and retains OffHand unless it is replacing a current two-handed
MainHand; an incoming two-handed MainHand clears OffHand; and an incoming
OffHand replaces a current two-handed MainHand with
`UnarmedFallbackDefinition` (or fails if that fallback is not configured).

At Player BeginPlay, `DefaultEquippedWeapon` is passed through direct
`EquipWeapon()`. If it is absent, Native code logs a warning and the Player
starts without melee capability. `UnarmedFallbackDefinition` is not a startup
default; this pass observes it only in the world-pickup two-handed-to-OffHand
normalization above.

For a world pickup transaction, displaced drops start with NoCollision.
Only a full success changes them to QueryOnly, consumes the source pickup, and
destroys it. A failed apply or drop-grounding operation restores the previous
composition and leaves the source pickup intact.

### World pickup and interaction boundary

AWorldWeaponPickup::WeaponDefinition is an editable definition reference and
can be changed through SetWeaponDefinition(). It is the pickup's current
represented definition, not an immutable runtime value.

- The actor owns a query-only interaction sphere, display component, grounding,
  and re-entry protection.
- Overlaps retain weak Player references and notify the Player; they do not
  activate abilities or change equipment directly.
- A former owner is rejected for `FormerOwnerRejectDuration` to avoid
  immediately collecting a newly dropped item.
- `StageDisplacedDrops()` passes the Native constant `FixedClearance` into
  `FWorldPickupGrounding::TryComputeGroundedRootLocation()` for definitions with a
  `WeaponMesh`. The helper itself accepts a generic `Clearance` parameter and
  calculates the root location that places the transformed local bounds corners
  along the valid ground normal. Without a `WeaponMesh`, `StageDisplacedDrops()`
  directly offsets the hit point along the hit normal by `FixedClearance`.
  `ProjectLocationToGround()` is an independent static trace helper that offsets
  by `NormalOffsetDistance` along the normal, with no active callers in Native
  source. Invalid grounding reports failure to the surrounding transaction so
  it can roll back.

APlayerCharacter owns the weak pickup candidate set and its current candidate
snapshot. It filters invalid candidates through CanInteract, resolves the
nearest valid pickup with deterministic distance/name tie-breaking, and avoids
an unconditional tick scan. UWorldInteractionPromptWidget, created by
APolyQuestPlayerController, is a passive view: it owns neither candidate
arbitration, input, GAS state, nor equipment mutation.

---

## Melee, defense, and action abilities

### Damage route and trace ownership

There is one Native melee damage route:

~~~text
UAbilityTask_MeleeTraceWindow
    -> FMeleeHitResolver
    -> Damage GameplayEffect spec
    -> target AbilitySystemComponent
~~~

UMeleeTraceSourceComponent selects a trace branch by owner context rather than
walking a cross-branch fallback chain:

- An equipped Player resolves the current MainHand melee definition. Its
  optional named owner-mesh sources are resolved by the definition; the
  display-mesh socket/marker path is the equipment component's internal marker
  implementation for the default source. If this equipped branch is invalid,
  it fails closed and does not fall through to a static or legacy source.
- An owner without an equipment component may use a valid
  `StaticMeleeWeaponDefinition` and its display-mesh sockets.
- Only when neither equipped nor static-definition geometry is present does the
  legacy component-name fixture apply, and it supports only an unnamed source.

An invalid named/static source fails closed rather than silently selecting an
unrelated branch. `UMeleeWeaponDefinition` supports `FOwnerMeshMeleeTraceSource`
profiles (`TraceSourceName`, `OwnerMeshSocketName`,
`BladeBaseMarkerRelativeLocation`, `BladeTipMarkerRelativeLocation`) with
`DefaultOwnerMeshTraceSourceName`, allowing owner-mesh Unarmed weapons to
configure multiple contact sources (such as `RightFist` and `LeftFist`).
`UWeaponEquipmentComponent` stores transient markers in source-keyed maps
(`MainHandBladeBaseMarkers`, `MainHandBladeTipMarkers`).

`UAbilityTask_MeleeTraceWindow` executes across three explicit phases:
1. **Phase 1 (Endpoint validation)**: All-or-nothing sampling across all
   configured sources; any invalid or non-finite source fails closed.
2. **Phase 2 (Trail forwarding)**: Source-keyed trail updates to
   `UMeleeWeaponTrailComponent`, which manages transient child
   `UNiagaraComponent` instances per named source with source-keyed weak
   requesters to prevent cross-source trail clobbering.
3. **Phase 3 (Multi-source sweep & deduplication)**: Sweeps all valid sources on
   the project `MeleeTrace` channel (`ECC_GameTraceChannel1`). Its window-scoped
   `DeliveredTargets` set enforces shared deduplication across all sources,
   guaranteeing that an actor overlapping multiple sweeps in the same window
   receives damage at most once.

### Shared combat gates

FMeleeHitResolver validates source/target identity, team, ASC availability,
dead and invulnerable state before applying the Damage GameplayEffect. It also
contains the melee Guard/Parry gates. A valid defensive response consumes or
redirects the incoming melee hit through its intended stamina/poise effect
contract instead of creating a second direct damage path.

The defensive order is Parry before Guard. The Native predicates
`ParryHalfArcDegrees` and `GuardHalfArcDegrees` define the shared front-arc
geometry. A live `State.Action.ParryActive` window consumes the contact and
applies the configured negative `Data.Poise.Parry` payload to the attacker. A
live `State.Action.Guarding` window consumes the contact and routes
the configured `Data.Stamina.GuardDamage` payload to the Player. Projectile
delivery uses the Guard gate only; it does not invent a projectile-specific
Parry path.

Projectile collision uses FCombatProjectileHitResolver, not the melee resolver,
but follows the corresponding team, living, invulnerability, and Player Guard
eligibility rules for projectile delivery.

### Action ability boundaries

- Action classes own their activation tags, temporary input/movement blocks,
  montage/task delegates, costs, and cleanup.
- UStaminaActionAbility is the common stamina-action base. It permits a
  committed action to deplete the attribute to zero under the AttributeSet
  clamp, then applies the common regeneration-delay contract on termination.
- Player Guard and Parry resolve against the effective defense profile instead
  of a separate shield-only state machine. Authored windows, montages, Effects,
  and timings remain [Authored asset] unless read back.
- Combo, charged, sprint, dodge, and prepared actions use their own Ability
  lifetime and cancellation tags. No controller/StateTree action enum mirrors
  these states.

#### Prepared melee skill lifecycle (`UPlayerMeleeSkillAbility`)

- `UPlayerMeleeSkillAbility` is the narrow prepared melee-skill lifecycle (e.g. `GA_Skill_Whirlwind`):
  - Its prepared slot activates its exact granted `FGameplayAbilitySpecHandle`.
  - The Ability confirms its tracked Montage before its single Cost and Cooldown commit, then arms the trace task, Dodge-cancel, and rate-window listeners.
  - Action tags, Guard cancellation, trace task, playback rate restoration, Montage stop, and input-block cleanup converge through one idempotent `EndAbility()`.
  - The Ability CDO receives native capability and teardown tags (`Ability.Action.CancelableBy.Dodge`, `Ability.Action.CancelableBy.Defense`, `Ability.Action.CancelableBy.Reaction`, `Ability.Action.Teardown.OnUnpossess`) in `PostLoad()` and `PostCDOCompiled()` so Blueprint default-tag serialization cannot strip lifecycle contracts.
  - Concrete identity (such as `Ability.Skill.Whirlwind`) and Cooldown GE tags remain authored on GA/GE assets.

#### Player RateWindow policy

`ULightAttackAbility` and `UPlayerMeleeSkillAbility` each consume
`Event.Action.RateWindow.Begin/End` only when both event actors are their
owning Avatar and `Payload.OptionalObject` exactly matches their tracked active
Montage. A valid Begin with a positive
`EventMagnitude` applies that magnitude as the current Montage play rate.
Successive or overlapping valid Begins use a last-one-wins policy: the most
recent override remains active until the local active-window count returns to
zero. A valid End decrements that count only when it is positive; restoration
occurs when the count reaches zero, or through the existing transition and
terminal cleanup paths. Each Ability restores its fixed native rate rather
than a captured pre-window baseline.

This is a consumer-local policy for these two Player Abilities. It does not
adopt `FAbilityMontageRateWindowLifecycle`, does not imply all Player
Abilities support RateWindow, and does not complete the conditional
Player/Enemy lifecycle unification in `TODO-07B8-C`.

#### Key Ability cancel, commit, and Montage cleanup contracts

- **`UDodgeAbility`**: Uses `UAbilityTask_PlayMontageAndWait` instance-bound callbacks (`OnCompleted`, `OnInterrupted`, `OnCancelled`). It does not bind the global `OnMontageEnded` multicast, preventing a re-triggered dodge from ending its successor, and does not terminate at `OnBlendOut`.
- **`UPlayerGuardAbility`**: Requires grounded movement, positive Stamina, held Guard input, and absence of blocking states. Only a confirmed active Guard Montage applies MoveSpeed and `StaminaRegenRateMultiplier` Duration GameplayEffects and cancels Sprint. Absorbing a contact at zero Stamina sends `Event.Reaction.Player.GuardBreak`.
- **`UPlayerGuardBreakAbility`**: Gameplay-Event triggered, owns `State.Status.Stunned`. Once its Montage is active, it locks movement and cancels Guard, Sprint, and attack abilities. `EndAbility()` restores walking only for a live, non-destroying character.
- **`UPlayerParryAbility`**: Pays `GE_Parry_Cost` on startup via `CommitStaminaCostOnly`. Only after its Montage is active does it lock movement, cancel Sprint/Guard/Attack, and expose the Notify-driven `State.Action.ParryActive` window. Natural completion of the full Montage is the sole cooldown commit point (`GE_Parry_Cooldown` with `Cooldown.Parry`); montage interruption, death, or teardown never commits cooldown.
- **`UChargedAttackAbility`**: Derives from `UStaminaActionAbility`, owns `Ability.Attack.Charged` and `State.Action.Attacking`, and adds `State.Action.Charging` prior to release. Latches valid Dodge/Defense cancel windows across the hold pause; commits Cost on normal release, evaluates held duration for damage/Poise scaling, and resumes Root Motion. Natural completion, cancellation, Dodge, and teardown converge through `EndAbility()`.

### Motion Warping

APlayerCharacter owns the sole UMotionWarpingComponent. Ordinary melee action
Abilities attempt a one-shot lock-on snapshot at their first legal warp
evaluation; when valid, the snapshot is cached. They use
FMeleeMotionWarpingLifecycle for geometry validation. A warp target is only
written when the Player and target are grounded and its distance/direction
contract is valid. It is cleaned up on cancellation, death, destruction,
unpossession, and EndPlay.

The Native motion-warp configuration requires finite values and
`MinTriggerDistance <= WarpStopDistance <= MaxTriggerDistance`, with a valid
warp target name and a maximum angle in `[0, 180] deg`. A target at the exact
stop distance is a deliberate no-op; the helper preserves the Player's Z and
does not emit an identity correction.

Execution does not reuse ordinary action Motion Warping. It has a separate
one-shot snap contract described below.

---

## Paired execution

### Session ownership and entry conditions

UPlayerFrontExecutionAbility and UPlayerBackstabExecutionAbility are
server-authoritative Player abilities. Each creates one transient
UExecutionLockContext and synchronously asks the target's
UEnemyVictimExecutionAbility to accept the session. A context captures the
specific actors, ASCs, abilities, request, activation token, and release state;
stale callbacks fail closed.

The two entry paths have distinct eligibility rules:

| Path | Core condition |
| --- | --- |
| Front execution | Valid living target, execution distance/geometry, and real zero-Poise Stance Break state |
| Backstab execution | Valid living target, `MaxBackAngleDegrees` rear geometry, and its dedicated Stance Break compatibility check |

Both paths snapshot their equipped melee definition and montage/range data.
Missing, invalid, or changed source data rejects the action rather than falling
back to an unrelated weapon, CDO, or target.

### Paired lock state

| Side | Ability identity and runtime state |
| --- | --- |
| Player | Player execution ability and State.Action.Execution.PlayerLocked |
| Enemy victim | Ability.Action.Execution.Victim, State.Action.Execution.VictimLocked, State.Status.Invulnerable, State.Status.Stunned, and movement/jump restrictions |

The victim ability identity and its active lock intentionally use different
namespaces: `Ability.Action.Execution.Victim` identifies the ability, while
`State.Action.Execution.VictimLocked` identifies the paired-lock lifetime.

The victim ability owns target-side movement/AI lock and cleanup. The Player
does not write target state tags directly. AEnemyAIController pauses the
locked Pawn's StateTree/navigation/focus progression and cannot issue fresh
tactical movement until a legal release or recovery path restores it.

The semantic ingress tags are `Event.Action.Execution.Request.Front`,
`Event.Action.Execution.Request.Backstab`, and
`Event.Action.Execution.Request.VictimStart`; the active Player montage emits
`Event.Action.Execution.Hit` and `Event.Action.Execution.Request.Release`.
Each request is checked against the active montage, source/target identity,
session, and activation token before it can advance the pair.

### Snap alignment

FExecutionSnapAlignment is a one-shot, swept alignment step after the paired
handshake and before the Player montage. It validates a finite horizontal range
and target direction, temporarily ignores the target capsule during the move,
and restores collision/transform on failure.

| Execution kind | Player snap location | Facing |
| --- | --- | --- |
| Front | TargetLocation + TargetForward2D * SnapDistance | -TargetForward2D (toward the target) |
| Backstab | TargetLocation - TargetForward2D * SnapDistance | +TargetForward2D (same direction as the target) |

`FExecutionSnapAlignment::IsExecutionDistanceRangeValid()` enforces all of the
following rules:
- All distance inputs (`MinDistance`, `MaxDistance`, `SnapDistance`) must be finite;
- `MinDistance >= 0.0f`;
- `MinDistance < MaxDistance`;
- `SnapDistance > 0.0f`;
- `MinDistance <= SnapDistance <= MaxDistance`;
- `MaxDistance` is bounded by `NativeMaxDistanceHardCap`.

The helper normalizes the target forward vector in XY, uses the Player's current Z for the
snap location, and derives the facing vector from that normalized planar
direction. The snap is independent of the ordinary Motion Warping lifecycle. A
blocked sweep or failed final transform tolerance ends the execution through the
normal cleanup path.

### Semantic event and terminal flow

~~~text
paired lock accepted
    -> VictimStart (presentation handoff)
    -> Hit (one authorized melee hit)
    -> Release (terminal decision and recovery/death handoff)
~~~

- Hit remains on FMeleeHitResolver -> Damage GameplayEffect; an execution
  context may bypass the target's temporary invulnerability only when it
  authorizes the exact active source, target, victim ability, and token.
- A lethal authorized hit adds State.Status.DeathPending. On legal Release,
  CommitExecutionDeath() enters SetDeadState -> HandleDeath ->
  CancelAllAbilities -> StartDeathRagdoll.
- A non-lethal legal Release restores the victim's recoverable movement/AI/poise
  state and may dispatch the existing Enemy Launch event.
- Montage end, cancellation, task startup failure, target invalidation, death,
  destruction, unpossession, and EndPlay all converge on idempotent context,
  task, collision-ignore, tag, and delegate cleanup.

Victim montages and notify placement are [Authored asset]; their actual graph
or timing cannot be inferred from the C++ contract alone.

---

## Bow, projectile, and target assist

### Projectile definition

UProjectileDefinition is an authored DataAsset that contains display, collision,
damage, target-assist, homing, and flight-trail parameters. It does not store an
active target or flight state.

| Contract | Source members |
| --- | --- |
| Movement and lifetime | `InitialSpeed`, `MaxSpeed`, `LifespanSeconds`, `CollisionRadius` |
| Target assist | `bEnableTargetAssist`, `TargetAssistMaxDistance`, `TargetAssistMaxAngleDegrees`, `TargetAssistMaxHeightDelta`, `TargetAssistMaxPitchDegrees` |
| Limited homing | `bEnableLimitedHoming`, `HomingStartDelaySeconds`, `HomingDurationSeconds`, `HomingTurnRateDegreesPerSecond`, `HomingMaxTotalTurnDegrees` |
| Flight trail | `FlightTrailSystem`, `FlightTrailSocketName`, `FlightTrailFinishTimeoutSeconds` |

`UProjectileDefinition::IsValidProjectileDefinition()` owns the finite/positive
checks and the dependency that limited homing requires target assist. The
projectile uses the authored turn budget as a finite deflection limit; whether
that permits a rear-hemisphere turn is an asset choice, not a second targeting
rule.

### Bow action boundary

`UBowDrawFireAbility` is an `InstancedPerActor`, `ServerOnly` Ability. It owns
the Bow's `Ability.Attack.Primary` / `State.Action.Attacking` lifetime and adds
`State.Action.Charging` only after an identity-validated DrawReady event. On
release it removes the charging state, resolves the launch direction and one
target snapshot, then spawns/initializes the projectile. Draw/Hold/Release
montage sections, speed Effects, projectile class overrides, and notify timing
are [Authored asset].

### Launch snapshot and target assistance

FCombatProjectileTargeting is a narrow read-only helper, not a general Lock-On
framework. It chooses at most one living hostile Character with a valid ASC
after filtering self, same team, dead/invulnerable state, distance, angle,
  height, pitch, camera-forward, viewport projection, and ECC_Visibility
  obstruction. Its Bow target-assist screen margin is the targeting filter's
  `ScreenMarginRatio`, separate from Player lock retention.
Equal candidates are ordered by angle, then distance, then stable name.

At Bow release, `FCombatProjectileLaunchRequest` stores the projectile
definition, source/ASC, combat payload, initial flight direction, selected
target, release-time `InitialTargetAimPoint`, and homing parameters.
`ACombatProjectile` derives its initial velocity from `InitialFlightDirection`;
its current Native flight logic does not read `InitialTargetAimPoint`. During
limited homing it validates the same cached target actor and recomputes that
actor's current aim point. It does not search again or retarget after launch.
Lock-On may contribute one validated snapshot at release, but Lock-On does not
own the projectile's flight target.

### Flight and hit delivery

ACombatProjectile owns a collision sphere and UProjectileMovementComponent.
It uses zero gravity and adjusts velocity direction for limited homing rather
than manually relocating the actor. Invalid target, timeout, or exhausted turn
budget stops steering and leaves the projectile flying straight. A valid pawn
hit or blocking impact disables the applicable collision/movement lifecycle.
A successfully resolved pawn hit is delivered once and enters terminal trail
cleanup; a rejected pawn is ignored so flight can continue. The Native path has
no AoE explosion or penetration-through-target contract.

Flight-trail components are presentation-only. A terminal trail may detach into
world space and wait for its Niagara completion callback or its configured
timeout before final actor cleanup. Trail assets and rendering are [Authored
asset] / [Not verified in this pass].

---

## Camera, lock-on, and floor visibility

### Fixed oblique camera

APlayerCharacter constructs a fixed world-oriented oblique setup through the
`CameraBoom` and `FollowCamera` properties `TargetArmLength`,
`RelativeRotation`, `CameraLagSpeed`, `CameraLagMaxDistance`, `FieldOfView`, and
`bDoCollisionTest`. The constructor disables pawn-control rotation and SpringArm
collision testing; it does not bind ordinary controller look input as a separate
free-look route. Exact tuning belongs to the constructor, not this ledger.

### Lock-on boundary

APlayerCharacter owns one weak AEnemyCharacter lock target.

- The authored `LockOnAction` performs strict viewport, cursor-nearest
  acquisition; its physical binding is [Authored asset].
- The authored `TargetCycleAction` uses deterministic screen-space ordering; its
  physical binding is [Authored asset].
- Acquisition, cycling, and ordinary death retargeting remain strict viewport
  checks.
- `LockOnTraceChannel` handles line-of-sight. Hidden-actor checks use the bounded
  retry path; visible obstruction, a world hit, or retry exhaustion fails
  closed.
- `LockOnRetentionMarginRatio` is retention-only hysteresis. It does not relax
  initial acquisition, cycling, or projectile target-assist rules.
- `LockOnOcclusionGraceDuration` is the obstruction grace owned by the Player.
  Paired execution may retain
  its already-owned valid target through its explicit lock conditions, without
  relaxing team, ASC, death, or retention checks for other targets.

Action-facing remains owned by the relevant action ability/root-motion contract.
Lock-On is not a global authorization to alter projectile targeting or action
state.

### Action-facing block

`State.Block.Facing` is the Config-registered GAS state used when an active
action must retain its authored facing. `UEnemyStanceBreakAbility`,
`UEnemyMeleeAbility`, `UEnemyHitReactionAbility`, and
`UEnemyLaunchReactionAbility` own the tag through
`ActivationOwnedTags`; GAS adds and removes those contributions with the
Ability lifetime. The Enemy Melee, Big Reaction, and Launch Reaction owners
also carry `Ability.Action.Teardown.OnUnpossess`, while Stance Break retains
its existing teardown selector. `UEnemySmallHitReactionAbility` and
`UEnemyVictimExecutionAbility` do not own the facing-block tag.

`APlayerCharacter` is a consumer rather than an owner: its action-facing mode
and locked-locomotion path treat `State.Block.Facing` as an action-owned
rotation condition. Its dedicated NewOrRemoved tag delegate only refreshes the
rotation mode and is bound/unbound with the existing ASC lifecycle; it does not
start or cancel Sprint.

### See-through occlusion boundary

`APlayerCharacter::UpdateSeeThroughOcclusion()` is the Native bridge for the
camera-to-player visibility test. It sweeps a sphere on `SeeThroughTraceChannel`
from a near-clipped camera point to the Player chest, ignores the Player and its
attached actors, writes `PlayerPosition` and `CeilingRadius` directly, and
interpolates only `TunnelRadius`. Sweep radius, near-clip offset, tunnel radius,
open/close interpolation speeds, and ceiling radius are owned by
`SeeThroughSweepRadius`, `SeeThroughNearClipOffset`, `MaxTunnelRadius`,
`TunnelRadiusOpenInterpSpeed`, `TunnelRadiusCloseInterpSpeed`, and
`MaxCeilingRadius` respectively.

The MPC reference and the material/shader interpretation of those parameters are
[Authored asset] / [Not verified in this pass]. This code path changes
presentation visibility only; it does not disable collision or change combat
target validity.

### Floor visibility boundary

`AFloorVolume` and `AFloorTriggerVolume` provide Native floor visibility management:

- **`AFloorVolume::SetFloorActive()` Native contract**:
  - Sets `TargetCutoffZ` from `ActiveCutoffZ` or `InactiveCutoffZ` and pushes the `FloorCutoffZ` scalar to `PlayerGlobalsMPC`.
  - Toggles `SetActorHiddenInGame(!bActive)` directly on `ManagedInteriorActors`.
  - When `bHideStructuralActorsWhenInactive` is enabled, structural actors are made visible on activation, and are hidden on deactivation (either immediately or when cutoff interpolation finishes).
  - This Native path does not call collision modification functions (`SetCollisionEnabled`, etc.); it operates purely through actor visibility and the MPC scalar.
- **`AFloorVolume::GatherContainedActors()` classification**:
  - Automatically gathers actors contained within `BoundsBox` on `BeginPlay()`, strictly excluding `APlayerCharacter`, `AController`, `AInfo`, `ALight`, `APostProcessVolume`, other `AFloorVolume`, and actors tagged `Floor.Ignore`.
  - Classifies contained actors into structural (`ManagedStructuralActors`) vs interior (`ManagedInteriorActors`) using explicit tags (`Floor.Structural` / `Floor.Interior`), interior actor/mesh keywords, presence of `ULightComponent`, positive structural keywords (`Wall`, `Floor`, `Arch`, `Ceiling`, `Roof`, `Stair`, `Pillar`, `Column`, `Bridge`), and specific material names (`M_Tiling_Master`, `M_StoneWall`, `M_Decorative_Arches`).
- **`AFloorTriggerVolume` Native contract**:
  - On `BeginPlay()`, if `TargetFloorVolume` is not explicitly assigned, iterates `AFloorVolume` instances in the current `UWorld`, preferring an exact `FloorIndex == TargetFloorIndex` match and otherwise selecting the spatially nearest `AFloorVolume`. If unresolved, logs a warning and subsequent overlap events will not execute a transition.
  - Listens only to `APlayerCharacter` overlap on its `TriggerBox`.
  - Upon overlap, calls `TargetFloorVolume->SetFloorActive(TargetFloorIndex >= TargetFloorVolume->FloorIndex)`.

The material-side interpretation of `FloorCutoffZ`, DitherTemporalAA shading, actual level placement of volumes/triggers, real floor indexing, multi-trigger hysteresis setups, and visual fade appearance are [Authored asset] / [Not verified in this pass].

---

## Enemy AI and StateTree

### Controller and attack ownership

AEnemyAIController owns enemy perception, target/focus, home location, attack
cooldown, pending attack profile, approach/reposition behavior, execution lock,
and StateTree component lifecycle. It is the tactical owner, while GAS is the
action-state owner.

UEnemyAttackProfile provides authored montage, damage Effect, range, cooldown,
and guard-stamina data. UEnemyAttackSet supplies weighted candidate entries and
engagement range. The controller snapshots its selected pending profile while
approaching rather than rolling a new attack every update. A requested attack's
active/completed state is determined by GAS tags such as
State.Action.Attacking, not by a second AI action enum.

`AEnemyAIController::UpdateControlRotation()` queries the controlled Enemy ASC
for `State.Block.Facing`. While the tag is present it does not hand ordinary
Gameplay Focus back to `Super::UpdateControlRotation()` or write an Actor yaw.
Root Motion still owns its existing focus-clear branch; when Root Motion ends,
the recovery handoff waits until the facing block has cleared.

### Attack profile and tactical movement

- `UEnemyAttackProfile` is immutable authored data for one attack: a valid
  `AttackMontage`, `DamageGameplayEffectClass`, finite positive `AttackRange`,
  finite non-negative `CooldownAfterAttack`, and finite non-negative
  `GuardStaminaDamage`. It owns no selector, timer, pending decision, or active
  runtime state.
- `UEnemyAttackSet` owns a positive `EngagementRange` and weighted profile
  entries. Its validation rejects empty entries, invalid profiles, duplicate
  profile references, non-finite/non-positive weights, and aggregate weight
  overflow. Weighted selection filters by the set's engagement range and valid
  entries, but does not pre-filter a profile merely because its own
  `AttackRange` is shorter; approach movement can close that gap.
- `UEnemyAIProfile` owns immutable spacing/leash data through
  `PreferredCombatDistance`, `LateralRepositionDistance`,
  `RepositionAcceptanceRadius`, `RepositionRetryDelay`, `LeashRadius`, and
  `ApproachTimeout`. Values must be finite and valid, and preferred combat
  distance cannot exceed the AttackSet `EngagementRange`.
- On possession, `AEnemyAIController` validates AttackSet, AIProfile, and the
  enemy Poise-recovery configuration, then caches `EngagementRange` as the
  instance-only `MeleeRange` and caches the leash radius. The range check is a
  horizontal actor-center distance; it is not derived from capsule radii.
- `PreparePendingAttackProfile()` selects once and retains the profile during
  approach. `TryRequestApproach()` uses that profile's `AttackRange` as the
  MoveTo acceptance radius, tracks the moving target, and clears the decision
  on navigation failure or the configured timeout. `TryRequestMeleeAttack()`
  directly calls the enemy ASC's `TryActivateAbilitiesByTag()` for
  `Ability.Attack.Enemy.Melee`; it does not send a `GameplayEvent`.
- Cooldown reposition is Controller-owned. The target-relative point is
  clamped so its distance from the target stays within `EngagementRange`; the
  controller alternates left/right, retries one failed request on the same
  side, then switches side, while enforcing `RepositionRetryDelay` and one
  active MoveTo. The temporary tactical pace override is restored on every
  completion, failure, cancellation, death, and execution lock.

### StateTree boundary

Current Native integration is supplied by the StateTree tasks, conditions, and
`AEnemyAIController` lifecycle:

- `AEnemyAIController` owns the `UStateTreeAIComponent` start/stop boundary and
  starts logic only after valid AttackSet, AIProfile/leash constraints, and Poise
  recovery configuration pass validation. The Controller and StateTree select
  tactical intent; GAS remains the action-state authority.

#### Native StateTree task lifecycle semantics

- **`FEnemyStateTreeTask_BeginAlert`**: Clears `PendingAttackProfile`, stops
  movement, and restores valid target focus; succeeds immediately. It does not
  play an animation Montage.
- **`FEnemyStateTreeTask_PrepareMeleeAttack`**: Validates living state, absence
  of hit reaction / active attack, attack cooldown expiry, valid combat target,
  melee range, and leash boundaries. On validation failure, it clears
  `PendingAttackProfile` and fails; on success, it selects and caches one
  weighted `PendingAttackProfile` exactly once without re-rolling during approach.
- **`FEnemyStateTreeTask_ApproachSelectedMeleeAttack`**: Retains the cached
  `PendingAttackProfile` and drives dynamic MoveTo toward the target using
  `SelectedProfile.AttackRange` as the acceptance radius. On timeout, navigation
  failure, or invalidated controller/target state, it clears
  `PendingAttackProfile` and fails; on exit, it stops the active approach MoveTo.
- **`FEnemyStateTreeTask_RequestMeleeAttack`**: Directly requests ability
  activation through the Controller and ASC (`Ability.Attack.Enemy.Melee`), then
  observes the ASC-owned `State.Action.Attacking` tag. It must not report
  completion if attack activation was rejected or not observed; it succeeds only
  after an observed attack finishes.
- **`FEnemyStateTreeTask_RepositionDuringCooldown`**: Responsible only for
  tactical movement during attack cooldown. It returns to `Wait` upon cooldown
  expiration or when temporarily limited by the minimum request interval; on
  exit, it stops active reposition MoveTo navigation.

#### Native StateTree conditions

| Native StateTree Condition | Queried Controller Method / Expression | Purpose |
| --- | --- | --- |
| `FEnemyStateTreeCondition_HasValidTarget` | `HasValidCombatTarget()` | Living, valid combat target exists |
| `FEnemyStateTreeCondition_IsTargetInMeleeRange` | `IsCombatTargetInMeleeRange()` | Target horizontal center distance <= `MeleeRange` |
| `FEnemyStateTreeCondition_IsAttackOnCooldown` | `IsMeleeAttackOnCooldown()` | Melee attack cooldown active |
| `FEnemyStateTreeCondition_CanRequestReposition` | `CanRequestCooldownReposition()` | Cooldown active, target valid, and interval elapsed |
| `FEnemyStateTreeCondition_IsAttackReady` | `!IsMeleeAttackOnCooldown()` | Melee attack ready off cooldown |
| `FEnemyStateTreeCondition_IsTargetOutsideMeleeRange` | `!IsCombatTargetInMeleeRange()` | Target horizontal center distance > `MeleeRange` |
| `FEnemyStateTreeCondition_HasPendingMeleeAttack` | `HasPendingAttackProfile()` | Valid attack profile cached |
| `FEnemyStateTreeCondition_IsPendingAttackInRange` | `IsPendingAttackInRange()` | Target within cached profile's `AttackRange` |
| `FEnemyStateTreeCondition_IsPendingAttackOutOfRange` | `HasPendingAttackProfile() && !IsPendingAttackInRange()` | Profile cached but target out of profile range |

*Note: `CanRequestCooldownReposition()` exists in Native, but the historical asset's `Wait -> Reposition` transition did not use it directly; the minimum retry interval is managed internally by the Controller/Task's `NextAllowedRepositionTime`.*

The behavior-relevant authored graph below is retained as
`[Historical Editor readback]` from the stable architecture record (`fefdb87`).
It is useful handoff information for new sessions, but it is not a fresh
assertion about an untracked or subsequently edited asset. If the asset
topology, bindings, transition triggers/order, or behavior-changing MoveTo flags
change, refresh this subsection with a new Editor readback before treating the
record as current.

#### `ST_Enemy_Goblin_Melee` recorded topology

The readback records `/Game/BP/Characters/Enemy/ST_Enemy_Goblin_Melee` using
`StateTreeAIComponentSchema`, with the native
`/Script/PolyQuest.EnemyAIController` as `AIControllerClass` and `Pawn` as
the Context Actor Class. The inherited Controller `StateTreeComponent.StateTreeRef`
points to this asset and automatic start is disabled. The Root has five ordered
leaf states: `Patrol`, `Alert`, `Chase`, `Combat`, and `Return`.

##### Root

- `OnEvent Event.AI.Target.Acquired -> Alert`; no conditions, normal priority,
  event-consuming.
- `OnEvent Event.AI.Target.Lost -> Return`; no conditions, normal priority,
  event-consuming.
- These are the common target-event entry routes rather than duplicate local
  transitions on every child state. The recorded transitions use normal
  priority and no transition delay.

##### Patrol

- Runs a `StateTreeDelayTask` with `Run Forever`; it has no local transition
  and waits for the Root target events.

##### Alert

- Runs `Enemy Begin Alert`, then succeeds immediately.
- `OnStateSucceeded -> Combat` when `Enemy Has Valid Target` and `Enemy Target
  Is In Melee Range`.
- Otherwise, `OnStateSucceeded -> Chase` when `Enemy Has Valid Target`.
- The authored order keeps the in-range `Combat` branch ahead of `Chase`.

##### Chase

- Runs `StateTreeMoveToTask` toward `AIController.Current Target`;
  `TargetActor` is bound to the Controller target and `AcceptableRadius` is the
  cached AttackSet `MeleeRange/EngagementRange`.
- `AllowStrafe` is disabled. `AllowPartialPath`, `TrackMovingGoal`,
  `RequireNavigableEndLocation`, and `ProjectGoalLocation` are enabled.
  `ReachTestIncludesAgentRadius` and `ReachTestIncludesGoalRadius` are disabled
  so arrival uses the same actor-center distance rule as the Native checks.
- `OnStateCompleted -> Alert` covers both MoveTo success and failure.

##### Combat

- Combat is a compound state with no parent-level task and exits to `Alert`
  when its child flow completes.
- `Decision` runs `Enemy Prepare Melee Attack`
  (`FEnemyStateTreeTask_PrepareMeleeAttack`). A successful in-range selection
  goes to `Attack`; an out-of-range selection goes to `Approach`; failure goes
  to `Wait`.
- `Approach` runs `Enemy Approach Selected Melee Attack`
  (`FEnemyStateTreeTask_ApproachSelectedMeleeAttack`) and keeps the selected
  profile while moving to its `AttackRange`. Success goes to `Attack`, timeout
  or navigation failure goes to `Wait`, and `Event.AI.Target.Lost` goes to
  `Alert`.
- `Attack` runs `Enemy Request Melee Attack`
  (`FEnemyStateTreeTask_RequestMeleeAttack`). It directly requests the enemy
  GAS Ability, observes `State.Action.Attacking`, and succeeds only after an
  observed attack finishes. Completion goes to `Reposition`; failure goes to
  `Wait`.
- `Reposition` runs `Enemy Reposition During Cooldown`
  (`FEnemyStateTreeTask_RepositionDuringCooldown`) and drives target-relative
  navigation while the attack cooldown remains active. Completion goes to
  `Wait`.
- `Wait` runs the authored `StateTreeDelayTask`. When the target is valid, attack
  ready, and in range it returns to `Decision`; when attack-ready but outside
  melee range it returns to `Alert`; while cooldown remains it returns to
  `Reposition`. The authored loop intentionally leaves the minimum reposition
  interval to the Controller/task rather than adding that interval as a second
  StateTree action state. A final unconditional `OnStateCompleted -> Alert`
  transition is the fallback when no guarded branch is selected.

##### Return

- Runs `StateTreeMoveToTask` toward `AIController.Home Location`;
  `Destination` is bound to `HomeLocation`, `TargetActor` is empty, and the
  acceptance radius is `HomeAcceptanceRadius`.
- `AllowStrafe` and `TrackMovingGoal` are disabled.
  `AllowPartialPath`, `RequireNavigableEndLocation`, `ProjectGoalLocation`,
  `ReachTestIncludesAgentRadius`, and `ReachTestIncludesGoalRadius` are enabled
  by the recorded home-arrival policy.
- `OnStateCompleted -> Patrol` covers successful arrival and terminal MoveTo
  failure.

##### Maintenance rule

When a stage changes this tree's state topology, events, conditions, Native task
bindings, transition trigger/order, property bindings, or behavior-changing
MoveTo flags, update this contract in the same stage. Do not record node layout,
GUIDs, colors, or other high-frequency Editor presentation data.

---

## Reaction, death, feedback, and UI

### Reaction and death contracts

#### Classification and event dispatch

`FHitReactionClassifier` is a pure classifier over exact `Data.Reaction.Small`,
`Data.Reaction.Big`, and `Data.Reaction.Launch` AssetTags. No matching tag is a
legal `None` result; more than one matching tier is `Invalid` and suppresses the
reaction event while leaving the already-applied Health change intact.

`APlayerCharacter::OnHealthAttributeChanged()` and
`AEnemyCharacter::OnHealthAttributeChanged()` are the authoritative dispatch
boundaries. They require authority, a strict Health decrease, and non-null
`GEModData`; healing, direct/unchanged writes, and teardown do not enter this
route. A single `FGameplayEffectSpec` that contains multiple Health modifiers is
de-duplicated so feedback and reaction are emitted at most once per Spec.

- Player lethal Health returns without dispatching a reaction (there is no native
  Player death pipeline in this scope). For a living Player, Small/Big/Launch
  dispatch `Event.Reaction.Player.Small`, `.Big`, or `.Launch`.
- Enemy lethal Health is resolved before reaction classification and enters the
  Dead/DeathPending rules below. For a living Enemy, Small/Big/Launch dispatch
  `Event.Reaction.Enemy.Small`, `.Big`, or `.Launch`.
- Enemy reaction dispatch skips an existing `State.Status.Stunned` or broken
  Poise state. The one exception is a Launch modifier from the same effect that
  performed the Poise-breaking modifier, allowing that combined GE to finish its
  intended Launch handoff. Invalid tier combinations are logged and fail closed.
- Each event carries the effect instigator, the target, the damage delta in
  `EventMagnitude` (`OldHealth - NewHealth`), and the original
  `EffectContextHandle`. The target ASC, not the resolver or a Widget, activates
  the matching reaction Ability.

`FHitReactionImpactResolver` supplies the target-local planar
`Target -> Attacker` direction. It prefers the finite actor-center line and
falls back to a finite `ImpactNormal`; invalid or coincident input returns
`ZeroVector`. Directional four-way montage selection remains an
[Authored asset] dependency.

#### Small reaction

`UPlayerSmallHitReactionAbility` and `UEnemySmallHitReactionAbility` are
`InstancedPerActor`, `ServerOnly`, Gameplay-Event abilities triggered by their
matching Small event. Both set `bRetriggerInstancedAbility = true`, own
`State.Action.SmallHitReacting`, and block activation while Dead or Stunned.
They do not change CharacterMovement, cancel an attack, or add movement/input
locks. A validated four-way Montage is played through
`UAbilityTask_PlayMontageAndWait`; completion, interruption, cancellation,
invalid startup, retrigger replacement, and teardown remove the old callbacks
and converge on idempotent `EndAbility()`. The four Montage references and
their overlay-slot wiring are [Authored asset] / [Not verified in this pass].

#### Big reaction and safe interrupt

`UPlayerBigHitReactionAbility` and `UEnemyHitReactionAbility` are grounded,
`InstancedPerActor`, `ServerOnly` Gameplay-Event abilities. They own
`State.Action.HitReacting`; the Enemy owner also owns `State.Block.Facing`.
They require a complete directional Montage set and a grounded
CharacterMovement state before activation. Only after the selected
Montage is confirmed active do they stop current velocity, capture and disable
`bCanWalkOffLedges`, bind `MovementModeChanged`, and cancel their permitted
active abilities (Enemy melee/Small reaction; Player's configured action set).
Falling ends the reaction and restores only the captured ledge setting. Montage
Root Motion is the presentation displacement source; these abilities do not use
`DisableMovement()`, force `MOVE_Walking`, impulses, or Motion Warping as a
replacement movement path. Montage/task delegates and all abnormal exits are
cleaned by `EndAbility()`.

#### Notify-timed Hyper Armor

`UAnimNotifyState_EnemyHyperArmor` is timing-only. It emits
`Event.Attack.HyperArmor.Begin` / `.End` through the mesh owner's ASC and puts
the source Animation in `OptionalObject`; it does not mutate Attributes,
Movement, Collision, Controller, or AI directly.

The active `UEnemyMeleeAbility` is the sole owner of the loose
`State.Status.HyperArmor` contribution. It accepts a Begin/End only when the
event belongs to the current started attack, the current Avatar is both
Instigator and Target, and the source Animation matches the active attack
Montage (or its accepted sequence identity). EndAbility, Montage end, and
duplicate/late events clear the contribution before attack task cleanup.
Enemy Big and Launch reactions are blocked while this tag is present; Stance
Break and terminal Dead teardown are not blocked by it and cancel the melee
owner, which clears the tag. Hyper Armor has no separate Trace, Resolver,
StateTree, or Player-side state machine.

#### Poise recovery and Stance Break

`UCharacterAttributeSet` only clamps Poise. `AEnemyCharacter::OnPoiseAttributeChanged()`
owns the server-side recovery and zero-crossing dispatch. A nonlethal partial
depletion clears/restarts one recovery timer, governed by `PoiseRecoveryDelaySeconds`,
`PoiseRecoveryRate`, and `PoiseRecoveryTickIntervalSeconds`. Each tick applies the
configured Instant GameplayEffect with SetByCaller `Data.Poise.Recovery` and
stops when Poise is full/zero, the enemy is Dead/Stunned, configuration is
invalid, or the effect fails to advance the value. Poise is never restored by a
direct Attribute write in this path.

A positive-to-zero crossing schedules one next-tick
`Event.Reaction.Enemy.StanceBreak` dispatch, allowing all modifiers of the same
GE to settle first. The deferred check requires authority, a living enemy with
positive Health, still-broken Poise, and a valid recovery configuration. The
effect definition/context pair is copied only as a short-lived correlation
key; no callback-local `FGameplayEffectSpec*` is retained. Lethal Health wins
over Stance Break under any modifier order. If no Stance Break Ability accepts
the event, the character logs and restores Poise through the same recovery GE so
it cannot remain permanently broken.

`UEnemyStanceBreakAbility` is `InstancedPerActor`, `ServerOnly`, and
Gameplay-Event triggered. It owns `Ability.Reaction.Enemy.StanceBreak` and
`State.Status.Stunned` and `State.Block.Facing`, blocks
Dead/Stunned/VictimLocked activation, and only after its Montage is confirmed
active disables movement and cancels Enemy Melee, Big, Small, and Launch
reaction Abilities. Its optional
`Event.Action.RateWindow.Begin/End` listeners change Montage playback rate only;
`FAbilityMontageRateWindowLifecycle` validates source identity and restores the
captured baseline. EndAbility invalidates the callback token, restores rate,
stops/ends tasks, restores walking and full Poise only while this Ability still
owns those locks, and leaves movement/Poise recovery to the execution Victim
Ability when `State.Action.Execution.VictimLocked` is present. Unpossession
selects only Abilities carrying `Ability.Action.Teardown.OnUnpossess`.

#### Launch reaction

`UPlayerLaunchReactionAbility` and `UEnemyLaunchReactionAbility` are matching
`InstancedPerActor`, `ServerOnly` Gameplay-Event abilities, but their grounded
presentation contracts are now intentionally distinct.

`UPlayerLaunchReactionAbility` retains the Native phase sequence:

~~~text
None -> Takeoff -> TurningToLaunch -> AwaitingAirborne -> Airborne -> LandingRecovery
~~~

It freezes the target-local impact direction and reference Yaw before the
Takeoff Montage, accepts `Event.Reaction.Launch.Commit` only from the current
Avatar and active Takeoff Montage (or its contained sequence), pauses Takeoff,
completes the facing task, and then lets `LaunchCharacter()` plus
CharacterMovement own capsule displacement. `MovementModeChanged` is the fast
path into Falling, with the existing watchdog governed by
`AirborneTransitionGraceSeconds`. Landing stops the paused Takeoff, clears
residual movement once, and starts the authored LandingRecovery Montage.

`UEnemyLaunchReactionAbility` retains that same sequence for its Legacy Physics
fallback and adds a separate `RootMotionKnockdown` phase. When the opt-in flag,
Enemy Montage, Root Motion/Slot/length validation, and exact `MOVE_Walking`
precondition pass, one continuous authored Montage owns the Enemy's takeoff,
backward displacement, knockdown, and recovery. Before task activation the
Ability cancels competing Enemy abilities, rejects residual Root Motion,
applies the resolved attacker-facing Yaw once, and captures
`bCanWalkOffLedges`; CMC remains the sole capsule, floor, step, and ledge
authority. Any movement-mode change away from `MOVE_Walking`, Montage
interruption, cancellation, death, destruction, unpossession, or startup
failure converges on the idempotent `EndAbility()` cleanup. A Root Motion
startup failure does not switch to `LaunchCharacter()` after the branch has
been selected; Legacy fallback is decided only before startup. The Root Motion
branch does not create or require `Event.Reaction.Launch.Commit`; the Enemy
Launch Ability owns `State.Block.Facing` across both branches.

Only the Player launch Ability listens to the existing Dodge cancel-window
events, and only during `LandingRecovery` from the matching recovery Montage.
It exposes one scoped `State.Action.CanCancel.Dodge` contribution; there is no
Player air Dodge, Enemy recovery Dodge, or generic reaction-cancel layer.
Montage/task/delegate cleanup, ledge-setting restoration, frozen snapshots,
watchdog failure, renewed Falling, death, destruction, and teardown all converge
on `EndAbility()`. A natural Enemy LandingRecovery or Root Motion Montage end
releases the pending Poise/Stance-Break deferral; an abnormal end clears it and
restores Poise when the living enemy remains at zero.

`AEnemyAIController` suppresses focus-driven rotation while actual Enemy Root
Motion is active or the controlled Enemy ASC owns `State.Block.Facing`. The
facing block is a cross-Ability lifecycle contract, not a Montage-playing
heuristic or a controller-side action state machine. Player grounded launch
alignment remains a separate `TODO-07B13` contract.

#### Enemy death and teardown

`State.Status.Dead` is the Enemy terminal source of truth. On an ordinary lethal
Health change, `AEnemyCharacter` keeps Health at `0`, sets the Dead loose-tag
count to one, stops StateTree/navigation/target/focus, cancels all enemy ASC
Abilities, stops and disables CharacterMovement, and then optionally starts
ragdoll. Dead handling is idempotent and clears Poise recovery, pending Stance
Break timers/deferrals, lock-on highlight, UI bindings, and invalidated Player
Motion-Warp targets. The AttributeSet clamps Health but does not choose this
terminal path.

An authorized paired execution hit follows a delayed route: Health remains `0`,
the Victim owns `State.Status.DeathPending`, and only the authenticated
`Release` calls `CommitExecutionDeath()`, which then enters the same
`SetDeadState -> HandleDeath -> CancelAllAbilities -> StartDeathRagdoll` chain.
DeathPending is not a revival state; healing is reset to zero and ordinary
non-execution lethal damage still dies immediately.

Ragdoll is attempted only when enabled and a SkeletalMesh Physics Asset exists.
The Capsule becomes `NoCollision`, the mesh uses the `Ragdoll` profile and
simulates physics, and the optional directional velocity change is consumed at
most once. `DeathRagdollImpulseBoneName == NAME_None` or a missing/non-simulated
bone applies no extra impulse; the code does not assume a pelvis/torso bone.
Bone choice, Physics Asset contents, death Montage/AnimBP, and final corpse
appearance are [Authored asset] / [Not verified in this pass].

Player has no native terminal death teardown in this scope; its lethal Health
callback intentionally emits no reaction event.

### Feedback boundary

#### Profiles and overlay

`UCombatFeedbackDataAsset` is the abstract shared profile root. It owns only the
optional hit Overlay material and its duration governed by
`HitFeedbackOverlayDurationSeconds`, while `UPlayerCombatFeedbackDataAsset` and
`UEnemyCombatFeedbackDataAsset` add role-specific camera shake, sound, hit-stop,
and blood/Niagara references. These assets store no ASC, GameplayEffect, timer,
or active presentation state.

`ABaseCharacter::TriggerHitFeedbackOverlay()` is a short-lived presentation
bridge. On a valid nonlethal Health decrease the character caches the previous
mesh Overlay, applies the configured material, and restores the cached value on
timer expiry only if the character still owns that Overlay. EndPlay clears the
timer and state. Missing or invalid shared profile data is a presentation
no-op; it never changes Health, Poise, damage, or Ability cleanup.

#### Player impact channels

`APlayerCharacter::OnHealthAttributeChanged()` drives received-hit Overlay,
typed tier Camera Shake, FOV punch, and (once per Health-modifier Spec)
received-hit sound after the authority, living-target, and enemy-team checks.
`None` and `Invalid` tiers do not start a reaction Camera Shake. Big/Launch
received and attacker impacts request an FOV punch; Parry success requests the
typed Big shake plus FOV punch; authorized execution impact uses the dedicated
execution shake plus FOV punch. A missing or wrongly typed Player profile
suppresses only these typed channels.

`UCameraModifier_FovPunch` is a local presentation leaf. It adds a bounded
negative offset to `FMinimalViewInfo::FOV` during `ModifyCamera()` and never
mutates `FollowCamera->FieldOfView`. The cumulative punch is clamped by
`MaxPunchDegrees` and recovers toward zero at `RecoveryInterpSpeed`. The Player
finds or adds one modifier on its local camera manager and the modifier
deactivates at a near-zero offset.

#### Enemy impact channels

`AEnemyCharacter::HandleCombatImpactFeedback()` accepts only an exact
`Team.Player` instigator. It dispatches Player attacker impact shake, then reads
the typed Enemy profile for tier hit-stop, impact sound, and blood Niagara. Each
tier and Execution defines `ImpactHitStopDurationSeconds` and
`ImpactHitStopTimeDilation` on the profile.
Sound uses a finite `ImpactPoint` when available and otherwise ActorLocation;
blood requires a finite non-zero ImpactNormal and orients the system from that
normal. A single Spec's multiple Health modifiers are de-duplicated. During an
authorized execution Hit scope, ordinary impact channels are suppressed and
the typed execution channels are dispatched once after the scope validates.

#### Global hit-stop ownership

`APolyQuestPlayerController` is the sole global hit-stop owner. Valid requests
reject non-finite values, arbitrate overlapping requests monotonically (lower
time dilation wins and the later real-time expiry wins), and use unscaled real
time for expiry. If another system changes global dilation, the controller
relinquishes its old request without overwriting that external value; teardown
restores the recorded baseline only when the controller still owns the applied
dilation.

GameplayCue is not the current Native feedback route. Concrete sound, Niagara,
material, color, rendering, and timing assignments remain [Authored asset] /
[Not verified in this pass].

### UI ownership

`APolyQuestPlayerController` idempotently creates the local Player Vital HUD,
Skill Bar HUD, and World Interaction Prompt in `BeginPlay()`/`OnPossess()`. It
skips creation on a dedicated server, binds the current Player ASC, and removes
all attribute/tag/equipment delegates plus viewport widgets on UnPossess and
EndPlay. The Controller also forwards rejected-stamina feedback to the Vital
widget; it does not own gameplay state.

| UI type | Responsibility |
| --- | --- |
| UPlayerVitalHUDWidget | Passive display/animation layer; reads Controller snapshots and owns no ASC, input, or gameplay mutation |
| UPlayerSkillBarHUDWidget | Reads prepared slots/cooldowns from Equipment and ASC; no independent gameplay timer |
| UWorldInteractionPromptWidget | Passive candidate text/view; no input or equipment mutation |
| Enemy health bar component | Enemy-owned screen-space component with configured draw size, relative Z, and collision disabled |

`UPlayerVitalHUDWidget` rejects non-finite or non-positive-Max inputs to a zero
display:
- **Health smoothing & zero-snap**: Health damage snaps the main bar immediately.
  The optional buffer bar waits for `BufferCatchUpDelay` then catches up via
  `FMath::FInterpTo` at `BufferCatchUpSpeed`. Healing interpolates the main bar at
  `HealthRegenInterpSpeed`, while the buffer bar leads or matches healing and never
  lags behind it. When Health reaches or drops below the zero threshold, both
  main and buffer bars snap to zero immediately on the same frame.
- **Stamina asymmetric interpolation & Zero-Snap Invariant**: Stamina drains
  catch up via `FMath::FInterpTo` at `StaminaDrainInterpSpeed` to soften discrete
  GAS periodic ticks into fluid continuous visual drain; natural recovery
  interpolates toward higher targets at `StaminaRegenInterpSpeed`. When Stamina
  depletes to or below the zero threshold, `SetStamina` overrides interpolation
  and immediately snaps `TargetStaminaPercent`, `CurrentStaminaPercent`, and the
  progress bar directly to zero on the very same frame (Zero-Snap Invariant),
  guaranteeing zero latency, zero residual fill, and complete immunity to stale
  ghost stamina during exhaustion or failed actions.
- **Exhaustion & feedback**: Reaching full stamina arms the optional charge flash.
  The `StaminaExhaustedOverlay` is toggled strictly by the Controller's
  `State.Status.Exhausted` tag callback, not a UI timer. Optional low-health
  vignette and vertical micro-shake are display-only, governed by
  `LowHealthThreshold`, `LowHealthPulsePeriod`, `DamageFlashDuration`, and
  micro-shake tuning properties.

Each Enemy owns one collision-disabled Screen-space
`EnemyHealthBarWidgetComponent` with pivot `(0.5, 1.0)`. It binds only
Health/MaxHealth, exposes lock-on highlight, and hides/unbinds on death or
EndPlay. The widget's optional hit flash/shake and auto-fade are display
behavior: damage wakes the bar, lock-on keeps it visible, and full-health unlock
can enter the auto-fade path governed by `AutoFadeDelay` and `FadeOutDuration`.
The configured Widget classes, hierarchy, bindings, animations, fonts, colors,
and final appearance are [Authored asset] / [Not verified in this pass].

The Skill Bar (`UPlayerSkillBarHUDWidget`) manages four index-aligned slots. Each
refresh validates the Equipment slot's exact prepared handle, current component
ownership, class identity, ASC actor info, and Dead state before evaluating one
of four discrete display states (`EPlayerSkillSlotDisplayState`):
- **`Empty`**: Structurally empty prepared slot (`IsPreparedSlotEmpty(SlotIndex) == true`).
  When the bar itself does not yet have both a bound Equipment component and
  ASC, all slots also use Empty as the safe neutral reset state.
- **`Invalid`**: A non-empty slot whose binding is invalid, including a dead
  Player, invalid ASC or ActorInfo, missing Spec, Spec marked `PendingRemove`,
  Ability class mismatch, or non-finite / implementation-rejected cooldown query
  results.
- **`Cooldown`**: Evaluated when `Duration > 0` and `Remaining` exceeds the
  implementation zero-threshold; displays the normalized cooldown progress ratio
  (`Remaining / Duration`).
- **`Ready`**: Evaluated when `Remaining` is within the implementation
  zero-threshold and `Duration >= 0`.

Cooldown time remaining and duration are queried through the bound Ability,
exact Handle, and ASC ActorInfo; the widget maintains no local gameplay logic
timer.

---

## Gameplay Tag taxonomy

`Config/Tags/PolyQuestGameplayTags.ini` is the sole authority for tag spelling
and membership. This architecture document does not maintain aggregate tag
totals or counts per family.

| Family | Role |
| --- | --- |
| Ability | Ability identity and activation/cancellation policy |
| State | Runtime action, input, movement, resource, and status state |
| Event | Semantic gameplay event ingress/egress |
| Input | Input intent routing |
| Data | SetByCaller / Effect payload data |
| Cooldown | Effect-granted cooldown state |
| Team | Combat team identity |

Important ownership distinctions:

| Contract | Tag form |
| --- | --- |
| Victim execution ability identity | Ability.Action.Execution.Victim |
| Victim paired-lock state | State.Action.Execution.VictimLocked |
| Player paired-lock state | State.Action.Execution.PlayerLocked |
| Action facing block state | State.Block.Facing |
| UnPossess teardown selector | Ability.Action.Teardown.OnUnpossess |
| Player exhaustion state | State.Status.Exhausted |
| Delayed lethal execution state | State.Status.DeathPending |
| Combat team identity | Team.Player, Team.Enemy |

New C++ requests and authored Ability/Effect data must use exact config tags.
Do not create spelling variants or turn an Ability.* identity tag into a State.*
lifetime tag.

---

## Non-goals and authored dependencies

### Current non-goals

- This ledger establishes no multiplayer runtime contract and does not introduce
  replication, prediction, rollback, or server/client ownership layers for
  future use.
- GAS remains the only combat-state authority. A controller, StateTree,
  Blueprint variable, or AnimBP flag cannot become a parallel action state
  machine.
- Lock-on retention hysteresis is not a global target-acquisition relaxation,
  target-cycle relaxation, or projectile-homing rule.
- ARCHITECTURE.md is not a TODO list, an implementation diary, an execution
  record, or proof of a successful user validation run.

### Dependencies requiring separate evidence

The following are intentionally not represented as verified Native runtime
truth in this pass:

- A fresh Editor readback of the current StateTree asset tree and transition
  wiring, including ST_Enemy_Goblin_Melee; the historical behavior topology
  above is retained as qualified authored-asset evidence.
- Blueprint parentage/graphs, Enhanced Input Mapping Context topology, and
  placed map actors.
- Gameplay Ability, Gameplay Effect, Montage, AnimBP, DataAsset, Niagara,
  material, sound, widget, and imported-content values or links.
- Exact visual composition, timing, color, VFX behavior, camera framing,
  floor fade result, and user input feel.
- Asset removal, migration, or retirement claims that require Editor reference
  inspection rather than a Native source scan.

Editor readback, compilation, Automation execution, PIE, and visual validation
remain separate evidence gates. When one of those gates is completed, only its
stable resulting contract belongs here; the validation receipt itself belongs in
the appropriate stage or handoff record.
