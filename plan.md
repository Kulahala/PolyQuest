# plan.md - AI Agent Collaboration File

## Rules

1. Read this file before continuing a PolyQuest stage.
2. Keep feedback, active plan, validation evidence, and unresolved blockers scoped to the current stage.
3. The implementation author cannot mark a stage as review-approved.
4. Retain the completed plan and closeout record until the next accepted stage deliberately replaces it.
5. Durable architecture belongs in `ARCHITECTURE.md`; accepted future direction and validation debt belong in `ROADMAP.md`.

---

## Active Stage: TODO-02E - Bidirectional Melee Combat Health And Lean Review v1

Baseline: `1f91b42 [Feature] 战斗动画通知归属审计 (Combat Notify Ownership Audit)`.

### Objective

Before `TODO-03A` equipment work, audit the completed bidirectional melee combat loop, perform the evidence-led lean pass, and synchronize the lagging project documentation. This stage adds no combat feature, changes no GAS contract, and performs no architectural refactor.

### Audit Matrix

Evidence sources: the C3B/C3C/D1/D2 stage reviews, this stage's three read-only Explore reports (enemy HyperArmor lifecycle, lean inventory, documentation drift), and Main's final re-reads.

| Audit surface | Existing evidence | 02E action |
| --- | --- | --- |
| Player-to-enemy damage chain (Trace Window -> Resolver -> GE -> C3B/C3C) | C3B/C3C/D1 reviews | Final re-read; record as audited |
| Enemy-to-player defense dispatch (Trace -> Resolver -> Parry/Guard/Health) | D2 first review, all surfaces closed | Final re-read; record as audited |
| Team filtering / Invulnerable / Dead rejection matrix | D1/D2 reviews | Spot-check; record as audited |
| Cancellation/death/EndPlay teardown across both sides' abilities | D1-D4 stage reviews | Consolidate into one teardown-matrix record |
| StateTree-to-GAS intent boundary | 02A/C-series validation | Spot-check; record as audited |
| Camera/facing interaction | Unchanged since 02B | Covered by the PIE regression smoke |
| HyperArmor lifecycle | Explore deep-read: all five teardown paths converge, no tag residual risk, identity-filtered events | Two notes dispositioned below |
| ROADMAP validation-debt review | Explore confirmed all 5 Known Risks plus the Sprint Loop deferral remain open, none silently resolved | No action; ownership verified |

Findings policy: P0/P1 blockers are repaired surgically in this stage; non-blocking findings are attached to their owning `ROADMAP.md` milestone or Known Risks entry with evidence and a closure trigger; documentation drift is repaired here as stage work.

Dispositions already decided from the audit evidence:

- HyperArmor `OnHyperArmorEnd` does not check `bHyperArmorActive` before its unconditional `SetLooseGameplayTagCount(HyperArmor, 0)`; the unconditional clear is the correct safety-net semantics, so the asymmetry is accepted without a code change.
- The Explore-reported "EndPlay implicitly relies on GAS actor-destruction cancellation" is a false positive: `ABaseCharacter::EndPlay()` already calls `CancelAllAbilities()`; the audit record states this explicitly.

### Lean Pass

Verified clean (recorded as audit conclusions, no action): zero engine redirect remnants; 34 source files with zero orphan files and zero orphan classes; a clean `.uproject` plugin list; zero Gameplay Tag inventory drift between `ARCHITECTURE.md` and the ini.

Executed items:

1. `APlayerCharacter` removes the legacy `DodgeAction` and `SprintAction` UPROPERTYs and their comments (user decision; runtime input routes through `DodgeSprintAction` since `TODO-01C2`); a full-tree grep confirms zero remaining references. The user re-saves `BP_Player` once in Editor; stale assignments failing there is expected and stays outside version control.
2. `Config/DefaultGame.ini` sets `ProjectName` from the template `Third Person Game Template` to `PolyQuest`.
3. `Input.Dodge` (zero Source references) is scanned against `Content/` package strings with the D4-style inspection; if no asset references it, the tag is removed from the ini, otherwise it is retained with the evidence recorded. `Cooldown.Parry` is confirmed in asset-side Granted Tags use and is retained.
4. `LookAction`, `MouseLookAction`, `Look()`, and `DoLook()` are retained unchanged (the 02B contract keeps them for future UI ownership; they are not unowned residue).
5. The empty `Config/Layouts/` directory is untracked by git; the user may delete it locally and it is excluded from the commit.

### Documentation Synchronization

1. `README.md` status advances through C3C, D1, D2, D3, and D4 (it currently lags five milestones and still names C3C as next); the next stage becomes `TODO-02F`; both the English and Chinese sections are updated with the project's evidence-conscious wording.
2. `ARCHITECTURE.md` drift repairs: the Light Attack blocked-tag list gains `State.Action.Parrying`, and the Light Attack lifecycle section records the `State.Action.CanCancel.Defense` owned tag introduced by D1.
3. `ROADMAP.md` moves `TODO-02E` to Done with the audit conclusions and lean record after the user gate passes; any non-blocking audit finding is attached to its owning entry; the existing debt entries remain unchanged.

### Route And Ownership

```text
Outer: ue-stage-workflow
Primary: ue5-cpp-gameplay
Support: ue5-blueprint-workflow, ue5-debug-validation
Route reason: The stage is an audit/lean/documentation slice whose touched surfaces (retired input properties, config identity, tag config, and project documents) are Main-owned and evidence-gated.
```

```text
Plan explorers: 3 (read-only: HyperArmor lifecycle, lean inventory, documentation drift)
Implementation executors: 0
Complex Executor: none
Main parallel work: none
Reason: All edits are small, evidence-gated, and cross-document; a second writer would risk divergent documentation claims.
```

Main owns the source/config/documentation edits, static review, staging, and the commit boundary. The user owns the `PolyQuestEditor` compilation, the `BP_Player` re-save, Editor readback, Scene01 PIE regression, and commit approval.

### Validation Matrix

- Main static gate: final source re-reads, CodeGraph, `git diff --check`, zero-reference grep after the legacy deletion, tag cross-check, and the `Input.Dodge` package-string scan.
- User gate: compile `PolyQuestEditor`; re-save `BP_Player` and confirm no Missing-property warnings; Scene01 PIE bidirectional combat regression smoke (player attack chain to enemy death, enemy attack through Parry/Guard/Health dispatch, Guard Break, HyperArmor window, interruption/death teardown, movement/camera unchanged).
- Documentation check: README, ARCHITECTURE, and ROADMAP match the final source state.

### Documentation And Commit Boundary

Candidate paths: `PlayerCharacter.h`, `Config/DefaultGame.ini`, `Config/Tags/PolyQuestGameplayTags.ini` (only if the `Input.Dodge` scan passes), `README.md`, `ARCHITECTURE.md`, `ROADMAP.md`, `plan.md`, plus any blocker-repair hunks the audit produces. Exclude every `Content/**` item, generated directories, and unrelated WIP. No commit occurs without explicit user approval.

### Non-Goals

No combat feature, no GAS contract change, no Content asset deletion (lean is Source/Config/documentation-only and evidence-led), no architectural refactor.

### Closeout Record (2026-08-16)

- Audit: all eight surfaces closed with no P0-P2 blocker and no new non-blocking finding requiring reassignment; the HyperArmor dispositions and the EndPlay false-positive clarification are recorded in the Audit Matrix above.
- Lean execution: legacy `DodgeAction`/`SprintAction` removed with zero word-boundary residuals; `ProjectName=PolyQuest`; `Input.Dodge` removed after a zero-match Source grep and a zero-match `Content/` package-string scan; the Look surface, `Cooldown.Parry`, and all Content assets were retained.
- Documentation: README advanced through C3C/D1/D2/D3/D4 with `TODO-02F` next in both languages; ARCHITECTURE repaired the Light Attack, tag inventory, and D4 Player Parry Notify class-name drift; ROADMAP moved `TODO-02E` to Done.
- User validation: the user confirmed the post-lean `PolyQuestEditor` compilation, the `BP_Player` re-save without missing properties, and the Scene01 PIE bidirectional regression smoke. Main ran no UBT, Editor, or PIE.
- Review: GLM's first Main review reported no source finding. A separate fresh review found P2 next-stage documentation drift (`TODO-02F` was incorrectly skipped for `TODO-03A`) and P3 stale `UAnimNotifyState_ParryWindow` documentation; both are repaired above. No source-level health defect was found. Per user instruction, this documentation-only repair does not receive a delta review. Static evidence includes the full diff re-read, dual-side tag-count cross-check (72 = 54+7+8+3), word-boundary residual greps, and `git diff --check`.
- Commit: `97a53a7 [Chore] 双向战斗健康审查与精简清理 (Bidirectional Combat Health Review)` contains `Source/PolyQuest/Public/Character/Player/PlayerCharacter.h`, `Config/DefaultGame.ini`, `Config/Tags/PolyQuestGameplayTags.ini`, `README.md`, `ARCHITECTURE.md`, `ROADMAP.md`, and this plan record. All `Content/**`, `PolyQuest.uproject` changes, generated directories, and unrelated user WIP remain excluded.
