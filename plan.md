# TODO-03AI1：普通近战冷却间距与持续重定位 v1

## 状态

- **Plan state:** CLOSED - implementation, user validation, strict review, fresh review, and documentation closeout completed. Retain this record until the next accepted stage replaces it.
- **Baseline:** `bc1a91e` (`TODO-03AI` planning gate closed). Preserve all existing user-owned Content WIP and unrelated worktree changes.
- **Prerequisites:** `TODO-03AI` and `TODO-03A5B` are closed. `TODO-03H1` waits for both implementation slices below.
- **Primary question:** can an ordinary melee enemy keep issuing bounded target-relative spacing requests for the full attack cooldown, alternate lateral sides, and stop deterministically at cooldown expiry without creating a second AI state machine or damage path?

```text
Outer: ue-stage-workflow
Primary: ue5-state-tree-ai
Support: ue5-cpp-gameplay, ue5-debug-validation
Route reason: the stage adds an AI-owned spacing data contract, Controller-owned navigation state, native StateTree task/condition integration, and user-authored StateTree validation.
```

```text
Plan explorers: 0
Implementation executors: 1
Complex Executor: one scoped lifecycle-sensitive implementation, executed sequentially as 03AI1A then 03AI1B
Main parallel work: none
Reason: Controller cooldown state, navigation completion, and StateTree transitions share one lifecycle; concurrent writers would create competing ownership.
```

## Locked behavior

- `EngagementRange` remains owned by `UEnemyAttackSet` and remains the attack-entry limit. It is not increased to create retreat space.
- The reposition point is **target-centered**. At generation time:
  1. `TargetToEnemy = EnemyLocation - TargetLocation` on the XY plane.
  2. Build a left/right perpendicular from `TargetToEnemy`.
  3. `RepositionPoint = TargetLocation + TargetToEnemyNormalized * PreferredCombatDistance + Side * LateralRepositionDistance`.
  4. Keep the target-centered radius as spacing tuning, then clamp lateral distance via $\sqrt{EngagementRange^2 - PreferredCombatDistance^2}$ so the authoritative attack-distance check `Distance2D(TargetLocation, RepositionPoint) <= EngagementRange` passes before issuing `MoveTo`. If `PreferredCombatDistance == EngagementRange`, lateral repositioning is clamped to `0` (pure straight backpedal to the engagement edge).

- The target point is therefore relative to the current target, while the enemy owns and issues the movement request. Use explicit names such as `TargetLocation`, `EnemyLocation`, `TargetToEnemy`, and `RepositionPoint`; do not use ambiguous `EnemyTargetPoint`/`EnemyPointTarget` semantics.
- During an active attack cooldown, the Controller may keep issuing reposition requests until cooldown expiry, but owns at most one active `MoveTo` and must respect the authored minimum request interval. This is continuous bounded repositioning, not a free-running orbit or per-frame request loop.
- Each successful reposition alternates left/right for the next request. A point-generation or navigation failure may retry the current side once after the configured interval; if that retry also fails, the next new request changes side so a bad point cannot trap the enemy in a same-side loop.
- Every request recalculates from the target's current location. If the player closes inside `PreferredCombatDistance`, the target-relative point naturally produces a retreat/side response; no separate unlimited backpedal state is introduced.
- On cooldown expiry, the Controller stops any active reposition request and existing `Combat`/`Chase` range logic decides whether to attack or close distance. No reposition request may be issued after expiry.
- Reposition stops and fails closed for invalid target, dead/stunned/hit-reacting enemy, teardown, Leash violation, invalid profile, invalid radial direction, failed navigation, or cooldown expiry. It never applies damage, grants abilities, changes GAS tags, or changes attack selection.

## Slice A - TODO-03AI1A: spacing data and Controller navigation primitive

Owned source surface:

- Add `UEnemyAIProfile` under `Source/PolyQuest/Public/AI/EnemyAIProfile.h` and `Source/PolyQuest/Private/AI/EnemyAIProfile.cpp`.
- Add an authored `AIProfile` reference/getter to `AEnemyCharacter`.
- Extend `AEnemyAIController` with cached profile validation, target-relative point generation, Leash checks, request-interval/retry state, and guarded `MoveTo` request/completion handling.
- Add a focused automation file under `Source/PolyQuest/Private/Tests/` for pure spacing/profile behavior. Test-only setters remain behind `WITH_DEV_AUTOMATION_TESTS`.

`UEnemyAIProfile` owns authored data only: `PreferredCombatDistance`, `LateralRepositionDistance`, `RepositionAcceptanceRadius`, `RepositionRetryDelay`, and `LeashRadius`. For this slice, the existing `RepositionRetryDelay` field is the minimum interval between cooldown reposition requests, including a failed-request retry; retain the field name for asset compatibility. All values require finite, positive bounds where applicable; `PreferredCombatDistance` must not exceed the possessed AttackSet `EngagementRange`. Runtime target, next-request time, side selection, retry state, active request ID, and current goal remain Controller state.

Use standard `AAIController::MoveTo`/path-following result ownership. Do not add manual `ProjectPointToNavigation`, a Blackboard, a Behavior Tree, a physics/overlap steering path, or a second movement component. Guard request completion by the active request identity and clear it from `OnUnPossess`, target loss, death, interruption, and teardown.

Validation gate for Slice A:

- User compiles `PolyQuestEditor`.
- Automation `PolyQuest.Enemy.CombatSpacing` passes profile validation, target-relative point construction, `EngagementRange` bound, deterministic L/R alternation across successful requests, one same-side failure retry, minimum request interval, and no request after cooldown expiry. It must not encode a fixed two-request cap.
- Existing `PolyQuest.Enemy.AttackSetSelection`, `PolyQuest.Equipment.TransactionMatrix`, and `PolyQuest.Melee.TraceSourceGeometry` remain regression checks.
- User creates `DA_EnemyAIProfile_GoblinMelee`, assigns it to `BP_Enemy_Goblin`, and reads back every field and reference. No StateTree behavior change is claimed yet.

## Slice B - TODO-03AI1B: StateTree cooldown integration

Owned source/asset surface:

- Add new native StateTree task `FEnemyStateTreeTask_RepositionDuringCooldown` and the minimal cooldown condition required by the asset. `FEnemyStateTreeCondition_CanRequestReposition` and its Controller query remain reusable native API, but the current Goblin asset must not use that immediate-request gate on `Wait -> Reposition`.
- Do not change the serialized base class, instance-data layout, or struct name of `FEnemyStateTreeTask_RequestMeleeAttack`; add a new task type when instance data or lifecycle differs.
- User authors the existing `ST_Enemy_Goblin_Melee` asset so its five top-level intents remain `Patrol`, `Alert`, `Chase`, `Combat`, and `Return`, with a bounded `Combat -> Attack -> Reposition -> Wait` flow.

The new task calls Controller APIs only. The `Combat -> Attack -> Reposition -> Wait` loop may repeat while cooldown remains: one active MoveTo is awaited, then the next request is gated by the Controller interval/retry state. It returns terminal statuses for cooldown expiry, target loss, interruption, and Leash/death paths; it must not leave a stale request running after completion. The user-authored `Wait` delay is `0.5s` and is a polling interval, not a replacement for the Controller-owned minimum request interval. The authored `Wait -> Reposition` transition therefore checks only `Enemy Is Attack On Cooldown`; temporary interval gating is handled inside the task.

Validation gate for Slice B:

- User compiles again and reads back task identity, external Controller binding, completion mode, conditions, and all StateTree transitions.
- User confirms NavMesh coverage for the Goblin fixture and runs PIE: continuous bounded lateral repositioning throughout a long cooldown, left/right alternation, one same-side failed-point retry, retreat when the player pushes inside the preferred band, no point beyond `EngagementRange`, cooldown expiry stopping further requests, attack recovery, target loss, death, stun/hit reaction, and Leash return.
- Existing Goblin attack Montage/Trace/Poise/death behavior and the player equipment/trace regression tests remain intact.

## Handoff to Gemini

```text
工作目录：E:\GameDevelop\PolyQuest
基线：bc1a91e
先读取当前 plan.md，并使用 ue-stage-workflow、ue5-state-tree-ai、ue5-cpp-gameplay、ue5-debug-validation。

严格按 TODO-03AI1A -> 用户编译/Automation/Editor readback -> Main 接受证据 -> TODO-03AI1B 执行；本次修订要求重新对齐此前的有限请求实现。
遵守 target-centered 点位：TargetToEnemy = EnemyLocation - TargetLocation；点位不得超过 EngagementRange；冷却期间持续按最小间隔请求；成功后左右交替；失败允许当前侧重试一次，重试仍失败后换侧；冷却结束立即停止。
不要保留每冷却两次的硬上限：冷却未结束即可继续请求，但同一时刻只能有一个活动 MoveTo，且必须消费 `UEnemyAIProfile::RepositionRetryDelay` 作为最小请求间隔。用户已将 `ST_Enemy_Goblin_Melee` 的 Wait 设为 `0.5s`，它只是 StateTree 轮询间隔，不能替代 Controller 的时间门禁。
攻击选择的远距离 Approach 不属于本次实现：本阶段不要修改 `UEnemyAttackSet::SelectAttackProfile` 的距离仲裁，也不要创建 Approach Ability；该行为记录在后续 `TODO-03AI2`。

允许修改仅限计划中列出的 PolyQuest Source/PolyQuest 测试和 StateTree 原生类型。Blueprint、DataAsset、StateTree 资产由用户在 Editor 作者化；用户已将 StateTree `Wait` 设为 `0.5s`，不得擅自改回。禁止 Bow、Boss、群体协同、Behavior Tree、远距离攻击 Approach 仲裁、第二伤害路径、装备系统、破坏性清理、无关重构和提交。

完成后执行严格自审，只报告修改路径、静态检查、测试结果、已知缺口和停止原因；不得把自审称为 Main fresh review。
```

## Documentation and commit boundary

- Before Slice B validation, update no stable architecture claim.
- After both slices pass the user-owned compile/Automation/Editor/PIE gates, Gemini strict self-review, and Main fresh review, update `ARCHITECTURE.md` with the implemented ownership and `ROADMAP.md` by recording `TODO-03AI1A` and `TODO-03AI1B`; `TODO-03H1` depends on both.
- Closeout evidence: the user confirmed the four named Automation tests succeeded after the StateTree transition edit and reported no behavioral difference; Gemini's strict review and Main's separate fresh/adversarial review found no P0-P2 source defect. The focused commit excludes all authored `Content/**` WIP and unrelated project/editor changes.

## Closeout Record

- **Implementation:** `UEnemyAIProfile`, target-centered/clamped reposition geometry, Controller-owned cooldown/request/retry/alternation state, guarded MoveTo completion, lifecycle cleanup, `FEnemyStateTreeTask_RepositionDuringCooldown`, supporting native conditions, and `PolyQuest.Enemy.CombatSpacing` were delivered in the approved source/test surface.
- **Editor adjustment:** User set `ST_Enemy_Goblin_Melee` `Wait` to `0.5s`, removed only `Enemy Can Request Reposition` from `Wait -> Reposition`, retained `Enemy Is Attack On Cooldown` and the unconditional fallback, then saved/compiled the asset. The C++ condition class was intentionally retained.
- **Validation:** User-confirmed Automation success: `PolyQuest.Enemy.AttackSetSelection`, `PolyQuest.Enemy.CombatSpacing`, `PolyQuest.Equipment.TransactionMatrix`, and `PolyQuest.Melee.TraceSourceGeometry`. The warning lines are expected negative matrix/fixture branches. No new native build was invoked by Main; Editor/PIE evidence is user-owned and reported as user confirmation only.
- **Review:** Gemini performed the strict implementation review. Main performed a separate fresh/adversarial source review after the repair; no P0-P2 defect remained. Static `git diff --check` was clean apart from normal line-ending warnings.
- **Scope:** Stage only the approved native AI/test files and `ARCHITECTURE.md`, `ROADMAP.md`, and `plan.md`. Preserve all unrelated user WIP and authored assets outside the commit.

## Non-goals

No ranged enemy, Bow/Staff projectile behavior, Boss phases, encounter coordination, free-running orbit outside an active cooldown, distance-banded attack/Approach arbitration, attack-memory heuristics, Behavior Tree/Blackboard, new GAS Ability/GameplayEffect, new damage/trace path, inventory, camera-facing rewrite, or broad enemy locomotion system.

## Post-closeout Repair

- **Scope:** Shield defense preflight compatibility with Unreal Editor-authored GameplayTag containers.
- **Cause:** Editor-authored Shield Ability CDOs may serialize only `Ability.Defense.Guard.Shield` / `Ability.Defense.Parry.Shield`; `HasTagExact()` on the generic parent categories incorrectly rejected the otherwise valid Shield pickup.
- **Repair:** `RunPreflight()` retains exact matching for the profile-specific tags and uses hierarchical `HasTag()` for the generic Guard/Parry categories. The transaction-matrix test now exercises the child-only CDO authoring shape and restores the expected native generic-tag setup after the test.
- **Evidence:** The user reran `PolyQuest.Equipment.TransactionMatrix`, `PolyQuest.Enemy.AttackSetSelection`, `PolyQuest.Enemy.CombatSpacing`, and `PolyQuest.Melee.TraceSourceGeometry` with `Success`; expected rejection, rollback, invalid socket, and invalid Poise messages remain negative branches. Main's fresh/adversarial review found no remaining P0-P2 defect. No new native build was invoked by Main; compile/PIE evidence remains user-owned.
- **Commit boundary:** Include only the two native source/test files and the synchronized `ARCHITECTURE.md`, `ROADMAP.md`, and this closeout record. Preserve all user-owned `Content/**`, Config, project, and unrelated WIP.
