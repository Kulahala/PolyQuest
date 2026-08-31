# TODO-03I2：Cross-Weapon Combat Tag Taxonomy Migration v1

## 阶段状态与执行路由

- **状态**：已实施；用户已确认相关 Automation 与 Scene01 PIE 通过；Main fresh review 与修复后的增量复核完成，文档收口后提交。
- **基线**：`main @ df246dc3542de9c115988cbd9e7ea56d10685067`。
- **工作区边界**：保留用户现有 `Content/**` WIP、删除、未跟踪资源与 `Config/Automation/Presets/1.json`；不得清理、回滚或纳入本阶段提交。
- **Outer**：`ue-stage-workflow`
- **Primary**：`ue5-cpp-gameplay`
- **Support**：`ue5-debug-validation`
- **Route reason**：本阶段是 Native GAS Ability/Tag selector 的跨文件合同迁移，包含 Config 声明、生命周期取消边界与 focused Automation；不涉及资产写入。
- **Execution route**：`manual/out-of-band Gemini`。
- **Ownership**：Main 负责合同、计划、范围、验证解释、fresh review、文档、staging 和 commit；Gemini 仅实现下列冻结的 Source/Config/test 路径；用户负责手动编译、Editor readback、Reference Viewer、PIE 与最终提交批准。

## 目标与冻结决策

本阶段只处理 I1 Tag 审计中已经证明存在职责重载的候选，不进行 96 个 Tag 的全量重命名。唯一运行时目标是让取消/销毁选择器按能力与生命周期语义命名，而不是按当前武器族命名。

### 新增正交 Tag

在 `Config/Tags/PolyQuestGameplayTags.ini` 声明并由 Native Ability 使用：

- `Ability.Action.CancelableBy.Dodge`
- `Ability.Action.CancelableBy.Defense`
- `Ability.Action.CancelableBy.Reaction`
- `Ability.Action.Teardown.OnUnpossess`

`UPlayerMeleeSkillAbility` 的 Native/Blueprint CDO 必须携带上述四个 Tag。具体技能身份仍由 authored Tag 提供，例如 `Ability.Skill.Whirlwind`，不得用新 Tag 取代技能身份。

### `Ability.Skill.Melee` 迁移策略

- 从 Native source、Dodge/Defense/Reaction/UnPossessed selector 和测试语义中移除。
- 不替换为宽泛的 `Ability.Skill`。
- 实施结果：Config 条目已删除；Gemini 报告产品资产零引用，Main 未重复执行独立 Reference Viewer readback。旧 Tag 只在测试中的显式“未注册”断言、历史文档和本阶段迁移记录中出现，不再是运行时合同。

### 保持不变的 Tag

- 物理输入：`Input.PrimaryAttack`、`Input.Aim`、`Input.Guard`、`Input.Parry`、`Input.AbilitySlot.1..4`。
- Ability identity：`Ability.Attack.*`、`Ability.Attack.Primary`、`Ability.Defense.*`、Shield 子 Tag、`Ability.Skill.Whirlwind`。
- 窗口状态与事件：`State.Action.CanCancel.*`、`Event.Action.CancelWindow.Dodge.Begin/End`。
- Bow `Event.Attack.Bow.DrawReady/Release` 与 Charged `Event.Attack.Charged.HoldReady/ReleaseHandoff`。

保留理由是这些 Tag 的 payload、Montage、Notify 或生命周期并不等价；共享按键不能证明可以共享 Tag 语义。

## 实现合同

### PlayerMeleeSkillAbility

- 删除 `MeleeSkillAbilityTag` 和 `EnsureMeleeSkillCategoryTag()`。
- 增加 `EnsureNativeCapabilityTags()`，在构造函数、`PostLoad()` 和编辑器 `PostCDOCompiled()` 中请求并补入四个新 Tag，防止 Blueprint 默认 Tag 容器覆盖 Native capability。
- `CanActivateAbility()` 与 `ActivateAbility()` 前置门禁改为要求四个新 Tag 有效；不得继续依赖 `Ability.Skill.Melee`。
- 保持现有 Montage、Cost/Cooldown、Trace、Dodge/Defense cancel window、Motion-Warp 和 `EndAbility()` 生命周期不变。

### Selector 与生命周期路径

- `UDodgeAbility`：移除旧技能 Tag 成员、校验和取消项，改用 `CancelableBy.Dodge`；其它 Primary/Light/Charged/Sprint 精确取消项保持。
- `UPlayerGuardAbility` 与 `UPlayerParryAbility`：将 `CancelableMeleeAbilityTags` 重命名为 `DefenseCancelableAbilityTags`；保留四个现有攻击 identity，并以 `CancelableBy.Defense` 替换旧技能项。可在 `WITH_DEV_AUTOMATION_TESTS` 下提供只读 getter 供测试断言。
- `UPlayerBigHitReactionAbility`、`UPlayerLaunchReactionAbility`：在 `BlockAbilitiesWithTag` 和 `AbilitiesToCancel` 中将旧技能项替换为 `CancelableBy.Reaction`，保持原有列表数量及 Dodge 只取消、不阻断的差异。
- `UPlayerGuardBreakAbility`：在 `AbilitiesToCancel` 中将旧技能项替换为 `CancelableBy.Reaction`；不把 Guard Break 改造成新的阻断路径。
- `APlayerCharacter::UnPossessed()`：保留 `Ability.Attack.Light` 的独立精确取消；另建独立容器选择 `Teardown.OnUnpossess`。不扩展 Charged、Sprint 或 Bow，除非审查发现明确泄漏证据。

### 测试 CDO 隔离

`PlayerMeleeMotionWarpingAutomationTests.cpp` 中的 UnPossessed 测试只为测试技能临时提供 `Teardown.OnUnpossess`。必须保存测试 Ability CDO 原始 `AbilityTags` 并在作用域结束时精确恢复，禁止 `AbilityTags.Reset()`；不得修改测试辅助 Ability 文件，也不得把 `DynamicAbilityTags` 当作未经验证的 `CancelAbilities()` 匹配保证。

## Gemini 批准修改路径

Gemini 只能修改以下文件；任何新增路径、公共合同、资产或 Config 扩展都必须停止并交回 Main：

```text
Source/PolyQuest/Public/AbilitySystem/Abilities/PlayerMeleeSkillAbility.h
Source/PolyQuest/Private/AbilitySystem/Abilities/PlayerMeleeSkillAbility.cpp
Source/PolyQuest/Public/AbilitySystem/Abilities/DodgeAbility.h
Source/PolyQuest/Private/AbilitySystem/Abilities/DodgeAbility.cpp
Source/PolyQuest/Public/AbilitySystem/Abilities/PlayerGuardAbility.h
Source/PolyQuest/Private/AbilitySystem/Abilities/PlayerGuardAbility.cpp
Source/PolyQuest/Public/AbilitySystem/Abilities/PlayerParryAbility.h
Source/PolyQuest/Private/AbilitySystem/Abilities/PlayerParryAbility.cpp
Source/PolyQuest/Private/AbilitySystem/Abilities/PlayerBigHitReactionAbility.cpp
Source/PolyQuest/Private/AbilitySystem/Abilities/PlayerLaunchReactionAbility.cpp
Source/PolyQuest/Private/AbilitySystem/Abilities/PlayerGuardBreakAbility.cpp
Source/PolyQuest/Private/Character/Player/PlayerCharacter.cpp
Config/Tags/PolyQuestGameplayTags.ini
Source/PolyQuest/Private/Tests/PlayerActionWindowAutomationTests.cpp
Source/PolyQuest/Private/Tests/HitReactionAutomationTests.cpp
Source/PolyQuest/Private/Tests/PlayerMeleeMotionWarpingAutomationTests.cpp
```

## 执行顺序与验证矩阵

1. 先声明四个新 Tag，再完成 `PlayerMeleeSkillAbility` 的 Native CDO 注入和旧成员移除。
2. 迁移 Dodge、Guard、Parry、Reaction、Guard Break 与 UnPossessed selector，保持原有调用顺序、忽略自身规则和取消/阻断数量。
3. 更新三个 focused Automation 文件，删除旧 Tag 断言，增加新 Tag、selector 精确内容、UnPossessed 范围和 CDO 恢复断言。
4. 执行 Rider error-level 检查和 `git diff --check`，并交还 Main；Gemini 不运行或声称用户编译、Editor、PIE，也不提交 Git。

Automation 至少覆盖：

- `PlayerMeleeSkillAbility` 四个新 Tag 的 CDO 有效性与 authored `Ability.Skill.Whirlwind` 身份保留。
- Dodge 在可取消窗口内能取消带 `CancelableBy.Dodge` 的技能，且不再依赖 `Ability.Skill.Melee`。
- Guard/Parry selector 保留四个攻击 identity、包含 `CancelableBy.Defense`、不包含旧技能 Tag。
- Big/Launch Reaction 的 Block/Cancel 列表包含 `CancelableBy.Reaction`，数量与 Dodge 阻断差异不回归。
- Guard Break 取消 `CancelableBy.Reaction` 技能但不新增阻断语义。
- UnPossessed 只取消 Light 与带 `Teardown.OnUnpossess` 的测试技能，不误伤 Guard；测试结束后 CDO Tag 与 ASC 状态完全恢复。
- 既有 Player action-window、Hit Reaction、Motion-Warp Section 1-6 回归保持。

用户验证门槛：

- 手动 `PolyQuestEditor` Development Editor 编译。
- Editor Readback：`GA_Skill_Whirlwind` 等产品 Ability 的四个 Native Tag、具体技能 identity、旧 Tag 引用情况；Reference Viewer 记录旧 Tag 零引用或保留债务。
- Scene01 PIE：Dodge、Guard、Parry、Guard Break、大小/击飞受击、UnPossessed 取消及现有 Light/Charged/Sprint/Bow 路径无行为回归。

## 非目标、风险与停止条件

- 不编辑、迁移、删除或重命名任何 `Content/**`、uasset、umap、Montage、Notify、AnimBP、Blueprint、Input Action、GameplayEffect、Build.cs 或 `.uproject`。
- 不实现 Mage/Staff、AOE、Beam、Projectile、新输入或通用 Ability dispatcher；不改变 Motion Warp、Trace、Resolver、Damage、Projectile、Enemy AI 或 StateTree。
- 不批量重命名其它 Tag；保留 `Event.Action.CancelWindow.Dodge.*`、Bow/Charged phase events 和 `Ability.Attack.Primary`。
- 若发现旧 Tag 还有合法的非取消用途、Blueprint 覆盖导致 Native capability 无法注入、`CancelAbilities()` 无法匹配预期 Tag，或必须修改未列路径，立即停止并返回证据，不得自行扩大范围。

## Main 收尾

用户验证与 Main fresh review 通过后，Main 更新 `plan.md` closeout、`ROADMAP.md` 当前/下一阶段指针、`ARCHITECTURE.md` 稳定 Tag/取消合同，并将本阶段历史证据追加到 `ROADMAP-archive.md`。README 仅在公开状态确实变化时更新。提交只暂存本阶段批准的 Source/Config/test 与文档路径，排除全部用户 Content WIP、`Config/Automation/Presets/1.json` 和其它未批准变更。

## 实施、复核与收口记录

- **执行结果**：四个正交 Tag `Ability.Action.CancelableBy.Dodge`、`Ability.Action.CancelableBy.Defense`、`Ability.Action.CancelableBy.Reaction`、`Ability.Action.Teardown.OnUnpossess` 已加入 Config 并接入 Native Ability/selector；`Ability.Skill.Melee` 已从 Config、生产取消/阻断选择器和测试语义移除，`Ability.Skill.Whirlwind` 仍保留为具体 authored skill identity。
- **实际变更路径**：共 16 个批准路径：`Config/Tags/PolyQuestGameplayTags.ini`；`PlayerMeleeSkillAbility.h/.cpp`、`DodgeAbility.h/.cpp`、`PlayerGuardAbility.h/.cpp`、`PlayerParryAbility.h/.cpp`；`PlayerBigHitReactionAbility.cpp`、`PlayerLaunchReactionAbility.cpp`、`PlayerGuardBreakAbility.cpp`、`PlayerCharacter.cpp`；`PlayerActionWindowAutomationTests.cpp`、`HitReactionAutomationTests.cpp`、`PlayerMeleeMotionWarpingAutomationTests.cpp`。未修改测试辅助 Ability 文件。
- **运行时合同**：Melee Skill CDO 通过 Native 注入保留四个 capability/teardown Tag；Dodge、Guard/Parry、Reaction、Guard Break 和 `UnPossessed()` 分别使用对应能力/生命周期 selector。Guard Break 保持只取消不新增阻断，UnPossessed 保持 Light 与 teardown selector 的独立取消，不扩展 Charged/Sprint/Bow。
- **复核修复**：Main fresh review 发现测试对已删除 Tag 的 `HasTagExact()` 断言是无效负断言，且测试 CDO 清理使用 `Reset()` 存在污染风险。Gemini 将其改为 `IsValid() == false` 显式断言、移除无效查询，并保存/精确恢复测试 CDO Tag；增量复核未发现 P0/P1/P2 blocker。未执行第二轮 adversarial review。
- **验证证据**：用户确认修复后的相关 Automation 与 Scene01 PIE 通过；Gemini 报告 Rider error-level 检查无错误并通过 `git diff --check`。Main 未重复运行编译、Automation、Editor readback 或 PIE，也不将静态检查代称为运行时证据。
- **未解决债务**：`Debt-03I2-DodgeSkillCancelUnitTest` 仍开放：无头 Automation 尚未用真实 AnimInstance/Montage fixture 端到端激活 Dodge、建立 cancel window 并取消带 `CancelableBy.Dodge` 的技能；当前由静态 CDO/tag 断言和用户 PIE 覆盖。关闭条件是建立专用集成测试关卡或轻量有效 fixture 并覆盖真实生命周期。独立手动 `PolyQuestEditor` 编译收据与 Main 独立 Reference Viewer 记录未形成。
- **文档与提交边界**：本次同步 `plan.md`、`ARCHITECTURE.md`、`ROADMAP.md`、`ROADMAP-archive.md`；不修改 `README.md`。提交仅包含上述 16 个 I2 Source/Config/test 路径与四份文档，排除 `Content/**`、`Config/Automation/Presets/1.json`、其他 Source/Config WIP、资产、Build.cs 与 `.uproject`。
- **后续指针**：I2 已关闭；下一开放玩家战斗实现阶段为 `TODO-05A：Stagger Front Execution v1`。`TODO-03I3` 仍是仅在具体 Staff/Mage 路线获批后启动的条件阶段。
