# TODO-03I1：Unified Combat Input Contract And Loadout Simplification v1

## 计划状态与阶段目标

- **状态**：已实施并完成 Main bounded defect-first fresh review；用户已确认 focused Automation 与 Scene01 PIE，通过本次文档收口后提交。Main 保留架构合同、范围、验证解释、文档收尾和提交所有权。
- **主要玩家问题**：Primary/Sprint 路由目前同时存在于武器的 AssociatedLoadout、Player 的 Loadout 镜像和 Equipment 解析路径，配置重复且可能留下 stale route identity；近战与 Bow 的相同物理按键又容易被误读为相同 Ability 生命周期。
- **阶段目标**：将 Primary/Sprint 的运行时 canonical route 收拢到当前主手 UWeaponDefinition，保持近战、Bow、Guard/Parry、1-4、GAS、Projectile 和输入时序行为不变。
- **唯一运行时问题**：共享物理输入是否可以在不合并 Ability 生命周期、payload、Montage 或取消语义的前提下，使用武器 direct fields 作为唯一 Primary/Sprint route？

## 当前基线与工作区边界

- **分支**：main。
- **基线**：main @ 6cc7369；此前 H5 的 a3e0f78 只属于历史阶段视角，已由 ROADMAP-archive.md 留档，不是本阶段当前 HEAD。
- **必须保留的 WIP**：Config/Automation/Presets/1.json，以及所有 Content/** 的修改、删除和未跟踪资源；不清理、不回滚、不纳入本阶段执行者提交。
- **资产事实**：当前主手、Bow、Player Blueprint 和 CombatLoadout 资产仍是用户拥有的本地 WIP；Gemini 不得手工编辑或迁移任何 uasset/umap。

## 冻结后的输入与所有权合同

| 物理意图 | canonical 解析来源 | 保持的执行边界 |
|---|---|---|
| Input.PrimaryAttack | 当前主手 UWeaponDefinition::PrimaryAttackAbilityTag | Player 只发布输入；UPrimaryAttackAbility 与 UBowDrawFireAbility 保留各自生命周期 |
| Sprint Attack | 当前主手 UWeaponDefinition::SprintAttackAbilityTag | Player 继续负责 Dodge/Sprint 时序；缺失 Sprint route 时保持当前普通 Primary fallback 流程，但绝不回读旧 Loadout |
| Input.Guard / Input.Parry | Effective DefenseProfile | OffHand 覆盖、MainHand 回退和通用默认逻辑不变 |
| Input.AbilitySlot.1..4 | DefaultPreparedActions 生成的精确 FGameplayAbilitySpecHandle | 不改为 Tag 路由 |
| Input.Aim | 当前 Bow 专属输入意图 | 不因共享按键而泛化为 Mage/Aim 合同 |
| Dodge、Interact、Lock-On、Target Cycle | APlayerCharacter 的物理输入与时序仲裁 | 不并入 Loadout |

相同按键只证明 UX 统一，不证明 Bow Draw/Hold/Release、近战 Primary 和未来 Mage Ability 具有相同 payload、Montage 或取消语义。

## Direct Route API 与校验

在 UWeaponDefinition 增加两个 authored FGameplayTag 字段：

- PrimaryAttackAbilityTag
- SprintAttackAbilityTag

固定校验合同：

- MainHand 的 PrimaryAttackAbilityTag 必须有效，并使用 AbilityCDO->AbilityTags.HasTagExact(PrimaryAttackAbilityTag) 在自身 BaseGrantedActions 中找到恰好一个匹配 Ability。
- SprintAttackAbilityTag 可以为空；非空时同样必须在自身 BaseGrantedActions 中恰好匹配一个 Ability。
- HandSlot == OffHand 时，两个攻击路由 Tag 必须均无效；误配置直接使 IsValidWeaponDefinition() 失败，并给出明确拒绝原因。
- 缺失、非法、重复匹配或未被当前 BaseGrantedActions 授予的 direct route，在定义验证和 Equipment preflight 中 fail-closed，不产生句柄、输入激活或旧值回退。
- 不新增硬编码的 Melee/Ranged 路由层；基础定义只验证 Tag 与 Ability grant 的关系。
- Bow 保留现有类型约束：PrimaryAttackAbilityTag 必须为 Ability.Attack.Primary，唯一匹配的 BaseGranted Ability 必须继承 UBowDrawFireAbility。
- Bow 完全移除 AssociatedLoadout 必填和 Input.PrimaryAttack -> Ability.Attack.Primary 旧表项校验。
- Melee 不再依赖 AssociatedLoadout 的路由校验；direct fields 由基础定义统一校验。

## Loadout 兼容壳与事务语义

以下内容暂时保留，用于反序列化现有 WIP、旧调用者和迁移期诊断：

- UCombatLoadoutDefinition
- UWeaponDefinition::AssociatedLoadout
- APlayerCharacter::InitialCombatLoadout
- APlayerCharacter::ActiveCombatLoadout
- SetActiveCombatLoadout() / GetActiveCombatLoadout()

固定语义：

- 旧 InputAbilityRoutes 和 Loadout 的 Sprint 字段不再参与 canonical 输入解析。
- InitialCombatLoadout 只可在 BeginPlay() 中作为兼容镜像来源；调用必须先判断非空。清空后不得产生 rejected-null 虚假警告，也不得影响 direct route。
- ActiveCombatLoadout 只表示兼容镜像，不是运行时路由真相。
- 增加窄 C++ ClearActiveCombatLoadout()，不增加 Blueprint 输入合同。
- TeardownEquippedWeapons()、无主手组合、切换到无旧 Loadout 的武器和失败恢复时，显式清空或重新同步镜像。
- ApplyComposition() 的成功与否只由 direct route、定义 preflight、Ability grant、显示组件和 prepared handles 决定；AssociatedLoadout 为空、无效或同步失败都不得使装备失败或触发回滚。
- 成功组合后，若旧 AssociatedLoadout 仍存在且有效，可以同步到兼容镜像；否则清空镜像并记录诊断。镜像同步不改变事务结果。
- 回滚只依据旧武器定义和重新生成的 Ability handles，不保存或依赖 OldLoadout 作为成功条件。

## 批准的源码与测试路径

本阶段原批准路径与审查修复例外如下。Gemini 仅可修改原批准路径；审查发现的测试夹具 P2 由 Main 明确纳入一次性修复范围后，额外允许修改 `MeleeMultiTraceSourceAutomationTests.cpp`，生产源码和公共合同未因此扩展：

- Source/PolyQuest/Public/Combat/Equipment/WeaponDefinition.h
- Source/PolyQuest/Private/Combat/Equipment/MeleeWeaponDefinition.cpp
- Source/PolyQuest/Private/Combat/Equipment/BowWeaponDefinition.cpp
- Source/PolyQuest/Public/Combat/Equipment/WeaponEquipmentComponent.h
- Source/PolyQuest/Private/Combat/Equipment/WeaponEquipmentComponent.cpp
- Source/PolyQuest/Public/Character/Player/PlayerCharacter.h
- Source/PolyQuest/Private/Character/Player/PlayerCharacter.cpp
- Source/PolyQuest/Public/Combat/Input/CombatLoadoutDefinition.h
- Source/PolyQuest/Private/Tests/CombatAutomationFixture.h
- Source/PolyQuest/Private/Tests/CombatAutomationFixture.cpp
- Source/PolyQuest/Private/Tests/WeaponEquipmentComponentAutomationTests.cpp
- Source/PolyQuest/Private/Tests/ProjectileLifecycleAutomationTests.cpp
- Source/PolyQuest/Private/Tests/MeleeMultiTraceSourceAutomationTests.cpp（review repair exception：补齐 I1 direct route 测试夹具）

其他未列出的文件不在范围内。若后续编译或直接调用证据证明必须修改，执行者必须停止并把证据返回 Main，不得自行扩展。

## 执行顺序

1. 在 UWeaponDefinition 加入 direct fields、精确 CDO 匹配和 OffHand 拒绝校验。
2. 修改 Melee/Bow definition validation，去除旧 Loadout 的运行时必填依赖。
3. 将 UWeaponEquipmentComponent 的 Primary/Sprint resolver 切换到 direct fields，补齐 preflight 日志。
4. 收敛装备事务的兼容镜像清理、同步和回滚，不改变 Guard/Parry、prepared handles、显示和 Trace ownership。
5. 在 Player 中保留物理输入事件、held duration、Primary 短按/长按及 Dodge/Sprint 时序；仅调整 Initial/ActiveCombatLoadout 的兼容镜像生命周期。
6. 更新 transient fixture 与 focused Automation；所有 fixture 创建的 MainHand 必须填 direct Primary/Sprint 字段，旧 Loadout 只用于专门的兼容冲突测试。
7. 执行 Rider error-level 检查和 git diff --check 后交还 Main。Gemini 不得修改文档、Config、Content、资产、Gameplay Tags、Input Action 或提交 Git。

## Automation 覆盖

至少覆盖以下矩阵：

- AssociatedLoadout == nullptr 时，Melee 和 Bow 仅凭 direct fields 完成 Equip 与 Primary/Sprint 解析。
- direct route 正确、旧 Loadout 配置错误或冲突时，解析完全以 direct fields 为准。
- MainHand 缺失 Primary、Primary 未匹配 CDO、Sprint 未匹配 CDO 时的定义/preflight 拒绝和零状态变更。
- OffHand 配置攻击路由 Tag 时拒绝。
- Sprint direct route 缺失时无 stale 旧 Sprint route，并保持现有 Primary fallback 行为。
- Guard/Parry 的 OffHand override、MainHand fallback 和默认路径不回归。
- 1-4 空槽、错误句柄、精确 class/handle 匹配不回归。
- Equip、World Pickup、Apply failure、Drop failure rollback 后，武器 direct route、Ability handles 和兼容镜像均与当前组合一致。
- 无 MainHand、Unarmed fallback 和 teardown 后无 stale route identity。
- Player Pressed/Released/Canceled、held duration、Primary short/hold arbitration 不回归。
- Bow Draw/Hold/Release、Projectile snapshot/释放和取消不回归。
- UnPossessed() 对 Light/Skill 的现有取消非对称性只做审计；没有泄漏证据时不扩充 Charged/Sprint 取消列表。

## 用户资产迁移与验证门槛

资产迁移由用户在 Editor 中完成，Gemini 不得直接编辑 uasset：

1. 在以下主手资产中把旧 Loadout 的 Primary/Sprint 值复制到 direct fields：
   - Content/_DataAssets/Weapon/DA_Weapon_Unarmed.uasset
   - Content/_DataAssets/Weapon/DA_Weapon_LightSword.uasset
   - Content/_DataAssets/Weapon/DA_Weapon_HeavySword.uasset
   - Content/_DataAssets/Weapon/DA_Weapon_Axe.uasset
   - Content/_DataAssets/Weapon/DA_Weapon_Bow.uasset
2. 清理 BP_Player 和 BP_test 的 InitialCombatLoadout。
3. direct fields 读回确认后，清理产品武器上的 AssociatedLoadout 引用；保留旧 DA_Melee_CombatLoadout 文件，不盲删。
4. 用 Reference Viewer 检查产品资产对旧 Loadout 的引用；无法覆盖的用户 WIP 单独记录为债务。
5. 读回现有 IMC_Default、Primary/Guard/Parry/AbilitySlot InputAction，确认没有新增或改变物理输入绑定。

用户必须提供：

- 手动 PolyQuestEditor Development Editor 编译结果；
- Equipment transaction、Projectile/Bow lifecycle 和 Player input focused Automation 结果；
- Scene01 PIE 中近战 Primary 短按/长按/Sprint、Guard、Parry、1-4、Bow Draw/Hold/Release、装备切换及失败回滚结果；
- 无重复 Ability 激活、旧 Loadout 覆盖或 stale route 的确认。

静态检查、CodeGraph、Automation 和二进制字符串扫描不能替代 Editor readback 或 PIE 证据。

## I2 交接审计交付物

I1 收口时由 Main 在 plan.md closeout 中附一份《战斗 Tag 当前路由与意图审计表》，作为 TODO-03I2 输入。每行至少包含：

- Tag 名称；
- 分类：物理输入、Ability identity、取消能力、active cancel window、语义阶段事件或 teardown selector；
- 当前 producer/caller 与 consumer/callee；
- payload、Montage 和生命周期语义；
- 是否存在实际冲突；
- I2 的迁移候选或不变理由。

I1 只审计，不执行 Tag 重命名或迁移。Ability.Skill.Melee、Bow phase events、CancelableMeleeAbilityTags 等候选项交由 I2 按证据处理。

## 成功标准、债务与非目标

成功标准：

- Primary/Sprint 的运行时 canonical route 只有武器 direct fields。
- Melee 与 Bow 共享物理输入意图，但保留独立 Ability 生命周期和 payload。
- Guard/Parry 仍由 DefenseProfile 解析，1-4 仍由精确 handles 激活。
- 旧 Loadout 冲突不会覆盖 direct fields；缺失/非法 direct route fail-closed。
- 装备、拾取、回滚和 teardown 后无 stale route identity。
- Trace、Resolver、Damage GameplayEffect、Projectile、Lock-On、Defense、Poise、Hit Reaction/Feedback ownership 不变。

保留的非阻塞债务：

- 旧 Loadout 类型、字段和资产的最终删除，等待产品资产零引用后的独立清理阶段。
- Controller/硬件输入验证仍是独立门槛。
- Tag taxonomy migration 属于 TODO-03I2。
- Staff/Mage targeting/delivery 属于条件阶段 TODO-03I3。

明确非目标：

- 不实现 Mage/Staff、AOE、Beam、新 Projectile 或新 Input Action。
- 不批量重命名 Gameplay Tags。
- 不新增通用 Ability dispatcher、Melee/Ranged 框架、FSM、全局输入服务或第二 GAS 权威。
- 不改 Motion-Warp、Trace、Resolver、Damage、Guard/Parry、Poise、Hit Reaction、Enemy AI/StateTree。
- 不删除、移动、重命名或手工修改 Content/**、旧 Loadout 资产、Config WIP 或导入资源。

依赖顺序：

~~~text
TODO-03H5
  -> TODO-03I1
  -> TODO-03I2
  -> TODO-05A
  -> TODO-05B
  -> TODO-07B5
  -> TODO-07B6
  -> TODO-03C
~~~

TODO-03I3 仅在具体 Staff/Mage 路线被接受后启动；TODO-03A7E 仍位于首个远程敌人之后。

## Main 收尾职责

- Gemini 只提供变更路径、静态检查、未运行用户门槛、严格 self-review 和剩余风险。
- Main 在用户验证后执行一次 bounded defect-first fresh review；发现跨合同或范围问题时停止并重新定界。
- Main 负责 plan.md closeout、ROADMAP.md 指针/债务同步、ARCHITECTURE.md 稳定合同更新、必要的 ROADMAP-archive.md 归档、staging 和 commit。
- 本阶段未获得用户验证前，不得声称编译、Editor readback、Automation 或 PIE 已通过。

## 实施、复核与收口记录

- **执行路由**：`manual/out-of-band Gemini`；Main 保留架构合同、范围判断、验证解释、fresh review、文档、staging 与 commit 所有权。Gemini 未修改文档、Config、Content、资产或 Git 状态。
- **实际变更路径**：`WeaponDefinition.h`、`MeleeWeaponDefinition.cpp`、`BowWeaponDefinition.cpp`、`WeaponEquipmentComponent.cpp`、`PlayerCharacter.h/.cpp`、`CombatLoadoutDefinition.h`、`CombatAutomationFixture.cpp`、`WeaponEquipmentComponentAutomationTests.cpp`、`ProjectileLifecycleAutomationTests.cpp`、`MeleeMultiTraceSourceAutomationTests.cpp`，共 11 个 Source/test 文件。`CombatAutomationFixture.h` 虽在原批准列表中，但实际未修改。
- **审查修复**：Main fresh review 发现 `MeleeMultiTraceSourceAutomationTests.cpp` 五个主手 transient fixture 未补齐 I1 direct route，属于 P2 测试夹具缺口。Gemini 仅在该测试文件补充 `UPrimaryAttackAbility` 与 `Ability.Attack.Primary`，修复后 delta review 未发现 P0/P1/P2 blocker；未改变生产代码或公共合同。
- **用户验证证据**：用户确认 I1 修复后的 PIE 与 Automation 通过；Gemini 交接报告记录 `PolyQuest.Equipment.TransactionMatrix`、`PolyQuest.Projectile.Lifecycle` 以及相关 focused 路径通过。该证据不扩展为 Main 重新运行的全量套件、独立编译或完整资产复现证明。
- **静态证据**：Gemini 报告 Rider error-level 检查为 `errors: []`，并报告 `git diff --check` 通过；审查修复文件另行完成相同检查。Main 未重复调用 Rider/编译/Automation/PIE。
- **未声称门禁**：没有独立手动 `PolyQuestEditor` Development Editor 编译收据，也没有可归档的完整产品资产逐项 Editor readback（direct fields、零 Loadout 引用、输入映射）。用户报告的资产迁移与 PIE 结果保留为用户证据，不改写为可复现 clean-checkout baseline。
- **提交边界**：本次提交只包含上述 11 个 Source/test 文件、`plan.md`、`ARCHITECTURE.md`、`README.md`、`ROADMAP.md` 与 `ROADMAP-archive.md`。`Config/Automation/Presets/1.json`、全部 `Content/**`、其他 Source/Config/文档 WIP、资产、Build.cs 与 `.uproject` 均排除。

## 《战斗 Tag 当前路由与意图审计表》

下表覆盖 I1 直接触及的输入路由与 I2 候选；纯反应/资源数据 Tag 不在本阶段拥有者决策范围内。

| Tag | 分类 | 当前 producer/caller -> consumer/callee | Payload、Montage 与生命周期 | 冲突与 I2 处理 |
|---|---|---|---|---|
| `Input.PrimaryAttack` | 物理输入 | `APlayerCharacter` -> `UWeaponEquipmentComponent` / `UPrimaryAttackAbility` | `Pressed/Released/Canceled` 携带输入 Tag 与 held duration；Primary Ability 再分流 Light/Charged | 无直接冲突；保留通用物理意图 |
| `Input.Aim` | 物理输入 | `APlayerCharacter` -> 当前 Bow authored route | 当前只表达 Bow Aim/锁定辅助输入；没有通用 Mage consumer | Bow 专属；只有具体 Staff 路线证明等价后再评估 |
| `Input.Guard`, `Input.Parry` | 物理输入 | `APlayerCharacter` -> Effective Defense Profile -> Guard/Parry Ability | Guard 持续输入；Parry 窗口由独立事件/Ability 生命周期拥有 | 无 direct-route 冲突；保留 DefenseProfile 所有权 |
| `Input.AbilitySlot.1..4` | 物理输入 | `APlayerCharacter` -> `TryActivatePreparedSlot` | 通过装备组件保存的精确 `FGameplayAbilitySpecHandle` 激活，不依赖 Tag | 无；保留精确 handle 合同 |
| `Ability.Attack.Primary` | Ability identity / direct route | `UWeaponDefinition` direct field -> `UPrimaryAttackAbility` 或 `UBowDrawFireAbility` CDO | 当前装备的 MainHand 只要求一个精确匹配的 granted CDO；各 Ability 保持独立 Montage/payload | 共享名称不等于共享生命周期；I2 只检查语义，不合并类 |
| `Ability.Attack.Light` | Ability identity / cancellation selector | `ULightAttackAbility` -> Dodge/Parry/Big reaction cancellation | Light Combo 自有 Montage、Trace Window、Cost 与快照 | 无已证实冲突；保留 |
| `Ability.Attack.Charged` | Ability identity / cancellation selector | `UChargedAttackAbility` -> Dodge/Parry/Big reaction cancellation | Hold/Release 与 Charged Montage 自有生命周期 | 与 Bow hold/release 仅输入相似；不合并 |
| `Ability.Attack.Sprint` | Ability identity / cancellation selector | `USprintAttackAbility` -> Dodge/Parry/Big reaction cancellation | Sprint Attack 自有 Task、Montage 与 Guard/Sprint 过渡 | 无已证实冲突；保留 |
| `Ability.Skill.Melee` | Ability identity + cancellation selector | `UPlayerMeleeSkillAbility`；也被 Dodge/Parry/Big/Launch 取消列表与 `UnPossessed()` 使用 | 当前是 Melee Skill 类别 tag，取消语义与具体技能身份共用；Whirlwind 具体身份另由 authored tag 表达 | 有过载风险；I2 评估是否拆为 weapon-independent cancellation capability，不能直接改成宽泛 `Ability.Skill` |
| `Ability.Skill.Whirlwind` | Ability identity | Whirlwind authored Ability/GE -> 具体 Skill 路由 | 具体技能身份与冷却，不作为所有技能的取消类别 | 无直接冲突；保留具体身份 |
| `Event.Input.Pressed/Released/Canceled` | 语义输入事件 | `APlayerCharacter::SendCombatInputEvent` -> active Abilities 的 `WaitGameplayEvent` | 事件携带 `Input.*` 与 held duration；不直接是通用 AbilityTrigger | 外层事件不能区分输入意图；新 event-trigger Ability 必须校验 payload 或使用专用外层 Tag |
| `Event.Attack.Bow.DrawReady`, `Event.Attack.Bow.Release` | 语义阶段事件 | Bow AnimNotify -> `UBowDrawFireAbility` | Bow Draw/Hold/Release 的 Montage 身份与释放时序 | 与 Charged Hold/ReleaseHandoff 语义不同；I2 默认不合并 |
| `Event.Attack.Charged.HoldReady`, `Event.Attack.Charged.ReleaseHandoff` | 语义阶段/交接事件 | Charged Notify、`UPrimaryAttackAbility`/`UChargedAttackAbility` -> Charged release handoff | HoldReady 标记蓄力阶段；ReleaseHandoff 传递 held duration 并触发释放 | 与 Bow 仅共享“长按-松开”表面形态；保留独立事件 |
| `Event.Action.CancelWindow.Dodge.Begin/End` | active cancel window 事件 | Action Window NotifyState -> Light/Charged/Sprint/Skill/Bow/Launch listeners | 由 Montage 时间窗发送 Begin/End；监听者据此设置可取消状态 | 名称偏 Dodge，但现有窗口也开启 Defense cancel；I2 核对是否应改为中性命名 |
| `State.Action.CanCancel.Dodge`, `State.Action.CanCancel.Defense` | active cancel window 状态 | 各 Ability 窗口 task 写入/清除 -> `UDodgeAbility`、Guard/Defense 路径读取 | ASC loose tag 计数表达当前动作可被哪类输入取消 | 语义已按取消能力拆分；I2 保留，除非调用矩阵证明命名/所有权缺陷 |
| `Event.Attack.TraceWindow.Begin/End` | 语义阶段事件 | `UAnimNotifyState_AttackTraceWindow` -> `UAbilityTask_MeleeTraceWindow` | Notify 时间窗驱动接触 Trace；payload 含 Animation/Notify 身份 | 不属于输入路由；不迁移 |
| `Ability.Defense.Guard`, `Ability.Defense.Parry` | Ability identity / Defense resolver | Effective Defense Profile -> Guard/Parry CDO | Guard/Parry 自有状态、窗口、GE 与取消清理 | Shield 子 Tag 是层级 child；I2 只在真实语义冲突时调整 |

I1 的结论是：物理输入可以统一，Ability identity、阶段事件、取消能力和 active state 不能仅凭按键相同而合并。I2 的第一输入是本表及其直接调用者，而不是一次性全量 Tag 改名。
