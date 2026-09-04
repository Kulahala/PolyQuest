# TODO-05A1-D2A：Execution Handshake Snap Alignment v1（实施与收口记录）

## 阶段状态与基线

- 状态：Source/Test 实施完成；用户已确认专项 Automation 与 Scene01 PIE 通过；Main 文档收口完成并已提交，工作树仍保留用户-owned WIP。
- 日期：2026-09-04。
- 仓库：E:\GameDevelop\PolyQuest。
- 实施前基线：`main @ 5c608a9da280a3532599dea547d5323c89e34515`，相对 `origin/main` ahead 4。
- 阶段提交：D2A Source/Test 与 Main 收口文档已提交；具体提交号见 Git history（提交标题为 `[Feature] 执行握手 Snap 对齐与回滚`）。
- 工作树：存在用户-owned WIP；状态数量不构成批准集合。所有未列入本计划的变更必须保留，不得借本阶段清理或回滚。
- 本阶段实施父 HEAD：`5c608a9da280a3532599dea547d5323c89e34515`；D2A Source/Test 与 Main 收口文档已纳入阶段提交。
- `REC-05A1-03-RET` closeout 已写入 `ROADMAP-archive.md`，替换本文件不会丢失其历史证据。
- `Content/**` 中的执行 GA、Montage、Sequence、AnimBP 和其他 authored/imported 资产仍归用户所有；本阶段不保存、不迁移、不暂存。

## 目标与成功标准

本阶段回答一个运行时问题：Front Execution 与 Backstab 在既有成对握手成功后，能否通过一次确定性的玩家位置/朝向 Snap 完成对齐，并在环境阻挡时安全回滚、清速、Release 受害者，而不破坏现有锁定、命中和清理合同。

成功标准：

1. Front/Backstab 都使用同一份纯数学 Snap Helper；执行路径不再依赖 Motion Warping、接地判断或执行专用测试 seam。
2. 合法 Snap 的最终位置和朝向分别在 `1.0 cm` 与 `1.0 deg` 容差内；Yaw 比较正确处理 `180/-180` 回绕。
3. Sweep 不因受害者自身胶囊体而误判阻挡；真正的环境 blocking hit fail-closed，并恢复原始 Transform 与速度。
4. Snap 失败沿用既有 formal Release/`EndAbility()` 清理，受害者不会残留 `MOVE_None`；本阶段不在 Victim Ability 中新增移动锁逻辑。
5. `ExecutionSnapDistance` 超出 Ability 的 `[MinExecutionDistance, MaxExecutionDistance]` 时，在 `CanActivateAbility()` 和 `ActivateAbility()` 两个入口均 fail-closed，不能进入 Commit、握手或 Hit 阶段。
6. 普通攻击现有 Motion-Warp target 不被执行 Ability 的结束路径清除；既有 `VictimStart -> Hit -> Release`、`FMeleeHitResolver -> Damage GameplayEffect` 唯一路径和 exactly-once 语义不变。
7. 已有的 Automation、Scene01 PIE、静态检查和 Main 的一轮 `ue-strict-review` 均按证据类型记录；本阶段未形成独立编译/执行资产 readback 收据，强制中断/销毁/UnPossess/Teardown 也不写成已验证。

## 工具路线与责任

- Outer：`ue-stage-workflow`。
- Primary：`ue5-cpp-gameplay`。
- Support：`ue5-debug-validation`。
- Route reason：这是 Native GAS/Ability/DataAsset 合同和 Automation 的窄垂直切片，不涉及 Blueprint graph 或 authored asset 写入。
- Execution route：`manual/out-of-band Gemini`。
- Contract owner：Main/Codex。
- Implementation executor：Gemini（`manual/out-of-band`）；Codex 子代理委派数：0。
- Main 负责范围冻结、公共合同、`plan.md`、静态门禁、验证解释、Fresh Review、文档收口、staging 和 commit 准备。
- Gemini 只负责本计划列明的 Source/Test 实现和严格实施自审；不得改变架构、所有权、公共 API、ASC/Tag/Input 合同或范围。
- 用户负责 Unreal Editor 只读 readback、Visual Studio `PolyQuestEditor (Development Editor)` 编译、Automation/PIE 和最终 commit approval。

## 冻结运行时合同与接口

### 不变合同

- `UPlayerFrontExecutionAbility` 与 `UPlayerBackstabExecutionAbility` 继续是独立的 `InstancedPerActor`、`ServerOnly` Ability；身份 Tag、输入优先级、Lock-On 原始目标和 Backstab 激活时朝向快照不变。
- `ExecutionLockContext`、Activation Token、Avatar/Actor/ASC/动画身份校验、VictimStart/Hit/Release 时序、`FMeleeHitResolver` 唯一路径、exactly-once 和既有幂等 `EndAbility()` 清理顺序不变。
- `MOVE_None` 的设置和恢复仍由 `EnemyVictimExecutionAbility` 所有；本阶段不修改 `EnemyVictimExecutionAbility.*`，只增加跨 Ability 集成断言。
- 不新增第二条伤害路径、共享处决基类、全局移动锁、连续追踪、父级/模糊 Tag 匹配、网络/多人合同或通用能力框架。

### 数据与纯数学 Helper

`UMeleeWeaponDefinition` 新增 `ExecutionSnapDistance`：

- 默认值冻结为 `190.0f` cm，依据现有执行 `WarpStopDistance=190cm` 合同。
- `ClampMin=1.0`；运行时要求有限且严格大于零。
- 不拆分 Front/Backstab 两个距离字段；不新增 Player getter，复用 `UWeaponEquipmentComponent::GetEquippedMainHandMelee()`。

新增公开但无 UObject 依赖的 Helper：

```cpp
enum class EExecutionSnapSide : uint8
{
    Front,
    Backstab
};

struct POLYQUEST_API FExecutionSnapAlignment
{
    static bool IsSnapDistanceValid(float SnapDistance);

    static bool TryBuildTransform(
        const FVector& PlayerLocation,
        const FVector& TargetLocation,
        const FVector& TargetForward,
        float SnapDistance,
        EExecutionSnapSide Side,
        FTransform& OutTransform);
};
```

Helper 合同：

- 每次调用先清零 `OutTransform`；任一输入为 NaN/Inf、距离无效、目标前向 XY 为零或方向未知时返回 `false`。
- `TargetForward` 只取 XY 并归一化；不访问 World、Actor、ASC、Ability 或碰撞系统。
- Front 位置为 `TargetLocation + Forward2D * Distance`，玩家朝向 `-Forward2D`。
- Backstab 位置为 `TargetLocation - Forward2D * Distance`，玩家朝向 `+Forward2D`。
- 输出 Z 使用 `PlayerLocation.Z`，不保留目标 Z；这是为避免玩家与目标存在高度差时 Snap 后胶囊体下陷并被碰撞驳回的已确认实现决策。

### Ability Snap 与回滚合同

- 在 `CanActivateAbility()` 和 `ActivateAbility()` 中，从当前主手近战 Weapon Definition 读取 Snap 距离并校验 Ability 的最小/最大执行距离；无有效武器或越界均在 `CommitAbility()`、Victim 握手和 Snap 前失败。
- 握手成功后保存玩家原始 Transform；使用 Helper 构造目标 Transform，再以 `SetActorLocationAndRotation(..., bSweep=true)` 尝试一次移动。
- Sweep 前在玩家 Capsule 上临时调用 `IgnoreActorWhenMoving(TargetActor, true)`；保存原有 Ignore 状态，成功、失败和所有提前返回路径都恢复到调用前状态。若仍返回目标自身 Hit，不将其分类为环境阻挡；其他 blocking hit 失败。
- 不能只依赖 Sweep 返回值：必须读取最终 Actor Transform，位置误差使用 `FVector::Dist`/等价长度比较，Yaw 误差使用 `FMath::FindDeltaAngleDegrees`，并分别与 `1.0f cm`、`1.0f deg` 容差比较。
- Snap 失败时以无 Sweep、`ETeleportType::TeleportPhysics` 恢复原始 Transform，随后调用玩家 `CharacterMovementComponent->StopMovementImmediately()`，再走既有 formal Release/`EndAbility()` 清理；成功路径不额外改变移动手感。
- 从两个执行 Ability 的 `EndAbility()` 移除执行路径对 `ClearMeleeMotionWarpTargets()` 的调用；执行路径不再创建或拥有普通攻击的 Warp target。
- 删除两个 Ability 中仅用于旧 Motion-Warp 配置的 `SetTestMotionWarpConfig` 及其专用测试 seam；`bUseMotionWarping`、`WarpTargetName`、`WarpStopDistance`、`MaxWarpAngleDegrees` 已按用户明确决定从两个执行 Ability 删除，不恢复兼容字段，也不把它们作为当前序列化合同。
- D2A v1 不加入 Ground Trace 或复杂坡地贴地；以玩家当前 `Z` 为高度基准，假设玩家胶囊体与目标处于可接受的共享/近似平坦基准面。复杂坡地、台阶和高低差仍是后续独立验证范围。

## 有序执行切片

### Slice 1：DataAsset 字段与纯数学 Helper

- 在 `MeleeWeaponDefinition` 增加字段及有限正值校验。
- 新增 `ExecutionSnapAlignment` 头/源文件，实现 Front/Backstab 几何、有限性 fail-closed 和输出清零。
- 先完成独立数学 Automation，再进入 Ability 集成；不触碰资产包。

### Slice 2：Front Execution 集成

- 只在现有 Front 激活入口、Snap/回滚私有路径和 `EndAbility()` 清理点接入 Helper。
- 保留目标检查、`CommitAbility()` 顺序、LockContext/Token、VictimStart/Hit/Release、Montage/Task 和 Resolver 路径。
- 加入 Snap 距离入口互锁、临时 Capsule Ignore、最终 Transform 容差、阻挡回滚和清速。

### Slice 3：Backstab 集成

- 采用与 Front 相同的 Snap/回滚合同，但保持 Backstab 独立的后方几何和激活时目标朝向快照。
- 不把两者合并为新的处决基类，不改变输入优先级、目标快照或命中验证。

### Slice 4：集成测试、静态门禁与 handoff

- 新增/更新聚焦 Automation，覆盖数学、入口范围、阻挡、回滚、释放、Ignore 状态和普通攻击 Warp target 保留。
- Gemini 读取最终 diff，完成严格实施自审并回交 changed paths、静态结果、未运行用户门禁和剩余风险。
- Main 在用户证据齐全后执行一次 `ue-strict-review`，再做债务交接和文档/提交判断。

## 批准修改路径

允许修改：

- `Source/PolyQuest/Public/Combat/Equipment/MeleeWeaponDefinition.h`
- `Source/PolyQuest/Private/Combat/Equipment/MeleeWeaponDefinition.cpp`
- `Source/PolyQuest/Public/Combat/Execution/ExecutionSnapAlignment.h`
- `Source/PolyQuest/Private/Combat/Execution/ExecutionSnapAlignment.cpp`
- `Source/PolyQuest/Public/AbilitySystem/Abilities/PlayerFrontExecutionAbility.h`
- `Source/PolyQuest/Private/AbilitySystem/Abilities/PlayerFrontExecutionAbility.cpp`
- `Source/PolyQuest/Public/AbilitySystem/Abilities/PlayerBackstabExecutionAbility.h`
- `Source/PolyQuest/Private/AbilitySystem/Abilities/PlayerBackstabExecutionAbility.cpp`
- `Source/PolyQuest/Private/Tests/ExecutionSnapAlignmentAutomationTests.cpp`
- `Source/PolyQuest/Private/Tests/FrontExecutionAutomationTests.cpp`
- `Source/PolyQuest/Private/Tests/BackstabExecutionAutomationTests.cpp`

明确排除：

- `EnemyVictimExecutionAbility.*`、`CombatAutomationFixture.*`、`PlayerCharacter.*`、`ExecutionLockContext.*`、`MeleeMotionWarping.*`、`WeaponEquipmentComponent.*`。
- `Build.cs`、Gameplay Tags、Input、Config、Blueprint、AnimBP、Montage、地图和全部 `Content/**`。
- 实施者不修改 `ARCHITECTURE.md`、`ROADMAP.md`、`ROADMAP-archive.md`、`README.md`；这些文件仅由 Main 在验证后执行本次文档收口。
- 任何未列出的公共 API、资产迁移、Motion-Warp 全局清理或生命周期重构。

## 验证矩阵

### Main 静态门禁

- 精确扫描旧 Motion-Warp 调用、旧字段运行时读取、Snap 距离入口校验、临时 Ignore 恢复和 `FindDeltaAngleDegrees` 使用。
- 读取最终变更 Source 与一跳 callers/callees；运行 `git diff --check`。
- 对所有变更 C++ 文件运行 Rider `lint_files` 或 `get_file_problems`（如连接可用）。
- CodeGraph 仅做定向符号/调用复核；`code-review-graph` 若构建 SHA 仍落后，记录 stale-coverage fallback，不将图结果当作运行时证据。

### Focused Automation

- `PolyQuest.Combat.ExecutionSnapAlignment`：Front/Backstab 位置、玩家 Z 基准、朝向、有限性、零向量、未知方向、1 cm/1 deg 容差和角度回绕。
- `ExecutionSnapDistance` 小于最小值、大于最大值、NaN/Inf 或无效 Weapon Definition 时，Front/Backstab 两处入口均不 Commit、不握手、不进入 Hit。
- 握手成功后目标已由既有 Victim Ability 进入 `MOVE_None`；取消/阻挡失败经 formal Release 后恢复 `MOVE_Walking`，Snap 不新增该移动锁。
- 使用临时 `AActor + UBoxComponent` 制造环境 blocking hit，验证原始 Transform 回滚、速度清零、Ignore 状态恢复和无 reservation 残留；不改地图。
- 验证普通攻击已有 Motion-Warp target 不被执行 Ability 清除；无 Montage/无伤害/无重复命中副作用。
- Front/Backstab 回归保持激活时目标/朝向快照和现有 `FMeleeHitResolver` exactly-once 断言。

### 回归套件

- `FrontExecution`
- `Backstab`
- `ExecutionLockIn`
- `ExecutionLethalRecovery`
- `ExecutionReleaseOutcomes`
- `ExecutionVictimPresentation`
- `ExecutionHitNotify`
- `PlayerMeleeMotionWarping`
- `PolyQuest.Equipment.TransactionMatrix`

### 用户门禁

- 用户手动编译 `PolyQuestEditor (Development Editor)`。
- 用户-owned 执行 Weapon/GA/Montage readback 是适用门禁，但本阶段未形成新的独立收据；不保存、不迁移、不提交资产。
- Scene01 PIE 验证 Front/Backstab 正常对齐、命中、VictimStart/Hit/Release、阻挡失败恢复和普通攻击 Motion-Warp 不回归。
- 用户提供的编译、Automation、Editor、PIE 证据分类型记录；静态检查、图结果和代码审查不能替代其中任何一项。

## 风险、债务与收口

- **D2A-VERTICAL-SURFACE（非阻塞）**：实现保留玩家当前 `Z`，解决了本阶段发现的玩家/目标高度差导致背刺 Snap 下陷问题，但不解决坡地、台阶和复杂高低差的贴地问题。关闭条件是后续独立 Ground Alignment/坡地验证阶段；在关闭前不得声称全地形支持。
- **Debt-REC-05A1-03-RET-Teardown（非阻塞）**：强制中断、销毁、UnPossess、Teardown 的 PIE 证据仍按原债务保留；本阶段普通阻挡回滚测试不关闭它。
- **旧 Motion-Warp 字段迁移（条件性）**：四个执行专用字段已按用户决定删除，当前运行未报告错误；若后续发现历史资产确需字段迁移或出现加载问题，另立资产清单、Reference/readback 和迁移批准阶段，不在 D2A 回补兼容层。
- Main 已完成 debt-handoff check，并将仍开放的地形限制、RET Teardown 债务和后续条件性决策同步到 `ROADMAP.md`。
- 阶段提交仅包含批准的 11 个 Source/Test 路径和 Main 收口文档；所有其他 WIP 原样排除。

## Gemini Handoff Prompt（实施前交接文本）

以下保留实施前发给 Gemini 的边界文本，用于追溯；实际实施结果、用户验证和收口结论见下文。

```text
你是 PolyQuest 的 TODO-05A1-D2A Implementation Executor。

仓库：E:\GameDevelop\PolyQuest
基线：main @ 5c608a9da280a3532599dea547d5323c89e34515
活动计划：E:\GameDevelop\PolyQuest\plan.md
路线：Outer=ue-stage-workflow；Primary=ue5-cpp-gameplay；Support=ue5-debug-validation
执行方式：manual/out-of-band Gemini
Contract owner：Main/Codex；Implementation writer：Gemini

批准 Source/Test 路径（绝对路径）：
- E:\GameDevelop\PolyQuest\Source\PolyQuest\Public\Combat\Equipment\MeleeWeaponDefinition.h
- E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\Combat\Equipment\MeleeWeaponDefinition.cpp
- E:\GameDevelop\PolyQuest\Source\PolyQuest\Public\Combat\Execution\ExecutionSnapAlignment.h
- E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\Combat\Execution\ExecutionSnapAlignment.cpp
- E:\GameDevelop\PolyQuest\Source\PolyQuest\Public\AbilitySystem\Abilities\PlayerFrontExecutionAbility.h
- E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\AbilitySystem\Abilities\PlayerFrontExecutionAbility.cpp
- E:\GameDevelop\PolyQuest\Source\PolyQuest\Public\AbilitySystem\Abilities\PlayerBackstabExecutionAbility.h
- E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\AbilitySystem\Abilities\PlayerBackstabExecutionAbility.cpp
- E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\Tests\ExecutionSnapAlignmentAutomationTests.cpp
- E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\Tests\FrontExecutionAutomationTests.cpp
- E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\Tests\BackstabExecutionAutomationTests.cpp
批准资产路径：无；批准文档路径：仅只读 E:\GameDevelop\PolyQuest\plan.md。

允许修改的生产函数边界：
- `UMeleeWeaponDefinition::IsValidWeaponDefinition`。
- `FExecutionSnapAlignment::IsSnapDistanceValid`、`TryBuildTransform`。
- `UPlayerFrontExecutionAbility::CanActivateAbility`、`ActivateAbility`、
  `EndAbility`，以及为接入同一合同所需的现有 `ValidateTargetPrerequisites`、
  `CheckFrontGeometry`、`SendFormalReleaseToVictim` 路径。
- `UPlayerBackstabExecutionAbility::CanActivateAbility`、`ActivateAbility`、
  `EndAbility`，以及对应的 `ValidateTargetPrerequisites`、
  `CheckBackstabGeometry`、`SendFormalReleaseToVictim` 路径。
- Automation 文件只允许增加/调整本计划列出的 D2A 断言和夹具使用。

先读取 AGENTS.md 和当前 plan.md。只实现计划批准的 D2A Slice 1-4：
MeleeWeaponDefinition 的 ExecutionSnapDistance、纯数学
FExecutionSnapAlignment、PlayerFrontExecutionAbility、
PlayerBackstabExecutionAbility，以及列明的三个 Automation 测试文件。

冻结合同：
1. Front/Backstab 保持独立 ServerOnly InstancedPerActor Ability；不改身份
   Tag、Input、LockContext、Activation Token、目标/朝向快照、
   VictimStart -> Hit -> Release、FMeleeHitResolver、Damage GE 或 exactly-once。
2. Snap 使用目标相对位置：Front 在目标前方，玩家朝向目标；Backstab 在目标
   后方，玩家朝向目标。只取 Forward XY，输出 Z 使用 PlayerLocation.Z；该选择用于避免玩家/目标高度差造成 Snap 下陷。
3. Snap 距离必须在 MinExecutionDistance 与 MaxExecutionDistance 之间；
   CanActivateAbility 和 ActivateAbility 都要在 Commit/握手前 fail-closed。
4. Sweep 前临时 Capsule IgnoreActorWhenMoving(TargetActor,true)，保存并恢复
   原 Ignore 状态。目标自身 Hit 不算环境阻挡，其他 blocking hit 失败。
5. 成功必须检查最终位置和旋转：位置容差 1.0cm，Yaw 容差 1.0deg，Yaw 差
   必须使用 FMath::FindDeltaAngleDegrees。
6. 失败用无 Sweep + ETeleportType::TeleportPhysics 恢复原始 Transform，调用
   CharacterMovementComponent->StopMovementImmediately()，再走既有 formal
   Release/EndAbility 清理。MOVE_None/MOVE_Walking 仍由 EnemyVictimExecutionAbility
   所有，本阶段不得修改该文件。
7. 从两个执行 Ability 的 EndAbility 移除 ClearMeleeMotionWarpTargets()，
   不能清掉普通攻击拥有的 Warp target。
8. 删除仅用于旧 Motion-Warp 配置的 SetTestMotionWarpConfig 测试 seam；按用户决定
   删除 bUseMotionWarping、WarpTargetName、WarpStopDistance、
   MaxWarpAngleDegrees 等执行专用 UPROPERTY，不恢复序列化兼容字段；运行时不得
   读取或写入这些旧字段。不得编辑任何 .uasset/.umap。

允许路径之外的 Source、公共 API、Tag/Input、Config、资产、Build.cs、
EnemyVictimExecutionAbility、CombatAutomationFixture 或文档需求都必须停止并
回报 Main，不能绕过计划扩展范围。禁止调用 UBT/Build.bat、启动或写入 live
Editor、暂存、提交、格式化无关文件或回滚用户 WIP。

执行顺序：先字段/Helper 和数学测试，再 Front，再 Backstab，再集成测试与静态
检查。完成前运行 git diff --check 和可用的 Rider 静态检查，读取最终 diff，
完成严格实施自审；每个根因最多一次有证据修复和一次定向复跑。

回交必须列出：changed paths、每个批准合同的静态证据、Automation/编译/Editor/
PIE 中未运行的门禁、严格自审 findings、未暂存未提交状态、垂直地形限制、
RET teardown 债务，以及明确没有修改 Content、资产、地图、Config 或文档。
```

## 实施与验证收口

### 实际变更路径

- `Source/PolyQuest/Public/Combat/Equipment/MeleeWeaponDefinition.h`
- `Source/PolyQuest/Private/Combat/Equipment/MeleeWeaponDefinition.cpp`
- `Source/PolyQuest/Public/Combat/Execution/ExecutionSnapAlignment.h`
- `Source/PolyQuest/Private/Combat/Execution/ExecutionSnapAlignment.cpp`
- `Source/PolyQuest/Public/AbilitySystem/Abilities/PlayerFrontExecutionAbility.h`
- `Source/PolyQuest/Private/AbilitySystem/Abilities/PlayerFrontExecutionAbility.cpp`
- `Source/PolyQuest/Public/AbilitySystem/Abilities/PlayerBackstabExecutionAbility.h`
- `Source/PolyQuest/Private/AbilitySystem/Abilities/PlayerBackstabExecutionAbility.cpp`
- `Source/PolyQuest/Private/Tests/ExecutionSnapAlignmentAutomationTests.cpp`
- `Source/PolyQuest/Private/Tests/FrontExecutionAutomationTests.cpp`
- `Source/PolyQuest/Private/Tests/BackstabExecutionAutomationTests.cpp`

没有修改 `EnemyVictimExecutionAbility.*`、`CombatAutomationFixture.*`、`PlayerCharacter.*`、`ExecutionLockContext.*`、`MeleeMotionWarping.*`、`WeaponEquipmentComponent.*`、Build.cs、Config、Blueprint、Montage、地图或任何 `Content/**` 资产。

### 已有证据

- **source/static**：Gemini 报告 Rider `get_file_problems` 对批准 C++ 路径为 0 errors；Main 完成最终限定范围 Fresh Review，确认 Snap、入口范围互锁、临时 Capsule Ignore 恢复、最终 Transform 容差、Yaw 回绕、阻挡回滚清速、formal Release 和普通攻击 Motion-Warp target 隔离；`git diff --check` 通过。
- **Automation**：用户确认 `PolyQuest.Combat.ExecutionSnapAlignment`、`PolyQuest.Combat.FrontExecution`、`PolyQuest.Combat.Backstab` 通过。当前记录不扩写为未运行的完整回归套件。
- **PIE/runtime**：用户确认 Scene01 中 Front/Backstab Snap 对齐、动画、命中和阻挡回滚行为通过。
- **未单独收据**：当前没有新的独立 `PolyQuestEditor (Development Editor)` 编译收据或本阶段执行 Weapon/GA/Montage 的 Editor readback 收据；因此不宣称这些门禁已在本阶段重新完成，也不宣称干净 authored baseline 或 packaging readiness。

### Main Fresh Review 结论

- 审查范围限定为上述 11 个 D2A Source/Test 路径及其一跳直接合同；`code-review-graph` 基于 `5c608a9` 的有界雷达给出覆盖提示，未作为运行时证明；定向 CodeGraph 用于确认 Ability/Equipment/Victim 调用关系。
- 未发现当前批准范围内的 P0/P1/P2 缺陷。图谱列出的测试缺口保留为覆盖提示，不改变用户已确认的 Automation/PIE 结论。

### 已确认设计决策

- `FExecutionSnapAlignment` 使用 `PlayerLocation.Z` 是有意设计：实际调试证明玩家与目标高度不一致时，使用目标 Z 会让玩家胶囊体下陷并被碰撞驳回；因此当前契约保留玩家高度。坡地、台阶和复杂高低差贴地仍未解决。
- `bUseMotionWarping`、`WarpTargetName`、`WarpStopDistance`、`MaxWarpAngleDegrees` 已按用户明确要求从两个执行 Ability 删除；当前不恢复序列化兼容字段。若未来出现历史资产迁移需求，另立资产阶段。
- `MinExecutionDistance`/`MaxExecutionDistance` 继续由 Ability 负责玩法触发窗口和安全边界，`ExecutionSnapDistance` 继续由近战 Weapon DataAsset 负责武器/动画吸附规格；D2A 不合并字段、不增加 Override。武器级触发窗口与 Snap 距离的统一另记为条件性 `TODO-05A1-D2D`，需要真实武器族差异、迁移和入口校验证据后再启动。
- StanceBreak 期间继续拒绝 Backstab。用户已确认当前 StanceBreak 是原地僵直/硬直 Montage，而不是跪地或倒地 Montage，因此“必须补齐跪地/倒地受害者 Montage”不再是后续兼容阶段的硬前置；本阶段仍不修改该行为，仍需先解决 StanceBreak 与 Victim 的生命周期交接。

### 本轮新增决策与后续阶段建议（2026-09-04）

- 建议安排条件性 `TODO-05A1-D2C: StanceBreak Backstab Compatibility v1`，而不是把行为变更塞回已收口的 D2A。当前源代码的两个明确门禁是 `PlayerBackstabExecutionAbility::ValidateTargetPrerequisites()` 对 `State.Status.Stunned` 的拒绝，以及 `EnemyVictimExecutionAbility::ValidateExecutionRequest()` 对 active `UEnemyStanceBreakAbility` 的拒绝。
- D2C 启动前必须冻结“仅允许 StanceBreak 所有的单一 `Stunned` 状态”与其他 Stunned 来源的区分，并定义显式的 StanceBreak -> Victim handoff。当前 `EnemyVictimExecutionAbility` 只在 Front 请求设置 `bHandoffFromStanceBreak`；而 `UEnemyStanceBreakAbility` 在检测到 VictimLocked 时会跳过自身的移动/Poise 恢复，若直接放开 Backstab，释放时可能遗留 `Poise == 0`。D2C 必须让 Backstab 获得同等且 exactly-once 的恢复所有权（或采用等价的明确状态），并覆盖取消、中断、目标销毁、UnPossess/Teardown。
- D2C 的首个表现门禁使用当前僵直 StanceBreak Montage 与现有 Backstab Victim Montage 做真实 PIE；不预设新增跪地分支。若该组合实际出现错位，再另立用户-owned 资产子切片。Front 优先级、`MOVE_None`/`MOVE_Walking`、Poise、AI 锁和 `VictimStart -> Hit -> Release` 均须保持单一所有权。
- 建议安排独立条件性 `TODO-05A1-D2D: Weapon-Authored Execution Range Contract v1`：将 `ExecutionMinDistance`、`ExecutionMaxDistance` 与 `ExecutionSnapDistance` 作为同一 `UMeleeWeaponDefinition` 的作者化规格，默认值保持 `0 / 250 / 190 cm`。Ability 仍保留不可作者化的有限性、`Min <= Snap <= Max` 和 Native hard cap 校验，避免 DataAsset 成为无保护的唯一裁判；现有 GA 字段迁移、旧资产读回、Automation 和 PIE 必须在该阶段单独闭合。D2D 不回溯修改 D2A。

### 债务与提交边界

- `D2A-VERTICAL-SURFACE`：玩家 Z 基准只能解决本阶段发现的高度差下陷问题，关闭条件是独立 Ground Alignment/坡地验证阶段。
- `Debt-REC-05A1-03-RET-Teardown`：强制中断、目标销毁、UnPossess、Teardown 的 PIE 收据仍缺失；关闭条件是可追溯用户 PIE receipt，或 Main 接受的 focused fixture/no-adoption。
- 文档收口已同步 `ROADMAP.md`、`ROADMAP-archive.md`、`ARCHITECTURE.md` 与 `README.md`；阶段提交只包含上述 11 个 Source/Test 路径和五份 Main 文档，所有用户-owned WIP 原样排除。
- 下一执行切片为 `TODO-05A1-D2B`；其职责是武器专属 Front/Backstab Montage 选择、激活时快照和装备切换锁，沿用本阶段 Snap 与 `VictimStart -> Hit -> Release` 合同。D2C（StanceBreak Backstab）与 D2D（Weapon-Authored Execution Range）均为有条件后续阶段，不属于本次提交的实现范围。
