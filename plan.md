# TODO-02C3N: Guard Success And Player Hit Audio Feedback v1

## Plan State

- **状态**：Main 方案已冻结，等待 Gemini 只读审阅并按交接提示执行；本次只更新计划，不修改源码、资产或配置。
- **仓库**：`E:\GameDevelop\PolyQuest`（UE 5.8，运行时模块 `PolyQuest`）。
- **基线**：`b2eb4f2559bdf7206f4be4012549693358144e6e`。
- **范围纪律**：保留工作区全部既有用户 WIP，尤其是 `Config/DefaultEngine.ini` 与 `Content/**` 的修改/删除；它们不属于本阶段实现，也不能被回滚、格式化或提交。
- **阶段目标**：在已经存在的 GAS/Resolver 边界上加入两个彼此独立、可选、可静默失败的原生音效通道：成功 Guard 音效，以及玩家受到敌方非致命实际伤害时的受击音效。
- **阶段成功条件**：Guard 只在成功吸收一次接触后播放一次；Player 只在权威、非致命、Health 实际下降且来源精确属于 `Team.Enemy` 时播放一次；现有 Parry、Overlay、Camera Shake、Small/Big/Launch 反应、Guard Break、投射物伤害和清理生命周期均保持不变。

## Route And Delegation

```text
Outer: ue-stage-workflow
Primary: ue5-cpp-gameplay
Support: game-feel, ue5-debug-validation
Route reason: 这是一个窄范围的原生 GAS 防御/Health 回调扩展和直接音效呈现接入；现有 ASC、Resolver、Player Controller 与 Enemy 反馈所有权已经足够，不需要新的反馈框架。
```

```text
Plan explorers: 0
Implementation executors: 1 (Gemini)
Complex Executor: one scoped lifecycle-sensitive implementation
Main parallel work: none
Reason: Guard 接触消费与 Player Health 回调分别是两个入口，但都共享 Player/ASC、有限位置校验和测试生命周期；由一个执行者按顺序写入可避免跨文件契约竞争，Main 保留契约、文档、验证解释、暂存和提交所有权。
```

### Ownership

- **Contract owner：Main**。Main 冻结 API、敌我过滤、反馈时序、去重语义、测试接受条件和非目标；任何契约变更都必须停工返回 Main 决策。
- **Implementation writer：Gemini**。Gemini 只能修改下列七个批准路径中的指定函数/测试 seam，不得修改文档、Config、Content、资产、Resolver、Controller、Enemy 或构建设置。
- **User-owned gates**：用户负责在 Unreal Editor/VS 2022 中完成资产可选赋值、`PolyQuestEditor (Development Editor)` 编译、Automation、Scene01 PIE 的声音与行为确认。
- **Main after validation**：Main 做一次普通 defect-first fresh review，解释用户证据，更新 `ROADMAP.md`、`ARCHITECTURE.md`、`README.md` 和本计划的收尾记录，按明确路径暂存并等待用户批准提交。

## Current Evidence And Call Chain

1. 近战路径是 `FMeleeHitResolver::TryResolveHit` → `APlayerCharacter::TryResolveIncomingDefense` → 当前活动的 `UPlayerParryAbility` 或 `UPlayerGuardAbility`。近战 Resolver 已把 `FHitResult` 传入并允许 Parry。
2. 投射物路径是 `FCombatProjectileHitResolver::TryResolveHit` → 同一个 Player 防御入口；C3M 已传入 `bAllowParry = false`，因此投射物跳过 Parry 并保留 Guard/伤害路径。两个 Resolver 不在本阶段修改。
3. `UPlayerGuardAbility::TryGuardMeleeHit` 当前只接收攻击者和体力伤害，成功应用 Guard Stamina GE 后会继续处理恢复延迟或 Guard Break；这是成功 Guard 音效应插入的唯一接触消费点。
4. `APlayerCharacter::OnHealthAttributeChanged` 已负责权威、非致命、Health 下降、Overlay、Camera Shake 和反应事件。新增受击音效必须挂在同一回调的有效伤害分支中，不能用新的 Health/伤害路径。
5. `AEnemyCharacter::HandleCombatImpactFeedback` 的 `ImpactSound` 是 C3K 已验证的 Enemy 受击路径，本阶段不修改、不复用其配置字段，也不把 Player 音效移到 Enemy。
6. `ICombatTeamAgent::Execute_GetCombatTeamTag` 是当前精确敌我关系契约；Player 受击音效只接受返回值精确匹配 `Team.Enemy` 的 Instigator。

## Frozen Product Decisions

### 1. Guard success audio

- `GuardSuccessSound` 是 `UPlayerGuardAbility` 上的可选 `USoundBase`，默认 `nullptr`；未配置时只跳过音频，不影响 Guard 消费。
- 成功吸收的 Guard 接触全部适用：近战、投射物，以及该接触把 Stamina 降至零并触发 Guard Break 的情况。Guard Break 仍是一次被吸收的接触，只播放一次。
- 只有现有 Guard 前置条件通过且 `ApplyGameplayEffectSpecToSelf` 对 Guard Stamina GE 返回成功后才触发；错误攻击弧、非活动/失效 Guard、缺失 GE、Spec 构造失败或 GE 应用失败不触发。
- 触发时机固定在 GE 成功之后、任何 Guard Break/恢复延迟分支之前；不新增计时器、Gameplay Event、Tag 或重复防御状态。
- `TryGuardMeleeHit` 改为接收 `const FHitResult& HitResult`；`APlayerCharacter::TryGuardIncomingMeleeHit` 同步增加该参数，并原样转发。`TryResolveIncomingDefense` 已有 HitResult，bAllowParry 为 false 时继续调用带 HitResult 的 Guard 路径。
- 位置规则：仅当 `HitResult.GetActor() == PlayerCharacter`、`ImpactPoint` 三轴有限且不是默认近零点时使用 `ImpactPoint`；否则退回 Player 的有限 `GetActorLocation()`。World、音效资产或最终位置无效时静默跳过音效。
- 增加一个私有 `TriggerGuardSuccessFeedback(const FHitResult&)`，同步消费 HitResult，不保留指针、Context 或跨帧回调。该 helper 不能改变 `true` 返回、Stamina、Guard Break 或 `EndAbility` 清理。

### 2. Player received-hit audio

- `ReceivedHitSound` 是 `APlayerCharacter` 上的可选 `USoundBase`，默认 `nullptr`；Enemy 的 C3K `ImpactSound` 不改变。
- 触发必须同时满足：`HasAuthority()`、Actor 未销毁、Health 新值大于零、Health 实际下降、存在 `GEModData`、Instigator 实现 `UCombatTeamAgent`，且 `GetCombatTeamTag()` 精确匹配 `Team.Enemy`。死亡、治疗、直接 Base 写入、无来源、友方/无阵营、仅 Poise 变化都不播放。
- 适用于现有 Small、Big、Launch 非致命 Health 反应，不要求反应 Tag 有效，也不因 Stunned、反应能力缺失或其他 Overlay/Camera/Reaction 分支提前返回而丢失音效。现有反馈顺序与行为保持不变。
- 在 `OnHealthAttributeChanged` 中以 `EffectSpec.GetModifiedAttribute(UCharacterAttributeSet::GetHealthAttribute())` 识别同一个 GE Spec 的首次 Health Modifier；只让首次回调尝试音效。这个检查只能决定音效是否调用，**不得对整个回调提前 return**，否则会改变既有 Overlay、Camera Shake 和 Reaction 行为。
- 触发顺序固定为：保留现有 Overlay 与 ReactionTier 计算，紧随既有 `TriggerHitFeedbackCameraShake(ReactionTier)` 调用 `TriggerReceivedHitSound(EffectSpec)`，并且必须位于 `Stunned` 与 `ReactionTier == Invalid` 的后续分支之前；这样硬直或无效反应 Tag 不能吞掉声音，也不能改变原有后续分支。
- 同一 GE Spec 含多个 Health Modifier 时只播放一次；两个独立 GE Spec 即使复用同一个 `FGameplayEffectContextHandle` 也各播放一次。
- 新增私有 `TriggerReceivedHitSound(const FGameplayEffectSpec&)`。从 `EffectSpec.GetContext().GetInstigator()` 做阵营校验，从 Context HitResult 取位置；只有 `GetActor() == this` 且 ImpactPoint 有限、非近零时使用 ImpactPoint，否则回退到有限的 Player ActorLocation。音效、World 或位置无效时仅跳过音频。
- 直接使用 `UGameplayStatics::PlaySoundAtLocation`。v1 不建立 GameplayCue、音频总线、混音/ducking、随机变体、池化、复制或网络广播；项目当前是单机，声音是本地呈现通道。

### 3. Test-only seams

- 新增的反馈计数、最后一次位置、可选音频 dispatch bypass/记录和必要 setter/getter 必须完全包在 `#if WITH_DEV_AUTOMATION_TESTS` 中，不得进入 Shipping/反射 API。
- 生产代码不能为了测试添加特殊分支、全局单例、可写的运行时 Tag 或持久化状态。

## Approved Native And Test Slice

除以下路径外不得修改任何文件。每个共享契约文件均为 **Contract owner: Main；implementation writer: Gemini**。

1. `E:\GameDevelop\PolyQuest\Source\PolyQuest\Public\AbilitySystem\Abilities\PlayerGuardAbility.h`
   - 前置声明 `USoundBase`。
   - 将 `TryGuardMeleeHit` 改为 `bool TryGuardMeleeHit(AActor* AttackingActor, float GuardStaminaDamage, const FHitResult& HitResult);`。
   - 增加可选 `GuardSuccessSound` UPROPERTY（`EditDefaultsOnly`、`BlueprintReadOnly`、Guard/Feedback 分类、默认空）。
   - 声明私有 `TriggerGuardSuccessFeedback(const FHitResult&)`。
   - 在 `WITH_DEV_AUTOMATION_TESTS` 中仅加入构造瞬态 Ability 所需的测试 setter/getter、反馈计数和最后位置读取；不改 Ability Tags、Activation/Cancel 矩阵、Montage、GE 生命周期或 Blueprint 行为。

2. `E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\AbilitySystem\Abilities\PlayerGuardAbility.cpp`
   - 在 `TryGuardMeleeHit` 中保留既有窗口、攻击弧、ASC/GE 校验、SetByCaller 体力扣除、恢复延迟、Guard Break 和返回值。
   - 仅在 Guard Stamina GE `WasSuccessfullyApplied()` 后调用一次 `TriggerGuardSuccessFeedback(HitResult)`，并确保 Guard Break 分支仍继续执行。
   - Helper 使用 `UGameplayStatics::PlaySoundAtLocation` 和有限位置回退；所有呈现失败都静默，不阻断消费或清理。
   - 只补所需直接 include（例如 `Kismet/GameplayStatics.h`、`Sound/SoundBase.h`），清理未使用 include；不触碰其他 Ability 生命周期。

3. `E:\GameDevelop\PolyQuest\Source\PolyQuest\Public\Character\Player\PlayerCharacter.h`
   - 前置声明 `USoundBase`（若已有则复用）。
   - 增加可选 `ReceivedHitSound` UPROPERTY，默认空，放在现有 `Combat|Feedback` 资产配置边界。
   - 将 `TryGuardIncomingMeleeHit` 改为携带 `const FHitResult&` 并保持 `TryResolveIncomingDefense` 的既有 HitResult/bAllowParry 契约。
   - 声明私有 `TriggerReceivedHitSound(const FGameplayEffectSpec&)`；只添加宏保护的测试 setter/getter/计数/位置 seam。
   - 不改变公开的 Parry Camera wrapper、输入、ASC、Tag、Equipment、Lock-on 或 Camera API。

4. `E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\Character\Player\PlayerCharacter.cpp`
   - 更新 `TryGuardIncomingMeleeHit` 对 Guard 的转发，确保 `HitResult` 一路不丢失；`bAllowParry=false` 仍直接进入 Guard。
   - 在现有 `OnHealthAttributeChanged` 的有效非致命下降分支中加入首次 Health Modifier 判断和 `TriggerReceivedHitSound` 调用；调用应紧随既有 `TriggerHitFeedbackCameraShake` 且位于 Stunned/Invalid 后续检查之前，不得用全局 early return 去重。
   - Helper 按 `AEnemyCharacter::HandleCombatImpactFeedback` 的对称模式，对 Instigator 做 `UCombatTeamAgent` 接口检查和精确 `Team.Enemy` 匹配，再做 HitResult/ActorLocation 有限性与音效调用。
   - 保留现有 Overlay、Camera Shake、反应分类、Stunned 处理、Invalid Tag 日志和事件分发的顺序与语义。
   - 直接 include `Combat/Melee/CombatTeamAgent.h`、`Sound/SoundBase.h` 等实际使用依赖；不修改 Enemy 反馈。

5. `E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\Tests\TestGuardStaminaCostGE.h`
   - 新建仅供 Automation 使用的 `UCLASS() UTestGuardStaminaCostGE : public UGameplayEffect` 声明，保持与现有测试 GE 风格一致。

6. `E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\Tests\TestGuardStaminaCostGE.cpp`
   - 构造 Instant、Additive 的 Stamina Modifier，使用 `Data.Stamina.GuardDamage` SetByCaller Tag，供真实 Guard 应用路径测试。
   - 不修改全局 CDO、Content GE 或生产配置。

7. `E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\Tests\PlayerDefenseAudioAutomationTests.cpp`
   - 新建 `PolyQuest.Combat.DefenseAudio` 原生 Automation 套件，利用 `FCombatAutomationFixture::SpawnPlayer`、瞬态 ASC/Ability/GE/USoundBase 和现有 World cleanup，不依赖 `.uasset`。
   - 覆盖至少以下矩阵：
     - 近战 Guard 成功、投射物 Guard 成功、Guard Break 仍吸收且只发一次；
     - 错误攻击弧、非活动 Guard、缺失/失败 GE、空音效的静默与原有返回值；
     - 合法 HitResult 使用 ImpactPoint；构造属于 Player 的有效 HitResult 时显式设置 `HitObjectHandle = FActorInstanceHandle(Player)`，缺失 HitResult、错误 Actor、零点、NaN、Inf 使用 ActorLocation 回退；无效 World/ActorLocation 安全跳过；
     - Player Small/Big/Launch 敌方非致命 Health 伤害发声；有效/缺失/错误 Actor 的 Context HitResult 位置回退；
     - Friendly、无团队、治疗、直接 Base 写入、Poise-only、致命伤害、已死亡状态均不发声；
     - 同一 GE Spec 多 Health Modifier 只发一次，两个独立 GE（即便复用 Context）各发一次；
     - Early-return、对象销毁/EndPlay 后无悬空调用、无状态污染，且现有 Overlay/Camera/Reaction 仍未被新去重逻辑吞掉。
   - 测试应读取宏保护的计数和位置，不用日志数量代替行为断言；每个用例独立清理 World、ASC、Ability 和临时资产。

## Execution Order

1. Gemini 先读取本计划、`AGENTS.md`、当前基线和七个批准路径，确认 Resolver 已有 HitResult 传递，不修改 Resolver。
2. 先完成两个公共头文件的最小契约更新，再实现 Guard 成功反馈；保持现有 Guard Break 分支可达。
3. 实现 Player 受击音效 helper 与 `OnHealthAttributeChanged` 的仅音效去重；将调用放在既有 `TriggerHitFeedbackCameraShake` 之后、Stunned/Invalid 分支之前，逐行核对现有 Overlay/Camera/Reaction 路径未被提前返回改变。
4. 新建测试 GE 和 `PolyQuest.Combat.DefenseAudio`，先覆盖失败/回退矩阵，再覆盖多 Modifier/独立 Spec 和生命周期。
5. 仅做静态自检：读取最终 diff、Rider `get_file_problems`/`lint_files`（若端点可用）和 `git diff --check`；不得调用 UBT、VS、Editor、PIE、打包或提交。
6. 按下方交接格式返回 changed paths、静态证据、未运行的用户门禁、严格自审发现和剩余风险；任何需要第八个文件、Tag、Config、Asset 或新生命周期规则的情况立即停工返回 Main。

## Validation Matrix And Gates

### Executor static gate

- 七个批准路径的 include、类型、UHT 形状和调用链自洽。
- `Rider get_file_problems` / `lint_files`：新增错误为零；既有 warning 与新增 warning 分开记录。
- `git diff --check`：零空白错误。
- 静态检查不是编译、Editor、Automation 或 PIE 证据。

### User-owned compile and Editor gate

- 在 Visual Studio 2022 编译 `PolyQuestEditor (Development Editor)`。
- 在 Unreal Editor 运行 `PolyQuest.Combat.DefenseAudio`，并回归至少 `PolyQuest.Combat.HitFeedback`、`PolyQuest.Combat.ParrySuccessFeedback`、`PolyQuest.Combat.HitReaction`、`PolyQuest.Projectile.Lifecycle` 及现有 Guard/Equipment 套件。
- 若要听到实际声音，用户在当前 Parry/Guard/Player authored Ability 或 BP/CDO 中自行赋值 `GuardSuccessSound`、`ReceivedHitSound` 并保存资产；资产变更保持本地 WIP，不进入本阶段源代码提交。

### User-owned PIE gate

- 在 `Scene01` 近战 Guard 成功时听到一次 Guard 音效；投射物 Guard 与 Guard Break 吸收也不重复。
- 角色受到敌方 Small/Big/Launch 非致命伤害时听到一次 Player 受击音效；友方、治疗、致命和仅 Poise 变化不误触发。
- 确认原有 Parry 音效、Hit-stop、Camera Shake、Overlay、受击 Montage、Guard Break、投射物伤害和结束/打断清理没有回归。

## Non-goals And Stop Conditions

- 不新增 GameplayCue、Gameplay Tag、Input、Damage/Health 路径、ASC、Controller API、网络复制、音频总线、ducking、随机变体、池化或通用 Feedback Dispatcher。
- 不修改 `MeleeHitResolver.cpp`、`CombatProjectileHitResolver.cpp`、`AEnemyCharacter`、`APolyQuestPlayerController`、Parry C3M 路径、GameplayEffect/Ability/Montage/AnimBP/Blueprint/DataAsset/Map 资产或 `PolyQuest.Build.cs`。
- 不把 Guard 音效误绑定到 Guard Ability 激活/循环 Montage；它只属于一次成功接触。
- 不把 Player 受击音效放到客户端猜测、Overlay、Camera Shake、Reaction Tag 或 Enemy 反馈回调之外的第二个入口。
- 如果音效 API 在当前模块需要未批准的 Build.cs 依赖、若 HitResult 传递实际缺失、若去重需要修改共享 AttributeSet/Resolver、若必须编辑资产或新增 Tag，停止实现并把证据交回 Main；不得绕过白名单。
- `USoundBase` 为空、World/Actor 无效、位置非有限或播放失败均是呈现层 no-op，不得让 Gameplay 结果失败。

## Documentation And Commit Boundary

- 本阶段实现验证通过后，Main 才能将 `TODO-02C3N` 移到 `ROADMAP.md` 的 Done Milestones，并记录音效边界、去重语义、用户验证证据和仍保留的本地资产 WIP。
- `ARCHITECTURE.md` 只记录已验证的 Guard/Player 音频所有权和调用链，不写可变调音 TODO；`README.md` 只更新已证实的公开状态。
- `plan.md` 保留本阶段完整计划和收尾证据，直到下一阶段被 Main 明确替换；执行者不得编辑它。
- 提交边界为七个批准源码/测试路径及 Main 明确批准的文档变更；排除所有 `Content/**`、未批准 Config 和其他用户 WIP。未获得用户明确提交批准前不得 `git add` 或 `git commit`。

## Gemini Handoff Prompt

将下面的提示原样交给 Gemini；它是执行指令，不是新的架构授权：

> 你是本阶段的实现执行者。工作目录必须是 `E:\GameDevelop\PolyQuest`，基线为 `b2eb4f2559bdf7206f4be4012549693358144e6e`；先读取仓库 `AGENTS.md` 和当前 `plan.md`，按 `ue-stage-workflow` 外层流程、`ue5-cpp-gameplay` 主技能，并参考 `game-feel` 与 `ue5-debug-validation` 执行。Contract owner：Main；implementation writer：Gemini。只允许修改本计划列出的七个路径：`Source/PolyQuest/Public/AbilitySystem/Abilities/PlayerGuardAbility.h`、`Source/PolyQuest/Private/AbilitySystem/Abilities/PlayerGuardAbility.cpp`、`Source/PolyQuest/Public/Character/Player/PlayerCharacter.h`、`Source/PolyQuest/Private/Character/Player/PlayerCharacter.cpp`、`Source/PolyQuest/Private/Tests/TestGuardStaminaCostGE.h`、`Source/PolyQuest/Private/Tests/TestGuardStaminaCostGE.cpp`、`Source/PolyQuest/Private/Tests/PlayerDefenseAudioAutomationTests.cpp`。共享契约文件仍由 Main 负责；你只能实现已冻结的函数和 test-only seam。
>
> 执行顺序：确认两个 Resolver 已经把 `FHitResult` 传入 Player 防御入口且不改 Resolver；更新 Guard 与 Player 头文件契约；在 Guard Stamina GE 成功后一次性触发 `GuardSuccessSound`；在 Player 现有权威非致命 Health 回调中为精确 `Team.Enemy` Instigator 触发 `ReceivedHitSound`，并将其调用放在既有 `TriggerHitFeedbackCameraShake` 之后、Stunned/Invalid 分支之前；完成 `TestGuardStaminaCostGE` 和 `PolyQuest.Combat.DefenseAudio` 的完整负向/回退/去重/生命周期矩阵。Guard Break 吸收仍算一次成功接触。Player 的 `GetModifiedAttribute(Health)` 只控制音效首次调用，绝不能对整个回调 early return。所有音频失败都是静默 no-op，不能改变现有 GAS、Overlay、Camera Shake、Reaction、Guard Break、Projectile 或清理行为。
>
> 非目标：不要改 Gameplay Tag、Input、ASC/AttributeSet、Resolver、Controller、Enemy、Build.cs、Config、Content、Blueprint、GA/GE/Montage/AnimBP/Map，不新增 GameplayCue、复制、音频总线、混音、变体、池化或通用框架；不要编译、运行 Editor/PIE、打包、`git add` 或提交。任何需要白名单外文件、公共 API、资产或生命周期规则的情况立即停工，把证据返回 Main。
>
> 测试构造属于 Player 的有效 `FHitResult` 时，显式设置 `HitObjectHandle = FActorInstanceHandle(Player)`，并用错误 Actor/零点/NaN/Inf 覆盖回退路径；阵营判断按 `AEnemyCharacter::HandleCombatImpactFeedback` 的 `ICombatTeamAgent::Execute_GetCombatTeamTag` 模式实现。实现后只做静态证据：逐项读取 diff，运行可用的 Rider `get_file_problems`/`lint_files` 和 `git diff --check`。完成回报必须包含：实际 changed paths；每个检查的真实结果；未运行的 VS 编译、Editor Automation、资产读回和 PIE 门禁；严格自审中找到的 P0-P3（没有就明确写无）；以及仍存在的风险/假设。不要把静态检查或测试设计写成编译、运行时或视觉验证结论。

## Closeout Record

- Implementation evidence: Gemini changed the seven approved C3N source/test paths. Guard success audio is dispatched only after a successful Guard-Stamina GameplayEffect and carries the resolver HitResult through melee and projectile defense; Player received-hit audio is dispatched only for authoritative nonlethal enemy damage, once per GameplayEffect Spec's first Health modifier. All test-only counters/bypasses remain behind `WITH_DEV_AUTOMATION_TESTS`.
- User compile/Editor/PIE evidence: the user confirmed `PolyQuest.Combat.DefenseAudio` Automation as `Success` and confirmed the focused Scene01 PIE route. No separate Development Editor compile result is claimed in this record. The reported missing HUD, missing Stamina-delay fixture, absent Guard-break receiver, and invalid reaction-tag messages are expected fixture/negative-path signals.
- Main fresh review: one defect-first fresh review of the final source and direct resolver/caller paths found no P0-P2 or remaining actionable P3. The review verified that audio dispatch counters occur only after final World/location validation; the unused `Combat/Melee/CombatTeamAgent.h` include in the new test was removed, and Rider re-lint plus `git diff --check` completed without new errors.
- Roadmap/documentation/commit: `ROADMAP.md` now marks C3N complete and retains the existing C3M coverage debt with its closure trigger; `ARCHITECTURE.md` records the Guard/Player audio ownership and no-GameplayCue boundary; `README.md` records the completed stage and adds the user-provided [Bilibili combat demo](https://www.bilibili.com/video/BV14Ntw6RELr). The aggregate preset `Config/Automation/Presets/1.json`, `Config/DefaultEngine.ini`, `Content/**`, and all other user WIP remain excluded from staging. The approved seven source/test paths plus these three documents are ready for the focused commit.

## Post-Closeout Correction

- After the C3N closeout, the user-approved `EnemyCharacter.cpp` correction extends the existing C3K Enemy feedback boundary to the first lethal Health hit, immediately before `SetDeadState()` begins terminal teardown. Dead follow-up callbacks remain silent; the lethal path keeps the same finite-context, team-filter, preset, and per-Spec de-duplication rules.
- The existing `CombatHitFeedbackAutomationTests.cpp` lethal section was updated to assert one lethal hit-stop/sound dispatch, no blood without a HitResult, expiry, and no feedback after Dead. `ARCHITECTURE.md`, `ROADMAP.md`, and the English C3K README summary now describe this durable behavior. `Config/DefaultEngine.ini` was committed separately as the user-confirmed render-setting chore; all other WIP remains excluded.
