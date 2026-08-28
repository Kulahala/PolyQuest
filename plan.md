# DOC-MAINT-01: Documentation Contract And History Archive v1（阶段计划与收口记录）

> 本阶段只整理项目文档职责、历史归档和未来路线表达，不实现玩法，不修改任何 Source、Config 或 Content 资产。历史 TODO-02B3 计划与收口证据已先保存到 ROADMAP-archive.md。

## Plan State

- **状态**：文档重组与 Main 单轮 defect-first fresh review 已完成，用户已批准提交。
- **仓库/基线**：E:\GameDevelop\PolyQuest，基线提交 f474515（main）。
- **Outer**：ue-stage-workflow。
- **Primary**：ue5-architecture（文档契约与模块边界整理）。
- **Support**：ue5-debug-validation（证据分类与收口检查）。
- **Route**：Main 直接执行；Implementation executors: 0。
- **用户门禁**：本阶段没有编译、Editor readback、Automation、PIE、打包或资产写入门禁；这些属于未来玩法阶段。
- **提交边界**：仅 AGENTS.md、ARCHITECTURE.md、ROADMAP.md、ROADMAP-archive.md、plan.md、README.md（README.md 只增加归档索引）。既有 Config/**、Content/**、Source/** WIP 全部保留。

## Objective

1. 让 ARCHITECTURE.md 只描述当前稳定的运行时所有权、数据流和产品边界。
2. 让 ROADMAP.md 只描述当前基线、玩家战斗序列、未完成里程碑、采用条件和验证债务。
3. 将维护前的详细阶段记录保存到 ROADMAP-archive.md，保留可追溯性但不让历史内容干扰当前决策。
4. 在文档中冻结最小的 05 族设计，避免把 Front Critical、Stagger Execution、Stagger Backstab 重新扩张成通用处决框架。

## Frozen Product Decision: 05 Punish Family

- **TODO-05A: Stagger Front Execution v1**：只有目标当前拥有既有 State.Status.Stunned、且该状态来自真实 Poise 降至零并进入现有 Stance Break 路径时，才允许正面处决。
- **Parry 不直接触发处决**：Parry 成功只沿现有 Defense/GameplayEffect 路径给攻击方施加作者化 Poise 伤害；只有该效果实际把 Poise 清到零并让 Enemy 进入 State.Status.Stunned，才获得 05A 资格。若 Poise 没有归零，不得处决。
- **TODO-05B: Backstab v1**：保留为与 05A 分离的背后动作，目标须存活且当前不处于 State.Status.Stunned，并在激活快照中满足后方几何条件。v1 不引入 stealth、alert、perception 或新的 AI 状态。
- **TODO-05C**：从当前路线退役。其正面处决内容并入 05A；Stagger Backstab 不进入 v1。未来若确有后方失衡处决需求，另立阶段和验证门。
- **技术依赖**：TODO-03A7 → TODO-05A → TODO-05B；TODO-07B4 是放在 TODO-03C 前的玩家战斗表现门，但不是 Motion Warping 或 05 族的技术前置。

## Approved Documentation Scope

### ARCHITECTURE.md

- 保留模块章节及其当前稳定契约。
- 删除顶部按 TODO 时间线堆叠的历史流水账，保留一段当前技术栈/所有权摘要。
- 将原“Not Yet Established”改为不授权实现的产品边界说明；未来顺序只引用 ROADMAP.md。

### ROADMAP.md

- 保留精简基线（必要时指出最新阶段/提交）、当前玩家战斗序列、开放里程碑和当前 Debt Register；稳定所有权原则由 ARCHITECTURE.md 负责，路线图只保留必要的未来约束。
- 保留 TODO-03A4B 作为条件项，但不把它列入当前核心序列。
- 用 05A/05B 两阶段替代旧 05A/05B/05C 结构，并明确 05A 的 Stunned/Poise 前提。
- 已完成阶段的详细记录迁移到 ROADMAP-archive.md。

### ROADMAP-archive.md

- 作为 append-only 历史证据保存维护前的 ROADMAP.md、Architecture 顶部历史状态段和 TODO-02B3 plan。
- 它不是行为、配置或路线图的权威来源；源码、Config、实际资产和当前活动文档优先。
- 后续追加归档时必须注明阶段、提交、验证类型和是否仍为当前契约。

### AGENTS.md / README.md / plan.md

- AGENTS.md 增加文档职责与归档边界，防止下一位执行者重新把历史流水账写回 Architecture。
- README.md 保持公开状态摘要，只补充 `ROADMAP-archive.md` 的项目布局与文档索引，不做大规模历史重写。
- 本文件记录 DOC-MAINT-01 的范围、冻结决策、证据和提交边界；下一个玩法阶段获批准后才替换本文件。

## Non-Goals

- 不修改任何 C++、Gameplay Tag、Build.cs、uproject、Config、Blueprint、Montage、AnimBP、UMG、DataAsset、地图、Niagara、音频或其他 Content。
- 不删除历史证据，不清理用户 WIP，不重排与 05 无关的玩法契约，不实现处决/背刺/镜头晃动。
- 不把当前“玩家战斗体验闭环”表述成完整玩家循环；存档、消耗品、奖励和死亡重载仍是后续阶段。

## Validation Matrix

- **静态文档检查**：章节职责、TODO 顺序、05C 旧引用、归档指针、证据措辞和 WIP 边界。
- **Git 检查**：git diff --check；确认只出现批准文档路径和新增归档文件。
- **Main fresh review**：只做一轮 defect-first review；不执行第二轮 adversarial review，也不派遣独立 Reviewer。
- **未执行**：Development Editor 编译、Editor readback、Automation、PIE/视觉、打包。

## Main Fresh Review

- **审查范围**：当前六份维护文档、活动路线与源码/Build.cs/Config 的关键静态边界；CodeGraph 仅作源码调用链证据，code-review-graph 索引落后于当前 HEAD，仅作补充影响证据。
- **结论**：未发现 P0/P1/P2 源码或架构阻塞；首轮已修正四项文档漂移（归档快照来源、Public/Private 依赖表述、README 归档索引及章节职责），本次又完成路线图的重复段落收敛。
- **路线决策**：TODO-05C 退役；05A 仅接受真实 Poise-to-zero 后的现有 `State.Status.Stunned`，Parry 本身不授予处决资格。`TODO-07B4: Player Attacker-Impact Camera Shake And Reaction-Tier Mapping v1` 的最小默认把由已接受 Player 接触造成的首次致命 Health 命中纳入同一档位映射一次，发生在死亡清理前；重复 Dead 回调、直接 Attribute 写入和非 Player 来源静默。实现阶段仍须以实际资产和用户 PIE 手感确认。
- **证据边界**：本审查与文档检查是静态证据，不等同于编译、Editor readback、Automation 或 PIE；本阶段没有启动这些门禁。

## Gemini Review Decision

- **采纳**“删除独立 `Done Milestones`”：当前最新阶段和提交已由 `Current State` 唯一指出，详细收口由 `plan.md` 与 `ROADMAP-archive.md` 保存；独立章节只会重复同一事实。
- **采纳**压缩 `Current State`：移除已由 ARCHITECTURE.md 负责的系统清单，只保留基线、最近阶段、产品入口、下一焦点和 WIP/证据边界。
- **采纳**合并原则段：删除 `Accepted Technical Direction`，用单一 `Route Constraints` 保留未来路线真正需要的 GAS/StateTree/Projectile/Motion Warping/产品边界约束。
- **采纳**移除 ROADMAP.md 底部的文档职责镜像：职责与归档纪律由 AGENTS.md 统一定义，ROADMAP.md 只在顶部保留指针。
- **结果**：ROADMAP.md 继续是路线图而不是架构说明书，只保留唯一的当前基线指针、开放里程碑、依赖和债务；完成阶段不再单独复制一遍。

## Closeout Record

- **归档**：维护前文档快照已写入 ROADMAP-archive.md。
- **文档结果**：ARCHITECTURE.md 顶部历史段已压缩并区分 Public/Private 依赖；ROADMAP.md 已进一步收敛为当前路线视图并删除重复的 `Done Milestones`；AGENTS.md 已记录新的去重/归档规则；README.md 已补充归档索引。
- **路线结果**：当前玩家战斗顺序为 TODO-03A3E → TODO-07B4 → TODO-03A7 → TODO-05A → TODO-05B → TODO-03C；TODO-05A → TODO-05B 的技术依赖保留，TODO-05C 退役。
- **证据边界**：本阶段只有源码/文档静态读取和 Git 差异检查；没有编译、Editor、Automation、PIE 或资产读回证据。
- **Review**：Main 已完成单轮 defect-first fresh review，未发现 P0/P1/P2 blocker；不得把归档或静态检查写成运行时证明。
- **Commit**：用户已批准；仅提交本文件列出的文档路径，Config/**、Content/**、Source/** WIP 不纳入。
