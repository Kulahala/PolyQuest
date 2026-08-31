# TODO-03H5：Post-Motion-Warp Combat Health Review v1

## 计划状态与阶段目标

- **状态**：H5 Review-Only 健康审查已完成；未发现 P0/P1/P2 blocker，未产生 Source 修复、调参、格式化或顺手优化。
- **主要运行时问题**：确认 TODO-03A7B、TODO-03A7C、TODO-03A7D、TODO-03A7F 收口后的四个 Player Motion-Warp 消费者，在触发区间、Ability-local 快照、Montage/GameplayTask 重入、能力取消、目标失效和伤害链方面没有引入回归。
- **阶段目标**：用源码、Config、可读资产契约和已有验证收据建立一份可追溯的健康审查结论；只有证据支持的 P0/P1/P2 blocker 才进入后续最小修复。
- **阶段结果**：确认四个消费者、共享 evaluator、Player/Enemy 生命周期桥、ASC/Tag/Input 边界和唯一伤害链没有证据支持的 P0-P2 回归；3.17 exact-stop 保持非阻塞 P3，后续指针推进到 `TODO-03I1`。

## 当前基线与工作区快照

- **分支**：`main`。
- **当前基线**：`main @ a3e0f78a1cfd9281d5ed9a29e4e9e7e4994d4535`，提交标题为 `[Feature] 统一近战 Motion-Warp 触发距离契约 (Unify Melee Motion-Warp Trigger Range Contract)`。
- **历史基线说明**：`ROADMAP-archive.md` 中的 `7c3ddf0`、`c91fbfd`、`90ad381` 等只代表各自阶段当时的任务视角；它们不替代本阶段当前 HEAD。此前阶段最新提交尚未形成时，使用上一提交作为父基线是历史记录语义，不应回写成 H5 当前基线。
- **工作区**：`Source/` 当前干净；`Config/Automation/Presets/1.json` 和大量 `Content/**` 含用户修改、删除及未跟踪资源，全部保留，不清理、不回滚、不纳入 H5。
- **文档状态**：A7F 详细 closeout 已在 `ROADMAP-archive.md` 保留；本文件现保存 H5 计划与审查 closeout。H5 收口后，`ROADMAP.md` 的当前指针推进到 `TODO-03I1`。
- **已知历史证据**：用户确认过 A7B/C/D/F 相关 focused Automation 和 Scene01 PIE；Gemini 报告过 Rider 检查、编译和 `git diff --check`。这些收据只覆盖各自明确场景，不自动成为 H5 新验证或完整 authored 资产证明。

## 路由、所有权与工具预算

~~~text
Outer: ue-stage-workflow
Primary: ue5-debug-validation
Support: ue5-cpp-gameplay
Execution route: manual/out-of-band Gemini only if Main later assigns a bounded slice
Implementation executors during audit: 0
Contract owner: Main
~~~

- Main 负责合同解释、审查范围、发现定性、验证解释、文档、staging 和 commit；当前没有实现执行者。
- 默认先以当前批准边界的 diff/源码和一跳直接 callers/callees 为主线。使用 CodeGraph 做定点导航；当前 `code-review-graph` 建立于旧 SHA，只作为标注为 stale 的补充影响证据，不作为覆盖或运行时证明。
- 只有出现 P0/P1 迹象，或发现共享 API、异步生命周期、Gameplay Tag/Input、资产契约风险时，才做最小扩展；每次扩展必须记录新增证据。禁止用重复全局扫描替代结论。

## 审查范围与冻结合同

### 直接审查路径

- 四个 Player Ability 的现有激活、Motion-Warp helper、Montage/Task 和 `EndAbility()`：`LightAttackAbility`、`ChargedAttackAbility`、`SprintAttackAbility`、`PlayerMeleeSkillAbility`。
- 共享 `MeleeMotionWarping` 的 `IsConfigValid()`、`EvaluateMeleeMotionWarpTransform()`，以及 Player/Enemy 的锁定、目标失效、移动模式、UnPossess、死亡和 EndPlay 桥接。
- `UAbilityTask_MeleeTraceWindow -> FMeleeHitResolver -> Damage GameplayEffect` 的直接调用链，以及其 Guard/Parry、Poise、Hit Reaction/Feedback 边界。
- 直接关联的 Gameplay Tags、Input、`PolyQuest.uproject` 和 `PolyQuest.Build.cs`；只用于契约核对，不借机重构 taxonomy 或输入系统。

### 必须保持的运行时合同

- 四个消费者继续使用 `MinTriggerDistance <= WarpStopDistance <= MaxTriggerDistance`；精确停距为 no-op，`Min < Stop` 的反向修正保持有界，`Min == Stop` 保持前向-only。
- 快照属于 Ability 实例，首次合法调用一次性消耗捕获机会，目标位置静态复用；死亡、销毁、异 World、离地、取消和 teardown 必须 fail-closed 并清理 Warp target。
- Player 拥有 Motion-Warp component 和窄桥，Ability 拥有激活、取消、成本、异步任务和快照生命周期；`ABaseCharacter` 继续拥有单一 ASC/AttributeSet。
- 四个 Ability 仍只通过 `Trace Window -> Resolver -> Damage GameplayEffect` 造成近战伤害；不新增第二伤害路径，不改变 Guard/Parry、Poise、Hit Reaction/Feedback 的 ownership。

## 审查矩阵与处置规则

### 1. 配置、区间与快照

- 核对 disabled、非法配置、区间边界、精确停距、前向和反向输入在共享 evaluator 与四个消费者映射中是否一致。
- 核对 Lock-On 清除、目标死亡/销毁交接、移动模式改变和异步结束后是否存在 stale Warp target、错误重选或快照跨 Ability 污染。
- 不把现有数值或 authored readback 缺口当作运行时缺陷；只有源码或用户行为证据支持时才升级严重级别。

### 2. `ReadyForActivation()` 同步重入（首要门禁）

- 对每个 Montage/Notify Task 检查：`ReadyForActivation()` 返回后，是否先确认 Ability 仍处于有效激活状态、`bEndAbilityRequested` 未置位、Task UObject 仍有效且身份未被替换，再读取 `IsActive()`、`IsFinished()` 或 Montage 状态。
- 覆盖 Montage 无效、立即完成、取消、同步 `EndAbility()`、`EndTask()`、Montage 替换、死亡和 EndPlay 后的路径；禁止在同步终态后继续解引用成员、恢复失效旧 Task 或写入旧状态。
- 以 A7D 已收敛的防重入模式作为对照，但不假定 Light/Charged/Sprint 已自动具备相同保护；发现候选点时先给出具体路径和证据，不直接称为 blocker。

### 3. Player/Enemy 生命周期、ASC 与 UnPossess

- 核对 `ClearLockedTarget()`、目标失效窄桥、`OnMovementModeChanged()`、`UnPossessed()`、Player/Enemy `EndPlay()` 和死亡 teardown 的顺序、World/对象有效性和幂等性。
- `UnPossessed()` 当前显式取消 `Ability.Attack.Light` 与 `Ability.Skill.Melee`，Charged/Sprint 的非对称性不自动视为缺陷。若审查确认 Warp 已清空、快照已失效且没有野指针、脏数据或 active Ability 泄漏，则定性为设计差异，交给 `TODO-03I1/03I2` 统一梳理，不在 H5 扩充取消列表。
- 只有发现可复现的 active Ability 泄漏、stale 状态、崩溃或玩家可见硬回归，才把该非对称性升级为 H5 blocker。

### 4. 伤害与相邻回归

- 确认四个 Ability 仍经由唯一的 `Trace -> Resolver -> Damage GE` 路径，且 Motion-Warp 不绕过或重复触发 Guard/Parry、Poise、Reaction 或 Feedback。
- Lock-On、Bow/Projectile、Enemy AI/StateTree/Poise/Death 只检查一跳直接受影响的边界，不进行无关系统总审查。

### 5. 已知 P3 只核对、不扩大

- A7F 的 3.17 exact-stop 测试限制保持为已知非阻塞 P3：`StartComboEntry()` 在 evaluator 结果观察前会先清理预存 Warp target。
- 本阶段只确认该限制没有恶化为 P1/P2，不为改善测试形态而改变生产清理顺序。
- authored GA/GE/Montage/Notify/Modifier/Root Motion readback、独立手动 `PolyQuestEditor` 编译和真实跨帧 Task seam 缺口同样只记录关闭触发器，不改写为运行时失败。

### 严重级别与条件修复

- **P0/P1/P2**：必须记录 Finding ID、严重级别、绝对路径和行号、证据类型、影响、复现或覆盖缺口以及关闭方式。
- **P3**：只记录为 Recommendation/Validation Debt，必须写明受影响边界、当前证据、玩家/技术影响、关闭触发器和归属阶段。
- 发现 P0-P2 后，Main 先判断是否属于已批准路径和 Main narrow-fix 例外。符合时只修现有函数内的最小生命周期、空安全、清理或数据完整性问题；不符合时停止并请求新的范围/执行授权。
- 任何修复都不得新增公开 API、Gameplay Tag、Input、Config 字段、全局 service、消费者、资产迁移或第二伤害路径。审查阶段禁止“顺手优化”、格式化和无证据重构。

## 验证、证据与停止条件

### 当前审查阶段（已完成）

- H5 审查全程为只读：不编译、不启动 Editor、不运行 Automation/PIE、不写入资产、不修改 Config WIP、不 stage/commit。
- 已有 A7B/C/D/F Automation、Scene01 PIE、编译和静态检查收据只按其实际覆盖范围引用；本阶段没有把它们包装成新的运行时或视觉证明。

### 若发生批准的修复

1. 修改文件执行 Rider error-level 检查和 `git diff --check`。
2. 只运行受影响的 focused Automation；不以测试数量替代行为覆盖说明。
3. 用户负责手动 `PolyQuestEditor` Development Editor 编译、相关 authored 资产 Editor readback 和 Scene01 PIE。
4. Main 对最终 diff 做一次 bounded defect-first fresh review，再决定文档收口和提交。

### H5 成功标准与实际结果

- 四个消费者、共享 evaluator、Player/Enemy 生命周期桥、ASC/Tag/Input 边界和唯一伤害链均有可追溯结论。
- 没有未处理的 P0-P2 blocker；本阶段无需进入代码修复或用户重新验证门禁。
- ReadyForActivation 同步重入风险已逐 Ability 给出结论；3.17 exact-stop P3 未被误升级；UnPossess 非对称性已按证据定性为设计差异并留给 `TODO-03I1/03I2`。
- 交付物为零源码变更的审计 closeout；没有产生任何“顺手修复”。

## 文档收口、依赖与非目标

- Main 在审查/验证结束后维护本文件的 H5 closeout；`ROADMAP.md` 只在阶段状态、债务触发器或当前/下一阶段指针变化时更新。
- `ARCHITECTURE.md` 只有在发现并验证稳定架构合同变化时才更新；`README.md` 只记录已证实的公开状态；历史细节进入 `ROADMAP-archive.md`。
- 若 Review-Only 收口无 blocker，`ROADMAP.md` 当前指针推进至 `TODO-03I1：Unified Combat Input Contract And Loadout Simplification v1`，并保留未关闭的 authored-validation debt。
- 后续依赖顺序：`TODO-03H5 -> TODO-03I1 -> TODO-03I2 -> TODO-05A -> TODO-05B -> TODO-07B5 -> TODO-07B6 -> TODO-03C`；`TODO-03I3` 仍是具体 Staff/Mage 路线出现后的条件阶段，`TODO-03A7E` 仍位于首个远程敌人之后。
- **非目标**：重新实现或调参 Motion-Warp、新增消费者、资产迁移/清理、Config WIP、Tag/Input 重命名、Mage/Bow 新功能、Enemy Motion-Warp、通用 dispatcher/service、第二伤害路径、Punish、网络/回滚，以及未经证据支持的 Player/Enemy 行为扩展。

## 计划与审查收据（2026-08-31）

- **审查基线**：`main @ a3e0f78a1cfd9281d5ed9a29e4e9e7e4994d4535`；Gemini 只读检查了四个 Player Ability、共享 evaluator、Player/Enemy 生命周期桥、Trace Window、`FMeleeHitResolver`、直接测试和相关契约。
- **审查结论**：四个消费者统一遵守显式距离区间和 Ability-local 静态快照；`ReadyForActivation()` 返回后的终态/Task 身份/Active 检查没有发现未防护的 P0-P2 路径；唯一 `Trace -> Resolver -> Damage GE` 路径、Guard/Parry、Poise、Reaction/Feedback 和 ASC 所有权没有变化。
- **UnPossess 定性**：当前只显式取消 `Ability.Attack.Light` 与 `Ability.Skill.Melee`。审查未发现 Charged/Sprint 因未列入取消列表而留下 Warp、快照、野指针或 active-state 泄漏，因此保持为设计差异，不在 H5 扩充。
- **P3 与债务**：A7F 3.17 exact-stop 集成断言仍受入口预清理时序限制；只确认其未恶化为 P1/P2。GA/GE/Montage/Notify/Modifier/Root Motion readback、独立手动 `PolyQuestEditor` 编译和真实跨帧 Task 时序仍是非阻塞验证债务，关闭条件是用户完成相应门禁或提供 evidence-backed no-adoption。
- **证据分类**：本阶段新增的是 Gemini 的源码静态审查报告；A7B/C/D/F 的用户 Automation/Scene01 PIE 和既有编译/静态报告属于历史收据，没有在 H5 重复运行或扩大其覆盖范围。
- **范围与工作区**：本阶段没有修改 Source、Config、Content、资产或其他项目文件；`Config/Automation/Presets/1.json` 与全部 `Content/**` WIP 继续保留。
