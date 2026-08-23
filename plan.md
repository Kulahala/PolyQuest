# TODO-07A1: Core Vital HUD And Enemy Bars v1

## Plan State

- Status: Completed; native UI source and focused Automation are implemented, and the user has completed the required compile, Editor readback, and PIE gates.
- Baseline: `84b6bd8` (`[Feature] 完成敌人落地延迟破韧 / Landing-Deferred Enemy Stance Break`).
- Objective: add a small passive vital UI for the existing single-player GAS runtime: one Player viewport HUD for Health and Stamina, plus one always-visible overhead Health bar for every living Enemy.
- Current `Content/**`, maps, imported assets, authored Blueprint/UMG/GA/GE/Montage/AnimBP/input assets, `Config/Automation/**`, `Config/Tests/**`, and all unrelated worktree changes remain user-owned WIP. Preserve and exclude them unless the user explicitly approves a stable asset closure. The scoped `AGENTS.md` documentation-ownership clarification belongs to this closeout.

~~~text
Outer: ue-stage-workflow
Primary: ue5-ui-umg-slate
Support: ue5-cpp-gameplay, ue5-debug-validation
Route reason: this is a UMG presentation and ASC Attribute-delegate lifecycle bridge; it must remain display-only and preserve the existing input/UI ownership.
~~~

~~~text
Plan explorers: 0
Implementation executors: 1 (Gemini, user-coordinated bounded executor)
Complex Executor: none
Main parallel work: contract review, validation interpretation, fresh/adversarial review, documentation closeout, and commit preparation after user approval.
Reason: Controller and Enemy UI lifecycles are a small coupled boundary. The active plan fully fixes their contract, while Main retains architecture, shared GAS ownership, review, documentation, and Git ownership.
~~~

## Locked Product Contract

1. `TODO-07A1` is UMG-only presentation. `UCharacterAttributeSet` and each character ASC remain the sole Health, MaxHealth, Stamina, and MaxStamina authority. Widgets never apply GameplayEffects, mutate Attributes, cancel abilities, or own combat state.
2. The Player HUD sits at the upper-left viewport edge and shows immediate `Current / Max` Health and Stamina values with matching progress bars. Values are integer-presented; a value change does not use a delayed damage bar, Tween, Widget Tick, or polling loop.
3. Every living Enemy has an overhead Screen Space `UWidgetComponent` Health bar. It contains no current/max/percentage text, Poise, target marker, fade, animation, or damage number. It hides on death and teardown.
4. Enemy bars remain constantly visible in v1. The accepted later product direction is idle-hidden bars that appear on damage or Lock-On; no visibility timer, fade policy, target API, or Lock-On integration belongs in this stage.
5. HUD creation and removal must not change the current Controller mouse cursor flags, `FInputModeGameAndUI`, focus, click/hover handling, Enhanced Input mappings, camera behavior, or gameplay input routing.
6. A missing configured Widget class, wrong native Widget parent, missing Pawn/ASC, invalid number, destruction, or re-possession must fail closed without a crash, delegate leak, duplicate viewport HUD, or impact on character gameplay.
7. This stage does not reuse the migrated Test Widgets or Test HUD C++ hierarchy. It does not add CommonUI, a ViewModel framework, an `EActionState`-like UI state, a generic `UHealthBarComponent`, menus, potions, currency, crosshair, arrow count, buffs, Poise UI, damage text, or lock-on.

## Approved Native Surface

### UMG presentation types

- Add `Source/PolyQuest/Public|Private/UI/PlayerVitalHUDWidget.*`.
  - `UPlayerVitalHUDWidget : UUserWidget` exposes C++-only `SetHealth(float Current, float Max)` and `SetStamina(float Current, float Max)`.
  - Bind exact Widget Tree names through `BindWidget`: `HealthProgressBar`, `HealthCurrentText`, `HealthMaxText`, `StaminaProgressBar`, `StaminaCurrentText`, `StaminaMaxText`.
  - Each setter checks every `BindWidget` pointer before use, so `NewObject`/Headless Automation with no Blueprint Widget Tree cannot crash. It validates `FMath::IsFinite(Current)` and `FMath::IsFinite(Max)` before clamping or rounding; invalid values or `Max <= 0` produce `Current = 0`, `Max = 0`, and `Percent = 0`. Valid values clamp display Current to `[0, Max]`, clamp Percent to `[0, 1]`, then format rounded integer text.
  - It has no ASC delegate, Actor reference, timer, Tick override, Blueprint gameplay callback, or input logic.
- Add `Source/PolyQuest/Public|Private/UI/EnemyHealthBarWidget.*`.
  - `UEnemyHealthBarWidget : UUserWidget` exposes C++-only `SetHealth(float Current, float Max)`.
  - Bind only `HealthProgressBar`; its Blueprint intentionally has no numeric TextBlock contract.
  - It checks the optional `HealthProgressBar` before use and follows the same finite-value and percent-safety rules, with no gameplay binding, mutation, timer, Tick, or target state.
- Do not modify `PolyQuest.Build.cs`: the current module already has `UMG` and `Slate` dependencies.

### Player Controller ownership

- Update `Source/PolyQuest/Public|Private/Framework/PolyQuestPlayerController.*`.
  - Add `UPROPERTY(EditDefaultsOnly, Category = "UI") TSubclassOf<UPlayerVitalHUDWidget> PlayerVitalHUDClass` and one `UPROPERTY(Transient)` HUD instance.
  - Add explicit idempotent private helpers: `EnsureHUDCreated()`, `BindToPawn(APawn* InPawn)`, `UnbindCurrentPawn()`, and `RefreshVitalHUD()`, plus four `FDelegateHandle` values and weak references to the currently bound Player Pawn and ASC.
  - `EnsureHUDCreated()` creates and adds the Widget exactly once only for a local Controller with a valid configured class. `BindToPawn()` always calls `UnbindCurrentPawn()` before subscribing to a valid `APlayerCharacter` ASC, then immediately calls `RefreshVitalHUD()`.
  - After `Super::OnPossess(InPawn)`, call `EnsureHUDCreated()` and `BindToPawn(InPawn)`. `BeginPlay()` calls the same idempotent pair for `GetPawn()` so either engine ordering produces one HUD and one binding. Repeated paths may safely unbind/rebind but must never create a second viewport Widget.
  - Subscribe independently to existing `Health`, `MaxHealth`, `Stamina`, and `MaxStamina` Attribute delegates. All four callbacks only invoke `RefreshVitalHUD()`, which reads the four current values from the same bound ASC and calls both Widget setters. This is also the sole initial-snapshot route, so a Max change cannot leave stale ratio or text.
  - `OnUnPossess()` calls `UnbindCurrentPawn()` and leaves the single HUD instance available for the next valid possession. `EndPlay()` unbinds, removes the instance from its parent, and clears references.
  - Do not move, remove, or weaken `APlayerCharacter::BindHealthEvents()` and its hit-reaction route. Controller HUD binding is a separate display subscriber.
  - Missing configuration or a wrong Widget parent logs at most once per Controller ownership cycle and does not create a fallback HUD.

### Enemy-owned overhead display

- Update `Source/PolyQuest/Public|Private/Character/Enemy/EnemyCharacter.*`.
  - Construct one `UWidgetComponent` named `EnemyHealthBarWidgetComponent`, attached to the Character Root. Set `EWidgetSpace::Screen`, collision disabled, Pivot `(0.5, 1.0)`, initial relative Z offset `130cm`, and DrawSize `160x20`; Enemy Blueprints own final position/size tuning.
  - Add a private weak typed Widget reference, independently named Health/MaxHealth delegate handles, and bind/unbind/refresh/hide helpers. These are UI-only and must not reuse or replace the existing death, HitReaction, Poise, or Stance Break handles.
  - During `BeginPlay()`, resolve and cache the typed Widget only when a Widget Class is configured. A configured but wrong Widget parent logs once; an unconfigured component or Headless Automation where `GetUserWidgetObject()` is null silently leaves the weak reference empty. Bind Health and MaxHealth delegates to the existing ASC and immediately request a snapshot in either case.
  - `RefreshEnemyHealthBar()` reads the current ASC values but only calls `SetHealth` when `EnemyHealthBarWidget.IsValid()`. It must never dereference a missing Slate/UserWidget object or retry-log every Attribute change. A MaxHealth callback refreshes from both current values.
  - The existing `HandleDeath()` path hides the component with `SetVisibility(false, true)` before the corpse/ragdoll presentation continues. `EndPlay()` unbinds first, calls the same recursive hide, clears the Widget weak reference, and then follows the existing teardown. No callback may write after destruction.

### Focused Automation

- Add `Source/PolyQuest/Private/Tests/VitalHudAutomationTests.cpp` with `PolyQuest.UI.VitalHUD` using the existing transient Game World fixture pattern. Do not load or mutate user `Content` assets.
- Cover the native behavior that is meaningful without a Widget Blueprint asset:
  - Player and Enemy Widget setters accept missing `BindWidget` pointers and produce finite, clamped percentages for normal, zero/negative Max, NaN, and Infinity inputs without crashing.
  - The Enemy CDO owns exactly one Screen Space, collision-disabled WidgetComponent with the agreed defaults.
  - A transient Enemy can bind once, refresh initial Health/MaxHealth, tolerate a Headless null `GetUserWidgetObject()`, unbind cleanly, and ignore a later old-ASC change.
  - A transient local Controller rebind/re-possess path reuses one HUD instance rather than creating a second one and removes its delegate ownership on teardown.
- Add only narrowly scoped `WITH_DEV_AUTOMATION_TESTS` hooks if the existing public surface cannot observe a lifecycle state. Do not expose a production Blueprint API, load a widget asset, or alter GAS Attribute semantics merely to make the test easier.

## User-Owned Editor Work

After the native parent classes compile, author fresh UI assets instead of touching the failed Test migration:

1. Create `/Game/UI/HUD/Vitals/WBP_PlayerVitalHUD` derived from `UPlayerVitalHUDWidget`.
   - Anchor top-left with an authored safe margin.
   - Use `POLYGON_HUD` resources `SPR_DarkFantasy_Frame_Bar_01_Background`, `SPR_DarkFantasy_Frame_Bar_01`, and `ICON_DarkFantasy_Stat_Health_01_Clean` as the visual baseline.
   - Use red for Health and cyan-green for Stamina. V1 intentionally has no guessed Stamina icon.
   - Give the Widget Tree the six exact native `BindWidget` names above.
2. Create `/Game/UI/HUD/Vitals/WBP_EnemyVitalBar` derived from `UEnemyHealthBarWidget`.
   - Create exactly one named `HealthProgressBar` plus ordinary visual background/frame layers.
   - Do not add numeric TextBlocks, Poise, target visuals, bindings, Tick logic, or animation.
3. In the active `BP_PlayerController`, set `PlayerVitalHUDClass = WBP_PlayerVitalHUD`.
4. Confirm in Editor that the Default GameMode `BP_GameMode` uses `BP_PlayerController`. Source/config confirms `BP_GameMode` is the project default and Rider confirms `BP_PlayerController` derives from `APolyQuestPlayerController`, but Rider could not read the GameMode CDO property value offline.
5. In every active Enemy Blueprint derived from `AEnemyCharacter`, set the component Widget Class to `WBP_EnemyVitalBar` and tune the vertical offset for that mesh. Do not alter imported Marketplace assets.
6. The user has manually deleted the failed migrated assets `/Game/UI/HUD/WBP_PlayerHUD`, `/Game/UI/HUD/WBP_EnemyHealthBar`, and `/Game/UI/Textures/T_PotionIcon` through Unreal Editor. Do not recreate or reuse them. During the later Editor readback, confirm that no redirector or remaining reference points to those retired paths; record only the actual Reference Viewer evidence in closeout.

## Validation, Review, And Closeout

### Static and Automation gate

1. Gemini supplies only the approved C++ and Automation diff, changed-path list, and a two-pass implementation self-review. It must not edit `plan.md`, docs, `Content/**`, maps, Blueprint/UMG/GA/GE/Montage/AnimBP/input assets, config, Editor state, staging, or commits.
2. Before user compilation, Main verifies the final Controller and Enemy callback/teardown call chains through CodeGraph, runs Rider `lint_files` or `get_file_problems` on touched C++, runs `PolyQuest.UI.VitalHUD`, and runs `git diff --check`.
3. Static, Automation, and Rider results are not PIE or visual evidence.
4. The transient automation world must not emit `No game viewport was found`: test-only non-local Controller creation may create a Widget for lifecycle assertions, but must not call `AddToViewport()` without a real viewport. Treat that log as an Automation defect until removed.

### User compile, Editor, and PIE gate

1. Manually compile `PolyQuestEditor`.
2. Read back the two Widget parents, all required Widget Tree names, `PlayerVitalHUDClass`, active GameMode Controller class, Enemy Widget Class, `Screen` Widget Space, and per-Enemy placement.
3. In PIE, verify:
   - Initial Health/Stamina values are correct and immediately update after Player damage, Stamina spend, and regeneration. The project currently has no playable Player healing source; Automation must directly exercise the same ASC positive-Health refresh branch without claiming a PIE healing route.
   - Player respawn/restart/re-possession never duplicates the upper-left HUD.
   - Multiple living enemies each show only a Health bar, immediately update after damage and remain independent; their native Health/MaxHealth refresh behavior is covered by the focused Automation fixture.
   - Enemy death and actor destruction hide/remove the overhead bar without warnings, stale update, or gameplay regressions.
   - Cursor visibility, click/hover behavior, Game-and-UI input mode, movement, combat, and camera behavior are unchanged.

### Review, documentation, and commit boundary

1. After user-confirmed Automation, compile, Editor readback, and PIE, Main performs a normal defect-first fresh review. Attempt the required independent `gpt-5.6-luna / xhigh` adversarial review; if that model/runtime is unavailable, record a clearly labelled Main adversarial fallback instead of claiming an independent review.
2. On approved closeout, update `README.md` with the verified visible result and `ARCHITECTURE.md` with the stable ASC-to-Controller/Enemy UI ownership and teardown contract.
3. Update `ROADMAP.md`: mark `TODO-07A1` done and add the accepted future enemy-bar visibility decision under `TODO-07A2`. Its owner must define idle hiding, damage-driven showing, Lock-On showing, duration/fade behavior, and interaction with `TODO-02B1`; 07A1 deliberately implements none of those rules.
4. Stage only approved source, Automation, and documentation paths by explicit path. Keep new UMG/Blueprint assets and all other `Content/**` WIP out by default. If the user later approves a stable UI asset closure, verify at least one staged `.uasset` is a Git LFS pointer before committing.
5. No commit occurs until the user explicitly approves the reviewed closeout.

## Closeout Record

- Implemented surface: added passive `UPlayerVitalHUDWidget` and `UEnemyHealthBarWidget` native bases; `APolyQuestPlayerController` creates one local viewport HUD and owns idempotent Player ASC binding for Health, MaxHealth, Stamina, and MaxStamina; `AEnemyCharacter` owns one Screen Space Health-bar component with independent Health/MaxHealth delegate lifecycle, death hiding, and teardown cleanup. The implementation does not alter Attribute authority, combat state, input ownership, camera, or existing Player Health-reaction binding.
- User evidence: the user confirmed `PolyQuest.UI.VitalHUD` Automation success, manual `PolyQuestEditor` compilation without errors, Editor readback of Widget/Controller/Enemy configuration, and focused PIE for initial display, Player damage and Stamina spend/recovery, independent multi-Enemy bars, Enemy death hiding, restart/re-possession, and unchanged cursor/input behavior. The latest Automation log no longer includes `No game viewport was found`.
- Validation boundary: there is currently no playable Player healing source. Automation directly drives a positive Health change through the same ASC-to-HUD callback path; this is evidence for UI refresh only, not a claimed PIE healing route. `TODO-03E` remains the owner of the first gameplay healing loop.
- Static evidence: final Controller/Enemy/Widget call paths were re-read through CodeGraph; targeted Rider error-level inspection reported no errors before user compilation; `git diff --check` reported no content errors. code-review-graph matched the baseline but did not fully map the untracked new UI sources/test, so its low-risk label and test-gap counters are supplemental only; direct source/Rider/Automation review remains the acceptance evidence. The native Automation fixture covers finite-value handling, missing `BindWidget` pointers, Screen Space component defaults, headless Widget absence, old-ASC unbinding, and HUD rebind/reuse.
- Review: Main normal defect-first review and Main adversarial fallback found no P0-P2 in the approved UI surface. `gpt-5.6-luna / xhigh` remains unavailable, so this is not represented as an independent reviewer result.
- Debt handoff: `TODO-07A2` owns any idle-hidden/damage-triggered/lock-triggered visibility, timeout, fade, and lock-clear policy after `TODO-02B1`; `TODO-03E` owns actual Player healing gameplay. No new UI framework, Poise display, damage number, or target system is introduced here.
- Asset record: the user authored and validated the fresh HUD Widgets in Editor, manually removed the failed migrated Test Widgets, and kept all `Content/**` assets out of this source/test/document closure.
- Commit boundary: include `Source/PolyQuest/Private/Character/Enemy/EnemyCharacter.cpp`, `Source/PolyQuest/Public/Character/Enemy/EnemyCharacter.h`, `Source/PolyQuest/Private/Framework/PolyQuestPlayerController.cpp`, `Source/PolyQuest/Public/Framework/PolyQuestPlayerController.h`, `Source/PolyQuest/Private/UI/PlayerVitalHUDWidget.cpp`, `Source/PolyQuest/Public/UI/PlayerVitalHUDWidget.h`, `Source/PolyQuest/Private/UI/EnemyHealthBarWidget.cpp`, `Source/PolyQuest/Public/UI/EnemyHealthBarWidget.h`, `Source/PolyQuest/Private/Tests/VitalHudAutomationTests.cpp`, `AGENTS.md`, `README.md`, `ARCHITECTURE.md`, `ROADMAP.md`, and `plan.md`. Exclude all `Content/**`, maps, Config/project WIP, generated files, and every unrelated change.
