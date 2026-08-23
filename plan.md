# TODO-03A6B: Shield Guard Locomotion, Pace, And Facing Alignment v1

## Plan State

- Status: Complete. The user confirmed the focused authored Guard route and PIE visual result after replacing the conflicting upper-body/full-body composition.
- Baseline: `c8a30b5` (`[Test] 清理自动化夹具信号噪音 (Automation Fixture Signal Hygiene)`).
- Objective: close the Shield Guard presentation route over the accepted A6A locomotion baseline without changing Guard gameplay authority. The original fixed-`300` and new-Guard-slot assumptions were rejected by the actual authored result: the existing `0.7` pace is retained, while Shield Guard uses a dedicated full-body locomotion branch.
- Preserve all current user-owned WIP. The existing untracked Guard GE, Montages, BlendSpace, and modified `ABP_Player_Dungeon` are authoring inputs only; do not overwrite, stage, or commit any `Content/**` asset without later explicit stable-closure approval.

```text
Outer: ue-stage-workflow
Primary: ue5-blueprint-workflow
Support: ue5-debug-validation
Route reason: the required behavior is an authored GE/Montage/AnimBP closure over already-proven native Guard, Movement, equipment-locomotion, and B2 facing contracts.
```

```text
Plan explorers: 0
Implementation executors: 0
Complex Executor: none
Main parallel work: none
Reason: Guard, movement, lock-facing, and authored animation layers share one integration contract; user owns Editor authoring and no native implementation slice is necessary.
```

## Accepted Runtime And Presentation Contract

1. Both `GA_PlayerShieldGuard` and `GA_Guard_Sowrd` retain `UPlayerGuardAbility`, held-input release/cancel, Guard Arc, Stamina cost/recovery, Guard Break, and exact active-GE handle cleanup. `GE_Guard_MoveSpeed` remains the shared pace source, as the existing Infinite, non-periodic `MoveSpeed` multiplier at `0.7`; no `Override = 300` was authored.
2. Generic Guard remains the C++/GAS category `State.Action.Guarding`. Shield Guard owns the active child `State.Action.Guarding.Shield`, while single-Sword Guard retains the generic parent. Hierarchical GameplayTag matching therefore preserves every existing generic Guard consumer without turning the child tag into an equipment-state signal.
3. `ABaseCharacter` still writes the resolved `MoveSpeed` Attribute to `CharacterMovement.MaxWalkSpeed`. B2 lock-facing remains the yaw owner: eligible non-Root-Motion Guard faces a valid lock at `800 deg/s`; unlocked Guard remains camera-relative movement-facing; Guard still cancels Sprint.
4. `ABP_Player_Dungeon` derives `IsShieldGuarding` from ASC matching of the child Shield Guard tag. `ResolvedLocomotionMode` remains the ordinary equipped-family selector and is deliberately not reused as the active Shield Guard condition.
5. Shield Guard uses the full-body `BS_Shield_Walk_Run`. While `IsShieldGuarding` is true, the existing `DefaultGroup.UpperBody` branch has zero visual weight; single-Sword Guard continues to use that upper-body overlay. This prevents a full-body Shield Block Move from being mixed beneath a second full-body Guard pose at `spine_01`.
6. `AM_Shield_Guard` remains present for `UPlayerGuardAbility`'s established Montage lifecycle, but its visual contribution is bypassed only during active Shield Guard. `ReactionOverlayGroup.ReactionOverlay` remains downstream. The Root Motion `Anim_SAS_V2_Block_Move_*` assets remain outside this CharacterMovement-driven route.

## Accepted User-Owned Asset Closure

1. `GA_PlayerShieldGuard` owns `Ability.Defense.Guard.Shield` and `State.Action.Guarding.Shield`; `GA_Guard_Sowrd` remains the generic Guard route.
2. `ABP_Player_Dungeon` uses the existing cached base locomotion pose for both the base input and `DefaultGroup.UpperBody` source. It gates only the UpperBody blend weight with `IsShieldGuarding`, then leaves the existing Default Slot and Reaction Overlay order intact.
3. The final visual result is a full-body Shield Guard locomotion pose under B2 lock-facing, without the prior waist twist from a full-body Block Move plus an upper-body Guard overlay. No new Guard BlendSpace, Slot Group, native API, Input action, or generic animation framework was introduced.

## Validation And Closeout

1. User-confirmed focused authored/PIE visual evidence: Shield Guard now uses the intended full-body locomotion without the earlier torso conflict, while the established locked-facing result remains correct. The user also reported the focused test route passing.
2. This stage changes no native C++ or Automation surface. Existing automation remains regression coverage for the unchanged Guard, equipment, and lock-facing contracts; it is not presented as proof of the authored AnimGraph topology.
3. Main's single defect-first fresh review found no P0-P2 runtime or lifecycle defect in the approved scope. The resolved issue was documentation/plan drift: the rejected `300` and `GuardOverlayGroup` assumptions are removed here. Source inspection confirms the generic Guard category uses hierarchical tag queries, while the current authored child tag remains compatible.
4. A user-authored shared Stride Warping experiment in `ABP_Player_Dungeon` has passed focused PIE validation after this stage's closeout. It remains local `Content/**` WIP and is not included here. `TODO-07B` owns its cross-weapon cadence calibration, action/reaction exclusion, and focused visual validation before any stable asset closure. Default commit scope remains documentation only; exclude all `Content/**`, Config, map, Blueprint, Input, AnimBP, GA/GE/Montage, project-file, generated, and unrelated user-WIP paths unless the user later authorizes that closure.
