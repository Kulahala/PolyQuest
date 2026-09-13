# TODO-03H6-TEST-SHRINK：测试 World Tick 样板去重

## 1. 目标、基线与职责

**状态（2026-09-13）：TODO-03H6-TEST-SHRINK实施、用户编译/七项Automation及Main Fresh Review已完成，用户明确授权文档收尾与提交。七份Source/Test净减71行；当前plan保留完成态，第9节记录收据及迁移前基线证据缺口。下一入口为TODO-03C，本次不启动其实现。**

将五份完全同义的单帧Tick和固定0.05秒分步推进实现集中到既有FCombatAutomationFixture，保持测试行为、实参、次数、时序、生命周期和有效断言不变。完成限定迁移、编译、七项Automation及Main Fresh Review后关闭本片，下一阶段进入TODO-03C，不自动追加下一批测试清理。

- 工作区：E:\GameDevelop\PolyQuest；引擎基准：D:\UE\UE_5.8。
- 实施基线：a19630d065bc9e342c0a5c786d1c61a2cb811879。落盘前Source和阶段文档无未提交差异；既有Content/Config/tmp WIP保留并排除。
- 上一阶段完整凭据：git show a19630d065bc9e342c0a5c786d1c61a2cb811879:plan.md 第9–10节。SHRINK完成态已归档；本次仅追加SHRINK→TEST-SHRINK交接，不改历史正文。
- Outer：ue-stage-workflow；Primary：ue5-cpp-gameplay；Support：ponytail lite。
- Route reason：Native测试支撑内的同义时间推进样板抽取，无生产契约变化。
- Execution route：manual/out-of-band Gemini；Implementation executors：1，由用户手动交接，本轮不自动派发。
- Main：计划、范围、Fresh Review、文档及提交；Gemini：批准七文件实现、静态检查和实施自审；用户：编译、Automation及最终提交批准。未经另行明确委托，Agent不代跑编译或用户验证。

## 2. Deliberate Non-goals

不新增文件、通用测试框架、可调步长参数、模板、生命周期策略或生产测试注入钩子。不改生产逻辑、Public API、GAS Tags、Config、Build.cs、资产、Montage工厂或Execution fixture。

Exhaustion的0.1秒步长和两次零Delta预热保留；SkillBar、Front/Backstab、VictimPresentation、ImpactGeometry及其他局部Tick/直接Montage推进均不迁移。不处理F12/F14、FIX2 World缺EndPlay或其他既有测试债，不统一World创建、BeginPlay/EndPlay、销毁、Timer/physics/nav前置、时间膨胀恢复及对象持有。其他候选保持ROADMAP既有归属，不增加03C前置。

## 3. Approved Paths 与调用点账本

仅下列七份Source/Test文件可改；同文件不等于允许改其他符号。调用数是基线复核结果，不包含函数定义及Advance内部的一次Tick调用。

| 批准绝对路径 | 符号与允许修改范围 |
|---|---|
| E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\Tests\CombatAutomationFixture.h | 在现有测试宏内增加TickWorld、AdvanceWorld声明与必要说明；不增加include，已有class UWorld前置声明 |
| E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\Tests\CombatAutomationFixture.cpp | 增加上述两个实现和EngineGlobals.h；已有Engine/World.h，SpawnPlayer/SpawnPassiveEnemy不改 |
| E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\Tests\CombatHitFeedbackAutomationTests.cpp | 删除TickHitFeedbackTestWorld、AdvanceHitFeedbackTimer；替换6次外部Tick、40次Advance；删除EngineGlobals.h |
| E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\Tests\PlayerDefenseAudioAutomationTests.cpp | 删除TickPlayerDefenseAudioTestWorld、AdvancePlayerDefenseAudioTestWorld；替换2次Advance |
| E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\Tests\ChargedAttackNiagaraFeedbackAutomationTests.cpp | 删除TickChargedAttackTestWorld、AdvanceChargedAttackTimer；替换4次Advance |
| E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\Tests\ParrySuccessImpactFeedbackAutomationTests.cpp | 删除TickTestWorld、AdvanceTestTimer；替换5次Advance |
| E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\Tests\ProjectileFlightTrailAutomationTests.cpp | 删除TickFlightTrailTestWorld、AdvanceFlightTrailTimer；替换2次Advance |

后四文件没有外部Tick调用，也没有显式EngineGlobals.h；五个消费者已有CombatAutomationFixture.h。只删除CombatHitFeedback中不再使用的EngineGlobals.h，不顺手清理其他include。当前准确账本为6次外部Tick、53次Advance；Gemini建议中的CombatHitFeedback 41次与Charged 5次已校正为40和4。

Main文档白名单：E:\GameDevelop\PolyQuest\plan.md、E:\GameDevelop\PolyQuest\ROADMAP.md、E:\GameDevelop\PolyQuest\ROADMAP-archive.md。纯测试支撑片不改生产架构事实，不修改ARCHITECTURE.md。Gemini不得改Main文档。

## 4. 共享接口与冻结契约

在既有WITH_DEV_AUTOMATION_TESTS内，为FCombatAutomationFixture新增：

```cpp
static void TickWorld(UWorld* World, float DeltaSeconds);
static void AdvanceWorld(UWorld* World, float DeltaSeconds);
```

### TickWorld

- 原样迁移if (World)内的World->Tick(ELevelTick::LEVELTICK_All, DeltaSeconds)，之后执行一次++GFrameCounter。
- World为空时不操作；零Delta仍Tick一次并递增帧号。不加其他状态门、不提前跳过零Delta。
- 不Clamp或拆分单帧调用；原有0.06f仍是一次Tick，不得替换成AdvanceWorld。

### AdvanceWorld

- 固定constexpr float MaxTickStepSeconds = 0.05f，不增加参数、模板或默认策略。
- 保留while (DeltaSeconds > KINDA_SMALL_NUMBER)；每步FMath::Min(DeltaSeconds, MaxTickStepSeconds)，调用TickWorld，再扣除该步长。
- 保留尾步和原浮点终止规则：0.08f仍为0.05f加0.03f；零/负时长不执行循环。
- 不新增Clamp、预热、TimerManager直调、时间膨胀补偿或特殊输入分支；无需为未使用的NaN/Inf调用扩大接口策略。

删除五文件局部两个helper，调用点直接换成FCombatAutomationFixture::TickWorld/AdvanceWorld，不留转发壳。实参、次数、顺序和调用位置不变；测试断言、CDO/实例配置、GAS权属及所有World生命周期保持。必要文档注释说明AdvanceWorld固定0.05秒，避免被误认为任意步长工具。

## 5. 验证矩阵、Editor清单与失败处理

本节保留批准时的验证要求与迁移前收据状态；实施后最终证据及原前置收据缺口见第9节，不把下表历史待补项改写为已执行。

Editor制作、资产写入、迁移及readback清单为空；纯测试支撑片不需要PIE。无新增运行时或资产行为，不沿用上一片五动作PIE作为本片门禁。

| Automation | 主要保护范围 | 迁移前证据状态 |
|---|---|---|
| PolyQuest.Combat.HitFeedback | 单帧/分步推进、Hit-stop和恢复 | 待适用成功收据或用户基线运行 |
| PolyQuest.Combat.AttackerImpactCameraShake | 攻击者反馈与计时恢复 | 同上 |
| PolyQuest.Combat.CameraFovPunch | 同文件相邻回归 | 同上 |
| PolyQuest.Combat.DefenseAudio | 防御反馈与延迟恢复 | 同上 |
| PolyQuest.Combat.ChargedAttackNiagaraFeedback | 蓄力计时与退出 | 复用SHRINK中Main核实的成功收据；该测试及相关支撑在当前基线未再改变 |
| PolyQuest.Combat.ParrySuccessFeedback | 非整步时长与反馈恢复 | 待适用成功收据或用户基线运行 |
| PolyQuest.Projectile.FlightTrail | 轨迹反馈与延迟清理 | 同上 |

### 迁移前

- 复用用户已确认的当前基线Development Editor编译通过，不为建立基线重复编译。
- 优先复用源码、fixture及相关配置未变且可追溯的成功Automation收据。现已确认Charged专项可复用；其余六项若没有适用收据，由用户在未迁移基线上补跑，保存首条错误及结果后再改源码。
- Git工作树干净或提交历史连续不构成测试成功证据。本轮计划落盘未运行上述测试；不得虚报基线全部通过。
- 若基线失败，先报告具体测试与首个有效错误，不归因于尚未实施的抽取，也不顺手修复或擅自减少七文件/七项验收范围。

### 实施后

1. Gemini核对七文件白名单、6次外部Tick及53次Advance映射、实参/次数/顺序与所有原有效断言保全；Rider可用时执行适用诊断，不可用记录coverage fallback；始终执行git diff --check。
2. 用户执行UE5.8 PolyQuestEditor / Development Editor编译及上述全部七项Automation。本片实现后验收不可由旧收据替代。
3. Main按ue-strict-review进行有界Fresh Review，核验帧号顺序、尾步、单帧调用与生命周期不变；图谱只用于适用的有界导航/影响，不代替运行证据。

复用既有行为测试，不新增包装函数重复套件或注入钩子。空World/零Delta等未由既有用例动态覆盖的边界按源码等价性核对并标明证据层级。新失败只在批准路径内做有证据修复加一次专项复跑；同根仍失败则报告，不修改生产逻辑、放宽断言或扩展其他测试债。

## 6. Executor交付要求

1. 先读本仓库AGENTS.md与本plan，核对a19630d基线、七文件和调用账本；只探索目标符号与必要直接依赖，不重新全库审计。
2. 遵守第5节迁移前证据门；先查已有适用收据，缺少时只请求缺项，不重复索要已确认的编译和Charged专项，不擅自代用户运行验证。
3. 门禁满足后按第4节一次完成七文件最小迁移；不增加抽象、参数、转发壳或生产开关。
4. 首次交付按项目规则优先派发一个干净只读子代理做严格实施自审，子代理不得递归；后续窄修复单兵复核，不再派发。
5. 交付列出实际文件、helper删除、调用点映射、保留的差异消费者、真实净行数、静态/自审证据与用户剩余门禁。不采用约净删90行作为目标，不为收益扩容。
6. 不改Main文档、资产、Config、Build.cs、清单外Source，不暂存、不提交、不推送，不自动进入03C。

## 7. 完成态与归档/提交门禁

七文件迁移、Development Editor编译、七项Automation及Main Fresh Review完成后才关闭本片。记录静态与用户运行证据，不将无关旧债作为新增前置，也不随抽取关闭EndPlay、F12/F14等既有债务。

Main更新本plan完成态、ROADMAP当前/下一入口并向archive追加完成态；不修改ARCHITECTURE稳定生产事实。下一入口为TODO-03C，不自动增设下一批测试清理。提交需要本片新的明确用户授权，仅暂存七份批准Source/Test与三份阶段文档；Content/Config/tmp WIP保留，严禁git add -A。

## 8. 本轮落盘记录

2026-09-13：用户要求将修订方案写入plan.md并给出执行提示词。Main仅修改三份阶段文档，固定SHRINK凭据并追加交接；本片未实施，未编译、未运行Automation/Editor/PIE、未派发执行者、未暂存或提交。执行提示词随本次回复交付。

## 9. Main完成态与提交凭据（2026-09-13）

- **范围与结果**：Gemini按七文件白名单完成五份同义样板抽取。共享TickWorld在有效World上先Tick再递增GFrameCounter；AdvanceWorld保留0.05f最大步长、KINDA_SMALL_NUMBER终止规则和尾步。六次外部Tick、53次Advance调用保留实参、顺序与次数；无参数化、转发壳或新文件。相对a19630d增加89行、删除160行，净减71行。
- **静态及实施自审**：Gemini交付报告记录七文件Rider errors为空、git diff --check通过及首次干净只读实施自审PASS。Main复核实际差异、批准范围和git diff --check；将基线去除原helper、替换调用名及移动指定include后，与五消费者现文件逐一比较，忽略空行/行尾空白后完全一致，断言、配置和World生命周期无其他变化。SpawnPlayer/SpawnPassiveEnemy保持原样。
- **用户编译**：用户在交付后明确确认编译通过，作为本片PolyQuestEditor / Development Editor收据；Main未自行运行编译，不冒充独立构建日志证据。
- **实施后Automation**：用户确认七项全部通过，并转交日志核查结果；Main在本机Saved/Logs/PolyQuest.log读取七项Result={Success}，对应2026.09.13-04.04.22 UTC（本地12:04:22）行2369/2387/2403/2433/2470/2499/2521；行2527为TEST COMPLETE. EXIT CODE: 0，行2526为GIsCriticalError=0。七项为AttackerImpactCameraShake、CameraFovPunch、ChargedAttackNiagaraFeedback、DefenseAudio、HitFeedback、ParrySuccessFeedback及Projectile.FlightTrail，完整路径见第5节；Main未代跑测试。
- **Fresh Review**：Main按ue-strict-review完成七文件有界只读审查，通过，无P0–P2发现。图谱基线对应a19630d；动态测试关系覆盖不足以当前diff/源码补足，没有追加无关CodeGraph探索或重复审查。用户随后明确允许文档收尾提交，沿用本轮审查结论。
- **证据边界与原前置偏差**：Charged专项的迁移前成功收据可复用，其余六项的迁移前成功收据未提供，不能证明原计划“先基线后迁移”的执行顺序。Main已在Review回执中说明，用户知悉后授权收尾提交；本片按源码等价比对和实施后七项通过完成验收，该历史缺口非代码缺陷、不阻塞03C，唯一后续触发归ROADMAP的Debt-03H6-TEST-SHRINK-Baseline。实施后Success不追认迁移前已执行。
- **其他验证边界**：纯测试支撑片按计划无Editor制作、资产readback或PIE门禁，不称PIE豁免/通过。空World/零Delta等没有新增动态用例，保留静态等价性覆盖。既有F12/F14、FIX2-PIE、World缺EndPlay及其他模块债务保持原归属，不被本片七项Success关闭。
- **文档与提交授权**：Main维护plan.md、ROADMAP.md和ROADMAP-archive.md，archive仅追加；ARCHITECTURE及生产源码不改。用户明确批准提交，精确清单为七份Source/Test加三份文档共10文件；2,122项Content/Config/tmp WIP保留并排除，不推送。当前plan作为完成态保留，下片替换前以本次提交固定引用。
- **下一入口**：TODO-03C Ranged Enemy v1。有限TEST-SHRINK已完成，不继续扩展World Tick消费者，不追加Montage/Execution fixture清理作为前置；下一阶段独立制定计划，远程投射物PIE及剩余集成矩阵按原ROADMAP触发验证。
