# PolyQuest Roadmap

本文件只维护当前入口、开放阶段、依赖顺序和未关闭债务。项目介绍见 [README](README.md)，运行时事实见 [ARCHITECTURE](ARCHITECTURE.md)，实施白名单与验证矩阵见 [plan](plan.md)，历史结果见 [归档](ROADMAP-archive.md)。

## Current State

- 最近交付：`08f4214`（2026-09-13），TODO-03H9-A 统一 Montage RateWindow 托管，已完成实现、最终 Main 代码复核、归档和提交。
- 当前交接：[plan.md](plan.md) 第 6 节；固定版本为 `git show 08f4214:plan.md`。Development Editor 编译、专项 Automation 和 PIE 已有用户确认，各自验证时点及剩余项以原记录为准。
- **03H9-A 总验收仍开放**，仅待 `Debt-03H9-A-ValidationReceipts` 对账；提交不等于全部门禁通过，不重复安排已修复缺陷，也不自动启动后续实现。
- 已接受顺序：**03H9-A 验收对账 → 03H9-B → 03H9-C → 03C → 03C-B**。03C-A、03A7E、03A4B、03I3、07B14 和 REC 候选仍按各自条件启动，不自动加入前置。
- 既有 Content/Config WIP 与各阶段资产收据债保持独立；当前状态不构成干净检出可复现或打包就绪证明。

## Contents

- [近期开放阶段](#active-milestones)
- [存档、关卡与表现路线](#later-milestones)
- [条件性阶段与建议](#conditional-work)
- [风险与验证债务](#known-risks-and-validation-debt)

## Route Constraints

- 保留 GAS 唯一战斗权威和现有伤害路径；StateTree 管宏观状态与意图，Controller 管感知、目标与导航，Ability/Effect 管动作、消耗、打断与清理。除非真实生产需求证明 StateTree 不足且另行接受替代方案，不引入 Behavior Tree、Blackboard、第二状态机或全局动作分派器。
- Motion Warping 仅用于具备真实窗口和目标/距离需求的局部动作。Enemy 转向由 Montage/Motion Warping 表现，C++ 只提供有效目标和生命周期数据，不逐帧强转 Actor。Root Motion/CMC 保留位移、碰撞、地面与台阶/边缘权属；`bCanWalkOffLedges` 不是空气墙，真实腾空及阻挡后的表现需独立契约。
- Player 与远程敌人只共享适用的投射物运行时/数据契约。Bow 的输入和 Draw/Hold/Release 不作为通用远程模板；Staff/Mage 按 03I3 的武器、投递、选点和输入四个维度决策。
- 小怪/精英可按真实需要共享决策基础；Boss 的阶段、强制机制、无敌、招牌连段及清理由 StateTree/GAS 保持所有权。存档、奖励与多人不在战斗接入中顺带扩展。

## Active Milestones

### Montage Windows

- [ ] TODO-03H9: Reusable Montage RateWindow And CancelWindow
  - 目标：项目 GAS 动作采用标准 Montage 播放入口后，配置既有 RateWindow/CancelWindow Notify 即可消费窗口；新动作只声明取消策略，不再复制 Context、Begin/End 监听、实例状态和退出清理。Notify 不绕过 ASC 激活、消耗和阻止规则。
  - A/B/C 各自冻结 Source/Test 符号、直接依赖、资产 readback 和验证矩阵；A/B 试点通过不代表父阶段完成，C 必须完成适用面迁移与新动作配置验收。
  - 公共层不按具体 Ability 类名登记或分支；普通新动作不修改公共窗口代码。裸 Montage_Play、Sequencer、无 ASC 动画不自动接管。漏接标准入口或策略冲突须报出可定位错误，不得配置了 Notify 却静默无效。
  - 复用既有窗口算法和 Notify 类身份，不扩展跨项目插件、网络复制、全局时间膨胀、角色级第二状态机或无关动作。类迁移、Tag/Config/Build.cs 或资产变更须由后续计划明确批准路径和兼容步骤，禁止手改二进制。

- [ ] TODO-03H9-A: RateWindow Ownership And Standard Playback Entry
  - 实现/最终 Main 增量审查已完成；PlayActionMontage 托管 RateWindow，SprintAttack/EnemySmallHitReaction 两试点已迁移。冻结合同、真实 ASC/Montage 时间推进验证和完整矩阵见当前 [plan](plan.md)；固定交接见 `git show 08f4214:plan.md`。
  - 总验收只引用 `Debt-03H9-A-ValidationReceipts` 的剩余清单与关闭条件，不在此重复维护收据。

- [ ] TODO-03H9-B: CancelWindow Ownership And Explicit Cancellation Policy
  - 复用A的播放归属，独立封装取消窗口监听、来源/绑定校验、幂等Tag贡献和退出清理。统一入口可通过显式策略选择取消目标及现有动作门控；不再要求普通动作手写Begin/End和Add/RemoveLooseGameplayTag，不因两类窗口同名而强行合并其状态算法。
  - 规划时核对当前Dodge/Defense相关策略和消费者，保留现有权限：Big/Launch不得因封装额外获得Guard/Parry等取消；Charged暂停保留、Launch阶段门及已有防御恢复策略以窄策略适配保留。新动作配置须同时满足窗口授权、可被取消标记、目标激活及阻止Tag兼容；统一校验这些关系，禁止通过全局删除阻止Tag制造可用性。
  - GAS仍决定激活、消耗与实际打断，目标CanActivate/Commit失败不得提前结束原动作；成功后的动作替换沿用批准契约。只撤销本次绑定自身Tag贡献，外部贡献保留；重复/迟到/跨激活事件和自然/强制退出安全。重叠窗口语义在计划中依据现有资产/调用冻结，若改变可取消时机须显式裁决，不顺带引入任意动作转换矩阵。
  - 在普通Dodge取消和蓄力暂停等差异场景验证后由C完成余下迁移；真实ASC验证窗口内外、消耗拒绝/真实Commit失败、错误来源、重复Begin/End、旧播放事件、重新激活与外部Tag贡献，结合适用用户编译/PIE/readback和Main终审。

- [ ] TODO-03H9-C: Complete Adoption And Configuration-Only Acceptance
  - 对当前Player/Enemy玩法Montage播放入口及两类窗口消费者做有界采用清单，包含尚未监听RateWindow的动作，避免只迁移已支持者而留下同类缺口。逐项记录播放入口、Rate能力、取消策略及迁移/明确不适用理由；不得以缺少旧实现为不适用理由。收口前所有适用入口使用公共设施，普通新动作遵循同一模板；分批实施可行，未完成项不能隐入后续逐动作TODO。
  - 保留现有Notify名称/属性与Montage引用兼容，提供一份实际可用的新动作配置范例和检查入口。开发者选标准播放入口、声明取消策略；作者配置Montage窗口与数值即可。验证应定位Ability、Montage和缺失入口/策略冲突，区分故意禁用取消与配置错误；不增加每个具体Ability专属注册表或全项目Editor框架。
  - 核心验收：用未被公共层识别名称的新最小GAS动作（测试或示例），仅按标准入口和数据配置两类窗口，在真实ASC/动画推进下验证改速恢复、窗口外拒绝、窗口内成功取消、消耗失败保留动作与全退出清理；更换另一Montage后仍通过，不修改公共窗口代码、不增加专属监听。再验证漏接/冲突配置明确失败，并完成现有Player/Enemy受影响回归。
  - 用户Development Editor编译、Automation、实际Montage readback及代表性PIE，Main Fresh Review通过后才关闭03H9并更新ARCHITECTURE稳定事实；必要发布宏组合检查随新公共类型验证。既有03H7/03H8与H6债务按原关闭条件独立对账，不以迁移成功替代收据。本片结束后进入03C，新远程动作直接采用标准入口。

### Ranged Enemy

- [ ] TODO-03C: Ranged Enemy v1
  - 按已接受排期，在 03H9-A/B/C 窗口封装与采用验收后独立规划。已完成前置的历史不再展开；仍开放的 adoption、编译、Automation、Editor/PIE 债务按下方各自阻塞性和关闭条件处理，不自动豁免或扩大前置。FIX2 PIE 债务在远程敌人首次可玩时实测；未接受建议与 fixture 候选不自动延迟本片。
  - 规划建议：复用现有 Controller 的请求/pending/冷却所有权，保留远程局部距离/LOS 策略；有界扩展 EnemyAttackProfile 并兼容近战默认值，远程 DamageGE 仅取 ProjectileDefinition。接口、校验和资产路径由本片计划冻结，不构成当前实现授权。
  - Reuse the existing immutable 03B projectile runtime and GAS delivery path. Add only enemy ranged Profile/Ability/StateTree timing and an AI-owned target snapshot; enemy projectiles are explicitly authored straight/non-homing by default.
  - Do not create Player Lock-On, global Target Assist, a second projectile hierarchy, or a second damage path.
  - A ranged attack must have a valid current LOS at its decision/launch boundary; when LOS is absent, the target snapshot may support navigation or repositioning but may not authorize a projectile through cover. Keep this as a narrow `HoldRange -> Fire -> Reposition` behavior check, not a general perception rewrite.
  - This is a bounded behavior/evidence slice, not permission to implement a general combat-AI framework. Freeze only the compatibility boundary for a possible `CombatContext -> Filter -> Score -> Intent` evaluator; first validate the actual `HoldRange -> Fire -> Reposition` encounter in PIE before adding shared scoring or new tactical assets.

- [ ] TODO-03C-B: Enemy Awareness, Perception Memory And Hearing v1 (scheduled post-03C)
  - This stage is accepted as the next enemy-AI foundation after `TODO-03C`: Backstab creates a stealth/awareness expectation, and a player sprinting through an enemy's visual blind spot must produce a believable turn/alert response instead of indefinite ignorance or omniscience. The implementation remains bounded; it is not a full stealth game system.
  - Keep Sight as exact target confirmation. Add Controller-owned `LastKnownLocation`, last-seen time/expiry, and perception source; replace indefinite distance-only retention with a bounded memory/search window. Add a minimal close-range/noise response that can turn toward an approximate threat location, then add `UAISense_Hearing` stimuli as an approximate location entering `Alert/Investigate`, never as a direct precise Player target.
  - Preserve the existing StateTree macro states and GAS ownership. The same perception-memory substrate may serve melee/ranged minions and elite variants; Boss omniscience or phase exceptions must be explicit authored rules, not an accidental fallback. Do not make a new awareness tag a hidden Backstab activation prerequisite in this stage; any explicit unaware/alerted Backstab rule requires its own accepted contract.
  - Success requires focused Sight/Hearing/memory Automation, user compile, applicable Editor readback, and PIE evidence for blind-spot approach/turn, occluded pursuit, investigation, reacquisition, timeout, leash return, and ranged fire LOS. Do not introduce a Behavior Tree, replacement HFSM, global sensing service, or second target authority.

## Later Milestones

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
  - Audit Save IDs, transaction ownership, reload/re-entry, rewards, encounter cleanup, fog-gate state, and actor/subsystem/authored-data boundaries. The former pre-03C punish prerequisite refers to the delivered TODO-05A/05B/05A1 execution slices (see [historical records](ROADMAP-archive.md)); their separate receipt debts remain below. This review fixes only confirmed data-integrity/lifecycle blockers.

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
  - 只处理表现重调与演示打磨，不新增战斗权属；post-E 表现基线的未完成验证仍归下方对应债务。
  - 最终 Montage 组合和 Notify 时序在动画集稳定后作者化，复用既有速率生命周期与已接受的 03H9 标准入口，不增加并行改速系统或用 Motion Warping 处理 locomotion。

## Conditional Work

以下阶段保留各自触发条件；列入本节不构成实施或资产授权。

### Optional Gameplay And AI

- [ ] TODO-03I3: Player Ranged Targeting And Delivery Contract v1 (conditional)
  - Start only when a concrete Staff/Mage player ability, authored asset route, and player-facing targeting question are accepted. This is a contract/design gate, not permission to invent a Mage implementation or to turn the current Bow path into a universal template.
  - Keep four dimensions explicit and orthogonal: weapon/equipment family (Melee, Bow, Staff, OffHand), attack delivery (contact trace, actor projectile, ground/area effect, beam/line effect), target-selection mode (locked actor, fire-time snapshot with finite tracking, ground point plus authored area shape/extent, route/line, or free direction), and physical input intent (`Primary`, `Aim`, `Guard`, `Parry`, prepared slots).
  - Define the smallest selected Staff/Mage slice against the existing GAS ownership: an actor-target projectile may reuse the immutable projectile runtime and finite fire-time snapshot only where its payload and lifecycle match; a ground/area ability must own a world-space point and shape/extent snapshot, while a beam/line ability owns a world-space direction or segment snapshot. Neither may depend on Lock-On or Bow Target Assist by analogy.
  - Preserve ability-owned snapshots, explicit world/ASC/target validity, cancellation/death/destroy cleanup, and one existing damage GameplayEffect path. No continuous target following, automatic retargeting, global targeting service, or hidden cross-weapon state is introduced.
  - Success requires a reviewed delivery/target matrix for the selected route, an explicit reuse-versus-new-contract decision, focused Automation for snapshot and teardown boundaries, user-owned compile, curated Editor readback of the named assets, and Scene01 PIE for the actual targeting interaction. Unselected modes remain documented boundaries rather than speculative code.
  - No generic Melee/Ranged enum solely for naming, no Bow Draw/Hold/Release merge by analogy, no new physical input action, no broad Ability dispatcher/framework, no Motion-Warp redesign, no second projectile hierarchy, and no damage/trace ownership change.

- [ ] TODO-07B14: Player Dodge Motion-Warp Distance And Asymmetric Air-Reaction Protection v1 (conditional)
  - Activate only when either a real Dodge Montage Warp Window plus a distance-control need is confirmed, or repeated Player/Enemy airborne/knockdown interactions demonstrate a missing protection or air-follow-up policy. The completed `TODO-07B11-RET` is not its runtime prerequisite. The two slices below have independent acceptance gates; one does not authorize the other.
  - **Slice A — Dodge distance**: evaluate local Motion Warping for the Root Motion Dodge Montage. C++ may derive and validate an input-relative target/distance, install and clear it through the existing Player-owned component, and let Montage/CMC consume it. No direct teleport, forced per-frame rotation, or promise that warping bypasses capsule collision, walls, slopes, or ledges. Missing Warp Window or invalid target is a valid no-adoption result.
  - **Slice B — asymmetric air reaction**: define a GAS-owned policy in which authored Enemy airborne states may accept repeated, unbounded-by-system-count follow-up hits whenever their reaction/Poise lifecycle permits; there is no global juggle-count cap, while per-contact dedupe and normal death/teardown guards remain mandatory. For the Player, protection is an Ability-owned lease rather than a manually timed sub-window: once the Player Launch Reaction launch Montage is confirmed active, the lease covers the full Player Launch Reaction Ability (grounded/downed/recovery as separately authored) and rejects ordinary re-hit/re-launch/ground-pound damage or reaction. No separate invulnerability Notify window is required; the lease is released only by natural montage/recovery completion or any `EndAbility()` cleanup path. If Tech-Roll is adopted, its handoff must retain or transfer protection without a gap; authorized execution/finisher exceptions remain valid. The dormant Legacy Launch branch is not a prerequisite or fallback for this policy.
  - Audit the existing `State.Status.Invulnerable` path and execution authorization before changing normal hit delivery; explicitly preserve authorized execution/finisher exceptions and do not solve the policy with collision-layer hacks or a parallel action state machine.
  - Required gates: focused Dodge warp-target/interruption/ledge Automation; Player protection-lease activation-to-`EndAbility()` and Enemy repeated-air-follow-up Automation; user Development Editor compile; curated Dodge/Reaction/GameplayEffect/Montage readback; Player and Enemy PIE combat, recovery, and teardown coverage; and Main Fresh Review.
  - Non-goals: no blanket Player immunity outside an active Player Launch Reaction Ability, no system-wide projectile/area-damage rewrite or duplicate same-contact delivery, no claim that all Soulslike games share identical invulnerability timing, no global Motion-Warp component, network/prediction work, or binary asset editing.

- [ ] TODO-03C-A: Enemy Combat Intent Utility v1 (evidence-gated, post-03C)
  - Activate only when the 03C encounter or a second real melee/ranged consumer demonstrates that weighted selection cannot express a required tactical choice, repeated-action problem, or no-valid-action fallback; otherwise retain the current narrow selection path.
  - Keep `UEnemyAttackProfile` as execution payload and place any context-dependent selection metadata on the owning attack-set entry or equivalent existing set boundary. Evaluate on decision events (combat entry, action completion, cooldown/LOS/distance change, or navigation failure), never as a per-frame second state machine.
  - Share the substrate across melee/ranged minions and elite variants through authored candidates and modifiers. Bosses may use it only for phase-local routine choices; StateTree/GAS retain phase transitions, invulnerability/forced mechanics, signature combos, damage, interruption, and cleanup.
  - Success requires focused filter/score/intent Automation plus user compile, curated Editor readback, and applicable melee/ranged PIE evidence. Do not introduce a Behavior Tree, Blackboard, second HFSM/StateTree, global action dispatcher, or second damage path.

- [ ] TODO-03A7E: Enemy Melee Motion-Warp Adoption v1 (post-ranged, optional)
  - Revisit `UEnemyMeleeAbility` only after the first ranged-enemy gate and a concrete enemy authored need. Keep AI/Controller/StateTree intent separate from the Ability-owned one-shot target snapshot and Root Motion lifecycle.
  - Reuse the Player evaluator only if the geometry and ownership contract is demonstrably identical; otherwise define a bounded Enemy-specific evaluator. No Player component sharing, continuous chase, AI retarget loop, or change to Enemy Trace/Resolver/Damage ownership.
  - This stage is not a prerequisite for `TODO-03C`; it requires its own authored Montage/Notify/Root Motion, Automation, compile/readback, PIE, and teardown evidence.

- [ ] TODO-03A4B: Weapon Defense Outcome Profiles v1 (conditional)
  - Before intentionally authoring a weapon with partial Guard damage, define one explicit no-defense/full-absorb/bounded-partial outcome while keeping Parry distinct and the existing Guard/Resolver/Damage GE ownership.
  - It is not a prerequisite for the current player-combat sequence while all authored Guards remain full absorb.

### Recommendations

- The delivered `TODO-07A2-A` stays intentionally icon-free: preserve stable grey-and-white numbered slot boxes (`1-4`), a grey cooldown mask, and a white sweep indicator. Do not turn later icons into an asset migration or a new skill-presentation data model without a separately accepted slice.
- Do not promote a full inventory before the `TODO-03D` save/ownership boundary and the `TODO-03F` reward contract are stable. `TODO-03D1` remains focused rest-site preparation and persistence, not a backpack, crafting, or mid-combat loadout system; `TODO-03E` remains the single-consumable route after its stated ownership and cancellation prerequisites.
- Do not add a standalone generic health review after the HUD closeout. Reserve `TODO-04B` and `TODO-06C` for their named persistence/encounter and integrated-route health questions.
- Keep CommonUI migration conditional on a real gamepad/focus-stack/pause-menu requirement. The delivered HUD remains on the existing UMG and `APolyQuestPlayerController` ownership route.
- Keep the current fixed 15% Lock-On retention and Bow 6% Target Assist values stable; the delivered `TODO-02B4` obstruction LOS/grace policy remains a separate Lock-On contract and must not be folded into screen-space hysteresis.

- Treat a missing or unsuitable Motion-Warp asset as a valid evidence-backed no-adoption outcome.

- For weapon authoring reuse after TODO-03I1, keep `UWeaponDefinition` direct fields and `BaseGrantedActions` as the runtime source of truth. With the current small asset set, duplicate a validated weapon/template asset first; consider a bounded C++/Blueprint family-default stage only when multiple new weapon families repeat the same route configuration or manual drift becomes a real cost. Any defaults must remain opt-in, preserve exact direct-route and OffHand fail-closed validation, and require Editor readback; do not add global defaults or an automatic asset migration by analogy.
- Combat Feedback authoring now uses the delivered TODO-07B7 taxonomy: an abstract Base profile for shared Overlay and typed Player/Enemy profiles for role-exclusive data. Preserve character-owned runtime lifecycles and do not add further per-enemy subclasses unless a later enemy has a genuinely different feedback contract.

- `REC-05A1-02`: Contextual Animation Compatibility Evaluation (conditional recommendation). Activate only after two or more real paired execution assets or materially different enemy body types demonstrate that Motion Warping plus Notify timing cannot provide stable spatial alignment, and the team explicitly accepts enabling UE 5.8's experimental Contextual Animation plugin, module dependencies, and authored assets. The first slice is a compatibility spike: verify plugin/module availability, one known-good paired asset and readback, one play/cancel/destroy path, and Scene01 PIE. Contextual Animation may own presentation-time sync points and spatial alignment only; `UExecutionLockContext` and GAS continue to own request validation, locks, hit authorization, Release, death/nonlethal outcomes, and cleanup. Do not add a generic `AbilityTask_PlayContextualAnimAndWait`, production plugin dependency, asset migration, or Boss grab/throw contract until the spike passes. Record evidence-backed no-adoption if the experimental plugin, module/build cost, asset workflow, or runtime stability is unsuitable; continue the existing Motion Warping/Notify route.

- `REC-03A7-01`: Motion-Warp Diagnostics And Reuse Cleanup v1（条件性推荐）。仅在至少两个真实普通近战消费者（当前图谱显示 Light、Charged、Sprint、Melee Skill）出现重复诊断分支、失败原因分类不一致，或出现可量化的跨 Ability 排障成本后启动。阶段只统一开发构建下的诊断结果/日志格式和窄的失败原因分类；不得合并各 Ability 的目标快照、接地/距离/角度判定、Motion-Warp 生命周期、清理或伤害所有权，不得把诊断接口变成全局运行时服务。需要覆盖真实消费者的 focused Automation、手动编译、适用的 Editor readback 与 Scene01 PIE，并证明返回值、目标选择和普通攻击表现不变。该阶段默认排在 D2A/D2B/D2D/E 之后，且不成为 `TODO-03C` 或任何处决阶段的硬前置。 FULL-AUDIT补充CAND-WARP-DUP-01的Charged/Sprint/Skill各154行重复证据（git show 851c7ac:plan.md 第4.3节）；仅诊断范围可在本条既定边界内处理，整体同步流程合并尚未接受，不能借此放宽本条对快照/判定/生命周期的限制；若确需改变该边界，须先独立接受具体契约和白名单。

- `REC-COMBAT-FEEDBACK-FOV-PUNCH-DATA-ASSET`: 战斗镜头 FOV Punch 打击顿挫幅度的数据驱动化（条件性推荐）。当前快门式微顿挫幅度（Small 0.0° / Big·Launch 1.5° / Execution 2.0° / Parry 1.8°）由 C++ 按照受击分类与动作叶子节点硬编码分发，平 A 保持清脆连贯无镜头伸缩疲劳，重击/击飞/弹刀/处决提供高反差的光学快门打击感。当未来武器系统引入轻/重型武器专属手感差异（例如轻短匕首微顿挫、重型巨剑强顿挫、重锤强冲击）且实测产生差异化镜头打击需求时，可在 `FPlayerCombatFeedbackTierSettings` 或武器定义（`UMeleeWeaponDefinition`）中暴露 `FovPunchDegrees` 参数进行数据驱动微调，以现有 C++ 硬编码数值作为默认回退值（Fallback Defaults）。当前阶段不提前设计多层武器资产配置或扩大数据资产依赖。

- Boss Grab/Throw 尚未接受；现有执行会话的 token、成对锁、授权命中与清理可作经验参考，但不宣称现有 Front/Backstab/Victim 能力直接支持投技。通用处决基类仅在测得真实重复且另行接受合同后考虑。Unarmed 不是隐式 Player Montage 回退，Small/Big 仍是 Victim 表现选择，不引入新的结果选择器。

### Deferred Presentation

- Dedicated Sprint Loop presentation remains deferred until the selected locomotion set and MoveSpeed cadence are stable; validate foot sliding, Root Motion isolation, and transitions in focused PIE.
- `REC-CAMERA-SEE-THROUGH-COMBAT-EXPANSION`: 战斗锁定/近身交火开孔半径自适应放大（条件性推荐）。在未来引入大体型 Boss 或多目标近战且实测出现明显视线遮挡盲区时，才按需在锁定状态下平滑扩展 `MaxTunnelRadius`（例如 280cm -> 350cm）；当前单兵近战原型阶段不提前引入全局战斗状态管理器或过度复杂化开孔数学。
- `REC-ENEMY-AMBUSH-XRAY-GATE`: 敌人遮挡红色轮廓的常态/战斗感知门控（条件性推荐）。为保护伏击（Ambush）与转角杀悬念，非战斗/未察觉状态下敌人的 `bRenderCustomDepth` 应保持为 `false`，杜绝隔墙透视剧透；仅在敌人感知到玩家或受到伤害进入战斗状态（Alert/Combat）后动态激活 `SetRenderCustomDepth(true)`。当项目未来建立统一的常态与战斗状态仲裁机制时再予接入。

## Known Risks And Validation Debt

本节是开放风险和证据缺口的唯一维护位置。条目沿用各阶段记录及其证据时点；本次文档整理不重新审计源码、不核销收据，也不把“未报告”判定为测试失败。旧 fixture/资产描述在相关维护时按真实状态对账，不能据此推断新阶段尚未实现或自动新增修复。

- **Debt-03H9-A-ValidationReceipts（A阶段剩余验证收据）**：Development Editor编译已由用户明确确认，不再列缺失。尚未单独报告：①非Editor Win64 Development构建；②本片相关的PolyQuest.Combat.EnemyStanceBreakRateWindow、PolyQuest.Combat.HitReaction、PolyQuest.Combat.PlayerBigHitReactionWindows、PolyQuest.Player.ActionWindows、PolyQuest.Combat.PlayerMeleeMotionWarping五项回归；③Sprint/EnemySmall实际GA→Montage引用、Small四向选择、Skeleton/Slot、Notify类型/数值/区间及派发模式readback。已报告通过的Player六组、Enemy及Managed保留各自时点，最终授权修复后再次确认的是Managed；不将较早报告写成最终版本全量重跑。关闭触发：用户补充上述适用结果，或明确接受有边界的验收调整并记录其理由。此项维持03H9-A总验收标记开放，不否认已完成的实现/代码复核，也不自动生成修复、重跑或资产修改任务；未确认项不默认为失败。不得据此宣称全门禁、干净资产基线或打包就绪；03H7/H6旧债务独立保留。

- **Debt-03H7-PostRepairValidation（非阻塞PIE收据债）**：03H8验收时用户已确认最终代码Development Editor编译、BigHitReactionWindows、PlayerMontageRateWindow六组、HitReaction及PlayerLaunchReactionRootMotion通过，当前编译/Automation收据已补齐。仅余最终生产回调修复后的Scene01四向取消、连续受击/立即重激活、自然结束/死亡/Falling实测收据；更早PIE不追认为最终版重跑。关闭触发：用户提供上述修复后PIE确认；在此之前不得宣称最终版全门禁或干净可发布基线。资产未改，既有readback仍有效；不阻塞03H8测试修复收尾或03C独立规划，既有H6债务不随本项关闭。

- **Debt-03H6-FIX1-PreFixProof（非阻塞证据债）**：H6-F01已完成共享实例授权与当前回归验收，但原计划要求的修复前真实Melee RED未实际运行。用户在知悉该边界后完成PIE并授权收尾提交；不宣称具有历史RED→GREEN证据，也不宣称生产资产自然触发。关闭触发：后续修改该实例授权规则、出现同类速率回归，或明确需要证明旧实现可复现时，在隔离副本使用修复前生产实现与完整测试，证明旧实例未停止、新旧ID不同、Context有效及两层bypass关闭后，Begin/End/Clear断言失败，再记录当前修复通过。不得回滚含WIP的工作树；这只能补充对照验证，不能改写原执行时序。

- **Debt-03H6-FIX2-PIE**：用户于2026-09-13因远程敌人尚未实现而接受FIX2本片PIE豁免；这是验收例外，不是PIE通过。后续在TODO-03C首次具备可玩的远程敌人时，验证射手移动/目标转向下的投射物Guard、受击/死亡方向和尾迹终止，并补Bow及近战Guard/GuardBreak/Parry/前处决/背刺相关回归；以实际PIE回执关闭。FIX2的Automation与Main终审已完成；该债务只保留未执行的实测边界，与Debt-03H6-ProjectileIntegration的Native集成矩阵分开记账。

- **H6-F04 / P1 — FIX-BuildGuard**：Front/Backstab的生产几何成员声明在开发测试宏内、调用/定义在宏外（git show 851c7ac:plan.md 第4.2节），Shipping/Test默认宏0构建将缺声明；静态确定，未实际编译。关闭：正式private生产声明+宏内测试薄入口，Development与Shipping/Test宏组合编译及几何Automation；不得强开测试规避。发布构建/打包前必须关闭，不自动改变既定FIX1→FIX2顺序或阻塞03C Development。

- **H6-F03 / P2 — FIX-ParryWindow**：重复Begin后单bool清理只撤销一次，ASC ParryActive计数残留；不是永久弹反证明，资产重叠未知。关闭：固定幂等/重叠身份语义，重复Begin/取消/重激活/外部tag贡献测试及适用Notify证据；非自动03C前置。

- **H6-F05 / P2 — FIX-TargetOrder**：PlayerLockOn与Projectile assist的近似相等排序均有比较环，独立标量反例见git show 851c7ac:plan.md 第4.2节。关闭：两个消费者严格弱序、确定tie-break、传递性/多候选回归；保留不同单位/优先级。未运行UE测试，具体03C采用路径由其计划核对，不自动扩大前置。

- **H6-F06 / P2 — FIX-ExhaustionTimer**：合法最短力竭时间0不排SetTimer回调，恢复后elapsed门槛仍不满足。关闭：明确0的立即/next-tick语义，保留动作结束与外部贡献，0/正值/恢复/销毁测试和适用PIE；非自动03C前置。

- **H6-F07 / P2 — FIX-InputTeardown**：没有Release的UnPossess/repossess保留HeldCombatInputStartTimes，新Started被拒。关闭：清除旧物理输入/长按/重试资格，断连与重持有测试，保留有意不取消Guard的既有契约；不要求CancelAll，非自动03C前置。

- **H6-F08 / P2 — FIX-TeamDispatch**：直接调用GetCombatTeamTag_Implementation绕过BP覆写，使Projectile/LockOn与近战分派不同；当前资产覆写未知。关闭：统一Execute入口，原生与BP覆写测试/readback；首次采用BP动态阵营前必须关闭，当前不自动扩大03C资产范围。

- **H6-F09 / P2 — FIX-FloorVisibility**：Volume先销毁而managed actors继续存活时，EndPlay只恢复MPC而保留其hidden写入。关闭：撤销本Volume隐藏贡献并保留初始外部hidden，销毁/重复清理测试及场景PIE；非03C前置。相关FLOOR-HIDE候选先修复后扣重。

- **H6-F10 / P2 — FIX-UIFlash**：两个血条widget在合法短duration<=.03到期仍写peak颜色，之后不再恢复。关闭：两个消费者到期恢复base，短/普通duration、大delta、重复受击测试与视觉证据；仅静态/标量证据，非03C前置。UI-CURVES候选先修复后扣重。

- **H6-F11 / P2 — FIX-TestTiming**：VitalHUD一次.60 tick的buffer断言忽略生产delay分支return，静态矛盾。关闭：分步证明delay和后续收敛并通过suite；不删断言或擅改生产余时契约，非03C前置；没有本轮实际失败日志。

- **H6-F12 / P2 — FIX-TestCancellation**：SkillBar测试grant后才改CDO，已创建实例仍自动结束，随后Cancel未覆盖Active取消。关闭：正确配置实例/授权前CDO、断言Active后再取消，保留冷却断言并运行suite；非生产回归/非自动03C前置。

- **H6-F13 / P2 — FIX-TestAI**：EnemyCombatSpacingTests202–676自建模拟状态机，生产Controller/Task/Ability改坏也不失败；1–200真实Profile/几何保留。关闭：真实入口+可控时间/导航覆盖同场景并证明生产回归会失败；不只删模拟用例，不再以旧段作为03C接缝证明。

- **H6-F14 / P2 — FIX-TestExecution**：VictimPresentation872–929缺LockedTarget，Ready失效两用例在抵达hook前就失败，inactive断言假阳性；第13节真实Montage测试仍有效。关闭：先可激活控制样本，再单变量失效并证明入口到达/锁清理，补审git show 851c7ac:plan.md 第4.2节列出的Front/Backstab换装和ExecutionLockIn弱前置，运行相应suite；独立测试片，不自动阻塞03C。

- **Debt-03H6-ProjectileIntegration**：已有resolver级允许Guard、禁用Parry及Dead来源测试不是完整飞行集成证明。方向相关Native覆盖已由FIX2关闭；剩余发射/飞行到Dead、Ragdoll、Destroy、SourceASC失效和重复碰撞终止的集成矩阵由03C收口，进入该阶段前不要求实现尚不存在的远程敌人。保留现有拒绝结算策略，不虚构死后伤害支持。

- **Debt-03H6-TEST-SHRINK-Baseline**：除Charged专项外，其余六项迁移前成功收据未提供；用户知悉后接受按源码等价比对及实施后七项Success收口。保留历史证据边界，不追认原先基线门已执行，非03C前置。关闭触发：取得当时可追溯成功收据，或后续相关回归归因确有需要时在受控环境完成a19630d对照并记录其不能补证历史执行顺序的限制；不为本次收口重跑未变代码。

- **REC-03H6-AdditionalShrink**：git show 851c7ac:plan.md 第4.3节除已接受A/B、既有REC-03A7-01的Warp候选、下列RET闭包外的建议统一导航入口。CAND-TEST-WORLDTICK-01的已接受五消费者范围已由TODO-03H6-TEST-SHRINK完成；其余World Tick差异消费者与其他候选不自动追加实施、不阻塞03C。关闭/启动触发：对应模块下次有界维护或用户明确接受时冻结具体ID/白名单/验证；不用全局清理替代。FLOOR-HIDE需先关闭F09，UI-CURVES先关闭F10并扣重；测试fixture保留真实前置，先处理相关F12/F14；Config必须另批路径。未接受建议保持本条指向逐项裁决，不伪装已实现。FIX2专项通过时仍有World Cleanup缺EndPlay日志；首次维护该测试world fixture时补EndPlay顺序及清理回归，作为非阻塞验证维护项，不把本次Success当清理证明。Guard人工活跃fixture的恢复延迟/GuardBreak提示仅说明资产激活未覆盖；采用完整能力fixture时再验这些路径，不要求本片增添资产。

- **Debt-03H6-LaunchRetirementReadback**：CAND-LAUNCH-SMOOTH-01与REACTION-LEGACY-01为同一RET-LaunchClosure，九文件874物理行不是净收益。关闭触发：独立退役计划接受后，取得旧Notify/GameplayTag/Blueprint引用readback，迁移仍有效的MotionWarping1667–1747 Guard fixture、HitReaction旧速度与双方Launch测试消费者，再证明闭包无用并编译/回归。资产WIP不是零消费者证据；当前有条件延期，不阻塞03C，不重复计父子收益。

- `D2A-VERTICAL-SURFACE` remains a non-blocking execution-presentation risk: the Snap Helper intentionally uses `PlayerLocation.Z`, which fixes the observed player/target height mismatch that otherwise made Backstab sink into the floor and fail collision, but it does not perform Ground Trace or solve slopes, steps, or larger vertical offsets. Close only through an independently accepted Ground Alignment/slope-validation stage; until then do not claim all-terrain alignment.

- `TODO-05A1-D2A` has user-confirmed focused Automation and Scene01 PIE plus Gemini-reported Rider error-level cleanliness and Main static review, but no separately archived manual `PolyQuestEditor (Development Editor)` compile or execution Weapon/GA/Montage readback receipt. Keep the source slice closed while treating authored/compile evidence as non-blocking debt; close with the named compile/readback receipt or an evidence-backed no-adoption decision before a clean authored-baseline or packaging claim.

- `TODO-05A1-D2B` has user-confirmed Front/Backstab Focused Automation and Scene01 PIE plus Main Fresh Review with no P0-P2 blocker. Gemini's report lists manual compile, Rider, and asset readback results, but no separate Main/user receipt is archived here; keep the weapon Montage authored baseline and compile/readback evidence as non-blocking debt until a named receipt or evidence-backed no-adoption decision exists.

- `TODO-05A1-D2C` has user-confirmed Backstab/Front/ExecutionVictimPresentation Automation and Scene01 PIE plus a final Main Fresh Review with no P0-P2 blocker. Gemini's report lists manual compile, Rider, and asset-readback results, but no separate Main/user receipt is archived here; keep D2C authored/compile readback as non-blocking debt until a named receipt or evidence-backed no-adoption decision exists.

- `Debt-05A1-D2C-OwnershipMatrix` remains a non-blocking test-evidence debt: the accepted `S=1,C=1` path uses a real StanceBreak activation, while malformed contribution-count rows are explicitly synthetic fail-closed boundaries. Close only with a real multi-contribution ownership fixture or an evidence-backed no-adoption decision under a separately accepted test scope.

- `TODO-05A1-D2D` has user-confirmed Focused Automation and Scene01 PIE plus Main Fresh Review with no P0-P2 blocker. Gemini's report lists manual `PolyQuestEditor` compile, Rider, and real weapon readback, but no separate Main/user receipt is archived here; keep D2D compile/readback and authored-baseline evidence as non-blocking debt until a named receipt or evidence-backed no-adoption decision exists.

- `TODO-05A1-E` has user-confirmed `PolyQuest.Combat.ExecutionImpactFeedback` Automation and Scene01 PIE plus Main Fresh Review with no P0-P2 blocker. Gemini's report lists manual `PolyQuestEditor` compile, Rider zero errors, and the two feedback DataAsset readbacks, but no separate Main/user receipt is archived here; keep E compile/readback and authored feedback assets as non-blocking debt until a named receipt or evidence-backed no-adoption decision exists. The closure trigger is a traceable user-owned compile/readback receipt; this debt does not block the source/Automation/PIE closeout or the `TODO-03C` route.

- `Debt-07A2-A-AuthoredReadback` is non-blocking: `TODO-07A2-A` has user-confirmed focused Automation and Scene01 PIE, plus Main Rider/error-level and bounded-review evidence, but no separately recorded manual `PolyQuestEditor (Development Editor)` compile or direct user-owned Widget/material readback. The approved manifest names `/Game/_UI/HUD/Skills/WBP_PlayerSkillBarHUD`, `/Game/_UI/HUD/Skills/WBP_PlayerSkillSlot`, and `/Game/_UI/HUD/Skills/Materials/M_UI_SkillCooldownSweep`, while the executor report named the two Widgets under `/Game/_UI/HUD/Vitals`; the actual package paths, Widget parents/bindings, `CooldownPercent` material parameter, `SkillBarHUDClass`, and shared cooldown-tag intent must be reconciled by a traceable Editor readback. This debt does not block the source/Automation/PIE closeout or `TODO-03C`, but blocks a clean authored-baseline or packaging claim.

- `Debt-07B10-CompileReadback` is non-blocking: the user-confirmed evidence covers `PolyQuest.Combat.ChargedAttackNiagaraFeedback` Automation and Scene01 PIE, while this closeout has no separate user-owned `PolyQuestEditor (Development Editor)` compile or exact Niagara/Charged GA/Montage attachment readback receipt. Close it with a traceable compile/readback receipt or an evidence-backed no-adoption decision; it does not block the Source/Automation/PIE closeout or `TODO-03C`, but it blocks a clean authored baseline or packaging claim.

- `Debt-07B12-AuthoredReadback` remains non-blocking: the user confirmed `PolyQuestEditor (Development Editor)` compilation, the five focused facing/lifecycle Automation suites, and Scene01 PIE, but this closeout has no separate user-owned receipt for exact `State.Block.Facing` Tag registration and the four Enemy Ability CDO/Montage settings. Close it with a traceable Editor readback or an evidence-backed no-adoption decision; it does not block the source/compile/Automation/PIE closeout or TODO-07B13, but it blocks a clean authored-baseline or packaging claim.

- The `b69dabe` post-E presentation baseline has no single accepted-stage compile/readback/PIE receipt. `APlayerCharacter::Tick()` now includes camera occlusion work, multiple HUD widgets carry timer-driven presentation updates, and `CameraBoom->bDoCollisionTest = false` changes the camera collision contract. Close this debt with a named Development Editor compile, curated material/Widget/Camera readback, Scene01 PIE coverage for death/UnPossess/respawn/map transition, and a focused performance/lifecycle check; do not infer closure from the commit history or graph output.

- Authored GA/GE/Montage/AnimBP/Blueprint/input/DataAsset/map fixtures remain local WIP; source/config commits are not clean-checkout reproductions. Close only with an explicitly curated asset baseline and readback.

- REC-05A1-03 Native and MIG gates are complete for the user-confirmed compile, Editor readback/Reference Viewer, seven focused Automation suites, Scene01 PIE, and Main Fresh Review. The current authored Montage and its dependencies remain user-owned WIP rather than a clean-checkout fixture; close that baseline debt only through an explicitly curated dependency closure, LFS verification, and a separately approved staging decision.

- `Debt-REC-05A1-03-RET-Teardown` remains open as a non-blocking validation debt: RET has user-confirmed compile, canonical Montage readback, seven execution Automation suites, and normal Front/Backstab PIE, but no forced interruption/destruction/UnPossess/Teardown receipt. Close with a traceable user-owned lifecycle receipt or an explicitly accepted focused fixture/no-adoption decision; this debt does not invalidate the RET retirement commit or archive closeout.

- Cost or state may be committed before rare Montage startup failure is known for Light, Charged, Dodge, Sprint Attack, or Parry. Close with a controlled post-commit playback-failure matrix before networking or frame-exact cost claims.

- Keyboard/mouse is the accepted input evidence; controller mappings and hardware behavior are not support claims until readback plus focused hardware validation exists.

- Player terminal Ability cancellation on death remains owned by TODO-03D; current Dead tags block new activation but do not promise cancellation of an already-active Bow or other Ability.

- The legacy BladeTraceBase/BladeTraceTip fallback remains for the current enemy fixture. Remove only after all enemy fixtures migrate to StaticMeleeWeaponDefinition and Reference Viewer proves zero references.

- Bow's fixed 6% no-lock Target Assist overscan is deliberate current tuning. A second projectile family requiring different bounds must introduce a finite validated definition field rather than a global targeting setting.

- **Directional Small/Big 历史条目对账**：旧“方向扩展仍延期”措辞与归档 `TODO-02C3L: Four-Directional Small/Big Hit Reactions v1` 的已实现记录冲突，不作为重新实施四向选择器的依据。原条目的兼容资产与 front/back/left/right PIE 证据范围未明确区分历史已验项和剩余项；相关资产下次验收时按 C3L 记录和当前资产核对，明确适用范围与收据后关闭。本文档整理不核销资产验证债，也不把当前四向玩法写成未实现。

- **Enemy Launch abnormal-Poise recovery（历史 C3H 取舍）**：只有聚焦试玩证实异常结束后的恢复丢失预期破韧反馈或手感不佳时，才接受独立 Enemy Launch/Poise 调整；验证中断、恢复、死亡和 exactly-once 破韧派发的 Automation/PIE。原取舍见归档 C3H 记录，不按文档维护自动改写当前 Launch。
- **Parry/Trace combined exactly-once（历史 C3M P3）**：resolver Parry 反馈和 DeliveredTargets 分别覆盖两端，不构成组合证明。以真实 UAbilityTask_MeleeTraceWindow 重复采样命中活跃 Parry，断言一次反馈派发后关闭；不增加第二个全局去重所有者。

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

- TODO-03H5 Review-Only audit completed at `a3e0f78` with no P0-P2 blocker and no Source change. `ReadyForActivation()` re-entry, Player/Enemy invalidation, the UnPossess cancellation asymmetry, and the unique Trace/Resolver/Damage ownership were checked from static source evidence; the latter asymmetry remains a design decision for `TODO-03I1/03I2`, not an H5 defect. The 3.17 exact-stop P3 and authored readback/manual-compile/cross-frame Task evidence debts remain open with their existing closure triggers.

- TODO-03I1 establishes direct MainHand Primary/Sprint routes, while TODO-03I4 removes the former `AssociatedLoadout`/`ActiveCombatLoadout` compatibility surface after the user-confirmed zero-reference asset precondition. User-confirmed affected Automation and Scene01 PIE are recorded; no separate manual `PolyQuestEditor` compile receipt is archived for I4. Close that evidence gap when an independent compile receipt is needed; it does not block TODO-07B7.

- TODO-03I2 source closeout establishes the orthogonal `Ability.Action.CancelableBy.*` and `Ability.Action.Teardown.OnUnpossess` selectors and removes the legacy `Ability.Skill.Melee` registration. User-confirmed Automation and Scene01 PIE passed, but `Debt-03I2-DodgeSkillCancelUnitTest` remains open because headless Automation lacks a real AnimInstance/Montage cancel-window fixture; close it with a dedicated integration fixture that activates Dodge and cancels a tagged skill end to end. No independent manual `PolyQuestEditor` compile or Main Reference Viewer receipt is archived for I2.

- `Debt-07B5-RealASCRetriggerE2E` remains open for Player and cross-frame completion of the Small Hit Reaction chain. TODO-07B8-B now proves Enemy HandleGameplayEvent -> old EndAbility -> new ActivateAbility with real same-direction/different-direction Montage instances, task/context cleanup and fresh baseline in one synchronous test run; it does not cover Player or animation advancement across frames. Close the remaining debt with those paths in a valid AnimInstance/Montage fixture or dedicated recorded PIE scene. Historical 07B5 independent compile/readback receipt limits remain unchanged.

- `Debt-07B8-AuthoredValidation` remains non-blocking for source closeout and TODO-03H6: A/B/C do not establish a clean-checkout authored baseline. A still lacks its independently archived manual PolyQuestEditor compile and GA_EnemyStanceBreak/AM_StanceBreak readback receipts. B has user-confirmed repaired PIE/Automation, but no separately archived build log or five-consumer Montage/window/Tick Type readback. C has Codex build logs, the 12-test integration run, user-confirmed PIE, independent review and the post-P2 Light/Motion-Warping regression; its actual six-Player GA/Montage references, window positions/rates/Tick Type and representative Enemy readback were not supplied as an itemized receipt. Close each receipt gap with traceable user-owned Editor evidence and the corresponding build result before claiming a clean authored baseline or packaging readiness. C's source/PIE/review gates are closed; this receipt debt is not an open window-identity defect.

- `TODO-05A1-C` has no separate archived manual `PolyQuestEditor (Development Editor)` compile or Editor readback receipt. User PIE and Automation are confirmed, but this authored-validation gap remains non-blocking until a compile/readback receipt or evidence-backed no-adoption decision is recorded; it must be closed before claiming a clean authored baseline or packaging readiness.

- `TODO-05A1-D1` 的源码、专项 Automation 与用户 PIE 已完成，Gemini 报告手动编译和 Editor readback 通过；但 Front/Backstab/Victim Montage、Notify 放置及 Launch/AnimBP/Blueprint 关系仍是用户 `Content/**` WIP，不构成干净 authored baseline 或 packaging 证明。关闭该 authored debt 的条件是形成可追溯的资产基线和对应 readback，或提供 evidence-backed no-adoption。`REC-05A1-03` Native gate 与 `REC-05A1-03-MIG` authored No-Op/readback gate 均已完成；D2A/D2B/D2C/D2D/E 与 `TODO-07A2-A` 已完成源码/专项 Automation/用户 PIE 与 Main review 收口，但各自独立编译/readback 收据仍按 Known Risks 处理；`TODO-03C` 保持独立敌人路线。

- RateWindow's supported boundary remains explicit after `TODO-07B8-C`: coincident Branching Point event loss is outside the identity-selection guarantee. Player has no lethal-health teardown route yet; future `TODO-03D` death/reload adoption must exercise Cancel/EndPlay with an active RateWindow. C covers the existing cancellation, interruption, destruction and applicable UnPossess routes. No global Time Dilation, Character-wide listener, batch migration or Agent asset writes are authorized.

## Stage Completion Standard

阶段完成需满足批准计划、适用的源码/资产验证、用户编译与 PIE 证据、审查、文档同步及明确提交批准。接受的验收调整须记录理由和剩余边界；提交本身不核销未完成门禁。
