# TODO-03A7F：Melee Motion-Warp Trigger Range Contract v1

## 计划状态与阶段目标

- **状态**：源码实现已完成，用户已确认 focused Automation 与 Scene01 PIE；Main 已完成批准范围内的 defect-first fresh review，未发现 P0/P1/P2 blocker。Gemini 只负责批准 Source/test 路径，Main 负责合同、验证解释、文档、staging 与 commit。
- **主要运行时问题**：当前 `MaxWarpDistance` 表示玩家位移修正量，但实际触发上限是 `WarpStopDistance + MaxWarpDistance`，字段语义不直观，也无法表达贴近目标时的反向修正。
- **阶段目标**：将所有 Player 近战 Motion-Warp 消费者统一迁移到显式目标距离区间：`MinTriggerDistance <= WarpStopDistance <= MaxTriggerDistance`。
- **阶段结果**：保留默认的前向-only 体验，同时允许明确配置 `MinTriggerDistance < WarpStopDistance` 的攻击在下限内进行有界反向修正；不增加第二个反向开关。

## 当前基线与工作区快照

- **分支**：`main`。
- **阶段父基线**：`main @ 7c3ddf0`（`[Feature] 完成近战技能 Motion-Warp 源码接入与健康度路线收口`）。Roadmap 和旧阶段记录中的提交号只代表当时任务视角，不回写为本阶段当前基线。
- **工作区**：本阶段有 11 个批准 Source 文件的未提交实现；`plan.md` 与 `ROADMAP.md` 为本阶段收口文档改动。`Config/Automation/Presets/1.json` 是用户 WIP，`Content/**` 含大量修改、删除和未跟踪资源；全部保留，不清理、不回滚、不纳入本阶段 staging。
- **历史记录**：TODO-03A7D closeout 已存在于 `ROADMAP-archive.md`；本计划替换当前 `plan.md` 前不再重复实现或改写该阶段。

## 冻结运行时合同

### 配置字段与默认值

共享 `FMeleeMotionWarpConfig`、`FComboChainEntry`、`UChargedAttackAbility`、`USprintAttackAbility` 和 `UPlayerMeleeSkillAbility` 使用同一组语义：

~~~text
bUseMotionWarping       = false
WarpTargetName          = MeleeContact
MinTriggerDistance      = 190.0f
WarpStopDistance        = 190.0f
MaxTriggerDistance      = 300.0f
MaxWarpAngleDegrees     = 60.0f
~~~

- 直接删除/重命名 `MaxWarpDistance` 为 `MaxTriggerDistance`。
- 不保留 `FormerlySerializedAs`、旧字段、双字段、兼容分支或 `bAllowReverseCorrection`。
- `MaxTriggerDistance` 是玩家与目标快照之间的最大触发距离，不是玩家位移修正量上限。
- 启用配置时必须满足：距离字段 finite 且非负，且 `MinTriggerDistance <= WarpStopDistance <= MaxTriggerDistance`；角度仍限制在 `[0,180]`。
- `IsConfigValid()` 必须显式拒绝 `MinTriggerDistance > WarpStopDistance` 或 `WarpStopDistance > MaxTriggerDistance`；不得只做单字段非负检查。
- `bUseMotionWarping == false` 仍然是不参与捕获、求值和目标写入的关闭状态。

### 距离区间行为

令 `D` 为玩家当前位置到静态目标快照的水平距离：

| 条件 | 行为 |
| --- | --- |
| `D < MinTriggerDistance` | fail-closed，清理 Warp target |
| `MinTriggerDistance <= D < WarpStopDistance` | 允许有界反向修正 |
| `D == WarpStopDistance` | 不触发，避免写入零修正 Transform |
| `WarpStopDistance < D <= MaxTriggerDistance` | 允许前向修正 |
| `D > MaxTriggerDistance` | fail-closed，清理 Warp target |

- `D == MinTriggerDistance` 与 `D == MaxTriggerDistance` 是包含边界。
- 实现包含边界时允许 `KINDA_SMALL_NUMBER` 数值容差（例如 `D < Min - KINDA_SMALL_NUMBER` 或 `D > Max + KINDA_SMALL_NUMBER` 才视为越界）；该容差只用于浮点稳定，不改变配置关系和产品语义。
- 零水平距离、非有限坐标、未接地、超角度、目标死亡/销毁/异 World 等既有门禁优先返回 false。
- 继续使用 `Target - Direction * WarpStopDistance` 计算目标位置；`D < WarpStopDistance` 时自然产生有界反向修正，不额外反转朝向。
- 以有符号修正量 `D - WarpStopDistance` 判定零修正；使用 `FMath::IsNearlyEqual(..., KINDA_SMALL_NUMBER)`（或等价绝对误差判断）将精确停距及数值上等价的零修正拒绝，不写入目标。
- 当 `MinTriggerDistance == WarpStopDistance` 时反向区间为空，行为恢复为旧的前向-only 语义。
- 删除旧的“玩家到最终 WarpLocation 不得超过 `MaxWarpDistance`”检查；区间自身限定前向修正范围 `MaxTriggerDistance - WarpStopDistance` 和反向修正范围 `WarpStopDistance - MinTriggerDistance`。

### 生命周期与既有边界

- 四个 Ability 的调用时序、一次性 Lock-On 快照、静态目标位置、目标失效清理、EndAbility/UnPossess/Task/Montage 防御保持不变。
- 不改变 ASC、AttributeSet、Gameplay Tag/Input、Cost/Cooldown、Guard/Parry、Sprint 所有权、Trace Window、`FMeleeHitResolver` 或 Damage GameplayEffect。
- 不增加目标跟随、自动重选、Tick/Timer、全局 Motion-Warp service 或新的 Player bridge。
- 现有 Motion-Warp debug 输出保留；仅将允许范围显示从 `Stop ~ Stop + MaxWarp` 改为 `Min ~ Max`，调试信息不成为生产逻辑或持久 HUD。

## 批准路径、所有权与执行顺序

~~~text
Outer: ue-stage-workflow
Primary: ue5-cpp-gameplay
Support: ue5-debug-validation
Execution route: manual/out-of-band Gemini
Implementation executors: Gemini
~~~

Gemini 只能修改以下路径。所有共享结构、字段语义、生命周期和 GAS/Tag/Input 合同均为 `Contract owner: Main`；实现写入者为 `Gemini`。

~~~text
E:\GameDevelop\PolyQuest\Source\PolyQuest\Public\Combat\Melee\MeleeMotionWarping.h
E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\Combat\Melee\MeleeMotionWarping.cpp
E:\GameDevelop\PolyQuest\Source\PolyQuest\Public\Combat\ComboChainDataAsset.h
E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\AbilitySystem\Abilities\LightAttackAbility.cpp
E:\GameDevelop\PolyQuest\Source\PolyQuest\Public\AbilitySystem\Abilities\ChargedAttackAbility.h
E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\AbilitySystem\Abilities\ChargedAttackAbility.cpp
E:\GameDevelop\PolyQuest\Source\PolyQuest\Public\AbilitySystem\Abilities\SprintAttackAbility.h
E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\AbilitySystem\Abilities\SprintAttackAbility.cpp
E:\GameDevelop\PolyQuest\Source\PolyQuest\Public\AbilitySystem\Abilities\PlayerMeleeSkillAbility.h
E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\AbilitySystem\Abilities\PlayerMeleeSkillAbility.cpp
E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\Tests\PlayerMeleeMotionWarpingAutomationTests.cpp
~~~

执行顺序：

1. 更新共享配置结构、`IsConfigValid()` 和纯几何 evaluator；保持 UObject/ASC/World/Lock-On 生命周期流程在各 Ability 内，不抽出新的全局状态 helper。
2. 更新 `FComboChainEntry` 的 authored 字段和 Light 到共享 config 的映射、校验、日志与测试包装；静态 `ULightAttackAbility::EvaluateMeleeMotionWarpTransform()` 必须同时映射 `MinTriggerDistance` 与 `MaxTriggerDistance`；不改变 `StartComboEntry()` 的门控、Task 时序或快照合同。
3. 更新 Charged、Sprint、Skill 的 reflected 字段、config 构造、日志和开发测试 seam 参数；保持各自原有的 Apply 时点、快照和 EndAbility 清理。
4. 更新全部测试调用点和断言，删除活动 Source/test 语义中的 `MaxWarpDistance`。
5. Gemini 完成静态自检后停止，不修改文档、资产、Config、Editor，不 stage/commit；需要未列出的文件、Tag、Input 或生命周期规则时立即返回 Main 决策。

测试 seam 的参数顺序固定为：

~~~text
bUseWarp, TargetName, MinTriggerDistance, WarpStopDistance,
MaxTriggerDistance, MaxWarpAngleDegrees
~~~

## Automation 与用户验证

### Automation 矩阵

继续使用 `PolyQuest.Combat.PlayerMeleeMotionWarping`，保留既有 Sections 1-6，并补齐以下覆盖：

- 共享 evaluator：关闭配置、非法顺序、负数、NaN/Inf、零方向、零距离、接地和角度门禁。
- 距离边界：`D < Min`、`D == Min`、`Min < D < Stop`、`D == Stop`、`Stop < D < Max`、`D == Max`、`D > Max`。
- `Min == Stop` 的旧前向-only 回归，以及 `Min < Stop` 的反向 Transform 位置/朝向回归。
- 非法顺序至少覆盖 `Min > Stop`、`Stop > Max` 和 `Min > Max`，全部 fail-closed。
- 反向边界除断言返回值外，还要断言 WarpLocation 位于玩家后方、Rotation 仍朝向目标；`Min == Stop` 时所有 `D < Stop` 均不得触发。
- Light、Charged、Sprint、Skill 四个消费者的字段映射、默认值、一次性快照、目标失效、静态复用和 EndAbility 清理。
- 保留既有 Task/Montage 激活、死亡/销毁、ClearLockedTarget、UnPossess、空中以及 `Trace -> Resolver -> Damage GE` 回归断言。

共享 evaluator 负责穷举几何边界；每个消费者至少验证一组前向、反向、精确停距和越界输入，避免只测共享 helper 而漏掉字段映射。

### 资产迁移与 Editor readback

这是有意的 authored-field migration，不由 Gemini 修改二进制资产，也不手工 patch `.uasset/.umap`：

1. 在源码改名前，用户在 Editor 记录所有实际旧值，形成迁移表。
2. 用户手动编译新源码后，对每个相关 Ability/Combo 资产填写：
   - `MinTriggerDistance = 旧 WarpStopDistance`
   - `WarpStopDistance = 旧 WarpStopDistance`
   - `MaxTriggerDistance = 旧 WarpStopDistance + 旧 MaxWarpDistance`
3. 已知二进制字符串候选至少包括：
   - `E:\GameDevelop\PolyQuest\Content\_Abilities\Weapon\LightSword\Charged\GA_Sword_ChargedAttack.uasset`
   - `E:\GameDevelop\PolyQuest\Content\_DataAssets\Player_Combat\DA_Combo_StraightSword.uasset`

   该列表仅用于定位，不是完整资产清单，最终以 Editor readback 为准。
4. 默认迁移值保持 `Min == Stop`；只有明确需要反向修正的攻击才将 `Min` 设为低于 `Stop`。
5. 用户逐项保存并 readback 新字段。未完成 readback 前，不宣称 authored adoption 或干净可复现资产基线。

### 成功标准与验证门槛

- Gemini 报告所有批准 Source/test 文件的静态检查和 `git diff --check` 通过。
- 独立用户 `PolyQuestEditor` Development Editor 手动编译收据：未形成（见本节验证债务）。
- 用户已确认 focused Automation 套件通过。
- Scene01 PIE 至少验证：
  - 默认 `Min == Stop` 时近距离不发生反向滑步，原有前向接触行为保持；
  - 一个明确 `Min < Stop` 的配置在贴近区间产生有界反向修正；
  - 精确停距和区间外不写 Warp target；
  - 前向接触、Guard/Dodge、Trace/Resolver/Damage、取消和 UnPossess 无回归。
- 缺少资产 readback 时，可以记录源码/Automation 收口和明确的 authored-validation debt，但不得把它写成视觉采用事实。

## 文档收尾与非目标

- Main 在验证和 fresh review 后维护 `plan.md` closeout；仅在路线、债务或当前/下一阶段指针变化时同步 `ROADMAP.md`。
- `ARCHITECTURE.md` 只在新合同通过验证后更新稳定的区间字段语义；`README.md` 只更新已证实的公开状态；历史 `ROADMAP-archive.md` 保留旧字段名和旧阶段视角，不做全局 cosmetic rename。
- 非目标：新 Motion-Warp 消费者、近战/远程统一框架、Loadout 或 Tag 重构、资产清理、共享 Ability 抽象、反向开关、第二伤害路径、动态跟随、网络/回滚、PlayerCharacter 或 Enemy 改动。

阶段依赖顺序：

~~~text
TODO-03A7F
  -> TODO-03H5：Post-Motion-Warp Combat Health Review v1
  -> TODO-03I1：Unified Combat Input Contract And Loadout Simplification v1
  -> TODO-03I2：Cross-Weapon Gameplay Tag Taxonomy Migration v1
  -> TODO-05A -> TODO-05B -> TODO-07B5 -> TODO-07B6 -> TODO-03C
~~~

`TODO-03H5` 只审查最终区间合同和四个消费者的健康度，不重新实现或调参已完成的 Motion-Warp adoption；所有现有 Content/Config WIP 继续由用户保管。

## 实施收口记录（2026-08-31）

- **实际结果**：共享 `FMeleeMotionWarpConfig`、`FComboChainEntry` 以及 Light、Charged、Sprint、Melee Skill 四个 Player 消费者已统一使用 `MinTriggerDistance <= WarpStopDistance <= MaxTriggerDistance`。旧 `MaxWarpDistance` 已从活动 Source/test 语义移除；`Min < Stop` 允许区间内的有界反向修正，`Min == Stop` 保持前向-only，精确停距不写入零修正目标。
- **实现边界**：保留既有一次性静态 Lock-On 快照、目标失效与 EndAbility/UnPossess/Task/Montage 清理、Player Motion-Warp 窄桥及 `Trace -> Resolver -> Damage GE` 唯一路径；没有新增消费者、跟随服务、重选目标、伤害路径、Tag/Input 或资产迁移。
- **批准实现路径**：
  `Source/PolyQuest/Public/Combat/Melee/MeleeMotionWarping.h`、
  `Source/PolyQuest/Private/Combat/Melee/MeleeMotionWarping.cpp`、
  `Source/PolyQuest/Public/Combat/ComboChainDataAsset.h`、
  `Source/PolyQuest/Private/AbilitySystem/Abilities/LightAttackAbility.cpp`、
  `Source/PolyQuest/Public/AbilitySystem/Abilities/ChargedAttackAbility.h`、
  `Source/PolyQuest/Private/AbilitySystem/Abilities/ChargedAttackAbility.cpp`、
  `Source/PolyQuest/Public/AbilitySystem/Abilities/SprintAttackAbility.h`、
  `Source/PolyQuest/Private/AbilitySystem/Abilities/SprintAttackAbility.cpp`、
  `Source/PolyQuest/Public/AbilitySystem/Abilities/PlayerMeleeSkillAbility.h`、
  `Source/PolyQuest/Private/AbilitySystem/Abilities/PlayerMeleeSkillAbility.cpp`、
  `Source/PolyQuest/Private/Tests/PlayerMeleeMotionWarpingAutomationTests.cpp`。
- **验证证据**：用户确认 `PolyQuest.Combat.PlayerMeleeMotionWarping` focused Automation 与 `/Game/Maps/Scene01` PIE 通过。Gemini 报告批准文件的 Rider error-level 检查、VS2022 编译和 `git diff --check` 通过；Main 本轮没有重复运行这些门禁，且不把 Gemini 报告改写为独立用户编译收据。
- **Fresh review**：Main 按批准路径执行 diff-first、一跳调用边界核对和缺陷优先复核，未发现 P0/P1/P2 阻塞。测试 3.17 的旧 Warp target 会先由 `StartComboEntry()` 清理，因而不能单独证明 evaluator 自身清理旧 target；这是不影响运行时合同的 P3 覆盖说明，不阻塞本阶段收口。
- **尚未形成的证据**：没有独立手动 `PolyQuestEditor` Development Editor 编译记录，也没有逐项 authored GA/Combo/Montage/Notify/Modifier/Root Motion/目标名的 Editor readback；因此不宣称资产已完成采用或可由干净检出复现。旧字段到新字段的迁移仍由用户在 Editor 中逐项确认，或以真实 evidence-backed no-adoption 关闭。
- **排除范围**：`Config/Automation/Presets/1.json`、全部 `Content/**`、其他 Source/Config/文档 WIP、Blueprint/Montage/AnimBP/DataAsset/地图、`PolyQuest.uproject`、`Build.cs`、Gameplay Tags、Input、旧 Test 项目及任何资产均未纳入本阶段提交；没有清理、回滚、导入或 Editor 写入。
- **提交状态**：文档收口与上述 11 个批准 Source 路径构成本阶段候选提交；最终提交 hash 以 Git 历史为准。下一阶段指针为 `TODO-03H5：Post-Motion-Warp Combat Health Review v1`。
