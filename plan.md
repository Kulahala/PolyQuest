# TODO-07B7: Combat Feedback Profile Taxonomy Split v1

## 阶段状态与执行路由

- **状态**：Phase A、用户资产迁移与 Phase B 均已完成。用户确认最终 `PolyQuestEditor` 编译成功后，focused Automation 与 Scene01 PIE 均通过；Main 的 scope-bounded Fresh Review 已收口。
- **基线**：`main @ 17fd9b6`（`[Refactor] 清退旧战斗 Loadout 兼容层`）。
- **工作区边界**：当前存在用户拥有的 `Content/**` 修改、删除和未跟踪资产，以及 `Config/Automation/Presets/1.json`。全部保留；本阶段不检查、清理、回滚、暂存或提交它们。
- **Outer**：`ue-stage-workflow`
- **Primary**：`ue5-cpp-gameplay`
- **Support**：`ue5-debug-validation`
- **Route reason**：本阶段只重构已落地的 Combat Feedback DataAsset 分类与其 Native 消费者，保留既有 Damage、GAS、Overlay、Camera Shake、Hit-Stop 和 Defense 的运行时所有权；资产类迁移必须由 Editor 完成。
- **Execution route**：`manual/out-of-band Gemini`。
- **Implementation executors**：Gemini 仅在 Main 批准的 Phase A / Phase B 各自窗口内写入冻结的 Source/test 路径。Main 负责合同、计划、范围、验证解释、Fresh Review、文档、staging 和 commit；用户负责 Editor、手动编译、PIE 与最终提交批准。
- **Contract owner**：Main。Gemini 不得改变公开 API、角色/GAS 所有权、Damage/Poise/Tag/Input 契约、资产命名决定或阶段边界。

## 目标与已接受决策

当前 `UCombatFeedbackDataAsset` 将 Player 的受击震屏、受创音效、Guard/Parry 与 Enemy 的命中顿帧、肉体音效、血液 Niagara 混在同一 Details 面板。它已经造成真实的 authoring 歧义，不等第二个敌人再处理。

本阶段把共同的受击 Overlay 留在基类，把角色专属字段分到两个 Native 派生 DataAsset，使编辑器面板、读取类型和错误配置的降级行为保持一致。

最终类型关系：

```text
UCombatFeedbackDataAsset (Abstract, common Overlay only)
|- UPlayerCombatFeedbackDataAsset
|  |- ReceivedHitSound
|  |- Small / Big / Launch Camera Shake
|  |- Guard / Parry sound and Parry Hit-Stop
|
`- UEnemyCombatFeedbackDataAsset
   |- ImpactSound / ImpactBloodSystem
   `- Small / Big / Launch Impact Hit-Stop
```

最终产品资产沿用已知的 canonical 路径，避免长期留下迁移命名：

```text
Content/_DataAssets/Player/FeedBack/DA_CombatFeedback_Player.uasset
Content/_DataAssets/Enemy/FeedBack/DA_CombatFeedback_Enemy.uasset
```

迁移期间只能临时使用：

```text
DA_CombatFeedback_Player_TypedMigration
DA_CombatFeedback_Enemy_TypedMigration
```

## 冻结运行时合同

### 数据与默认值

- 最终 `UCombatFeedbackDataAsset` 声明为 `UCLASS(Abstract, BlueprintType)`，只保留：
  - `HitFeedbackOverlayMaterial`
  - `HitFeedbackOverlayDurationSeconds`，默认 `0.10f`
- 新增 `UPlayerCombatFeedbackDataAsset`、`UEnemyCombatFeedbackDataAsset`、`FPlayerCombatFeedbackTierSettings`、`FEnemyCombatFeedbackTierSettings`、`FPlayerCombatFeedbackDefenseSettings`。
- 两个派生类各自提供同名的 typed `GetTierSettings(EHitReactionTier)`；`None` 和 `Invalid` 必须返回 `nullptr`。
- Player profile 只拥有：`ReceivedHitSound`、Small/Big/Launch 的 `ReceivedHitCameraShakeClass` 和 `AttackerImpactCameraShakeClass`、`GuardSuccessSound`、`ParrySuccessSound`、Parry Hit-Stop。
- Enemy profile 只拥有：`ImpactSound`、`ImpactBloodSystem`、Small/Big/Launch 的 Impact Hit-Stop。
- 保持既有 Enemy Hit-Stop 默认值：Small `0.03 / 0.10`、Big `0.05 / 0.03`、Launch `0.05 / 0.05`（duration / dilation）；保持 Player Parry Hit-Stop 默认值 `0.05 / 0.03`。
- `Duration` 使用 `ClampMin = "0.0"` 与 `Units = "Seconds"`；`TimeDilation` 使用 `ClampMin = "0.001"`、`ClampMax = "1.0"`；Overlay duration 同样保持非负秒数约束。

### 角色与 Ability 消费者

- `ABaseCharacter` 继续只持有一个 `UCombatFeedbackDataAsset*`，`TriggerHitFeedbackOverlay()` 只读取基类 Overlay，不因 profile 类型错配而失效。
- 增加对称、只读 typed accessor：
  - `APlayerCharacter::GetPlayerCombatFeedbackData() const`
  - `AEnemyCharacter::GetEnemyCombatFeedbackData() const`
- 正确 profile 缺失、`nullptr` 或角色误配另一方 profile 时，角色专属通道记录每 Actor 一次 Warning 后 fail-closed；不阻断伤害、反应、格挡、弹反、死亡或既有清理。可选的 Sound、Niagara、Camera Shake 留空仍是静默 no-op，不记录 Warning。
- Player 的 Received Hit / Attacker Impact Camera Shake、Received Hit Sound、Guard success 和 Parry success 全部只从 Player accessor 读取。
- Enemy 的 Impact Hit-Stop、Impact Sound、Impact Blood 全部只从 Enemy accessor 读取。
- 错配 profile 时共同 Overlay 仍按 Base 数据工作，角色专属反馈跳过。
- 删除无调用者、且会在 Base 抽象后失效的 `ABaseCharacter::ConfigureTestHitFeedbackOverlay`。测试通过 typed transient profile 加 `SetTestCombatFeedbackData()` 配置，不新增生产 bypass。
- 不新增 GameplayCue、反馈 dispatcher、Damage 分支、Tag/Input、全局配置、每敌人 profile 层级或兼容/回退路由。

## 双阶段执行与资产门禁

### 用户预迁移记录（Phase A 前）

这是保留既有 authoring 数值的必要步骤，不是源码实现：

1. 在当前两个 Base-type profile 仍可读时，记录或截图其所有角色专属字段和值：Player 的 Camera Shake、ReceivedHitSound、Defense；Enemy 的每 tier Hit-Stop、ImpactSound、ImpactBloodSystem；共同 Overlay 也一并记录。
2. 不在本阶段前删除、保存覆盖或重命名这两个旧资产；它们是手工迁移的值来源。
3. 回报已保存的值记录即可开始 Gemini Phase A。若旧资产值已无法读取，停止，不猜测、不从源码默认值补造产品调参。

### Phase A: Native 类型与消费者迁移（Gemini）

1. 在 `CombatFeedbackDataAsset.h/.cpp` 拆出 Base / Player / Enemy 类型与 typed settings；**此时 Base 仍 concrete**，不得加入 `Abstract`。
2. 将 Player、Enemy、Guard、Parry 读取点迁移到其 typed accessor；确保错误类型、`nullptr` 和空可选资源按冻结合同降级。
3. 保持 `ABaseCharacter` 的共同 Overlay 管线和 base pointer，不引入第二个角色字段或缓存。
4. 更新 transient fixture 与聚焦 Automation，全部使用正确的 typed transient profile；补齐 wrong-profile、null-profile、可选资源为空以及现有成功路径。
5. Gemini 只完成 Source/test 静态检查后停机。不得实施 Phase B、不得 Editor 写入、不得删除旧资产、不得改文档、暂存或提交。

### 用户资产迁移与 readback（Phase A 编译成功后）

1. 用户手动编译 `PolyQuestEditor` Development Editor；只有编译成功才继续。
2. 创建 `DA_CombatFeedback_Player_TypedMigration`（`UPlayerCombatFeedbackDataAsset`）与 `DA_CombatFeedback_Enemy_TypedMigration`（`UEnemyCombatFeedbackDataAsset`），按预迁移记录恢复对应字段；共同 Overlay 填入两个 profile。
3. 将 `BP_Player` 指向 Player typed profile，将所有当前 Enemy Blueprint / archetype 指向 Enemy typed profile；读回父类、字段和引用确认。
4. 用 Reference Viewer 确认两个旧 Base-type profile 为零引用。若任何引用仍存在，停止并回报引用方，不得删除或标记 Base Abstract。
5. 删除旧资产；将两个 `*_TypedMigration` 资产重命名回本阶段 canonical 名称，修复 redirectors；再读回最终路径、父类和角色分配。
6. 回报上述 readback、零引用与删除/redirector 结果。只有这份证据允许 Main 下达 Phase B handoff。

### Phase B: Base 抽象化（Gemini，需新的 Main 明确授权）

1. 仅将 `UCombatFeedbackDataAsset` 改为 `UCLASS(Abstract, BlueprintType)`，并在既有聚焦测试中断言 Base 是 abstract、两个派生类仍可构造。
2. 不重新设计字段、不再扩展消费者、不改产品资产或文档。
3. 完成相同的 scoped static checks 后停止，等待用户最终编译、Automation 和 PIE。

## 批准修改路径

Phase A 和被单独授权的 Phase B 都只能修改以下路径；Phase B 的正常预期只触及 DataAsset 源码和必要的既有测试断言，但仍不得自行扩大路径。

```text
Source/PolyQuest/Public/Combat/Feedback/CombatFeedbackDataAsset.h
Source/PolyQuest/Private/Combat/Feedback/CombatFeedbackDataAsset.cpp
Source/PolyQuest/Public/Character/BaseCharacter.h
Source/PolyQuest/Private/Character/BaseCharacter.cpp
Source/PolyQuest/Public/Character/Player/PlayerCharacter.h
Source/PolyQuest/Private/Character/Player/PlayerCharacter.cpp
Source/PolyQuest/Public/Character/Enemy/EnemyCharacter.h
Source/PolyQuest/Private/Character/Enemy/EnemyCharacter.cpp
Source/PolyQuest/Private/AbilitySystem/Abilities/PlayerGuardAbility.cpp
Source/PolyQuest/Private/AbilitySystem/Abilities/PlayerParryAbility.cpp
Source/PolyQuest/Private/Tests/CombatAutomationFixture.cpp
Source/PolyQuest/Private/Tests/CombatHitFeedbackAutomationTests.cpp
Source/PolyQuest/Private/Tests/PlayerDefenseAudioAutomationTests.cpp
Source/PolyQuest/Private/Tests/ParrySuccessImpactFeedbackAutomationTests.cpp
```

任何其他 Header/Source、Config、Gameplay Tag、Build.cs、`.uproject`、Blueprint、AnimBP、Montage、Input、Map、`.uasset`、新文件或测试基础设施需求，都必须停止并给 Main 证据；不得以静默兼容层、硬编码默认值或扩大 fixture 来绕过这个边界。

## Automation 合同

Phase A 必须让 `CombatAutomationFixture` 为 Player / Enemy 分别创建正确类型的 transient profile。聚焦测试应保留当前已验证行为，并新增或调整以下可证明矩阵：

| 场景 | 必须证明 |
| --- | --- |
| 正确 Player profile | Overlay、Received/Attacker Camera Shake、Received Hit Sound、Guard、Parry 与现有去重语义保持。 |
| 正确 Enemy profile | Overlay、Small/Big/Launch Impact Hit-Stop、Impact Sound、Blood 保持。 |
| Player 误配 Enemy profile | Overlay 仍可工作；Player 专属震屏/音效/Guard/Parry 静默跳过；伤害和防御生命周期继续。 |
| Enemy 误配 Player profile | Overlay 仍可工作；Enemy Impact/Hit-Stop/Blood 静默跳过；伤害流程继续。 |
| `nullptr` profile | Player、Enemy、Guard、Parry 均 fail-closed，不能阻断 Damage 或 Defense 生命周期。 |
| 可选资源为空 | 对应单一反馈通道静默 no-op，不造成错误日志、崩溃或错误的计数。 |
| Phase B 类型合同 | Base 为 abstract；两个 derived profile 可构造；不再依赖 Base 的已移除字段。 |

该矩阵关闭 `Debt-07B6-ProfileNullCoverage`，并以角色本地一次性 diagnostic 处理原先的 Warning 非对称问题；它不替代用户最终的 Editor 或 PIE 证明。

## 验证门槛

| 门槛 | 所有者 | 证据 |
| --- | --- | --- |
| Phase A 静态 | Gemini | 对批准 C++ 文件执行 Rider error-level inspection、旧混合字段的 scoped search、`git diff --check`；报告错误、未运行门禁和范围。 |
| Phase A 编译 | 用户 | 手动 `PolyQuestEditor` Development Editor 成功。 |
| 资产迁移/readback | 用户 | typed 父类、字段迁移、角色 assignment、旧资产零引用、删除、rename 与 redirector 修复。 |
| Phase B 静态 | Gemini | 仅在 Main 给出用户资产证据后执行 Abstract 小改与相同 static checks。 |
| 最终编译 | 用户 | Phase B 后手动 `PolyQuestEditor` Development Editor 成功。 |
| Focused Automation | 用户 | `PolyQuest.Combat.HitFeedback`、`PolyQuest.Combat.AttackerImpactCameraShake`、`PolyQuest.Combat.DefenseAudio`、`PolyQuest.Combat.ParrySuccessFeedback`、`PolyQuest.Combat.HitReaction`。 |
| Scene01 PIE | 用户 | Player 受击、Guard、Parry；Enemy 受击 Impact/Blood/Hit-Stop；错配/null 不阻断核心战斗；Overlay 仍在两类角色上生效。 |

不得把 Rider、`git diff --check`、Reference Viewer、编译或 Automation 说成 Scene01 PIE / 视觉证明，反之亦然。

## 非目标、风险与停止条件

- 不添加第二个敌人、不做单个 Enemy profile 派生层级、不做每武器/每技能反馈表，不重做反馈调参。
- 不改 GAS、Damage、Poise、Reaction、StateTree、Input、Gameplay Tag、Loadout、武器或 Camera 所有权。
- 不保留旧混合字段、Base 兼容属性、动态 fallback、反射名称别名或迁移 C++ shim；资产迁移完成后 Base 必须是抽象的。
- 不删除用户内容或写入 `.uasset`；所有资产操作都是用户 Editor 操作，且仅在零引用证据后进行。
- 若 Phase A 编译失败、旧资产值无法保存、迁移资产不能按 typed class 创建、Reference Viewer 仍有旧 Base 引用、或者需要批准路径外的 API/资产/Config 修改，立即停止并返回具体证据。

## Main 收尾与提交边界

用户已完成最终编译、Automation 与 PIE，Main 已完成一次 scope-bounded defect-first Fresh Review。修复其发现的测试门禁问题并完成最终窄复核后，未保留 P0-P2；本次收尾已同步 `ARCHITECTURE.md` 的稳定 DataAsset 所有权、`ROADMAP.md` 的阶段状态与 `Debt-07B6-ProfileNullCoverage` 关闭记录，并已把本计划 closeout 归档到 `ROADMAP-archive.md`。

提交只包含本阶段批准的 Source/test 与 Main 文档路径，严格排除所有用户 `Content/**` WIP、`Config/Automation/Presets/1.json` 和其他未批准变更；用户已明确批准本次暂存和提交。

## 实施、验证与收口记录（2026-09-01）

### 实际 Source/test 范围

本阶段实际修改且将提交的 Source/test 路径严格为：

```text
Source/PolyQuest/Public/Combat/Feedback/CombatFeedbackDataAsset.h
Source/PolyQuest/Private/Combat/Feedback/CombatFeedbackDataAsset.cpp
Source/PolyQuest/Public/Character/BaseCharacter.h
Source/PolyQuest/Private/Character/BaseCharacter.cpp
Source/PolyQuest/Public/Character/Player/PlayerCharacter.h
Source/PolyQuest/Private/Character/Player/PlayerCharacter.cpp
Source/PolyQuest/Public/Character/Enemy/EnemyCharacter.h
Source/PolyQuest/Private/Character/Enemy/EnemyCharacter.cpp
Source/PolyQuest/Private/AbilitySystem/Abilities/PlayerGuardAbility.cpp
Source/PolyQuest/Private/AbilitySystem/Abilities/PlayerParryAbility.cpp
Source/PolyQuest/Private/Tests/CombatAutomationFixture.cpp
Source/PolyQuest/Private/Tests/CombatHitFeedbackAutomationTests.cpp
Source/PolyQuest/Private/Tests/PlayerDefenseAudioAutomationTests.cpp
Source/PolyQuest/Private/Tests/ParrySuccessImpactFeedbackAutomationTests.cpp
```

### 已收口合同

- `UCombatFeedbackDataAsset` 已是只承载共同 Overlay 的抽象根类型；`UPlayerCombatFeedbackDataAsset` 承载 Player 的 Camera Shake、Received Hit Sound 与 Defense 数据，`UEnemyCombatFeedbackDataAsset` 承载 Enemy 的 Impact Sound/Blood 与 tier Hit-Stop 数据。
- `ABaseCharacter` 仍只持有一个 Base profile 指针并独占 Overlay 生命周期；Player/Enemy 分别通过 typed accessor 读取专属字段。误配、空 profile 只让专属通道以每 Actor 一次诊断 fail-closed，共同 Overlay 继续按 Base 字段工作；空的可选资源仍是静默 no-op。
- 未改变 Damage、Poise、GAS、Gameplay Tag/Input、Hit-Stop owner、Guard/Parry 生命周期或新增 GameplayCue、dispatcher、第二条伤害路径、每 Enemy profile 层级和兼容回退。

### 用户资产与验证证据

- 用户确认已在 Editor 完成 Player/Enemy typed profile 迁移与角色 assignment，确认旧 Base-type profile 为零引用后删除，并修复 redirector；这些用户拥有的 `.uasset` 变更不纳入本次 Source/document 提交。
- 用户明确确认最终 `PolyQuestEditor` 编译成功；随后在 Editor 运行了 focused Automation 与 `/Game/Maps/Scene01` PIE。反馈相关 7 个 Automation 路径均通过，含最终复验的 `PolyQuest.Combat.HitFeedback`。
- Main Fresh Review 发现 `CombatHitFeedbackAutomationTests.cpp` Section 24 的 mismatch/null fixture 可因裸条件分支而静默跳过。Main 在已批准测试路径内补充 `TestNotNull` 前置断言、失败清理与 `return false`，用户重跑 `PolyQuest.Combat.HitFeedback` 成功；最终窄复核未发现 P0-P2。

### 债务、排除项与下一步

- `Debt-07B6-ProfileNullCoverage` 已由正确 profile、错配 profile、`nullptr` profile 与可选资源为空的矩阵关闭；07B7 没有新增已接受债务。
- 本次不检查、修改、暂存或提交任何 `Content/**`、`Config/Automation/Presets/1.json`、其他 Config/Source WIP、Blueprint、Map、输入、动画、Niagara、声音、`.uproject` 或 `Build.cs`。
- 当前下一开放实施阶段是 `TODO-05A: Stagger Front Execution v1`；`plan.md` 保留本收口记录，直到下一阶段计划被接受。
