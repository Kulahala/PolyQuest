# plan.md - AI Agent Collaboration File

## Rules

1. Read this file before continuing a PolyQuest stage.
2. Keep feedback, active plan, validation evidence, and unresolved blockers scoped to the current stage.
3. The implementation author cannot mark a stage as review-approved.
4. Retain the completed plan and closeout record until the next accepted stage deliberately replaces it.
5. Durable architecture belongs in `ARCHITECTURE.md`; accepted future direction and validation debt belong in `ROADMAP.md`.

---

## Active Stage: TODO-02D4 - Combat Animation Notify Ownership And Naming Audit v1

Baseline: `1b92a79 [Feature] 敌人通知时序霸体 (Enemy Notify-Timed Hyper Armor)`.

### Objective

在进入 `TODO-02E` 前，完成原生战斗 `UAnimNotify` / `UAnimNotifyState` 的语义归属审计和反射命名迁移。D4 只改变四个 Player 专用 Notify 的反射类名和 Editor 显示名；不改变伤害、Gameplay Tag、Ability listener、Montage 时序、AI 或任何战斗生命周期。

### Locked Naming Contract

| Ownership | Current reflected class | Final reflected class | Editor display name |
| --- | --- | --- | --- |
| Shared | `UAnimNotifyState_ActionDodgeCancelWindow` | unchanged | `Dodge Cancel Window` |
| Shared | `UAnimNotifyState_DodgeInvulnerability` | unchanged | `Dodge Invulnerability` |
| Shared | `UAnimNotifyState_AttackTraceWindow` | unchanged | `Attack Trace Window` |
| Player | `UAnimNotify_ChargedAttackHoldReady` | `UAnimNotify_PlayerChargedAttackHoldReady` | `Player Charged Attack Hold Ready` |
| Player | `UAnimNotifyState_ComboInputWindow` | `UAnimNotifyState_PlayerComboInputWindow` | `Player Combo Input Window` |
| Player | `UAnimNotifyState_ComboBranchWindow` | `UAnimNotifyState_PlayerComboBranchWindow` | `Player Combo Branch Window` |
| Player | `UAnimNotifyState_ParryWindow` | `UAnimNotifyState_PlayerParryWindow` | `Player Parry Window` |
| Enemy | `UAnimNotifyState_EnemyHyperArmor` | unchanged | `Enemy Hyper Armor` |

`Shared` only means that the Notify type and event semantics may be consumed by a compatible ASC-owning actor. It does not imply that Player and Enemy currently share every Montage. Existing Gameplay Event Tag names remain unchanged, including Charged HoldReady, Combo, Parry, Trace, Dodge, and Hyper Armor events. D4 adds no generic Hyper Armor/Reaction base class; the existing `SendGameplayEvent` helper remains the shared transport layer.

### Migration Scope

No `CoreRedirects`, legacy aliases, or compatibility wrapper types will be added. The old Player reflected class names must disappear completely from `Source/` after the rename. This intentionally breaks serialized old Notify references until the user migrates each authored Montage in Unreal Editor.

The user explicitly confirmed that the existing Notify tracks are disposable after larger project changes. Exact pre-rename track/time inventory is therefore waived. An Editor recovery point remains recommended, but it does not block the native rename or require the new Player Notify placements to reproduce the old timings.

Package-string inspection is only a migration-candidate aid, not Editor readback. It also identified `AM_Dodge_F`, `AM_Light_Attack02_Sword`, `AM_SprintAttack_Sword`, and `AM_Attack01_Axe` as audit-only assets; they must not be migrated unless Editor readback finds one of the renamed Player classes there.

### Approved Native Change

Only modify:

- `Source/PolyQuest/Public/Animation/Combat/AnimNotifyState_ActionWindows.h`
- `Source/PolyQuest/Private/Animation/Combat/AnimNotifyState_ActionWindows.cpp`

Rename exactly the four Player reflected declarations, definitions, and `GetNotifyName_Implementation()` strings in the table above. Do not move files, change `SendGameplayEvent`, change event payload identity, add tags, change AbilityTask logic, add redirects, or retain old classes under another name.

### Existing Semantic Evidence To Preserve

- `Event.Attack.TraceWindow.Begin/End` is consumed by Player Light, Charged, Sprint, and Enemy Melee, so `UAnimNotifyState_AttackTraceWindow` stays Shared.
- Player Light / Charged / Sprint consume the Dodge Cancel window. `UAnimNotifyState_ActionDodgeCancelWindow` stays Shared because its semantic event contract is not actor-identity-specific.
- `UAnimNotifyState_DodgeInvulnerability` stays Shared by the same reusable semantic contract, even though the current authored use is Player-facing.
- Charged HoldReady, Combo Input/Branch, and Parry Window are currently Player-only Ability contracts, so their reflected classes and display names get the `Player` prefix.
- Enemy Hyper Armor stays Enemy-specific and retains its current reflected class and display name.

### User-Owned Editor Migration

After the user manually compiles `PolyQuestEditor` with the renamed native classes:

1. Open the three migration-candidate Montages and every additional Editor-discovered reference.
2. Remove each old or Missing/Unknown Player Notify.
3. Add the corresponding new Player Notify at the currently intended track and timing; this migration deliberately does not preserve retired placement data.
4. Save every migrated asset.
5. Confirm there are no Missing/Unknown Notify references and no old Player Notify references in Editor.
6. Audit without migration: `AM_Dodge_F`; Player Light/Sprint/Charged Shared Notify placements; `AM_Attack01_Axe` Shared Trace and Enemy Hyper Armor placements.

Do not modify `Content/**` through filesystem tools. The user owns all Montage migration, Blueprint/AnimBP work, imported resources, maps, and Editor validation.

### Validation Matrix

#### Main static gate

- Re-read the four renamed Notify implementations, `SendGameplayEvent`, and all direct event listeners.
- Use CodeGraph before source search; use code-review-graph only as supplemental stale coverage when its built SHA remains `be3fdfd`, behind this stage baseline.
- Confirm the four old class names have no `Source/` residuals after the rename, and cross-check their event tags/listeners remain byte-for-byte semantically unchanged.
- Run C4458-shadowing scan for touched C++ and `git diff --check`.
- Query error memory if an injected `memory` MCP is available. If not injected, record that no memory query ran.

#### User compile and Editor readback

- Compile `PolyQuestEditor` manually.
- Confirm all recorded assets resolve the new Player classes with no Missing/Unknown Notify.
- Confirm old class references are zero in Editor; package-string scanning is supporting evidence only.

#### Scene01 PIE

- Charged holds at HoldReady, releases to resume the attack, and opens the existing Trace Window.
- Light Combo Input/Branch windows still buffer and transition correctly.
- Parry only consumes enemy melee inside its authored Parry Window.
- Dodge invulnerability and Dodge Cancel remain correct.
- Player and Enemy Trace Windows, plus Enemy Hyper Armor, have no regression.

The Charged HoldReady smoke closes `TODO-01H`'s existing validation debt only after the user confirms it in PIE.

### Non-Goals

- No damage, Health, Poise, Stamina, Tag, GameplayEffect, GameplayAbility, AbilityTask, AI, StateTree, Montage timing, collision, or Root Motion change.
- No Player/Enemy common base class or generic reaction/armor framework.
- No `CoreRedirects`, legacy aliases, manual `.uasset` edits, asset imports, asset deletion, or asset commit.
- No renaming Gameplay Event Tags absent a real runtime semantic conflict.

### Route And Ownership

```text
Outer: ue-stage-workflow
Primary: ue5-cpp-gameplay
Support: ue5-blueprint-workflow, ue5-debug-validation
Route reason: Reflected UClass names, serialized Montage references, Editor display names, and GAS event consumers are one migration contract.
```

```text
Plan explorers: 0
Implementation executors: 0
Complex Executor: none
Main parallel work: none
Reason: The four reflected classes, their consumers, and the no-redirect asset migration boundary are tightly coupled; a second writer would increase the chance of orphaned serialized references.
```

Main owns source integration, static review, documentation, staging, commit scope, and evidence accounting. The user owns recovery points, Montage inventory/migration, Editor readback, manual `PolyQuestEditor` compilation, Scene01 PIE, and final commit approval. No live Unreal Editor MCP query or write is part of this stage or claimed as evidence.

### Documentation And Commit Boundary

After user compile, Editor migration/readback, PIE, and review pass:

- `ARCHITECTURE.md` records the stable Shared/Player/Enemy combat Notify naming contract.
- `ROADMAP.md` moves `TODO-02D4` to Done and removes the `TODO-01H` HoldReady debt only with user-confirmed HoldReady PIE evidence.
- This file records the final implementation, user validation, review, debt handoff, and exact commit boundary.

The candidate commit contains only the two action-window C++ files and the exact D4 hunks in `ARCHITECTURE.md`, `ROADMAP.md`, and `plan.md`. Explicitly exclude all `Content/**`, `.uproject`, tag config, GA/GE, Montage, AnimBP, Blueprint, map, input, imported resource, and unrelated user WIP. Do not commit without explicit user approval.

### Implementation And Closeout Record (2026-08-16)

#### Native Implementation

- Renamed only four reflected Player Notify classes in `AnimNotifyState_ActionWindows.*`: `UAnimNotify_PlayerChargedAttackHoldReady`, `UAnimNotifyState_PlayerComboInputWindow`, `UAnimNotifyState_PlayerComboBranchWindow`, and `UAnimNotifyState_PlayerParryWindow`.
- Updated only their `GetNotifyName_Implementation()` display names. `SendGameplayEvent`, every Gameplay Event Tag, `FGameplayEventData` identity field, Shared Notify type, Enemy Hyper Armor type, Ability listener, damage, AI, and Montage-timing logic remain unchanged.
- Added no `CoreRedirects`, aliases, Gameplay Tags, GameplayEffects, GameplayAbilities, or generic Notify hierarchy. Old serialized Player Notify references intentionally require user-owned Editor migration.

#### User Validation Evidence

- The user confirmed the post-migration test route passed, including the focused Charged HoldReady smoke required to close the existing `TODO-01H` validation debt. Main did not run UBT, Unreal Editor, or PIE.
- By explicit user decision, retired Notify track/timing placement was disposable after larger project changes. The Editor migration placed the new Player Notify types at the currently intended authored timing rather than preserving legacy positions.

#### Review Record

- Main normal review found no P0-P2 defect: the final source diff is restricted to four reflected names and four display names; old names are absent from `Source/**`; all event strings and their Charged, Light Combo, and Parry listeners remain unchanged.
- `gpt-5.6-luna / xhigh` was unavailable. Main completed a separate adversarial fallback over UHT/reflection naming, intentional no-redirect asset migration, Shared/Enemy isolation, unchanged event payload identity, and listener compatibility; no P0-P2 defect was found. This is not an independent review.
- The only P3 finding was the old `ROADMAP.md` draft wording that implied pre-rename Editor readback and optional redirects. This closeout replaces it with the accepted no-redirect migration contract and Done record.
- CodeGraph provided direct source/caller evidence. `code-review-graph` and `memory` MCP tools were not injected, so neither is claimed as review coverage. `git diff --check` passed; the C++ diff introduces no local variables and therefore no new `C4458` shadowing surface.

#### Debt Handoff And Commit Boundary

- No new validation debt was accepted. `TODO-01H` Charged HoldReady relocation debt is removed from `ROADMAP.md` because the user confirmed the focused smoke.
- The approved commit is limited to `AnimNotifyState_ActionWindows.h/.cpp` and exact D4 hunks in `ARCHITECTURE.md`, `ROADMAP.md`, and this file. All `Content/**`, `.uproject`, maps, input, Blueprint/AnimBP, Montage, GA/GE, imported resources, generated output, and unrelated user WIP remain excluded.
