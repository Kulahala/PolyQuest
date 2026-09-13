# TODO-03H8：修复 ExecutionReleaseOutcomes / VitalHUD 自动化红错

## 1. 目标、基线与执行路线

**状态（2026-09-13）：实现、用户编译/Automation/同会话重复执行与 Main Fresh Review 已通过，文档收尾完成；用户已批准有界提交。第 1–6 节保留批准计划，最新完成态以第 8 节为准。**

修正两个测试的前置与时序错误，保留有效断言，使 PolyQuest.Combat.ExecutionReleaseOutcomes 与 PolyQuest.UI.VitalHUD 两个完整套件通过。

- 工作区：E:\GameDevelop\PolyQuest；引擎：D:\UE\UE_5.8。
- 规划基线：52ce3078d72b8a139d05c048d533313591dd8f0d。既有 Content、Config 等 WIP 全部保留；不得暂存、回滚或归因给本片。
- 03H7 已归档于 ROADMAP-archive.md 的“TODO-03H7 — Main Implementation Closeout（2026-09-13）”；完整旧交接读取 git show 52ce3078d72b8a139d05c048d533313591dd8f0d:plan.md，不重复归档。其修复后收据缺口仍唯一归 Debt-03H7-PostRepairValidation。
- Outer：ue-stage-workflow；Primary：ue5-debug-validation；Support：ponytail:ponytail lite。
- Route reason：两项 Automation 的测试前置与离散 Tick 预期修复，无生产行为变更。
- Execution route：manual/out-of-band Gemini；Implementation executors：1（用户手动交接）；本轮实际派发：0。
- Main 负责计划、终审、文档与提交；Gemini 负责批准测试修复及实施自审；用户负责编译、Automation、PIE 和最终提交批准。

用户已选择最小方案：Execution 保留回调单测，不升级真实 Montage 播放集成。Ponytail lite：复用既有 setter、清理 lambda 与 Tick 测试入口，不新增公共 fixture 或生产接口。

## 2. 证据与冻结决策

### 2.1 ExecutionReleaseOutcomes

已有失败日志 Saved/Logs/PolyQuest-backup-2026.09.13-05.42.13.log 记录 Victim Montage 播放失败，随后 Section 13 的 Victim active/locked during BlendOut 断言失败。

当前源码链路：GrantAndConfigureVictimAbility 在激活前设置实例 bypass；EnemyVictimExecutionAbility::ActivateAbility 调用 ClearRateWindow(false)，重置该实例 bypass；Section 13 未像同文件移动模式用例那样在激活后重新设置测试前置。不具备真实播放条件的瞬态 Montage 因而进入播放路径，恢复态未建立就发生清理。

修复保留回调状态契约单测，不声称真实 Montage 播放、自然 BlendOut 时刻或动画队列派发已验证。不判定错误的引入版本。

### 2.2 VitalHUD

当前 Section 1.14 受伤后仅调用一次 SimulateTickForTesting(0.60f)，随即期望缓冲条为 0.4。UpdateBufferHealth 在帧入口 BufferDelayTimer > 0 时扣减时间后直接返回，因此该帧仍为 1.0 符合生产实现。

默认 BufferCatchUpDelay=0.5f、BufferCatchUpSpeed=4.0f。UE 5.8 Engine/Source/Runtime/Core/Public/Math/UnrealMathUtility.h 的 FInterpTo 实际计算为：

```cpp
Current + (Target - Current) * Clamp(DeltaTime * InterpSpeed, 0, 1)
```

单次 DeltaTime=0.25f、速度 4.0f 的系数为 1，可直接到达目标。连续指数衰减公式不适用于这个单次 Tick 调用；无需改成至少 1.60 秒。这里验证离散 Tick 行为，不证明实际帧率下的收敛耗时。

证据为真实 AGENTS、源码、引擎实现、Git 与已有日志静态核对。本轮未重新编译或运行 Automation。CodeGraph 已先行查询但未返回目标测试区段，采用聚焦源码读取作为 coverage fallback。

## 3. Approved Paths 与实施步骤

Gemini 仅可修改以下两份文件的限定区段及必要注释；同文件不等于可修改其他功能。

| 绝对路径 | 符号与区段 |
|---|---|
| E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\Tests\ExecutionReleaseOutcomesAutomationTests.cpp | FExecutionReleaseOutcomesAutomationTest::RunTest，仅 Section 13 |
| E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\Tests\VitalHudAutomationTests.cpp | FVitalHudAutomationTest::RunTest，仅 Section 1.14 的 HealthInterpWidget 受伤后缓冲条追赶片段 |

### 3.1 Execution：激活后恢复前置并保全退出路径

1. SetupFrontAndVictimExec 返回后检查 Front/Victim 指针及两者 Active。
2. 在当前 Victim 实例调用既有 SetTestBoundAnimInstance(MockAnimInstance) 与 SetTestBypassMontageActiveCheck(true)。不改 CDO 或共享 setup helper。
3. 通过 FrontAbility->GetTestExecutionContext() 获取 Context 并判空。
4. 保留 Hit → VictimStart 顺序。Hit 后检查 DamageConsumed，并用 Context->GetHitState() 断言 EExecutionSessionHitState::NonLethal。
5. VictimStart 后、手动 BlendOut 前，断言 Victim Active、IsTestNonLethalRecoveryActive() 为真、VictimLocked 存在。
6. 手动调用既有 BlendOut 测试入口，保留 Active/锁不释放的断言；Completed 后检查 Ability 结束、Recovery 关闭、锁移除。
7. Setup 后原有及新增前置失败出口，均先调用 CleanupFrontExec(FrontHandle, VictimHandle)，再 return false；正常出口保留现有清理，不重复清理。
8. 注释明确“回调状态契约单测”；不得补写未执行的真实播放或自然派发结论。

清理用于保证退出路径完整；不预判原代码已造成跨套件污染，因为本套件另有 World 析构清理。

### 3.2 VitalHUD：分开延迟与追赶阶段

保留初始化与受伤当帧主条 0.4、缓冲条 1.0 的断言。受伤后按以下顺序执行：

| 步骤 | 调用 | 断言 |
|---|---|---|
| 延迟耗尽 | SimulateTickForTesting(0.60f) | GetTestBufferDelayTimer() <= 0；缓冲条仍为 1.0 |
| 追赶介入 | SimulateTickForTesting(0.05f) | 缓冲条严格介于 0.4 与 1.0 |
| 单步收敛 | SimulateTickForTesting(0.25f) | 缓冲条按现有容差等于 0.4 |

第三步注释说明默认速度 4.0f 与 0.25f 单次 Tick 使插值系数达到 1。保留后续治疗、跟随及死亡断言，不修改生产计时逻辑、不扩大容差。

## 4. Runtime Contracts 与 Deliberate Non-goals

- 零生产代码、公开 API、类型、Tag、ASC 权属、Release 握手及生命周期变更。
- 保留非致死 Recovery 的 BlendOut 保锁、Completed 清理契约。
- 保留 HUD 延迟帧直接返回、后续帧插值，以及治疗和死亡行为。
- 不新增测试入口、bypass、公共 fixture、依赖或框架，不修改共享 helper。
- 不修改资产、Config、Build.cs、其他测试，不开展真实播放升级、全库测试瘦身或 TODO-03C 实施。
- 无 Editor 制作或资产 readback 操作，Agent 无资产写入授权。

若修正前置后暴露生产缺陷，停止并提交首个分歧证据，由 Main 重新冻结范围；不得放宽生产校验或删除有效断言让测试变绿。

## 5. 验证矩阵

| 门禁 | 执行者与通过标准 |
|---|---|
| 修复前记录 | 用户分别运行两个原始套件，保存版本、时间与首个失败；已有日志不冒充当前版本复现。若结果不同，先核对差异 |
| 静态检查 | Gemini 检查两文件限定 diff，git diff --check；适用且可用时做 Rider 诊断，否则记录覆盖缺口 |
| 编译 | 用户运行 UE 5.8 PolyQuestEditor / Development Editor，保留成功收据 |
| 专项 Automation | 用户运行两个完整套件，均 Success，所有新增前置和阶段断言通过 |
| 顺序回归 | 用户在同一会话执行 ExecutionReleaseOutcomes → VitalHUD → ExecutionReleaseOutcomes，均成功 |
| Main Fresh Review | 按 ue-strict-review 检查前置真实性、断言保全、退出清理、范围与证据，无未关闭 P0–P2 |

本片测试修复不新增 PIE 要求。按既有 Debt-03H7-PostRepairValidation，验收时另行补齐最终代码的编译、PolyQuest.Combat.PlayerBigHitReactionWindows、PolyQuest.Combat.PlayerMontageRateWindow 六组（Bow、Charged、Dodge、Light、MeleeSkill、Sprint）、PolyQuest.Combat.HitReaction、PolyQuest.Combat.PlayerLaunchReactionRootMotion，以及 Scene01 四向取消、连续受击/立即重激活、自然结束/死亡/Falling 收据。该债务独立记账，缺失时保留，不随本片自动关闭。

## 6. Executor 交付与停止条件

1. 先读真实 AGENTS.md 与本计划，核对工作区、基线和两份目标文件差异；保留 WIP。只探索目标与直接依赖，证据充分即实施，不全库漫游。
2. 在第 3 节两个区段内自主完成局部实现与静态检查；不得修改生产代码、共享 helper、文档或清单外路径。
3. 首次交付按项目规则优先安排一个干净只读子代理进行严格实施自审，禁止递归派发；后续微修单兵复核。Main 另行承担终审。
4. 交付两文件 diff 摘要、静态结果、自审记录、证据归属及未执行门禁，不把静态检查或回调单测说成真实播放/PIE 成功。
5. 同一根因最多一次有据修复和一次针对性重跑；再次失败时保留首个失败证据并停止。需要越界或修改生产契约时报告所需范围，不自行扩围。
6. 不暂存、不提交、不推送，不未经明确授权代跑编译、Automation 或 Editor，不自动实施 TODO-03C。

## 7. 文档与完成态

规划时仅授权 plan.md；用户现已批准 plan.md、ROADMAP.md、ROADMAP-archive.md 三文档收尾。03H7 核心已归档，第 1 节固定旧交接提交指针。

后续 Main 文档维护范围为 E:\GameDevelop\PolyQuest\plan.md、E:\GameDevelop\PolyQuest\ROADMAP.md、E:\GameDevelop\PolyQuest\ROADMAP-archive.md，现已获用户授权收尾；Gemini 不得修改。该测试片不改变稳定架构，无需更新 ARCHITECTURE.md。

编译、两个完整套件、顺序回归及 Main 终审通过后关闭 TODO-03H8，并归档核心结果。03H7 未补齐收据继续归原唯一债务，不声明最终版全门禁或干净可发布基线。

用户现已批准两份测试与三份文档一并提交，包含另一会话制定的 ROADMAP 03H9-A/B/C 封装排期；仅暂存批准路径，保留无关 WIP，不推送。下一入口按最新 ROADMAP 为 TODO-03H9-A。

## 8. Main Closeout（2026-09-13）

- **实现范围**：两份批准测试仅修改 ExecutionReleaseOutcomes Section 13 和 VitalHUD Section 1.14，共增加38行、删除5行。无生产代码、CDO、共享 helper、Tag、API、Config 或资产改动。
- **修复内容**：Section 13 在激活后恢复既有实例测试前置，补齐上下文/Recovery/锁断言与前置失败清理；VitalHUD 分别验证延迟耗尽、追赶介入和单步收敛，保持原治疗与死亡断言。
- **Executor 证据**：Gemini 报告 git diff --check、两文件 Rider 诊断及干净只读子代理实施自审通过。Main 不将执行者报告描述为自己执行。
- **用户验证**：用户明确确认 Development Editor 编译通过、53项 Automation 全部通过，包含两个目标套件及03H7专项/六组RateWindow/HitReaction/PlayerLaunchReactionRootMotion；并确认全套后未重启 Editor，再单独运行 ExecutionReleaseOutcomes Success，覆盖同会话重复执行。用户附件 f01a3271-595e-43c1-bc38-e9b81276b203/pasted-text.txt 的 Test Run 3 直接记录该次 Execution Success。
- **Main Fresh Review**：按 ue-strict-review 主审、ponytail-review 辅审完成只读检查，无 P0–P2 缺陷，无需提出 P3 简化建议，无修复。局部测试改动无共享契约疑点，跳过图查询；Main 未代跑编译、Automation 或 Editor/PIE。
- **证据边界**：Execution 仍为回调状态契约单测，不证明真实 Montage 播放/自然派发；VitalHUD 为离散 Tick 验证。保留既有失败日志，不追认修复前曾按计划重新独立执行两个套件，也不判定引入版本。
- **债务**：Debt-03H7-PostRepairValidation 的当前编译与 Automation 已补齐，仅余 Scene01 四向取消、连续受击/立即重激活、自然结束/死亡/Falling 的修复后 PIE 收据；唯一记录仍在 ROADMAP。H6-F14/其他H6债务不随本片关闭。
- **完成态与后续**：TODO-03H8 实现、验证、Main终审与文档收尾完成，下一入口按用户新增排期为 TODO-03H9-A → 03H9-B → 03H9-C → TODO-03C。用户已批准两份测试与三份文档的有界提交，保留全部无关 WIP，不推送。