# TODO-02B3: Lock-On Retention Hysteresis v1（阶段收口记录）

> 本文件保留 TODO-02B3 的完整计划与收口证据。用户已确认 15% retention 和 retention-only Cycle no-op 两个冻结点；Gemini 已按批准范围完成实现。本阶段只处理既有 Player Lock-On 的“保持”屏幕边界，不改变获取、循环、死亡交接或 Bow/Projectile 的既有所有权。

## Plan State

- **状态**：实现、用户确认的 Automation/PIE 门禁、Main 单轮 defect-first fresh review 均已完成；本记录进入文档收口与提交阶段。
- **用户冻结确认**：retention 固定为每轴 15% 的单一原生常量、不增加配置字段；retention-only 状态下 Cycle 严格 no-op，保留当前锁定，不隐式清锁或自动换目标。
- **仓库**：E:\GameDevelop\PolyQuest（UE 5.8，运行时模块 PolyQuest）。
- **阶段基线**：fdf8fa2c76b119b51cc9b7027756ac272425fb08（main，2026-08-29）。
- **现有工作区纪律**：保留全部既有 WIP：Config/Automation/Presets/1.json、大量 Content/** 修改/删除或未跟踪项，以及本阶段六个 Source/test 文件和四份文档改动。Config/Content/其他 WIP 不属于本阶段，不能回滚、清理、格式化或纳入提交。
- **最近完成阶段**：TODO-02B3 是当前收口阶段；其前序最近提交为 fdf8fa2（Automation Unity 辅助符号修复）。该历史提交不是 TODO-02B3 的运行时证明。
- **前一阶段追溯**：C3N closeout、用户既有验证和提交边界已保留在 ROADMAP.md、ARCHITECTURE.md、README.md 的已完成记录中；本文件不再把 C3N 的历史 handoff 当作当前执行指令。

## Objective And Player Problem

固定高位斜视镜头下，已锁定目标在屏幕边缘因相机轻微移动或敌人位移而越出一像素时会立即清锁，导致锁定、朝向和 Bow 的锁定优先表现生硬。TODO-02B3 只增加一个有限的屏幕空间保持缓冲：

- 主动获取和滚轮循环仍只接受严格视口内的候选；
- 已经拥有的 LockedTarget 在小范围越出视口时继续保持；
- 目标真正离开保持边界、死亡、被销毁或不再满足现有敌我/ASC/状态规则时，仍按当前生命周期清锁或死亡交接；
- 不引入新的目标源、计时宽限、预测或第二条伤害/投射物路径。

## Evidence Baseline And Current Call Chain

以下实现结论来自当前 on-disk 源码、Config、Automation 测试、基线 CodeGraph 调用链和代码审查图的只读结果；用户另确认了本阶段 Automation 与 focused Scene01 PIE 通过。静态、用户 Automation、用户 PIE 和独立编译/Editor readback 证据仍分开记录。

1. APlayerCharacter::Tick() 每帧调用 ValidateCurrentLockedTarget()；有效后才更新锁定朝向。
2. TryAcquireLockOnTarget()、HandleTargetCycleTriggered() 和 TryRetargetAfterLockedTargetDeath() 都通过 BuildLockOnCandidates()；该函数会调用 TryProjectLockOnWorldPoint()，再使用 FPlayerLockOnTargeting::IsStrictlyWithinViewport()。
3. ValidateCurrentLockedTarget() 先检查 Player ASC/Controller、Player Dead、目标 Dead，再调用 FCombatProjectileTargeting::IsValidTargetCandidate() 和 CacheCurrentLockedTargetCandidate()。除确认死亡外，当前无效原因都会清锁。
4. CacheCurrentLockedTargetCandidate() 同时投影 Player 和当前目标，计算顺时针角度/距离并更新 LastValidLockedTargetCandidate；ResolveValidLockedTarget() 直接复用这条验证路径。
5. UBowDrawFireAbility 在启用 Target Assist 的 Release 时调用 ResolveValidLockedTarget()；Bow 自己的自动候选仍由 FCombatProjectileTargeting 以当前 6% 屏幕边界处理。两者必须继续是两个窄边界，不能把 Bow helper 变成全局 Lock-On 设置。
6. PlayerLockOnAutomationTests.cpp 与 ProjectileLifecycleAutomationTests.cpp 已补齐保持外扩、真实循环输入 seam、超过 15% 清锁和 Bow 锁定快照回归；B3 使用了 retention 内的严格视口外坐标与明确越界坐标。

## Frozen Product And Runtime Contract

### 1. Acquisition、Cycle、Death Handoff

- BuildLockOnCandidates() 的投影 margin 固定为 0.0f；IsStrictlyWithinViewport() 的旧语义和边缘拒绝行为保持不变。中键获取、滚轮循环和死亡后的候选扫描都不得使用 retention margin。
- 已保持但暂时落在严格视口外的当前目标不进入循环候选。此时滚轮输入必须 no-op 并保留当前锁定，不得因为 FindCycledTargetIndex() 找不到当前目标而隐式清锁，也不得自动换成别的目标；目标回到严格视口后才恢复正常循环。严格视口内的循环顺序和方向完全不变。
- 只有当前目标确认死亡时才进行一次现有的顺时针严格候选扫描；死亡目标即使最后一帧位于 retention 区域，也不得让交接扫描采用 overscan。其他失效原因仍不自动寻找替代目标。

### 2. Retention Hysteresis

- v1 冻结为每轴 15% 的有限 viewport 扩展：-0.15 * Width < X < 1.15 * Width，-0.15 * Height < Y < 1.15 * Height。扩展边界使用严格不等式，正好落在外边界仍清锁；margin 为零时必须与旧严格判定完全一致。
- 只在已有 LockedTarget 的保持/验证路径对目标投影使用 15%；Player 的投影锚点仍使用严格 margin 0.0f。这样不会把 Player 自身或候选获取范围扩大。
- 保持判定必须继续要求：WorldPoint、投影坐标、Viewport 和 margin 为有限值；Viewport 为正；本地 Controller、CameraManager、Camera 前方点积和 ProjectWorldLocationToScreen() 有效。任何失败都 Fail-Closed。
- 目标仍必须通过现有 FCombatProjectileTargeting::IsValidTargetCandidate()：错误阵营/同队、Dead、Invulnerable、Destroyed、缺失 ASC 或其他现有资格失败都清锁。此阶段不改 Team、ASC、GameplayTag 或死亡生命周期。
- 15% 是 v1 生产调用的唯一上限和唯一原生常量；纯判定 helper 只校验显式 margin 为有限且非负，不再额外硬编码第二个 max。运行时调用点只允许使用 0.0 或 0.15；不新增 UPROPERTY、Config、DataAsset、编辑器调参字段，也不再留一个未定义的“small upper cap”。Focused PIE 只验证边界手感和没有行为回归；若以后要改数值，另开已批准阶段。
- 不增加 Timer grace period、连续丢失帧计数、目标预测、LOS 改写、相机重置、自动重选、目标排序重写或任何 projectile targeting 共用框架。

### 3. Shared Consumers And Ownership

- Tick()、TryGetLockedTargetDirection()、Lock-facing、Action-facing、Dodge-facing 与 ResolveValidLockedTarget() 必须观察同一条 retention 验证结果；不复制一套“宽松锁定”布尔值。
- Lock highlight、LastValidLockedTargetCandidate 的顺时针锚点、Bow Release 的一次性目标快照和已发射箭的生命周期所有权保持原样。Bow 发射后滚轮、清锁、死亡或后续投影变化不得改写已发射 Projectile。
- 不修改 FCombatProjectileTargeting 的 6% 规则，不修改 Bow Ability、Projectile Actor、Damage GameplayEffect、GAS/ASC、GameplayTags、Input Mapping 或 Content 资产。

## Route And Delegation

    Outer: ue-stage-workflow
    Primary: ue5-cpp-gameplay
    Support: ue5-debug-validation
    Route reason: 窄范围原生 Lock-On 屏幕空间保持校验和现有 Automation 扩展；不涉及资产、Blueprint、Input、GameplayTags、Editor 写入或新的 GAS 路径。

    Plan explorers: 0
    Implementation executors: 1 (Gemini)
    Execution route: manual/out-of-band Gemini
    Complex Executor: one scoped lifecycle-sensitive implementation
    Main parallel work: none
    Reason: Lock-On 投影、死亡交接、朝向和 Bow 读取共享同一生命周期合同；单一执行者按冻结 API 顺序修改，Main 保留合同、验证解释、fresh review、文档、暂存和提交所有权。

### Ownership

- **Contract owner: Main**：冻结 margin、边界不等式、Cycle no-op、死亡交接、消费者复用、测试接受条件和非目标。任何需要新文件、公开 API、Tag、Input、Config、Asset 或生命周期规则的情况必须停工返回 Main。
- **Implementation writer: Gemini**：只修改下列批准源码/测试路径，并完成严格实现自审；不得编辑项目文档、Config、Content、Blueprint、Editor 状态或提交。
- **User-owned gates**：用户在 VS 2022/Unreal Editor 中手动编译 PolyQuestEditor (Development Editor)、运行 Automation、做 Editor readback 和 Scene01 PIE/视觉验证。
- **Main after validation**：Main 独立执行一轮 defect-first fresh review；本阶段不执行第二轮 adversarial review，也不把 Gemini 的自审计为独立 review。通过后由 Main 收口 ROADMAP.md、ARCHITECTURE.md、README.md、本计划，按明确路径暂存并提交。

## Approved Native And Test Slice

除以下路径外不得修改任何文件。共享契约文件均标记为 Contract owner: Main；implementation writer: Gemini。

1. E:\GameDevelop\PolyQuest\Source\PolyQuest\Public\Character\Player\PlayerLockOnTargeting.h
   - 增加一个纯屏幕空间、带显式 margin ratio 的判定原语（建议名 IsWithinViewportWithMargin），保留 IsStrictlyWithinViewport() 原声明和语义。
   - 文档说明 finite/positive viewport、finite/non-negative margin、严格扩展边界和 margin=0 等价性；helper 不承担产品上限，运行时调用点只传 0.0 或 0.15；不放置 Editor/Config 可调字段。

2. E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\Character\Player\PlayerLockOnTargeting.cpp
   - 实现上述纯判定，检查 margin 与计算出的扩展量有限；不要改变排序、鼠标最近、循环或死亡 successor 算法。
   - 不复用或修改 CombatProjectileTargeting.cpp 的 Bow 6% helper。

3. E:\GameDevelop\PolyQuest\Source\PolyQuest\Public\Character\Player\PlayerCharacter.h
   - 为私有 TryProjectLockOnWorldPoint() 增加明确的 margin 参数（默认或调用点显式为 0 均可，但获取/循环调用必须肉眼可见地传 0）。
   - 在 WITH_DEV_AUTOMATION_TESTS 下增加真实循环触发所需的最小 seam（如 TriggerTestTargetCycle(float)）；Shipping/反射 API 不得新增。
   - 不改变 ResolveValidLockedTarget()、Lock-on、Bow、ASC、Input 或 GameplayTag 的生产公开契约。

4. E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\Character\Player\PlayerCharacter.cpp
   - 在本文件匿名 namespace 放置唯一的 constexpr float retention ratio 0.15f，或等价的单一原生来源；不得引入第二个 cap。
   - BuildLockOnCandidates() 的 Player/候选投影显式使用 0.0f。
   - CacheCurrentLockedTargetCandidate() 对 Player 使用 0.0f，仅对当前 Target 使用 0.15f；保留 finite、camera、controller、ASC、team、Dead/Invulnerable 检查和缓存更新顺序。
   - 保持 ValidateCurrentLockedTarget() 的死亡分支和 TryRetargetAfterLockedTargetDeath() 的一次严格扫描不变。
   - 调整 HandleTargetCycleTriggered()：严格候选仍只来自 BuildLockOnCandidates()；仅当该函数成功返回、当前 LockedTarget 通过本帧 retention 验证但不在严格候选列表时，输入才 no-op 并保留锁定，不得清锁或自动重选。`NextIndex == INDEX_NONE` 时不得只依据旧的 LastValidLockedTargetCandidate；需要使用本次验证结果或一次窄的当前资格+投影重检确认“仍在 retention、仅缺席严格候选”。BuildLockOnCandidates() 失败（Player 投影、Controller/Camera、世界等基础失败）以及其他原有失败语义继续清锁/返回，不得用“LockedTarget 仍非空”做宽泛兜底。
   - ResolveValidLockedTarget()、TryGetLockedTargetDirection()、Lock-facing/Action-facing/Dodge-facing 只通过现有验证路径获得新保持语义，不添加旁路状态。

5. E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\Tests\PlayerLockOnAutomationTests.cpp
   - 扩展纯判定：margin=0 与 strict 等价；X/Y 四边的刚内/刚外；精确扩展边界拒绝；NaN/Inf、无效 viewport、负/非有限 margin Fail-Closed。
   - 扩展真实 Player fixture：严格视口外目标不能 Acquire；严格候选外目标不能被 Cycle 选入；已锁定目标在边缘/刚越界且仍在 15% 内时保持高亮和锁定；超过 15%（X、Y）后清锁且不自动重选；相机/目标移动跨边界时结果稳定。
   - 覆盖 retention 状态下的 Cycle no-op、死亡目标一次严格顺时针交接、Dead/Destroyed/Invulnerable/错误阵营/Player Dead、缺失 Controller/Camera、投影失败和 teardown。
   - 保留现有 Lock-facing、Action-facing、Dodge、Sprint、Guard、Parry、Root Motion、Bow requester 回归；测试 hook 必须最终走与生产相同的 margin 判定，不复制第二套数学。

6. E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\Tests\ProjectileLifecycleAutomationTests.cpp
   - 修正现有 B3 “X = 0 即无效锁”断言：边缘点在新 retention 合同下应保持；无效/回退样例改为明确超过 15% 的坐标（例如 X < -0.15 * Width）。
   - 增加一个仅在 15% retention 内、但位于严格视口外且超出 Bow 6% 自动候选边界的锁定目标（例如 1920 宽视口的 X=-150；15% 外扩下界为 -288，Bow 6% 下界为约 -115.2），确认 ResolveValidLockedTarget() 和 Target Assist Release 仍优先使用该锁定快照；再用明确超过 15% 的坐标（例如 X=-300）确认清锁并回到 B2 自动候选。
   - 保留 Target Assist disabled、箭发射后不重定向、死亡交接和 Projectile 生命周期断言；不修改 Bow/Projectile 生产代码。

### Read-only reference paths (do not edit)

- E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\Combat\Projectile\CombatProjectileTargeting.cpp：只读取 Bow 的 6% 边界、Camera 前方和候选资格，不能改成共享 Lock-On helper。
- E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\AbilitySystem\Abilities\BowDrawFireAbility.cpp：只确认 ResolveValidLockedTarget() 的一次性 Release 快照调用。
- E:\GameDevelop\PolyQuest\PolyQuest.uproject、Source/PolyQuest/PolyQuest.Build.cs、Config/Tags/PolyQuestGameplayTags.ini：只做依赖/Tag 只读核对；本阶段不应需要改动。

## Execution Order And Stop Conditions

1. Gemini 先读取本计划、AGENTS.md、当前基线和六个批准路径，逐行确认 BuildLockOnCandidates 的三类调用、CacheCurrentLockedTargetCandidate 的双投影和 B3 的旧边界断言。
2. 先完成 FPlayerLockOnTargeting 纯判定，再扩展 TryProjectLockOnWorldPoint 的显式参数；不先改 IsStrictlyWithinViewport 的行为。
3. 按“获取/循环显式 0 → 保持目标 0.15 → Cycle no-op”顺序修改 Player 调用链；每一步检查死亡交接、highlight 和 ResolveValidLockedTarget 没有旁路。
4. 扩展两个既有 Automation 套件，优先写失败/边界/生命周期用例，再写 Bow retention 快照回归；不新建通用测试框架。
5. Gemini 只做静态自检，并完成两遍执行者自审：第一遍 defect-first 检查生命周期/边界/回归，第二遍按冻结合同做 adversarial 检查；随后读取最终 diff/调用链、运行 Rider lint 或 problem 检查（端点可用时）、git diff --check、批准路径审计。两遍都不能称为 Main 的独立 fresh review；不得调用 UBT、Build.bat、Visual Studio build、Editor、Automation、PIE、打包、Git stage/commit。
6. 任何需要第七个源码/测试文件、修改 Bow/Projectile/Build.cs/Config/Tag/Input/Content、改变死亡交接或增加第二个 grace/cap 的需求，立即停止并把证据交回 Main。

## Validation Matrix And Evidence Gates

### Gemini/Main static gate (not runtime proof)

- IsStrictlyWithinViewport() 的旧测试与调用行为保持；BuildLockOnCandidates() 所有获取/循环/死亡调用仍传 0。
- CacheCurrentLockedTargetCandidate() 只有当前 Target 使用固定 0.15，Player 锚点不扩大；所有失败路径仍清锁或走既有死亡交接。
- ResolveValidLockedTarget()、Lock-facing/Action-facing/Dodge-facing 和 B3 Release 读取同一验证结果；Projectile 发射后不读取锁。
- 批准路径之外无差异；git diff --check 为零；Rider/CodeGraph/CRG 输出只作为静态/影响证据。codegraph 当前索引与 fdf8fa2 一致；code-review-graph 只补充调用影响，不证明运行时。

### User-owned compile and Editor readback evidence

- 本记录没有独立的 Visual Studio `PolyQuestEditor (Development Editor)` 编译日志，也没有新增 Editor 资产读回；不从用户 Automation/PIE 结果反推独立编译或 readback 证据。
- 本阶段没有新的资产读写要求；现有 LockOn/TargetCycle 输入引用和 Bow Target Assist 资产不在批准修改路径内。

### User-owned Automation gate (confirmed)

- 用户确认 TODO-02B3 相关 Automation 通过，包含 `PolyQuest.Player.LockOn` 以及 `PolyQuest.Projectile.Lifecycle` / `PolyQuest.Projectile.TargetAssist` 的 Bow 回归。
- 该结果是用户运行证据；本轮未重新运行 Automation。

### User-owned focused Scene01 PIE gate (confirmed)

- 中键获取与滚轮循环仍只接受严格视口内目标；严格视口外候选不能主动锁入。
- 已锁目标在视口边缘、刚越出边界但在 15% 内时保持锁定、高亮、合法锁定朝向和 Bow Release 锁定优先；相机轻移和目标轻移均不瞬间脱锁。
- 目标超过任一轴的 15% 扩展边界、死亡/销毁、Invulnerable、错误阵营、Player Dead、Controller/Camera/投影失效时按合同清锁或只做一次死亡交接；不自动重选。
- 在 retention 区域滚轮输入不清锁、不偷换目标；目标回到严格区域后循环恢复。
- 用户确认 focused `Scene01` PIE 通过，覆盖上述锁定保持/清锁、Cycle no-op、Bow 锁定优先与既有朝向/生命周期回归；本轮未重新启动 Editor 或 PIE。

### Evidence labels

- 当前已确认：源码/Config 静态读取、基线 CodeGraph 调用链、code-review-graph 的影响补充、用户确认的 TODO-02B3 Automation 与 focused Scene01 PIE。
- 当前未单独提供：本阶段 Development Editor 编译日志和 Editor readback 记录；本轮未重新运行任何用户门禁。Gemini 的两遍自审不等于 Main 的独立 fresh review。

## Known Debt And Blocker Decision

- Main 的单轮 defect-first fresh review 未发现 P0/P1/P2 或需要返工的源代码阻塞；既有 Lock-On/Bow 源码边界完整，保持用例和循环触发 seam 已在本阶段补齐。
- helper 契约注释完整度、个别测试注释措辞和 Y 轴更深的集成矩阵属于非阻塞观察，本阶段不把它们升级为债务或追加实现范围。
- Content/** 与 Config/** WIP、作者化输入/Widget/Bow/Animation 资产未形成干净检出 fixture，是版本边界和验证说明，不是本阶段源码实现 blocker；继续原样保留。
- Bow 的固定 6% overscan 是已有、已验证的武器族规则；本阶段不把它改为 15%，也不把 15% 写回 Projectile 配置。
- 若 focused PIE 证明 15% 手感仍需改值，不能在本阶段由执行者临时调参；应先结束本阶段并单独确认新阶段/新 plan。

## Documentation And Commit Boundary

- 本阶段计划阶段只允许 Main 修改 plan.md 和必要的 ROADMAP.md 漂移表述；ARCHITECTURE.md 与 README.md 仅记录已通过门禁的稳定事实，本次已写入 hysteresis 的收口摘要。
- 实现与用户门禁通过、Main 单轮 fresh review 完成后，Main 已将 TODO-02B3 移到 Done Milestones，并在 Architecture/README 写入实际 margin、Cycle no-op、死亡交接和 Bow 分界的已验证摘要。
- 提交候选为六个批准源码/测试路径及 Main 明确批准的文档收尾；排除 Config/Automation/Presets/1.json、所有 Content/**、Build.cs、uproject 和其他 WIP。本次用户已明确授权提交，仍不得扩大暂存范围。

## Gemini Handoff Prompt

以下提示可在用户已确认本计划后原样交给 Gemini；它是冻结执行边界，不是新的架构授权：

> 你是 TODO-02B3 的实现执行者。工作目录必须是 E:\GameDevelop\PolyQuest，基线为 fdf8fa2c76b119b51cc9b7027756ac272425fb08；先读取 AGENTS.md 和当前 plan.md。执行路线是 manual/out-of-band Gemini，Primary Skill 为 ue5-cpp-gameplay，Support Skill 为 ue5-debug-validation。Contract owner: Main；implementation writer: Gemini。
>
> 只允许修改本计划列出的六个绝对路径。保持 IsStrictlyWithinViewport、获取、循环、死亡一次性交接、Team/ASC/Dead/Invulnerable、Lock-facing、Bow 6% 自动候选和已发射 Projectile 生命周期不变。新增的 retention 只对已有 LockedTarget 的目标投影使用固定每轴 15% 严格扩展；纯 helper 只校验有限/非负 margin，运行时调用点只能传 0.0 或 0.15；Player 投影和所有 BuildLockOnCandidates 调用使用 0。只有 BuildLockOnCandidates 成功、且当前目标本帧通过 retention 但不在严格候选列表时，Cycle 才能 no-op 保留锁定；`NextIndex == INDEX_NONE` 时不得只依据旧缓存，必须以本次目标资格+投影重检确认该条件。基础投影/Controller/Camera/世界失败仍按原语义清锁或返回，不得以 LockedTarget 非空作为宽泛兜底。不得新增第二个 cap、Timer grace、预测、LOS/相机行为、Tag/Input/Config/Asset/Build.cs 或通用 targeting 框架。
>
> 测试必须走相同生产 margin 判定，覆盖 0 等价 strict、X/Y 边界、NaN/Inf、Acquire/Cycle strict、retention 内保持、超过 15% 清锁、Cycle no-op、状态/teardown、死亡严格交接，以及 B3 Bow lock snapshot 回归。B3 至少使用一个严格视口外但仍在 retention 内、且超出 Bow 6% 自动边界的坐标（1920 宽时可用 X=-150），并用 X=-300 一类坐标覆盖超过 15% 的清锁；不得修改 Bow/Projectile 生产源码。
>
> 只做静态自审：先做一遍 defect-first，再做一遍针对冻结合同的 adversarial 自审；若 `.codegraph/` 可用，先用 CodeGraph 核对符号和调用链；`.code-review-graph/` 只能作为变更影响补充，不能当编译或运行时证明；随后读取最终 diff，运行 Rider lint/problem 检查（若可用）与 git diff --check。两遍自审都不能冒充 Main 的独立 fresh review。不得编译、运行 Editor/Automation/PIE、打包、stage、commit 或扩大范围。完成 handoff 时列出 changed paths、静态证据、两遍自审 findings、未运行的用户门禁和剩余风险；遇到未列出的文件或契约需求立即停工返回 Main。

## Dependency Order After This Stage

路线保持用户确认的唯一顺序，当前阶段已完成，后续只制定不执行：

TODO-03A3E World Pickup Interaction Prompt v1
→ TODO-03A7 Selected Melee Motion-Warp Contact Assist v1
→ TODO-05A Front Critical v1
→ TODO-05B Backstab v1
→ TODO-05C Stagger Execution And Stagger Backstab v1
→ TODO-03C Ranged Enemy v1

每一项都必须先通过自己的 adoption、compile、Automation、Editor/PIE 和 review 门禁；路线图未来项不是本阶段已实现事实。

## Closeout Record

- **Implementation evidence**：Gemini 修改了六个批准 Source/test 路径：15% 每轴 retention helper、严格 Acquire/Cycle/Death 投影、retention-only Cycle no-op、Player 验证路径复用，以及 Lock-On/Bow 生命周期回归用例；未修改 Bow/Projectile 生产代码、Config 或 Content。
- **User compile/Editor/Automation/PIE evidence**：用户确认本阶段 Automation 与 focused Scene01 PIE 通过。本记录没有独立 Development Editor 编译日志或新增 Editor readback，不作推断；本轮未重新运行这些门禁。
- **Main fresh review**：Main 已完成一轮独立 defect-first fresh review，未发现 P0/P1/P2 或需要返工的源代码问题。Gemini 的两遍执行者自审仅作为交接证据，不替代该 review；本阶段不执行第二轮 adversarial review。
- **Documentation/commit**：本次收口同步 ROADMAP.md、ARCHITECTURE.md、README.md 与本计划，并登记未来 `TODO-07B4`。提交只包含六个批准 Source/test 路径与四份文档；Config/Automation/Presets/1.json、所有 Content/**、uproject、Build.cs 和其他 WIP 排除在外。
