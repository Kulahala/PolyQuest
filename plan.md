# TODO-02C3M: Parry Success Impact Feedback v1

## Plan State

- Status: Completed / user-validated / Main fresh-reviewed / closeout committed.
- Baseline: `024a374494ecca9525ff8607b07c402e4247582b`.
- Repository: `E:\GameDevelop\PolyQuest` (UE 5.8, runtime module `PolyQuest`).
- Preserve all existing user WIP. The C3M native/test slice is joined at closeout by the two user-approved C3K tuning files (`EnemyCharacter.h` and `CombatHitFeedbackAutomationTests.cpp`) as one explicit impact-feedback tuning freeze. `AGENTS.md`, `Config/**`, `Content/**`, generated folders, and all other unrelated source changes remain excluded.
- Objective: make a successful Player Parry feel decisive with a short hit-stop, the existing Player Big-hit camera shake, and an optional Parry-owned sound, while making the shared defense boundary explicitly reject Parry for projectile contacts.
- Player-facing success: one valid melee Parry consumes the contact, leaves Player Health and Guard Stamina unchanged, applies the existing counter-Poise behavior when an attacker ASC exists, and emits a brief freeze/camera/audio response at the contact location. A projectile striking during the Parry window must not be Parried; it must retain the existing Guard-or-damage route.
- Approved tuning freeze: Parry uses `0.05s / 0.03`; the accompanying C3K presets are Small `0.03s / 0.1`, Big `0.05s / 0.03`, and Launch `0.05s / 0.05`. These values are user-approved presentation tuning, not an implementation defect.

## Route And Delegation

Outer: `ue-stage-workflow`
Primary: `ue5-cpp-gameplay`
Support: `game-feel`, `ue5-debug-validation`
Route reason: this is a narrow native GAS/resolver lifecycle change plus presentation reuse. The existing Controller hit-stop and Player camera-manager ownership are sufficient; no new feedback framework, GameplayCue, input route, tag, replication path, or asset system is needed.

Plan explorers: 0
Implementation executors: 1 (Gemini)
Complex Executor: one scoped lifecycle-sensitive implementation (Gemini; the Parry/resolver boundary and feedback cleanup must be treated as one contract)
Main parallel work: none
Reason: the changed paths share one synchronous defense decision and one Player-owned presentation call. A single bounded executor can implement the frozen slice without competing writers; Main retains contract, asset, validation, documentation, staging, and commit ownership.

## Ownership And Handoff Boundary

- Contract owner: Main. Main owns the public signatures, Parry-vs-projectile policy, feedback order, defaults, GAS/ASC semantics, test acceptance, and any scope decision.
- Implementation writer: Gemini. Gemini may edit only the explicitly approved source/test paths below, and only the named functions or test seams. It must not change the policy while implementing.
- User-owned Editor work: if the current authored Parry Gameplay Ability/Blueprint exposes the new field, assign `ParrySuccessSound` there if desired; save any mutable assets, compile `PolyQuestEditor (Development Editor)` in Visual Studio 2022, run Automation in the Unreal Editor, and perform Scene01 PIE/audio/visual verification.
- Main-owned work after validation: fresh defect-first review, `plan.md`/`ROADMAP.md`/`ARCHITECTURE.md`/`README.md` synchronization, explicit-path staging, and commit preparation. Gemini must not edit project documentation, stage, or commit.
- No executor may modify `AGENTS.md`, `ROADMAP.md`, `ARCHITECTURE.md`, `README.md`, `plan.md`, `Config/**`, `Content/**`, `.uasset`, `.umap`, AnimBP/Montage assets, `PolyQuestPlayerController` production code, Enemy feedback code, `PolyQuest.Build.cs`, Gameplay Tags, or Input configuration.

## Current Evidence And Call Chain

1. Native melee delivery currently follows `FMeleeHitResolver::TryResolveHit` -> `APlayerCharacter::TryResolveIncomingDefense` -> `UPlayerParryAbility::TryParryMeleeHit`. A successful Parry is terminal for that trace contact.
2. Native projectile delivery follows `FCombatProjectileHitResolver::TryResolveHit` -> the same `TryResolveIncomingDefense` entry. C3M now passes `bAllowParry = false`, so projectiles skip Parry and retain the Guard-or-damage route.
3. `UPlayerParryAbility::TryParryMeleeHit` already owns the front-arc/window gate, optional attacker Poise counter, and contact-consumption return value. It currently has no success presentation.
4. `APolyQuestPlayerController::RequestCombatImpactHitStop(float, float)` already owns global time-dilation arbitration, real-time expiry, external-dilation detection, and teardown restoration. Do not add another timer or Controller API.
5. `APlayerCharacter::TriggerHitFeedbackCameraShake(EHitReactionTier)` already owns local-controller checks, Big/Small/Launch class selection, single-instance replacement, and camera-manager cleanup. The Parry path needs only a narrow C++ wrapper that selects the existing Big class; it must not expose a general new tier or duplicate the manager lifecycle.
6. Existing C3K impact feedback is damage-path-owned on Enemy and is intentionally separate. Parry success is a defense contact event, not Health damage, so it must not dispatch a Health reaction, blood, overlay, or Enemy feedback.

## Frozen Product Decisions

### 1. Parry eligibility and projectile policy

- Only a resolver-approved **melee** contact may call `TryParryMeleeHit`.
- The shared Player entry becomes:

```cpp
bool TryResolveIncomingDefense(
    AActor* AttackingActor,
    float GuardStaminaDamage,
    const FHitResult& HitResult,
    bool bAllowParry);
```

- `FMeleeHitResolver` passes `Request.HitResult` and `bAllowParry = true`.
- `FCombatProjectileHitResolver` passes `Request.HitResult` and `bAllowParry = false`.
- When `bAllowParry` is false, skip the Parry lookup entirely and continue to the existing Guard path; if Guard does not consume the projectile contact, apply the existing projectile GameplayEffect unchanged.
- When `bAllowParry` is true and an active Parry instance is found, preserve the current return semantics: return the Parry result directly. A failed/invalid active Parry does not silently fall through to Guard, matching the established melee behavior. `TryGuardIncomingMeleeHit` keeps its existing signature and behavior; it does not need the HitResult.
- Do not add a new Gameplay Tag, Input route, ASC, damage route, projectile flag, or global Parry de-duplication state. Existing `UAbilityTask_MeleeTraceWindow::DeliveredTargets` and `ACombatProjectile::bHitDelivered` remain the one-contact guards.

### 2. Parry API and success feedback

Change the Parry method to:

```cpp
bool TryParryMeleeHit(AActor* AttackingActor, const FHitResult& HitResult);
```

Keep the existing window, front `120`-degree arc, ability/GE validation, counter-Poise attempt, and `true`/`false` meaning. A missing attacker ASC still skips only the counter and still returns success after a valid Parry; a malformed local Parry configuration still returns `false` and emits no feedback.

Add these optional, Parry-owned authoring fields to `UPlayerParryAbility`:

```cpp
UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Player|Parry|Feedback", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", Units = "Seconds"))
float ParrySuccessHitStopDurationSeconds = 0.05f;

UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Player|Parry|Feedback", meta = (AllowPrivateAccess = "true", ClampMin = "0.01", ClampMax = "1.0"))
   float ParrySuccessHitStopTimeDilation = 0.03f;

UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Player|Parry|Feedback", meta = (AllowPrivateAccess = "true"))
TObjectPtr<USoundBase> ParrySuccessSound;
```

The approved defaults follow the current C3K Big impact tuning (`0.05s`, `0.03`), while the fields remain independently tunable on Parry. Invalid/non-finite duration or dilation is fail-closed for the hit-stop channel only; it must not cancel a valid Parry or counter.

Add one private helper:

```cpp
void TriggerParrySuccessFeedback(const FHitResult& HitResult);
```

Call it exactly once, after the existing successful Parry gate and counter attempt, immediately before returning `true`. The helper must execute channels in this order, with independent validity checks:

1. Request `APolyQuestPlayerController::RequestCombatImpactHitStop(...)` through the current Player's existing Controller route. If World/Controller is unavailable, or values are invalid, skip only hit-stop.
2. Call the Player's narrow `TriggerParrySuccessCameraShake()` wrapper, which internally reuses `BigHitFeedbackCameraShakeClass` and the existing local camera-manager lifecycle. Missing/non-local camera is a no-op.
3. If `ParrySuccessSound` is configured, play it with `UGameplayStatics::PlaySoundAtLocation`. Use `HitResult.ImpactPoint` only when `HitResult.GetActor() == PlayerCharacter`, all three coordinates are finite, and the point is not the default near-zero value; otherwise use the Player `ActorLocation` if finite. A missing sound, invalid point, missing World, or audio failure is a silent presentation no-op.

The `FHitResult` is copied/consumed synchronously; do not retain a pointer, `FGameplayEffectContextHandle`, or callback state across frames. Sound failure must never block contact consumption, counter-Poise, cooldown, or `EndAbility` cleanup.

### 3. Player camera wrapper

Add this narrow C++-only method to `APlayerCharacter`:

```cpp
void TriggerParrySuccessCameraShake();
```

Its implementation must only forward to the existing private camera-shake path with `EHitReactionTier::Big`. Do not make `TriggerHitFeedbackCameraShake` public, do not create a second shake manager, and do not change the existing Health hit-reaction tier mapping.

### 4. No GameplayCue in v1

Do not add a GameplayCue class, cue tag, cue asset, or generic feedback dispatcher. PolyQuest is single-player and currently has one Player Parry consumer; direct native Ability -> Player/Controller calls are the smallest authoritative path. Reconsider a GameplayCue only when at least one of these is real and accepted: replicated/network-visible Parry feedback, multiple actors/consumers sharing the same event, or a materially larger asset-driven feedback fan-out.

## Approved Native And Test Slice

Only the following paths are approved. For every shared-contract path, **Contract owner: Main; implementation writer: Gemini**.

1. `Source/PolyQuest/Public/AbilitySystem/Abilities/PlayerParryAbility.h`
   - Add the `USoundBase` forward declaration, the three feedback UPROPERTY fields, the new `TryParryMeleeHit` signature, and the private helper declaration.
   - Add only minimal `#if WITH_DEV_AUTOMATION_TESTS` setters/getters needed to construct a transient test instance: `SetTestCurrentActorInfo`, `SetTestCurrentSpecHandle`, `TestSetParryWindowOpen`, `SetTestParryCounterPoiseGameplayEffectClass`, `SetTestParrySuccessSound`, optional tunable setters, `GetTestParrySuccessFeedbackCount`, `GetTestParrySuccessSoundDispatchCount`, and `GetTestLastParrySuccessSoundLocation` (exact names may follow project style). Keep every seam test-only and non-reflected.
   - Do not change ability tags, activation blocks, cancellation ownership, public Blueprint behavior, or Montage/task members.

2. `Source/PolyQuest/Private/AbilitySystem/Abilities/PlayerParryAbility.cpp`
   - Allowed runtime functions: `TryParryMeleeHit` and the new `TriggerParrySuccessFeedback`; add only their required includes.
   - Preserve `ActivateAbility`, `EndAbility`, window event identity, arc math, counter GE construction, and cooldown semantics except for compile-safe signature plumbing.
   - Counter application remains optional: an absent attacker ASC or an unsuccessful counter spec does not erase a valid Parry success. The helper must run once for that accepted contact.

3. `Source/PolyQuest/Public/Character/Player/PlayerCharacter.h`
   - Add the narrow `TriggerParrySuccessCameraShake()` declaration near the existing combat feedback API.
   - Keep the existing `TryResolveIncomingDefense` C++ API non-Blueprint and update its exact signature with `HitResult` and `bAllowParry`.
   - Any test-only feedback readback remains inside `WITH_DEV_AUTOMATION_TESTS`; do not add a general feedback callback or public sound API.

4. `Source/PolyQuest/Private/Character/Player/PlayerCharacter.cpp`
   - Update only `TryResolveIncomingDefense` to honor `bAllowParry` and forward `HitResult`; keep Guard fallback behavior exact.
   - Implement only the narrow camera wrapper by forwarding to `TriggerHitFeedbackCameraShake(EHitReactionTier::Big)`.
   - Do not alter camera follow, lock-on, movement, input, or Health reaction code.

5. `Source/PolyQuest/Private/Combat/Melee/MeleeHitResolver.cpp`
   - Pass `Request.HitResult` and `true` to the new Player defense entry.
   - Do not alter team filtering, dead/invulnerable checks, Context construction, damage modifiers, or trace-contact return semantics.

6. `Source/PolyQuest/Private/Combat/Projectile/CombatProjectileHitResolver.cpp`
   - Pass `Request.HitResult` and `false` to the new Player defense entry.
   - Preserve Guard consumption, ordinary projectile Health damage, instant-GE success handling, `bHitDelivered`, and all targeting/lifecycle behavior.

7. `Source/PolyQuest/Private/Tests/TestParryCounterPoiseGE.h`
   - Add a native-only transient test `UGameplayEffect` declaration. It must be an instant Poise additive modifier driven by the existing `Data.Poise.Parry` SetByCaller tag; no Content asset or Config entry.

8. `Source/PolyQuest/Private/Tests/TestParryCounterPoiseGE.cpp`
   - Implement only the test GE constructor and its SetByCaller Poise modifier using the same style as existing native test GEs.

9. `Source/PolyQuest/Private/Tests/ParrySuccessImpactFeedbackAutomationTests.cpp`
   - Add the `PolyQuest.Combat.ParrySuccessFeedback` suite. It may include existing fixture/test helpers but must not modify them.
   - All test-only counters, transient asset setup, and seams must remain under the existing automation-test build boundary; no production-only instrumentation is allowed.

No other source/test path is approved. If a required change falls outside this list, stop and return the evidence to Main.

### Closeout-only tuning freeze

The user explicitly approved including these two existing C3K tuning changes in the same freeze commit; they are not Gemini's C3M implementation surface:

10. `Source/PolyQuest/Public/Character/Enemy/EnemyCharacter.h`
   - Freeze Small `0.03s / 0.1`, Big `0.05s / 0.03`, and Launch `0.05s / 0.05` defaults and the corresponding `0.001` dilation lower bound.

11. `Source/PolyQuest/Private/Tests/CombatHitFeedbackAutomationTests.cpp`
   - Keep the C3K preset assertions synchronized with those approved defaults.

## Automation Acceptance Matrix

The new suite must cover the following observable contracts without Content assets:

1. **Successful melee Parry**
   - Build a transient Player/attacker fixture and a valid `FHitResult` whose actor is the Player and whose ImpactPoint is finite and non-zero.
   - Exercise the real `FMeleeHitResolver` path where practical. Assert Player Health and Guard Stamina do not decrease, the attacker Poise loses the configured counter amount through `UTestParryCounterPoiseGE`, and Parry feedback counts increase exactly once.
   - Assert the existing Controller is active at the approved `0.05s` / `0.03` and exposes the expected expiry/restoration through its existing test readback; assert one Parry feedback invocation, one Big camera-shake start using the already configured test Big shake, and one sound dispatch at the exact non-zero ImpactPoint. Do not add a Controller request counter or modify Controller production/test files.

2. **Sound location and independent fallback**
   - A finite, non-zero Player-owned ImpactPoint uses that point.
   - A missing, mismatched, or non-finite HitResult point falls back to finite Player ActorLocation without retaining a Context pointer.
   - A null sound asset does not prevent hit-stop, Big shake, Parry success, counter-Poise, or cleanup.
   - Missing World/Controller/CameraManager/Big shake or invalid hit-stop values fail closed per channel and do not crash or change Parry return semantics.

3. **Existing Parry gates**
   - Closed window, wrong front arc, null attacker, invalid local counter configuration, and dead/invalid setup produce no success feedback.
   - An attacker Actor without an ASC still yields a valid Parry success and feedback, while only the counter-Poise application is skipped.

4. **Projectile separation and Guard regression**
   - With a valid active Parry window, a projectile resolver request does not call Parry, does not emit Parry feedback, and still applies ordinary projectile damage when Guard does not consume it.
   - A projectile with an active Guard continues through the existing Guard consumption path. Do not weaken or duplicate the Guard contract.

5. **Exactly-once contact behavior**
   - The C3M suite proves one resolver-delivered Parry contact produces one feedback call; the existing `UAbilityTask_MeleeTraceWindow` / `DeliveredTargets` suite proves repeated samples in one window produce one damage delivery. A combined Parry-specific Trace Window assertion remains a low-priority coverage recommendation, not a new runtime owner.
   - Preserve the existing task/projectile de-duplication owners; do not make the new Parry Ability globally suppress future contacts.

6. **Hit-stop lifecycle**
   - Advance the test World using the existing real-time/controller tick helper and assert the global dilation restores after expiry.
   - Exercise Controller teardown/World teardown while feedback is active and assert no stale dilation or crash. This reuses, rather than reimplements, C3K Controller ownership. The test must follow the existing `CombatHitFeedbackAutomationTests.cpp` `FWorldCleanup` RAII pattern (or an equivalent scope guard) to restore global dilation to `1.0f` on every early return.

If the current fixture cannot host an active transient Parry instance without a production seam, first use a `FGameplayAbilitySpec` with a transient `UPlayerParryAbility` primary instance plus its existing test-only ActorInfo/state setters. Only if that exact engine API is unavailable may Gemini propose a minimal Player test seam; it must remain in the approved Player header/cpp and stop for Main approval before adding it.

## Execution Order

1. Gemini reads the approved files, current C3K feedback implementation, direct resolver callers, and existing automation fixture patterns. It verifies the baseline and does not touch the Editor.
2. Add the frozen API plumbing (`HitResult`, `bAllowParry`) and update exactly the two resolver call sites. Re-read the melee/projectile branch matrix to confirm Parry is melee-only and Guard remains shared.
3. Add Parry-owned tunables and the synchronous feedback helper. Reuse the existing Controller and camera paths; keep each feedback channel fail-closed and independent.
4. Add the transient counter-Poise GE and the new focused Automation suite, including active-instance construction and resolver-level contact coverage without changing production fixtures.
5. Run Rider `get_file_problems`/`lint_files` on every touched C++ file, run `git diff --check`, and inspect the complete diff plus direct callers/callees. Resolve newly introduced diagnostics.
6. Return changed paths, static evidence, unrun user gates, strict self-review findings, and residual risks. Stop without compilation, Editor/PIE, asset saves, documentation edits, staging, or commit.
7. User compiles and runs the prescribed Editor Automation/PIE gates. Main interprets the evidence, performs the final fresh review, and only then closes documentation and stages after explicit approval.

## Validation Gates

### Gemini static gate (must not be mislabeled as runtime proof)

- Read final source and test diffs plus the direct `TryResolveIncomingDefense` callers/callees.
- Rider diagnostics on all nine approved paths that contain C++ (or the available subset, reported explicitly) show no new errors/warnings.
- `git diff --check` passes.
- No UBT/Visual Studio/Rider build, live Unreal MCP write, Editor Automation run, PIE, audio/visual claim, staging, or commit by Gemini.

### User compile and Editor readback

1. Compile `PolyQuestEditor (Development Editor)` in Visual Studio 2022. **User result: passed.**
2. If sound is desired and an authored Parry Gameplay Ability/Blueprint exists, assign a valid `USoundBase` to `ParrySuccessSound`; save the asset in Editor. Leaving it empty is an intentional silent fallback, not a source failure.
3. Confirm the Parry ability still owns its existing Montage/window/counter/cooldown fields and that no new Gameplay Tag, Input, or GameplayCue asset was introduced.
4. Keep authored assets local WIP; do not hand-edit or stage `.uasset`/`.umap` files as part of the source slice.

### Automation and PIE

Run at minimum:

- `PolyQuest.Combat.ParrySuccessFeedback`
- `PolyQuest.Combat.HitFeedback`
- `PolyQuest.Projectile.Lifecycle`
- `PolyQuest.Player.ActionWindows`
- the existing Guard/Parry and Equipment transaction suites relevant to the active defense grants.

**User result:** the requested Automation suites passed, including `PolyQuest.Combat.ParrySuccessFeedback` and the existing combat/reaction regressions.

In Scene01, verify:

- a valid melee Parry produces a perceptible but brief freeze, the existing Big camera shake, and the configured sound at the contact;
- Player Health/Guard Stamina remain unchanged and attacker Poise/counter behavior remains intact;
- a projectile during Parry is not Parried, while Guard and ordinary projectile damage still behave as before;
- repeated sweep samples do not stack feedback for one delivered contact;
- missing sound or unavailable camera/controller does not block or wedge the Parry Ability;
- normal Parry interruption, cooldown, and teardown remain clean.

**User result:** focused Scene01 PIE passed, including the Parry freeze, Big camera response, sound route, projectile separation, and cleanup behavior.

## Non-Goals And Stop Conditions

- No GameplayCue, Niagara/blood effect, overlay, Health reaction Montage, Enemy feedback, hit-reaction tier dispatch, knockback, or new damage/Poise route.
- No changes to `APolyQuestPlayerController` production implementation; the existing hit-stop API is the only Controller integration.
- No changes to Parry activation/cost/cooldown timing, Montage Notify ownership, front-arc math, Guard behavior, projectile movement, trace geometry, target assist, or de-duplication owners.
- No replication, client prediction, network authority expansion, generic feedback bus, or asset import/migration.
- Stop and report to Main if implementation needs a new public API beyond the frozen signatures, a new Tag/Input/Config/Build.cs entry, a production fixture seam not covered above, an asset edit, or a lifecycle rule change.
- Stop on any missing/invalid authored Parry asset only as an Editor/user authoring issue; do not infer a runtime substitute or silently broaden the native fallback.

## Documentation, Debt, And Commit Boundary

- Before closeout, Main compares all implementation findings and user-gate results against the existing C3M entry in `ROADMAP.md`. Any unresolved risk must name its boundary, evidence, impact, closure trigger, and owning stage; “deferred” alone is not enough.
- On accepted validation, Main updates `ARCHITECTURE.md` with the stable Parry-success feedback and melee-only Parry/projectile-defense contract, updates `README.md` only for the evidence-backed player-facing result, marks `TODO-02C3M` done in `ROADMAP.md`, and records the final closeout in this plan until the next stage replaces it.
- The source/docs freeze commit includes the nine C3M native/test paths, the two explicitly approved C3K tuning paths, and Main-owned documentation only. Exclude `AGENTS.md`, all `Content/**`/`Config/**` WIP, maps, Blueprints, Montage/AnimBP/GA/GE assets, generated output, and unrelated changes.

## Gemini Handoff Prompt

> 你是本阶段的唯一实现执行者。工作目录必须是 `E:\GameDevelop\PolyQuest`，基线为 `024a374494ecca9525ff8607b07c402e4247582b`；先读取当前 `plan.md`、项目 `AGENTS.md` 和本计划列出的真实源码，再开始改动。Outer=`ue-stage-workflow`，Primary=`ue5-cpp-gameplay`，Support=`game-feel`/`ue5-debug-validation`。Contract owner: Main；implementation writer: Gemini。
>
> 只允许修改以下九个路径：`Source/PolyQuest/Public/AbilitySystem/Abilities/PlayerParryAbility.h`、`Source/PolyQuest/Private/AbilitySystem/Abilities/PlayerParryAbility.cpp`、`Source/PolyQuest/Public/Character/Player/PlayerCharacter.h`、`Source/PolyQuest/Private/Character/Player/PlayerCharacter.cpp`、`Source/PolyQuest/Private/Combat/Melee/MeleeHitResolver.cpp`、`Source/PolyQuest/Private/Combat/Projectile/CombatProjectileHitResolver.cpp`、`Source/PolyQuest/Private/Tests/TestParryCounterPoiseGE.h`、`Source/PolyQuest/Private/Tests/TestParryCounterPoiseGE.cpp`、`Source/PolyQuest/Private/Tests/ParrySuccessImpactFeedbackAutomationTests.cpp`。不要改文档、资产、Config、Tags、Input、Build.cs、Controller、Enemy、fixture 或其他 WIP；不要 `git add`、不要 commit。
>
> 按计划冻结的契约实现：`TryResolveIncomingDefense(AActor*, float, const FHitResult&, bool bAllowParry)`；近战传 `true`，投射物传 `false`，Guard 路径保持不变。`TryParryMeleeHit(AActor*, const FHitResult&)` 只在有效近战 Parry 成功后触发一次 Parry-owned feedback：调用现有 Controller hit-stop（批准默认 `0.05s/0.03`）、Player 现有 Big camera shake、可选 `ParrySuccessSound`；各通道独立 fail-closed，Sound 仅在 HitResult actor 匹配 Player、ImpactPoint 有限且非默认近零时使用该点，否则回退 Player ActorLocation，不保存 Context/HitResult 指针。缺少 attacker ASC 只跳过 counter，不能否定 Parry 或反馈。不要 GameplayCue，不要新增去重状态。
>
> 用 `UTestParryCounterPoiseGE` 验证现有 `Data.Poise.Parry` counter；新增 `PolyQuest.Combat.ParrySuccessFeedback` 原生 Automation，覆盖成功近战、Sound/位置 fallback、缺失资产、错误方向/窗口、无 attacker ASC、投射物禁止 Parry 但保留 Guard/伤害、Trace Window 重复 sweep 单次反馈、Hit-stop 恢复与 teardown。测试 World 必须沿用现有 `FWorldCleanup` RAII（或等价 scope guard），任何 early return 都要把全局时间膨胀恢复为 `1.0f`。优先使用 transient `FGameplayAbilitySpec` primary instance 和现有 test-only setters；需要新增 seam 时必须留在批准的 Player/Parry 文件、`WITH_DEV_AUTOMATION_TESTS` 内，并在超出冻结边界前停下报告。
>
> 完成后只运行静态检查：Rider `get_file_problems`/`lint_files`、`git diff --check`，并阅读完整 diff/直接调用链。返回：changed paths、静态检查证据、未运行的编译/Editor Automation/PIE 门禁、严格自审 findings、remaining risks。不要声称编译、PIE、音频或视觉验证成功；遇到未列出的文件、公共契约、资产或生命周期需求立即停止并把证据交还 Main。

## Main Fresh Review

- Findings: no P0/P1/P2 source defect. The previously noted numeric differences are approved user tuning and are treated as a documentation synchronization item.
- The only remaining observation is P3 test granularity: the C3M suite does not itself drive repeated `UAbilityTask_MeleeTraceWindow` samples; existing `MeleeMultiTraceSource` coverage proves the shared `DeliveredTargets` owner and C3M coverage proves the resolver feedback call. No additional runtime owner or code repair is required for this closeout.
- Static evidence: direct source/diff review completed; `git diff --check` passed. The Code Review Graph was used only as supplemental context because the working-tree delta is uncommitted. Rider diagnostics from the executor reported no new errors.

## Closeout Record

- Implementation: Gemini completed the nine approved C3M native/test paths.
- User compile/Automation/PIE: user confirmed Development Editor compilation, the focused Automation/regression matrix, and Scene01 PIE passed.
- Additional freeze: user approved `EnemyCharacter.h` and `CombatHitFeedbackAutomationTests.cpp` as the C3K presentation-tuning freeze.
- Documentation: Main synchronized `ROADMAP.md`, `ARCHITECTURE.md`, and `README.md`; the next accepted stage is `TODO-02C3N`.
- Commit boundary: this closeout commit contains the nine C3M native/test paths, the two user-approved C3K tuning paths, and Main-owned `plan.md`/`ROADMAP.md`/`ARCHITECTURE.md`/`README.md`; unrelated WIP remains excluded.
