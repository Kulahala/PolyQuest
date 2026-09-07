# TODO-07B10: Charged Attack Niagara Feedback v1 实施计划

## 1. 阶段定位、基线与路由

- **Target Objective**：为 `UChargedAttackAbility` 增加一个 Ability-owned 的附着 Niagara 蓄力反馈。确认蓄力期间为 Gather，相同有效持有时长达到 `MaximumChargeDuration` 后为 Full；VFX 只消费生命周期与相位，不参与任何玩法判定。
- **基线**：`main @ 4549a5462c5981ac1b0360b70d8d89fea7d6a6bd`（TODO-02B4 已提交）。
- **工作树**：非 clean。全部 `Content/**`、`Config/**`、Blueprint、AnimBP、Montage、地图、插件及其他本地 WIP 均为用户所有，必须保留并排除。当前 `ROADMAP.md` 也有用户未提交变动；本阶段不修改它。
- **Archive preflight**：PASS。当前 `plan.md` 对应的 TODO-02B4 有唯一的 2026-09-07 `ROADMAP-archive.md` 收口条目、基线、范围与验证记录。该归档里的旧 TODO-03C 后续指针是历史漂移；实时 `ROADMAP.md` 的 TODO-07B10 是当前权威。不重复归档或改写 TODO-02B4 历史记录。
- **技能路由**：
  - Outer: `ue-stage-workflow`
  - Primary: `ue5-cpp-gameplay`
  - Support: `unreal-niagara`, `ue5-debug-validation`
  - Route reason: 该切片在现有 GAS Ability 生命周期内增加局部 Niagara 组件、挂载解析与确定性 Automation，不改变战斗权属或资产写入边界。
- **执行路线**：`manual/out-of-band Gemini`。Codex/Main 保留架构、范围、验证解释、Fresh Review、文档、暂存和提交所有权；Codex 不创建 in-app 子代理。Gemini 只实施本计划批准的 Source/Test 路径，不得提交。

## 2. 冻结运行时契约与刻意非目标

### 冻结运行时契约

1. **唯一时钟与相位**
   - `MaximumChargeDuration` 是 Full VFX 与现有 `BeginRelease()` Damage/Poise 插值的共同唯一权威。
   - 在 Montage 已确认活动且 `SetCharging(true)` 后，读取同一玩家输入时钟 `GetCombatInputHeldDuration(Input.PrimaryAttack)`；`HeldDuration` 包含 Primary-to-Charged handoff 之前的按住时间。
   - 计算 `RemainingToFull = max(0, MaximumChargeDuration - HeldDurationAtActivation)`。当 `RemainingToFull <= KINDA_SMALL_NUMBER` 时直接以 Full 启动，完全不创建 `UAbilityTask_WaitDelay`；否则只创建一个 Ability-owned Delay。
   - 固定 Niagara 参数 `User.ChargePhase`：`0.0f = Gather`，`1.0f = Full`。System 负责视觉过渡；C++ 只写相位和生命周期。
   - `Event.Attack.Charged.HoldReady` 继续只是动画姿态暂停，既有 Montage/Notify 不移动、不改作 VFX 时钟。

2. **生成、重入与清理**
   - 使用 `UNiagaraFunctionLibrary::SpawnSystemAttached`，显式传入 `bAutoDestroy = true`、`bAutoActivate = false`；创建后先写 `User.ChargePhase`，再 `Activate(true)`，避免第一帧使用资产默认参数。
   - 直接 Release Handoff 不会调用 `SetCharging(true)`，因此不创建 Charge VFX。
   - Delay 指针必须在 `ReadyForActivation()` 前保存；该调用返回后若 `EndAbility()` 已同步发生，禁止恢复旧指针、旧组件或旧状态。
   - 私有幂等 `CleanupChargeFeedback()` 在 `BeginRelease()` 和唯一的 `EndAbility()` 清理出口复用：先对 Delay `OnFinish.RemoveAll(this)`，再 `EndTask()`、置空；随后对有效组件调用 `Deactivate()` 并清空引用。禁止按 `bWasCancelled` 分流到 `DeactivateImmediate()` 或 `DestroyComponent()`。
   - Full 回调入口必须检查 `bEndAbilityRequested`、`bReleaseStarted`、`bChargingStateApplied` 与当前 VFX 有效/活跃性。Release、取消、Dodge/Hit 中断、死亡、销毁、Montage 结束、激活失败及旧回调均不得留下或复活 VFX。
   - 缺 Niagara 资产、`SpawnSystemAttached` 返回空、非法/非有限时长、无效组件、无效显式源或 Socket 时，VFX 单独 fail-closed 并诊断；不得中止或改变 Charged 的伤害、Poise、Cost、Tag、Trace、Montage 或 GAS 生命周期。

3. **挂载契约**
   - `ChargeVFXTraceSourceName` 是 Ability-side `EditDefaultsOnly, BlueprintReadOnly` 字段，CDO 默认严格为 `NAME_None`。
   - 对 `bUseOwnerMeshSocketForTrace` 的主手近战武器：空 override 才通过 `DefaultOwnerMeshTraceSourceName` 解析；显式 override 必须精确命中 `OwnerMeshTraceSources`，并返回该条目的 `OwnerMeshSocketName`。显式 `Weapon_L` 无法解析时绝不暗中回退 `Weapon_R`。
   - Display-mesh 武器返回有效 `MainHandDisplayComponent` 与 `NAME_None`，让 Niagara 直接跟随已由 `AttachSocketName` 建立的显示挂载链；不把它强制转换成 OwnerMesh trace profile。
   - `UWeaponEquipmentComponent` 负责封装上述只读解析，向 Ability 暴露非 Blueprint 的 C++ 挂载 parent/socket 查询；不泄露可写显示组件状态。
   - 当前空手作者化预期保持为默认 `Weapon_R`，由 Charged GA 资产显式覆盖 `Weapon_L`。不从 `UAnimNotifyState_AttackTraceWindow::TraceSourceNames` 获取该 VFX 源。

### Deliberate Non-goals

- 不改 `MaximumChargeDuration` 的 Damage/Poise 算法、`HeldDuration` 输入权属、任何 Gameplay Tag、Input、TraceWindow、伤害路径、Motion Warp、Montage 或 Cost 行为。
- 不增加 Timed Niagara Montage Notify、固定 VFX 时长、第二个 GAS Tag、泛化 VFX Subsystem、VFX-to-gameplay 回调、网络复制、Config 或 Build.cs 改动。
- 不修改 `.uasset`、`.umap`、Niagara、GA、Montage、DataAsset、Blueprint、地图或导入资产；用户只读/受控 Editor 操作另列为验证门禁。
- 不把 Headless/NullRHI 自动化结果包装成真实 Niagara 渲染、Editor readback 或 PIE 视觉证据。

## 3. 批准路径、接口与所有权

### Gemini 实施白名单

- `Source/PolyQuest/Public/AbilitySystem/Abilities/ChargedAttackAbility.h`
- `Source/PolyQuest/Private/AbilitySystem/Abilities/ChargedAttackAbility.cpp`
- `Source/PolyQuest/Public/Combat/Equipment/WeaponEquipmentComponent.h`
- `Source/PolyQuest/Private/Combat/Equipment/WeaponEquipmentComponent.cpp`
- `Source/PolyQuest/Private/Tests/ChargedAttackNiagaraFeedbackAutomationTests.cpp`（新增）

### 最小接口变更

- `UChargedAttackAbility`：增加 `ChargeVFXSystem`、`ChargeVFXTraceSourceName`、私有 Niagara/Delay 运行时状态和私有启动、切相位、清理 helper。
- `UWeaponEquipmentComponent`：新增只读非 Blueprint C++ 查询，概念签名为 `TryResolveMainHandChargeVFXAttachment(FName RequestedOwnerMeshTraceSourceName, USceneComponent*& OutAttachParent, FName& OutAttachSocketName)`；只返回当前已验证装备的 attachment parent/socket。
- `WITH_DEV_AUTOMATION_TESTS` 下可加入最小 test-tracking seam，沿用 `ProjectileFlightTrail` 的模式记录 System、attachment、phase、Delay、cleanup 与强制 Spawn-null；不得新增 Shipping 查询 API 或通用 VFX 抽象。

### Main 与用户所有权

- Gemini 首轮交付前应完成一次干净、只读的隔离实施自审；若执行环境不支持，必须如实说明，不能以扩大范围替代。
- 用户拥有一个 Niagara System、现有 Charged GA 的赋值/`Weapon_L` override、Montage/TraceWindow readback 和 Scene01 PIE；不手改二进制资产。
- Main 仅在用户验证与 Fresh Review 完成后，更新 `ARCHITECTURE.md`、`ROADMAP.md`、`ROADMAP-archive.md` 和本计划的收口记录；实施阶段本身不包含文档收口或 Git Commit。

## 4. Focused Automation 与验证矩阵

### `PolyQuest.Combat.ChargedAttackNiagaraFeedback`

1. `StartsGatherOnlyAfterConfirmedCharging`：Montage 确认与 `SetCharging(true)` 后仅启动一次 Gather；Release Handoff 不创建。
2. `UsesHeldDurationAndBypassesFullDelay`：预先持有时长计入剩余 Delay；已满蓄直接 Full 且没有 Delay；非满蓄通过 `World->Tick(ELevelTick::LEVELTICK_All, DeltaSeconds)` 到阈值后才 Full。
3. `CleanupPreventsLateFullPhase`：Release、Input Cancel、EndAbility 和 actor destruction 清理 tracking state；清理后的旧 Delay 回调不得再次 Full 或重启。
4. `ResolvesOwnerMeshSourcesFailClosed`：`NAME_None` 使用默认 `Weapon_R`，显式 `Weapon_L` 生效，非法显式源失败且不回退。
5. `ResolvesDisplayMeshRootAttachment`：Display-mesh 路径返回 `MainHandDisplayComponent` 与 `NAME_None`。
6. `NullSystemAndSpawnFailurePreserveGameplay`：系统为空或 test seam 强制 Spawn-null 时，VFX 无操作，而 Charged 的释放、Cost、Poise、Tag 与清理仍可正常推进。

测试以受控 `UWorld` 和开发期 tracking seam 做确定性生命周期断言，覆盖 NullRHI/Headless；真实组件参数与渲染表现留给用户 Editor/PIE 门禁。

### 证据门禁

| 门禁 | 所有者 | 需要的证据 |
| --- | --- | --- |
| 静态回查 | Gemini | 只限批准路径的最终 diff、Rider 错误级检查（可用时）、`git diff --check`、隔离实施自审；不表述为编译或运行时验证。 |
| 手动编译 | 用户 | `PolyQuestEditor (Development Editor)` 实际结果。 |
| Automation | 用户 | `PolyQuest.Combat.ChargedAttackNiagaraFeedback` 全部 Success，及受影响的既有 Charged 输入/Trace/Motion Warp 聚焦回归。 |
| Editor readback | 用户 | Charged GA 的 `MaximumChargeDuration`、`ChargeVFXSystem`、`ChargeVFXTraceSourceName`；Niagara `User.ChargePhase`、Bounds、Gather/Full、Deactivate/auto-destroy；空手 `Weapon_L` 到实际 OwnerMesh Socket，及 Charged Montage TraceWindow 值。 |
| Scene01 PIE | 用户 | 短按 Gather 后释放、满蓄 Full、改动 `MaximumChargeDuration` 后阈值同步、Dodge/Hit cancel、死亡，以及命中/伤害/Trace 无回归。 |

## 5. 停止条件与 Gemini 交付要求

- 任何额外 Source 文件、Tag、Input、Config、Build.cs、`.uasset`、`.umap`、Montage/Notify 迁移、资产导入或编辑需求出现时立即停止并回报。
- 不得触碰 `ROADMAP.md`、`ARCHITECTURE.md`、`ROADMAP-archive.md`、`plan.md`、全部 `Content/**` 与 Config；不得 `git add -A`、提交、回滚或删除用户 WIP。
- 实施中若发现 `MaximumChargeDuration`、`HeldDuration`、`ChargeVFXTraceSourceName` 或 `OwnerMeshTraceSources` 无法按本计划表达，先报告真实源码/资产证据，不得自行放宽 fail-closed、改成默认右手或引入全局替代方案。
- Gemini 交付必须包含：五个批准 Source/Test 路径的 diff；静态检查结果；新增/保留 Automation 说明；对 `ReadyForActivation()` 重入、Delay 委托注销、NullRHI/Spawn-null、显式源 fail-closed、Display root attachment、BeginRelease/EndAbility 单一清理和旧回调的自审；留给用户的编译、Editor、Automation 与 PIE 门禁。
- 同一根因只允许一轮有证据的修复及一次目标重跑；根因重复或连续两轮修复失败后停止并提供首个失败证据。

## 6. 实施、验证与 Fresh Review 收口（2026-09-08）

### 实施结果

- Gemini 将实现限定在本计划五个 Source/Test 路径内：`UChargedAttackAbility` 增加 Ability-owned Niagara 生命周期与 Delay 相位切换，`UWeaponEquipmentComponent` 增加只读主手挂载解析，新增 `PolyQuest.Combat.ChargedAttackNiagaraFeedback` 专项 Automation。
- 已落实冻结契约：`MaximumChargeDuration` 与 `HeldDuration` 共同决定 Full 时刻；`ChargeVFXTraceSourceName` 的 CDO 默认是 `NAME_None`；`bAutoActivate=false` 时先写 `User.ChargePhase` 再激活；满蓄直接旁路 Delay；清理先注销 Delay 委托、再 `EndTask()`，最后统一 `Deactivate()`；显式非法 OwnerMesh 源严格 fail-closed，Display-mesh 使用根部 `NAME_None` 挂载。
- 代码、测试和资产配置没有引入新的 Tag、Input、Config、Build.cs、伤害路径、GAS 权属或二进制资产改动。

### 验证证据

- **Source/static evidence（Gemini/Main）**：批准 diff、关键生命周期/挂载路径与专项测试结构已回查；`git diff --check` 无异常。该证据不等同于编译或运行时证明。
- **User evidence**：用户确认 `PolyQuest.Combat.ChargedAttackNiagaraFeedback` Automation 成功，且 Scene01 PIE 通过；用户提供的执行报告还记录了 Niagara/Charged GA 配置与 Gather/Full 视觉表现。Main 本轮未重新编译、运行 Automation 或进入 PIE。
- **Editor/资产边界**：Niagara、Charged GA、Montage、Blueprint、地图和其他 `Content/**` 仍是用户-owned WIP；本阶段不把它们纳入 Source/doc 提交，也不宣称 clean-checkout authored baseline。

### Main Fresh Review

- Main 按 `ue-strict-review` 的两批预算完成批准范围内的缺陷优先审查，未发现可由当前 diff、具体行号和可解释场景证明的 P0/P1/P2/P3 Finding；无阻断项。
- 以下仅作为历史覆盖边界，不升级为实现缺陷或开放路线图项：`OnChargeFullDelayFinished()` 未使用 generation/token（现有清理已先移除委托并结束 Task，未证明旧 timer 能影响新激活）；清理助手使用指针存在性判断而非 `IsValid()`（未复现失效对象崩溃）；Headless/Null-RHI 下 Spawn-null 后仍可能建立 Delay（专项测试证明玩法路径保持，未证明状态污染）。

### 收口与后续

- 本计划保留为最近阶段的法定交接凭据；详细历史收据同步写入 `ROADMAP-archive.md`，稳定运行时契约同步写入 `ARCHITECTURE.md`，活动路线转写到 `ROADMAP.md`。
- 当前收到的用户确认未包含独立的 `PolyQuestEditor (Development Editor)` 编译与逐项 Editor readback 收据；因此保留 `Debt-07B10-CompileReadback` 作为非阻塞 authored-validation debt。关闭条件是可追溯的用户编译/readback 收据或 evidence-backed no-adoption；该债务不阻止 Source/Automation/PIE 收口或下一条 `TODO-03C` 路线。
- 本阶段提交只包含批准的五个 Source/Test 路径与四份收口文档；所有既有 Content/Config/Blueprint/地图/插件 WIP 均明确排除。
