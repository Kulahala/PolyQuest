# TODO-07B3: Projectile Flight Trail v1

## Plan State

- Status: Completed. The user confirmed focused PIE and the sixteen-suite Unreal Editor Automation matrix; the terminal-fade correction passed Main's delta Fresh Review after its non-finite-timeout, null-System silence, and Niagara completion-reentrancy repairs.
- Baseline: c1934a3 ([Fix] 补齐 Locomotion 模式枚举空洞以修复动画蓝图混合分支 (Fill Locomotion Mode Enum Gap For AnimNode BlendListByEnum)). The existing uncommitted Projectile Flight Trail source/test work is this stage's active worktree, not unrelated WIP.
- Objective: add one optional, data-driven Niagara flight trail to the existing ACombatProjectile. The first authored closure is a white arrow-tail trail that naturally finishes after a hit or lifespan expiry before its owner Actor is destroyed; future ordinary, fire, and frost Definitions can select different Niagara Systems without new Projectile Actor or Blueprint subclasses.
- User decisions frozen for v1:
  - Editor authors only one arrow-mesh Socket: FX_Trail, located at the arrow tail/fletching.
  - Editor authors only the ordinary white flight-trail Niagara System in this slice.
  - A configured System with a missing named Socket logs one focused Warning for that Projectile Actor and falls back to ProjectileMeshComponent root. It never invalidates the Definition, prevents launch, or changes combat delivery.
  - A null System is an intentional silent visual no-op.
  - A terminal flight trail stops emitting through Deactivate(), completes naturally, then destroys its still-owning Projectile Actor through OnSystemFinished or a Definition-authored timeout fallback. Detaching is visual-only while the Actor remains alive; ownership is never transferred.
- Preserve all unrelated WIP. Do not modify, stage, move, delete, or infer product behavior from .gitignore, Config/**, Content/**, maps, Blueprints, AnimBPs, GA/GE/Montage assets, .uproject, or generated output.

~~~text
Outer: ue-stage-workflow
Primary: ue5-cpp-gameplay
Support: unreal-niagara, ue5-debug-validation
Route reason: this slice adds immutable Definition presentation data and one Actor-owned Niagara lifecycle while retaining the current Projectile movement, collision, homing, and GAS delivery contracts.
~~~

~~~text
Plan explorers: 0
Implementation executors: 1 (Gemini only after its read-only plan review is accepted and the user explicitly authorizes execution)
Complex Executor: one scoped lifecycle-sensitive C++ slice
Main parallel work: none
Reason: Definition data, Actor component lifetime, socket fallback, and headless Automation observation are one integrated lifecycle. Main owns the frozen public contract, documentation, validation interpretation, fresh review, staging, and commit; Gemini may write only the approved source/test slice.
~~~

## Evidence And Design Decision

- UProjectileDefinition is already the immutable source for display Mesh, display transform, movement, collision, target assist, Homing, and Damage GE data. ACombatProjectile::InitializeProjectile() validates it, applies Mesh/transform, configures movement, and starts the runtime lifecycle.
- ACombatProjectile has a CollisionComponent root, a collision-free ProjectileMeshComponent display child, and a UProjectileMovementComponent whose rotation follows velocity. It already destroys itself after a valid Pawn hit or a blocking impact, and its EndPlay() is the common destruction/lifespan teardown boundary.
- The private Niagara module is already linked by PolyQuest.Build.cs for TODO-07B2. This stage must not change module dependencies.
- Mesh Socket and Definition have separate responsibilities:
  - the arrow Static Mesh owns where a physical tail effect begins;
  - UProjectileDefinition owns which System plays and which optional Socket name it requests;
  - ACombatProjectile owns component creation, attachment, activation, stopping, and destruction safety.
- This is deliberately not a generic projectile-VFX slot system. No TArray of effect slots, no FX_Tip runtime route, no impact VFX, and no fire/frost asset authoring are added now.

## Frozen Runtime Contract

### 1. Projectile Definition

In Source/PolyQuest/Public/Combat/Equipment/ProjectileDefinition.h, add only these optional fields under Projectile|VFX:

1. TObjectPtr<UNiagaraSystem> FlightTrailSystem = nullptr
2. FName FlightTrailSocketName = NAME_None
3. float FlightTrailFinishTimeoutSeconds = 0.35f

ProjectileDefinition.h contains only class UNiagaraSystem; for this field. CombatProjectile.h contains only class UNiagaraComponent; and class UNiagaraSystem;. NiagaraComponent.h and NiagaraSystem.h are included only by CombatProjectile.cpp. Do not add a hard-coded asset path, Gameplay Tag, Blueprint event, soft-loading path, mutable runtime state, or a VFX-profile abstraction.

FlightTrailFinishTimeoutSeconds is the maximum post-Deactivate wait only, not the authored particle lifetime. It is meaningful only when FlightTrailSystem is non-null and must be exposed as a positive editable value. IsValidProjectileDefinition() remains a gameplay/data-integrity validator: it must not reject a null System, an empty Socket name, a missing Mesh Socket, or a bad presentation timeout. A null System is valid and silent. A non-null System plus a missing requested Socket is resolved at runtime as a visual fallback, not a rejected launch. A non-finite or non-positive timeout is normalized to the native 0.35-second fallback with one focused Warning for that Projectile Actor.

### 2. Projectile-Owned Niagara Component

In Source/PolyQuest/Public/Combat/Projectile/CombatProjectile.h and Source/PolyQuest/Private/Combat/Projectile/CombatProjectile.cpp:

1. Add one reflected UNiagaraComponent default subobject named FlightTrailComponent, exposed consistently with the existing collision, display, and movement components.
2. Add the normal read-only native getter GetFlightTrailComponent(). It is not BlueprintCallable and creates no external lifecycle owner.
3. In the constructor, attach it initially below ProjectileMeshComponent; set bAutoActivate = false, bAutoManageAttachment = false, and auto destroy off.
4. Add virtual LifeSpanExpired() override plus exactly these private helpers:
   - ConfigureAndStartFlightTrail(const UProjectileDefinition& Definition)
   - StopFlightTrail()
   - ResolveFlightTrailSocket(const UProjectileDefinition& Definition, FName& OutSocketName, bool& bOutUsedRootFallback)
   - BeginTerminalFlightTrailFadeOut()
   - FinishTerminalFlightTrailFadeOut()
   - OnFlightTrailFinishTimeout()
   - OnFlightTrailSystemFinished(UNiagaraComponent* FinishedComponent)
   Add only the required cached timeout, terminal-fade boolean, and timer handle state. OnFlightTrailSystemFinished is a UFUNCTION bound once to FlightTrailComponent->OnSystemFinished in PostInitializeComponents and removed in EndPlay.
5. InitializeProjectile() keeps its current null/invalid Definition rejection, display Mesh assignment, launch direction, movement, lifespan, source ignore, and Homing logic. After Mesh assignment and before returning success, it calls ConfigureAndStartFlightTrail().
6. ConfigureAndStartFlightTrail() first converges any prior effect through StopFlightTrail(). With no System it returns silently. With a System it:
   - resolves FlightTrailSocketName == NAME_None to OutSocketName = NAME_None and bOutUsedRootFallback = false. This is the normal display-component-root route and never logs;
   - resolves a non-None Socket only when ProjectileMeshComponent has a Static Mesh and DoesSocketExist(SocketName) is true;
   - resolves an explicitly requested but unavailable Socket, including a null Static Mesh, to OutSocketName = NAME_None and bOutUsedRootFallback = true;
   - attaches both the normal root route and a valid Socket route with SnapToTargetNotIncludingScale, so authored Niagara width does not inherit DisplayScale;
   - on bOutUsedRootFallback, emits at most one LogPolyQuest Warning per actor initialization and attaches to the display-component root;
   - assigns the System and activates the single component.
7. StopFlightTrail() is idempotent immediate teardown used only for failed/repeated initialization and EndPlay. It calls Deactivate(), clears its assigned System, and records inactive test state; it never destroys the default subobject or changes movement/collision state.
8. BeginTerminalFlightTrailFadeOut() is the only hit/lifespan presentation route:
   - it is guarded against re-entry, clears the normal lifespan timer, disables collision and contact delegates, stops ProjectileMovement and Homing, and disables the Actor tick;
   - with no active Trail System it immediately calls FinishTerminalFlightTrailFadeOut(), which destroys the Actor;
   - with an active Trail it detaches FlightTrailComponent using KeepWorldTransform while the Actor remains alive, hides only ProjectileMeshComponent after detachment, and calls Deactivate() without clearing the System;
   - it waits for OnSystemFinished, while one timer bounded by CachedFlightTrailFinishTimeoutSeconds calls OnFlightTrailFinishTimeout() as a leak-prevention fallback;
   - it does not call SetActorHiddenInGame, SetAutoDestroy(true), RemoveOwnedComponent, Rename/re-parent the component, DeactivateImmediate(), or Destroy() before the finish callback/timeout.
9. A successful HandlePawnImpact() and HandleBlockingImpact() call BeginTerminalFlightTrailFadeOut() after their current one-time hit state is committed. LifeSpanExpired() calls the same route. OnSystemFinished ignores components other than FlightTrailComponent and ignores normal non-terminal completion. EndPlay() clears the timer and delegate binding, then calls immediate StopFlightTrail(); external Destroy and world teardown therefore remain immediate and safe.
10. Do not add a tick, a second projectile actor path, UNiagaraFunctionLibrary::SpawnSystemAtLocation, GameplayCue, GPU readback, particle collision gameplay, new Tags, replication, or any change to target acquisition, Homing, movement, collision, damage hit resolver, Guard, or ASC ownership.

### 3. Headless Automation Observation

The real Niagara renderer is not a test dependency. Under WITH_DEV_AUTOMATION_TESTS in ACombatProjectile, add only these test-only observations:

- SetTestFlightTrailTrackingEnabled(bool)
- IsTestFlightTrailActive() const
- GetTestFlightTrailSystem() const
- GetTestFlightTrailAttachSocketName() const
- DidTestFlightTrailUseRootFallback() const
- IsTestTerminalFlightTrailFadeOutActive() const

When tracking is enabled, ConfigureAndStartFlightTrail() must still resolve and perform the real component attachment decision, then record System, active state, resolved Socket, and fallback state without starting a renderer. In this test-only mode, a recorded active TestFlightTrailSystem counts as an active trail for BeginTerminalFlightTrailFadeOut(), so the real terminal state machine, timer, detach, and OnSystemFinished handler can be tested even though FlightTrailComponent has no renderer Asset. BeginTerminalFlightTrailFadeOut() records terminal state and StopFlightTrail() records inactive state. This mirrors the established TODO-07B2 no-GPU test pattern and is not a production fallback or alternate VFX route.

## Approved Source And Test Surface

**Contract owner: Main. Implementation writer: Gemini only after explicit execution authorization.** Any need to touch an unlisted header/public surface, Build.cs, Config, Gameplay Tag, input route, GAS/ASC contract, asset, map, Blueprint, AnimBP, Montage, or documentation is a stop condition requiring a Main decision.

### Shared contracts, Main-owned and Gemini-writable only as frozen above

- Source/PolyQuest/Public/Combat/Equipment/ProjectileDefinition.h
  - Add only the three VFX fields and forward declaration. Do not modify ProjectileDefinition.cpp validation.

- Source/PolyQuest/Public/Combat/Projectile/CombatProjectile.h
- Source/PolyQuest/Private/Combat/Projectile/CombatProjectile.cpp
  - Add only FlightTrailComponent, its getter, the seven frozen helper/handler methods, the LifeSpanExpired override, test-only observation state/accessors, and lifecycle calls in the exact functions named above.
  - Preserve the current FCombatProjectileLaunchRequest shape, bInitialized, HitResolver request, movement configuration, Homing, source-ignore, collision response, and all existing logging/debug behavior.

### Test-only surface

- Add Source/PolyQuest/Private/Tests/ProjectileFlightTrailAutomationTests.cpp with Automation name PolyQuest.Projectile.FlightTrail.
- Reuse the existing transient-world pattern. Construct the valid test Socket exactly as NewObject<UStaticMeshSocket>(TransientMesh), assign SocketName = FX_Trail, then call TransientMesh->AddSocket(Socket). Test Meshes/Sockets and a transient UNiagaraSystem must be created in test memory only; do not load, edit, or depend on Content Niagara assets, a viewport, GPU simulation, Blueprints, or map state.
- Existing ProjectileLifecycleAutomationTests.cpp and ProjectileTargetAssistAutomationTests.cpp remain regression suites. Do not alter them unless a direct API assertion requires a narrow compatibility update.

## Execution Order

1. Preserve the existing Flight Trail field/component implementation and add the frozen Definition timeout field, terminal-fade declarations, and minimal forward declarations/includes.
2. Replace only hit/lifespan immediate Trail destruction with terminal fade-out: stop gameplay immediately, detach Trail visually while retaining Actor ownership, Deactivate without clearing the System, and finish through OnSystemFinished or timeout.
3. Update EndPlay to cancel fade timer/delegate and retain immediate teardown for external destruction/world shutdown.
4. Extend the test-only observation seam and the isolated flight-trail Automation suite.
5. Gemini performs source reread, direct caller/callee review, Rider static inspection on touched C++, and git diff --check; it must not compile, enter PIE, edit assets, update docs, stage, or commit.
6. The user performs Socket/Niagara/DataAsset authoring, manual PolyQuestEditor compile, Automation, and PIE. Main interprets evidence and performs the separate Fresh Review.

## Native Automation Contract

PolyQuest.Projectile.FlightTrail must cover:

1. The Actor CDO has exactly one FlightTrailComponent with auto activation and auto destroy disabled.
2. A valid Definition with no System initializes normally, leaves the component inactive/unassigned, emits no Warning, and remains immediate-destroy on hit/lifespan.
3. A transient System plus a transient Mesh containing FX_Trail resolves that exact Socket, records active state, and uses no root fallback.
4. A transient System with FlightTrailSocketName = NAME_None legitimately attaches to ProjectileMeshComponent root without Warning.
5. A transient System requesting a missing Socket produces one expected negative Warning, remains launch-valid, records root fallback, and keeps movement/collision functional.
6. A successful hostile Pawn impact and a blocking impact immediately commit their existing damage/block behavior, disable further movement/collision, hide only the projectile Mesh, retain the Actor during terminal Trail fade, and never deliver a second hit.
7. Broadcast completion from FlightTrailComponent during terminal fade and assert one final Actor Destroy. Separately set a short transient timeout, advance the World, and assert timeout cleanup when no completion arrives.
8. LifeSpanExpired enters the same terminal fade route. External Destroy and World teardown bypass lingering and clean immediately. These paths must not change hit delivery count, Damage GE application, speed, target assist, or Homing behavior.

Run the new suite plus all existing fifteen Automation suites in the Unreal Editor. Focused regression minimum: PolyQuest.Projectile.Lifecycle, PolyQuest.Projectile.TargetAssist, PolyQuest.Combat.HitReaction, and PolyQuest.Melee.WeaponTrail; the full matrix must remain green.

## User-Owned Editor And PIE Contract

1. On the Static Mesh referenced by DA_Projectile_Arrow, create one Socket named FX_Trail at the arrow tail/fletching and save it. Do not create FX_Tip in this stage.
2. Create one white world-space Niagara Ribbon/trail System that follows the arrow flight path, has no collision/gameplay readback, and is visually continuous at the current projectile speed and Homing turns. Its Emitter State Inactive Response must be Complete, not Kill; use 0.20-0.30-second particle life, alpha Scale Color from 1 to 0, and Scale Ribbon Width from 1 to 0.
3. In DA_Projectile_Arrow, assign that System to FlightTrailSystem, set FlightTrailSocketName = FX_Trail, and set FlightTrailFinishTimeoutSeconds to at least the longest residual particle life plus margin (initial 0.35 seconds).
4. Manually compile PolyQuestEditor.
5. In Scene01, validate straight shots, high-speed continuity, valid locked Homing turns, a no-target straight shot, wall impact, hostile Pawn impact, lifespan expiry, and leaving PIE. The white trail must begin at the arrow tail, follow turns without visible shearing, stop spawning at terminal contact, then visibly narrow/fade before Actor teardown. Verify the collision/damage result is immediate even while the visual tail remains.
6. Report Editor readback, compile, targeted/full Automation, and PIE visual evidence separately.

## Non-Goals, Documentation, And Commit Boundary

- No fire/frost Systems or Definitions are authored this stage. A future elemental arrow only adds its own Niagara System and Definition assignment to this contract; it does not require another Projectile Actor.
- No FX_Tip runtime attachment, generic multi-effect slot array, impact effect, Beam/AOE actor, GameplayCue, new Tag, second movement path, target-selection change, or serialization change is included. Do not detach a default subobject in an attempt to transfer ownership, set it to auto destroy, remove it from the Actor, or rename/re-parent it into the World.
- After validation and Main Fresh Review, Main alone updates plan.md, ROADMAP.md, and ARCHITECTURE.md; update README.md only if its public status/evidence summary needs the completed stage.
- Default commit scope is this stage's approved C++/Automation/docs only. Exclude Content/**, Config, maps, Blueprints, AnimBPs, GA/GE/Montage assets, imported resources, .uproject, generated folders, and all unrelated user WIP unless the user separately approves a stable asset closure.

## Closeout Record

- **Implemented contract:** `UProjectileDefinition` now optionally authors one Niagara Flight Trail System, one attachment Socket name, and a terminal-fade timeout. `ACombatProjectile` remains the only owner of one inactive default `FlightTrailComponent`; it resolves the definition after mesh setup, preserves all existing movement/collision/Homing/GAS delivery behavior, and owns start, terminal fade, timeout fallback, and immediate teardown.
- **Terminal safety:** null Systems are silent no-ops; a missing named Socket falls back to the display-component root with one focused warning. A non-finite or non-positive timeout with a configured System falls back to `0.35s`. On Pawn hit, blocking hit, or lifespan expiry, gameplay stops immediately while the detached trail completes naturally; the timer is installed before `Deactivate()` so a synchronous Niagara completion callback cannot leave a stale timer after Actor destruction.
- **Validation evidence:** the user confirmed focused PIE and all sixteen current Editor Automation suites, including `PolyQuest.Projectile.FlightTrail`, `PolyQuest.Projectile.Lifecycle`, and `PolyQuest.Projectile.TargetAssist`. Main's scoped `git diff --check` passed; Rider errors-only inspection of the four C++/test paths returned zero errors.
- **Review and debt handoff:** Main's initial Fresh Review found two P1 lifecycle issues and one P2 silent-no-op contract breach; all were repaired and the delta Fresh Review found no remaining P0-P2. No new accepted product debt was created. `TODO-03AI3` remains the next gameplay slice, followed by `TODO-03H4` before `TODO-03C`.
- **Commit boundary:** include only the three approved C++ files, the new native Automation suite, and this documentation closure. User-owned Niagara, Static Mesh socket, DataAsset, Blueprint, map, Config, and all other `Content/**` WIP remain excluded.
