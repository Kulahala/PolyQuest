# TODO-03H6-FIX1：RateWindow 实例授权修复与共享校验

## 1. 目标、基线与角色

**状态（2026-09-13）：FIX1 实现验收收口，Main delta review 无未关闭 P0–P2。Development Editor 编译、6 组 Focused Automation 共 11/11 通过；用户按本轮 Scene01 清单验证后确认“测试通过，可以开始文档收尾然后提交”，已授权本片提交。修复前 RED 未实际运行，不追认为通过；按本轮收尾授权保留为非阻塞证据债 Debt-03H6-FIX1-PreFixProof。下一阶段为 FIX2，03C 尚不放行。**

修复 H6-F01：同一 Montage 重播后，旧实例仍未停止、旧 Ability/Context 仍有效时，旧 Begin、End、Clear 不得修改新实例速率。所有适用 Player/Enemy 消费者复用既有 FAbilityMontageRateWindowLifecycle 中的一份实例授权规则。

- 工作区：E:\GameDevelop\PolyQuest。
- 基线 HEAD：851c7ac5f01fc470d64469831be40125dd0dece2；引擎：D:\UE\UE_5.8。
- 规划落盘前 Source 与阶段文档无未提交差异；既有 2,122 项 WIP（Content 2,117、Config 1、tmp 4）保留并排除。
- FULL-AUDIT 完整证据固定读取：git show 851c7ac5f01fc470d64469831be40125dd0dece2:plan.md；第4节为 Findings/候选，第6/7/8节为覆盖、检查点和结果。上一阶段交接已追加至 ROADMAP-archive.md。
- Outer：ue-stage-workflow；Primary：ue5-cpp-gameplay；Support：none。
- Route reason：既有 Native GAS RateWindow helper 与消费者的实例授权修复，不改变玩法权属。
- Execution route：manual/out-of-band Gemini；Implementation executors：1（Gemini，用户手动交接；本轮未自动派发）。
- Main：架构、计划、范围、Fresh Review、文档与提交；Gemini：批准范围内测试/源码、自审和问题排查；用户：手动编译、Automation/Editor/PIE验证和最终提交批准。

采纳 Gemini 对 bypass、绑定时序、Melee fixture 和 Player 包装函数边界的补充；不采用默认填入测试 ID 1 的做法。没有真实实例时保存 INDEX_NONE；纯算法测试依靠显式 bypass，关闭 bypass 后必须拒绝。真实实例测试捕获实际 ID，不能用合成 ID 替代。

### Deliberate Non-goals

不处理 FIX2、不合并 BindRateWindow 样板、不删除 getter、不合并 Context、不改 Montage 停止策略、GAS 权属、Tag、Config、资产或 Build.cs。不新建生产文件、通用 Ability 基类或通用测试框架。只修 Enemy 的方案更小，但既定路线要求共享授权；本片只共享这一条规则，其余去重留给 SHRINK。

## 2. Runtime Contracts 与接口

### 2.1 单一生产授权规则

在现有 MontageRateWindowLifecycle.h/.cpp 新增普通 Native 静态接口，无反射或 Blueprint 暴露：

```cpp
static bool IsCurrentMontageInstance(
    const UAnimInstance* AnimInstance,
    const UAnimMontage* Montage,
    int32 BoundInstanceID);
```

实现仅放在 .cpp，同时检查：
1. AnimInstance、Montage 有效，绑定 ID 不为 INDEX_NONE。
2. GetActiveInstanceForMontage(Montage) 返回实例。
3. 返回实例的 Montage、Instance ID 与绑定身份一致，且 !IsStopped()。

不使用 IsPlaying()：暂停不应使 Charged 丧失实例所有权。静态接口不接收 bypass 参数，也不含测试放行分支。保留 Character-owned ASC 与 GAS 的唯一玩法权威；不增加网络机制或平行动作状态。

### 2.2 Helper 内部与测试接缝

新增私有 BoundMontageInstanceID，默认 INDEX_NONE。内部绑定身份检查集中在一个私有方法：生产分支调用静态接口，WITH_DEV_AUTOMATION_TESTS 分支允许既有 bTestBypassMontageActiveCheck 放行。

- bypass 仅跳过 Montage 实例检查，不跳过对象有效性、Ability/Avatar、Tag、Notify、倍率等既有前置；不扩大消费者原有 bypass 条件。
- SetTestActiveContext 保持原参数列表；实现移至 .cpp，有真实当前实例时捕获实际 ID，否则保存 INDEX_NONE。不自动开启 bypass。
- TestApplyBegin/TestApplyEnd 继续作为纯窗口算法入口，不将其结果描述成实例授权或真实播放证据。
- 补充接缝验证：无真实实例时，开启 bypass 可继续既有窗口测试；关闭后 Begin/End 不得改变集合或速率，Clear 仍清空状态。
- 真实 Melee 与双实例回归显式断言 Ability/helper 两层 bypass 均关闭。

### 2.3 绑定、事件和清理时序

BindAndCapture 保持原调用位置，顺序固定：
1. RestoreAndClear 仅在旧绑定仍获授权时恢复旧 baseline。
2. 无论是否恢复，都清空旧窗口、弱引用、baseline、捕获标记和绑定 ID。
3. 校验新输入，查询当前映射实例并捕获真实 ID。
4. 通过授权后捕获 baseline 并标记绑定成功。测试 bypass 且无实例时允许原有模拟绑定，但 ID 仍为 INDEX_NONE。

Begin/End 在修改窗口集合或速率前校验绑定身份；Restore 使用同一授权。身份失效不得写当前实例，Clear 始终幂等清空自身状态。恢复路径不新增 Ability 必须 Active 的要求。

保留 Event.Action.RateWindow.Begin/End 精确匹配、Avatar、SourceAnimation、Notify 身份、有限正倍率、重复 Begin 幂等和 Last-Active-Wins。现有 baseline 有限正值校验与回退政策不变。保持 ReadyForActivation 返回点的同步重入防护及单一清理出口，不移动 Task、委托或 EndAbility 时序。

### 2.4 十二个消费者裁决

| 消费者 | 本片处理 |
|---|---|
| Player Light | 替换 RateWindow Begin/End/Clear 的实例判断，保留跨 entry 的 Context/Token/快照 |
| Charged、Sprint、MeleeSkill、Bow、Dodge | 五个 .h 与 HasOwnedRateWindowMontageInstance() const 声明不变；只将 .cpp 中包装函数改为共享接口转发，保留暂停/Section/重触发/结束条件差异 |
| Enemy Melee、Big Hit、Small Hit、Launch、Victim | 替换 RateWindow Begin/End/Clear 内按旧 ID 存活判断的重复实现，保留各自准入和失效处理 |
| StanceBreak | 通过现有 helper 获得绑定 ID 保护，无需修改 Ability 源码；独立验证 Poise/移动恢复及 VictimLocked 交接 |

Victim 按 ID 停止所属实例的逻辑不改。本片只验收 RateWindow 调速授权，不扩展为所有 Montage 操作的统一授权框架。

## 3. Approved Paths 与分步门禁

以下目录与精确文件枚举组成封闭白名单，不允许整目录修改。Gemini 无文档写入权；开发测试薄入口不构成第一阶段生产行为修改授权。

| 绝对目录 | 允许文件与符号范围 |
|---|---|
| E:\GameDevelop\PolyQuest\Source\PolyQuest\Public\AbilitySystem\Abilities | MontageRateWindowLifecycle.h：接口、私有绑定状态、测试接缝声明；EnemyMeleeAbility.h：仅新增开发测试宏内调用 ClearRateWindow(true) 的薄入口 |
| E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\AbilitySystem\Abilities | MontageRateWindowLifecycle.cpp：授权实现、绑定/事件/恢复检查及测试上下文实现 |
| E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\AbilitySystem\Abilities | LightAttackAbility.cpp、ChargedAttackAbility.cpp、SprintAttackAbility.cpp、PlayerMeleeSkillAbility.cpp、BowDrawFireAbility.cpp、DodgeAbility.cpp：仅 RateWindow 实例判断及必要 include |
| E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\AbilitySystem\Abilities | EnemyMeleeAbility.cpp、EnemyHitReactionAbility.cpp、EnemySmallHitReactionAbility.cpp、EnemyLaunchReactionAbility.cpp、EnemyVictimExecutionAbility.cpp：仅 RateWindow 实例判断及必要 include |
| E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\Tests | EnemyMontageRateWindowAutomationTests.cpp、PlayerMontageRateWindowAutomationTests.cpp、EnemyStanceBreakRateWindowAutomationTests.cpp、ExecutionVictimRootMotionAutomationTests.cpp：本片实例授权、恢复及直接相关 fixture/断言 |

不修改共享 CombatAutomationFixture；文件内支撑复用现有可播放 Montage 构造方式。

### 第一阶段：真实 Enemy Melee 反例

以下为原批准实施顺序，保留追溯；本阶段没有取得修复前 RED。最终交付按第7节记录用户收尾授权及证据债，不声称完成了以下测试先行时序。

第一阶段写入仅限 EnemyMontageRateWindowAutomationTests.cpp 与 EnemyMeleeAbility.h 的上述薄入口。先完成测试和静态交付，交给用户编译/运行。未取得满足前置的修复前失败证据，不得修改生产授权代码或进入第二阶段。

- 用现有 SpawnPassiveEnemy 配置回调与测试 setter 装配有效 AttackSet、单个 AttackProfile、DamageGE、AIProfile、Poise 恢复配置和可播放 Montage；配置先于 Controller Possess，避免缓存无效配置。
- 安装有效 AnimInstance，确认 ASC ActorInfo 指向它；设置合法目标，通过 PreparePendingAttackProfile、TryRequestMeleeAttack 真实激活。
- 明确断言数据有效、目标在 EngagementRange/AttackRange 内、pending 成功、Ability/Context/helper/绑定 ID 有效；不得手设 Ability Active 或 Controller 有效性标志替代路径。
- 使用无 Root Motion 的临时可播放 Montage，旧实例进入窗口 A；以速率 2.0、bStopAllMontages=false 重播同资产。
- 明确断言旧实例仍存在且未停止，新当前 ID 不同，旧 Ability/Context 仍有效，两层 bypass 均关闭。
- 经旧 Context 发送窗口 B Begin、窗口 A End，再调用真实 Clear；分别断言新实例速率仍为 2.0。每个负例使用独立或重新建立的前置，避免串扰。
- 修复前应暴露误写。前置未成立只能报告 fixture 失败，不得弱化断言、扩大 bypass 或将旧实例已停止测试当作替代。
- 直接驱动 Notify/Context 回调属于真实实例上的事件消费证明，不声称为时间轴自然调度或生产资产触发证明。

### 第二阶段：共享修复与回归

用户回传第一阶段有效失败收据后，按第2节实现共享授权、迁移消费者并补齐回归。常规实现细节在白名单内自主推进，不反复请示；第一阶段门禁不是可跳过的局部实现细节。

## 4. 验证矩阵

| 门禁 | 内容与成功标准 | 当前状态 / 证据所有者 |
|---|---|---|
| 修复前反例 | 第一阶段前置全部成立，Begin/End/Clear 暴露旧代码误写；编译错误或激活失败不是 bug 复现 | 未运行；非阻塞证据债 Debt-03H6-FIX1-PreFixProof，最终验收依据与授权见第7节 |
| 共享授权 | 同实例成功；空/失效对象、无效 ID、已停止、同资产替换拒绝；暂停保留授权；Clear 幂等；重绑定不覆盖新实例；bypass 关闭后无实例拒绝 | 对应 Automation 通过；用户回传 Gemini 执行收据，测试局限见第6节 |
| Player | 六类专项；Light 换 entry、Charged 暂停/释放、Bow Section、Dodge 重触发、旧 Context、取消及现有同步重入 | 六类 PlayerMontageRateWindow 用例通过；用户回传 |
| Enemy | 五类消费者正常窗口与失效身份；Small Hit 重触发；Victim 非致死恢复及所属实例停止边界 | EnemyMontageRateWindow、ExecutionVictimRootMotion/Presentation 通过；用户回传 |
| StanceBreak | 正常恢复、同资产替换后拒绝旧调速、取消与 VictimLocked 下 Poise/移动恢复归属 | EnemyStanceBreakRateWindow 通过；真实实例测试与 synthetic 恢复测试已分开 |
| 静态 | 白名单 diff、git diff --check、适用且可用的 Rider 诊断；12 个消费者对照证明授权规则仅一份生产实现 | Main 源码/diff 复核及 git diff --check 通过；本轮未新增 Rider 诊断证据 |
| 编译 | UE 5.8 PolyQuestEditor / Development Editor 编译通过 | 用户回传 Gemini Build.bat 收据：Result: Succeeded，Exit Code 0；Main 未重跑 |
| PIE | Scene01 适用 Player 动作及 Enemy 攻击/反应/处决恢复，无卡速、残留动作锁或异常恢复 | 用户在本轮清单之后明确确认测试通过；属于用户 PIE 回执，Main 未操作 Editor，不作为双实例 RED 或资产自然触发证据 |
| Fresh Review | Main 按 ue-strict-review 在批准 diff 内终审，无 P0-P2 blocker | 2026-09-13 定向 delta review 通过；历史门禁记录不等同代码 Finding |

Focused Automation：
- PolyQuest.Combat.PlayerMontageRateWindow
- PolyQuest.Combat.EnemyMontageRateWindow
- PolyQuest.Combat.EnemyStanceBreakRateWindow
- PolyQuest.Combat.ExecutionVictimRootMotion
- PolyQuest.Combat.ExecutionVictimPresentation
- PolyQuest.Player.ActionWindows

本片无资产写入、迁移或制作清单。用户记录 PIE 实际采用的角色、Ability/Montage 与结果；普通 PIE 不替代双实例反例，也不证明生产资产已触发该缺陷。未授权自动运行编译、Automation 或 live Editor；如用户后续明确委托，按当时授权执行并记录证据归属。查询 live Editor 前应用 unreal-mcp。

规划证据为静态源码与本机 UE 5.8 API；CodeGraph 已用于目标导航，未展示的宏内测试/长函数区段采用聚焦源码 coverage fallback；图谱不是运行证据。

## 5. Executor 交付、停止条件与文档收口

### Gemini 交付

- 首先读取本计划与 AGENTS.md，核对真实 HEAD/Source diff；只读取目标、直接依赖和相关测试，不重做 FULL-AUDIT。
- 第一阶段交付：两文件精确 diff、测试前置、预期失败断言、静态检查及用户编译/运行步骤；等待真实失败收据。
- 第二阶段交付：精确变更清单、修复前后收据、12 个消费者迁移/无需修改裁决、共享规则唯一性对照、严格实施自审与未完成验证门禁。
- 显式核验回调对象/状态有效性、ReadyForActivation 重入边界和单一清理出口。
- 首次交付按项目规则优先使用一个干净只读子代理严格自审；同一切片后续修复回路单兵复核，不重复派发子代理。不冒充 Main Fresh Review。
- 不改文档、白名单外文件、资产、Tag、Config、Build.cs 或全局工具；不暂存、提交、推送，不回滚 WIP。

### 停止条件

清单外需求、动作契约变化、反例前置无法成立或基线存在冲突时，停止相关扩展并报告具体证据。相同根因最多一次有依据修复及一次定向复跑，不以削弱断言或扩大 bypass 换绿灯。

### Main 文档范围与关闭条件

- E:\GameDevelop\PolyQuest\ROADMAP-archive.md：只追加 FULL-AUDIT → FIX1 交接与本片最终收口，固定旧计划证据为 git show 851c7ac:plan.md。
- E:\GameDevelop\PolyQuest\plan.md：当前交接、门禁和最终结果；由 Main 维护。
- E:\GameDevelop\PolyQuest\ROADMAP.md：活跃指针、旧审计引用、H6-F01 唯一风险与关闭状态；其余 Findings/债务保持归属。
- E:\GameDevelop\PolyQuest\ARCHITECTURE.md：仅验证与终审通过后补充稳定实例授权契约，本次规划落盘不改。

原计划关闭条件为：真实反例成立且修复后通过、共享生产授权唯一、专项与用户编译/PIE通过、Main Fresh Review 无 P0-P2 blocker。修复前 RED 未执行的偏差及本轮用户收尾授权见第7节，唯一证据债保留在 ROADMAP，不静默改写历史。FIX2、有限 SHRINK 与其他独立风险继续开放，03C 尚不放行。

用户已在2026-09-13明确批准文档收尾后提交；仅暂存本片18份Source文件和4份文档，核验cached diff，保留所有无关WIP，不推送。

## 6. Main 定向复核记录（2026-09-13）

- 上轮两项 P2 已关闭：Victim CDO 改为合法属性反射读取，RAII 保存/恢复原配置且仅覆盖同步激活；Helper End 负例先建立窗口 A，双实例替换后断言新速率和旧集合均不变；StanceBreak 新增真实播放、完整 Notify 身份及两层 bypass 关闭的 Begin/End 回归，同时保留普通恢复与 VictimLocked 权属交接验证。
- 实例测试直接派发 GameplayEvent，是事件消费/实例授权证据，不是生产资产 Notify 时间轴自然触发证据。垃圾对象测试返回拒绝，但新建垃圾对象本身缺少当前映射，因此不宣称这些负例可以独立捕获删除 IsValid 的变异；对象有效性防护另有生产源码静态核验。本项是证据局限，不新增运行时缺陷或扩展测试框架。
- 本轮读三份测试的定向增量及直接依赖，结合前轮生产审查，未发现新 P0–P2；未重新审计 FULL-AUDIT 或其他子系统。图谱仅作有界导航，反射/运行时前置以源码与用户收据为准；无新的跨文件调用疑点，跳过额外 CodeGraph 查询。
- 用户回传六组 Focused Automation 的 11 个 Success：Player 的 Bow/Charged/Dodge/Light/MeleeSkill/Sprint，以及 EnemyMontageRateWindow、EnemyStanceBreakRateWindow、ExecutionVictimRootMotion、ExecutionVictimPresentation、ActionWindows。执行者为 Gemini，用户转交；Main 未运行编译或测试。全量 Combat 结果未确认，也不是新增门禁。
- 当前阶段 Source 差异共 18 文件：12 个生产 cpp、2 个头文件、4 个测试 cpp；均在原白名单内。本轮 Main 只更新 plan.md、ROADMAP.md，未暂存或提交，也未改动既有资产/Config/tmp WIP。
- 当时剩余门禁为修复前真实 Melee 失败收据；随后用户确认未实际运行。最终不再使用“待找到旧收据”的表述，改按第7节记录未执行事实与本轮收尾裁决。

## 7. 最终文档收尾与提交授权（2026-09-13）

- 用户在已获知“普通PIE不能补证修复前RED”的说明后，按提供的Scene01清单验证并回复“测试通过，可以开始文档收尾然后提交”。据此完成实现验收和提交；这是当前交付的收尾授权，不是原测试先行门禁曾执行的证明。
- 当前验收依据：共享实例授权及消费者源码复核；Gemini执行、用户回传的Development Editor编译成功与11/11专项成功；用户本轮PIE成功；Main定向复核无P0–P2。Main没有重跑编译、Automation、Editor/PIE或扩大审计。
- 原计划RED未运行，唯一非阻塞证据债为ROADMAP中的Debt-03H6-FIX1-PreFixProof。当前真实实例Automation只证明修复后的身份隔离；未来需要补证明时必须隔离旧生产实现与当前完整测试，不回滚本工作树、不伪造历史。
- ARCHITECTURE补充统一实例授权与绑定清理契约；ROADMAP保留下一阶段FIX2、有限SHRINK和既有独立债；ROADMAP-archive只追加本片结果；本plan保留为本片凭据，不直接改成下一片计划。
- 提交包含12个生产cpp、2个头文件、4个测试cpp，以及plan.md、ROADMAP.md、ROADMAP-archive.md、ARCHITECTURE.md。既有Content/Config/tmp WIP排除；最终提交哈希以Git记录为准，不将本提交自身哈希写入自身内容。
