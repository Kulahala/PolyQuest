# PolyQuest Roadmap

## Purpose And Scope

PolyQuest is a UE 5.8 C++ GAS-first, single-player stylized action RPG. It rebuilds the validated player-facing contracts of the UE 5.7 Test project through a new GAS/StateTree/runtime boundary; the old FSM, save schema, authored assets, and Marketplace content are reference evidence only.

This file is the active route: it records the durable dependency order, open milestones, adoption conditions, canonical validation-debt triggers, and a compact pointer to the current/next stage. Detailed stage baselines, worktree snapshots, validation receipts, and closeout detail live in plan.md. Historical stage closeouts are preserved in ROADMAP-archive.md and are not current behavior authority. Document roles and archive discipline are defined in AGENTS.md.

## Current State

> The TODO-03A7 parent-baseline wording below is a retained historical snapshot from that task's closeout. It is not the template for future stages; future detailed baselines belong in `plan.md`.

- This stage closeout is based on parent `main @ 90ad381` (the committed `TODO-07B4` stage); `TODO-03A7` is implemented and user-PIE-confirmed in the approved stage change, but has no separate manual `PolyQuestEditor` compile or Editor-readback record. Lock-On acquisition/cycle remain strict with fixed per-axis `15%` retention for an already-owned target, while Bow keeps its independent `6%` automatic Target Assist boundary.
- The product route is `/Game/Maps/Scene01` with a fixed elevated oblique camera and a desktop Controller/input boundary. `TODO-03A7B/C/D/F`, `TODO-03H5`, `TODO-03I1/I2`, `TODO-07B5/B6/B7`, and `TODO-03I4` have implementation closeouts recorded in `ROADMAP-archive.md`; their remaining authored/readback/compile debts are tracked only where still applicable in Known Risks. `TODO-03I4` has removed the zero-reference legacy Loadout C++/test surface, while `TODO-07B7` has split the feedback profile by role. Direct weapon fields, Defense Profile, prepared handles, the equipment transaction, and typed feedback profiles are now the relevant contracts. `TODO-05A`, `TODO-05B`, `TODO-07B8-A`, and `TODO-05A1-A/B/C/D1` now have source, focused Automation, user validation, and Main review closeouts; their separate authored/readback/compile debts remain explicit in Known Risks where applicable. Detailed evidence remains in `plan.md`.
- Mutable GameplayAbility/GameplayEffect, Montage, AnimBP, Blueprint, input, DataAsset, map, Niagara, sound, and imported Content remain user-owned local WIP. Manual compilation, Editor readback, PIE/visual checks, imported-asset decisions, packaging, and final commit approval remain separate gates.

## Current Player-Combat Sequence

Before introducing the first ranged enemy, the player-combat route is:

Implemented/PIE-confirmed: TODO-03A3E → TODO-07B4 → TODO-03A7 (entry 0 only) → TODO-05A → TODO-05B → TODO-07B8-A → TODO-05A1-A → TODO-05A1-B → TODO-05A1-C → TODO-05A1-D1 → REC-05A1-03 (Native contract)
Current implementation closeouts: TODO-03A7B (Light entry 0..2 lifecycle adoption) → TODO-03A7C (Charged/Sprint source adoption; authored compile/readback debt remains) → TODO-03A7D (Melee Skill source adoption; Whirlwind authored readback debt remains) → TODO-03A7F (explicit trigger-range contract; authored migration/readback debt remains) → TODO-03H5 (Review-Only health audit; no P0-P2 blocker) → TODO-03I1 (Primary/Sprint direct route) → TODO-03I2 (cross-weapon cancellation/tag taxonomy; Dodge E2E fixture debt remains) → TODO-07B5 (Small Hit Reaction retrigger; real ASC cross-frame fixture debt remains) → TODO-07B6 (initial Combat Feedback DataAsset consolidation) → TODO-03I4 (Legacy Combat Loadout compatibility removal) → TODO-07B7 (typed Player/Enemy feedback profiles; profile-null coverage closed) → TODO-05A (Stagger Front Execution; user gates and review closed) → TODO-05B (Backstab; user gates and review closed) → TODO-07B8-A (Enemy Stance Break RateWindow; authored compile/readback debt remains) → TODO-05A1-A (paired lock) → TODO-05A1-B (lethal recovery) → TODO-05A1-C (Release outcomes; authored compile/readback debt remains) → TODO-05A1-D1 (VictimStart timing and Launch fallback; authored Content remains local WIP) → REC-05A1-03 (Unified Hit Notify Native contract; authored migration debt remains)
Next execution slice: REC-05A1-03-MIG Authored Hit Notify Migration / Legacy Retirement gate (Native contract gate closed; curated asset inventory and readback required)
Following execution slices: TODO-05A1-D2A (Execution Handshake Snap Alignment v1) → TODO-05A1-D2B (Weapon-Specific Execution Montage Selection v1) → TODO-05A1-E (Execution Impact Feedback v1)
Next independent enemy route: TODO-03C (Ranged Enemy v1)
Recommended post-C execution-presentation sequence: REC-05A1-03 Native gate is closed → run REC-05A1-03-MIG on a curated asset inventory (replace old Front/Backstab Notify instances, read back timing and references, then prove zero old Notify/Tag references) → TODO-05A1-D2A (one-shot Snap) → TODO-05A1-D2B (weapon-specific Montage selection) → TODO-05A1-E. New weapon-specific Montages may use the unified contract now; Legacy retirement remains blocked until the migration gate. Then evaluate TODO-07B8-B and conditional TODO-07B8-C from real authored consumers. TODO-03C remains a separate route, not a hard dependency of these presentation stages.
Conditional maintenance slice: REC-03A7-01 (Motion-Warp Diagnostics And Reuse Cleanup) may be activated after the D2A/D2B/E execution-presentation sequence, or earlier only when a concrete ordinary-attack diagnostic need meets its evidence trigger. It is independent of TODO-03C and is not a prerequisite for execution or ranged-enemy work.
Conditional future player-ranged contract: TODO-03I3 (only when a concrete Staff/Mage player route is accepted; not a hard prerequisite for TODO-05A/B or TODO-03C)
Later optional enemy adoption: TODO-03A7E (after the first ranged-enemy gate)

`TODO-03A7` remains deliberately limited to the selected Light entry 0; `TODO-03A7B/C/D/F` preserve their recorded authored-adoption/readback debts. `TODO-03H5` found no P0-P2 blocker in the bounded Motion-Warp health audit. `TODO-03I1` makes `UWeaponDefinition::PrimaryAttackAbilityTag` and optional `SprintAttackAbilityTag` the canonical MainHand Primary/Sprint route; `TODO-03I4` has now removed the former `AssociatedLoadout`/`ActiveCombatLoadout` C++ and test compatibility surface after the user-confirmed zero-reference asset precondition. `TODO-03I2` keeps cancellation and teardown selectors orthogonal from authored skill identity, and its Dodge E2E fixture debt remains tracked below. The post-I2 player-combat route is `TODO-07B5 → TODO-07B6 → TODO-03I4 → TODO-07B7 → TODO-05A → TODO-05B → TODO-07B8-A → TODO-05A1-A → TODO-05A1-B → TODO-05A1-C → TODO-05A1-D1`; `TODO-05A1-A` closed the paired lock-in contact slice, `TODO-05A1-B` closed delayed lethal recovery, `TODO-05A1-C` closed authenticated Release outcomes and paired presentation, and `TODO-05A1-D1` closed notification-timed victim presentation and Launch fallback through their source, focused Automation, user PIE, and Main review gates. `REC-05A1-03` Native contract is now closed; its authored migration/retirement gate is next, followed by `TODO-05A1-D2A`, `TODO-05A1-D2B`, and `TODO-05A1-E`; `TODO-03C` remains the next independent ranged-enemy route. `TODO-03A4B` remains conditional equipment work, not a prerequisite unless a partial-Guard weapon is intentionally authored.
`TODO-03I3` is deliberately conditional: it defines the future Staff/Mage targeting and delivery contract only after a concrete authored player route and player-value question exist. It does not retroactively classify the current Bow as a generic ranged template, and it does not add a hard dependency to punish or the first ranged enemy.

## Route Constraints

Stable ownership and data-flow contracts live in `ARCHITECTURE.md`. Future stages preserve native C++/GAS authority and the existing melee/projectile delivery paths; no parallel FSM, generic action dispatcher, or second damage path is introduced.

StateTree/Controller/GAS boundaries remain explicit: StateTree selects intent, Controllers own AI target/focus/navigation state, and Abilities/GameplayEffects execute combat. Motion Warping is attack-local and optional; unsuitable authored assets may close with an evidence-backed no-adoption decision.

The current runtime has concrete weapon-family paths rather than a universal Melee/Ranged layer: melee uses contact trace/damage delivery, while Bow owns its Draw/Hold/Release and projectile/limited-target-assist path. `Input.Aim` and Bow phase events remain Bow-specific until a concrete route proves semantic equivalence; future Staff/Mage work must use the orthogonal family/delivery/target dimensions recorded in `TODO-03I3`.

Player combat and the first ranged enemy share only the narrow projectile runtime/data contract. Persistence, inventory/rewards, death reload, and multiplayer remain separate product boundaries.

For older completed stages, validation evidence, and exact historical wording, see ROADMAP-archive.md and the Git history. Do not infer current behavior from archived future TODO text.

## Active Milestones

### Player Combat Closure

- The source implementation closeouts for `TODO-03A7B` (Light entry `0..2`), `TODO-03A7C` (Charged release/Sprint Attack), and `TODO-03A7D` (Melee Skill) are recorded in `plan.md` and `ROADMAP-archive.md`; their authored Montage/Notify/Root Motion readback and separate manual compile records remain non-blocking validation debt. `TODO-03A7F` has closed the shared trigger-range source contract, and `TODO-03H5` has completed its Review-Only health audit with no P0-P2 blocker or Source change. `TODO-03I1` has closed the direct route and `TODO-03I4` has removed the zero-reference Loadout compatibility surface from Source/test; the direct route, Defense Profile, prepared handles, and equipment transaction remain the runtime contract. `TODO-03I2` has closed the bounded cross-weapon Tag taxonomy migration; its Dodge end-to-end fixture and independent compile/readback debts remain tracked below. `TODO-07B5` is closed at the source/PIE gate; its real ASC cross-frame retrigger fixture debt is tracked below. `TODO-07B6` established the initial feedback DataAsset consolidation, and `TODO-07B7` completed the typed Player/Enemy profile split, profile-null coverage, user-confirmed final compile, focused Automation, and Scene01 PIE. `TODO-05A`, `TODO-05B`, `TODO-07B8-A`, and `TODO-05A1-A/B/C/D1` have now completed their source, focused Automation, user validation, and Main review gates; their authored/readback/compile status remains explicit in Known Risks where applicable. `REC-05A1-03` is now the execution-presentation prerequisite, followed by `TODO-05A1-D2A/D2B/E`; `TODO-03C` remains the next independent ranged-enemy route.

### Runtime Health And Integration

- `TODO-03H5` is closed as a Review-Only health gate: the four Player Motion-Warp consumers, shared evaluator/bridge, static snapshots, `ReadyForActivation()` re-entry, teardown, UnPossess, ASC/Tag/Input boundaries, and the unchanged `Trace -> Resolver -> Damage GE` path were audited with no P0-P2 blocker and no Source change. The A7F 3.17 exact-stop limitation remains a non-blocking P3; authored readback, independent manual compile, and real cross-frame Task evidence remain validation debt.

### Combat Input And Ability Routing

- `TODO-03I1` establishes MainHand `PrimaryAttackAbilityTag` and optional `SprintAttackAbilityTag` as the canonical Primary/Sprint route; Guard/Parry resolve through the Effective Defense Profile and prepared `1-4` use exact handles. `TODO-03I4` has now removed `UCombatLoadoutDefinition`, `AssociatedLoadout`, Player mirrors, and test compatibility assertions after the user-confirmed zero-reference asset precondition. No Loadout route participates in runtime input or equipment transactions. The detailed closure evidence is in `plan.md` and `ROADMAP-archive.md`.

- `TODO-03I2` is closed as the bounded cross-weapon Tag taxonomy migration. Four orthogonal capability/teardown tags now separate Dodge, Defense, Reaction, and UnPossess cancellation semantics; concrete `Ability.Skill.Whirlwind` identity remains authored independently; the legacy `Ability.Skill.Melee` tag is no longer registered. The stage preserved physical inputs, Bow/Charged phase events, Motion-Warp, Trace/Resolver/Damage, and GAS ownership. User-confirmed Automation and Scene01 PIE passed; the headless Dodge end-to-end fixture and independent compile/readback receipts remain validation debt. `TODO-05A`, `TODO-05B`, `TODO-07B8-A`, and `TODO-05A1-A/B/C/D1` have since closed their own source and user gates; `REC-05A1-03` now precedes `TODO-05A1-D2A/D2B/E` for execution presentation, while `TODO-03C` remains an independent route.

### Combat Feedback Authoring

- `TODO-07B7` is delivered as the role-specific taxonomy closure for the initial TODO-07B6 consolidation: the abstract Base profile retains Overlay only, while typed Player/Enemy profiles expose only their own feedback fields. User-confirmed typed-asset migration, zero-reference legacy-asset deletion, final compile, focused Automation, and Scene01 PIE passed. The profile-null/diagnostic debt is closed; product `.uasset` changes remain user-owned WIP outside this source/document commit. `TODO-05A`, `TODO-05B`, `TODO-07B8-A`, and `TODO-05A1-A/B/C/D1` have now closed their source, user validation, and review gates; `TODO-05A1-E` is the planned execution-specific consumer of this feedback taxonomy, while `TODO-03C` remains independent.

- [ ] TODO-03I3: Player Ranged Targeting And Delivery Contract v1 (conditional)
  - Start only when a concrete Staff/Mage player ability, authored asset route, and player-facing targeting question are accepted. This is a contract/design gate, not permission to invent a Mage implementation or to turn the current Bow path into a universal template.
  - Keep four dimensions explicit and orthogonal: weapon/equipment family (Melee, Bow, Staff, OffHand), attack delivery (contact trace, actor projectile, ground/area effect, beam/line effect), target-selection mode (locked actor, fire-time snapshot with finite tracking, ground point plus authored area shape/extent, route/line, or free direction), and physical input intent (`Primary`, `Aim`, `Guard`, `Parry`, prepared slots).
  - Define the smallest selected Staff/Mage slice against the existing GAS ownership: an actor-target projectile may reuse the immutable projectile runtime and finite fire-time snapshot only where its payload and lifecycle match; a ground/area ability must own a world-space point and shape/extent snapshot, while a beam/line ability owns a world-space direction or segment snapshot. Neither may depend on Lock-On or Bow Target Assist by analogy.
  - Preserve ability-owned snapshots, explicit world/ASC/target validity, cancellation/death/destroy cleanup, and one existing damage GameplayEffect path. No continuous target following, automatic retargeting, global targeting service, or hidden cross-weapon state is introduced.
  - Success requires a reviewed delivery/target matrix for the selected route, an explicit reuse-versus-new-contract decision, focused Automation for snapshot and teardown boundaries, user-owned compile, curated Editor readback of the named assets, and Scene01 PIE for the actual targeting interaction. Unselected modes remain documented boundaries rather than speculative code.
  - No generic Melee/Ranged enum solely for naming, no Bow Draw/Hold/Release merge by analogy, no new physical input action, no broad Ability dispatcher/framework, no Motion-Warp redesign, no second projectile hierarchy, and no damage/trace ownership change.

### Punish

- `TODO-05A: Stagger Front Execution v1` 已完成：正面处决的源码、聚焦 Automation、用户编译/Editor readback/Scene01 PIE 和 Main Fresh Review 均已通过；详细收口保留在 `plan.md` 与 `ROADMAP-archive.md`。其稳定合同是目标必须处于真实 Stance Break 拥有的 `State.Status.Stunned` 且 Poise 归零，沿用现有目标 reservation、几何、可选 Motion Warp、Montage 命中和 `FMeleeHitResolver` 清理路径。

- `TODO-05B: Backstab v1` 已完成：Backstab 保持独立 Player Ability，使用当前 Lock-On 原始目标和激活时后方几何快照；命中帧复验目标有效性、存活、非无敌、非 Stunned 与距离，并通过现有 `FMeleeHitResolver` 单一路径一次性结算。输入顺序保持 Sprint → Front Execution → Backstab → direct Primary，错误目标、Task/Montage、取消、死亡和 teardown 均 fail-closed。用户确认编译、聚焦 Automation 和 Scene01 PIE 通过，Main Fresh Review 无 P0-P2 blocker；详细收口位于 `plan.md` 与 `ROADMAP-archive.md`。
- 05B 不引入潜行/感知/AI 框架、目标侧受害 Ability、双人锁定、延迟死亡或通用处决基类。是否抽取窄共享生命周期，留待后续真实重复度评估；Stance Break 期间的后方动作仍不属于 v1。

- `TODO-05A1-A: Execution Lock-in Contact Window And Paired Lock` 已完成：Front/Backstab 通过同一份执行会话 context 与同步 Request/Release 握手获得目标侧 Victim Lock；`PlayerLocked`、`VictimLocked`、`Invulnerable`、移动锁和 AI/StateTree 锁由各自 Ability/Controller 所有。执行命中仍沿用 `FMeleeHitResolver`，仅当前 context 可穿透 Victim 的无敌状态；当前已有 Lock-On 目标在成对执行态保留，普通候选搜索仍不放宽。用户确认编译、相关 Automation 与 Scene01 PIE 通过，Main Fresh Review 无 P0-P2 blocker；详细收口位于 `plan.md` 与 `ROADMAP-archive.md`。

- [ ] TODO-07B8: Combat Montage RateWindow Adoption v1
  - Slice A, Enemy Stance Break (`TODO-07B8-A`): 已完成。复用现有 `UAnimNotifyState_MontageRateWindow` 事件，在 `UEnemyStanceBreakAbility` 中按 active Montage/Sequence identity 接入 Begin/End；保存并恢复实际 baseline play rate，覆盖 Montage 中断、Ability 取消、销毁和 UnPossess 清理。用户确认专项 Automation 与 Scene01 PIE，未单独归档的编译/readback 债务见下方。
  - Slice B (`TODO-07B8-B`), Selected Enemy Montage RateWindow Adoption: 在真实资产存在且有明确时序收益时，逐个接入 Enemy HitReaction、Launch 或 Melee Montage；每个消费者单独验证 Begin/End、嵌套窗口、中断、取消、销毁和 UnPossess 恢复。现有 Player 消费者只做回归验证，不进行无收益的批量重写。
  - Slice C (`TODO-07B8-C`), Player/Enemy RateWindow Lifecycle Unification (conditional): 只有在 Slice B 或其他真实 Enemy consumer 与至少一个真实 Player consumer 都暴露出重复生命周期缺陷或可量化维护成本时启动。先冻结窄的 Ability-side helper/protocol，选择一个 Player 与一个 Enemy consumer 做试点并通过 focused Automation、编译和适用的 Editor/PIE 门禁；试点语义兼容后才逐个评估 Light Combo、Charged、Sprint、Bow、Dodge、HitReaction 等消费者。不得强行合并 pseudo-End、Hold、Recovery 或 Dodge 独有生命周期，也不得改变 GAS、Tag、Input 或伤害所有权。
  - “通用”定义为可复用的 Ability 侧生命周期协议/窄 helper 和统一事件契约，不是 `ABaseCharacter` 的全局监听器。每个 Ability 必须显式 opt-in、校验 `Payload.OptionalObject`、处理重叠窗口策略，并在所有终止路径恢复速率。
  - RateWindow 是局部 Montage 播放速率乘数，不是全局时间膨胀；1 秒完整窗口要接近 3 秒时可从 `RateMultiplier ≈ 0.33` 开始，部分窗口按实际未缩放段计算并由 Editor/PIE 调整。`Stunned` 生命周期仍由 Stance Break Ability 所有，不能把固定 3 秒当作后续双人锁定的正确性依赖。
  - Slice A（`TODO-07B8-A`）已作为 `TODO-05A1-A` 的推荐支撑门完成，但不是锁定协议的硬正确性依赖；RateWindow 结束前后都必须能安全转移到目标侧受害者 Ability。B/C 也不得通过第二条伤害路径、全局 Time Dilation 或自动修改非战斗 Montage 来实现。

- `TODO-05A1: Execution Lock-in & Recovery v1` 已完成。A/B/C/D1 四个切片依次闭合成对锁定、授权命中/DeathPending、Authenticated Release、致死/非致死结果分流与通知时序驱动的受害者表现；稳定契约和验证证据见 `plan.md` 与 `ROADMAP-archive.md`。REC-05A1-03、D2A/D2B、E 组成当前优先的表现闭环，`TODO-03C` 仍保留为独立的远程敌人路线，不在本条重复展开已完成切片。

- `TODO-05A1-D1: Execution Victim Presentation Timing And Launch Fallback v1` 已完成：握手只建立锁定，`UAnimNotify_PlayerExecutionVictimStart` 经 Player/Context/Token/Actor/ASC/动画身份校验后同步转发同一 `Event.Action.Execution.Request.VictimStart`，受害者 Montage 延迟到该通知才播放；`VictimStart -> Hit -> Release` 不改变既有唯一伤害、结果提交和 Launch/Death 所有权，缺失/失败表现安全降级。用户确认 PIE 与 Automation，Main Fresh Review 无 P0-P2 blocker；详细路径和证据保留在 `plan.md` 与 `ROADMAP-archive.md`。

- REC-05A1-03 Native Contract Gate：已完成。统一 Notify/Tag、Front/Backstab exact listener、Legacy 兼容和用户编译/readback/Automation/PIE 门禁均已通过；详细收口位于 `plan.md` 与 `ROADMAP-archive.md`。Legacy 路径仍保留，等待下方独立迁移门。
  - 现阶段已确认用户编译、Tag/Notify Editor readback、ExecutionHitNotify/FrontExecution/Backstab Automation 与 Scene01 PIE；Main Fresh Review 未发现 P0-P2。新武器专属 Montage 可使用统一 Notify。
- [ ] REC-05A1-03-MIG: Authored Execution Hit Notify Migration / Legacy Retirement
  - 以 Editor/Reference Viewer 建立全部 Front/Backstab Execution Montage 与旧 Notify 引用清单；只替换旧 Hit Notify，不改动其他事件时序或二进制文件。
  - 每个替换资产读回 Notify 类型、Tag、时序和引用；运行旧/统一入口回归、用户编译和 Scene01 PIE。所有旧 Notify 类/Tag 达到零引用并完成 Main Review 后，才另立删除 Legacy listener、旧 Notify 类和旧 Tag 的提交。
  - 清单、readback 或 authored 基线不完整时保持 Legacy 兼容；该门不包含 D2A Snap、D2B 武器 Montage 选择或 E 反馈。

- [ ] TODO-05A1-D2: Execution Presentation v1（拆分为两个独立子阶段）
  - [ ] TODO-05A1-D2A: Execution Handshake Snap Alignment v1
    - 在 REC-05A1-03 Native 契约门通过后，握手成功、玩家 Montage 启动前，将玩家一次性吸附到执行侧锚点；Backstab 使用受害者正后方，Front 使用受害者正前方，保持现有 Front/Backstab 语义。
    - 对齐距离由 `UMeleeWeaponDefinition::ExecutionSnapDistance` 单一作者化字段拥有，每个近战武器定义可独立调整；D2A 不把方向拆成多个猜测字段，只有真实资产证明需要差异时才另行扩展。
    - 处决路径完全停用执行专用 Motion Warping、地面模式判断和逐帧跟随；普通 Light/Charged/Sprint/Skill 的 Motion Warping 语义不变。D2A 需要纯 C++ 对齐矩阵、碰撞失败清理、Target `MOVE_None` 回归和 Scene01 PIE。
  - [ ] TODO-05A1-D2B: Weapon-Specific Execution Montage Selection v1
    - 在 D2A 稳定后再增加 Front/Backstab 武器专属 Player Montage、激活时快照和装备切换锁；缺失配置不隐式回退到 Unarmed 或全局 CDO。
    - D2B 沿用 REC-05A1-03、D1 的 Hit/VictimStart/Release 和 D2A Snap 距离契约；需要真实 Montage 作者化、Editor readback、Automation、编译和 Scene01 PIE。

- [ ] TODO-05A1-E: Execution Impact Feedback v1
  - 在授权执行 Hit 成功且 Exactly-Once 结算确认后，复用现有 Player Combat Feedback/Controller-owned Hit-Stop/Camera Shake 路径，并复用现有 Enemy typed feedback profile 的冲击音与血液 Niagara；不新建 `ExecutionFeedbackComponent` 或第二条伤害路径。
  - Hit 负责一次性伤害确认、震屏/Hit-Stop 和出血冲击；Release 只负责 Launch 或存活站立的运动与姿态结果，Victim Montage 自身负责 Small/Big 等纯动画表现。取消、命中失败、重复事件和未授权伤害不得触发执行反馈，普通伤害反馈不得被重复叠加。
  - Camera Shake、Hit-Stop 参数、血液 Niagara 和音效仍由用户在 Editor 作者化；关闭条件是 focused Automation 的 exactly-once/分流断言、用户编译/readback 与 Scene01 PIE 视觉验证。

- Boss 投技目前只是架构假设，不是已接受需求或实施阶段。现有 `UExecutionLockContext` 的 token、双侧锁定、同步事件、授权命中和清理经验可作为未来参考，但 Front/Backstab/EnemyVictim 当前合同不宣称直接支持 Boss Grab/Throw；出现真实 Boss 投技需求时再单独制定 `Boss Grab/Throw` 阶段，不提前抽取通用处决基类。

- TODO-05C is retired from the active route. Its former front-execution slice is absorbed into TODO-05A; its former Stagger Backstab slice is explicitly not adopted.

### Ranged Gate And Conditional Equipment

- [ ] TODO-03C: Ranged Enemy v1
  - Add one StateTree-driven ranged enemy only after TODO-03A3E, TODO-07B4, TODO-03A7, TODO-03A7B, TODO-03A7C, TODO-03A7D, TODO-03A7F, TODO-03H5, TODO-03I1, TODO-03I2, TODO-07B5, TODO-07B6, TODO-03I4, TODO-07B7, TODO-05A, TODO-05B, TODO-07B8-A, and TODO-05A1 each pass their own adoption, compile, Automation, Editor/PIE, and review gates (or a Motion-Warp slice closes with an evidence-backed no-adoption decision).
  - Reuse the existing immutable 03B projectile runtime and GAS delivery path. Add only enemy ranged Profile/Ability/StateTree timing and an AI-owned target snapshot; enemy projectiles are explicitly authored straight/non-homing by default.
  - Do not create Player Lock-On, global Target Assist, a second projectile hierarchy, or a second damage path.

- [ ] TODO-03A7E: Enemy Melee Motion-Warp Adoption v1 (post-ranged, optional)
  - Revisit `UEnemyMeleeAbility` only after the first ranged-enemy gate and a concrete enemy authored need. Keep AI/Controller/StateTree intent separate from the Ability-owned one-shot target snapshot and Root Motion lifecycle.
  - Reuse the Player evaluator only if the geometry and ownership contract is demonstrably identical; otherwise define a bounded Enemy-specific evaluator. No Player component sharing, continuous chase, AI retarget loop, or change to Enemy Trace/Resolver/Damage ownership.
  - This stage is not a prerequisite for `TODO-03C`; it requires its own authored Montage/Notify/Root Motion, Automation, compile/readback, PIE, and teardown evidence.

- [ ] TODO-03A4B: Weapon Defense Outcome Profiles v1 (conditional)
  - Before intentionally authoring a weapon with partial Guard damage, define one explicit no-defense/full-absorb/bounded-partial outcome while keeping Parry distinct and the existing Guard/Resolver/Damage GE ownership.
  - It is not a prerequisite for the current player-combat sequence while all authored Guards remain full absorb.

### Player Loop And Persistence

- [ ] TODO-03D: Checkpoint, Rest, Death Reload, And Save Foundation v1
  - Establish new PolyQuest checkpoint/rest, death reload, stable Save IDs, one transaction owner, failed-save behavior, and a terminal Player-Ability cancellation route through each Ability's EndAbility cleanup.
  - Do not migrate old SaveGame identifiers, raw actor references, or old world names.

- [ ] TODO-03D1: Rest-Site Combat Action Preparation And Loadout Persistence v1
  - At a save/checkpoint interaction, arrange compatible MainHand/OffHand ability classes or Bow projectile definitions into 1-4. UWeaponEquipmentComponent remains the runtime owner; SaveGame stores only a validated serializable selection snapshot.
  - This is focused preparation UI/persistence, not a backpack, skill tree, crafting, or mid-combat loadout system.

- [ ] TODO-03E: Potion And Consumable Loop v1
  - Add the first healing consumable after ownership, save, interruption, and cancellation contracts exist; retain notify-timed healing only where the chosen animation needs it.

- [ ] TODO-03F: World Drops, Fixed Rewards, And Reward Persistence v1
  - Separate transient currency drops from persistent fixed rewards and one-time elite defeat state. Each durable reward needs a stable authored ID, one transaction owner, failed-save behavior, and a reload fixture.

- [ ] TODO-04A: Encounter, Fog Gate, And Clear Persistence v1
  - Build the encounter clear-persistence loop, fog boundary, and participant ownership after the save/checkpoint boundary is verified. Add multi-enemy coordination only if the first real encounter proves ordinary StateTree/GAS spacing insufficient.

- [ ] TODO-04B: Persistence And Encounter Health Review v1
  - Audit Save IDs, transaction ownership, reload/re-entry, rewards, encounter cleanup, fog-gate state, and actor/subsystem/authored-data boundaries after the pre-03C punish gate. This review fixes only confirmed data-integrity/lifecycle blockers.

### Level, Boss, And Release Regression

- [ ] TODO-06A: First Stylized Level Route And Integration v1
  - Integrate checkpoint, ordinary combat, elite/fixed-reward branch, encounter, fog gate, Boss approach, and focused route validation without adding a second reward or encounter system.

- [ ] TODO-06B: First Boss Phase And Completion v1
  - Add one Boss StateTree/combat profile, Phase 1 -> transition -> Phase 2 intent, authored abilities, completion persistence, reward handoff, and return/continue behavior.

- [ ] TODO-06C: Route Health And Lean Review v1
  - Exercise the integrated route and audit ownership, dead references, temporary fixtures, obsolete configuration/assets, interruption/death/reload teardown, and documentation drift. Any removal still requires zero-reference or replacement evidence and explicit approval.

- [ ] TODO-07C: Full Route Regression v1
  - Establish repeatable New Game, Continue, checkpoint/rest, combat, equipment, consumable, reward, ranged, punish, Boss, and completion-Continue validation. Fix regressions only; do not add a new gameplay system here.

### HUD And Presentation

- [ ] TODO-07A2: Extended HUD And Combat Observability v1
  - Add only the HUD/debug surfaces needed to make validated Poise, target, damage, and later route state legible while retaining the ASC-backed read-only ownership of TODO-07A1.

- [ ] TODO-07B: Feedback, Presentation Retune, And Demo Polish v1
- TODO-07B1/07B1A, TODO-07B2, TODO-07B3, TODO-07B4, TODO-07B5, TODO-07B6, and TODO-07B7 are delivered; `TODO-03I4` has separately removed the legacy Loadout compatibility surface. The umbrella owns focused presentation work, not new combat ownership.
  - Re-author final Montage composition and Notify timing only after the selected animation set is stable. Reuse the proven TODO-02F rate lifecycle; do not add overlapping playback-rate systems or use Motion Warping for locomotion.


## Recommendations

- Keep the current fixed 15% Lock-On retention and Bow 6% Target Assist values stable until a focused stage presents new evidence.
- Keep TODO-03A4B conditional; do not build partial Guard or generic mitigation before a real authored weapon requires it.
- Treat a missing or unsuitable Motion-Warp asset as a valid evidence-backed no-adoption outcome.
- Keep Behavior Trees unadopted unless StateTree fails a concrete production requirement.
- Use one shared interaction candidate snapshot for prompt and pickup; do not add a generic interactable framework for TODO-03A3E.
- For weapon authoring reuse after TODO-03I1, keep `UWeaponDefinition` direct fields and `BaseGrantedActions` as the runtime source of truth. With the current small asset set, duplicate a validated weapon/template asset first; consider a bounded C++/Blueprint family-default stage only when multiple new weapon families repeat the same route configuration or manual drift becomes a real cost. Any defaults must remain opt-in, preserve exact direct-route and OffHand fail-closed validation, and require Editor readback; do not add global defaults or an automatic asset migration by analogy.
- Combat Feedback authoring now uses the delivered TODO-07B7 taxonomy: an abstract Base profile for shared Overlay and typed Player/Enemy profiles for role-exclusive data. Preserve character-owned runtime lifecycles and do not add further per-enemy subclasses unless a later enemy has a genuinely different feedback contract.
- `TODO-07B8` remains an accepted umbrella for concrete Enemy Montage timing needs. Its Slice A (`TODO-07B8-A`) now has the opt-in Stance Break implementation, focused Automation, user PIE, and Main review closeout; `TODO-07B8-B` gives future HitReaction/Launch/Melee consumers individual assets-and-tests gates, while `TODO-07B8-C` is the conditional Player/Enemy helper pilot rather than a batch rewrite. `TODO-05A1-A/B/C/D1` has now closed the paired lock, lethal-hit Recovery, Release outcome, and notification-timed victim presentation slices; REC-05A1-03, D2, and E are the prioritized execution-presentation follow-ups. Keep the RateWindow and execution routes independently gated; a shared execution base may still be considered only after measured 05A/05B duplication and a separately accepted contract. These follow-ups remain separate from the `TODO-03C` ranged-enemy contract.
- `REC-05A1-02`: Contextual Animation Compatibility Evaluation (conditional recommendation). Activate only after two or more real paired execution assets or materially different enemy body types demonstrate that Motion Warping plus Notify timing cannot provide stable spatial alignment, and the team explicitly accepts enabling UE 5.8's experimental Contextual Animation plugin, module dependencies, and authored assets. The first slice is a compatibility spike: verify plugin/module availability, one known-good paired asset and readback, one play/cancel/destroy path, and Scene01 PIE. Contextual Animation may own presentation-time sync points and spatial alignment only; `UExecutionLockContext` and GAS continue to own request validation, locks, hit authorization, Release, death/nonlethal outcomes, and cleanup. Do not add a generic `AbilityTask_PlayContextualAnimAndWait`, production plugin dependency, asset migration, or Boss grab/throw contract until the spike passes. Record evidence-backed no-adoption if the experimental plugin, module/build cost, asset workflow, or runtime stability is unsuitable; continue the existing Motion Warping/Notify route.
- `REC-05A1-03`: Native Unified Execution Hit Notify and Tag contract is closed at the source, user compile, Editor readback, focused Automation, Scene01 PIE, and Main review gates. Its separate authored migration/retirement gate is `REC-05A1-03-MIG`: curated asset inventory, Editor-only replacement, timing/reference readback, old/new regression, zero-reference proof, and a later deletion commit. New weapon-specific Montages use the unified Native contract; old paths remain until MIG closes. Do not patch `.uasset` files, run an automatic migration, add a broad parent/child event match, or introduce a second damage route. This remains separate from D1 `VictimStart`, D2A Snap alignment, D2B weapon selection, E feedback, and any future Boss grab/throw requirement.
- `REC-05A1-03-MIG` owns the authored migration sequence after the Native gate: inventory every current Front/Backstab execution Montage, replace only the old Hit Notify instances in Unreal Editor, read back event timing and references, run old/new entry regression, and retain the legacy classes/tags until Reference Viewer/readback proves zero references. It is not permission to edit binary assets on disk or to remove compatibility code early.
- `REC-03A7-01`: Motion-Warp Diagnostics And Reuse Cleanup v1（条件性推荐）。仅在至少两个真实普通近战消费者（当前图谱显示 Light、Charged、Sprint、Melee Skill）出现重复诊断分支、失败原因分类不一致，或出现可量化的跨 Ability 排障成本后启动。阶段只统一开发构建下的诊断结果/日志格式和窄的失败原因分类；不得合并各 Ability 的目标快照、接地/距离/角度判定、Motion-Warp 生命周期、清理或伤害所有权，不得把诊断接口变成全局运行时服务。需要覆盖真实消费者的 focused Automation、手动编译、适用的 Editor readback 与 Scene01 PIE，并证明返回值、目标选择和普通攻击表现不变。该阶段默认排在 D2A/D2B/E 之后，且不成为 `TODO-03C` 或任何处决阶段的硬前置。
- No additional execution-specific REC is promoted at this time. Unarmed is not an implicit player-Montage fallback; Small/Big remains authored Victim presentation rather than a new result selector; Boss Grab/Throw remains an unaccepted requirement; and a shared Front/Backstab Ability base waits for measured duplication and a separately accepted contract (the existing conditional `TODO-07B8-C` route).

## Known Risks And Validation Debt

- Authored GA/GE/Montage/AnimBP/Blueprint/input/DataAsset/map fixtures remain local WIP; source/config commits are not clean-checkout reproductions. Close only with an explicitly curated asset baseline and readback.
- REC-05A1-03 Native gate evidence is complete for the user-confirmed compile, Editor readback, named focused Automation, and Scene01 PIE. The remaining debt is authored migration and zero-reference proof; it closes only through REC-05A1-03-MIG and must not be solved by source-side deletion.
- Cost or state may be committed before rare Montage startup failure is known for Light, Charged, Dodge, Sprint Attack, or Parry. Close with a controlled post-commit playback-failure matrix before networking or frame-exact cost claims.
- Keyboard/mouse is the accepted input evidence; controller mappings and hardware behavior are not support claims until readback plus focused hardware validation exists.
- Player terminal Ability cancellation on death remains owned by TODO-03D; current Dead tags block new activation but do not promise cancellation of an already-active Bow or other Ability.
- The legacy BladeTraceBase/BladeTraceTip fallback remains for the current enemy fixture. Remove only after all enemy fixtures migrate to StaticMeleeWeaponDefinition and Reference Viewer proves zero references.
- Bow's fixed 6% no-lock Target Assist overscan is deliberate current tuning. A second projectile family requiring different bounds must introduce a finite validated definition field rather than a global targeting setting.
- Directional Small/Big reaction expansion remains deferred until compatible assets, a bounded selector, and focused front/back/left/right PIE evidence exist.
- Enemy Launch abnormal-Poise recovery and the Parry/Trace combined exactly-once assertion remain documented follow-ups with their existing focused closure triggers.
- The retained legacy SwordShield BlendSpace asset is authoring clutter, not a runtime path; deletion requires Reference Viewer evidence and explicit approval.
- The hierarchical Primary-tag positive fixture remains conditional until a real Ability.Attack.Primary child tag is adopted.
- TODO-03A3E's native Automation coverage primarily uses transient test seams; direct overlap/delegate depth and an explicit assertion that a stale E snapshot leaves equipment unchanged remain validation debt. Close only if a future interaction regression or dedicated test-depth stage needs those paths; this is not a blocker for TODO-07B4.
- `PolyQuest.Equipment.TransactionMatrix` now contains the corrected source asset path, but still depends on a local WIP Guard asset and has no fresh compile/readback evidence. Keep the `Content/**` asset outside this stage; close in a separately approved equipment/asset baseline with user readback.
- `Config/Automation/Presets/1.json` is not a complete test manifest. The 25/25 result recorded in the archived TODO-03A3E closeout comes from the user's manual selection of all current PolyQuest suites, not from the preset; a preset refresh is optional config maintenance, not a TODO-03A3E blocker.
- TODO-07B4 source and focused Automation are confirmed, and the user has confirmed the focused Scene01 PIE route. This closeout does not contain a separate manual `PolyQuestEditor` compile record or six-field `BP_Player` Editor readback; treat the attacker-field authoring baseline as uncurated until those exact gates are read back. The debt closes before packaging or a clean authored-fixture claim and is not a blocker for the next source-only planning slice.
- TODO-03A7 source and focused Automation/Scene01 PIE are confirmed. Its remaining debt is no separately recorded manual `PolyQuestEditor` compile or Motion-Warp asset readback; the production-entry coverage debt was addressed in TODO-03A7B. Close the authored readback before claiming a clean baseline; it is not a blocker for drafting the next player-combat plan.
- TODO-03A7B source, focused Automation, and Scene01 PIE are confirmed, but no independent manual `PolyQuestEditor` compile or entry 1/2 Motion-Warp Editor readback is recorded. Keep this as a non-blocking authored-validation debt; close when the user records the compile and the named `DA_Combo_StraightSword`/Montage/AnimBP/component readback, or record an evidence-backed no-adoption result for unsuitable entries. The current 3.14 seam does not independently construct a post-`ReadyForActivation()` synchronous task end; close that coverage gap only if a deterministic seam is available without weakening production task gates.
- TODO-03A7C source, focused Automation, and Scene01 PIE are confirmed for Charged release and Sprint Attack. No independent manual `PolyQuestEditor` compile or per-Ability GA/Montage/Notify/Modifier/Root Motion Editor readback is recorded; keep this as non-blocking authored-validation debt. Close it with the named asset readback and compile, or with real evidence-backed no-adoption for an unsuitable Ability. The existing test seam does not replace production Task/Montage timing proof.
- TODO-03A7D source, focused Automation, and an earlier Scene01 PIE confirmation are recorded for Melee Skill. The post-fix round has no separate new PIE receipt, no independent manual `PolyQuestEditor` compile record, and no `GA_Skill_Whirlwind`/Montage Editor readback; keep the source slice closed but the authored adoption gate open. Close it with the named readback/compile, or with real evidence-backed no-adoption. The existing test seam does not replace production Task/Montage timing proof.
- TODO-03A7F source, focused Automation, and Scene01 PIE are confirmed for the explicit `MinTriggerDistance`/`WarpStopDistance`/`MaxTriggerDistance` contract across the four current Player consumers. No independent user `PolyQuestEditor` compile receipt or authored field/Montage/Notify/Modifier/Root Motion readback is recorded; keep the source contract closed but the intentional field-migration/adoption gate open. Close it with the relevant Editor readback and compile, or with real evidence-backed no-adoption. The 3.17 exact-stop integration assertion has a non-blocking P3 limitation because `StartComboEntry()` clears a pre-existing target before the evaluator result is observed.
- TODO-03H5 Review-Only audit completed at `a3e0f78` with no P0-P2 blocker and no Source change. `ReadyForActivation()` re-entry, Player/Enemy invalidation, the UnPossess cancellation asymmetry, and the unique Trace/Resolver/Damage ownership were checked from static source evidence; the latter asymmetry remains a design decision for `TODO-03I1/03I2`, not an H5 defect. The 3.17 exact-stop P3 and authored readback/manual-compile/cross-frame Task evidence debts remain open with their existing closure triggers.
- TODO-03I1 establishes direct MainHand Primary/Sprint routes, while TODO-03I4 removes the former `AssociatedLoadout`/`ActiveCombatLoadout` compatibility surface after the user-confirmed zero-reference asset precondition. User-confirmed affected Automation and Scene01 PIE are recorded; no separate manual `PolyQuestEditor` compile receipt is archived for I4. Close that evidence gap when an independent compile receipt is needed; it does not block TODO-07B7.
- TODO-03I2 source closeout establishes the orthogonal `Ability.Action.CancelableBy.*` and `Ability.Action.Teardown.OnUnpossess` selectors and removes the legacy `Ability.Skill.Melee` registration. User-confirmed Automation and Scene01 PIE passed, but `Debt-03I2-DodgeSkillCancelUnitTest` remains open because headless Automation lacks a real AnimInstance/Montage cancel-window fixture; close it with a dedicated integration fixture that activates Dodge and cancels a tagged skill end to end. No independent manual `PolyQuestEditor` compile or Main Reference Viewer receipt is archived for I2.
- TODO-07B5 source closeout establishes Player/Enemy Small Hit Reaction retriggering through per-round `UAbilityTask_PlayMontageAndWait` callbacks, with old-task delegate teardown before replacement and `State.Action.SmallHitReacting` retained as an owned active-state tag rather than a self-block. User-confirmed `PolyQuest.Combat.HitReaction` Automation and Scene01 PIE passed; `Debt-07B5-RealASCRetriggerE2E` remains open because headless Automation does not prove the complete `HandleGameplayEvent -> GAS retrigger -> latest TriggerEventData -> cross-frame Montage Task` chain. Close it with a valid AnimInstance/Montage integration fixture or dedicated PIE/Automation scene; no independent manual `PolyQuestEditor` compile or Editor readback receipt is archived for 07B5.
- TODO-05A source implementation, `PolyQuest.Combat.FrontExecution` Automation, user compile/Editor readback/Scene01 PIE, and Main Fresh Review are complete with no P0-P2 blocker. The v1 contract intentionally remains ordinary-attack cancelable; its lock-in/Recovery evolution is now owned by the accepted `TODO-05A1` route.
- TODO-05B source implementation, `PolyQuest.Combat.Backstab` Automation, user compile/authored setup/Scene01 PIE, and Main Fresh Review are complete with no P0-P2 blocker. The v1 contract intentionally uses an activation-time rear snapshot, ordinary GAS cancellation, and no target-side lock-in; its bidirectional lock and lethal-hit Recovery evolution is now owned by the accepted `TODO-05A1` route.
- `TODO-07B8` remains open only for future Slice B/C consumers. `TODO-07B8-A` has completed its opt-in Enemy Stance Break implementation, focused Automation, user Scene01 PIE, and Main review; no independent manual `PolyQuestEditor` compile or explicit `GA_EnemyStanceBreak`/`AM_StanceBreak` Editor readback is archived, so that authored-validation debt remains open. A tuned three-second window is not a substitute for `TODO-05A1` lock transfer.
- `TODO-05A1-A` is closed with source, focused Automation, user compile/PIE, Lock-On retention repair, and Main Fresh Review evidence. `TODO-05A1-B` is closed with the authorized hit transaction, `DeathPending`, delayed terminal death, Lock-On retention through pending, focused Automation, user validation, and final delta Fresh Review. `TODO-05A1-C` is closed at the source, focused Automation, user PIE, and Main Fresh Review gates: it owns the authenticated Release Request/Release handshake, lethal finalization, and nonlethal Launch-or-standing fallback. `TODO-05A1-D1` is now closed at the source, focused Automation, user PIE, and Main Fresh Review gates: it owns notification-timed Victim presentation while preserving the existing Hit/Release outcome contract.
- `Debt-05A1-C-PairedReleasePresentation` is closed. The Release state axis, Hit/Release ordering, formal-release fail-safe cleanup, optional Victim Montage, Player-tail retention, and post-Release outcome dispatch are implemented and covered by focused Automation; the second repair round added the real GE-failure cleanup path and reached 15/15 sections `Success`.
- `TODO-05A1-C` has no separate archived manual `PolyQuestEditor (Development Editor)` compile or Editor readback receipt. User PIE and Automation are confirmed, but this authored-validation gap remains non-blocking until a compile/readback receipt or evidence-backed no-adoption decision is recorded; it must be closed before claiming a clean authored baseline or packaging readiness.
- `TODO-05A1-D1` 的源码、专项 Automation 与用户 PIE 已完成，Gemini 报告手动编译和 Editor readback 通过；但 Front/Backstab/Victim Montage、Notify 放置及 Launch/AnimBP/Blueprint 关系仍是用户 `Content/**` WIP，不构成干净 authored baseline 或 packaging 证明。关闭该 authored debt 的条件是形成可追溯资产基线和对应 readback，或提供 evidence-backed no-adoption。`REC-05A1-03` Native gate 已完成；`REC-05A1-03-MIG` 负责后续资产迁移与旧路径退役，随后才进入 `TODO-05A1-D2A/D2B/E`，各阶段仍需其对应的编译、focused Automation、Editor/readback 和 Scene01 PIE 门禁。
- Gemini 临时 Motion-Warp 诊断 helper 已撤回；当前 `MeleeMotionWarping.*` 与两个处决调用点相对基线无该 helper diff，普通近战 Motion-Warp 仍保留。后续若真实消费者触发重复诊断或维护成本，按条件性 `REC-03A7-01` 单独制定批准路径和验证矩阵，不把临时日志保留为长期公共 API。
- `REC-05A1-03` 的 Native 契约门已关闭，新武器 Montage 可以采用统一入口；旧 Notify/Tag 的退役仍属于 `REC-05A1-03-MIG`，必须等待完整资产清单、Editor 零引用 readback、两套处决 Automation、用户编译和 Scene01 PIE。MIG 关闭前不得宣称旧路径已删除或 authored baseline 已统一。
- `TODO-07B8-B/C` remain conditional RateWindow work. B cannot start from a guessed asset list; it requires a named Enemy Montage and a measured timing benefit. C cannot start from source symmetry alone; it requires evidence from at least one real Player and one real Enemy consumer, then a pilot proving the narrow helper preserves each consumer's distinct lifecycle. Neither stage authorizes a global Time Dilation or listener.

## Deferred TODOs

- Dedicated Sprint Loop presentation remains deferred until the selected locomotion set and MoveSpeed cadence are stable; validate foot sliding, Root Motion isolation, and transitions in focused PIE.

## Stage Completion Standard

A roadmap stage is complete only after its approved plan, focused source/asset validation, user-owned compilation/PIE evidence where applicable, review, documentation synchronization, and explicit commit approval are complete.
