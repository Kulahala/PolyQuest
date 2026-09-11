# TODO-07B12: Action Facing Block Contract v1

## 1. 阶段定位、基线与路由

- **Target Objective**：在 GAS 内建立精确的 State.Block.Facing 生命周期，使 Enemy 的 Gameplay Focus 与 Player 的 Lock-On locomotion 在动作期间不能接管角色朝向。
- **阶段关系**：ROADMAP.md 已排定 TODO-07B11 -> TODO-07B12 -> TODO-07B13；本阶段只处理 facing block 生命周期和两个消费端，不提前实现 Player grounded launch。
- **基线**：main @ cc2f0de660eaeb3082816c95ffb9f556cf66d915（2026-09-11）。
- **Archive Preflight**：PASS（实施前）。实施前 `ROADMAP-archive.md` 仅有一个 TODO-07B11 收口章节（第 1459 行）；旧 `plan.md` 仍由 HEAD 的 Git 历史保留。本次收口已追加唯一 TODO-07B12 章节，不重复归档。
- **当前工作树（实施前快照）**：非 clean。现有 Content/**、Blueprint、AnimBP、Montage、地图、Config 与其他 WIP 均为用户所有；本阶段必须保留、排除，不得回滚、清理、格式化或批量暂存。初始 ROADMAP.md 用户改动不在实施切片范围；收口文档由 Main 按批准边界更新。
- **初始证据边界**：本计划建立时仅有当前 Source、Config、文档和定向 CodeGraph 静态证据；阶段收口后的用户证据与 Main Review 记录见第 9 节。
- **技能路由**：
  - Outer: ue-stage-workflow
  - Primary: ue5-cpp-gameplay
  - Support: none
  - Route reason: native GAS-owned Tag lifecycle plus bounded Enemy Controller and Player Lock-On consumers。
- **执行路线**：manual/out-of-band Gemini。Implementation executors: 1（Gemini）；in-app delegation: 0。Main 保留架构、范围、验证解释、Fresh Review、文档、暂存和提交所有权；Gemini 不得提交。

## 2. 目标、所有权与刻意非目标

### 目标

- 注册 State.Block.Facing，并由四个 Enemy Ability 的 ActivationOwnedTags 作为唯一生命周期来源。
- 覆盖 UEnemyStanceBreakAbility、UEnemyMeleeAbility、UEnemyHitReactionAbility、UEnemyLaunchReactionAbility；Launch 的 Root Motion 与 Legacy 分支共用同一 Ability 生命周期。
- 让 Enemy Controller 在 Block 期间停止普通 Focus 驱动的朝向接管，同时保留 Root Motion 清除/恢复语义。
- 让 Player 的 action-facing 与 Lock-On locomotion 消费该 Tag；移除 Tag 后按现有 RotationRate 恢复，不做瞬时 snap。
- 保留自然结束、取消、死亡、销毁、UnPossess、同步重入和多贡献计数的清理契约。

### Deliberate Non-goals

- 不迁移或修改任何 Player Ability，包括 UPlayerLaunchReactionAbility；Player 在本阶段只做消费端。
- 不给 UEnemySmallHitReactionAbility 或 UEnemyVictimExecutionAbility 添加 State.Block.Facing。
- 不新增 State.Block.Movement、Controller-side action FSM、全局旋转框架、网络复制、预测/回滚或额外模块依赖。
- 不新增 C++ 每帧强制 Actor 旋转；Enemy 动作中的转向仍由作者化 Montage/既有 Legacy task 负责。
- 不使用 IsAnyMontagePlaying()、AbilitySpec ActiveCount 或平行 bool 作为生产状态权威。
- 不引入 Motion Warping；只有在 Editor readback 证明存在真实 Warp Window 和动态目标/距离消费者时，另立后续阶段。
- 不编辑或迁移 .uasset、.umap、Blueprint、AnimBP、Montage、地图、导入资源或其他用户 WIP。
- 不改伤害、Poise、移动输入、Stamina、Jump、Target Selection、StateTree 或 Execution Lock。

## 3. 冻结运行时契约

### GAS owner matrix

| Ability owner | ActivationOwnedTags 新增项 | Ability.Action.Teardown.OnUnpossess |
|---|---|---|
| UEnemyStanceBreakAbility | State.Block.Facing | 已有，保持 |
| UEnemyMeleeAbility | State.Block.Facing | 新增到 AbilityTags |
| UEnemyHitReactionAbility | State.Block.Facing | 新增到 AbilityTags |
| UEnemyLaunchReactionAbility | State.Block.Facing | 新增到 AbilityTags |

- 依靠 UE 5.8 UGameplayAbility 对 ActivationOwnedTags 的自动加减；禁止手动 AddLooseGameplayTag/RemoveLooseGameplayTag、额外 loose-tag 计数器或手动移除新 Tag。
- 新增 teardown selector 只能进入 AbilityTags，绝不能进入 AbilitiesToCancel。
- 保留每个 Ability 现有的 AbilitiesToCancel 内容和数量。尤其是 EnemyHitReactionAbility.cpp 与 EnemyLaunchReactionAbility.cpp 中已有 AbilitiesToCancel.Num() == 2，以及 HitReactionAutomationTests.cpp 的精确数量断言，必须原样成立。
- 在已有 ValidateActivationSetup() 中只补新 Tag 的 validity 检查；没有现有验证函数的路径不新增共享验证框架。
- AEnemyCharacter::UnPossessed() 继续通过现有 Ability.Action.Teardown.OnUnpossess 调用 CancelAbilities；不得修改全局取消逻辑。

### Enemy Controller consumer

- 删除 IsEnemyLaunchReactionActive() 的声明、定义、调用及仅供该查询使用的 EnemyLaunchReactionAbilityTag 成员；全局检索确认无外部消费。
- 新增私有 IsEnemyFacingBlocked()，直接查询受控 Enemy 的 ASC 与 State.Block.Facing。
- UpdateControlRotation() 固定顺序：死亡/Execution Lock 早退；Root Motion active 时保持清除 Focus；随后检查 Facing Block；Block active 时不调用普通 Super::UpdateControlRotation()、不写 Actor yaw、不清除普通 Focus。
- Root Motion 结束而 Facing Block 仍在时，不消费 bWasRootMotionActive recovery handoff；Tag 清除后才恢复 Focus 与既有平滑 yaw。
- 单独存在 State.Action.HitReacting 时仍允许普通 Controller facing，证明新 Tag 才是阻断权威。

### Player consumer

- Player 新增 FacingBlockStateTag 和 FDelegateHandle FacingBlockTagChangedHandle。
- BindSprintStateEvents() 与 UnbindSprintStateEvents() 是 Sprint/Facing 委托的唯一注册和注销路径；其调用必须对称覆盖 PossessedBy、OnRep_PlayerState、UnPossessed，绑定新 ASC 前先解除旧 ASC。
- 使用 NewOrRemoved 事件注册 OnFacingBlockTagChanged；该回调只调用 UpdateActionFacingRotationMode()，不得调用 CancelSprintAbility() 或启动 Sprint。
- UpdateActionFacingRotationMode() 将新 Tag 纳入 action-owned rotation；CanApplyLockedLocomotionFacing() 将新 Tag 作为阻断条件；Tag 移除后只恢复模式，下一 Tick 按现有 RotationRate 转向。
- 保持 ApplyActionFacing()、ApplyLockAwareActionFacing()、ApplyDodgeFacing()、Bow aim、Root Motion、移动输入和 Stamina 行为不变。

## 4. Approved Paths 与接口边界

Gemini 只可修改以下路径：

- Config/Tags/PolyQuestGameplayTags.ini
- Source/PolyQuest/Public/AbilitySystem/Abilities/EnemyStanceBreakAbility.h
- Source/PolyQuest/Private/AbilitySystem/Abilities/EnemyStanceBreakAbility.cpp
- Source/PolyQuest/Public/AbilitySystem/Abilities/EnemyMeleeAbility.h
- Source/PolyQuest/Private/AbilitySystem/Abilities/EnemyMeleeAbility.cpp
- Source/PolyQuest/Public/AbilitySystem/Abilities/EnemyHitReactionAbility.h
- Source/PolyQuest/Private/AbilitySystem/Abilities/EnemyHitReactionAbility.cpp
- Source/PolyQuest/Public/AbilitySystem/Abilities/EnemyLaunchReactionAbility.h
- Source/PolyQuest/Private/AbilitySystem/Abilities/EnemyLaunchReactionAbility.cpp
- Source/PolyQuest/Public/AI/EnemyAIController.h
- Source/PolyQuest/Private/AI/EnemyAIController.cpp
- Source/PolyQuest/Public/Character/Player/PlayerCharacter.h
- Source/PolyQuest/Private/Character/Player/PlayerCharacter.cpp
- Source/PolyQuest/Private/Tests/EnemyStanceBreakRateWindowAutomationTests.cpp
- Source/PolyQuest/Private/Tests/HitReactionAutomationTests.cpp
- Source/PolyQuest/Private/Tests/EnemyLaunchReactionRootMotionAutomationTests.cpp
- Source/PolyQuest/Private/Tests/EnemyRootMotionFacingAutomationTests.cpp
- Source/PolyQuest/Private/Tests/PlayerLockOnAutomationTests.cpp

允许的接口变化只有：

- Config 中新增精确 Tag State.Block.Facing。
- 四个 Ability 的私有 FGameplayTag 字段、构造函数注册和已有 setup validity 检查。
- EnemyMeleeAbility.h 在 WITH_DEV_AUTOMATION_TESTS 下增加最小只读 GetTestActivationOwnedTags()；不增加 Shipping API、UFUNCTION 或反射接口。
- EnemyAIController 的私有通用查询替换 Launch-specific 查询。
- Player 的私有 Tag、delegate handle 和回调。

Main-only 文档路径为 plan.md、ARCHITECTURE.md、ROADMAP.md、ROADMAP-archive.md；只有阶段门禁完成后才可更新，Gemini 不得触碰。

## 5. 实施顺序

1. 注册 State.Block.Facing，保持现有 Tag 文件风格，不引入其他 Tag。
2. 在四个 Enemy Ability 中加入 owned Tag；在 Melee/Big/Launch 中加入 teardown selector 到 AbilityTags；保留所有既有 cancellation 列表、数量断言、callback/token、weak-reference、ReadyForActivation() 重入保护和单一 EndAbility() 清理出口。
3. 在 Enemy Controller 中删除 Launch-specific active-spec 路径，加入 ASC Tag 查询，并保持 Root Motion recovery handoff 的时序。
4. 在 Player 中加入独立 Tag delegate 的对称绑定/注销和两个消费门口的判定。
5. 只扩展既有 Automation 文件，先完成 CDO/契约断言，再完成 transient ASC、Controller 和 Player 行为覆盖。

## 6. Automation 验证矩阵

- PolyQuest.Combat.EnemyStanceBreakRateWindow：Stance owned/teardown Tag；真实激活期间 Tag 存在；自然结束、取消、UnPossess 后移除；至少一次 InstancedPerActor re-entry。
- PolyQuest.Combat.HitReaction：Melee/Big/Launch 的 owned/teardown CDO matrix；Small/Victim negative；AbilitiesToCancel 成员与数量保持原值。
- PolyQuest.Combat.EnemyLaunchReactionRootMotion：Root Motion 与 Legacy 分支共用 Tag；自然结束、取消、UnPossess、同步重入和清理。
- PolyQuest.Enemy.RootMotionFacing：删除 LaunchSpec->ActiveCount = 1 的假造状态；Controller fixture 使用 EnemyASC->AddLooseGameplayTag(TagBlockFacing)/RemoveLooseGameplayTag(TagBlockFacing)，并确保测试清理。覆盖无 Block 正常转向、Block 保持 yaw/Focus、Root Motion recovery 延迟、Tag 清除后恢复，以及单独 HitReacting 仍可转向。
- PolyQuest.Player.LockOn：Block 期间 yaw 与 bOrientRotationToMovement 保持；移除后按 RotationRate 恢复；回调不触发 Sprint cancel/restart；重复 Possess/replication/UnPossess 不产生重复或悬挂 delegate；既有攻击、Dodge、HitReact、SmallHit、Root Motion、Bow aim 回归。
- 对能够使用现有 transient fixture 的四个 Enemy owner，验证 ASC Tag 的激活/结束计数；不得通过直接修改 AbilitySpec.ActiveCount 伪造生产状态。

## 7. 用户验证与证据门禁

### Main static gate

- 检查批准路径 diff、Tag 名称、ASC 权属、生命周期、UnPossess selector 和异步清理；运行 git diff --check。
- 使用一次有界 ue-strict-review Fresh Review。若 code-review-graph 仍基于旧索引，记录 coverage fallback，不把图谱输出当作运行时证明。
- 同一根因最多一次有依据修复和一次定向重跑；重复失败即停止并保留首个失败证据。

### User-owned Editor readback

- 确认 Gameplay Tags 中存在精确 State.Block.Facing。
- 读取四个 Enemy Ability CDO 的 ActivationOwnedTags、AbilityTags、实际 Montage 引用、Skeleton、Slot、AnimBP、Root Motion/In-Place 状态。
- 若存在真实 Motion Warp Window，记录其目标/距离消费者；不存在则记录 no-adoption，不新增 Motion Warping。
- 不修改任何 .uasset、.umap、Blueprint、AnimBP、Montage 或地图。

### User-owned compile, Automation and PIE

- 编译 PolyQuestEditor (Development Editor)。
- 运行 PolyQuest.Combat.EnemyStanceBreakRateWindow、PolyQuest.Combat.HitReaction、PolyQuest.Combat.EnemyLaunchReactionRootMotion、PolyQuest.Enemy.RootMotionFacing、PolyQuest.Player.LockOn。
- Scene01 PIE 覆盖 Enemy Stance Break、Melee、Big、Launch 的 Focus/yaw 阻断与恢复，取消/死亡/销毁/UnPossess/re-entry，Small Hit 不误阻断，以及 Player Lock-On 回归。
- 不将本阶段结果扩写为位移、伤害、网络、包装、发布或完整 authored baseline 证明。

## 8. Gemini 交付要求、停止条件与收口

- 首先阅读本 plan.md、目标 Header/CPP、直接调用者和既有 Automation；仅在 Approved Paths 内实现。
- 首轮交付前完成一次干净、只读的实施自审，明确检查 AbilitiesToCancel.Num() == 2 约束、ActiveCount 假造移除、IsEnemyLaunchReactionActive() 清理、Player delegate 对称生命周期、ReadyForActivation() 重入、UnPossess、销毁和单一 EndAbility() 清理。
- 交付必须包含批准路径 diff、静态检查结果、Automation 结果/新增覆盖、未运行的用户门禁，以及明确的非目标说明。
- 不得 git add -A、提交、回滚、删除、导入资产或修改 plan.md、ROADMAP.md、ARCHITECTURE.md、Content/**、Blueprint、AnimBP、Montage、地图、Build.cs。
- 任何清单外 Source、Tag、Input、Config、共享资产迁移、Motion Warping 组件、网络逻辑或墙体专属表现需求出现时立即停止并报告。
- 没有真实 Warp Window 与动态消费者时不实现 Motion Warping；没有新增 Player launch 证据时不扩展到 TODO-07B13。
- 阶段完成后由 Main 解释验证证据、执行 ue-strict-review、同步 ARCHITECTURE.md/ROADMAP.md/ROADMAP-archive.md，并在用户明确批准后才暂存或提交。

## 9. Main 收口记录与提交边界（2026-09-11）

- **实现结果**：Gemini 交付改动保持在 18 个 Approved Paths 内；`State.Block.Facing` 已注册并由四个 Enemy Ability 的 `ActivationOwnedTags` 管理，Enemy Controller 与 Player Lock-On 消费端已接入。`AbilitiesToCancel.Num() == 2`、UnPossess selector、异步 token/委托和单一 `EndAbility()` 清理契约保持。
- **用户证据**：用户确认 `PolyQuestEditor (Development Editor)` 编译通过、五组 Focused Automation（StanceBreak、HitReaction、Enemy Launch Root Motion、Enemy RootMotionFacing、Player LockOn）通过，以及 Scene01 PIE 通过。该记录不扩写为视觉、网络或包装证明。
- **Main Fresh Review**：Main 依据 `ue-strict-review` 完成有界两批审查；正常与对抗性核对均未发现可由批准 diff/source 证据定级的 P0/P1/P2 缺陷。`code-review-graph` 与当前 HEAD 对齐；其未覆盖提示保留为静态残余风险。
- **验证债务**：本阶段未形成独立用户-owned Editor readback 收据来逐项记录精确 Tag 注册及四个 Enemy Ability CDO/Montage 设置，归口为 `Debt-07B12-AuthoredReadback`。该债务不阻塞源码、编译、Automation、PIE 与 Review closeout，但在关闭前不宣称 clean authored baseline 或 packaging readiness。
- **文档与提交**：Main 已同步 `ARCHITECTURE.md`、`ROADMAP.md` 与 `ROADMAP-archive.md`；提交只包含批准的 Config/Source/Test 路径和上述文档，所有其他用户 WIP 排除。用户已明确批准提交，提交哈希以 Git 历史为准。
