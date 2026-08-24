# TODO-07B2: Melee Weapon Trail v1

## Plan State

- Status: Complete. This record preserves the accepted B2 contract and closeout evidence; the approved source/test slice and documentation are ready for the focused commit boundary below.
- Baseline: `1f80cf5` (`[Feature] 三档受击镜头抖动 (Reaction-Tier Hit Camera Shake)`).
- Objective: add one Niagara-only white melee trail to every current Player and Enemy melee Trace Window. The trail follows the existing world-space Blade Base/Tip samples, starts only while the exact active `UAbilityTask_MeleeTraceWindow` exists, and retains a short particle fade after that window closes.
- Preserve all user-owned WIP. Do not modify, stage, move, delete, or infer behavior from unrelated `Content/**`, Config, maps, Blueprints, input, AnimBPs, GA/GE/Montage assets, `.uproject`, generated output, or external imported-resource changes.

```text
Outer: ue-stage-workflow
Primary: unreal-niagara
Support: ue5-cpp-gameplay, ue5-debug-validation
Route reason: the player-facing result is a Niagara System driven continuously by existing combat samples, while the narrow native work owns the component, AbilityTask lifetime, test seam, and no-regression validation.
```

```text
Plan explorers: 0
Implementation executors: 1 (Gemini only after its read-only plan review is accepted and the user explicitly authorizes execution)
Complex Executor: Gemini external executor for one frozen Task/component lifecycle slice
Main parallel work: none
Reason: Trace Window, Character component ownership, Niagara module boundary, and Automation teardown form one lifecycle-sensitive integration. Main owns the contract, documentation, validation interpretation, fresh review, staging, and commit; Gemini may write only the frozen source/test slice.
```

## Evidence And Decisions

- The current live source has one shared `UMeleeTraceSourceComponent` on `ABaseCharacter`. `UAbilityTask_MeleeTraceWindow::Activate()` captures initial Blade Base/Tip endpoints; `TraceCurrentSegment()` captures their current endpoints once per task tick and then performs the unchanged prior-to-current sweep plus `FMeleeHitResolver` delivery.
- The five current callers of that Task are `ULightAttackAbility`, `UChargedAttackAbility`, `USprintAttackAbility`, `UPlayerMeleeSkillAbility`, and `UEnemyMeleeAbility`. Player dynamic equipment endpoints and the Enemy static/fixed compatibility route already converge at `TryGetBladeEndpoints()`.
- `PolyQuest.Build.cs` now carries only the private Engine `Niagara` module dependency. `NiagaraToolsets` in the project file remains outside B2.
- There is no current `GameplayCue` or `GameplayCueManager` route in `Source/`, `Config/`, or `PolyQuest.uproject`. B2 does not establish one.
- The user selected coverage for Player and current Enemy melee, not Player-only presentation. When a Trace Window ends, the system stops emitting through normal Niagara deactivation; particles already emitted fade naturally. Initial authored targets are `0.08s` for the blade sheet and `0.12s` for the farther tip accent.
- The user authored local `Content/_FeedBack/Materials/M_MeleeTrail_White` and `Content/_FeedBack/Niagara/NS_MeleeWeaponTrail` assets, then assigned the System to the inherited component on `BP_Player` and `BP_Enemy_Goblin`. Those mutable `Content/**` assets remain user-owned WIP and are excluded from this source/test/docs commit.

## Frozen Runtime Contract

1. Add one native-only `UMeleeWeaponTrailComponent : UNiagaraComponent` under `Source/PolyQuest/Private/Combat/Melee/`. `ABaseCharacter` creates exactly one `MeleeWeaponTrail` default subobject, attaches it to the Character Root only for lifetime ownership, and exposes it as an inherited visible component. It is configured with `bAutoActivate = false`, `bAutoDestroy = false`, and `bAutoManageAttachment = false`.
2. The component must never attach to, move, reparent, query, or change `WeaponMesh`, `BladeTraceBase`, or `BladeTraceTip`. It receives only the current world-space positions through the exact Niagara User parameters `User.BladeBase` and `User.BladeTip`. Its Root attachment is not a transform source for the effect.
3. The component exposes only narrow non-Blueprint C++ requests to start, update, and end a trail for one requester token. It stores that token as a weak `UObject` reference. An update or end request from an older Task must do nothing after a newer Task has taken ownership. A missing Niagara System is a silent visual no-op: it must not log, block a Trace Window, change hit delivery, or leave an active requester.
4. `UAbilityTask_MeleeTraceWindow` is the sole B2 runtime caller. After successful initial endpoint capture in `Activate()`, it resolves the avatar's root-owned Trail component and starts it with that initial pair. It caches only a weak component reference for teardown.
5. In `TraceCurrentSegment()`, after current endpoint capture and before the existing sweep loop, the Task updates the same trail with the already captured pair. It must not make another endpoint query, change the start/end Sweep samples, subdivisions, collision channel, damage effect, target set, hit resolver, or Trace Window Gameplay Event contract.
6. In `OnDestroy()`, the Task ends only its own trail request before it clears Task state and calls `Super::OnDestroy()`. This covers normal Notify end, `EndAbility`, cancellation, invalid endpoint shutdown, and late teardown. It uses normal `Deactivate()` only; B2 must not call `DeactivateImmediate()` to force a hard visual cut. Actor/component destruction may discard residual particles with its owner, but cannot dereference a stale Task or stale component.
7. B2 does not activate from `State.Action.Attacking`, Montages, AnimNotifies, raw input, impact delivery, Guard, Bow Draw/Hold/Release, Projectile, Targeting, or a generic timer. There is no second trace path and no gameplay effect, tag, input, ability, movement, collision, damage, target-selection, camera, or replication change.
8. B2 does not introduce GameplayCue/Cue Notify assets, cue paths, cue tags, `GameplayCueManager` configuration, or a generic presentation dispatcher. The current Overlay, Player CameraShake, and Trail each retain their existing local owner and exact teardown path.

## Approved Source And Test Surface

**Contract owner: Main. Implementation writer: Gemini only after explicit execution authorization.**

- `Source/PolyQuest/PolyQuest.Build.cs`
  - Contract owner: Main. Implementation writer: Gemini.
  - Add exactly `"Niagara"` to `PrivateDependencyModuleNames`; do not add a public dependency, plugin declaration, `NiagaraToolsets`, or `.uproject` change.
- `Source/PolyQuest/Private/Combat/Melee/MeleeWeaponTrailComponent.h/.cpp` (new)
  - Contract owner: Main. Implementation writer: Gemini.
  - Define the private component and its exact requester-scoped start/update/end lifecycle. It may contain `WITH_DEV_AUTOMATION_TESTS` read-only observation/configuration support only for the named B2 Automation suite; it must not expose Blueprint, gameplay, asset-loading, or global presentation APIs.
- `Source/PolyQuest/Public/Character/BaseCharacter.h` and `Source/PolyQuest/Private/Character/BaseCharacter.cpp`
  - Contract owner: Main. Implementation writer: Gemini.
  - Add only the private `MeleeWeaponTrail` default-subobject declaration and constructor creation/root attachment. Do not change ASC, Attributes, Mesh Overlay, MoveSpeed, fixed weapon display, BeginPlay, Possession, EndPlay, combat-team, or public callable API behavior.
- `Source/PolyQuest/Public/AbilitySystem/Tasks/AbilityTask_MeleeTraceWindow.h` and `Source/PolyQuest/Private/AbilitySystem/Tasks/AbilityTask_MeleeTraceWindow.cpp`
  - Contract owner: Main. Implementation writer: Gemini.
  - Add only the weak Trail-component cache and calls in `Activate()`, `TraceCurrentSegment()`, and `OnDestroy()` described above. The two `OpenMeleeTraceWindow()` signatures and every existing hit/trace input remain frozen.
- `Source/PolyQuest/Private/Tests/TestMeleeTrailAbility.h/.cpp` (new)
  - Contract owner: Main. Implementation writer: Gemini.
  - Create a native test-only ability that owns the real `UAbilityTask_MeleeTraceWindow` for Automation. It is not a production Ability, not a Blueprint asset, and does not duplicate Task logic.
- `Source/PolyQuest/Private/Tests/MeleeWeaponTrailAutomationTests.cpp` (new)
  - Contract owner: Main. Implementation writer: Gemini.
  - Add the thirteenth suite, `PolyQuest.Melee.WeaponTrail`, using a real ASC-hosted Task and the existing deferred-spawn combat fixture.
- `Source/PolyQuest/Private/Tests/CombatAutomationFixture.cpp` and `Source/PolyQuest/Private/Tests/CombatAutomationFixture.h`
  - Contract owner: Main. Implementation writer: Gemini only if a minimal pre-`FinishSpawning()` test-only Trail configuration is required by the chosen observation seam.
  - Do not migrate unrelated fixtures or add a test Content dependency. If a valid no-render native seam can avoid this file, leave it unchanged.

No unlisted source, Header/public API, Gameplay Tag, Input, Config, project, or asset file is approved. An executor that needs one must stop and return the exact evidence to Main rather than broadening the implementation.

## Niagara Asset Authoring (User-Owned)

After native source passes static review, the user authors the following assets in the Unreal Editor under the existing feedback root:

| Asset | Required role |
| --- | --- |
| `Content/_FeedBack/Materials/M_MeleeTrail_White` | Unlit, Additive white trail material with alpha-over-life support; it is presentation only and has no collision or gameplay readback. |
| `Content/_FeedBack/Niagara/NE_MeleeTrail_BladeSheet` | CPU Ribbon emitter that consumes `User.BladeBase` and `User.BladeTip` as world-space samples and renders a broad swept blade sheet. Its width/orientation must span the actual Blade Base-to-Tip segment rather than render a single centerline. |
| `Content/_FeedBack/Niagara/NE_MeleeTrail_TipAccent` | CPU Ribbon emitter that follows the Blade Tip only, is visibly narrower, and has the longer particle lifetime. |
| `Content/_FeedBack/Niagara/NS_MeleeWeaponTrail` | System containing the two emitters and exposing exactly `User.BladeBase` / `User.BladeTip` Position parameters. |

Authoring constraints:

- Both emitters are CPU simulation with Local Space disabled. The System consumes the supplied positions as world positions; the native component's Root attachment must not double-transform the trail.
- Start with BladeSheet lifetime `0.08s` and TipAccent lifetime `0.12s`, Additive white output, and alpha fading to zero over life. The visible farther tip is therefore longer than the base side without a separate gameplay route.
- Use fixed sensible bounds appropriate to the weapon swing. Do not add GPU simulation, collision, Niagara Gameplay Events, event handlers, data-interface readback, socket sampling, or a second position source.
- `Deactivate()` must stop new particle emission while the existing particles finish their configured lifespan. Do not use a hard kill/Immediate deactivation to compensate for bad lifetime tuning.
- Assign the same authored `NS_MeleeWeaponTrail` to the inherited `MeleeWeaponTrail` component on `BP_Player` and `BP_Enemy_Goblin`. Do not attach the System to a mesh/socket and do not alter weapon trace marker authoring.

These assets are user-owned mutable `Content/**` work and remain outside the default B2 commit unless the user later gives separate explicit approval for a stable asset closure.

## Automation And Validation

### Native Automation

Add `PolyQuest.Melee.WeaponTrail`; it must use a real AbilitySystemComponent, `UTestMeleeTrailAbility`, and `UAbilityTask_MeleeTraceWindow`, never a hand-written approximation of Task behavior. The test may use a `WITH_DEV_AUTOMATION_TESTS` observation seam on the component, but it must not require a `.uasset`, a viewport, a GPU simulation, or a test switch that changes production no-asset behavior.

It must cover all of the following:

- The BaseCharacter-owned component is root-attached and starts with Auto Activate, Auto Destroy, and Auto Manage Attachment disabled.
- A configured test path receives the initial Blade Base/Tip pair, receives a Task-tick update from the same captured current pair, and observes normal deactivation after normal Task end/cancellation.
- An older Task's late `OnDestroy()` cannot deactivate the newer Task's active trail. The newer Task can still end its own request.
- Invalid endpoint shutdown and Player/Enemy destruction clear the current request safely without stale dereference or surviving active state.
- With no Niagara System assigned, opening/ticking/ending a real Trace Window remains functional, produces no new configuration Warning, and does not alter trace endpoints, hit acceptance, damage delivery, or existing Trace Window state.
- The suite leaves no Auto-destroyed component, no duplicated Task, no trail activation outside the Trace Window, and no H3 fixture-signal warning. Do not hide output with `AddExpectedError`, a lowered log category, or a test-only runtime fallback.

Run the new suite plus the twelve existing suites through the Unreal Editor Automation front end:

`PolyQuest.Equipment.TransactionMatrix`, `PolyQuest.Melee.TraceSourceGeometry`, `PolyQuest.Player.ActionWindows`, `PolyQuest.Combat.HitReaction`, `PolyQuest.Enemy.AttackSetSelection`, `PolyQuest.Enemy.CombatSpacing`, `PolyQuest.UI.VitalHUD`, `PolyQuest.Player.Exhaustion`, `PolyQuest.Player.LockOn`, `PolyQuest.Projectile.Lifecycle`, `PolyQuest.Projectile.TargetAssist`, and `PolyQuest.Combat.HitFeedback`.

Any retained warning must map to the existing intentional negative assertion ledger: invalid multi-tier reaction tags, Stance Break fallback, equipment preflight/rollback, or invalid static trace geometry. B2 adds no successful-path missing-Niagara warning.

### Static And User Gates

Before asking for user validation, Main/Gemini must read all changed source and direct Task/component callers/callees, use CodeGraph, run a scoped code-review-graph impact read against `1f80cf5` if its index covers the baseline, run Rider error-level inspection on touched C++ paths, and run `git diff --check`. These are static gates only; they are not compile or visual evidence.

User-owned validation after source static preflight:

1. Editor readback: verify both inherited components point to `NS_MeleeWeaponTrail`; the System has exactly the two required User Position parameters, CPU Ribbons, Local Space off, no gameplay events/readback/collision, and the requested `0.08s` / `0.12s` fade relationship.
2. Compile `PolyQuestEditor` manually and report the result.
3. Run all thirteen Automation suites in the Unreal Editor front end and preserve raw logs.
4. PIE in `Scene01`: verify Player Light, Charged, Sprint, and current Player Melee Skill windows; current Enemy melee; the blade sheet spans the weapon; the tip accent remains longer; and a normal close/cancel blends out naturally.
5. PIE regressions: idle, ordinary locomotion, Guard/Parry, Bow Draw/Hold/Release, Projectile flight/impact, hit reaction, any `State.Action.Attacking` interval outside a Trace Window, weapon switching, no target, and rejected target behavior show no unintended trail or changed damage/target selection. Destroy/teardown must not leave a visual component or stale activation.

## GameplayCue Decision

Do not introduce GameplayCue merely because PolyQuest now has several visual effects. The deciding factor is ownership and dispatch topology, not effect count:

- Mesh Overlay is an `ABaseCharacter` material/timer lifecycle that restores the prior Overlay.
- Camera Shake is Player-local and tracks one exact CameraManager-owned instance across tier replacement, UnPossess, and EndPlay.
- B2 Trail requires per-Task, per-tick endpoint updates and stale-requester protection.

Putting these into Cue Notifies now would add a second dispatch/mapping lifecycle, obscure the existing precise teardown owners, and force Cue assets/configuration without providing a current single-player benefit. Reconsider a dedicated GameplayCue adoption gate only when one GameplayEffect/GameplayEvent needs data-driven fan-out to shared impact VFX, audio, and presentation across several recipients, or when multiplayer prediction/replication becomes an accepted project boundary. That future gate must define cue ownership, asset paths, stacking/removal semantics, and validation before any migration; it is not part of B2.

## Review, Closeout, And Commit Boundary

- Gemini first performs a strict read-only plan review. It may inspect the approved source/direct dependencies and Engine API facts, but it must not edit code, documents, assets, Config, project files, or Git state; it must not compile, launch the Editor, run PIE, or claim user evidence.
- After explicit execution authorization, Gemini performs a strict implementation self-review limited to the frozen paths. It is not an independent fresh review. Main validates the report against the repository, interprets user compile/Automation/PIE evidence, and performs one defect-first fresh review after accepted validation.
- Only after that review and the mandatory roadmap debt-handoff check does Main update `README.md`, `ARCHITECTURE.md`, `ROADMAP.md`, and this B2 closeout record.
- Default commit boundary: only B2 C++, Automation, and the four project documents. Exclude all `Content/**`, Config, maps, Blueprints, Input, AnimBPs, GA/GE/Montage assets, `.uproject`, generated output, imported resources, and unrelated user WIP. Do not commit until the user explicitly approves it.

## Closeout Record

- Runtime ownership: `ABaseCharacter` owns one root-attached, inactive `UMeleeWeaponTrailComponent`. `UAbilityTask_MeleeTraceWindow` is its sole runtime caller: it starts after the existing initial endpoint capture, updates from the same current endpoint pair before the unchanged Sweep/Resolver loop, and ends only its own weak requester token from `OnDestroy()`. A missing Niagara System is a silent visual no-op; normal `Deactivate()` stops emission without a hard kill. No GameplayCue, Tag, Input, damage, targeting, collision, or second trace route was added.
- Automation: new `PolyQuest.Melee.WeaponTrail` uses a real ASC-hosted `UTestMeleeTrailAbility` and covers component defaults, initial/continuous endpoint forwarding, stale Task A versus active Task B requester arbitration, active Player endpoint invalidation followed by valid recovery, Player and content-free Enemy real-Task destruction, and the no-asset path. The endpoint-invalidation case deliberately makes the two markers coincide, proves `TraceCurrentSegment()` closes the Task and clears its requester, then proves a subsequent valid Task opens normally.
- User evidence: the user confirmed focused `Scene01` PIE visual validation for the authored trail route and the Unreal Editor Automation front-end matrix of thirteen suites, including `PolyQuest.Melee.WeaponTrail`, as Success. The final endpoint/Enemy-teardown repair is Automation-only and does not change the already validated runtime/asset route.
- Static/fresh review: Main re-read the final Task/component/test call path with CodeGraph, used code-review-graph as supplemental impact evidence, and inspected the untracked Automation source directly where graph macro coverage was incomplete. Rider error-level inspections returned zero errors for the Task and new suite; `git diff --check` passed. The defect-first fresh review found no P0-P2.
- Warning ledger: retained warnings map to deliberate negative assertions only: invalid multi-tier reaction tags, Enemy Stance Break fallback, equipment preflight/active-swap/rollback, invalid static trace geometry, and the new explicit coincident blade-marker signal in `PolyQuest.Melee.WeaponTrail`. No missing-fixture, missing-Niagara, or `Invalid AbilitySpecHandle` success-path warning is accepted.
- Scope and debt handoff: the focused commit contains only B2 C++, Automation, and these four documents. All `Content/**` assets and Blueprint assignments, Config, maps, `.uproject`, generated files, imported resources, and unrelated WIP remain excluded. No unresolved B2 runtime risk or validation debt requires a new Roadmap entry.
