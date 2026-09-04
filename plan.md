# TODO-05A1-E：Execution Impact Feedback v1（实施计划与收口）

## 阶段状态与基线

- 状态：已完成实现、用户验证与 Main Fresh Review；本记录保留为最近阶段的详细收口。
- 日期：2026-09-05。
- cwd：E:\GameDevelop\PolyQuest；分支：main；基线 HEAD：3dff750deea6b6ac18392c0bc716c3f2e71f710d。
- 工作树非 clean，包含用户-owned Content/**、Config、文档及其他 WIP；全部保留，不执行 reset、checkout、批量删除或 git add -A。
- D2D 已在 ROADMAP-archive.md:1328-1340 归档，ROADMAP.md:105-108 已指向 E；Archive Preflight：PASS。
- 当前导航证据：.codegraph/ 与 .code-review-graph/ 存在。图谱只作为 source/static 导航或影响分析证据，不替代编译、Editor readback、Automation、PIE 或视觉证据。

## 目标、范围与成功标准

唯一运行时问题：授权 Execution Hit 成功并完成 Exactly-Once 结算后，是否能独立触发玩家震屏、敌人顿帧、冲击音效和血液 Niagara，同时保持唯一伤害路径、Release 结果、Victim Montage 和普通近战反馈契约不变。

成功标准：

1. Front 与 Backstab 的授权 Hit 在 NonLethal 和 DeathPending 结果下各触发一次 Execution feedback。
2. Player attacker 使用独立 Execution Camera Shake；Enemy 使用独立 Execution Hit-Stop，并复用现有 ImpactSound 与 ImpactBloodSystem。
3. 普通 tier Camera Shake、Hit-Stop、Sound、Blood 不会在授权 Execution Hit scope 内先行重复触发；普通攻击路径行为保持不变。
4. GE 失败、未确认致死、取消、重复 Notify、重复 Release、错误 Context/Actor/ASC/Team、Dead 或 Destroyed 均不触发 Execution feedback。
5. Execution 配置独立于 EHitReactionTier 和 GE 的 Small/Big/Launch 标签；缺失 Execution 配置不回退 Big。
6. 用户完成 PolyQuestEditor 编译、两个 profile 的 Editor readback、focused Automation 和 Scene01 PIE 视觉验证后，Main Fresh Review 无 P0-P2 blocker。

## 批准修改路径与冻结契约

批准修改路径：

- Source/PolyQuest/Public/Combat/Feedback/CombatFeedbackDataAsset.h
- Source/PolyQuest/Private/Combat/Feedback/CombatFeedbackDataAsset.cpp
- Source/PolyQuest/Public/Character/Player/PlayerCharacter.h
- Source/PolyQuest/Private/Character/Player/PlayerCharacter.cpp
- Source/PolyQuest/Public/Character/Enemy/EnemyCharacter.h
- Source/PolyQuest/Private/Character/Enemy/EnemyCharacter.cpp
- Source/PolyQuest/Private/Combat/Melee/MeleeHitResolver.cpp
- Source/PolyQuest/Private/AbilitySystem/Abilities/PlayerFrontExecutionAbility.cpp
- Source/PolyQuest/Private/AbilitySystem/Abilities/PlayerBackstabExecutionAbility.cpp
- Source/PolyQuest/Private/Tests/ExecutionImpactFeedbackAutomationTests.cpp

不修改 ExecutionLockContext.*、Gameplay Tags、Input、Config、Build.cs、Blueprint、AnimBP、Montage、地图或任何 .uasset/.umap。

### Typed Feedback DataAsset

在 CombatFeedbackDataAsset.h 增加两个独立的 BlueprintType 结构：

- FPlayerCombatFeedbackExecutionSettings：只保存 AttackerImpactCameraShakeClass。
- FEnemyCombatFeedbackExecutionSettings：保存 ImpactHitStopDurationSeconds 与 ImpactHitStopTimeDilation。

UPlayerCombatFeedbackDataAsset 和 UEnemyCombatFeedbackDataAsset 各增加名为 Execution 的字段。

- Player Execution Camera Shake 的源码默认值为空。
- Enemy Execution Hit-Stop 的源码默认值为 0.05 秒和 0.03，初始值与 Big 一致。
- GetTierSettings(EHitReactionTier) 和 Small/Big/Launch 字段保持不变。
- 不增加 Execution 枚举值，不读取 GE 的 Small/Big/Launch 标签，不提供 Big fallback。

用户 Editor readback manifest 仅限：

- /Game/_DataAssets/Player/FeedBack/DA_CombatFeedback_Player
- /Game/_DataAssets/Enemy/FeedBack/DA_CombatFeedback_Enemy

用户可在 Editor 将 Player Execution Camera Shake 与 Enemy Execution Hit-Stop 初始设为 Big 对应值，再独立调优；未得到额外批准前不保存其他资产。

### Player Execution 震屏

在 APlayerCharacter 增加 C++ 入口：

    void TriggerExecutionImpactCameraShake();

增加私有 Execution resolver，仅读取 PlayerFeedback->Execution.AttackerImpactCameraShakeClass，然后复用现有 StartHitFeedbackCameraShakeInstance。

入口必须保留 authority、ASC、非销毁、非 Dead、Local Controller 和 Camera Manager 检查。缺失 Execution class 时静默跳过，不回退 Big 或其他 tier；现有 received-hit、Parry、Small/Big/Launch 路径不变。

### Enemy Execution 反馈与通道复用

在 AEnemyCharacter 增加 C++ 入口：

    void HandleExecutionImpactFeedback(
        UExecutionLockContext* ExecutionContext,
        AActor* SourceActor,
        const FHitResult& HitResult);

在同一 EnemyCharacter.cpp 内抽取三个轻量私有 helper：

- DispatchImpactHitStop(float DurationSeconds, float TimeDilation)
- DispatchImpactSound(...)
- DispatchImpactBlood(...)

helper 必须使用调用方已经解析的同一 Enemy typed profile，并保留现有测试计数器、位置 fallback、法线校验和 Controller-owned Hit-Stop 语义，不重新引入第二套配置来源。

HandleExecutionImpactFeedback 必须校验：

- authority、非销毁；
- Context active 且 victim accepted；
- Context HitState 严格为 NonLethal 或 DeathPending；
- Context 的 source actor、target actor、source ASC、victim ASC 与传入对象完全一致；
- source 与 target 同世界；
- source 为有效 APlayerCharacter，Combat Team 精确为 Team.Player，且未 Dead；
- target 为当前 this，未 Dead；DeathPending 只接受与 Context 状态一致的结果。

通过校验后：

- 触发 Player Execution Camera Shake；
- 读取 Enemy Execution Hit-Stop；
- 复用共享 ImpactSound 与 ImpactBloodSystem；
- Hit-Stop 参数必须 finite，duration > 0，time dilation 在 (0, 1]；非法值只跳过 Hit-Stop，不回退 Big，也不阻止 sound/blood；
- profile 缺失或类型错误时安全 no-op，不回滚 Health、Context、Release 或 Ability cleanup。

普通 HandleCombatImpactFeedback 必须改为调用共享 helper，同时保持：

- Player tier attacker shake 在 profile 缺失前的既有触发顺序；
- None/Invalid 到 Small 的普通 fallback；
- Sound 的 ImpactPoint finite 检查和 ActorLocation fallback；
- Blood 的精确 target actor、finite point、finite 且非零 ImpactNormal 检查；
- 所有既有普通 Combat.HitFeedback 断言。

### OnHealthAttributeChanged 拦截点

在 AEnemyCharacter::OnHealthAttributeChanged 中统一计算：

    const bool bInExecutionHitScope =
        ActiveVictimExecutionAbility.IsValid()
        && ActiveVictimExecutionAbility->IsInAuthorizedHitScope();

致死分支和非致死分支调用 HandleCombatImpactFeedback 前都必须判断 !bInExecutionHitScope。这样覆盖致死与非致死两个时序，避免普通 Shake/Hit-Stop/Sound/Blood 先于 Execution feedback。

非致死分支的 TriggerHitFeedbackOverlay 保留。Health、DeathPending、Dead、Poise、StanceBreak、Reaction Event 和既有状态所有权不改变。

### Resolver 时序

在 FMeleeHitResolver::TryResolveHit 中将 FExecutionHitScopeGuard 放入明确的嵌套作用域：

1. 保留现有 source/target/team/dead/invulnerable/Context 授权检查。
2. 在嵌套 scope 内应用 GE。
3. 保留 GE 失败和未确认致死的现有语义。
4. 当 bLethal && !bPendingConfirmed 时，设置 SetGESuccess(false)，在嵌套 scope 内直接 return false；依靠 RAII 析构执行 AbortHitScope，外层不得触发 Execution feedback。
5. scope 正常析构后，只有 GE 成功且 Context 状态为 NonLethal 或 DeathPending 时，调用 TargetEnemy->HandleExecutionImpactFeedback 一次。
6. 反馈失败不得改变 Health、Context、Release 或 Ability cleanup。
7. 不新增 Context 字段、不新增伤害路径；Exactly-Once 继续由现有 Context 状态转换、bDamageEventConsumed 和 IsHitAuthorized 保证。

### 合成 HitResult 法线

在 PlayerFrontExecutionAbility.cpp 和 PlayerBackstabExecutionAbility.cpp 的 Execution Hit 构造处补充有限的 target-to-attacker ImpactNormal：

    const FVector TargetToAttacker =
        PlayerCharacter->GetActorLocation() - TargetActor->GetActorLocation();
    HitRequest.HitResult.ImpactNormal =
        TargetToAttacker.IsNearlyZero()
            ? FVector::ZeroVector
            : TargetToAttacker.GetSafeNormal();

只修改执行命中的合成 FHitResult，不改变普通近战轨迹、Snap、距离、角度、Montage、Tag 或 Release。退化为零向量时 Blood 通道安全 no-op。

## 执行顺序、所有权与停机条件

    Outer: ue-stage-workflow
    Primary: ue5-cpp-gameplay
    Support: ue5-debug-validation
    Route reason: native GAS/DataAsset feedback and synchronous execution-scope lifecycle change
    Execution route: manual/out-of-band Gemini
    Implementation executors: Gemini

执行顺序：

1. 增加 typed Execution profile 类型和默认值。
2. 增加 Player Execution shake resolver/入口。
3. 抽取 Enemy 三通道 helper，接入普通路径并保持行为等价。
4. 增加 Enemy Execution 入口和双分支普通反馈拦截。
5. 重排 Resolver 的嵌套 scope 与析构后 dispatch。
6. 补 Front/Backstab ImpactNormal。
7. 新增专项 Automation 并执行静态自查。

所有共享文件均为 Contract owner: Main，implementation writer: Gemini：

- CombatFeedbackDataAsset.*：只允许新增两个结构、Execution 字段和构造默认值；不得改变 tier getter。
- PlayerCharacter.*：只允许新增 Execution 入口和 resolver；不得改变现有 shake 生命周期。
- EnemyCharacter.*：只允许新增 Execution 入口、三个私有 channel helper 和 OnHealth 的 scope 条件；普通路径必须等价。
- MeleeHitResolver.cpp：只允许调整 TryResolveHit 的嵌套 scope 和析构后 dispatch。
- Front/Backstab cpp：只允许补 Execution 合成 HitResult 的 ImpactNormal。

Gemini 不改文档、Content、Config、Editor 状态，不编译、不运行 PIE、不提交。首次切片交付时按本地规则优先派发一个全新只读实施自审；后续窄修复直接单兵复核。

立即回交 Main 的条件：

- 需要修改 ExecutionLockContext.*、新增 Tag/Input/Config/Build.cs、第二伤害路径、公共通用反馈服务或未列文件；
- 需要保存或修改 manifest 外 Content、Blueprint、AnimBP、Montage、地图或 imported/read-only asset；
- 需要伪造固定 ImpactNormal 才能驱动 Blood；
- 同一静态、编译、Automation 或运行时根因在一次证据驱动修复及一次针对性复验后仍失败；
- 普通 helper 抽取导致既有 tier fallback、计数器、位置兜底或用户可见行为变化。

## 测试与验证矩阵

新增 Source/PolyQuest/Private/Tests/ExecutionImpactFeedbackAutomationTests.cpp，使用现有 FCombatAutomationFixture、Execution fixture 和计数器，不新增生产测试 seam。至少覆盖：

- Front 与 Backstab；
- NonLethal 与 DeathPending；
- Player shake、Enemy Hit-Stop、ImpactSound、ImpactBlood exactly-once；
- 第二次 canonical Hit、重复 Release、重复 Resolver 请求不增加任何计数；
- GE 失败、未授权/foreign Context、错误 source/target/ASC、错误 team、Dead/Destroyed、未确认致死不触发反馈；
- Player Execution class 缺失时不回退 Big；
- Enemy Execution Hit-Stop 的 zero、negative、NaN、infinity、dilation > 1；
- 缺失或错误 profile 安全 no-op；
- 有效 HitResult 的 actor/point/normal；
- actor 不匹配、point 非 finite、normal 为 zero 或非 finite 时跳过 Blood；
- Sound 的 ImpactPoint fallback；
- 普通 Small/Big/Launch Combat.HitFeedback 行为不回归。

保留并回归：

- ExecutionHitNotify
- ExecutionLethalRecovery
- ExecutionLockIn
- ExecutionReleaseOutcomes
- ExecutionVictimPresentation
- FrontExecution
- Backstab
- Combat.HitFeedback

用户门禁：

1. Main 对批准 C++ 路径执行 Rider lint_files 或 get_file_problems、轻量静态审查和 git diff --check。
2. 用户手动编译 PolyQuestEditor (Development Editor)。
3. 用户对两个 manifest profile 做 Editor readback，确认新字段、初始值、共享音效/血液引用及无 manifest 外保存。
4. 用户运行 focused Automation。
5. 用户在 Scene01 PIE 验证 Front/Backstab 的 NonLethal、DeathPending、震屏、顿帧、音效、血液位置，以及重复 Hit/Release 不重复反馈。
6. 不将静态、编译、Editor readback、Automation、PIE、视觉、网络或 packaging 证据互相替代。

最终 Review：

- Main 按 ue-strict-review 做一次独立、缺陷优先 Fresh Review。
- 第一批限定批准 diff，并在 code-review-graph 基线匹配时执行一次影响雷达。
- 第二批只读批准 diff、直接一跳 caller/callee 和具体生命周期疑点，必要时至多一次定向 CodeGraph。
- 第二批后硬停止，不进行无目标漫游或自动追加 adversarial review。

## 文档、债务与提交边界

- 本阶段开始时只由 Main 替换 plan.md；D2D 历史记录保留，不重复归档。
- D2A/D2B/D2C/D2D 独立 compile/readback debt、D2A-VERTICAL-SURFACE、Debt-REC-05A1-03-RET-Teardown 继续保留，不因 E 自动关闭。
- 用户-owned authored WIP 不构成 clean-checkout fixture；不自动迁移、保存或暂存任何 .uasset/.umap。
- 实现、用户验证、Main Fresh Review 和债务交接完成后，才追加 E 的 ROADMAP-archive.md closeout，并按实际结果更新 ROADMAP.md。
- 候选提交只包含批准 Source/Test 路径及 Main 收口文档，排除全部 Content、用户 Config、Blueprint、AnimBP、Montage、地图和其他 WIP；提交必须等待用户明确 approval。

## 已冻结默认值

- 采用三通道私有 helper 复用方案。
- 致死与非致死两个 OnHealthAttributeChanged 分支都拦截普通 impact feedback，保留 Overlay 与既有状态逻辑。
- 未确认致死在嵌套 FExecutionHitScopeGuard 作用域内早退，绝不触发 Execution feedback。
- Player Execution Camera Shake 源码默认为空，Editor 初始值由用户设为 Big 对应类。
- Enemy Execution Hit-Stop 源码默认 0.05 秒和 0.03，Editor 初始值由用户确认与 Big 一致。
- Execution 永不映射为 EHitReactionTier，永不使用 Big fallback。
- 不新增 ExecutionFeedbackComponent、GameplayCue、Gameplay Tag、Input、网络、回滚、通用反馈服务或 Boss Grab/Throw 契约。

## 收口记录（2026-09-05）

- **实际结果**：批准的 10 个 Source/Test 路径已完成 Execution Impact Feedback v1。授权 `Hit` 在 `NonLethal` 与 `DeathPending` 成功结算后各 dispatch 一次；Player 使用独立 Execution Camera Shake，Enemy 使用 Execution Hit-Stop，并复用既有 ImpactSound/ImpactBlood 通道。普通 `OnHealthAttributeChanged` 反馈在授权 Hit scope 内被拦截，`VictimStart -> Hit -> Release`、唯一 `FMeleeHitResolver -> Damage GameplayEffect` 路径、Release 结果和 Ability cleanup 未改变。
- **变更边界**：实际变更与批准列表一致：两个 CombatFeedbackDataAsset 文件、PlayerCharacter 头/源、EnemyCharacter 头/源、`MeleeHitResolver.cpp`、Front/Backstab Execution Ability 两个 cpp，以及 `ExecutionImpactFeedbackAutomationTests.cpp`。未修改 `ExecutionLockContext.*`、Gameplay Tags、Input、Config、Build.cs、Blueprint、AnimBP、Montage、地图或任何 `.uasset/.umap`。
- **用户验证证据**：用户确认 focused Automation（`PolyQuest.Combat.ExecutionImpactFeedback`）与 Scene01 PIE 通过，覆盖 Front/Backstab 的命中反馈和普通反馈不重复叠加。该确认不扩展为网络、包装或 clean authored baseline 证明。
- **执行者回交证据**：Gemini 报告 Visual Studio `PolyQuestEditor (Development Editor)` 编译、Rider 对批准 C++ 路径零 Error，以及两个反馈 DataAsset 的 Editor readback 通过；这些保留为执行者证据，不改写为 Main 独立编译/readback 收据。
- **Main Fresh Review**：Main 按 `ue-strict-review` 完成两批有界、缺陷优先审查；第一批核对批准 diff 与影响范围，第二批定向核对 `TryResolveHit`、Enemy 双分支拦截、三通道 helper、Execution dispatch、Player shake resolver、合成 `ImpactNormal` 与专项 Automation，第二批后硬停止。`code-review-graph` 基线与 `3dff750` 匹配，定向 CodeGraph 只作一跳导航；当前批准范围内未发现 P0/P1/P2 blocker。`git diff --check` 在源码审查阶段通过，文档收口后另行复核。
- **剩余债务**：D2A/D2B/D2C/D2D 的独立 Main/user compile/readback 收据、`D2A-VERTICAL-SURFACE`、`Debt-05A1-D2C-OwnershipMatrix` 与 `Debt-REC-05A1-03-RET-Teardown` 继续按各自关闭条件保留；E 的用户-owned authored profile/音效/血液/Camera Shake 资产仍不是 clean-checkout fixture。未将这些债务误报为本阶段 blocker。
- **文档与提交边界**：本次收口同步 `plan.md`、`ROADMAP.md`、`ROADMAP-archive.md`、`ARCHITECTURE.md` 和（仅在状态确实需要时）`README.md`；提交只包含批准的 10 个 Source/Test 路径及上述 Main 文档，明确排除 `AGENTS.md`、全部 `Content/**`、用户 Config、Blueprint、地图和其他 WIP。
- **下一任务判断**：E 已完成后，产品主路线的下一独立执行切片为 `TODO-03C: Ranged Enemy v1`。当前不新增无明确关闭条件的泛化“健康度审查”；若优先清债，应另立只处理已命名 compile/readback、teardown fixture、文档漂移和明确生命周期 blocker 的窄阶段，并先冻结其证据与关闭门禁，不改玩法范围。
