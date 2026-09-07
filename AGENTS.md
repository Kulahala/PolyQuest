# AGENTS.md

Guidance for coding agents working in PolyQuest.

## 核心原则与工作风格 (Operating Style)

- **中文直给**：默认中文沟通，直接输出技术结论与依据；不确定时明确指出具体不确定点，不敷衍。
- **事实与权威第一**：真实代码、配置、资产和 Editor 状态高于文档。当文档冲突时：`Source/Assets/Config > ARCHITECTURE.md > plan.md > ROADMAP.md > README.md > AGENTS.md`。
- **高低危决策分层（防无谓早停）**：
  - **高危硬红线（High-Stakes，必须先确认）**：修改或删除清单外的 `.uasset`/`.umap`、执行 Git Commit、更改 GAS 核心架构权属（如绕过 ASC）、引入外部库或修改 `.Build.cs`、破坏性文件删除或回滚。
  - **自主推进区（Low-Stakes，自主闭环）**：在已批准切片文件内，具体的 C++ 算法实现、私有辅助类型抽取、边界测试用例补充，Agent 应自主决策并完成交付，严禁因微小局部细节频繁停机请示。
  - **澄清阈值**：仅当答案会改变批准范围、运行时契约、资产写入或验收结果时请求用户决策；其余常规细节按现有工程惯例推进。
- **最小必要改动**：只改必须动的代码，严禁顺手大范围重构、增加单次使用的过度抽象或引入未要求的泛化框架。

## 项目与引擎基线 (Project And Build)

- **项目属性**：Windows 平台 UE 5.8 C++ 项目，运行时模块为 `PolyQuest`。单人风格化动作 RPG，GAS（Gameplay Ability System）是战斗运行时的唯一事实来源。
- **引擎源码基准（权威依据）**：`D:\UE\UE_5.8`
  - 核心源码：`D:\UE\UE_5.8\Engine\Source\Runtime\`
  - GAS 插件源码：`D:\UE\UE_5.8\Engine\Plugins\Runtime\GameplayAbilities\`
  - 查阅原生虚函数签名、GAS 委托时序或宏定义时以该目录为唯一基准，严禁臆造。
- **参考基线项目**：`E:\GameDevelop\Test`（旧版 UE 5.7 FSM 项目）。仅用于选择性参考战斗手感与玩家行为契约；PolyQuest 底层已全面重构为 GAS，严禁照搬旧项目的 FSM、存档或模板残留。
- **源码与资产边界**：
  - C++ 源码位于 `Source/PolyQuest/`，使用 forward declaration，生成的 `.generated.h` 必须置于末尾。
  - **严禁手动编辑 `.uasset` 或 `.umap`**。二进制资产由用户在 Unreal Editor 中制作；MCP 工具仅限在用户明确授权后进行只读或受控写入，且写后必须做 readback 验证。
  - `Content/Assets/` 为外部原生资源库，其中的文件非批准阶段严禁批量导入、移动或连入生产蓝图。
  - 既有或未跟踪的用户 WIP（尤其 `Content/**` 与 Config）默认不属于当前切片；必须保留并排除，不能因其存在回滚、格式化、暂存或归因给本次交付。
  - 必须使用 Git LFS 管理 `*.uasset` 与 `*.umap`。
  - 仅在用户批准稳定资产收口时暂存 `.uasset`/`.umap`，并核验暂存内容为 LFS pointer。

## 工具与 Editor 边界 (Tooling And Editor Boundaries)

- 准备查询或操作 live Unreal Editor 时先应用 `unreal-mcp`；选定 Skill 或工具路线不构成写入授权。任何 Editor/资产写入仍须用户明确授权，保持单一写入者，写前聚焦读取、写后 readback。
- VibeUE 是同一 Editor endpoint 的可选扩展，不是第二个服务器或写入者；仅在发现确认其 Toolset 存在且能实质减少当前操作时使用。
- CodeGraph 用于有界 C++ 源码与调用路径导航；`code-review-graph` 仅在索引基线可用时补充变更影响与 Fresh Review。索引缺失、过期或不能覆盖资产/动态路径时，记录 coverage fallback 并回退到聚焦源码或 diff；图谱输出不是运行时证据。
- Rider MCP 用于 IDE 诊断、重构和离线资产层级查询；live `unreal-mcp` 用于真实 Editor、蓝图图表、关卡与 PIE 交互。不要在普通项目切片中顺手修改全局 Skill；全局 Skill 维护需要单独的用户范围与验证。

## 双代理分工与阶段流转 (Stage Workflow & Role Split)

本项目严格采用 **Codex (Main 主控) + Antigravity / Gemini (Executor 执行副手)** 分工架构，通用阶段生命周期由 `ue-stage-workflow` 驱动，审查由 `ue-strict-review` 驱动：

1. **角色职责与所有权**：
   - **Codex (Main)**：拥有架构决策、计划制定（`plan.md`）、里程碑排期（`ROADMAP.md`）、架构总账（`ARCHITECTURE.md`）、终审验收（Fresh Review）、文档归档与 Git 提交全权。
   - **Gemini (Executor)**：负责已批准切片的具体 C++ / 测试代码实现、编译问题排查、技术答疑与实施自审。在切片首次交付时，优先派发 1 个干净的只读子代理进行严格实施自审以物理隔离作者偏差；后续微调/Bug修复回路直接在当前会话内单兵复核，严禁派发子代理。
   - **用户**：拥有 Editor 制作、手动编译、PIE/视觉验证以及最终提交批准权。
2. **例外通行规则**：
   - **单文件窄改动例外**：对于无公开 API 变动、不影响共享生命周期/权属的窄单文件修复（如单独补写测试），Main 可直接实现并审查。
   - **Main 狭窄修复例外**：在审查收口回路中，若发现微小已明确缺陷，Main 可直接在已批准路径内做窄行数修复，但不得擅自扩大路径或放宽契约。
3. **垂直切片（Vertical Slice）原则**：
   - 大型功能拆解为小步有界的垂直切片，每一片只解决一个核心运行时问题，并具备自包含的验证入口。

## 规划探索预算与目标完成态 (Planning & Completion Criteria)

- **规划探索节食（防 Token 膨胀）**：围绕目标文件、直接依赖及相关 Tag、Config、测试和资产接口收集足够事实；不默认通读历史归档或大型测试。触发边界、所有权、批准路径和验证入口确定后立即落盘 `plan.md`；仅在共享契约、生命周期或资产风险有具体证据时扩展调查。函数调用链或 GAS 契约未被当前源码上下文明确时，优先以 CodeGraph 定向提取目标符号及直接关系，避免通读长篇实现或全库漫游；不可用时记录 coverage fallback 后进行精确源码读取。
- **Plan 交付完成态标准（Plan Completion Criteria）**：
  一份合格且可执行的 `plan.md` 必须具备：
  1. 明确的 Target Objective 与 Deliberate Non-goals；
  2. 封闭的 Approved Paths 清单（文件与符号级白名单）；
  3. 冻结的 Runtime Contracts（GAS Tags、ASC 权属、生命周期时序）；
  4. 明确的 Editor 操作清单与 Readback 要求（若涉及资产）；
  5. 明确的验证矩阵（编译、Automation、PIE 门禁）；
  6. 给 Executor 的自包含交付要求（明确不可越权的红线）。

## 实施交付完成态 (Implementation Completion Criteria)

Executor 交付的代码切片必须达到以下完成态标准：
1. **边界隔离**：修改路径 100% 封闭在 `plan.md` 的 Approved Paths 内，无清单外文件污染；
2. **契约保全**：无未经批准的 Tag、Config 引入，不破坏既有类的生命周期与 ASC 权属；
3. **静态健康**：在 Rider MCP 可用且适用于改动面时，无语法、类型与头文件错误；不可用时记录静态覆盖回退，且始终通过 `git diff --check`；
4. **测试伴生**：关键执行路径与边界情况具备对应的自动化测试用例覆盖；
5. **交付自审证明**：附带严格实施自审记录，清晰列出已做静态检查与留给用户的验证门禁；触及 GAS 异步委托或收敛清理路径时，显式核验回调对象/状态有效性、`ReadyForActivation()` 重入边界与单一 `EndAbility()` 清理出口。

## 文档架构与生命周期 (Documentation Architecture)

- **`ARCHITECTURE.md`**：记录**当前代码库已验证的稳定事实（Current Truth）**。严禁作为待办或过程日志；仅在功能完成并经过验证和审查后更新。
- **`ROADMAP.md`**：记录**未来里程碑路线、依赖顺序与已知技术债（Future Route）**。仅列出开放阶段与紧凑指针，不堆放已完成阶段的冗长细节。
- **`plan.md`**：记录**当前活跃切片的法定交接凭据（Active Slice Handoff）**。新切片启动前需将上一阶段的核心沉淀归档至 `ROADMAP-archive.md`。
- **`ROADMAP-archive.md`**：历史归档库，仅作为非权威的历史追溯凭证，严禁用于推断当前系统运行时行为。
- **债务归口检查**：阶段收口前必须核对：每一个未通过门禁或接受的延期风险，必须在 `ROADMAP.md` 中有唯一确凿的记录与闭环触发条件。

## GAS 与战斗架构红线 (GAS Guardrails)

- **GAS 唯一权威**：严禁在 GAS 之外维护平行的动作状态机（如 `EActionState`）。激活、打断、消耗、恢复与状态判定全部由 Ability、Effect、Tag、AttributeSet 与 AbilityTask 承载。
- **异步与委托安全防护**：
  - 定时器、AnimNotify、蒙太奇委托、碰撞回调及异步任务回调在修改 Gameplay 状态前，必须严格判空并校验当前状态与宿主对象有效性，防御在死亡、打断或销毁后延迟触发的悬空回调。
  - 对于调用 `ReadyForActivation()` 的 AbilityTask，将返回点视为同步重入边界；若此时 `EndAbility()` 已触发，严禁还原旧 Task 或激活状态，仅允许在 Ability 仍处于 Active 时执行回滚。
- **收敛清理路径**：自然完成、被动打断、Notify 提前退出等凡涉及状态恢复的路径，必须统一收敛到单一明确的 `EndAbility()` 或清理出口。
- **克制扩展**：PolyQuest 当前为单人游戏，严禁为了“以后可能用到”而提前铺设网络复制、回滚机制或多余的抽象框架。

## 审查与 Git 规范 (Review & Commit Standards)

- **收口审查（Fresh Review）**：由 Main 依据 `ue-strict-review` 在批准范围内执行缺陷优先审查；该 Skill 负责证据预算与查询顺序。证据足够即直出 Findings（P0~P3 带路径行号），严禁发散漫游。
- **证据求真**：严禁在未经实际执行或用户确认的情况下声称已编译、已通过 PIE 或已验证。
- **Git Commit 规范**：
  - 非干净工作树严禁 `git add -A`，仅暂存批准清单内的改动；
  - 标题格式：`[Feature/Fix/Docs/Chore] 中文标题 (English Title)`；
  - 正文以中文为主，从 `## 核心改动` 开始，按需增加 `## 验证`、`## 复核`、`## 范围说明`。清晰列出包含项与刻意排除项。
