# TODO-03H6-SHRINK：RateWindow 绑定去重与测试接口清理

## 1. 目标、基线与职责

**状态（2026-09-13）：TODO-03H6-SHRINK实现、验证及Main Fresh Review已完成。用户回传修复后的EnemyLaunchReactionRootMotion Success，并确认编译、PIE通过，明确授权文档收尾及提交。批准范围为15份Source/Test及4份Main文档；本plan保留完成态，证据见第9、10节。下一阶段为独立TODO-03H6-TEST-SHRINK，尚未启动实施。**

完成已接受的有限 SHRINK：五个玩家 Ability 共用一份 RateWindow 绑定实现，删除11个无消费者测试 getter。完成验证及 Fresh Review 后，解除 SHRINK 对 TODO-03C 的排期阻塞；不将本片关闭等同于全项目不存在重复。

- 工作区：E:\GameDevelop\PolyQuest；引擎唯一依据：D:\UE\UE_5.8。
- 基线 HEAD：3271323b91386fd5f5d23e471c23f64f3a9197dd，FIX2 已提交。落盘前 Source 和阶段文档无未提交差异；既有 Content、Config、tmp WIP 保留并排除。
- FIX2 完整凭据：git show 3271323b91386fd5f5d23e471c23f64f3a9197dd:plan.md 第8节；FIX2 完成态及本次交接已归档 ROADMAP-archive.md。
- FULL-AUDIT 台账固定为 git show 851c7ac:plan.md 第4节；不重新全库审计。
- Outer：ue-stage-workflow；Primary：ue5-cpp-gameplay；Support：ponytail lite。
- Route reason：五个 Native GAS 消费者的同义绑定样板抽取及普通测试接口删除，不改变运行时所有权。
- Execution route：manual/out-of-band Gemini；Implementation executors：1，由用户手动交接，本轮不自动派发。
- Main：架构、计划、范围、Fresh Review、文档、暂存和提交；Gemini：批准范围内 C++/测试实现、静态检查、实施自审和问题排查；用户：编译、Automation、Editor/PIE 与最终提交批准。未经另行明确委托，Agent 不自行运行编译或用户验证。

### 1.1 已接受的技术裁决

1. 不给生产 helper 添加测试注入钩子。本机 UE5.8 AbilityTask_WaitGameplayEvent.cpp 的 Activate 只注册监听、不主动广播 GameplayEvent；但 GameplayTask.cpp 的 ReadyForActivation 还包含任务激活、Owner 通知和失败时 EndTask 等路径，不能据此删除返回检查。保留防御检查，静态审查其重入边界，不强求人为注入逐分支覆盖。
2. Private helper 使用非模板 struct FMontageRateWindowBinding，内部提供模板静态 Bind 方法。Public 头文件只添加 friend struct FMontageRateWindowBinding;，不包含 Private 头文件、不增加复杂模板 friend。
3. 使用正常 AddDynamic(Context, &TContext::OnBegin/OnEnd)。本机 Delegate.h 的名称提取对简单模板别名 TContext 可得到 OnBegin/OnEnd；没有模板反射异常的编译证据，不预设改用 __Internal_AddDynamic。真实编译失败时基于首条错误定向修复。
4. 11 getter 经当前 Source 检索仍仅有定义。只删除对应方法，不连带字段、setter 或有效测试接口。

### 1.2 Deliberate Non-goals

不重新全库审计，不统一 Ability/Context 类型，不重做 FIX1，不调整玩法，不抽取跨文件测试 fixture，不退役 Launch，不清理额外 getter/setter，不处理其他 FULL-AUDIT 候选。无 Blueprint API、Tag、Config、Build.cs、外部库、资产、网络复制或 GAS 外动作状态新增。

额外候选继续由 ROADMAP 的 REC-03H6-AdditionalShrink、REC-03A7-01 和 RET 闭包分别承接；只在相关模块下次维护或用户明确接受后独立冻结范围，不自动纳入本片或成为新03C前置。

## 2. A：共享绑定实现与 Runtime Contracts

### 2.1 五消费者迁移

迁移 ChargedAttack、SprintAttack、PlayerMeleeSkill、BowDrawFire、Dodge。当前五个 BindRateWindow 在类型名和结束标志名称归一后完全相同。

新增 Private、无状态、非反射的 FMontageRateWindowBinding，内部提供模板静态 Bind。参数包括具体 Ability、AnimInstance、Montage 和实时结束标志引用，保留具体 Context 类型。五个原 BindRateWindow 方法保留为薄转发。helper 不保存状态、不创建新的 UObject 类型、不引入运行时策略表。

### 2.2 顺序与失败契约

1. 原 ClearRateWindow() → 增加 RateWindowBindingToken。
2. 检查 Ability 活动与结束状态、AnimInstance/Montage 有效性、实际活动实例及未停止状态。
3. 保存 RateWindowAnimInstance、RateWindowMontage 和实例ID，调用既有 BindAndCapture，检查 IsBound。
4. 创建各自 Context，写入原有持有成员、OwningAbility 和 token；创建 Begin/End Task 并在激活前写入原有持有成员，检查创建结果后绑定动态委托。
5. 依次调用 Begin、End 的 ReadyForActivation；每次返回后先检查当前 Ability/结束标志/token/Context 身份，再按原顺序检查 Task 指针、有效性、活动状态及实例资格。
6. 当前绑定失败沿用原 EndAbility 出口。若同步结束或重触发已经改变身份，只返回 false，不清理新绑定、不恢复旧状态、不结束新一代 Ability。

### 2.3 权属与接口边界

- Bow 保留 bEndAbilityInProgress，其余四个保留 bEndAbilityRequested；不统一改名，不按值快照结束标志。
- Context、Task 的 UPROPERTY 持有关系、Owner/token 门禁、反射回调和具体类型保持不变。
- FIX1 的 FAbilityMontageRateWindowLifecycle::IsCurrentMontageInstance 继续作为实例授权依据；不修改共享状态算法、Tag、baseline 和恢复政策。
- ClearRateWindow、EndAbility、OnRateWindowBegin/End、Montage 播放及 BindRateWindow 的调用位置均保留在原 Ability。
- 不新增 Blueprint API、运行时注入钩子、全局测试状态或生产测试开关。GAS 和原 ASC 权属不变。
- 对象/状态有效性防护、两次 Ready 返回检查以及单一清理出口不能因抽取而删除或放宽。

### 2.4 其他消费者的封闭裁决

| 消费者 | 本片裁决与依据 |
|---|---|
| Light | 保留。Combo entry 切换、失败恢复及监听建立后捕获基线的顺序不同。 |
| Enemy Melee、Big、Small、Launch | 保留。监听建立与 Montage/其他 Task 启动交织，并承担各自动作清理。 |
| Enemy Victim | 保留。仅在非致死 Recovery 中条件绑定，失败存在局部清理出口。 |
| Enemy StanceBreak | 保留。ExecutionContext 同时承载 Montage 回调，启动顺序独立。 |

上述保留裁决满足其他消费者适用性核对，不要求新增迁移阶段。只共享五份完整同义流程，不为几行相似 Task 创建代码扩展公共框架。

## 3. B：11 getter 逐项删除

全部位于 WITH_DEV_AUTOMATION_TESTS 内，为普通非虚、非反射方法。实施前再次核验消费面。

| 头文件（目录见第4节） | 删除符号 |
|---|---|
| EnemyVictimExecutionAbility.h | GetTestHasSavedCanWalkOffLedges |
| EnemyLaunchReactionAbility.h | GetTestRootMotionKnockdownCompletedNaturally |
| DodgeAbility.h | GetTestAttackingStateTag、GetTestDodgingStateTag、GetTestHitReactingStateTag、GetTestPlayerLaunchReactionAbilityTag |
| ChargedAttackAbility.h | GetTestChargeVFXSystem、GetTestChargeVFXTraceSourceName、GetTestMaximumChargeDuration、GetTestChargeVFXComponent、GetTestAttachParent |

只删除11个单行方法，保留关联生产字段、setter 和有效 GetTestSavedCanWalkOffLedges；当前 Victim getter 的真实消费者位于 ExecutionVictimRootMotionAutomationTests.cpp。若实施基线出现新消费者，对该项记录确切保留用途，不迁移清单外调用方，不为追求11项删除而削弱测试。

## 4. Approved Paths 与符号白名单

原封闭清单为14份 Source/Test（含1份新增 Private header）；2026-09-13用户明确追加批准下列EnemyLaunch专项测试文件，现为15份。同文件不等于批准其他符号。新增项由Main按单文件测试窄修复例外处理，不扩大Gemini原14文件交付范围。除此之外的源码、测试、资产、Config、Build.cs 和工具文件均不可修改。

| 批准绝对路径 | 符号 / 允许修改范围 |
|---|---|
| E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\AbilitySystem\Abilities\MontageRateWindowBinding.h | 新增无状态 FMontageRateWindowBinding 及模板静态 Bind；必要 include 和契约注释；禁止测试注入钩子 |
| E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\AbilitySystem\Abilities\ChargedAttackAbility.cpp | BindRateWindow 薄转发及必要 include |
| E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\AbilitySystem\Abilities\SprintAttackAbility.cpp | BindRateWindow 薄转发及必要 include |
| E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\AbilitySystem\Abilities\PlayerMeleeSkillAbility.cpp | BindRateWindow 薄转发及必要 include |
| E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\AbilitySystem\Abilities\BowDrawFireAbility.cpp | BindRateWindow 薄转发及必要 include |
| E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\AbilitySystem\Abilities\DodgeAbility.cpp | BindRateWindow 薄转发及必要 include |
| E:\GameDevelop\PolyQuest\Source\PolyQuest\Public\AbilitySystem\Abilities\ChargedAttackAbility.h | 单行 helper friend、第3节5个 getter 删除；必要时在现有测试宏内增加直接调用原 BindRateWindow 的单行薄入口 |
| E:\GameDevelop\PolyQuest\Source\PolyQuest\Public\AbilitySystem\Abilities\SprintAttackAbility.h | 单行 helper friend；必要时在现有测试宏内增加直接调用原 BindRateWindow 的单行薄入口 |
| E:\GameDevelop\PolyQuest\Source\PolyQuest\Public\AbilitySystem\Abilities\PlayerMeleeSkillAbility.h | 单行 helper friend；必要时在现有测试宏内增加直接调用原 BindRateWindow 的单行薄入口 |
| E:\GameDevelop\PolyQuest\Source\PolyQuest\Public\AbilitySystem\Abilities\BowDrawFireAbility.h | 单行 helper friend；必要时在现有测试宏内增加直接调用原 BindRateWindow 的单行薄入口 |
| E:\GameDevelop\PolyQuest\Source\PolyQuest\Public\AbilitySystem\Abilities\DodgeAbility.h | 单行 helper friend、第3节4个 getter 删除；必要时在现有测试宏内增加直接调用原 BindRateWindow 的单行薄入口 |
| E:\GameDevelop\PolyQuest\Source\PolyQuest\Public\AbilitySystem\Abilities\EnemyVictimExecutionAbility.h | 仅删除 GetTestHasSavedCanWalkOffLedges |
| E:\GameDevelop\PolyQuest\Source\PolyQuest\Public\AbilitySystem\Abilities\EnemyLaunchReactionAbility.h | 仅删除 GetTestRootMotionKnockdownCompletedNaturally |
| E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\Tests\PlayerMontageRateWindowAutomationTests.cpp | 复用现有真实 ASC fixture，补充绑定拒绝、失败回退及重新绑定测试；不抽取跨文件 fixture，不删除有效断言 |
| E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\Tests\EnemyLaunchReactionRootMotionAutomationTests.cpp | 用户追加批准：仅修复文件内合成Montage/AnimInstance/ASC播放前置，补充真实播放及Ability活动断言；保留非法配置负例、自然结束/取消/销毁清理断言；不改生产Ability、不新增bypass或公共fixture |

薄测试入口只能直接调用原生产方法，不能改变过程、伪造返回值或提供回调注入。

Main 专属文档清单：E:\GameDevelop\PolyQuest\plan.md、E:\GameDevelop\PolyQuest\ROADMAP.md、E:\GameDevelop\PolyQuest\ROADMAP-archive.md；E:\GameDevelop\PolyQuest\ARCHITECTURE.md 仅在验证和终审后更新稳定事实。本次计划落盘只改前三份。

## 5. Editor 清单与验证矩阵

Editor 制作、资产写入、资产迁移与 readback 清单均为空。PIE 使用既有场景/资产；若验证必须修改资产，先向 Main 报告具体对象和原因，未经用户另批不得写入。

| 门禁 | 执行要求与证据 |
|---|---|
| 静态 | 核对原14路径及追加测试文件共15路径和符号白名单、getter消费面、Public不包含Private、两次Ready返回检查、单一清理出口；执行 git diff --check。Rider可用且适用时补充error-level诊断；不可用记录coverage fallback。 |
| 编译 | 用户执行 UE5.8 PolyQuestEditor / Development Editor，验证UHT、五种模板实例化、friend访问及AddDynamic；真实错误按首条证据定位，不预设内部宏后备。 |
| 玩家既有Automation | PolyQuest.Combat.PlayerMontageRateWindow 全部六项：Light、MeleeSkill、Sprint、Charged、Bow、Dodge。保留真实ASC激活、Begin/End实际回调、非法事件、取消、旧Context拒绝、同资产替换及Dodge重触发断言。 |
| 新增绑定测试 | 在现有五消费者fixture中覆盖：结束态绑定拒绝；活动Ability遇到非法AnimInstance/Montage后失败并完整结束；同一活动Ability连续绑定产生独立Context/token，旧回调不能影响新绑定，最终无旧监听残留。失败路径检查GAS活动状态、Context、Task和生命周期清理，不能只断言返回false。 |
| 防御分支证据 | Ready内部同步结束/重入防御保留并静态审查；不使用helper注入钩子强行覆盖，不把普通取消/重触发测试称为该返回点动态覆盖。 |
| getter回归 | PolyQuest.Combat.ChargedAttackNiagaraFeedback、PolyQuest.Combat.ExecutionVictimRootMotion、PolyQuest.Combat.EnemyLaunchReactionRootMotion；EnemyLaunch专项按用户追加批准修复并经用户重新编译/运行，其余通过收据按未受影响范围保留。 |
| 用户PIE | 既有Scene01中五个迁移动作分别验证窗口速率、退出恢复、取消后再次激活；重点检查Charged暂停/释放、Bow Section切换、Dodge连续重触发。getter删除不另加PIE。 |
| Fresh Review | Main应用ue-strict-review，在批准diff内核验绑定等价性、实例/Context隔离、Ready重入边界、失败出口、测试有效性及范围；不重新全库审计。 |

静态、编译、Automation、PIE和视觉证据分别记录；本计划制定时没有本片编译/Automation/PIE或实现Fresh Review收据。对同一失败根因最多一次有证据修复加一次针对性复跑；复现仍在则报告，不自动循环或扩大范围。

## 6. Executor 自包含交付要求

1. 先读实时 AGENTS.md、本 plan、git status及目标源码/直接依赖。基线参考第1节；若HEAD变化先检查是否触及批准契约，不回滚或覆盖既有WIP。源码导航遵守CodeGraph优先；图谱缺失、过期或未覆盖时记录coverage fallback并聚焦读取，不重跑全库审计。
2. 先Charged/Sprint完成抽取，再迁移MeleeSkill/Bow/Dodge，最后删getter；保持同一封闭切片，不因局部命名、算法或测试细节反复请示。
3. 本片首次交付按项目规则优先派发1个干净只读子代理做严格实施自审；只审批准diff及必要契约，不写文件。后续微调/Bug修复单兵复核，不再派发。不可用则明确记录，不冒充独立自审。
4. Gemini只改第4节14份Source/Test清单。不修改Main文档，不操作资产/Config/Build.cs，不引入依赖，不暂存、不提交、不推送、不自动进入03C。
5. 超出白名单、需要资产写入、架构/生命周期契约改变或验收标准放宽时，向Main报告具体证据和最小必要扩展；不能自行放宽。已批准范围内常规细节自主闭环。
6. 交付说明列出实际修改文件/符号、共享实现及五消费者迁移、其他消费者保留裁决、11 getter逐项结果、实际diff统计（生产缩减/测试增量/getter删除分列）、静态与实施自审证据、待用户编译/Automation/PIE门禁和真实剩余风险。
7. 不将计划、静态结果、历史绿色收据或普通取消测试当本片运行时证据。Ready防御分支没有动态覆盖时明确写静态覆盖边界。

## 7. 完成标准与文档 / 提交门禁

- 五消费者迁移、其他消费者保留裁决、11 getter逐项删除或基线漂移后的证据保留全部交付；只做A或B不能关闭本片。不按预设净行数扩容。
- 编译、适用Automation、用户PIE与Main Fresh Review门禁完成。用户明确接受豁免时，在ROADMAP写唯一债务与具体关闭触发；不能自行豁免。
- Ready防御分支的静态覆盖边界记录在交付/收口证据中，不增设阻塞任务；未来任务类型或激活流程改变、出现实际同步回调路径时补对应集成测试。
- Debt-03H6-FIX2-PIE和其他已知债务保持原归属，不因SHRINK完成而消失；REC-03H6-AdditionalShrink继续承接额外候选，相关fixture维护前处理既定前置，不顺手加片。
- Main验收后更新ARCHITECTURE稳定事实、ROADMAP下一入口及风险唯一归属、归档完成态，并保留可追溯的当前plan。按用户在本计划落盘后接受的新排期，完成后进入独立TODO-03H6-TEST-SHRINK，再到TODO-03C；不扩展当前实现范围。
- Git提交必须用户明确批准。届时仅暂存批准清单内实际改动，核对cached diff与空白检查；既有Content/Config/tmp WIP排除，严禁git add -A。提交标题/正文遵守AGENTS，未获批准不提交。

## 8. 本轮计划落盘记录

2026-09-13：Main按用户接受的修订方案建立本计划，追加FIX2→SHRINK归档并同步ROADMAP交接指针；Source/Test、ARCHITECTURE及资产/Config未修改。不运行编译、Automation、Editor/PIE，不派发子代理、不暂存、不提交。文档检查收据由本次交付回复给出，不将本节作为实现验收。

## 9. 用户批准的 EnemyLaunch 测试前置窄修复（2026-09-13）

- 首次失败证据：Saved/Logs/PolyQuest.log，2026.09.13-03.07.44 UTC；原6项PlayerMontageRateWindow和ChargedAttackNiagaraFeedback、ExecutionVictimRootMotion为Success，EnemyLaunchReactionRootMotion为Fail。首个异常链为PlayMontage失败 → root motion knockdown montage did not start → 2.5活动阶段/2.5b Facing状态断言失败。8项绿色不能关闭第9项门禁。
- 当前失败路径涉及的生产EnemyLaunch .cpp与专项测试在修复前均与3271323基线一致；本片EnemyLaunch头文件只删未调用getter。无证据把播放失败归因于getter删除，也未做同环境基线复跑，不能声称已排除一切环境/顺序因素。Debt-03H6-LaunchRetirementReadback属于旧Launch退役，不承接本失败。
- 用户明确批准将EnemyLaunchReactionRootMotionAutomationTests.cpp加入本片，由Main单兵窄修复。只修改该测试文件及当前计划/路线图记录，不动Gemini原交付、生产Ability、公共fixture、资产或Config，不派发后续修复子代理。
- 修复：沿用既有RateWindow suite的本地可播放合成动画做法，创建Skeleton/root track/Default section；Montage-only AnimInstance挂入对应敌人Mesh并刷新ASC ActorInfo，临时敌人使用独立实例。取消该suite原先的CDO Mock注入及播放bypass调用，保留原自然完成、取消、非Walking、非法Montage和销毁晚回调测试目的与断言。
- 测试增加root track建立、ASC动画引用一致及实际Montage播放/Ability活动检查。首个合法激活失败时早停；非法Montage长度仍使用有效Sequence数据以隔离单一负例。动画数据构建依赖WITH_EDITOR，因此该EditorContext suite明确同时受WITH_DEV_AUTOMATION_TESTS与WITH_EDITOR保护。
- 静态证据：Rider专项文件error-level检查无错误，git diff --check通过；原98个编号标签保留，修复前后其余Source文件SHA-256一致。此处不是UE编译、Automation运行、真实跨帧Root Motion移动或视觉证明。
- 验收结果：用户回传Test Run 3的PolyQuest.Combat.EnemyLaunchReactionRootMotion为Success，随后明确确认编译、PIE通过。其余8项保留原未受影响通过收据，不冒充全套新版复跑。非法前置拒绝、Walking转Falling中止及无StanceBreak消费者恢复Poise日志与负例/生命周期场景一致，专项失败已关闭，不转挂旧Launch退役债。

## 10. Main完成态与提交凭据（2026-09-13）

- **实现**：Gemini交付原14文件；五个玩家Ability共用Private无状态FMontageRateWindowBinding，原Context/Token、结束标志引用、实例授权、两次Ready返回检查及EndAbility出口保持。Public只加friend和测试宏内薄入口；11个普通无消费者getter净删，字段/setter及有效Getter保留。Main追加的第15文件仅修复EnemyLaunch测试播放前置，未改生产Ability或公共fixture。
- **实际行数**：相对3271323，五个生产cpp合计净减335行，新增helper99行及friend5行，绑定抽取合计净减231行；getter净减11行；五个薄测试入口增加5行，PlayerRateWindow测试增加38行，EnemyLaunch测试修复净增19行。15份Source/Test合计增加264行、删除444行，净减180行；不计文档及既有WIP，不继承审计预估。
- **静态与自审**：Gemini报告Rider无错误、首次独立只读实施自审Pass；Main的EnemyLaunch窄修复Rider error-level结果为空。Main核对批准diff、11 getter残余引用及git diff --check通过。图谱基线匹配3271323；动态测试关系覆盖不足以源码核对，不将图谱Untested标签当真实缺测试。
- **Automation**：Main此前核实本机日志的PlayerMontageRateWindow.Bow/Charged/Dodge/Light/MeleeSkill/Sprint、ChargedAttackNiagaraFeedback、ExecutionVictimRootMotion共8项Success；追加修复后采用用户回传EnemyLaunchReactionRootMotion专项Success。九项均有适用成功收据，不声明当前版本全套同时重跑。
- **用户门禁**：用户在本片验证矩阵及Fresh Review回执后确认“编译通过，pie通过，可以开始文档收尾然后提交”，据此关闭Development Editor编译与Scene01五动作PIE门禁。无本片资产制作/readback要求；不把该回执扩展为打包、网络、旧Launch资产退役readback或FIX2远程投射物PIE证明。Main未代跑编译、Automation或Editor/PIE。
- **Fresh Review**：Main按ue-strict-review审查15份批准Source/Test及直接契约，通过，无P0–P2发现。Ready防御分支保留静态覆盖，不添加生产注入钩子；未来任务类型/激活流程出现真实同步回调路径时再补集成覆盖。合成root轨道及播放断言不额外证明真实跨帧Root Motion位移。
- **债务与路线**：本片无未关闭门禁，Debt-03H6-TestSeamCandidates随11 getter清理关闭；FIX2-PIE、旧Launch退役readback及其他审计项保持原归属。按用户已接受排期，下一步独立制定TODO-03H6-TEST-SHRINK（仅World Tick样板）计划，其后进入TODO-03C，不自动追加Montage/Execution fixture瘦身。
- **文档与提交授权**：更新plan.md、ROADMAP.md、ROADMAP-archive.md、ARCHITECTURE.md；archive仅追加完成态。用户已明确批准提交，精确暂存15份Source/Test及4份文档，共19文件；2,122项既有Content/Config/tmp WIP保留并排除，不推送。新切片替换plan前以本次提交固定本凭据。
