# TODO-05B：Backstab v1 收口记录

## 阶段状态与基线

- 状态：源码实施完成，用户编译、Automation、Editor/PIE 门禁已确认，Main 预算受限 Fresh Review 无 P0-P2 阻塞；本文档记录最终收口。
- 基线：`main @ 94af5a1`（TODO-05A 正面失衡处决完成后的父提交）。本记录对应的归档快照工作树不干净，包含用户-owned `Content/**` WIP、`Config/Automation/Presets/1.json` 和其他未跟踪资源；这些内容不属于本阶段。
- Outer：`ue-stage-workflow`；Primary：`ue5-cpp-gameplay`；Support：`ue5-debug-validation`。
- 执行路线：`manual/out-of-band Gemini`。Contract owner：Main/Codex；Implementation writer：Gemini 仅负责批准源码/测试路径。Main 负责合同、验证解释、Fresh Review、文档、暂存和提交。

## 实际实现

TODO-05B 新增独立的玩家 Backstab Strike。`Input.PrimaryAttack` 在非 Sprint、主手为近战武器且存在当前 Lock-On 目标时，依次尝试 Front Execution、Backstab；两者都不接受时继续既有 direct Primary。Backstab 在激活瞬间以目标水平朝向的反向判定后方几何并保存目标弱引用；命中帧只复验目标有效性、存活、非无敌、非 `State.Status.Stunned` 和距离，不因目标在前摇中转身而吞掉命中。

- 新 Ability 为 `InstancedPerActor`、`ServerOnly`，复用现有 GAS Montage/Event Task、可选 Player Motion Warping、`FMeleeHitResolver -> Damage GameplayEffect` 单一伤害路径。
- `UAnimNotify_PlayerBackstabExecutionHit` 只发送 `Event.Action.Execution.Backstab.Hit`，不直接写 Health、Poise 或状态。
- 所有配置、坐标和向量均做有限值/边界校验；无效目标、距离、Tag、Task、Montage、resolver 或 teardown 均 fail-closed。
- Motion Warp 设置失败时清理目标并回退一次性水平 Yaw 对齐；输入、Bow、Sprint、Guard/Parry、Dodge 和准备槽路由保持既有所有权。
- 每轮激活使用自增 token 与 `UPlayerBackstabExecutionContext` 隔离异步 Montage、命中和目标委托。自然结束、中断、取消、目标失效、Task 启动失败和 UnPossess 均进入幂等 `EndAbility()`，清理委托、Task、Montage、Warp、reservation 和瞬态命中标志。
- 本阶段明确不引入 `BeingExecuted`/Victim Tag、目标侧受害 Ability、AI 暂停、双人动画锁定、延迟死亡、霸体/无敌策略或通用处决基类。

## 实际变更路径

```text
Config/Tags/PolyQuestGameplayTags.ini
Source/PolyQuest/Public/AbilitySystem/Abilities/PlayerBackstabExecutionAbility.h
Source/PolyQuest/Private/AbilitySystem/Abilities/PlayerBackstabExecutionAbility.cpp
Source/PolyQuest/Public/Animation/Combat/AnimNotify_PlayerBackstabExecutionHit.h
Source/PolyQuest/Private/Animation/Combat/AnimNotify_PlayerBackstabExecutionHit.cpp
Source/PolyQuest/Private/Character/Player/PlayerCharacter.cpp
Source/PolyQuest/Public/Character/Player/PlayerCharacter.h
Source/PolyQuest/Private/Tests/BackstabExecutionAutomationTests.cpp
```

`PlayerCharacter.h` 的改动仅是 `WITH_DEV_AUTOMATION_TESTS` 下的两个输入测试 seam，用来建立真实 held-input 前置并验证 direct Primary fallback；没有新增 Shipping API 或改变生产输入合同。

## 冻结的运行时合同

- 注册并使用 `Ability.Action.Execution.Backstab` 与 `Event.Action.Execution.Backstab.Hit`；Front 优先、Backstab 次之、direct Primary 最后。
- 目标必须是当前 Lock-On 原始 `AEnemyCharacter`，与玩家处于同一 World，存活、非无敌、非 Stunned；目标处于 Stance Break/Stunned 时 Backstab 拒绝并回退普通攻击。
- 后方夹角默认 `60` 度，合法范围 `[0, 90]`；距离必须满足 `0 <= MinExecutionDistance < MaxExecutionDistance`。激活时快照后方判定，命中时不重新判定目标朝向。
- 命中事件必须匹配当前 token、Avatar/Instigator、reservation 和当前 Backstab Montage；一次合法或 resolver 失败的命中均消费事件，不允许重试或重复扣血。
- `ReadyForActivation()` 后立即检查 Ability 终态、Task UObject、Task 活跃/完成状态及 Montage 播放状态；任何启动竞态都通过统一清理结束，不能留下 `ActivationOwnedTags`。

## Review 与修复

- Main 按 `AGENTS.md` 执行了一轮 diff-first、直接调用边界、生命周期/异步/弱引用/Tag 回滚核查；未使用无关多跳图探索。
- 首轮发现的 P1 是：`ReadyForActivation()` 后 Wait/Montage Task 失效时，Ability 可能仍 active 而未收口。修复已限定在批准的 Backstab Ability 与测试路径：同步回调先返回；若 Ability 仍 active 且 Task 无效/已结束，则立即走 `EndAbility()`；`EndAbility()` 仅对有效 Task 调用 `EndTask()`，并清理 Context、委托、reservation、Warp、Montage 和瞬态标志。
- 同一修复轮还补齐了错误 Task、Motion-Warp 返回值、异步代际 token、Guard 仲裁、错误 Montage/Target、resolver 失败不可重试以及 direct Primary fallback 的回归覆盖。最终窄复核未发现 P0/P1/P2 blocker。

## 验证证据

- 用户确认 `PolyQuest.Combat.Backstab` Automation 最终运行结果为 `Success`，覆盖 CDO/Tag、几何和 finite 边界、目标失效、转身快照、Exactly-Once、Task 启动失败、代际隔离、无锁/Stunned 回退和输入优先级。
- 用户此前确认 Visual Studio `PolyQuestEditor (Development Editor)` 编译通过，并在 `/Game/Maps/Scene01` 完成合法背刺、目标转身、失衡/无锁回退等 PIE 验证；authored Backstab 资产配置由用户在 Editor 完成。
- Gemini 报告及 Main 检查：批准 C++ 文件 Rider error-level 检查为 0，`git diff --check` 通过。Main 未重复启动编译、Editor、Automation 或 PIE；上述运行时结果均按用户证据记录。
- Test Run 3 中的 resolver 失败、Poise 恢复和 Wait Task inactive 日志来自明确的负路径/Fail-Closed 用例，不代表阶段失败。

## 范围排除与提交边界

- 不纳入任何 `Content/**`、Blueprint、Montage、AnimBP、地图、产品 `.uasset`、`Config/Automation/Presets/1.json`、Build.cs、`.uproject` 或其他用户 WIP。
- 不修改 Enemy/AI、`FMeleeHitResolver`、`FMeleeMotionWarping`、WeaponEquipmentComponent、现有 Front Execution 的生产逻辑或第二条伤害路径。
- 阶段提交只包含“实际变更路径”中的 8 个 Source/Config/test 路径，以及本阶段收口所需的 `plan.md`、`ROADMAP.md`、`ROADMAP-archive.md`、`ARCHITECTURE.md`。

## 后续阶段与债务

- TODO-05B 没有未关闭的阶段 blocker，也没有新增接受的验证债务。
- `TODO-05A1：Execution Lock-in And Recovery` 仅在后续 PIE/产品验收证明目标逃逸、致死后摇截断或确有双人表现需求时启动；届时由目标侧受害 Ability、临时状态、死亡延迟和 Recovery 统一解决，不回填本阶段。
- `TODO-07B8`（Enemy Reaction RateWindow）和 `TODO-07B9`（Execution Impact Feedback）仍是独立条件阶段；是否抽取窄共享执行生命周期，须待 05A/05B 的真实重复度和产品需求再次证明。
- 05B 完成后，当前开放玩家战斗阶段指向 `TODO-03C：Ranged Enemy v1`；条件性的 `TODO-03I3` 仍不构成硬依赖。
