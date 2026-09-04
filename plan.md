# TODO-05A1-D2D：Weapon-Authored Execution Range Contract v1（实施计划）

## 阶段状态与基线

- 状态：D2D 实施、用户-owned Automation/PIE 门禁与 Main Fresh Review 已完成；本文件保留为当前最近阶段记录，等待显式 commit approval。
- 日期：2026-09-05（规划快照始于 2026-09-04）。
- 仓库：`E:\GameDevelop\PolyQuest`；分支：`main`；D2D 正式 parent：`29a06cd393c672ceefa48c80b177ff3378b54d39`。该提交是前一阶段的独立 Unity Build 符号重定义修复；规划时快照 `b82313c` 仅作历史基线，不含 D2D 代码。
- 工作树：非 clean，含用户-owned `Content/**`、Config、`AGENTS.md`、`ROADMAP.md` 及其他 WIP；当前 D2D 实际修改为批准的 16 个 Source/Test 文件，全部未暂存。所有既有 WIP 原样保留，不使用 reset、checkout 或 `git add -A`。
- Archive preflight：PASS。D2C 的日期化 closeout 已存在于 `ROADMAP-archive.md`；本次收口将追加 D2D 历史条目，不覆盖既有证据。
- 当前导航证据：`.codegraph/` 存在且索引有效（211 files / 3,811 nodes / 12,209 edges）。这是 source/static 导航证据，不是编译、Editor readback 或 PIE 证据。
- 用户已确认 D2D Focused Automation 与 Scene01 PIE 通过；Gemini 回交报告另列手动 `PolyQuestEditor (Development Editor)` 编译、Rider 与真实资产 readback 通过，均保留为执行者证据，未改写为 Main 独立收据。

## 启动门禁与目标

### 启动门禁

`ROADMAP.md` 将 D2D 标为条件性下一切片：只有真实武器族证据证明存在触发窗口差异、重复配置或维护漂移时才实施。

实施前由用户在 Unreal Editor **只读**确认并冻结 manifest，不保存任何资产：

1. 至少两个真实近战武器族及其 `UMeleeWeaponDefinition` 对象路径。
2. 每个武器当前 `ExecutionSnapDistance`，以及旧 Front/Backstab GA 中的 Min/Max 覆盖值（如存在）。
3. 同一武器的 Front 与 Backstab 窗口是否一致，是否存在重复或漂移。

若没有真实差异、重复配置或维护漂移，记录 evidence-backed no-adoption 并停止，不写实现。若同一武器的 Front/Backstab 窗口不同，停止并返回 Main 作产品/契约决策；不新增方向专属距离字段。manifest 是采用证据，不是 Gemini 修改资产的授权。

### 唯一运行时问题

装备中的近战武器是否能成为 Front/Backstab 处决距离窗口与一次性 Snap 距离的唯一作者化来源，同时保持 D2A 一次性 Snap、D2B 武器 Montage、D2C StanceBreak 兼容、统一 Hit Notify、唯一伤害解析和既有生命周期不变。

### 成功标准

1. `UMeleeWeaponDefinition` 唯一持有 `MinExecutionDistance`、`MaxExecutionDistance`、`ExecutionSnapDistance`，默认值为 `0 / 250 / 190 cm`。
2. 三值共享同一 Native 校验；非法值不自动夹紧，DataAsset 校验与 Front/Backstab 两个入口均 fail-closed。
3. 预激活只读取 live 武器值；激活后保存不可反射的三值快照，Snap、命中复核和后续几何使用快照。
4. `CommitAbility()` 对武器对象、Montage 和三值使用位级精确 `==` 抗漂移比较；失败发生在 Victim handshake、锁定和 Snap 之前。
5. 水平二维中心距离边界包含；Snap 保持既有 Z 锚定及 Front/Backstab 方向语义。
6. 现有 Focused Automation、P0 回归用例和 Scene01 PIE 的 Front/Backstab、命中、Release、取消、换装阻断与 D2A-D2C 行为通过。

## 基线证据与已确认风险

- `Source/PolyQuest/Public/Combat/Equipment/MeleeWeaponDefinition.h:95-97` 当前只有 `ExecutionSnapDistance = 190.0f`；DataAsset 尚无 Min/Max。
- `Source/PolyQuest/Private/Combat/Equipment/MeleeWeaponDefinition.cpp:242-245` 当前只校验 Snap，未校验窗口关系。
- `PlayerFrontExecutionAbility.h:86,163-166` 与 `PlayerBackstabExecutionAbility.h:86,164-167` 当前各自声明并内联写入 `MinExecutionDistance`/`MaxExecutionDistance`；两个构造器也分别写入 `0/250`。
- 两个 Ability 的 `CanActivateAbility()`、`ValidateTargetPrerequisites()`、`Check*Geometry()`、`TryApplyExecutionSnap()`、`CommitAbility()`、`ActivateAbility()` 和命中处理仍读取 Ability-owned Min/Max；这是本阶段要收口的直接调用链。
- `ExecutionSnapAlignment.h/.cpp` 当前只有 `IsSnapDistanceValid()` 与 `TryBuildTransform()`；纯范围校验尚未存在。
- `ExecutionSnapAlignmentAutomationTests.cpp` 当前只覆盖 Snap 值；Front/Backstab 测试夹具通过 `SetTestExecutionDistances()` 改写 Ability 字段，迁移后会改写测试用的 transient 主手 DataAsset，必须补齐恢复。
- **P0 已确认**：`Source/PolyQuest/Private/Tests/ExecutionReleaseOutcomesAutomationTests.cpp:587` 仍为 `SetTestExecutionDistances(50.0f, 300.0f)`；该用例玩家与目标相距 100 cm。保留 `Max <= 250 cm` Native hard cap 时，300 cm 会在激活前被拒绝并击穿既有 `Backstab ability activated` 断言，故该文件必须纳入批准路径并改为 `(50.0f, 250.0f)`。不采用放宽到 1000 cm 的方案。
- `ROADMAP.md` 的旧条件条目曾使用 `ExecutionMinDistance`/`ExecutionMaxDistance` 文字；本计划以用户指定及现有 Ability 语义的精确名称 `MinExecutionDistance`/`MaxExecutionDistance` 为唯一新契约，本轮不顺手修改 roadmap。

## 冻结运行时契约

### 1. DataAsset 单一事实来源

在 `UMeleeWeaponDefinition` 的 `Weapon|Execution` 类别新增/保留以下三个 `EditDefaultsOnly, BlueprintReadOnly` 浮点字段（单位 cm）：

| 字段 | 默认值 | 语义 |
|---|---:|---|
| `MinExecutionDistance` | `0.0f` | 玩家中心到目标中心的水平二维触发距离下界，包含边界 |
| `MaxExecutionDistance` | `250.0f` | 同一距离窗口上界，包含边界；Native hard cap 为 250 cm |
| `ExecutionSnapDistance` | `190.0f` | 一次性把玩家对齐到目标前/后方的中心位移；保持既有 Z 锚定 |

Editor `ClampMin/ClampMax` metadata 只能改善作者输入提示，不能替代 Native 校验，也不能把非法值静默夹紧。不得保留 Ability-owned Min/Max、旧 GA fallback、方向别名或第二套距离来源。

### 2. 统一纯校验

在 `FExecutionSnapAlignment` 增加唯一范围入口：

```cpp
static bool IsExecutionDistanceRangeValid(
    float MinDist,
    float MaxDist,
    float SnapDist,
    FString* OutFailureReason = nullptr);
```

契约：入口先清空 `OutFailureReason`（若非空），按以下顺序检查并只报告首个失败；不做任何自动修正：

1. 三值均 finite。
2. `MinDist >= 0.0f`。
3. `MaxDist > MinDist`。
4. `SnapDist > 0.0f`。
5. `MinDist <= SnapDist <= MaxDist`。
6. `MaxDist <= 250.0f`。

`IsValidWeaponDefinition(FString& OutReason)` 在保留 superclass/其他武器校验的前提下调用该 helper，不重复编写分支或错误文案。`IsSnapDistanceValid()` 继续服务 `TryBuildTransform()` 的局部正值检查，不能产生另一套范围规则。

### 3. Front/Backstab live 与 snapshot 边界

- 删除两个 Player Execution Ability 上的 Min/Max `UPROPERTY` 及构造器赋值；不新增反射字段。
- 两个 Ability 各保存以下非反射激活快照，并在构造/结束时清零：

```cpp
float ActiveMinExecutionDistance = 0.0f;
float ActiveMaxExecutionDistance = 0.0f;
float ActiveExecutionSnapDistance = 0.0f;
bool bHasActiveExecutionDistanceSnapshot = false;
```

- `CanActivateAbility()` 与 `ActivateAbility()` 的前置校验从当前装备的 `UMeleeWeaponDefinition` 读取 live 三值，并调用同一 helper；无效值必须在 `CommitAbility()`、Victim handshake、ExecutionLockContext 和 Snap 之前拒绝。
- `CheckFrontGeometry()` / `CheckBackstabGeometry()` 改为显式接收 `MinDist`、`MaxDist`，调用点清楚标示 live DataAsset 或 activation snapshot；不得再隐式读取已删除字段。建议固定为：

```cpp
bool CheckFrontGeometry(
    const APlayerCharacter* PlayerCharacter,
    const AEnemyCharacter* TargetActor,
    float MinDist,
    float MaxDist,
    float& OutDist2D,
    float& OutAngleDegrees) const;
```

Backstab 使用同构签名；角度阈值仍由既有方向字段负责。
- `ActivateAbility()` 在已解析 live 武器/Montage、进入 `CommitAbility()` 前冻结三值；`CommitAbility()` 必须同时精确比较 live 武器对象、live Montage 和三值 `==`。任一变化即 fail-closed，不使用 `FMath::IsNearlyEqual`。
- 一次性 Snap、Front 命中几何、Backstab 命中二维距离复核均读取激活快照；Backstab 不增加实时角度重定向。不得把 live DataAsset 在激活后再次读入行为路径。
- `EndAbility()` 无论自然结束、取消、中断、失败或 teardown 都清除快照和有效标记；不改变现有 Token/Task/Target 清理顺序。

### 4. 测试 setter 与夹具隔离

- 保留 `SetTestExecutionDistances(float InMin, float InMax)` 的测试名称与 `WITH_DEV_AUTOMATION_TESTS` 边界，但删除头文件内联的 Ability 字段写入。
- 在两个 `.cpp` 中实现 setter：解析当前测试 transient 主手 `UMeleeWeaponDefinition` 后只写入其 Min/Max；由于头文件仅前置声明，禁止在 header 内解引用 DataAsset。setter 不夹紧、不隐式修改 Snap；找不到测试武器时不引入新的生产 API。
- 现有各测试的 reset/cleanup 必须恢复完整三元组 `0.0f / 250.0f / 190.0f`，并在直接改写非法值的分支后恢复；不得依赖 setter 修复 `Snap < Min` 或跨用例污染。

## 批准路径与所有权

### 允许修改的唯一 Source/Test 路径

核心 Source：

- `E:\GameDevelop\PolyQuest\Source\PolyQuest\Public\Combat\Equipment\MeleeWeaponDefinition.h`
- `E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\Combat\Equipment\MeleeWeaponDefinition.cpp`
- `E:\GameDevelop\PolyQuest\Source\PolyQuest\Public\Combat\Execution\ExecutionSnapAlignment.h`
- `E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\Combat\Execution\ExecutionSnapAlignment.cpp`
- `E:\GameDevelop\PolyQuest\Source\PolyQuest\Public\AbilitySystem\Abilities\PlayerFrontExecutionAbility.h`
- `E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\AbilitySystem\Abilities\PlayerFrontExecutionAbility.cpp`
- `E:\GameDevelop\PolyQuest\Source\PolyQuest\Public\AbilitySystem\Abilities\PlayerBackstabExecutionAbility.h`
- `E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\AbilitySystem\Abilities\PlayerBackstabExecutionAbility.cpp`

Focused Tests：

- `E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\Tests\ExecutionSnapAlignmentAutomationTests.cpp`
- `E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\Tests\FrontExecutionAutomationTests.cpp`
- `E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\Tests\BackstabExecutionAutomationTests.cpp`
- `E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\Tests\ExecutionHitNotifyAutomationTests.cpp`
- `E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\Tests\ExecutionLethalRecoveryAutomationTests.cpp`
- `E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\Tests\ExecutionLockInAutomationTests.cpp`
- `E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\Tests\ExecutionReleaseOutcomesAutomationTests.cpp`
- `E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\Tests\ExecutionVictimPresentationAutomationTests.cpp`

### 明确排除

- 全部 `Content/**`、`.uasset`、`.umap`、Blueprint、AnimBP、Montage、地图和 imported/read-only 包；不得手工改包或自动保存。
- `CombatAutomationFixture.cpp`、`WeaponEquipmentComponent.*`、`EnemyStanceBreakAbility.*`、`ExecutionLockContext.*`、`PlayerCharacter.*`。
- Gameplay Tags、Input、Config、Build.cs、普通攻击 Motion-Warp、第二条伤害路径、通用处决基类、资产迁移脚本、包装/提交。
- 不放宽 250 cm hard cap，不保留旧 GA fallback，不新增方向专属距离字段、通用 Runtime Service 或生产测试 seam。

### 责任与路线

```text
Outer: ue-stage-workflow
Primary: ue5-cpp-gameplay
Support: ue5-debug-validation
Route reason: Native GAS/DataAsset contract migration with shared snapshot, validation, and focused Automation gates.
Execution route: manual/out-of-band Gemini
Contract owner: Main/Codex
Implementation writer: Gemini
Codex child delegates: 0
```

Main/Codex 保有架构、契约、计划、范围变更、验证解释、Fresh Review、文档、staging 与 commit ownership。Gemini 只写上述冻结路径；首次切片可按项目规则派发一个无历史污染的只读自审代理，但该自审不替代 Main 的独立 review。用户拥有 Editor authoring/readback、手动编译、Automation/PIE 与最终 commit approval。

## 执行顺序与停止条件

1. 用户完成只读真实武器 manifest；缺少两族差异、发现同武器方向窗口冲突或需要资产迁移时，返回 Main 决策并停止。
2. Main 接受本计划后发送下方 Gemini handoff；Gemini 先读当前 `plan.md` 和批准路径，不得先改文件外内容。
3. Gemini 按依赖顺序实现：`FExecutionSnapAlignment` helper → `UMeleeWeaponDefinition` 字段/校验 → 两个 Ability 的 live/snapshot/Commit/geometry 路径 → 测试 setter/reset 与矩阵；P0 的 300 cm 用例必须同步修正。
4. Gemini 做静态自审、精确搜索旧字段、`git diff --check`，列出改动路径和未运行的用户-owned 门禁；不得运行 UBT/Build.bat、Editor 写入、打包或提交。
5. Main 读取最终 diff，仅对批准路径运行轻量静态检查、Rider C++ inspection（可用时）和 `git diff --check`；不调用 UBT 或启动 Editor。
6. 用户手动编译 `PolyQuestEditor (Development Editor)`，完成 Editor readback、Focused Automation 与 Scene01 PIE；失败时保留首个错误，按同一根因最多一次证据驱动修复和一次针对性复验。
7. 用户门禁通过后，Main 对批准 diff 运行一次有界 `ue-strict-review`，再决定是否同步 `ROADMAP.md`/`ARCHITECTURE.md`/archive；未通过前不标记阶段完成、不提交。

任一实现需要未列路径、公共 API、Tag/Input/Config、资产写入、改变武器装备所有权、改变 D2A-D2C 生命周期或新增生产 seam，立即停止并只回报证据。若测试夹具无法构造某个边界而必须改生产代码，也停止交 Main 决策；不绕过范围。

## Focused Automation 与验收矩阵

| 入口 | 必须覆盖的断言 | 证据类型 |
|---|---|---|
| `PolyQuest.Combat.ExecutionSnapAlignment` | 默认 `0/250/190` 有效；finite、负值、`Max<=Min`、Snap 非正、Snap 越界、`Max>250` 全部失败；失败原因非空且按首个失败返回；DataAsset `IsValidWeaponDefinition()` 与 helper 一致 | Editor Automation |
| `PolyQuest.Combat.FrontExecution` | live DataAsset 窗口用于预激活；包含 Min/Max 边界；非法三值在 handshake 前拒绝；激活后改写 DataAsset 不影响已冻结 Snap/命中复核；恢复夹具三元组 | Editor Automation |
| `PolyQuest.Combat.Backstab` | 与 Front 同样的窗口、边界、非法值、snapshot/anti-drift 覆盖；保留方向角度和 Backstab 语义 | Editor Automation |
| `PolyQuest.Combat.ExecutionHitNotify` | 统一 Hit Notify、唯一 `FMeleeHitResolver`、重复事件和目标销毁行为不变；命中距离使用 snapshot | Editor Automation |
| `PolyQuest.Combat.ExecutionLethalRecovery` | 致死/非致死、Release/取消/中断清理不变；无第二伤害路径 | Editor Automation |
| `PolyQuest.Combat.ExecutionLockIn` | 一次性 Snap、锁定、换装阻断和 D2A/D2B/D2C 回归不变 | Editor Automation |
| `PolyQuest.Combat.ExecutionReleaseOutcomes` | 第 587 行 `(50,300)` 改为 `(50,250)` 后既有 Backstab 激活与结果断言继续通过；目标 100 cm 语义不变 | Editor Automation |
| `PolyQuest.Combat.ExecutionVictimPresentation` | VictimStart/Hit/Release、Front/Backstab Montage、Poise/移动/AI 锁清理不变；不因 DataAsset 收口改变受害者表现 | Editor Automation |
| Scene01 PIE | 至少两个真实武器的窗口/边界、无效配置拒绝、Snap 方向/Z 锚定、Front/Backstab 命中与 Release/取消/换装回归 | 用户 PIE/readback |

静态检查、CodeGraph、Rider、`git diff --check` 或 Gemini 自审都不能替代用户编译、Editor readback 或 PIE。D2A/D2B/D2C 既有独立编译/readback 收据债务继续单独记录，不因本计划自动关闭。

## 文档、债务与提交边界

- 本轮只替换 `plan.md`；不修改 `ROADMAP.md`、`ARCHITECTURE.md`、`README.md` 或 `ROADMAP-archive.md`。实现、用户验证和 Main Review 全部通过后，只有 Main 才能同步稳定契约、阶段状态、债务与归档。
- 需持续记录的非阻塞债务：D2A/D2B/D2C 独立手动编译与执行 Weapon/GA/Montage readback 收据尚未单独归档；本阶段不能把静态或执行者报告改写成这些证据。
- 阶段候选提交只允许批准 Source/Test 路径及经 Main 收口批准的文档；全部用户-owned Content/Config/Blueprint/地图 WIP 原样排除，并等待显式 commit approval。

## Gemini Handoff Prompt

```text
你是 PolyQuest 的 TODO-05A1-D2D Implementation Executor。只执行已冻结的 Source/Test 切片，不改变契约或范围。

仓库与基线：
- CWD: E:\GameDevelop\PolyQuest
- Branch: main
- Baseline: b82313c17d83e141f343efabaddf06c553c35fd0
- Active plan: E:\GameDevelop\PolyQuest\plan.md
- 当前工作树含用户-owned Content/Config/AGENTS/ROADMAP WIP；保留它们，不 reset、checkout、clean 或 git add -A。

路线与所有权：
- Outer=ue-stage-workflow
- Primary=ue5-cpp-gameplay
- Support=ue5-debug-validation
- Execution route=manual/out-of-band Gemini
- Contract owner=Main/Codex
- Implementation writer=Gemini
- Codex child delegates=0

在写 Source 前必须等待 Main/user 提供真实 Editor 只读 manifest：至少两个真实近战武器族、各自 UMeleeWeaponDefinition 路径、当前 ExecutionSnapDistance、旧 Front/Backstab GA 的 Min/Max 覆盖，以及同武器两方向窗口是否一致。没有真实差异/重复/漂移时做 no-adoption 回报并停止；同武器方向窗口不同或需要资产迁移时停止回报 Main 决策。不得保存、迁移、创建或修改任何 .uasset/.umap。

只允许修改以下文件：
E:\GameDevelop\PolyQuest\Source\PolyQuest\Public\Combat\Equipment\MeleeWeaponDefinition.h
E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\Combat\Equipment\MeleeWeaponDefinition.cpp
E:\GameDevelop\PolyQuest\Source\PolyQuest\Public\Combat\Execution\ExecutionSnapAlignment.h
E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\Combat\Execution\ExecutionSnapAlignment.cpp
E:\GameDevelop\PolyQuest\Source\PolyQuest\Public\AbilitySystem\Abilities\PlayerFrontExecutionAbility.h
E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\AbilitySystem\Abilities\PlayerFrontExecutionAbility.cpp
E:\GameDevelop\PolyQuest\Source\PolyQuest\Public\AbilitySystem\Abilities\PlayerBackstabExecutionAbility.h
E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\AbilitySystem\Abilities\PlayerBackstabExecutionAbility.cpp
E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\Tests\ExecutionSnapAlignmentAutomationTests.cpp
E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\Tests\FrontExecutionAutomationTests.cpp
E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\Tests\BackstabExecutionAutomationTests.cpp
E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\Tests\ExecutionHitNotifyAutomationTests.cpp
E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\Tests\ExecutionLethalRecoveryAutomationTests.cpp
E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\Tests\ExecutionLockInAutomationTests.cpp
E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\Tests\ExecutionReleaseOutcomesAutomationTests.cpp
E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\Tests\ExecutionVictimPresentationAutomationTests.cpp

冻结契约：
1. UMeleeWeaponDefinition 是唯一作者化来源，字段精确命名为 MinExecutionDistance、MaxExecutionDistance、ExecutionSnapDistance，默认 0.0f / 250.0f / 190.0f cm。Min/Max 是玩家中心与目标中心的水平二维距离窗口，边界包含；Snap 是既有一次性前/后方中心位移，保持 Z 锚定与方向语义。
2. 在 FExecutionSnapAlignment 增加：
   static bool IsExecutionDistanceRangeValid(float MinDist, float MaxDist, float SnapDist, FString* OutFailureReason = nullptr);
   先清空原因，按顺序检查：三值 finite；Min>=0；Max>Min；Snap>0；Min<=Snap<=Max；Max<=250。非法值不夹紧，报告首个失败。UMeleeWeaponDefinition::IsValidWeaponDefinition() 和两个 Ability 入口共用它，不复制校验分支。
3. 删除 Front/Backstab Ability 的 Min/Max UPROPERTY 与构造器赋值。各自保存非反射快照：ActiveMinExecutionDistance、ActiveMaxExecutionDistance、ActiveExecutionSnapDistance、bHasActiveExecutionDistanceSnapshot。预激活读取 live DataAsset；激活后 Snap、Front 几何、Backstab 命中二维距离复核读取快照；EndAbility 清空快照。
4. CheckFrontGeometry/CheckBackstabGeometry 必须显式接收 MinDist、MaxDist，调用点明确区分 live 与 snapshot。CommitAbility() 在 handshake、锁定、Snap 前精确比较 live 武器对象、Montage 和三值，使用 ==，禁止 FMath::IsNearlyEqual。
5. 保持 D2A Snap、D2B Montage resolver/快照、D2C/统一 Hit Notify、FMeleeHitResolver、VictimStart->Hit->Release、唯一伤害、Front 优先级和现有生命周期不变。
6. 保留 SetTestExecutionDistances(float,float) 测试名称，但移出 header 内联实现；在 .cpp 写当前测试 transient 主手 DataAsset 的 Min/Max，不夹紧、不暗改 Snap。所有测试 reset/cleanup 恢复完整 0/250/190 三元组。
7. 必须修复 P0：ExecutionReleaseOutcomesAutomationTests.cpp:587 的 SetTestExecutionDistances(50.0f, 300.0f) 改为 (50.0f, 250.0f)。该用例目标距 100 cm，保持测试语义。

绝对非目标：
- 不改 Content、资产、Blueprint、AnimBP、Montage、地图、Gameplay Tags、Input、Config、Build.cs。
- 不改 CombatAutomationFixture、WeaponEquipmentComponent、EnemyStanceBreakAbility、ExecutionLockContext、PlayerCharacter。
- 不放宽 250 cm hard cap，不保留 GA fallback，不新增方向字段、公共 Runtime Service、通用基类、第二伤害路径或生产测试 seam。
- 不修改 plan.md、ROADMAP.md、README.md、ARCHITECTURE.md、ROADMAP-archive.md；不运行 UBT/Build.bat、Editor 写入、打包或提交。

测试与交付：
- 更新 ExecutionSnapAlignment 的 helper/DataAsset 全矩阵（边界、finite、关系、hard cap、失败原因）。
- 更新 Front/Backstab 的 live window、边界、非法值、snapshot anti-drift 和夹具恢复；保留现有 D2A-D2C、Hit/Release/清理回归。
- 保持 ExecutionHitNotify、ExecutionLethalRecovery、ExecutionLockIn、ExecutionVictimPresentation 的既有断言；只在批准路径内补充必要的 DataAsset 三元组恢复或 P0 数值修正。
- 运行适用的静态检查与 git diff --check；按项目规则，首次切片优先派发一个无历史污染的只读严格自审代理，后续修复由你单兵复核。自审不是 Main Fresh Review。
- 回交必须列出：改动路径/函数、三元组校验与状态覆盖、P0 修正、静态检查结果、未运行的用户-owned 编译/Editor readback/Automation/PIE、self-review findings、剩余风险。

停止条件：需要未列文件、公共 API、Tag/Input/Config、资产写入、改变所有权/生命周期、放宽 hard cap，或现有夹具无法构造边界且必须新增生产接口。遇到任一条件只回报证据，不绕过范围。
```

## D2D Closeout（2026-09-05；parent HEAD `29a06cd`；working-tree not clean）

### 触发门与范围

- 用户 Editor 只读 manifest：PASS。`DA_Weapon_LightSword` 的 `ExecutionSnapDistance = 140 cm`，`DA_Weapon_HeavySword` 为 `170 cm`；旧 Front/Backstab GA 的窗口均为 `0..250 cm`，同一武器没有方向分裂，满足采用门槛。
- `29a06cd` 是 D2D 开始前的独立修复提交，仅将 `PlayerBackstabExecutionAbility.cpp` 的匿名命名空间 helper 改名以解决 Unity Build 符号冲突；提交说明明确排除 D2D。故 D2D review parent 统一采用 `29a06cd`，不把该修复混入 D2D 变更。
- 实际 D2D 修改严格为计划批准的 16 个 Source/Test 文件；没有 `Content/**`、资产、Tag、Input、Config、Build.cs 或未列 Source 路径变更。

### 实现结果

- `UMeleeWeaponDefinition` 现在持有唯一作者化三元组 `MinExecutionDistance`、`MaxExecutionDistance`、`ExecutionSnapDistance`，默认 `0 / 250 / 190 cm`。
- `FExecutionSnapAlignment::IsExecutionDistanceRangeValid()` 统一执行 finite、非负/严格关系、Snap 闭包和 `Max <= 250 cm` 校验，返回首个失败原因；DataAsset 与 Front/Backstab 入口共用该规则，非法值 fail-closed 且不夹紧。
- Front/Backstab 删除 Ability-owned Min/Max，预激活读取 live DataAsset，激活后保存非反射三值快照；`CommitAbility()` 对武器/Montage/三值使用精确 `==` 抗漂移，Snap、几何和命中复核使用快照，`EndAbility()` 清除快照。
- `SetTestExecutionDistances()` 已改为写测试 transient 主手 DataAsset；相关夹具恢复完整 `0 / 250 / 190` 三元组。`ExecutionReleaseOutcomesAutomationTests.cpp` 的 P0 `(50,300)` 已修正为 `(50,250)`。

### 验证与复核证据

- 用户证据：用户确认 D2D Automation 与 Scene01 PIE 通过；执行者回交列出 15 个 Combat Automation suite 为 `Success`，并覆盖轻剑/重剑窗口、Snap、处决全流程和换装阻断。
- 执行者证据：Gemini 报告 Visual Studio `PolyQuestEditor (Development Editor)` 编译、Rider `get_file_problems` 零 Error、真实资产 readback 通过；这些不作为 Main 独立收据。
- Main 静态证据：批准 Source/Test diff 与计划一致，`git diff --check` 通过；按 `ue-strict-review` 完成两批有界 Fresh Review，未发现批准范围内 P0/P1/P2 blocker。Review 使用 `29a06cd` 作为正式 parent，并核对其前置 bug-fix 提交不含 D2D 代码。

### 剩余风险与关闭条件

- D2D 独立的 Main/user 编译与执行 Weapon/GA/Montage/DataAsset readback 收据尚未单独归档；当前仅有 Gemini 回交证据，保持为非阻塞 authored-validation debt。关闭条件：形成命名、可追溯的用户收据，或作 evidence-backed no-adoption 决策。
- `Content/**`、Blueprint、AnimBP、Montage、DataAsset 和地图仍是用户-owned WIP；本阶段不宣称 clean-checkout authored fixture 或 packaging readiness。
- `D2A-VERTICAL-SURFACE`、`Debt-REC-05A1-03-RET-Teardown` 及 D2A/D2B/D2C 既有独立 readback 债务按原关闭条件保留，不因 D2D 自动关闭。

### 文档与提交边界

- Main 已完成 `ROADMAP.md`、`ARCHITECTURE.md` 与 `ROADMAP-archive.md` 的 D2D 收口同步；`plan.md` 保留本计划、handoff 与本 closeout。
- 候选提交只包含 16 个 D2D Source/Test 文件及经 Main 明确批准的文档；用户-owned Content/Config/Blueprint/地图和其他 WIP 原样排除，等待显式 commit approval。
- 后续执行指针为 `TODO-05A1-E`；D2D 不再作为“下一执行切片”。
