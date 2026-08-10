# plan.md — AI Agent Collaboration File

## Rules

1. Read this file before continuing a PolyQuest stage.
2. Keep feedback, active plan, validation evidence, and unresolved blockers scoped to the current stage.
3. The implementation author cannot mark a stage as review-approved.
4. After a stage is complete and review-approved, clear transient detail below this header rather than turning this file into project history.
5. Durable architecture belongs in `ARCHITECTURE.md`; future direction belongs in `ROADMAP.md`.

---

## Stage Handoff

No active stage. `TODO-01A: Player Ability Lifecycle And One-Hit Attack v1` has completed local validation and review.

- The user confirmed `PolyQuestEditor` compilation and PIE behavior for the complete LMB-to-target-ASC path, including normal recovery, reactivation blocking during the active Montage, one cost application, one target damage application, no-target safety, and preserved movement/look/jump.
- Main review repaired the premature completion path caused by treating Montage blend-out as full completion. Natural end now uses `OnCompleted`; interrupted, cancelled, invalid-configuration, and `ABaseCharacter::EndPlay()` paths still converge through `EndAbility()`.
- A fresh adversarial review found no P0/P1 issue. The retained conditional P2 is future direct `UAbilityTask_PlayMontageAndWait::ExternalCancel()` use: before any future caller relies on it, define Montage-stop behavior and route the cleanup through the ability contract.
- This commit stages native source, tags, stage documents, and only stable imported sequence/model packages. Mutable GA/GE/Montage/AnimBP/player/input authoring assets remain uncommitted local WIP, so the recorded PIE fixture is not reproducible from this commit alone.
