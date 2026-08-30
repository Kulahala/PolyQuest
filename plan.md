# TODO-03A7D：Melee Skill Motion-Warp Adoption v1

## 计划状态与阶段目标

- **状态**：源码实现与修复已完成，用户确认 focused Automation 通过，Main defect-first fresh review 通过；本阶段不把 `GA_Skill_Whirlwind` 标记为 `adopted`，因为逐项 Editor readback 仍未形成收据。
- **唯一运行时问题**：确认现有 `UPlayerMeleeSkillAbility` 能否在不改变 GAS、伤害、Trace、Guard/Parry 与输入路由的前提下，为实际存在的 `GA_Skill_Whirlwind` 提供一次性近战 Motion-Warp 接触辅助。
- **阶段结果**：允许两种诚实收口：
  - `adopted`：源码、测试、Editor readback 与 Scene01 PIE 均证明 Whirlwind 的 Montage/Root Motion/Notify 合同兼容，并由用户在 Editor 中 opt-in；
  - `evidence-backed no-adoption`：资产证据证明不兼容或不足，保持默认关闭，不修改共享资产，不把候选资产写成已采用事实。
- **范围原则**：只审查当前实际存在的 Skill；当前离线资产索引确认的唯一子类是 `GA_Skill_Whirlwind`。不得把未来法师或其他潜在 Skill 当成当前实现对象。

## 当前基线与工作区快照

- **分支**：`main`。
- **阶段父基线**：`73711b1b5e50a71ef1b8ffaa1316b08371459c78`（`[Feature] Charged/Sprint 近战 Motion-Warp 接入与阶段收口`）。
- `plan.md` 中旧的 `ae0139b` 是 C 阶段当时视角的历史基线；D 阶段建立时以 `73711b1` 为实时基线，不回写旧快照。
- 阶段建立时工作区的用户 WIP 为 `Content/**` 与 `Config/Automation/Presets/1.json`，没有 `Source/**` WIP；本次收口快照另包含 D 阶段批准的四个 Source/test 文件及本计划的文档改动。
- 所有用户 WIP 均保留：不得清理、回滚、导入、移动、重定向、复制或纳入 staging。D 阶段提交仍不得使用 `git add -A`。

## 已确认的架构事实与前置条件

- `ABaseCharacter` 拥有单机 ASC 与 `UCharacterAttributeSet`；`UPlayerMeleeSkillAbility` 是 `InstancedPerActor`、`ServerOnly` 的 GAS Ability。不得新增 ASC、AttributeSet、并行动作状态真值或网络/回滚层。
- `UWeaponEquipmentComponent` 仍是准备槽、精确 `FGameplayAbilitySpecHandle` 与技能激活的唯一运行时所有者。D 不新增 Input/Tag 路由，不改装备授权边界。
- 现有唯一近战伤害链必须保持：`UAbilityTask_MeleeTraceWindow → FMeleeHitResolver → Damage GameplayEffect`。Motion-Warp 只改变接触位移，不直接施加伤害、不读写 Health/Poise、不改 SetByCaller、Cost/Cooldown 或 Trace timing。
- `APlayerCharacter` 已拥有 `UMotionWarpingComponent`、`SetMeleeMotionWarpTarget()`、`ClearMeleeMotionWarpTargets()`、有效 Lock-On 解析与死亡/销毁/清锁/离地/EndPlay 清理桥。D 复用这些窄桥，不增加目标跟随服务。
- `FMeleeMotionWarpConfig`、`FMeleeMotionWarpSnapshot` 与纯几何 `FMeleeMotionWarpingLifecycle::EvaluateMeleeMotionWarpTransform()` 已存在。D 只复用 evaluator；不把 UObject/ASC/World/Lock-On 生命周期流程抽成全局或回调驱动 helper。该取舍是为避免在 Light/Charged/Sprint 之外再引入新的共享状态/抽象，保持本阶段最小改动。
- `PolyQuest.Build.cs` 已链接 `GameplayAbilities`、`GameplayTags`、`GameplayTasks` 与 `MotionWarping` 所需模块；`PolyQuest.uproject` 已启用 MotionWarping；`Config/Tags/PolyQuestGameplayTags.ini` 已有 `Ability.Skill.Melee`。这些文件本阶段冻结不改。
- 当前 `UPlayerMeleeSkillAbility` 已在 Montage Task 成功确认后 Commit，再激活 Trace/Dodge-cancel/Rate listeners，并取消 Guard；D 已补齐默认关闭的 Motion-Warp 配置、Ability-owned 静态快照、一次性 Apply 与生命周期清理，实际资产是否采用仍由 Editor readback 决定。

## 批准路径、所有权与执行路线

```text
Outer: ue-stage-workflow
Primary: ue5-cpp-gameplay
Support: ue5-debug-validation
Execution route: manual/out-of-band Gemini
```

| 路径 | 批准内容 | 合同所有权 |
| --- | --- | --- |
| `Source/PolyQuest/Public/AbilitySystem/Abilities/PlayerMeleeSkillAbility.h` | 五项默认关闭的 authored Motion-Warp 配置、Ability-owned 快照/私有函数、开发测试 seam | `Contract owner: Main`；`implementation writer: Gemini` |
| `Source/PolyQuest/Private/AbilitySystem/Abilities/PlayerMeleeSkillAbility.cpp` | Task/Montage 门禁、Commit 后一次性捕获/Apply、生命周期清理 | `Contract owner: Main`；`implementation writer: Gemini` |
| `Source/PolyQuest/Private/Character/Player/PlayerCharacter.cpp` | `UnPossessed()` 新增独立的 `Ability.Skill.Melee` 精确取消 | `Contract owner: Main`；`implementation writer: Gemini` |
| `Source/PolyQuest/Private/Tests/PlayerMeleeMotionWarpingAutomationTests.cpp` | 现有 Motion-Warp 套件 Section 6 Skill 矩阵与最小取消回归 | `Contract owner: Main`；`implementation writer: Gemini` |

冻结不改：`PlayerCharacter.h`（除非 Main 另行批准公共接口）、`MeleeMotionWarping.h/.cpp`、`ComboChainDataAsset`、`WeaponEquipmentComponent`、`PolyQuest.uproject`、`PolyQuest.Build.cs`、Gameplay Tags、所有 Content/Config 资产及项目文档（实现阶段由 Main 维护）。Gemini 不得提交、stage、修改 Editor 或自行扩大路径；需要未列出的文件、Tag、Input、资产或生命周期规则时立即停下并返回证据。

## 冻结的源码与运行时合同

### Authored 配置与快照

在 `UPlayerMeleeSkillAbility` 增加以下 `EditDefaultsOnly, BlueprintReadOnly` 属性，默认值必须与共享 evaluator 一致：

- `bUseMotionWarping = false`；
- `WarpTargetName = MeleeContact`；
- `WarpStopDistance = 190.0f`；
- `MaxWarpDistance = 110.0f`；
- `MaxWarpAngleDegrees = 60.0f`。

Ability 私有持有一个非反射 `FMeleeMotionWarpSnapshot`，并提供 `ResetMeleeMotionWarpState()` 与 `TryApplyMeleeMotionWarpTarget(APlayerCharacter*)`。不得创建新的全局 Singleton、通用 Skill dispatcher 或第二份几何数学。

### 激活时序

1. `ActivateAbility()` 开始时复位临时状态、快照，并清空 Player Warp targets；`bTestBypassMontageActiveCheck = false` 必须在 `#if WITH_DEV_AUTOMATION_TESTS` 下同步复位。
2. 保留现有 ASC、Player、接地、AnimInstance、Skill Montage、Cost/Cooldown/Damage/Stamina Regen GE 与 Gameplay Tag 门禁。
3. 用局部 `CreatedMontageTask` 保存本次 Montage Task 身份，再写入成员 `MontageTask`；创建失败走现有 `EndAbility()`，不留下副作用。
4. `CreatedMontageTask->ReadyForActivation()` 后，先检查同步结束与上下文，再检查 `MontageTask.Get() == CreatedMontageTask`、`CreatedMontageTask->IsActive() && !CreatedMontageTask->IsFinished()`，最后检查 `BoundAnimInstance->Montage_IsActive(ActiveMontage)`。任一失败都不得 Commit 或 Apply Warp，并通过 canonical `EndAbility()` 收尾。
5. `CommitAbility()` 成功后再次确认 `bEndAbilityRequested == false`，且 Player/World/AnimInstance/Montage 与 `CreatedMontageTask` identity/active 状态仍有效；若 Commit 内部同步结束或置换了 Task，立即走 `EndAbility()`，不得 Apply Warp。上下文成立后保留现有 `SetRuntimeActionTags(true)` 与一次性 `ApplyLockAwareActionFacing()`，随后恰好调用一次 `TryApplyMeleeMotionWarpTarget()`，再激活 Trace/Dodge-cancel/Rate listeners，最后执行既有 Guard cancellation。Apply 失败只表示“不做位移修正”，不得取消已经确认的 Skill。
6. `WITH_DEV_AUTOMATION_TESTS` 下的 `bTestBypassMontageActiveCheck` 只能替换最终 `Montage_IsActive()` 结果；不得绕过 Task 创建、Task active/finished、Task identity、ASC/Controller/World、目标验证、evaluator 或清理。

### 一次性捕获与 fail-closed

- disabled 或 `FMeleeMotionWarpingLifecycle::IsConfigValid()` 失败的配置不查询目标、不消耗捕获机会，并清除可能残留的 Player Warp target。
- 首个合法 opt-in 调用必须在任何 Player/World/Controller/ASC/Lock-On 查询前标记 `bAttemptedCapture`；Controller 游离、ASC 错配、无锁、目标无效或死亡时也不可在后续恢复后重新选敌。
- 首次只接受当前 Lock-On 且经 `ResolveValidLockedTarget()` 二次确认的存活、未销毁、同 World、接地 Enemy；快照保存弱引用、Actor location 与接地状态。
- 后续调用只重新验证快照目标存活/销毁状态与 World，永不重新锁定、跟随、更新目标位置或共享其他 Ability 的快照。
- 几何求值使用 Player 当前 location/forward/接地状态与当前配置，目标使用静态快照；停距、最大修正距离、最大角度、有限坐标与双方接地规则完全委托现有 evaluator。
- 目标、上下文或几何失败时清空 Player Warp targets 并返回；不得写入临时 Transform。目标死亡/销毁、清锁、离地与 Player EndPlay 继续由既有 Player/Enemy 窄桥即时清理。

### 结束与 UnPossess

- `EndAbility()` 的第一次实际清理路径必须在 `#if WITH_DEV_AUTOMATION_TESTS` 下先将 `bTestBypassMontageActiveCheck` 复位为 `false`，再清空 Player Warp targets、复位快照并完成既有 Action Tag、Trace、Rate、Montage delegate/task 与 Montage stop 清理；自然结束、中断、取消、Task 失败和同步重入均汇入此路径。
- `APlayerCharacter::UnPossessed()` 保留现有 `Ability.Attack.Light` 取消，并用**独立**的 `Ability.Skill.Melee` Tag 容器再调用一次 `CancelAbilities()`。不合并两个 Tag，不取消 `Ability.Skill` 之外的 Ability，不改变输入/装备路由。
- 不新增离地即取消所有 Skill 的规则；离地现有 Warp 清理保持不变，Skill 是否结束仍由既有 GAS/能力生命周期决定。

## 实际资产采用门

用户在 Editor 中逐项 readback `GA_Skill_Whirlwind` 及其直接引用：

- 父类确为 `UPlayerMeleeSkillAbility`；确认五项配置、`SkillMontage`、Damage/Cost/Cooldown/恢复 GE 的实际默认值；
- 确认候选 `AM_Sword_ChargAttack` 是否含显式 `AnimNotifyState_MotionWarping`、匹配的 `WarpTargetName`、`RootMotionModifier_SkewWarp`、有效 Notify 时序、Root Motion 与 AnimBP 消费设置；Player 组件已关闭在 Montage 内自动搜索，因此不能依赖隐式搜索；
- 检查该 Montage 是否与 Charged Ability 共用。若共享 Montage 的 Notify/停距语义不兼容，不修改共享资产，Whirlwind 以 evidence-backed `no-adoption` 收口；
- 只有 readback 通过后，用户才将 Whirlwind 的 `bUseMotionWarping` 设为 true。任何 `.uasset/.umap`、Blueprint、Montage、AnimBP 或 DataAsset 变更都不进入本阶段 Source/test 提交。

## Automation 与用户验证

### Automation

继续使用 `PolyQuest.Combat.PlayerMeleeMotionWarping`，新增 Section 6，至少覆盖：

1. 默认配置、disabled/非法配置、无锁目标不捕获且不写 Warp；
2. 合法接地目标的一次性捕获、快照位置/接地状态与预期 Transform；
3. 第二次调用、目标移动、切锁不重新捕获、不自动 retarget；
4. 首次合法调用时 Controller/ASC/World/目标/接地状态失效的捕获机会消耗与后续 fail-closed；
5. 目标死亡/销毁、异 World、非有限坐标、停距/距离/角度边界清理 Warp；
6. `ResetMeleeMotionWarpState()`、`EndAbility()` 与激活起始路径清理快照/targets，并可靠复位测试 bypass；
7. 能在无头环境安全构造的 Montage Task identity/active 门禁。`UPlayerMeleeSkillAbility` 的 `SetTestCurrentActorInfo(...)`、`SetTestBoundAnimInstance(...)`、`SetTestBypassMontageActiveCheck(...)`、快照访问器等 seam 必须与既有 Light/Charged/Sprint 命名和 `#if WITH_DEV_AUTOMATION_TESTS` 范围一致。不得伪造 active Task 来声称真实动画时序已覆盖；真实 `ReadyForActivation()`、Task Finished 与 Montage playback 联动由静态复核和用户 PIE 覆盖；
8. 使用现有可保持 Active 的 `UTestMeleeTrailAbility`（通过 Spec dynamic tag 标记 `Ability.Skill.Melee`，不新增生产 Ability）验证 `UnPossessed()` 取消该类别，同时确认不会扩大到无关 Tag；
9. Sections 1–5 既有 Shared/Light/Charged/Sprint 矩阵全部保持通过。

所有测试 seam 必须严格包在 `#if WITH_DEV_AUTOMATION_TESTS`，只用于无头测试解耦，不旁路生产门禁、唯一 Trace/Resolver/Damage 链或清理流程。

### 用户门槛

1. Gemini 完成批准路径的 `git diff --check`、Rider C++ 静态诊断与路径审计后，用户手动编译 `PolyQuestEditor` Development Editor；
2. 用户完成上述 Editor readback，并明确记录 adopted 或 no-adoption 证据；
3. 用户运行 focused Automation；
4. 在 `/Game/Maps/Scene01` PIE 中通过准备槽激活 Whirlwind，验证合法近/中/远距离与角度的 Root Motion 接触、无明显穿透/过冲、目标失效清理、Guard/Dodge/Trace/Damage 回归和 UnPossess teardown；若 no-adoption，则验证默认关闭时 Skill 行为与基线一致，不宣称 Motion-Warp 已生效。

用户编译、Automation、Editor readback 与 PIE 是彼此独立的证据类别；任何一类未运行都必须如实记录，不能用静态图、自动化或二进制字符串替代。

## 文档、收口与依赖

- C 阶段 closeout 已存在于 `ROADMAP-archive.md`；本计划获接受后由 Main 维护本文件，Gemini 不编辑项目文档。
- 阶段验证与 fresh review 通过后，Main 只按需要更新 `ROADMAP.md` 的里程碑/债务指针；本次因缺少逐项资产 readback，不更新 `ARCHITECTURE.md` 或 `README.md` 的采用事实。
- B/C 的 authored compile/readback 债务仍是独立、非 D 源码计划 blocker；D 必须独立证明 Whirlwind，不替它们补证据。
- 收口后的依赖指针为：`TODO-03A7D → TODO-03H5 → TODO-05A → TODO-05B → TODO-07B5 → TODO-07B6 → TODO-03C`；`TODO-03A7E` 仍是首个 ranged-enemy gate 之后的可选 Enemy 阶段。
- 提交边界：候选提交最多包含本计划列出的 4 个 Source/test 路径及 Main 明确批准的文档收口；不包含任何 Content、Config、资产、Editor 状态或未列路径。用户明确批准后才 staging/commit。

## 阶段收口记录（2026-08-31）

- **源码结果**：`UPlayerMeleeSkillAbility` 完成一次性静态 Lock-On 快照与共享几何求值接入；合法调用只在已确认的 Montage/Commit 时序中尝试一次，失败、目标失效、取消、UnPossess、自然结束和 teardown 均经既有清理边界 fail-closed。`Ability.Skill.Melee` 的 UnPossess 取消与既有 `Ability.Attack.Light` 取消保持独立。
- **本次 Main 窄修复**：在已批准的 `PlayerMeleeSkillAbility.cpp::ActivateAbility()` 内加入 Notify Task `ReadyForActivation()` 前后重入/有效性守卫，并在六个 Task 完成后再次确认 Player/World；未改变公开 API、ASC、Gameplay Tag、Input、伤害链或资产范围。
- **实际批准路径**：`Source/PolyQuest/Public/AbilitySystem/Abilities/PlayerMeleeSkillAbility.h`、`Source/PolyQuest/Private/AbilitySystem/Abilities/PlayerMeleeSkillAbility.cpp`、`Source/PolyQuest/Private/Character/Player/PlayerCharacter.cpp`、`Source/PolyQuest/Private/Tests/PlayerMeleeMotionWarpingAutomationTests.cpp`。
- **复核与验证证据**：Gemini 报告 Rider error-level 检查无 error、`git diff --check` 通过；用户确认修复后的 `PolyQuest.Combat.PlayerMeleeMotionWarping` focused Automation 成功。用户此前确认过 Scene01 PIE，但修复后的本轮未提供独立的新 PIE 收据；这些证据不扩展为全量回归、手动编译或资产 readback。
- **未闭合门槛**：`GA_Skill_Whirlwind` 父类/五项配置、Montage Notify/Modifier/Root Motion/目标名/时间窗的逐项 Editor readback，以及独立手动 `PolyQuestEditor` 编译记录仍缺失。默认配置继续保持关闭，不宣称 `adopted`；这是一项非阻塞 authored-validation debt，关闭触发器是用户完成指定 readback/编译，或提供真实 evidence-backed `no-adoption`。
- **文档决策**：本次同步 `ROADMAP.md` 的当前指针与 canonical debt，并追加本阶段历史记录到 `ROADMAP-archive.md`；`ARCHITECTURE.md` 与 `README.md` 不写入未经 readback 的资产采用事实。
- **后续路线**：在进入 `TODO-05A` 前新增 `TODO-03H5：Post-Motion-Warp Combat Health Review v1`，只做跨四个 Player Motion-Warp 消费者的健康度与债务审计，不新增功能。
- **提交边界**：本次提交只包含上述四个批准 Source/test 路径与 `plan.md`、`ROADMAP.md`、`ROADMAP-archive.md`；`Content/**`、`Config/**`、其他 Source WIP、资产、Editor 状态均排除。

## 明确非目标

- 不接入其他或未来 Skill，不批量修改法师/远程技能；
- 不创建通用 Skill dispatcher、全局 Motion-Warp service、Tick 跟随、自动 retarget、Locomotion Warp 或第二个目标系统；
- 不改变 Lock-On acquisition/cycle、Bow/Projectile、Enemy Melee、Hit Reaction、Guard/Parry、Poise、Damage GE、Trace/Resolver、Cost/Cooldown、Input、Equipment 或 Gameplay Tag taxonomy；
- 不导入、迁移、修复、删除或手工 patch `.uasset/.umap`，不清理现有 WIP，不编译/运行 Editor/Automation/PIE，不提交。
