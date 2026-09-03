# REC-05A1-03-MIG：Authored Execution Hit Notify Migration / Legacy Retirement Gate

## 阶段状态与基线

- 状态：阶段已完成并通过收口门禁；结果为 No-Op / already canonical。Legacy Notify、Legacy Tag 与兼容 listener 暂不删除，后续另立 Retirement 阶段。
- 日期：2026-09-04。
- 仓库：E:\GameDevelop\PolyQuest。
- 当前基线：main @ 95dda582dfbeea27d3f6e5f800ab2ced5fdd2a0f。
- 工作树：存在用户-owned WIP；以下状态数量仅为计划制定时的快照，不构成批准路径或提交集合。实际范围以冻结的 migration manifest 为准。
- 现有 plan.md 是 REC-05A1-03 Native Contract 收口记录，基线早于当前 HEAD；上一阶段详细收口已在 ROADMAP-archive.md 归档。
- ROADMAP.md 在计划制定时将本阶段列为下一执行切片；本次 No-Op/readback 收口已完成，下一执行切片为 `TODO-05A1-D2A`，Legacy 删除仍需另立 Retirement 阶段。
- 计划制定时观察到的磁盘候选 AM_LightSword_PlayerExecution、GA_PlayerFrontExecution、GA_PlayerBackstabExecution 均为未跟踪 WIP；其中 Montage 的静态字符串显示统一 Notify，但这不是当前 Editor 或 Reference Viewer 证据，实际状态以冻结 manifest/readback 为准。

## 目标与成功标准

以 No-Op 验证为主路径，确认所有实际由 Front/Backstab 执行能力使用的 authored Execution Montage 是否已经采用 UAnimNotify_PlayerExecutionHit。

成功标准：

1. 在 Editor 中冻结完整 migration manifest，列出所有运行时可达 Front/Backstab Execution Montage 及旧 Notify 引用。
2. 已经是统一 Notify 的资产不编辑、不重新保存；存在旧 Notify 的资产才做一对一替换。
3. 每个替换资产保持原 Notify Track、帧/秒位置、Section、Slot、邻接事件和 Montage 结构。
4. Reference Viewer/readback 证明项目 authored 资产不再引用两个旧 Native Notify 类，Tag readback 不再显示旧方向 Hit 事件的 authored 使用。
5. 用户编译、指定 Automation、Scene01 PIE 和 Main Fresh Review 全部通过。
6. Legacy listener、旧 Notify 类、旧 Tag 和旧测试引用保留到后续独立 Retirement 阶段。

## 工具路线与责任

- Outer：ue-stage-workflow。
- Primary：ue5-blueprint-workflow。
- Support：unreal-mcp（仅在用户明确授权 live Editor 操作时使用）。
- Route reason：本阶段是 authored Content/AnimMontage 的 Editor 流程和引用验证，不修改 Blueprint graph、Native C++、Gameplay Tag 或运行时逻辑。
- Execution route：用户-owned Unreal Editor authoring。
- Implementation executors：Gemini（manual/out-of-band；仅按下方 handoff 执行）。
- Contract owner：Main/Codex。
- Implementation writer：Gemini，仅能修改 migration manifest 中明确批准的 authored Montage package。
- Main 负责计划、范围、manifest 冻结、证据解释、Review、文档和提交门禁；用户负责 Editor 授权、手动编译、Automation、PIE/视觉验证和最终提交批准。
- Gemini 不得修改 plan.md、ROADMAP.md、ROADMAP-archive.md、ARCHITECTURE.md、README.md，不得暂存或提交，不得使用文件系统直接编辑 .uasset/.umap。

## 冻结运行时合同

- 统一入口固定为 UAnimNotify_PlayerExecutionHit，发送 Event.Action.Execution.Hit。
- 旧映射保持不变：
  - UAnimNotify_PlayerFrontExecutionHit -> Event.Action.Execution.Front.Hit
  - UAnimNotify_PlayerBackstabExecutionHit -> Event.Action.Execution.Backstab.Hit
  - UAnimNotify_PlayerExecutionHit -> Event.Action.Execution.Hit
- Front/Backstab Ability 的 ExecutionMontage、精确 listener、动画身份校验、ExecutionLockContext、FMeleeHitResolver、Release、VictimStart 和 exactly-once 逻辑不改。
- 不修改 Release、VictimStart、RateWindow、MotionWarping、Root Motion、Section、Slot、Blend 或其他 Notify 的时序。
- authored 零引用只统计项目 .uasset/.umap 和项目-owned package；Native C++、Automation 和兼容 listener 中保留的旧类名/旧 Tag 不阻塞本门。

## 批准范围

### 初始候选与只读引用根

- 初始候选 Montage：Content/BP/Montages/LightSword/AM_LightSword_PlayerExecution.uasset。
- 初始只读引用根：
  - Content/_Abilities/Weapon/LightSword/Execution/GA_PlayerFrontExecution.uasset
  - Content/_Abilities/Weapon/LightSword/Execution/GA_PlayerBackstabExecution.uasset
  - Content/BP/Characters/Player/BP_Player.uasset
- 上述文件当前是未跟踪 WIP；它们只是 inventory seed，不是预批准提交集合。
- 实际写入集合只能来自 Slice 1 冻结的 migration manifest。未列入 manifest 的资产不得编辑、保存或暂存。
- Saved/Autosaves、生成目录、imported/read-only package、无运行时归属的复制品和无关 Montage 永不自动纳入。

### 明确不在范围内

- 所有 Native C++、Gameplay Tag 配置、Build.cs、Input、Blueprint graph、AnimBP、地图、GA/GE 默认值和敌方 Victim Montage。
- D2A Snap、D2B 武器/方向专属 Montage 选择、E 反馈、Launch、震屏、普通攻击 Motion Warping。
- 旧 Notify 类、旧 Tag、Legacy listener、旧测试和兼容代码的删除。

## 有序执行切片

### Slice 1：No-Op 优先与 manifest 冻结

1. 在 Editor 打开 /Game/BP/Montages/LightSword/AM_LightSword_PlayerExecution。
2. 先检查全部 Notify Track、Hit 点和显示名；若全部是 Player Execution Hit 且没有旧方向 Notify，标记 No-Op / already canonical，不编辑、不重新保存，直接进入 Slice 3。
3. 通过 Front/Backstab GA 的 ExecutionMontage 默认值确认实际归属。
4. 明确记录 Front 与 Backstab 共用同一轻剑 Execution Montage 是当前预期设计，不是配置错误；武器/方向专属 Montage 属于 TODO-05A1-D2B。
5. Gemini 先只读回交候选 manifest；Main/Codex 冻结并明确批准行集合后，才允许任何资产写入。
6. 由 Main/Codex 在本文件中维护 manifest 表格；Gemini 只回交每行所需的包路径、使用方、Sequence/Section/Slot、Notify 类/显示名/Tag、Track、帧号/秒数、邻接事件、tracked 状态、imported 状态和 Reference Viewer 结果。
7. 使用 Reference Viewer/Asset Registry 检查所有项目 authored package，而不是只检查文件名命中的候选。
8. 缺少 Montage、无法解析 GA 归属、存在重复命中点、未知复制品或只读引用时停止。

### Slice 2：仅在需要时做一对一替换

- 只操作 manifest 中由用户确认的资产。
- 每个旧 Hit Notify 替换为一个 UAnimNotify_PlayerExecutionHit，保持原 Track、帧/秒位置、Section、Slot 和事件顺序。
- 已经是统一 Notify 的实例不重复添加；共享 Montage 不按 Front/Backstab 方向复制 Hit 点。
- 不修改其他 Notify、Sequence、Montage 结构、Root Motion、Motion Warping 或 Ability 默认值。
- 保存前后只保存批准 package，不执行无关 Save All。

### Slice 3：Native Class 与 Tag readback

Native C++ 类的定位：

1. Content Browser -> Settings/View Options -> Show C++ Classes。
2. 导航至 PolyQuest C++ Classes/PolyQuest/Animation/Combat/。
3. 对 AnimNotify_PlayerFrontExecutionHit 和 AnimNotify_PlayerBackstabExecutionHit 右键执行 Reference Viewer。
4. Engine/Module 内部反射边可以存在，但不能有项目 authored .uasset/.umap 连线。
5. 检查 AnimNotify_PlayerExecutionHit，确认类可用且显示名为 Player Execution Hit。

Gameplay Tag 验证：

- Project Settings -> GameplayTags。
- 搜索 Execution.Front.Hit 与 Execution.Backstab.Hit。
- 若当前 UE 5.8 面板提供关联资产/引用列表，确认 authored 资产列表为空。
- 再确认 Event.Action.Execution.Hit 已注册。
- 若面板没有关联资产列表，记录工具限制，不能把“搜索到 Tag”当成零引用证明。

辅助静态证据：

- 优先使用用户已有的 Tag/Notify 扫描脚本。
- 当前仓库没有发现该脚本；无脚本时使用精确文本/二进制扫描检查 Content/**，排除 Saved/Autosaves/** 和生成目录。
- 静态扫描只用于辅助定位，不能替代 Editor/Reference Viewer/readback。

### Slice 4：回归与阶段关闭

- 迁移前若存在可运行旧资产，记录 Legacy baseline；没有可追溯旧资产时记录 no-baseline，不补造证据。
- 迁移或 No-Op 确认后运行：
  - PolyQuest.Combat.ExecutionHitNotify
  - PolyQuest.Combat.FrontExecution
  - PolyQuest.Combat.Backstab
  - PolyQuest.Combat.ExecutionLockIn
  - PolyQuest.Combat.ExecutionLethalRecovery
  - PolyQuest.Combat.ExecutionReleaseOutcomes
  - PolyQuest.Combat.ExecutionVictimPresentation
- 用户在 Scene01 PIE 验证 Front/Backstab 实际 Montage 的单次命中、Lock/Invulnerable/DeathPending/Release、致死/非致死结果、取消/销毁清理及无 Ensure/崩溃。
- 用户手动编译 PolyQuestEditor (Development Editor)。
- Main 收齐证据后执行一次独立 ue-strict-review。

## Migration Manifest 与验收记录

manifest 由 Main/Codex 维护，必须在每个资产保存前冻结，并在 readback 后补齐结果。Gemini 不得直接编辑本文件。至少包含：

| 字段 | 要求 |
| --- | --- |
| Asset package | 唯一项目包路径 |
| Runtime owner | Front、Backstab 或共享 |
| Montage source | Sequence、Section、Slot |
| Before | 旧/统一 Notify、Track、帧/秒 |
| After | 新 Notify、Track、帧/秒 |
| Neighbors | VictimStart、Release、RateWindow、MotionWarping 等邻接事件 |
| Reference | Reference Viewer 与 Tag 面板结果 |
| Package state | saved、untracked、imported/read-only、依赖闭包 |
| Outcome | replaced、already canonical、blocked、no-baseline |

## 关闭条件

- manifest 完整，所有运行时可达 Front/Backstab Montage 已分类。
- 每个旧 Notify 已替换，或明确记录为 already canonical。
- 两个旧 Native Notify 类无项目 authored asset 引用；Engine/Module 反射边不计入失败。
- authored 资产不再使用旧方向 Hit 事件；源码/Automation 兼容引用可保留。
- 所有发生替换的批准 package 已保存；本阶段 No-Op package 明确未编辑、未重新保存；redirector、未保存 package、复制品和未知引用已处理。
- 用户编译、Automation、Scene01 PIE 和 Main Review 通过。
- Legacy 源码仍保留，不能把 MIG 结果描述成 Legacy 已删除。

## 资产与提交边界

- No-Op 或只修改未跟踪 WIP 时，默认只提交门禁记录与文档，不孤立暂存 Montage。
- 若未跟踪 Montage 及其 Sequence 依赖被修改，不得只暂存 Montage；除非用户另行批准完整 authored dependency closure，否则保持本地 WIP，并在收口中注明不构成 clean-checkout authored baseline。
- 只有用户明确批准完整 authored dependency closure 后，才允许按显式路径暂存相关 .uasset/.umap；每个资产必须验证 Git LFS pointer。
- 禁止 git add -A；排除 Saved/Autosaves、孤立未跟踪依赖、imported/read-only 资产、无关 Content、Config WIP、AGENTS.md 和任何源码删除。
- Legacy 删除必须另立阶段/提交，并在删除前更新相关 Automation 与源码引用。

## 文档收口

- 本阶段由 Main 用当前 HEAD 重写 plan.md；上一 Native Contract 记录保留在 ROADMAP-archive.md。
- 计划制定和 Gemini 执行期间不修改 ROADMAP.md；阶段关闭后只同步里程碑状态、下一阶段指针和真实债务。
- MIG 收口历史追加到 ROADMAP-archive.md 时必须标明 working-tree snapshot，不得描述为 clean HEAD。
- 没有新的稳定运行时契约时不修改 ARCHITECTURE.md 或 README.md。

## 风险与停止条件

- No-Op 只跳过替换编辑，不跳过完整 manifest、Reference Viewer、Tag readback、回归和 Review。
- 当前静态扫描没有发现旧 class 字符串，不能据此宣称零引用。
- imported/read-only asset 若仍引用旧 Notify，不能擅自编辑；保持 Legacy 兼容并记录 closure trigger。
- 发现未列出的 Asset、Blueprint、Tag、Config、Source 或生命周期变更需求时，返回 Main 重新定范围。
- 任一验证根因最多允许一次有证据修复和一次定向复跑；再次失败则停止并保留首个失败证据。
- 若所有资产已 canonical 但没有可提交的完整依赖闭包，允许关闭本地 authored/readback 功能门，但必须明确“不构成 clean-checkout authored baseline”。

## Gemini Handoff Completion Requirements

Gemini 回交必须列出：

- 实际检查和修改的 package 路径，以及 manifest 条目。
- No-Op 或替换分支的判定依据。
- 每个资产的 Notify 前后类型、Tag 映射、Track、帧/秒、邻接事件和引用 readback。
- Native C++ class Reference Viewer 结果、Gameplay Tag 面板结果及静态扫描辅助结果。
- 是否执行了用户编译、Automation、Scene01 PIE；未运行门禁必须明确列出。
- 是否存在未跟踪依赖、imported/read-only 引用、redirector 或 no-baseline。
- 严格实施自审 findings、剩余风险、未暂存/未提交状态。
- 明确没有修改 manifest 外资产、Source、Config、Blueprint、地图或文档。

## 收口记录（2026-09-04；parent HEAD `95dda582dfbeea27d3f6e5f800ab2ced5fdd2a0f`；working-tree not clean）

### 实际结果与 Migration Manifest

- 用户完成 Editor readback：`/Game/BP/Montages/LightSword/AM_LightSword_PlayerExecution` 的命中 Notify 已是 `UAnimNotify_PlayerExecutionHit` / `Player Execution Hit`，未执行替换、未重新保存，分支判定为 `No-Op / already canonical`。
- Front 与 Backstab 继续共用同一轻剑 Execution Montage；这是当前预期设计，方向/武器专属 Montage 仍属于 `TODO-05A1-D2B`。
- Main/Codex 维护的最终 manifest 行：

| Asset package | Runtime owner | Before / After | Timing / neighbors | Package state | Reference / Tag readback | Outcome |
| --- | --- | --- | --- | --- | --- | --- |
| `/Game/BP/Montages/LightSword/AM_LightSword_PlayerExecution` | Front + Backstab（共享） | 已为 `Player Execution Hit`；未发生替换 | 未编辑、未改变 Track/帧/秒/Section/Slot 或邻接事件 | 未跟踪用户-owned WIP；无保存或暂存 | 两个旧 Native Notify 无项目 authored 连线；旧方向 Tag 无资产引用 | `already canonical` / `No-Op` |

- No-Op 不构成 clean-checkout authored baseline；相关未跟踪 Montage/Sequence 依赖仍按用户-owned WIP 保留，未孤立暂存。

### 用户验证收据

- **Editor readback**：`AM_LightSword_PlayerExecution` 确认为 `Player Execution Hit`；`AnimNotify_PlayerFrontExecutionHit` 与 `AnimNotify_PlayerBackstabExecutionHit` 的 Reference Viewer 仅有模块级反射边，无项目 authored 资产连线；旧 Tag 无资产引用。
- **Build**：用户确认 Visual Studio `PolyQuestEditor (Development Editor)` 编译通过，编辑器正常运行。
- **Automation**：用户确认以下 7 项全部 `Success`：`ExecutionHitNotify`、`FrontExecution`、`Backstab`、`ExecutionLockIn`、`ExecutionLethalRecovery`、`ExecutionReleaseOutcomes`、`ExecutionVictimPresentation`。
- **Scene01 PIE**：用户确认 Front/Backstab 处决的扣血、动画时序、Release、状态恢复均通过，无崩溃或 Ensure。

### Main Fresh Review

- Review route：`ue-strict-review`，Main 单轮、diff-first、缺陷优先；审查范围限定为本阶段文档收口与用户确认的 No-Op 资产证据。
- Findings：未发现当前批准范围内 P0/P1/P2 blocker；无需要修复的 Source/asset contract 回归。
- `code-review-graph`：`skipped (docs-only closeout; no Source/shared-contract diff)`；无必要的第二批 CodeGraph 扩展。
- 证据层已分开记录：用户验证属于 Editor readback、build、Automation、PIE/runtime；本次 Review 结论不替代这些门禁。

### 提交与后续边界

- 本阶段没有 authored asset、Source、Config、Blueprint、地图或测试文件变更；默认提交候选仅为 Main 收口文档，须等用户明确批准后再 staging/commit。
- 旧 Notify 源码、兼容 listener、旧 Tag 与旧测试引用保留；Legacy 删除必须另立 Retirement 阶段和独立提交，并在删除前重新确认 authored 零引用与完整验证闭环。
