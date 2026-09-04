# REC-05A1-03-RET：Legacy Execution Notify / Tag Retirement

## 阶段状态与基线

- 状态：阶段已收口（带非阻塞验证债务）；实现与适用用户门禁已完成，取消/销毁/UnPossess/Teardown PIE 仍未覆盖，不宣称这些路径已验证。本阶段目标是退役 Legacy Execution Notify、方向 Hit Tag、兼容 listener/task 和旧正向测试路径。
- 日期：2026-09-04。
- 仓库：E:\GameDevelop\PolyQuest。
- 当前基线：main @ 55dc5304c88e9506bba84917d9863175abab0856，相对 origin/main ahead 3。
- 工作树：存在用户-owned WIP；以下状态数量仅为计划制定时的快照，不构成批准路径或提交集合。实际范围以冻结的 migration manifest 为准。
- 当前 plan.md 的 MIG 收口证据已由 55dc530 归档；RET 计划替换本文件前，保留 MIG 历史，不改写其证据。
- AM_LightSword_PlayerExecution.uasset、相关 GA、Sequence 及其他 Content/** 仍是用户-owned WIP，本阶段不修改、不保存、不暂存。

## 目标与成功标准

删除两个方向专用 Native Execution Notify、两个方向 Hit Tag，以及 Front/Backstab Ability 中仅为兼容旧路径保留的 listener/task 和测试 seam；运行时只保留统一 UAnimNotify_PlayerExecutionHit 与 exact Event.Action.Execution.Hit。

成功标准：

1. 55dc530 的 MIG Editor receipt 被接受为 RET Entry Proof；仅在发现资产或 manifest 漂移时补做聚焦 readback，不要求重复截图。
2. 两个旧 Notify 类及其所有生产引用删除；两个旧 Tag 从项目 Config 删除并在运行时请求时无效。
3. Front/Backstab 只监听 canonical Hit，且不改变 LockContext、Token、动画身份、VictimStart/Hit/Release、Resolver、exactly-once 或任何清理顺序。
4. 六个受影响 Automation 文件切换到 canonical 输入，删除 Legacy 正向兼容测试，并保留旧 Tag 的明确负向注册断言。
5. 用户手动编译、七项处决 Automation、正常 Front/Backstab Scene01 PIE、最终 Editor readback 和 Main ue-strict-review 全部通过；强制中断/销毁/UnPossess/Teardown 因当前环境无法可重复构造，经用户明确接受为非阻塞验证债务。
6. 本阶段不产生 authored .uasset/.umap 提交；提交边界只包括批准的 Source、Config、Test 和文档路径。

## 工具路线与责任

- Outer：ue-stage-workflow。
- Primary：ue5-cpp-gameplay。
- Support：ue5-debug-validation。
- Route reason：本阶段是 Native GAS/Gameplay Tag/Automation 的清理，不涉及 Blueprint graph 或资产写入。
- Execution route：manual/out-of-band Gemini（Source/Config/Test）；用户-owned Unreal Editor 最终 readback。
- unreal-mcp 默认不使用；只有用户明确授权 live Editor 操作时才启用，并遵守单一写入者和写后读回规则。
- Implementation executor：Gemini。
- Contract owner：Main/Codex。
- Main 负责范围冻结、Entry Proof 解释、计划和文档、静态门禁、Review、staging 与 commit；用户负责 Editor readback、手动编译、Automation、PIE 和最终 commit approval。
- Gemini 不得修改 plan.md、ROADMAP.md、ROADMAP-archive.md、ARCHITECTURE.md、README.md、AGENTS.md，不得暂存或提交。

## 冻结运行时合同

- 唯一玩家处决命中入口为 UAnimNotify_PlayerExecutionHit，发送 Event.Action.Execution.Hit。
- UPlayerFrontExecutionAbility 与 UPlayerBackstabExecutionAbility 仍是独立 InstancedPerActor、ServerOnly Ability，身份 Tag、输入优先级和目标快照不变。
- ExecutionLockContext、Activation Token、Avatar/Actor/ASC/动画身份校验、VictimStart -> Hit -> Release 时序、FMeleeHitResolver -> Damage GameplayEffect 唯一路径和 exactly-once 语义不变。
- 所有自然完成、取消、死亡、销毁、Montage 中断、UnPossess、Task 启动失败和 resolver 失败仍汇聚到原有幂等 EndAbility() 清理。
- 不新增父级/模糊 Tag 匹配、兼容层、第二条伤害路径、共享处决基类、输入或网络合同。

## 批准范围

### Entry Proof 与资产边界

- MIG receipt 记录的 /Game/BP/Montages/LightSword/AM_LightSword_PlayerExecution 已是 Player Execution Hit，Front 与 Backstab 共用该 Montage 是当前预期设计；方向/武器专属 Montage 属于 TODO-05A1-D2B。
- Main 在执行前只读确认当前 HEAD、manifest 和目标 WIP 没有 receipt 之后的未记录漂移。若确认无漂移，直接进入源码 retirement；若有漂移，再执行一次聚焦 Editor readback。
- 需要补检时的 Native 路径：Content Browser -> Settings/View Options -> Show C++ Classes -> PolyQuest C++ Classes/PolyQuest/Animation/Combat/，右键 AnimNotify_PlayerFrontExecutionHit 与 AnimNotify_PlayerBackstabExecutionHit -> Reference Viewer；允许 Engine/Module 反射边，但不得有项目 authored .uasset/.umap 连线。
- 需要补检时的 Tag 路径：Project Settings -> GameplayTags 搜索 Execution.Front.Hit 与 Execution.Backstab.Hit，检查关联资产列表；面板无关联列表时记录限制并使用精确静态扫描辅助。
- Main 冻结 migration manifest（RET 的 retirement manifest），至少包含包路径、Runtime owner、Notify/Tag 前后状态、Reference/Tag readback、tracked/imported 状态、依赖闭包和 outcome。

### 明确不在范围内

- AM_LightSword_PlayerExecution.uasset、任何 GA/GE/Montage/Sequence/AnimBP/Blueprint/地图和其他 Content/**。
- PolyQuest.Build.cs、输入路由、Ability 身份 Tag、ExecutionLockContext、EnemyVictimExecutionAbility、FMeleeHitResolver、Motion Warping、Root Motion、Release/VictimStart 合同。
- D2A Snap、D2B 武器/方向 Montage 选择、E 反馈、Launch 行为或其他架构重构。
- README.md、AGENTS.md 和无关 Config WIP。

## 有序执行切片

### Slice 1：Entry Proof 与范围冻结

1. Main 复核 55dc530 的 MIG receipt、当前 git status 和 manifest。
2. 只有发现 receipt 后资产/归属/引用发生变化时，才要求用户补做一次聚焦 Reference Viewer/GameplayTags readback。
3. 任何旧 Notify authored 引用、未知复制品、imported/read-only 依赖或未解析 GA 归属都会停止本阶段，不得在 RET 内编辑资产。

### Slice 2：Native listener retirement

- 删除两个旧 Notify 的四个 .h/.cpp 文件。
- 在 Front/Backstab Ability 的 .h/.cpp 中移除 LegacyHitEventTag/LegacyHitTag、WaitLegacyHitEventTask 的创建/绑定/激活/检查/清理，以及对应 WITH_DEV_AUTOMATION_TESTS seam。
- HandleHitEventReceived() 只接受 canonical exact Tag；保留每个 ReadyForActivation() 后的终态、Context、Task 有效性检查。
- 不改其他状态、委托、目标监听、Montage 回调、锁定或伤害逻辑。

### Slice 3：Tag 与 Automation closure

- 从 Config/Tags/PolyQuestGameplayTags.ini 只删除 Event.Action.Execution.Front.Hit 和 Event.Action.Execution.Backstab.Hit，保留 Event.Action.Execution.Hit。
- 更新 ExecutionHitNotifyAutomationTests、FrontExecutionAutomationTests、BackstabExecutionAutomationTests、ExecutionLethalRecoveryAutomationTests、ExecutionReleaseOutcomesAutomationTests、ExecutionVictimPresentationAutomationTests。
- 删除旧 Notify CDO/旧 Tag 正向断言和 Legacy standalone/cross-direction 正向路径；生产事件全部改用 canonical Tag；连续 canonical Hit 保留 exactly-once 断言。
- 保留两个旧 Tag 的 RequestGameplayTag(..., false) IsValid()==false 负向断言；这类测试字面量和历史 archive 是允许的例外，不能形成生产消费路径。

### Slice 4：用户门禁、Review 与关闭

- Main 先完成静态检查，再交给用户手动编译和运行时验证。
- 编译后用户重新打开 canonical Montage，确认无 Missing Class/Missing Tag 警告；删除后不再要求旧类 Reference Viewer。
- 收齐证据后 Main 执行一轮 ue-strict-review，再同步文档和准备提交。

## 具体文件清单

删除：

- E:\GameDevelop\PolyQuest\Source\PolyQuest\Public\Animation\Combat\AnimNotify_PlayerFrontExecutionHit.h
- E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\Animation\Combat\AnimNotify_PlayerFrontExecutionHit.cpp
- E:\GameDevelop\PolyQuest\Source\PolyQuest\Public\Animation\Combat\AnimNotify_PlayerBackstabExecutionHit.h
- E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\Animation\Combat\AnimNotify_PlayerBackstabExecutionHit.cpp

修改：

- E:\GameDevelop\PolyQuest\Source\PolyQuest\Public\AbilitySystem\Abilities\PlayerFrontExecutionAbility.h
- E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\AbilitySystem\Abilities\PlayerFrontExecutionAbility.cpp
- E:\GameDevelop\PolyQuest\Source\PolyQuest\Public\AbilitySystem\Abilities\PlayerBackstabExecutionAbility.h
- E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\AbilitySystem\Abilities\PlayerBackstabExecutionAbility.cpp
- E:\GameDevelop\PolyQuest\Config\Tags\PolyQuestGameplayTags.ini
- E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\Tests\ExecutionHitNotifyAutomationTests.cpp
- E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\Tests\FrontExecutionAutomationTests.cpp
- E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\Tests\BackstabExecutionAutomationTests.cpp
- E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\Tests\ExecutionLethalRecoveryAutomationTests.cpp
- E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\Tests\ExecutionReleaseOutcomesAutomationTests.cpp
- E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\Tests\ExecutionVictimPresentationAutomationTests.cpp

## 验证矩阵

### Main 静态门禁

- 精确扫描旧 Notify 类名、旧 Tag、Legacy task/listener 和旧 include；生产 Source/Config 不得有正向引用。
- git diff --check。
- 对批准的 C++ 文件运行 Rider lint_files 或 get_file_problems（如可用）。
- 检查 canonical Tag 仍注册，配置项基线 113 项删除两项后为 111 项；不修复无关文档漂移。

### 用户门禁

- 手动编译 PolyQuestEditor (Development Editor)。
- 运行：PolyQuest.Combat.ExecutionHitNotify、PolyQuest.Combat.FrontExecution、PolyQuest.Combat.Backstab、PolyQuest.Combat.ExecutionLockIn、PolyQuest.Combat.ExecutionLethalRecovery、PolyQuest.Combat.ExecutionReleaseOutcomes、PolyQuest.Combat.ExecutionVictimPresentation。
- Scene01 PIE 验证 Front/Backstab canonical Hit、扣血、VictimStart -> Hit -> Release、致死/非致死结果、锁定/无敌/状态恢复、取消/销毁/UnPossess 清理，以及无崩溃/Ensure。
- 重新打开 /Game/BP/Montages/LightSword/AM_LightSword_PlayerExecution，确认 canonical Notify 正常加载且无 Missing Class/Missing Tag 警告；不保存或提交该资产。

## 用户验证回执（2026-09-04）

- **Editor readback：Pass（用户确认）**：`AM_LightSword_PlayerExecution` 正常打开，使用 `Player Execution Hit`，无 Missing Class / Tag 警告。
- **编译：Pass（用户确认）**：Visual Studio 编译通过，编辑器正常运行。
- **Automation：Pass（用户确认）**：`ExecutionHitNotify`、`FrontExecution`、`Backstab`、`ExecutionLockIn`、`ExecutionLethalRecovery`、`ExecutionReleaseOutcomes`、`ExecutionVictimPresentation` 共 7/7 全部 `Success`。
- **Scene01 PIE：Pass（用户确认）**：正面与背刺处决均完成正常触发、扣血、`VictimStart -> Hit -> Release` 和状态恢复；背刺位移对齐正常。
- **未覆盖门禁**：处决中断、受害者销毁、UnPossess、Teardown；当前 PIE 环境没有强制销毁/中断调试入口，按规则记录为未覆盖，不推断为通过。
- **Main review/static：已完成**：Fresh Review 未发现 P0/P1/P2；已修复批准路径内的 Legacy 语义残留注释，限定 `git diff --check` 通过。本轮 Main 未重新编译、运行 Automation 或写入 Editor。
- **当前判定**：RET 的实现、主要验证和静态复核已完成；用户已明确接受未覆盖的生命周期路径为非阻塞验证债务，阶段可以归档并提交，但不得把这些路径写成已通过。
- **验证债务**：`Debt-REC-05A1-03-RET-Teardown`。关闭条件是用户提供可追溯的取消/中断与销毁或 UnPossess/Teardown PIE receipt，或经 Main 明确接受的 focused fixture / evidence-backed no-adoption；该债务不阻塞本阶段提交。

## 文档收口与提交边界

- Main 已更新 ARCHITECTURE.md：删除 Legacy 接受路径，执行 Tag 列表只保留 canonical Hit，按实际配置计数同步为 111。
- Main 已更新 ROADMAP.md：RET 标记为已关闭，下一执行指针为 TODO-05A1-D2A；`Debt-REC-05A1-03-RET-Teardown` 作为非阻塞项保留。
- Main 将向 ROADMAP-archive.md 追加 RET closeout，记录真实 parent HEAD 与 working-tree not clean；不改写 MIG 历史。
- 当前 plan.md 替换前须保留 MIG closeout 的历史追溯；本文件作为 RET 当前/最近阶段记录保留至下一阶段正式接受。
- 只有以下类别可进入 RET commit：上述 Source 删除/修改、Config/Tags/PolyQuestGameplayTags.ini、六个 Automation 文件以及 Main 批准的文档更新。
- 排除 AGENTS.md、Config/DefaultEngine.ini、Automation preset、全部 Content/**、所有用户-owned .uasset/.umap 和其他 WIP。
- 建议提交标题：[Chore] 退役旧处决通知与标签 (Retire Legacy Execution Notify And Tags)。
- 本阶段 commit approval 已由用户在本轮明确授权；此前 MIG 的批准不作为 RET 范围依据，staging 仍严格按本清单执行。

## 风险与停止条件

- Entry Proof 后若目标 Montage、GA 归属或 authored 引用发生漂移，停止并补做聚焦 readback；不凭旧截图继续删除。
- 发现任一 imported/read-only 资产仍引用旧 Notify，保持其兼容路径并返回 Main 重新定范围。
- 发现未列出的 Source、Tag、Config、资产、生命周期或公共 API 需求，立即停止，不绕过 manifest。
- 任一静态、编译、Automation 或 PIE 根因最多允许一次有证据修复和一次定向复跑；同根因再次失败则保留首个失败证据并停止。
- 静态检查、Reference Viewer、Tag 面板或 Review 结果不能替代编译、Automation 或 PIE 证据。

## Gemini Handoff Completion Requirements

Gemini 回交必须列出：

- changed/deleted paths 和 manifest 对应条目；
- 删除的旧 Notify、旧 Tag、Legacy task/listener/seam；
- canonical Tag、LockContext、Token、时序、Resolver 和清理合同未变的证据；
- git diff --check、Rider 检查及严格实施自审结果；
- 未运行的编译、Automation、Editor readback、PIE 门禁；
- 未暂存/未提交状态、任何 imported/read-only 或漂移风险；
- 明确没有修改 Content/**、资产、地图、Blueprint 或文档。

## Gemini Handoff Prompt

```
你是 PolyQuest 的 REC-05A1-03-RET Implementation Executor。

仓库：E:\GameDevelop\PolyQuest
基线：main @ 55dc5304c88e9506bba84917d9863175abab0856

先读取当前 plan.md 与 AGENTS.md。55dc530 已归档 MIG Editor receipt：
AM_LightSword_PlayerExecution 已是 Player Execution Hit，两个旧 Notify 无
项目 authored 连线，旧 Tag 无资产引用。不要要求用户重复截图；Main 会先确认
receipt 后没有目标资产/manifest 漂移。发现漂移就停止并回报。

路线：Primary=ue5-cpp-gameplay；Support=ue5-debug-validation；
Execution=manual/out-of-band Gemini；Contract owner=Main/Codex。

只允许修改 plan 中列明的四个旧 Notify 文件、两个 Player Execution Ability
的 .h/.cpp、PolyQuestGameplayTags.ini 和六个 Execution Automation 测试文件。
删除旧 Notify；删除旧方向 Hit Tag；删除 LegacyHitEventTag、
WaitLegacyHitEventTask、绑定/激活/清理和测试 seam；所有生产测试事件改用
Event.Action.Execution.Hit。保持 LockContext、Activation Token、动画身份、
VictimStart/Hit/Release、FMeleeHitResolver、exactly-once 和所有清理路径不变。

禁止修改任何 Content/**、uasset/umap、Blueprint、AnimBP、地图、Build.cs、
AGENTS.md、plan.md、ROADMAP.md、ROADMAP-archive.md、ARCHITECTURE.md 或 README.md；
禁止调用 UBT/Build.bat、提交、暂存或未经授权调用 live Editor/unreal-mcp。
不增加父 Tag 匹配、兼容层、第二条伤害路径或新公共 API。

完成前运行 git diff --check 和可用的 Rider 静态检查，读取最终 diff，并完成
严格实施自审；首次交付优先使用一个全新只读审查上下文。每个根因最多一次
证据修复和一次定向复跑。

回交列出 changed paths、删除内容、canonical 合同证据、静态结果、未运行的
用户门禁、自审 findings、未暂存/未提交状态和剩余风险。
```

## 下一步

- 本文件保留为 RET 最近阶段记录，包含已完成实现、用户验证回执和已接受的非阻塞债务。
- Main 追加 `ROADMAP-archive.md` closeout 后，按批准路径准备并提交 RET；不把未覆盖生命周期路径扩写为通过。
- 后续可另立 test-only fixture 阶段关闭 `Debt-REC-05A1-03-RET-Teardown`，不改变本次退休提交边界。
