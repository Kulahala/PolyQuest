# TODO-03H4A: Authoring Tooltip Metadata v1

## Plan State

- Status: Completed. Gemini implemented the frozen Native Header metadata slice; Main completed the separate defect-first fresh review and the user authorized documentation closeout and commit after confirming the current-stage tests passed.
- Baseline: `a6057ab` (`[Docs] 完成战斗投递与表现健康审查`).
- Objective: make the current core combat authoring surface legible through concise Chinese Editor Tooltips, without changing gameplay behavior, tuning, serialization, asset wiring, or Blueprint graphs before `TODO-03H4B` and `TODO-03C`.
- Pre-implementation static baseline: core Native headers contained rich English `/** ... */` comments but no explicit `ToolTip` property metadata. Rider offline property reads returned no usable CDO property list for representative `.uasset` files, so actual inherited-Native hover text and Blueprint-local variables remain user-owned Editor readback evidence.
- Preserve all current user WIP. `.gitignore`, `Config/**`, `Content/**`, maps, Blueprints, AnimBPs, GA/GE/Montage assets, `.uproject`, generated files, and unrelated source changes are not automatic review or commit candidates.

## Route And Delegation

Outer: `ue-stage-workflow`
Primary: `ue5-cpp-gameplay`
Support: `ue5-blueprint-workflow`
Route reason: the runtime surface is frozen; the Native portion is reflected `UPROPERTY` authoring metadata, while Blueprint-local authoring must be performed and read back in Unreal Editor.

Plan explorers: 0
Implementation executors: 1 (Gemini only after its read-only plan review is accepted and the user explicitly authorizes execution)
Complex Executor: none
Main parallel work: none
Reason: the source work is a bounded metadata-only change. Main retains all authoring policy, public-contract ownership, Editor evidence interpretation, documentation, staging, and commit ownership; Gemini may write only the frozen Header metadata slice.

## Frozen Authoring Contract

### 1. Native Tooltip policy

- Retain existing English C++ documentation comments. Add selected Chinese `meta = (ToolTip = "...")` text only where current core authoring requires a Chinese hover explanation; do not translate comments wholesale or mechanically duplicate every comment.
- Tooltip language is Chinese plus exact technical identifiers such as `GA`, `GE`, Gameplay Tag, `bEnableTargetAssist`, or field names where they prevent ambiguity.
- Every selected Tooltip states only the stable authoring contract: purpose, unit/range where relevant, required companion field or intentional fallback. It must not repeat mutable production balance values or claim unverified visual/runtime behavior.
- Merge `ToolTip` into the existing metadata list. Do not change a property's name, type, default, visibility, Category, `Clamp*`, `UIMin/UIMax`, `Units`, `EditCondition`, replication, public API, Tag, Input route, or source implementation.
- Metadata strings must not contain unescaped ASCII double quotes. Use unquoted identifiers, Chinese punctuation, or single quotes inside the explanatory text so UHT parsing remains unambiguous.
- `ToolTip` examples establish wording style only:
  - `TargetAssistMaxDistance`: maximum horizontal candidate distance in cm; effective only while `bEnableTargetAssist` is enabled.
  - `HomingStartDelaySeconds`: straight-flight time in seconds before limited homing starts; requires `bEnableLimitedHoming`.
  - `DodgeSprintHoldThresholdSeconds`: shared Dodge/Sprint input threshold in seconds; release below it requests Dodge and reaching it requests Sprint.

### 2. Native source scope

Contract owner: Main. Implementation writer: Gemini. In every listed Header, Gemini may modify only an existing current-core editable `UPROPERTY` metadata list to add or improve `ToolTip` text. No `.cpp`, `Build.cs`, Config, test, public function, field declaration shape, asset, or documentation edit is allowed.

- Weapon and trace authoring: `Combat/Equipment/WeaponDefinition.h`, `MeleeWeaponDefinition.h`, `BowWeaponDefinition.h`, `OffHandWeaponDefinition.h`, and `ProjectileDefinition.h`.
  - Cover hand/locomotion presentation, display attachment/offsets, owner-mesh trace profile identity and markers, trace radius/subdivisions, Bow default projectile/launch Socket, OffHand Shield-presentation flag, projectile movement/collision, target assist, homing, and flight-trail terminal presentation.
- Enemy data and Character/AI authoring: `AI/EnemyAIProfile.h`, `AI/EnemyAIController.h`, `Combat/Enemy/EnemyAttackProfile.h`, `EnemyAttackSet.h`, `Character/Enemy/EnemyCharacter.h`.
  - Cover spacing/leash/sight, attack range/cooldown/guard stamina, Attack Set ownership, Poise recovery, and authored failure/relationship constraints.
- Active Native ability authoring: current `EditDefaultsOnly` fields in `AbilitySystem/Abilities/` whose CDO is used by an asset under `Content/_Abilities/Player/**` or `Content/_Abilities/Enemy/**`.
  - Cover authored Montage sections/references and required/optional GE links where the owner/fallback is not self-evident; Charged damage/charge/Poise values; Bow projectile/movement settings; Guard/Parry/Dodge/Sprint/Jump/Stamina links; and Player/Enemy reaction launch speeds.
  - `EnemyMeleeAbility.h` is not a Tooltip target: its Montage, Damage GE, range, and cooldown are immutable runtime snapshots from `UEnemyAttackProfile`; authoring text belongs on `EnemyAttackProfile.h` and `EnemyAttackSet.h`.
- Current Character/component authoring: `Character/BaseCharacter.h`, `Character/Player/PlayerCharacter.h`, `Combat/Melee/MeleeTraceSourceComponent.h`, and `Combat/Equipment/WeaponEquipmentComponent.h`.
  - Cover only active editable combat, feedback, movement threshold, loadout/equipment, static-trace, and presentation fields. Input action bindings, runtime caches, test seams, and visible-only diagnostic state are not Tooltip targets unless they are currently edited as production authoring data.
- A desired Tooltip outside this exact Native scope is a stop condition. Gemini returns the field, source evidence, and why it is current-core authoring data for a Main decision; it must not broaden the file list itself.

### 3. User-owned Editor authoring

- User writes Blueprint-local Tooltip/Description text only in the current core assets under:
  - `Content/_DataAssets/{Weapon,Player_Combat,Enemy}/`
  - `Content/_Abilities/{Player,Enemy}/`
  - `Content/BP/Characters/Player/BP_Player.uasset` and `Content/BP/Characters/Enemy/BP_Enemy_Goblin.uasset`
- Exclude `BP_test`, test fixtures, imported resources, unused/legacy assets, maps, AnimBPs, Montage/Notify tuning, and every asset outside those paths.
- Target only author-editable local variables/references that control combat, movement, trace geometry, projectile targeting, Enemy AI, or presentation. Do not modify values, types, parent classes, `Instance Editable`/`Expose on Spawn` flags, event graphs, functions, or component topology.
- Inherited Native properties receive their explanation from C++ metadata; do not create Blueprint shadow variables to add a Tooltip.
- Built-in GameplayEffect Modifier rows are intentionally excluded. If an asset exposes a real Description field, the user may add the same concise authoring note; otherwise leave it alone. Do not introduce wrapper classes, replacement GE assets, or a generic documentation system.

## Execution Order

1. Gemini performs a first read-only review of this plan, the real Header fields, class/asset ownership, and active asset paths. It reports only P0-P2 blockers, required Main decisions, non-blocking refinements, and Editor/Automation feasibility; it does not edit, compile, write Editor state, stage, or commit.
2. After user acceptance, Gemini rereads the approved plan and makes the frozen Header-only metadata change. It must preserve every runtime/serialization contract and follow the selected wording policy.
3. Gemini runs targeted Rider `get_file_problems` or `lint_files` on each changed Header and `git diff --check`; it then supplies a strict implementation self-review with changed paths, exact Tooltip coverage, static results, unrun user gates, and remaining risks.
4. User compiles `PolyQuestEditor (Development Editor)` and performs Editor readback plus Blueprint/GA/GE-local Tooltip authoring. Gemini never writes `.uasset` files or live Editor state in this stage.
5. Main interprets the evidence, performs one independent defect-first Fresh Review, synchronizes completed-stage documentation, and prepares a scoped commit only after explicit user approval.

## Validation Matrix

### Static gate before user handoff

- Read final diff and each altered reflected declaration; ensure all new metadata is syntactically merged and no field behavior/default changes occur.
- Rider reports zero Errors/Warnings for changed Headers; `git diff --check` passes.
- CodeGraph/code-review-graph are supplemental only. Because the diff is metadata-only and Blueprints are dynamic assets, direct Header review plus Editor readback is the evidence for Tooltip coverage.

### User-owned compile and Editor readback

1. Compile `PolyQuestEditor (Development Editor)` and report the exact result.
2. In the Details panel, hover representative Native fields on `DA_Weapon_Unarmed`, `DA_Projectile_Arrow`, `DA_EnemyAIProfile_GoblinMelee`, one `DA_EnemyAttackProfile_GoblinAxe*`, `GA_Player_Bow_DrawFire`, and `GA_Sword_ChargedAttack`.
3. In `BP_Player` and `BP_Enemy_Goblin`, and the current Player/Enemy GA/GE assets, add/read back only approved Blueprint-local Tooltip/Description text. Confirm inherited Native fields show their C++ Tooltips rather than shadow variables.
4. Confirm text is Chinese plus required technical identifiers, units and companion-field dependencies are correct, and no numeric value, asset reference, class parent, graph, Tag, or input mapping changed.

### Runtime scope

- PIE and Automation are not required for an accepted metadata-only result. If compilation, Editor readback, or accidental asset saving reveals a runtime/property regression, pause the stage and run only the affected compile, Automation, and PIE route before closeout.

## Non-Goals, Documentation, And Commit Boundary

- No numeric rebalance, GE Modifier redesign, new DataAsset field, GameplayCue, generic settings/documentation framework, asset migration, localization pipeline, Blueprint graph work, UI widget work, module change, or `TODO-03H4B` cleanup is included.
- The authored GE values remain the current Editor asset source of truth. Native Tooltip text may explain a GE's role but must not duplicate mutable multipliers, damage values, or timeout tuning into architecture/roadmap prose.
- After validation, `ARCHITECTURE.md` remains unchanged unless a durable runtime contract unexpectedly changes; `README.md` remains unchanged. Main updates `ROADMAP.md` and this plan only at stage closeout.
- Default commit scope is changed Native Header files plus synchronized project documentation. User-authored `Content/**` Tooltip/Description WIP remains excluded unless the user separately approves a stable authored-asset closure and its LFS pointer check.

## Closeout Record — 2026-08-26

- **Delivered surface:** 145 Chinese `ToolTip` metadata entries across 34 approved Native Public Headers. Existing English code comments, reflected field shapes, defaults, categories, clamps, units, edit conditions, runtime logic, Gameplay Tags, Inputs, Config, and assets remain unchanged.
- **Main wording corrections:** clarified Enemy `AttackRange` as an AI execution/approach distance rather than collision range; removed an unsupported death-animation fallback claim for missing Physics Assets; documented current Sight retention, owner-mesh Trace mutual exclusion/default-source rules, target-assist pitch reference, and the Hit Feedback Overlay scope. An accidental BOM-only diff was restored.
- **Static evidence:** Main direct diff review confirmed every changed Header content line is a `UPROPERTY` metadata declaration; metadata scan found no duplicate `ToolTip` key or unsafe ASCII quote; Rider error-only inspection reported zero Errors; `git diff --check` passed. Rider warning-level output on untouched declarations remains existing hygiene debt for the separate slimming/health route, not an H4A behavior change.
- **User evidence:** the user confirmed the current-stage tests passed and explicitly authorized closeout/commit. This record does not invent a separate compile log, PIE result, or Editor hover screenshot beyond that confirmation.
- **Documentation/commit boundary:** `ARCHITECTURE.md` and `README.md` remain unchanged because no runtime contract changed. The scoped commit includes only the 34 Native Headers, this completed plan, and `ROADMAP.md`; all user-owned `Content/**`, `Config/**`, project, map, Blueprint, AnimBP, GA/GE, Montage, and imported-asset WIP remain excluded.
