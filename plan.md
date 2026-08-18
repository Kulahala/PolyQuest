# TODO-03H1：Weapon And Combat Architecture Health And Lean Review v1

## 状态与目标

- Plan state: COMPLETED
- Baseline: 9ac9d35（TODO-03AI2 距离感知加权近战接近与攻击）
- Prerequisites: TODO-03A5B、TODO-03AI、TODO-03AI1、TODO-03AI2 已完成；TODO-03A4B 仅在真实 partial-Guard 武器出现时纳入。
- Primary question: 进入 TODO-03B 前，武器、近战 Trace、敌人静态几何和 GAS 命中链是否保持单一真实来源、清晰所有权和可控扩展边界；是否存在有完整证据支持的死兼容路径可以安全删除。

~~~
Outer: ue-stage-workflow
Primary: ue5-architecture
Support: ue5-cpp-gameplay, ue5-debug-validation
Route reason: 本阶段是跨武器、近战 Trace、敌人几何和 GAS 命中链的架构健康审查，必要时只做证据充分的最小私有清理。
~~~

## 当前事实与锁定结论

- UWeaponEquipmentComponent 是 APlayerCharacter 专属，拥有玩家装备事务、AbilitySpec、输入路由、显示组件和 Marker 生命周期；本阶段不抽通用 WeaponComponent 基类。
- AWorldWeaponPickup 只负责世界表示、交互、掉落和地面投影，不拥有伤害、Trace 或 Ability 授予逻辑。
- 敌人不使用玩家装备事务，而是通过 UMeleeTraceSourceComponent -> StaticMeleeWeaponDefinition 读取共享静态武器几何。
- 玩家 Light、Charged、Sprint、技能攻击与敌人近战共用一条命中链：UAbilityTask_MeleeTraceWindow -> SweepMultiByChannel -> FMeleeHitResolver -> Damage GameplayEffect。
- UWeaponDefinition、UMeleeWeaponDefinition、UEnemyAttackProfile、UEnemyAttackSet、UEnemyAIProfile 只保存作者化数据；运行时状态仍位于 Component、Controller 或 Ability。
- Weapon Display、敌人固定武器和 Pickup Mesh 不通过碰撞、Overlap 或物理接触造成伤害。
- StaticMeleeWeaponDefinition 配置后严格 fail-closed；只有未配置静态定义时才允许使用 BladeTraceBase / BladeTraceTip fallback。
- 当前没有提前引入通用 Weapon Actor、Projectile 基类或 Projectile Damage Framework。
- 旧 BladeTraceBase / BladeTraceTip fallback 仍被当前敌人资产和 Scene01 引用，本阶段不能直接删除。

## 审查与实施切片

### Slice A：静态所有权与引用闭包审查

Main 负责：

- 读取 UWeaponEquipmentComponent、UMeleeTraceSourceComponent、UMeleeWeaponDefinition、FMeleeHitResolver、UAbilityTask_MeleeTraceWindow、UEnemyMeleeAbility、ABaseCharacter 和 AWorldWeaponPickup 的直接调用链。
- 扫描所有 Sweep、Damage GameplayEffect 创建和应用点，确认不存在第二条近战伤害路径。
- 扫描 DataAsset 的运行时写入，确认不存在装备状态、Active Ability、Trace 历史或冷却状态泄漏到作者化资产。
- 检查 BP_Player、BP_Enemy_Goblin、ST_Enemy_Goblin_Melee、武器 DataAsset 和 Scene01 的引用关系。
- 形成“保留、可删、需 Editor 证明”的明确清单。

.code-review-graph 当前构建于 74ab01c，落后于本阶段基线 9ac9d35；其结果只能作为补充证据，必须以直接源码扫描和 Editor/Reference Viewer 读回为准。

### Slice B：有证据的最小清理

Gemini 仅可执行 Main 在 Slice A 后明确列出的具体清理项。删除某个兼容符号、字段或私有路径必须同时满足：

1. 直接源码、配置和文本引用扫描为零；
2. CodeGraph/代码关系扫描没有调用者或资产入口；
3. Unreal Editor 的资产引用/Reference Viewer 读回确认没有 Blueprint、DataAsset、StateTree、Map 或其他内容引用；
4. 删除后有针对性回归测试或等价验证；
5. 不涉及导入资产、.uasset/.umap 手工修改、类路径迁移或公共 API 重构。

仅 CodeGraph 显示零引用，不足以删除。

当前明确保留：

- UMeleeTraceSourceComponent 的 BladeTraceBase / BladeTraceTip fallback；
- ABaseCharacter::DisableFixedWeaponDisplayCollision() 的敌人固定 WeaponMesh 碰撞保护；
- 玩家装备组件的动态 Display/Marker 创建与销毁；
- AWorldWeaponPickup 通用世界容器；
- 现有 FMeleeHitResolver 和 UAbilityTask_MeleeTraceWindow 共享伤害路径。

若没有满足删除准入的候选，禁止为了制造改动而重构；本阶段可以以“审计无合格清理项”完成。

### Slice C：文档与债务收口

通过验证后：

- ARCHITECTURE.md 只记录稳定的所有权、数据流和单一近战伤害契约。
- ROADMAP.md 将 TODO-03H1 标记为完成。
- 若旧 fallback 仍保留，记录明确债务：待所有敌人切换到静态 Socket 定义且 Reference Viewer 确认零引用后，才允许独立清理。
- plan.md 保留本阶段 closeout、验证证据、复核来源和未关闭风险。
- 不把临时作者化步骤写入 ARCHITECTURE.md。

## 允许与禁止修改

允许修改，但仅限实际证据支持的最小范围：

- Source/PolyQuest/Public/Combat/Equipment/WeaponEquipmentComponent.h
- Source/PolyQuest/Private/Combat/Equipment/WeaponEquipmentComponent.cpp
- Source/PolyQuest/Public/Combat/Melee/MeleeTraceSourceComponent.h
- Source/PolyQuest/Private/Combat/Melee/MeleeTraceSourceComponent.cpp
- Source/PolyQuest/Public/Combat/Equipment/MeleeWeaponDefinition.h
- Source/PolyQuest/Private/Combat/Equipment/MeleeWeaponDefinition.cpp
- Source/PolyQuest/Public/Combat/Melee/MeleeHitResolver.h
- Source/PolyQuest/Private/Combat/Melee/MeleeHitResolver.cpp
- Source/PolyQuest/Public/AbilitySystem/Tasks/AbilityTask_MeleeTraceWindow.h
- Source/PolyQuest/Private/AbilitySystem/Tasks/AbilityTask_MeleeTraceWindow.cpp
- 直接受影响的敌人近战 Ability、BaseCharacter 私有实现和 Automation 测试。
- ARCHITECTURE.md、ROADMAP.md、本 plan.md 的审查和收尾记录。

禁止：

- 抽 UWeaponComponent 基类或改变玩家/敌人的所有权模型；
- 修改 ASC、AttributeSet、GameplayTag、输入、Ability 生命周期或共享伤害契约；
- 引入 Bow、Projectile、远程敌人、Boss、持久化或新的通用 Weapon/Projectile Framework；
- 修改装备行为、攻击手感、Trace 几何或 StateTree 行为以外的功能；
- 手工编辑、删除、迁移或重命名 .uasset、.umap、导入资产或 Blueprint 类路径；
- 删除 Content/、Config/ 或其他用户 WIP；
- 无证据的格式化、泛化重构或提交 Git。

## 验证矩阵

### 静态验证

- CodeGraph 调用者、调用链和引用扫描。
- .code-review-graph 影响/关系扫描，并明确注明基线落后时的覆盖限制。
- 直接 rg 扫描 C++、配置、文档和可读资产线索。
- 对每个候选删除项记录“为什么可删”和“Editor 证据在哪里”。
- git diff --check。

### Automation

保持以下测试通过：

- PolyQuest.Melee.TraceSourceGeometry
  - 玩家动态 Marker；
  - 敌人静态 Socket；
  - 静态定义非法时 fail-closed；
  - 静态定义为空时才使用旧 fallback；
  - 固定 WeaponMesh 不阻挡 Camera、Visibility、WorldStatic。
- PolyQuest.Equipment.TransactionMatrix
  - Sword、Unarmed、TwoHanded、Shield、Apply rollback、Drop rollback；
  - 不因审查性修改破坏玩家装备事务。
- PolyQuest.Enemy.AttackSetSelection。
- PolyQuest.Enemy.CombatSpacing。

### 用户编译与 Editor Readback

由用户执行：

- 编译 PolyQuestEditor；
- 读取 BP_Player：只有一个玩家 UWeaponEquipmentComponent；
- 读取 BP_Enemy_Goblin：无玩家装备事务组件，MeleeTraceSource.StaticMeleeWeaponDefinition 指向共享武器定义；
- 读取 DA_Weapon_Axe 的 WeaponMesh、Trace_Base、Trace_Tip；
- 使用 Reference Viewer 确认任何候选删除符号没有 Blueprint、DataAsset、StateTree 或 Map 引用；
- 确认固定敌人武器显示为 NoCollision、不生成 Overlap、忽略 ECC_Camera。

### PIE

由用户执行 Scene01：

- 玩家 Sword、Unarmed、Shield 装备与拾取回归；
- 被替换装备仍正常掉落；
- Goblin 使用共享武器 Socket 产生近战命中；
- 敌人 WeaponMesh 不遮挡摄像机；
- 玩家和敌人继续使用同一 Trace、Resolver、GAS 命中链；
- 无新增碰撞伤害或第二次 DamageEffect 应用。

## 复核与提交门禁

- Gemini 执行严格自审，只覆盖实际清理差异和直接调用链。
- Main 先执行 normal review，再执行 adversarial fallback。
- 若 gpt-5.6-luna 可用，启动独立只读 Fresh Reviewer；不可用时明确记录为 Main adversarial fallback，不宣称独立复核。
- 只有用户确认编译、Editor Readback、Automation 和 PIE 后才可收尾。
- 提交仅包含实际修改的原生源码、测试和三份文档；排除 Content/、Config/、.uproject、导入资产、删除项和其他用户 WIP。
- 允许无 C++ 改动、仅审查文档收口的干净结果，不强行制造重构。

## 委托记录与交接

~~~
Plan explorers: 0
Implementation executors: 1
Complex Executor: none
Main parallel work: none
Reason: 架构判断、公共契约、GAS 生命周期、Editor 引用证据和文档由 Main 保持所有权；Gemini 只执行 Main 明确批准的低耦合私有清理，若无合格候选则不改代码。
~~~

Gemini 执行提示词：

~~~
工作目录：E:/GameDevelop/PolyQuest
基线：9ac9d35
先读取当前 plan.md，并使用 ue-stage-workflow、ue5-architecture、ue5-cpp-gameplay、ue5-debug-validation。

本阶段目标是 TODO-03H1 架构健康与瘦身审查，不是新功能开发。
先按计划完成引用闭包和近战伤害链审计，再等待 Main 明确列出可删除的具体符号/路径。
不得只凭 CodeGraph 删除；每个删除项必须有直接源码扫描、CodeGraph 关系和 Unreal Editor/Reference Viewer 读回证据。
当前已知仍需保留 BladeTraceBase/BladeTraceTip fallback、玩家专属 UWeaponEquipmentComponent、AWorldWeaponPickup、共享 UAbilityTask_MeleeTraceWindow/FMeleeHitResolver 和敌人 StaticMeleeWeaponDefinition 路径。

允许修改范围仅限 Main 明确批准的 Source/PolyQuest 私有实现、配套 Automation 测试和必要文档。
禁止抽 WeaponComponent 基类、改变公共 API、改 ASC/GameplayTag/Input/Ability 生命周期、引入 Projectile Framework、修改装备/伤害行为、手工编辑 .uasset/.umap、删除 Content、扩大重构或提交 Git。
若没有满足删除准入的候选，保持 C++ 不变并报告“审计无合格清理项”。

完成后执行严格自审，报告：
1. 实际修改路径；
2. 每个删除项的三类证据；
3. 直接调用链与单一伤害路径确认；
4. Automation/静态检查结果；
5. 用户编译、Editor Readback、PIE 尚需的证据；
6. 未解决债务与停止原因。
不要把自审称为 Main fresh review。
~~~

## Closeout Record

- **审计结论**：
  - 武器装备体系（`UWeaponEquipmentComponent` 玩家专属、`AWorldWeaponPickup` 仅为世界交互容器、DataAsset 纯不可变数据）完全符合单一真实来源。
  - 近战轨迹与静态几何（`UMeleeTraceSourceComponent -> StaticMeleeWeaponDefinition`）严格实施 Fail-Closed，摄像机通道忽略策略有效保障弹簧臂稳定。
  - 近战伤害链（`MeleeTraceWindow -> FMeleeHitResolver -> Damage GE`）为全项目唯一近战伤害路径，格挡/弹反优先终态拦截，无第二伤害路径或穿透漏洞。
  - 敌人 AI 意图（StateTree 宏观意图）与执行决策（`AEnemyAIController` 决策快照 + GAS Ability 严格 Commit/EndAbility 清理）边界清晰。
  - 项目 Gameplay Tag 注册与当前源码使用保持对齐，未发现未注册 Tag 或本阶段新增的死符号。
- **实际清理**：
  - 0 个 C++ 源码改动。经代码全局扫描，旧残留（`GrantedWeaponAbilities` 等）早已清空，无合格/必要的代码修改项，不强行重构。
- **Automation**：
  - 用户确认运行结果全部为 `Success`：
    - `PolyQuest.Enemy.AttackSetSelection`：Success
    - `PolyQuest.Enemy.CombatSpacing`：Success
    - `PolyQuest.Equipment.TransactionMatrix`：Success
    - `PolyQuest.Melee.TraceSourceGeometry`：Success
  - 测试日志中的 Warning 均为测试套件主动注入非法输入以验证 Fail-Closed 与回滚机制的预期行为。
- **编译 / Editor Readback / PIE**：
  - 自动化测试 100% 通过；沿用 `TODO-03AI2` 编译与 Scene01 PIE 证据（0 C++ 源码修改无新回归面）；未在本会话中宣称新的独立 PIE 录屏或编译日志。
- **复核来源**：
  - Gemini strict executor review（确认 0 C++ 修改合规性与无破坏原则）；
  - Main normal review 与 adversarial review（确认无 P0-P2 源码/生命周期缺陷）；
  - 未启动 Luna/gpt-5.6-luna 独立 Reviewer，不宣称独立复核结果。
- **保留债务及关闭条件**：
  - `UMeleeTraceSourceComponent` 的 `BladeTraceBase` / `BladeTraceTip` legacy fallback 仍保留，当前仍被 `BP_Enemy_Goblin` / `Scene01` 引用。
  - 关闭条件：所有敌人切换到 `StaticMeleeWeaponDefinition` 且 Unreal Editor Reference Viewer 确认零引用（不绑定到特定阶段代号）。
- **提交范围**：
  - 仅包含 4 份文档同步（`README.md`、`ARCHITECTURE.md`、`ROADMAP.md`、`plan.md`），0 C++ 源码修改；
  - 严格排除所有 `Content/**`、`Config/**`、`.uproject` 等本地 WIP 资产。
