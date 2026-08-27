# TODO-02C3K: Combat Impact Feedback v1

## Plan State

- Status: Completed / Closed. `PolyQuest.Combat.HitFeedback` passed its 23 numbered Automation coverage sections; the user confirmed focused PIE audio/visual verification. Two Main fresh-review P2 findings were remediated, revalidated through focused Automation/PIE, and cleared by a Main delta fresh review.
- Baseline: `45811f8` (`[Feature] 支持装备武器显示缩放 (Add Equipped Weapon Display Scale)`).
- Objective: make one confirmed Player-to-Enemy, nonlethal Health-damage event read as impact through a short global hit-stop, one shared impact sound, and one world-space enemy blood Niagara burst.
- Player-facing success: Small, Big, and Launch damage feel materially different through bounded hit-stop intensity; all valid Player melee and projectile impacts play the same configured flesh sound; a valid impact point produces one short blood burst on the enemy surface; enemy attacks against the Player receive none of these new C3K channels.
- User decisions frozen for v1:
  - Hit-stop is global and applies only after the Player has successfully caused nonlethal Health damage to an Enemy.
  - Presets are `Small = 0.02s / 0.15`, `Big = 0.05s / 0.05`, and `Launch = 0.04s / 0.05`, where the second value is global time dilation.
  - `None` and invalid multi-tier reaction tags use the Small preset for base impact feedback. Existing invalid-tier Warning and reaction-event skip behavior remain unchanged.
  - One shared sound and one shared blood Niagara System serve all tiers. C3K does not create three sound/VFX sets.
  - Blood is a one-shot in world space at `FHitResult.ImpactPoint`, with the Niagara local `+Z` aligned to `ImpactNormal`, auto-destroyed, and never attached to a Character mesh, bone, or socket.
- Preserve all user WIP. The current worktree has 322 unrelated changes. No existing `Content/**`, Config, map, Blueprint, GA/GE, Montage, AnimBP, imported resource, generated output, or unrelated source path is an implementation or commit candidate.

## Route And Delegation

Outer: `ue-stage-workflow`
Primary: `ue5-cpp-gameplay`
Support: `game-feel`, `unreal-niagara`
Route reason: this is a narrow native C++ presentation response layered on top of the already-authoritative Health delegate. It needs explicit controller ownership and teardown for global time dilation, while the sound and Niagara burst remain authored presentation references rather than gameplay state.

Plan explorers: 0
Implementation executors: 1 (Gemini, only after plan review and explicit Main handoff)
Complex Executor: one scoped lifecycle-sensitive implementation
Main parallel work: none
Reason: the Enemy Health callback, PlayerController-owned global-dilation lifetime, and Automation assertions form one coupled single-player lifecycle. Splitting them risks two owners for a global engine value.

Main owns the C3K runtime contract, public API, GAS/context interpretation, integration acceptance, documentation, staging, and commit. Gemini may write only the exact frozen five-file source/test slice below. It must stop for any additional public surface, Tag, Config, Build.cs, asset, Blueprint, test-fixture, lifecycle, or scope need.

## Source Evidence And Frozen Runtime Contract

### Existing authority and event boundary

1. `AEnemyCharacter::OnHealthAttributeChanged()` is the only new C3K trigger point. It already rejects non-authority, teardown, destruction, lethal Health, healing/direct writes, missing `GEModData`, and dead state before its existing reaction dispatch.
2. `FMeleeHitResolver` and `FCombatProjectileHitResolver` both add their already-resolved `FHitResult` to the Damage `FGameplayEffectContextHandle` before applying the GE. C3K consumes that existing context only; it does not add a second trace, overlap, damage application, notify, or hit-routing path.
3. Existing Player target-side Overlay and CameraShake stay owned by `APlayerCharacter`. Enemy attacks against the Player keep that existing behavior and do not invoke new C3K hit-stop, sound, or blood.
4. `PolyQuest.Build.cs` already exposes `Niagara` as a private dependency. No module or Build.cs change is approved.

### Controller-owned global hit-stop

Contract owner: Main. Implementation writer: Gemini. Shared public surface: `APolyQuestPlayerController`; no Blueprint-callable API, Tag, input route, Config value, or generic subsystem is added.

1. Add one C++-only native request method to `APolyQuestPlayerController`:

   ```cpp
   void RequestCombatImpactHitStop(float DurationSeconds, float TimeDilation);
   ```

   It is not a `UFUNCTION`, Blueprint route, GameplayCue, timer, gameplay state, or reusable global-effects framework.

2. In the protected override section, add `virtual void Tick(float DeltaSeconds) override;` and private hit-stop state. In UE 5.8, `APlayerController::TickActor` invokes `PlayerTick` only when a `PlayerInput` exists but invokes `Tick` on the normal path regardless, so `Tick` is the sole C3K maintenance hook for both PIE and the no-`ULocalPlayer` Automation world. Do not also advance the same state from `PlayerTick`; that would make local controllers process expiry twice in one frame. The state records whether C3K currently owns a request, the global dilation captured before its first request, the actual dilation C3K last applied/read back, and a real-time expiry. Private helper names are implementation detail, but natural expiry and `EndPlay` must converge on one idempotent restore path.

3. Use `UWorld::GetRealTimeSeconds()` for expiry. Do not use `FTimerManager`, delay AbilityTasks, or scaled world time: those can be slowed by global dilation and leave a freeze active too long. `Tick` must call `Super::Tick(DeltaSeconds)` and then run the shared maintenance once. The maintenance ignores scaled `DeltaSeconds` for expiry and reads the current real-time clock.

4. Validate every request fail-closed: World exists, duration and dilation are finite, duration is strictly positive, and `0.0f < TimeDilation <= 1.0f`. Invalid requests do not change global dilation or retained state.

5. On the first valid request, capture the current global dilation as the restoration value. Apply the requested dilation using `UGameplayStatics::SetGlobalTimeDilation`, then read back `GetGlobalTimeDilation(World)` and record the actual value as the controller-owned value so engine clamping cannot create a false external-override mismatch.

6. While active, overlap arbitration is monotonic:
   - `AppliedDilation = min(current C3K applied dilation, requested dilation)`;
   - `ExpiryRealTimeSeconds = max(current expiry, now + requested duration)`.

   Thus a later Small hit cannot weaken or shorten an active Big/Launch stop, while later significant impacts may extend it.

7. Before extending an active request, and on every maintenance/expiry path, compare live dilation with the recorded applied value using `FMath::IsNearlyEqual(..., KINDA_SMALL_NUMBER)`. If another system has changed global dilation outside that tolerance, C3K relinquishes the old request without writing a restore. A later valid C3K request captures that external value as its new restoration baseline. On normal expiry or `EndPlay`, restore only when the live global dilation is still nearly equal to the controller-owned applied value; this preserves an external owner's intervening override.

8. `EndPlay` must clear the C3K request through the same restore helper before existing HUD teardown and `Super::EndPlay`. A request arriving after expiry must first settle the old request through the same helper, then capture the now-live baseline. No active hit-stop state may survive controller destruction, map transition, PIE teardown, or Automation world cleanup.

9. Under `WITH_DEV_AUTOMATION_TESTS`, expose only narrow test accessors/wrappers needed to drive a request and inspect active state, captured/applied dilation, and expiry. They must not exist in shipping behavior or become production gameplay controls.

### Enemy impact-feedback selection and presentation

Contract owner: Main. Implementation writer: Gemini. Shared GAS/context contract: C3K reads one already-applied `FGameplayEffectSpec` and never mutates its GE, Context, Tags, ability activation, reaction dispatch, death path, or damage result.

1. In `AEnemyCharacter::OnHealthAttributeChanged`, preserve the existing gates and `TriggerHitFeedbackOverlay()` call. Immediately after the valid nonlethal Health-decrease / `GEModData` gate and that Overlay call, derive the reaction tier once from the existing `EffectSpec` Asset Tags without emitting the existing invalid-tier Warning yet.

2. Invoke the new private C3K helper before the existing Stunned, Poise-broken, and reaction-event early returns. This deliberately makes a successful Health hit feel like impact even when a reaction Montage is suppressed.

3. The helper must require that the Effect Context Instigator:
   - is valid;
   - implements `UCombatTeamAgent`; and
   - returns the exact `Team.Player` tag.

   Enemy-sourced, untagged, invalid, or non-Player contexts are silent no-ops. Do not cast to a concrete Player class as a substitute for the Team contract.

4. Map reaction tiers to the six authorable CDO values:

   | Tier | Duration | Dilation |
   | --- | --- | --- |
   | Small | `SmallImpactHitStopDurationSeconds = 0.02` | `SmallImpactHitStopTimeDilation = 0.15` |
   | Big | `BigImpactHitStopDurationSeconds = 0.05` | `BigImpactHitStopTimeDilation = 0.05` |
   | Launch | `LaunchImpactHitStopDurationSeconds = 0.04` | `LaunchImpactHitStopTimeDilation = 0.05` |
   | None / Invalid | use the Small row | use the Small row |

   `None` and `Invalid` are feedback mappings only. The existing later branch must continue to skip `None` reaction events, and an invalid multi-tier tag must still produce its existing Warning and skip the reaction event at the same behavior boundary as before. A Stunned Enemy still returns before that warning just as it does today.

5. Resolve the local `APolyQuestPlayerController` through the World and request hit-stop only when that exact controller exists. A missing/wrong controller makes only the hit-stop request a silent no-op; it must not suppress an otherwise valid sound or blood route. Do not introduce a bare `APlayerController` fallback or a WorldSubsystem.

6. Add these `EditDefaultsOnly`, `BlueprintReadOnly`, private CDO fields to `AEnemyCharacter` under `Combat|Enemy|ImpactFeedback`, using concise Chinese `ToolTip` metadata and header forward declarations only:
   - one `TObjectPtr<USoundBase>` shared Player-on-Enemy impact sound;
   - one `TObjectPtr<UNiagaraSystem>` shared enemy blood-impact System;
   - Small/Big/Launch duration values with `Units = "Seconds"` and non-negative editor clamps;
   - Small/Big/Launch time-dilation values with an editor range that excludes zero and exceeds neither `1.0`.

   Keep concrete `USoundBase`, `UNiagaraSystem`, `UGameplayStatics`, and Niagara includes in `.cpp`. Runtime validation remains required even with editor clamps.

7. Play the configured sound once with `UGameplayStatics::PlaySoundAtLocation`. Use a finite, valid context `ImpactPoint` when available; otherwise use a finite `GetActorLocation()` fallback. A missing sound asset is an intentional silent cosmetic no-op and must not suppress valid hit-stop or blood routing. Sound-location eligibility is independent from the stricter blood HitResult actor check.

8. Spawn blood only when the existing Context has a `FHitResult` whose actor is this Enemy, whose `ImpactPoint` is finite, and whose `ImpactNormal` is finite and nonzero after normalization. Spawn the configured System with `UNiagaraFunctionLibrary::SpawnSystemAtLocation`, world transform only, `bAutoDestroy = true`, and `FRotationMatrix::MakeFromZ(NormalizedNormal).Rotator()` so Niagara local `+Z` follows the surface normal. A missing System or invalid/missing/mismatched hit result is a silent no-blood path and must not suppress valid hit-stop or sound.

9. Under `WITH_DEV_AUTOMATION_TESTS`, add lean counters and last-request values only as needed to prove C3K routing, selected preset, sound dispatch eligibility, and blood-request location/normal eligibility without loading a real sound or Niagara asset. Every such field, getter, and test wrapper must be fully macro-gated; a test dispatch record may prove eligibility even when the production asset pointer is null, but it must not become a shipping feedback registry, event bus, persistent component, or asset test seam.

### Non-goals and hard prohibitions

- No GameplayCue, new Gameplay Tag, GE change, GameplayAbility change, Damage resolver rewrite, AnimNotify, trace, overlap, or duplicate callback trigger.
- No changes to `APlayerCharacter`, Player camera shake selection, Overlay ownership, Guard/Parry resolution, Poise, stance break, launch, ragdoll, death, projectile terminal handling, collision, input, AI, replication, or persistence.
- No decals, persistent bleeding, player blood, per-bone attachment, bone-name authoring, hit numbers, camera rewrite, audio mixer/bus work, SFX variation system, or three per-tier VFX/SFX asset families.
- No world subsystem, generic impact-feedback framework, timer-based global-dilation restoration, manual `.uasset`/`.umap` changes, live Editor writes, imports, or asset migration.

## Approved Files And Executor Boundary

### Approved implementation paths

1. `Source/PolyQuest/Public/Framework/PolyQuestPlayerController.h`
2. `Source/PolyQuest/Private/Framework/PolyQuestPlayerController.cpp`
3. `Source/PolyQuest/Public/Character/Enemy/EnemyCharacter.h`
4. `Source/PolyQuest/Private/Character/Enemy/EnemyCharacter.cpp`
5. `Source/PolyQuest/Private/Tests/CombatHitFeedbackAutomationTests.cpp`

All other files are prohibited Gemini targets. In particular, `PolyQuest.Build.cs`, `PlayerCharacter.*`, both hit resolvers, reaction Ability classes, Tags, Config, DataAssets, Blueprints, maps, sound assets, Niagara assets, and project documents remain Main/user-owned and unchanged in this implementation pass.

### Required execution order

1. Implement and statically reason through the controller's request/arbitration/real-time-expiry/EndPlay ownership first, including test-only inspection hooks.
2. Add the Enemy CDO authoring fields and one private selection/dispatch helper. Integrate it at the frozen Health delegate point while retaining the current reaction classifier and Warning semantics.
3. Extend the existing `PolyQuest.Combat.HitFeedback` test rather than creating another test target. Change its local controller fixture from bare `APlayerController` to `APolyQuestPlayerController`; prove expiry through `World->Tick()` reaches the controller's `Tick` even without a `ULocalPlayer`, and do not rely on `PlayerTick` being called.
4. Add context-building helpers in the existing test only as needed to attach valid, missing, invalid, and mismatched `FHitResult` data to the real Damage GE Context. Keep test worlds isolated and restore any global time-dilation baseline before world destruction. Exercise one no-HitResult route (sound actor-location fallback and blood skip) and one valid-HitResult route (sound impact location and blood normal/location record).
5. Run only approved static checks. Do not compile, open the Editor, enter PIE, edit assets, stage, commit, reformat unrelated files, or touch the WIP tree.

## Validation Matrix

### Native Automation: `PolyQuest.Combat.HitFeedback`

Use the existing real Player/Enemy ASC fixture and the custom `APolyQuestPlayerController`. Native tests prove routing and state only; they do not prove audible mixing, sound content, Niagara renderer behavior, particle collision, or visual quality.

1. Reset Player/Enemy Health and controller global-dilation state between cases. Assert the baseline global dilation is restored after every C3K expiry/teardown path.
2. For Player-to-Enemy nonlethal Health damage, prove Small, Big, Launch, `None`, and invalid multi-tier tags select the frozen presets. `None`/invalid must select Small for C3K while leaving the existing reaction-event behavior unchanged.
3. Assert rapid-hit arbitration: later Small cannot raise Big/Launch dilation or shorten expiry; a later valid stronger/later request produces the lower dilation and later real-time expiry.
4. Tick the World across the requested real-time duration and prove exact controller restoration through the `Tick` path, including the no-`PlayerInput` fixture case. Test an external global-dilation replacement during an active stop: maintenance, expiry, and `EndPlay` must preserve that external value rather than overwrite it. Assert that one World tick cannot advance expiry twice.
5. Build a valid Context `FHitResult` targeting the Enemy and assert C3K records one eligible blood request at the exact finite ImpactPoint with a normalized normal; also assert eligible sound dispatch uses the impact location. Build a no-HitResult case and assert sound uses the Enemy actor location while blood is skipped.
6. Verify nonfinite/zero normal, nonfinite point, and a HitResult targeting another Actor skip only blood eligibility; valid Player-to-Enemy hit-stop/sound routing remains intact. Sound and blood test records must be independent, and all records/getters remain `WITH_DEV_AUTOMATION_TESTS`-gated.
   A missing local `APolyQuestPlayerController` may suppress only hit-stop; it must not become an implicit gate for valid sound/blood dispatch.
7. Verify no C3K route for Player-target damage, Enemy/invalid-team source, direct Health writes, positive Health GE/healing, Poise-only GE, successful Guard/Parry consumption, invulnerability, lethal damage, already-dead state, teardown, or duplicate/non-damaging callbacks.
8. Retain and re-run existing Overlay/CameraShake assertions. Confirm C3K did not cause Player damage to change CameraShake behavior or introduce hit-stop/blood when an Enemy damages the Player.

### Static gate before user validation

- Use CodeGraph to inspect the final `OnHealthAttributeChanged`, controller request/expiry/EndPlay call path, and `CombatHitFeedbackAutomationTests` fixture. Use code-review-graph only as supplemental diff/impact evidence when its index matches the baseline.
- Run Rider `get_file_problems` or `lint_files` on all five touched C++ files, inspect final header/include boundaries, and run `git diff --check` for the approved paths.
- Confirm no Build.cs, Tag, Config, player-feedback, resolver, GE/GA, damage, asset, Blueprint, map, or unrelated-WIP diff was introduced.

### User-owned Editor and visual gates

1. Manually compile `PolyQuestEditor (Development Editor)` in Visual Studio 2022 and report the exact result.
2. Run `PolyQuest.Combat.HitFeedback`, then the full Editor Automation matrix.
3. In `BP_Enemy_Goblin`, assign one shared impact `USoundBase` and one shared non-looping `UNiagaraSystem` to the new CDO fields. The candidate `Content/Audio/MetaSounds/HitSounds/SFX_HitFlesh.uasset` may be used only after Editor readback confirms it is a valid `USoundBase`. Create/configure `Content/Effects/Niagara/NS_EnemyBloodImpact.uasset` as local user-owned WIP with bounded, one-shot lifetime; do not add it to this source/test commit.
4. In `Scene01`, test melee and Bow nonlethal Small/Big/Launch hits repeatedly. Tune only the six CDO values and the two local assets until the stop reads as impact without making input, Ability cleanup, projectile trail fade, ragdoll, or normal locomotion feel stuck.
5. Verify blood spawns at the actual world impact surface, points outward along the surface normal, fades/destroys naturally, and does not follow an enemy after movement or ragdoll.
6. Verify Player-target damage has no new C3K hit-stop/sound/blood; Guard/Parry and invulnerable contacts have none; lethal Enemy hits still transition through existing ragdoll without a C3K blood burst; all existing Overlay/CameraShake/reaction behavior remains intact.

## Documentation, Debt, And Commit Boundary

- Gemini must not modify project documentation during implementation. Main updates `ROADMAP.md`, `ARCHITECTURE.md`, and `README.md` only after user validation and Main fresh review have evidence.
- After accepted validation/review, document the stable ownership rule: Enemy Health/context selects immediate impact presentation, while `APolyQuestPlayerController` solely owns C3K global-dilation lifetime. Then move C3K to Done in `ROADMAP.md` and retain this completed closeout until the next accepted stage replaces it.
- GameplayCue is deliberately rejected in v1. Re-evaluate it only when PolyQuest adds multiplayer prediction/replication, a duration-owned cosmetic state, or several independent systems that need a shared GAS cosmetic-event contract.
- The sound, Niagara System, `BP_Enemy_Goblin` assignment, and all other Content changes are local authoring WIP and remain excluded. The later source/test commit may include only the five approved paths plus Main-owned documentation after explicit user approval and a scoped staged-diff review. `git add -A` is prohibited.

## Closeout Record

- Implemented surface: the approved `APolyQuestPlayerController.*`, `AEnemyCharacter.*`, and `CombatHitFeedbackAutomationTests.cpp` only. No GameplayCue, Tag, Config, Build.cs, resolver, GE/GA, asset, Blueprint, map, or generic feedback framework was introduced.
- User validation: `PolyQuest.Combat.HitFeedback` completed with `Success`; focused PIE confirmed global hit-stop, shared flesh sound, world-space blood burst, and Player-target exclusion. The known test-fixture HUD warning and invalid multi-tier reaction warnings remain expected negative-path coverage, not runtime failures.
- Main review: the first fresh review found a stale cross-execution `FGameplayEffectSpec` identity cache and an external time-dilation tolerance gap. The final source uses execution-local `ModifiedAttributes` state for multi-Health-modifier deduplication, `UGameplayStatics` for all global-dilation writes, and `KINDA_SMALL_NUMBER` consistently. Delta fresh review found no P0-P2.
- Debt handoff: `TODO-02C3L: Eight-Directional Small/Big Hit Reactions v1` is the next accepted presentation stage before `TODO-03C`; Launch remains owned by C3I/C3J and is explicitly excluded.
- Commit scope: the five approved native source/test paths plus `plan.md`, `ROADMAP.md`, `ARCHITECTURE.md`, and `README.md`; all mutable `Content/**`, Config, project, map, Blueprint, AnimBP, Montage, GA/GE, and imported-resource WIP remain excluded.
