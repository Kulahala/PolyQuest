# TODO-03B-3: Player Bow Locked-Target Preference v1

## Plan State

- Status: Complete. The user confirmed focused PIE plus `PolyQuest.Projectile.Lifecycle`, `PolyQuest.Projectile.TargetAssist`, and `PolyQuest.Player.LockOn` Automation as `Success`. The prior `Projectile.Lifecycle` fixture-only `Bow_L` failure was repaired by using the existing read-only Hero Mesh `Weapon_R` equipment socket; no authored asset was modified or staged.
- Baseline: `8f74187` (`[Feature] 完成锁定移动朝向与死亡交接 / Complete Locked Locomotion Facing And Death Retarget`).
- Objective: at Player Bow Release, prefer one still-valid B2 locked Enemy as the existing projectile target snapshot while retaining mouse-pointer initial flight direction and every established Projectile/Homing lifecycle rule.
- Current `Content/**`, Config, maps, authored Blueprint/UMG/input/GA/GE/Montage/AnimBP assets, project files, and all other worktree changes are user-owned WIP. Preserve and exclude them.

~~~text
Outer: ue-stage-workflow
Primary: ue5-cpp-gameplay
Support: ue5-debug-validation
Route reason: B3 changes the narrow Player lock-resolution surface, Bow GAS Release target source, and Projectile lifecycle Automation without changing the underlying projectile runtime.
~~~

~~~text
Plan explorers: 0
Implementation executors: 0
Complex Executor: none
Main parallel work: none
Reason: Player public C++ querying, Bow Ability, and existing lock/projectile lifecycle contracts intersect at Main-only integration boundaries. Existing source inspection resolved the material design choices.
~~~

## Locked Product Contract

1. `APlayerCharacter` adds the C++-only `ResolveValidLockedTarget()` query. It executes the existing B2 lock validation once, returns the current target after either normal validation or successful B2 death handoff, and returns null after a clear. The existing raw `GetLockedTarget()` weak-reference read remains unchanged. No Blueprint API or generic targeting framework is added.
2. `UBowDrawFireAbility::SpawnProjectile()` keeps pointer direction as `InitialFlightDirection`. Only when `bEnableTargetAssist` is true does it first ask `ResolveValidLockedTarget()`. A finite resolved target Aim Point becomes the existing `TargetActor` and `InitialTargetAimPoint` snapshot.
3. A valid B2 lock wins over the old automatic query. It does not need to satisfy the pointer cone, automatic-candidate ordering, or visibility ranking again. The existing flight-time max distance/max height, dead/invulnerable, turn-rate, duration, and turn-budget checks remain unchanged.
4. If there is no valid lock, or its aim point is non-finite, Bow performs its unchanged one-time `TryFindBestTargetCandidate()` query with the existing geometry, 6 percent viewport overscan, and Visibility rules. No valid lock must never block Release.
5. If `bEnableTargetAssist` is false, Bow neither resolves a lock nor runs automatic selection. `bEnableLimitedHoming` still requires Target Assist by the existing `UProjectileDefinition` validation. No global defaults or DataAsset schema changes are in this stage.
6. A launched Projectile retains its Release-time target Actor snapshot. Later lock cycling, clearing, or B2 death handoff never re-targets that Projectile; its existing moving-target tracking and terminal straight-flight behavior remain authoritative.
7. Do not change `FCombatProjectileLaunchRequest`, `ACombatProjectile`, `UProjectileDefinition`, Tags, Input, Camera, collision, Damage GE delivery, Enemy Projectiles, or Bow Draw/Hold facing.

## Approved Source And Test Surface

- `Source/PolyQuest/Public|Private/Character/Player/PlayerCharacter.*`
  - Add `ResolveValidLockedTarget()` as a non-UFUNCTION C++ query that centrally reuses `ValidateCurrentLockedTarget()`.
- `Source/PolyQuest/Public|Private/AbilitySystem/Abilities/BowDrawFireAbility.*`
  - Add the release-time priority selection path and a `WITH_DEV_AUTOMATION_TESTS`-only screen projection hook for the existing local target-assist filter. It is not a runtime or authored API.
- `Source/PolyQuest/Private/Tests/ProjectileLifecycleAutomationTests.cpp`
  - Add an isolated transient Bow/lock fixture and B3 target-source assertions. The equipment path uses the same read-only Hero skeletal-mesh fixture as `PolyQuest.Equipment.TransactionMatrix` so its owner-socket preflight and display-socket chain are real; it does not modify or stage any Content asset.

## Validation Matrix

### Static And Automation

1. `PolyQuest.Projectile.Lifecycle` covers: lock priority over a pointer-favored automatic candidate; pointer initial direction preservation; no-lock automatic fallback; Target Assist disabled ignores a lock; invalid lock clearing/fallback; B2 death handoff selection; post-Release Player lock switch/clear leaves the Projectile target snapshot unchanged.
2. Rerun `PolyQuest.Projectile.TargetAssist` and `PolyQuest.Player.LockOn` for selection, Homing/U-turn, and B2 lifecycle regression coverage.
3. Read final callers/callees, run Rider error-level inspection on touched C++, run `git diff --check`, and use code-review-graph only as supplemental diff-impact evidence. Do not invoke UBT, UAT, packaging, or an Editor build without explicit user delegation.

### User-Owned Editor, Compile, And PIE

1. In `Content/_DataAssets/Weapon/DA_Projectile_Arrow`, confirm `bEnableTargetAssist=true` and `bEnableLimitedHoming=true`, with finite positive Target Assist/Homing values, and confirm `Content/_DataAssets/Weapon/DA_Weapon_Bow` references it. Rider's offline property reader did not expose these fields, so Editor readback is the source of truth.
2. Manually compile `PolyQuestEditor`.
3. In PIE or Standalone verify: no-lock mouse assist; locked target priority while the mouse points toward another Enemy; post-Release lock switch/clear/death does not retarget an existing arrow; invalid lock does not suppress Bow Release; initial arrow direction remains mouse-driven; existing limited-turn and U-turn behavior remain unchanged.

## Closeout And Commit Boundary

1. Main performed the AGENTS.md default single defect-first fresh review. No P0-P2 source defect was found: enabled Target Assist alone reads the current B2-validated lock once, preserves pointer-derived initial flight, falls back to B2 automatic selection on no/invalid lock, and leaves Target Assist-disabled direct flight unchanged. `FVector::ContainsNaN()` was verified against UE 5.8 headers to reject both NaN and Inf aim points. code-review-graph reported low impact but does not recognize the Unreal Automation macro body as coverage, so direct test/source review remains authoritative.
2. Rider error-level inspection reported no errors for the five B3 C++ files; `git diff --check` passed. Source call-path review confirmed `SpawnProjectile -> ResolveValidLockedTarget -> ValidateCurrentLockedTarget`, and the release request remains the sole target snapshot handoff to `ACombatProjectile`. User runtime evidence is limited to the reported Automation and PIE results; no separate B3 Editor property readback or standalone compile result is claimed.
3. Documentation is synchronized in `README.md`, `ARCHITECTURE.md`, and `ROADMAP.md`; B3 is marked complete. `TODO-03H2: Core Combat Runtime Health And Lean Review v1` is the next gate before the next gameplay feature. It owns the audit of recurring fixture/log signal quality, including the direct-test `Invalid AbilitySpecHandle` warnings observed while `SpawnProjectile()` returns its normal level-one fallback; this is recorded as an audit item, not misreported as a B3 runtime defect.
4. Default commit scope is B3 C++, Automation, and documentation only. Exclude all `Content/**`, Config, maps, Input/Widget/AnimBP, project files, generated directories, and unrelated WIP unless later explicitly approved.
