# plan.md — AI Agent Collaboration File

## Rules

1. Read this file before continuing a PolyQuest stage.
2. Keep feedback, active plan, validation evidence, and unresolved blockers scoped to the current stage.
3. The implementation author cannot mark a stage as review-approved.
4. After a stage is complete and review-approved, retain its final plan and closeout record below this header until the next accepted stage deliberately replaces it.
5. Durable architecture belongs in `ARCHITECTURE.md`; accepted future direction and validation debt belong in `ROADMAP.md`.

---

## Last Completed Stage: TODO-02B - Oblique Perspective Camera And Combat Facing v1

Baseline: `6569030141bd5ae86d4f9ffec2fa58e4eddba89d`.

### Objective

Replace the free over-the-shoulder orbit with a fixed-world oblique Perspective camera. Movement stays camera-relative. Light, Charged, Sprint Attack, and Dodge resolve one horizontal facing from current movement input at action startup, then retain it from ordinary movement while their GAS action tag is present.

### Route And Ownership

```text
Outer: ue-stage-workflow
Primary: ue5-cpp-gameplay
Support: camera-systems, unreal-enhanced-input, ue5-blueprint-workflow, unreal-mcp
Plan explorers: 0
Implementation executors: 0
Complex Executor: none
Main parallel work: none
Reason: Camera, input, public player API, and GAS action-tag lifecycle are one Main-owned integration boundary. The user owns Editor authoring, manual PolyQuestEditor compilation, and Scene01 PIE/visual validation.
```

### Approved Runtime Scope

- `CameraBoom` uses fixed absolute world rotation; the accepted native defaults are Pitch `-55`, Yaw `-45`, arm length `2000cm`, Perspective FOV `60`, and position-only SpringArm lag (`18`, maximum `75cm`, `1/60s` substepping). Rotation Lag remains disabled and SpringArm retains camera collision ownership.
- Movement and action-facing use the CameraBoom's actual world yaw rather than `Controller->ControlRotation`. No-input actions retain the actor's horizontal forward direction.
- Native input leaves `MouseLookAction` and `LookAction` unbound; their authored pointers remain intact, and `DoLook()` is deliberately inert.
- `State.Action.Attacking` or `State.Action.Dodging` disables `CharacterMovement.bOrientRotationToMovement`. When both tags clear, ordinary locomotion-facing returns. No persistent facing state, per-frame Root Motion yaw override, lock-on, target switching, mouse ground projection, Motion Warping, orthographic camera, or generic camera framework was added.
- Light, Charged, Sprint Attack, and Dodge reuse `ApplyActionFacing()` at their existing valid startup point. Sprint Attack establishes its runtime action tags before facing and Montage playback; combo continuation does not recompute direction.

### Validation And Review Record

- Main read back the live `BP_Player` CDO through Unreal MCP: the CameraBoom has absolute rotation, `-55/-45`, `2000cm` arm length, no Pawn Control Rotation, `ECC_Camera`, position lag `18 / 75cm / 1/60s`, and no Rotation Lag. FollowCamera is Perspective at FOV `60`. This is Editor-readback evidence only.
- The user confirmed the focused Scene01 PIE and visual route, including accepted position-lag behavior for empty-space Root Motion camera motion. Main did not run UBT, Visual Studio compilation, PIE, or authored asset writes.
- Main completed final source/direct-caller reads, UE 5.8 Camera/SpringArm and GAS tag-order verification, CodeGraph, scoped code-review-graph structural analysis, error-memory lookup, and `git diff --check`. Main normal review and a Main adversarial fallback found no P0-P2 C++/GAS Tag blocker.
- The requested fresh `gpt-5.6-luna / xhigh` Reviewer returned HTTP `503`, so no independent-review result is claimed. `TODO-07B` remains the only accepted owner for a future occlusion or presentation retune if later visual evidence demands one.

### Commit Boundary

The focused future commit may contain the six native TODO-02B source files; exact `plan.md`, `ARCHITECTURE.md`, and `ROADMAP.md` hunks; plus the explicitly approved small project files `AGENTS.md`, `Config/DefaultEditor.ini`, `Config/DefaultGame.ini`, and `PolyQuest.uproject`. The `.uproject` change intentionally carries the registered UE 5.8 association and enabled plugin metadata. Exclude all `Content/**`, authored input/Blueprint/map/animation/Montage WIP, imported resources, and generated output. The next accepted stage replaces this record instead of clearing it during documentation closeout.
