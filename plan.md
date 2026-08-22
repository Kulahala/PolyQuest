# TODO-02C3G: Launch, Airborne, And Landing Reaction v1

## Plan State

- Status: Completed native C3G implementation, lifecycle repair, strict review, and documentation closeout. The user reports Automation success and focused PIE success after the paused-pose, airborne-watchdog, and landing-brake repair. `gpt-5.6-luna / xhigh` remains unavailable, so the second strict pass is a Main adversarial fallback rather than an independent Reviewer result. Mutable authored Content remains outside the source/config/document commit.
- Baseline: 88cf64d ([Feature] 完成地面大受击与根运动硬直 / Grounded Big Hit Reactions).
- Objective: upgrade the legal-but-no-op Data.Reaction.Launch tier into one authoritative Player/Enemy lifecycle: authored takeoff, real airborne CharacterMovement, actual landing, landing recovery, and deterministic cleanup. This is a vertical slice above C3E/C3F, not a rewrite of Small or Big.
- Current Content/**, maps, imported assets, input assets, and project-setting WIP are user-owned and outside this stage. Preserve them unless the user later explicitly approves a stable asset closure.

~~~text
Outer: ue-stage-workflow
Primary: ue5-cpp-gameplay
Support: ue5-debug-validation, ue5-blueprint-workflow
Route reason: C3G extends native GAS event dispatch, GameplayAbility lifecycle cleanup, CharacterMovement flight/landing ownership, one AnimNotify event bridge, and user-authored Montage/AnimBP/GE/AttackProfile configuration.
~~~

~~~text
Plan explorers: 0
Implementation executors: 1 (Gemini, user-coordinated bounded executor)
Complex Executor: none
Main parallel work: 复核实现、解释验证、fresh/adversarial review、文档收尾与提交。
Reason: Launch has one coupled target-side GAS, CharacterMovement, montage, and cleanup lifecycle. Parallel writers would split that lifecycle and increase stale delegate/tag risk; Gemini receives one explicit bounded handoff and Main retains architecture, final review, documentation, staging, and commit ownership.
~~~

## Locked Product Contract

1. Data.Reaction.Launch is a real, non-lethal launch reaction for both Player and Enemy. A valid Launch damage effect changes Health first; a rejected reaction never rolls back damage.
2. C3G uses CharacterMovement as the sole source of capsule displacement from launch through landing; authored Root Motion never owns C3G movement:
   - AM_LaunchTakeoff uses the selected A_KnockDown_Begin_RootMotion_Sword performance only as a full-body pose source. Configure the project-owned launch asset or a user-owned derivative so it extracts no Root Motion and locks its root; do not modify the imported original merely to change this setting.
   - UAnimNotify_ReactionLaunchCommit sits on the first clearly airborne horizontal pose. Commit pauses, rather than stops, AM_LaunchTakeoff at that pose, then calls ACharacter::LaunchCharacter(LaunchVelocity, true, true). A paused montage produces no further Root Motion delta while preserving the horizontal visual pose.
   - UCharacterMovementComponent exclusively owns the complete physical arc: departure velocity, MOVE_Falling, gravity, collision, walls, slopes, ledges, and landing. The template upright Falling loop must not become visible during a successful Launch because the paused full-body Takeoff pose remains active.
   - Actual ground contact stops the paused Takeoff presentation and starts AM_LaunchLandingRecovery from its prone opening pose. LandingRecovery is in-place for C3G: it contributes no extracted Root Motion translation or rotation.
3. Default tuning is horizontal 450.0 cm/s and vertical 550.0 cm/s. C3F FHitReactionImpactResolver snapshots target-to-attacker; Launch negates it to move away from the attacker. Invalid, zero, NaN, or infinite direction/tuning fails closed and never creates a substitute random launch.
4. The private lifecycle phases are Takeoff -> AwaitingAirborne -> Airborne (paused Takeoff pose held) -> LandingRecovery. Both GAs own the existing State.Action.HitReacting. Do not add a competing State.Action.Launching tag.
5. Launch starts only from ground. It rejects Dead, Stunned, Hyper Armor, and existing State.Action.HitReacting (therefore active Big or Launch). It may cancel active Small. Damage still applies when reaction activation is rejected. C3G has no air-combo escalation, Small-to-Big override, or Big-to-Launch override.
6. Player Launch uses the C3F Player Big action matrix. After confirmed Takeoff startup it blocks/cancels Ability.Attack.Primary, Ability.Attack.Light, Ability.Attack.Charged, Ability.Attack.Sprint, Ability.Skill.Melee, Ability.Dodge, Ability.Movement.Sprint, Ability.Movement.Jump, Ability.Defense.Guard, Ability.Defense.Parry, and Ability.Reaction.Player.Small. Ability.Attack.Primary still covers primary melee arbitration and Bow.
7. Player LandingRecovery is not cancelable in C3G. TODO-01C4: Player Reaction Recovery Cancel Windows is the accepted future owner for an authored Dodge cancel window, normal Dodge stamina cost, and no cancellation when stamina is insufficient.
8. A Launch damage GE carries exactly Data.Reaction.Launch. It must not also carry Data.Reaction.Small, Data.Reaction.Big, or Data.Poise.*. Landing-deferred Stance/Guard Break is out of scope and cannot occur through a C3G Launch GE.
9. Enemy death keeps existing C2 ragdoll / CancelAllAbilities precedence. Player terminal death stays TODO-03D; C3G only verifies that an existing Dead tag blocks new Launch.

## Native Runtime Plan

### Tags, health dispatch, and pure velocity

- Register:
  - Ability.Reaction.Player.Launch
  - Ability.Reaction.Enemy.Launch
  - Event.Reaction.Player.Launch
  - Event.Reaction.Enemy.Launch
  - Event.Reaction.Launch.Commit
- Retain Data.Reaction.Launch as the classifier input. Do not rename/retire C3F Small/Big tags or State.Action.HitReacting.
- Extend FHitReactionImpactResolver with this pure, testable interface: static bool TryBuildLaunchVelocity(const FVector& LocalAttackerDirection, float TargetYaw, float HorizontalSpeed, float VerticalSpeed, FVector& OutLaunchVelocity). Reset OutLaunchVelocity to ZeroVector before validation; planarize/normalize LocalAttackerDirection; reject zero or non-finite XY, non-finite TargetYaw, non-finite/non-positive HorizontalSpeed, and non-finite/non-positive VerticalSpeed. C3G deliberately rejects a zero horizontal speed: this tier is an away-from-attacker launch, not a vertical-only lift. Build world velocity from FRotator(0.0f, TargetYaw, 0.0f).RotateVector(-LocalAttackerDirection) * HorizontalSpeed with Z = VerticalSpeed. Return failure rather than logging, accessing an Actor/ASC/World, querying a DataAsset, or causing gameplay side effects.
- Extend APlayerCharacter::OnHealthAttributeChanged and AEnemyCharacter::OnHealthAttributeChanged so EHitReactionTier::Launch sends its target-specific Gameplay Event with the existing instigator, target, damage magnitude, and Effect Context. Preserve the current authority, healing/null GEModData, lethal, Dead, Stunned, Invalid, and Enemy Poise-Broken filters. Hyper Armor must not yield an accepted Launch lifecycle through the GA activation guard.

### Separate Player/Enemy Launch GAs and Commit Notify

- Add matching Public/Private pairs:
  - UPlayerLaunchReactionAbility
  - UEnemyLaunchReactionAbility
  - UAnimNotify_ReactionLaunchCommit
- Both GAs are InstancedPerActor, ServerOnly, and Gameplay Event-triggered. Expose only authored Takeoff Montage, LandingRecovery Montage, and finite launch-speed defaults for derived GA Blueprints. Do not add a generic reaction base class or reaction DataAsset: Player input and Enemy AI cancellation differ materially.
- Create, bind, and ReadyForActivation the UAbilityTask_WaitGameplayEvent for Event.Reaction.Launch.Commit before ReadyForActivation starts the Takeoff Montage task. This follows the existing Bow pattern and prevents a first-frame/very-short Takeoff Notify from being lost.
- UAnimNotify_ReactionLaunchCommit follows the existing Animation/Combat event bridge: it sends owner as Instigator/Target and the callback Animation (UAnimSequenceBase) as OptionalObject. An active Launch GA must not require OptionalObject == TakeoffMontage, because Slot playback commonly supplies a sequence rather than the UAnimMontage object. It accepts Commit only when avatar identity matches, that sequence is the active Takeoff Montage itself or belongs to one of its Slot tracks, phase is Takeoff, Commit has not run, ASC/avatar/MovementComponent are valid, and TryBuildLaunchVelocity succeeds. Stale, duplicate, or wrong-montage events are ignored or converge through guarded cleanup without launching.
- Before confirmed Takeoff playback, only normal GAS activation state may change. CancelAbilities, Player input blocking, StopMovementImmediately, Enemy AI StopMovement, movement delegate binding, and ledge-rule override occur only after Montage_IsActive() confirms Takeoff started. Failed task/startup cannot cancel the old action or mutate CharacterMovement.
- At accepted Commit, set the commit/phase guard, pause the active Takeoff montage at its authored airborne pose, retain its montage task and active-montage identity, transition to AwaitingAirborne, and call LaunchCharacter(..., true, true). MovementModeChanged is the fast path into MOVE_Falling; a UAbilityTask_WaitDelay(0.10f) watchdog ends once only if a valid Falling transition has not occurred after the pending launch can be consumed. Do not EndTask, Montage_Stop, clear ActiveMontage, change the global RootMotionMode, or otherwise continue Takeoff playback at Commit. OnActiveMontageEnded must identity-check the phase-local expected montage, then end only for a Takeoff end before Commit or for the active LandingRecovery end. A paused Takeoff remains owned and must be ignored while AwaitingAirborne/Airborne; a stale/mismatched Montage is always ignored. Failure to enter Falling never forces MOVE_Walking.

### Airborne, landing, and teardown

- Bind one identity-safe, phase-aware MovementModeChangedDelegate only after confirmed Takeoff:
  - AwaitingAirborne accepts the confirmed transition to MOVE_Falling and enters Airborne.
  - Airborne starts LandingRecovery only after actual grounded contact.
  - LandingRecovery ends if movement returns to Falling; it never forces the character back to ground.
- On actual grounding, set the LandingRecovery phase, call StopMovementImmediately() once to clear residual landing velocity, then explicitly stop the paused Takeoff montage and end its old montage task; only then clear the old ActiveMontage, create the LandingRecovery task, and start the prone-to-standing montage. This ordering makes the expected old Takeoff end broadcast stale and therefore harmless while preventing high-speed ground bounce from aborting LandingRecovery. LandingRecovery must pass startup confirmation and use no extracted Root Motion. Existing scoped ledge-state cleanup may remain only when the implementation actually changes it; it must never become a substitute movement source.
- EndAbility is the one convergence path for natural LandingRecovery completion, Takeoff interruption before Commit, missing/stale Commit, failed Falling validation, task/delegate callbacks, runtime death/teardown, and renewed fall in recovery. It removes all tasks, OnMontageEnded bindings, and movement delegates; stops only its own active montage; clears phase-local references; and restores only its owned ledge override.
- During flight and abnormal teardown after Commit, code must never manually SetMovementMode(MOVE_Walking), teleport, or apply a second force. The confirmed-ground LandingRecovery transition alone may clear residual velocity once before its in-place recovery montage; CharacterMovement otherwise decides final motion and collision response.
- C3G forbids C3G Montage Root Motion extraction, global AnimInstance RootMotionMode switches, DisableMovement(), AddImpulse, FRootMotionSource, Motion Warping, manual Tick flight, forced Walking, and persisted movement-mode overrides.

### Player/Enemy priority and cancellation

- Player Launch owns State.Action.HitReacting, State.Input.Block.Movement, and State.Input.Block.Jump. Its BlockAbilitiesWithTag and post-startup AbilitiesToCancel use the same eleven-tag C3F Player Big matrix.
- Enemy Launch owns State.Action.HitReacting, stops current path movement only after Takeoff starts, calls StopMovementImmediately(), and cancels Enemy Melee plus Enemy Small. Existing StateTree State.Action.HitReacting waiting remains the only AI pause/recovery route; do not add a StateTree state, behavior tree, controller framework, or MoveTo workaround.
- UPlayerGuardBreakAbility gains named PlayerLaunchReactionAbilityTag in AbilitiesToCancel. Its validity preflight and CDO test require exactly ten unique entries: C3F's nine plus Player Launch.
- UEnemyStanceBreakAbility gains named EnemyLaunchReactionAbilityTag in AbilitiesToCancel. Its validation and CDO test require four unique entries: Enemy Melee, Enemy Big, Enemy Small, and Enemy Launch. Guard Break/Stance Break stay higher priority and cancel active Launch instead of queueing a reaction.

## User-Owned Editor And Asset Gate

Gemini must not manually modify .uasset/.umap files, import/reparent/retarget assets, start the Editor, compile, run UBT, stage, or commit.

1. Create two full-body reaction Montages compatible with the shared Player/Goblin skeleton route:
   - AM_LaunchTakeoff: use A_KnockDown_Begin_RootMotion_Sword only as visual performance. It must output no extracted Root Motion and use root lock; place UAnimNotify_ReactionLaunchCommit on its first convincing horizontal airborne pose. The current preview suggests roughly 0.24 seconds, but use the first visually confirmed departure frame rather than a fixed timestamp.
   - AM_LaunchLandingRecovery: use A_KnockDown_End_Sword directly from its prone opening pose. It starts only after physical landing and must be in-place, with no extracted Root Motion translation or rotation. Do not add AS_Land or A_KnockDown_Loop_Sword to C3G.
2. Use the established full-body reaction Slot and Root Motion from Montages Only. Keep C3E Small's ReactionOverlay unchanged. During C3G flight the paused full-body Takeoff montage, not the template upright Falling loop, must own the mesh pose.
3. Create GA_PlayerLaunchReaction and GA_EnemyLaunchReaction from the native classes, assign both Montages and finite speeds, and add them to the respective StartupAbilities. C++ must not assume an asset path.
4. Replace the current Player whirlwind/skill test Damage GE with a dedicated Launch GE carrying exactly Data.Reaction.Launch. Add a dedicated Enemy Launch UEnemyAttackProfile and matching Launch GE, then place it only in a focused deterministic Scene01 fixture; preserve ordinary Small/Big regression fixtures.
5. Read back the configured Montage/GA/GE/AttackProfile references before PIE. Mutable assets remain user-owned WIP unless the user explicitly includes them in a later commit closure.

## Approved Native Surface

- Config/Tags/PolyQuestGameplayTags.ini.
- New Source/PolyQuest/Public|Private/AbilitySystem/Abilities/PlayerLaunchReactionAbility.*.
- New Source/PolyQuest/Public|Private/AbilitySystem/Abilities/EnemyLaunchReactionAbility.*.
- New Source/PolyQuest/Public|Private/Animation/Combat/AnimNotify_ReactionLaunchCommit.*.
- Existing Source/PolyQuest/Public|Private/Combat/Reaction/HitReactionImpactResolver.*.
- Existing Source/PolyQuest/Public|Private/Character/Player/PlayerCharacter.*, Source/PolyQuest/Public|Private/Character/Enemy/EnemyCharacter.*, Source/PolyQuest/Public|Private/AbilitySystem/Abilities/PlayerGuardBreakAbility.*, Source/PolyQuest/Public|Private/AbilitySystem/Abilities/EnemyStanceBreakAbility.*, and Source/PolyQuest/Private/Tests/HitReactionAutomationTests.cpp.
- Test-only CDO inspection accessors are allowed only when required for automation. No Build.cs change, CharacterMovement subclass, generic framework, StateTree asset/code change, Player death implementation, source asset path, or manual asset-file edit is approved.

## Validation Matrix

### Native/static and automation

- Before user compile/PIE, inspect changed direct callers/callees, tag requests, task/delegate bindings, and cleanup branches; run git diff --check.
- Extend PolyQuest.Combat.HitReaction for:
  - Launch classification and Player/Enemy Launch event dispatch without regressing None/Small/Big/Invalid;
  - Player/Enemy Launch CDO policy, ability/event tags, triggers, owned/blocked tags, Player's eleven-tag matrix, and Enemy Melee/Small boundary;
  - Guard Break's ten-entry and Stance Break's four-entry Launch-inclusive containers;
  - TryBuildLaunchVelocity attacker-away direction, target yaw conversion, finite scaling, zero vector, NaN/Inf, invalid rotation, and invalid speed failure;
  - Commit Notify contract: registered event tag, optional Animation/Slot identity policy, source-order requirement that the Commit listener is active before Takeoff playback, and the static lifecycle rule that Commit pauses/retains Takeoff rather than stopping/ending it; actual pose continuity remains a PIE gate;
  - rejection/no accepted Launch lifecycle for Dead, Stunned, Hyper Armor, Enemy Poise-Broken, lethal, and multi-tier Invalid effects; plus Small cancellation where a controlled native fixture can prove it.
- Source/static tests do not prove root-lock authoring, paused-pose continuity, flight collision, or asset playback. Those are PIE gates.
- Re-run PolyQuest.Player.ActionWindows, PolyQuest.Enemy.AttackSetSelection, PolyQuest.Enemy.CombatSpacing, PolyQuest.Equipment.TransactionMatrix, PolyQuest.Melee.TraceSourceGeometry, PolyQuest.Projectile.Lifecycle, and PolyQuest.Combat.HitReaction. Expected negative-fixture warnings remain warnings.

### User compile and Scene01 PIE

1. Manually compile PolyQuestEditor and report the result.
2. Player Launch skill hits Goblin: confirm Takeoff, true departure at Commit, CharacterMovement gravity/collision arc, horizontal paused Takeoff pose throughout flight with no frame of template upright Falling, prone-to-standing LandingRecovery only after ground contact, and AI recovery after cleanup.
3. Focused Enemy Launch AttackProfile hits Player with the same ordering and returns movement/jump/combat input only after LandingRecovery. Confirm no Player Dodge CancelWindow in C3G.
4. Test wall, slope, ledge, uneven ground, early interrupted Takeoff, renewed fall during recovery, and Enemy death in flight. No duplicate launch, stale State.Action.HitReacting, stale input block, stuck AI pause, forced Walking, or residual ledge setting is allowed.
5. Reconfirm Small overlay, grounded Big Root Motion, Guard/Parry/Dodge contact resolution, Stance/Guard Break precedence, Bow/primary cancellation, and that each Launch GE has no Poise or second reaction tier tag.

## Gemini Review And Execution Handoff

- Absolute cwd: E:\GameDevelop\PolyQuest.
- Baseline: 88cf64d plus the actual current source/config; preserve all unrelated user WIP.
- Route: ue-stage-workflow outer lifecycle; ue5-cpp-gameplay primary; ue5-debug-validation and ue5-blueprint-workflow support only for validation/Editor checklist.
- Execution order for the current visual-contract amendment: inspect the already-written Player/Enemy Launch GAs -> replace only Commit Stop/EndTask behavior with phase-safe Takeoff Pause/retention -> explicitly stop the paused Takeoff before LandingRecovery -> update focused static/automation coverage as justified -> static diff review -> hand user the exact root-lock, compile, and PIE checklist. Do not rewrite completed tags, dispatch, cancellation, velocity, or test work without evidence.
- Non-goals: no third airborne animation asset in C3G, eight-direction reactions, targeting, damage balance, knockback redesign, generic reaction inheritance, manual flight Tick, physics impulse, Player death, queued post-landing Stance/Guard Break, C3G recovery Dodge cancellation, StateTree redesign, asset migration, global RootMotionMode change, or unrelated cleanup/refactor.
- Prohibited: no Editor mutation/startup, no UBT/Rider build, no compile/PIE claim, no staging/commit, no git add -A, no destructive Git command, and no change outside approved native surface without user approval.
- Required evidence: changed-file list; compile-safe static reasoning for every task/delegate/tag contract; git diff --check; user-supplied compile/automation/PIE evidence; and strict implementation self-review. Gemini's self-review is not an independent fresh review.
- Stop and report rather than guessing when Montage/Slot/AnimBP/GA/GE/AttackProfile authoring is missing, a public tag cannot be registered, source conflicts with this plan, or a user-owned asset choice changes the contract.

## Explicit Non-Goals, Debt, And Closeout

- C3G does not add eight-direction Small/Big/Launch selection, a direction DataAsset, weapon-specific reaction tables, homing/projectiles, RootMotionSource, Motion Warping, ragdoll replacement, movement/network framework, damage/stamina tuning, or a new AI system.
- TODO-01C4 is the accepted future owner of Player LandingRecovery Dodge cancel windows. It must later define authored windows, normal Dodge stamina cost, same-montage identity checks, and no-cancel behavior with insufficient stamina.
- `TODO-02C3H: Landing-Deferred Stance And Guard Break Resolution v1` is the accepted future owner of combined Launch/Poise or Launch/Guard outcomes. C3G Launch GEs carry no `Data.Poise.*`, so current C3G cannot produce a mid-air break.
- Before C3G documentation closeout or commit, compare accepted plan, user compile/PIE evidence, review findings, and unresolved debt with ROADMAP.md. Record every real unresolved validation gap or accepted follow-up there with owner and concrete closure trigger; do not mark C3G done until its gates have evidence.

## Closeout Record

- User evidence: Automation success and focused PIE success after the final airborne-pose, `0.10s` watchdog, and landing-brake repair. No separate compile log is claimed here.
- Static evidence: direct source/caller/callee/tag review, Rider error-level lint with no findings, and `git diff --check` pass. `code-review-graph` was unavailable because both review calls returned `Transport closed`; direct source review is the coverage fallback.
- Strict review: Main normal review and Main adversarial fallback found no P0-P2. The Notify comment, previous plan status, watchdog timing, and landing-brake wording were synchronized in this closeout.
- Deferred contracts: `TODO-01C4` owns Player LandingRecovery Dodge cancel windows; `TODO-02C3H` owns landing-deferred Stance/Guard Break; `TODO-07B` owns position-relative eight-direction Small/Big presentation. None is implemented by C3G.
