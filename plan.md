# TODO-05A1-D2B：Weapon-Specific Execution Montage Selection v1（实施计划）

## 阶段状态与基线

- 状态：D2B 已实现并完成 Main 收口；用户已确认 Focused Automation 与 Scene01 PIE 通过，Main Fresh Review 未发现 P0-P2 blocker，等待显式 commit approval。
- 日期：2026-09-04。
- 仓库：`E:\GameDevelop\PolyQuest`；分支：`main`。
- 基线：`main @ 5cba1e2f42146082b7fee26d488ac1feec7e6d36`，相对 `origin/main` ahead 5。
- 工作树：321 条状态记录，包含用户-owned `Content/**`、`Config/**`、`AGENTS.md` 和其他 WIP；这些变更不是本阶段批准集合，必须原样保留。
- D2A 收口已写入 `ROADMAP-archive.md`；本阶段替换本文件不会丢失 D2A 历史证据。
- Archive preflight: PASS；替换本阶段 `plan.md` 前已确认 D2A 的日期化归档条目、基线、范围、验证与下一指针均存在。
- 阶段开始时仅有源码/文档/定向 CodeGraph 基线证据；收口证据与其来源在下方“D2B 实施与 Main 收口记录”中分层记录。

## 目标与成功标准

本阶段回答一个运行时问题：Front Execution 与 Backstab 是否能从当前主手近战 `UMeleeWeaponDefinition` 选择各自的玩家 Montage，并在激活时锁定该武器/Montage，避免执行期间换装或配置漂移破坏既有握手和命中合同。

成功标准：

1. `FrontExecutionMontage` 与 `BackstabExecutionMontage` 成为唯一的方向专属玩家 Montage 来源；缺失方向只拒绝该路线，不回退到 Unarmed 或 Ability 全局 CDO。
2. `CanActivateAbility()`、`ActivateAbility()` 和 `CommitAbility()` 使用同一只读 resolver；Commit 前复核当前主手与 Montage 身份，任何不一致均 fail-closed，不建立 Victim 握手。
3. Commit 成功后，Montage Task、动画身份校验、D2A Snap 距离读取和 `EndAbility()` 停止逻辑只使用激活快照，不在执行中重新选择 Montage。
4. 执行期间继续由既有 `State.Action.Attacking`/`CanSwapNow()` 阻止 `EquipWeapon()` 与 `TryEquipWorldPickup()`；不新增换装锁服务或 Gameplay Tag。
5. 既有 `VictimStart -> Hit -> Release`、`FMeleeHitResolver -> Damage GameplayEffect`、exactly-once、目标/朝向快照、formal Release 和幂等清理合同保持不变。
6. 现有 Front/Backstab 输入优先级不变；缺失方向 Montage 时能继续普通 Primary 路由。

## 冻结接口与行为

### Weapon DataAsset

- 在 `UMeleeWeaponDefinition` 增加以下可选字段，均默认 `nullptr`，使用 `EditDefaultsOnly`、`BlueprintReadOnly`、`Weapon|Execution` 分类：
  - `TObjectPtr<UAnimMontage> FrontExecutionMontage`
  - `TObjectPtr<UAnimMontage> BackstabExecutionMontage`
- 不改变 `IsValidWeaponDefinition()`；可选 Montage 缺失不使武器整体失效。现有 `ExecutionSnapDistance` 合法性校验保持不变，因此不修改 `MeleeWeaponDefinition.cpp`。

### Ability resolver 与快照

- 两个 Ability 各自提供同形状的私有 resolver，使用 `GetEquippedMainHandMelee()`，清空输出并对 Avatar、装备、主手近战定义及对应方向 Montage 做 fail-closed 检查：

```
bool TryResolveExecutionMontage(
    const FGameplayAbilityActorInfo* ActorInfo,
    const UMeleeWeaponDefinition*& OutWeaponDef,
    UAnimMontage*& OutMontage) const;
```

- 两个 Ability 各自增加以下 `Transient` 快照；`TObjectPtr<const UMeleeWeaponDefinition>` 语法已由 UE 5.8/项目现有代码核实：

```
UPROPERTY(Transient)
TObjectPtr<const UMeleeWeaponDefinition> ActiveExecutionWeaponDefinition;

UPROPERTY(Transient)
TObjectPtr<UAnimMontage> ActiveExecutionMontage;
```

### 生命周期与 Commit 顺序

- `CanActivateAbility()` 在现有 `Super`、Damage GE、距离/角度、目标和几何门禁中调用 resolver；不再读取 Ability 全局 `ExecutionMontage`。
- `ActivateAbility()` 在 Commit 前重新调用 resolver并写入快照；解析失败或快照无效直接走既有 `EndAbility()`，不得创建锁定会话或发送 Victim 请求。
- `CommitAbility()` 固定按以下顺序执行：
  1. `WITH_DEV_AUTOMATION_TESTS` 下的 `bTestForceCommitAbilityFailure` 优先返回失败；
  2. 检查两个快照有效；
  3. 用同一 resolver 读取当前主手/方向 Montage，并与快照做指针身份比较；
  4. 通过后才调用 `Super::CommitAbility()`。
- Commit 成功后，所有 Montage 播放、`Montage_IsActive`、Hit/VictimStart/Release 的动画身份校验和停止逻辑只使用 `ActiveExecutionMontage`。D2A Snap 使用 `ActiveExecutionWeaponDefinition->ExecutionSnapDistance`，不重新读取 live 武器。
- `EndAbility()` 保留既有 formal Release、delegate、Task 和 Montage 清理顺序；停止快照 Montage 后清空武器/Montage 快照。不得清理普通攻击拥有的 Motion-Warp target。

### 测试适配器与隔离

- 保留 `SetTestExecutionMontage()` 名称，仅在 `WITH_DEV_AUTOMATION_TESTS` 下实现为写入当前测试玩家 transient 主手武器的对应方向字段；不恢复 Ability 级 fallback。
- CDO、无 Avatar、无装备组件或无主手近战定义时安全静默返回。
- `FrontExecutionAutomationTests.cpp` 与 `BackstabExecutionAutomationTests.cpp` 的每个 Section 必须在 Ability 结束、清理 Handle 后把两个方向字段恢复为 `nullptr`，并在下一次配置前先清理；组合测试同时配置两个方向时也必须显式清理，禁止单 World 夹具状态串扰。
- 不新增“在同步 Commit 缝隙注入换装”的生产或公共测试接口；使用现有 force-fail seam、事件身份断言和 post-activation 字段变更覆盖可观测边界。

## 执行路线、切片与范围

- Outer：`ue-stage-workflow`
- Primary：`ue5-cpp-gameplay`
- Support：`ue5-debug-validation`
- Route reason：Native GAS、DataAsset 和现有 Automation 的窄垂直切片，不涉及 Blueprint 图、资产迁移或新模块。
- Execution route：`manual/out-of-band Gemini`。
- Contract owner：Main/Codex；Implementation executor：Gemini；Codex 子代理委派数：0。
- Main 负责范围冻结、公共合同、计划、静态门禁、验证解释、Fresh Review、文档、staging 和 commit gate；用户负责 Editor authoring/readback、手动编译、Automation、PIE 和最终 commit approval。
- Gemini 首次切片交付前优先派发 1 个无历史污染的只读子代理做严格实施自审；后续修复回路由 Gemini 单兵复核，该自审不替代 Main 的 `ue-strict-review`。

有序切片：

1. 在 `MeleeWeaponDefinition.h` 增加两个方向字段；在两个 Ability Header 中移除全局 `ExecutionMontage` CDO 字段，加入 resolver 声明、前向声明和 `Transient` 快照。
2. 在两个 Ability CPP 中接入 resolver、快照、Commit 复核，以及快照驱动的 Montage/Snap 使用；保持目标、锁、事件、伤害和清理路径。
3. 更新两个 Execution Automation 文件的配置适配器、方向选择/缺失回退/快照固定/换装闸门断言和 Section 复位。
4. Gemini 完成静态自审并回交证据；用户完成编译、Editor readback、Automation 和 Scene01 PIE 后，Main 做一轮限定范围 `ue-strict-review`。

批准修改路径：

- `E:\GameDevelop\PolyQuest\Source\PolyQuest\Public\Combat\Equipment\MeleeWeaponDefinition.h`
- `E:\GameDevelop\PolyQuest\Source\PolyQuest\Public\AbilitySystem\Abilities\PlayerFrontExecutionAbility.h`
- `E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\AbilitySystem\Abilities\PlayerFrontExecutionAbility.cpp`
- `E:\GameDevelop\PolyQuest\Source\PolyQuest\Public\AbilitySystem\Abilities\PlayerBackstabExecutionAbility.h`
- `E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\AbilitySystem\Abilities\PlayerBackstabExecutionAbility.cpp`
- `E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\Tests\FrontExecutionAutomationTests.cpp`
- `E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\Tests\BackstabExecutionAutomationTests.cpp`

明确排除：`MeleeWeaponDefinition.cpp`、`PlayerCharacter.*`、`WeaponEquipmentComponent.*`、`CombatAutomationFixture.*`、`EnemyVictimExecutionAbility.*`、`ExecutionLockContext.*`、`Build.cs`、Gameplay Tags、Input、Config、Blueprint、AnimBP、Montage、地图、全部 `Content/**` 及任何未列出的公共 API。需要未列出路径、资产、Tag、Input、Config 或生命周期规则时必须停止并返回 Main 决策。

## 验证矩阵与验收门禁

### Main 静态门禁

- 精确扫描旧 Ability 全局 `ExecutionMontage` 读取，确认只保留武器字段和活动快照路径。
- 核对 resolver 在 `CanActivateAbility()`、`ActivateAbility()`、`CommitAbility()` 的单源使用、快照清理、live identity 复核和 `ExecutionSnapDistance` 快照读取。
- 核对没有新增 Tag/Input/Build.cs/装备组件逻辑；运行 `git diff --check`。
- Rider 可用时对全部变更 C++ 文件执行 `lint_files` 或 `get_file_problems`；当前基线不把 Rider 不可用写成通过。
- CodeGraph 只做批准符号的一跳调用复核；`code-review-graph` 当前索引基于旧 SHA `55dc530`，只能记录为 stale-coverage fallback，不能作为运行时证据。

### Focused Automation

- Front/Backstab 使用不同 synthetic Montage 时分别选择正确字段；单方向缺失只拒绝对应 Ability。
- 缺失 Montage 时 `CanActivateAbility()` 与 `ActivateAbility()` 均不 Commit、不握手、不进入 Hit，并能回退普通 Primary。
- 强制 Commit 失败不建立 reservation；快照不完整或 live 武器/Montage 身份不一致时 fail-closed。
- 激活后修改 transient 武器字段不会切换活动 Montage；现有事件 OptionalObject/动画身份断言继续以激活快照为准。
- Ability 活跃期间直接 `EquipWeapon()` 和 `TryEquipWorldPickup()` 被拒绝，结束后标签、快照、reservation 和测试字段均清理。
- 保持现有 Front/Backstab 的目标快照、VictimStart/Hit/Release、单次伤害和生命周期回归断言。

### 用户门禁

- 用户在 Editor 中为实际候选武器 DataAsset 读取/作者化两个方向字段，并确认 Montage、统一 Hit Notify、Slot 与 AnimBP 关系；本阶段不预设资产路径，不自动迁移或保存未批准资产。
- 用户手动编译 `PolyQuestEditor (Development Editor)`。
- 用户运行 `FrontExecution`、`Backstab`、`ExecutionLockIn`、`ExecutionLethalRecovery`、`ExecutionReleaseOutcomes`、`ExecutionVictimPresentation`、`ExecutionHitNotify` 及相关装备交易回归。
- 用户在 Scene01 PIE 验证候选武器的方向选择、缺失方向回退、换装阻断、命中、Release 和结束清理。
- 静态检查、CodeGraph、Gemini 自审和 Automation 不替代编译、Editor readback 或 PIE 证据。
- 同一根因最多允许一次证据驱动修复和一次针对性复验；根因复现即停止，不轮询、不扩大范围。

## D2B 实施与 Main 收口记录

### 实际变更

- Gemini 按冻结范围完成 7/7 个批准 Source/Test 文件：`MeleeWeaponDefinition.h`、Front/Backstab Ability 各自的 `.h/.cpp`，以及 Front/Backstab Execution Automation 测试；未扩展到 `MeleeWeaponDefinition.cpp`、装备组件、Config、Tag/Input、Blueprint、Montage、地图或 `Content/**`。
- `UMeleeWeaponDefinition` 现在提供可选 `FrontExecutionMontage` 与 `BackstabExecutionMontage`。两个 Ability 通过同形状 resolver 从当前主手近战定义读取方向字段，激活前后使用 `Transient` 武器/Montage 快照；缺失方向 fail-closed，不回退到 Unarmed 或 Ability CDO。
- `CommitAbility()` 按测试强制失败、快照有效性、live 主手/Montage 指针身份、基类 Commit 的顺序执行；Commit 后 Montage、动画身份、D2A Snap 距离和停止逻辑均使用快照。既有 `State.Action.Attacking`/`CanSwapNow()` 换装闸门、VictimStart/Hit/Release、唯一 Resolver 和清理合同未改动。
- 测试适配器改为写入当前测试玩家 transient 主手武器字段；每个 Section 显式清理两个方向字段并覆盖方向缺失回退、快照固定、身份漂移、换装拒绝、Commit 失败和 EndAbility 清理。

### 验证证据分层

- 用户直接确认：Front/Backstab Focused Automation 通过，Scene01 PIE 中方向 Montage 选择、缺失方向回退普通 Primary、执行期间换装阻断、命中/Release 与结束清理通过。
- Gemini 回交报告：列出 Visual Studio `PolyQuestEditor (Development Editor)` 编译、Rider `get_file_problems`（7 个批准 C++ 文件零 errors）和执行资产配置/readback 已通过。Main 未重复执行这些用户-owned 门禁，故该报告不改写为 Main 独立编译/readback 收据。
- Main 静态门禁：批准差异与一跳调用关系已审查；旧 Ability 全局 `ExecutionMontage` 残留扫描、快照/Commit 顺序核对和 `git diff --check` 已通过。`code-review-graph` 索引基于旧 SHA `55dc530`，按规则记为 stale-coverage fallback；未用其作运行时证明。

### Main Fresh Review 结论

- 按 `ue-strict-review` 完成两批、有界、缺陷优先审查；第二批后硬停止，未进行第三批漫游或重复测试。
- 当前批准范围内未发现可证实的 P0、P1 或 P2 缺陷；未发现 Ability 生命周期、快照身份、Exactly-Once、换装闸门或测试隔离的行为回归。Gemini 的实施自审不计作 Main Fresh Review。

### 剩余债务与收口边界

- D2B 没有单独归档的 Main/用户手动编译与执行 Weapon/GA/Montage readback 收据；保留为非阻塞 authored-validation debt。Gemini 回交报告可作为来源标注的辅助证据，但不能支持干净 authored baseline 或 packaging 声明。
- `D2A-VERTICAL-SURFACE` 仍覆盖坡地、台阶和较大高度差的 Ground Alignment；`Debt-REC-05A1-03-RET-Teardown` 仍覆盖强制中断、销毁、UnPossess、Teardown 的 PIE 收据。D2A 的独立编译/readback 债务也继续保留。
- 本次 Main 只同步阶段文档，不暂存、不提交；所有用户-owned `Content/**`、Config 和其他 WIP 原样排除。下一执行切片为 `TODO-05A1-E`，D2C/D2D 仍需各自产品/证据触发条件。

## 风险、债务与文档边界

- 删除 Ability 全局 `ExecutionMontage` 不保留 deprecated alias 或 fallback；若历史 GA 资产出现加载警告或仍需迁移，另立资产清单、Reference/readback 和迁移阶段。
- 未配置方向 Montage 的武器仍可合法装备，但对应执行路线必须拒绝；Unarmed 不作为隐式执行动画来源。
- D2A 的独立编译/readback 收据、`D2A-VERTICAL-SURFACE` 和 `Debt-REC-05A1-03-RET-Teardown` 继续保留，不因 D2B 关闭。
- Main 已在验证与 Fresh Review 后同步 `ROADMAP.md`、`ARCHITECTURE.md`、`README.md` 和 `ROADMAP-archive.md`；文档同步不等于 staging 或 commit approval。
- 不提交任何内容，直到用户明确批准；所有未列入批准路径的现有 WIP 原样排除。

## Gemini Handoff Prompt

```
你是 PolyQuest 的 TODO-05A1-D2B Implementation Executor。

仓库：E:\GameDevelop\PolyQuest
基线：main @ 5cba1e2f42146082b7fee26d488ac1feec7e6d36
活动计划：E:\GameDevelop\PolyQuest\plan.md
路线：Outer=ue-stage-workflow；Primary=ue5-cpp-gameplay；Support=ue5-debug-validation
执行方式：manual/out-of-band Gemini
Contract owner：Main/Codex；Implementation writer：Gemini

只允许修改以下 7 个 Source/Test 文件：
- E:\GameDevelop\PolyQuest\Source\PolyQuest\Public\Combat\Equipment\MeleeWeaponDefinition.h
- E:\GameDevelop\PolyQuest\Source\PolyQuest\Public\AbilitySystem\Abilities\PlayerFrontExecutionAbility.h
- E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\AbilitySystem\Abilities\PlayerFrontExecutionAbility.cpp
- E:\GameDevelop\PolyQuest\Source\PolyQuest\Public\AbilitySystem\Abilities\PlayerBackstabExecutionAbility.h
- E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\AbilitySystem\Abilities\PlayerBackstabExecutionAbility.cpp
- E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\Tests\FrontExecutionAutomationTests.cpp
- E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\Tests\BackstabExecutionAutomationTests.cpp

冻结合同：
1. 在 UMeleeWeaponDefinition 增加可选 FrontExecutionMontage 和 BackstabExecutionMontage，默认 nullptr；缺失方向只拒绝该路线，不回退到 Unarmed 或 Ability CDO Montage。
2. 两个 Ability 各自实现同形状的 TryResolveExecutionMontage(ActorInfo, OutWeaponDef, OutMontage)，统一使用 GetEquippedMainHandMelee()。
3. 两个 Ability 各自加入 UPROPERTY(Transient) TObjectPtr<const UMeleeWeaponDefinition> ActiveExecutionWeaponDefinition 和 UPROPERTY(Transient) TObjectPtr<UAnimMontage> ActiveExecutionMontage。
4. CanActivateAbility、ActivateAbility、CommitAbility 共享 resolver；Commit 顺序必须是测试强制失败桩、快照有效性、live 主手/Montage 身份比较、Super::CommitAbility。
5. Commit 后所有 Montage 播放、动画身份校验、EndAbility 停止和 D2A Snap 距离读取只用快照，不重新选择 live Montage。
6. 保持 InstancedPerActor、ServerOnly、ExecutionLockContext、Activation Token、VictimStart -> Hit -> Release、FMeleeHitResolver、exactly-once、formal Release 和既有清理合同；换装继续依赖 State.Action.Attacking/CanSwapNow，不新增锁 Tag 或服务。
7. SetTestExecutionMontage() 保留名称但改为写入当前测试玩家 transient 武器的对应方向字段；CDO/无 Avatar/无装备/无主手时静默返回。每个测试 Section 在 Ability/Handle 清理后把两个方向字段恢复 nullptr。

执行顺序：先改 DataAsset/Header 与 resolver/快照，再改两个 Ability CPP 的生命周期引用，最后更新 Front/Backstab Automation。不要修改 MeleeWeaponDefinition.cpp、PlayerCharacter、WeaponEquipmentComponent、CombatAutomationFixture、EnemyVictimExecutionAbility、Config、Build.cs、Gameplay Tags、Input、Blueprint、AnimBP、Montage、地图或 Content。

验证与回交：
- 做精确旧 ExecutionMontage 残留扫描、快照/Commit 顺序检查和 git diff --check；Rider 可用时运行 lint_files/get_file_problems。
- 覆盖不同 Front/Backstab Montage、缺失方向普通 Primary 回退、强制 Commit 失败、live 身份不一致、激活后字段变更、换装拒绝与 Section 隔离。
- 不新增同步 Commit 竞态注入测试接口。
- 完成后只回交 changed paths、静态检查结果、未运行的用户编译/Editor/Automation/PIE 门禁、自审 findings 和剩余风险。
- 首次交付优先使用 1 个无历史污染只读子代理完成严格实施自审；不要把该自审称为 Main Fresh Review。
- 不提交、不修改计划/路线图/架构文档、不扩大范围；若需要未列出的文件、公共 API、Tag、Input、Config、资产或生命周期规则，立即停止并把证据返回 Main。
```
