# TODO-07B4：Player Attacker-Impact Camera Shake And Reaction-Tier Mapping v1

> 本阶段只补齐玩家攻击命中敌人时的三档镜头冲击反馈。GAS、现有伤害路径、Hit-Stop、敌人受击分类和用户资产所有权保持不变。攻击者反馈与玩家受击/防御反馈使用独立配置字段，但允许引用同一组 Camera Shake 资产；两者继续共用既有单一运行时 Shake 生命周期。实现由手动/out-of-band Gemini 执行；Main 保留契约、验证解释、Fresh Review、文档和提交所有权。

## Plan State And Route

- **状态**：字段解耦修复已完成；用户已确认 focused Automation 与 Scene01 PIE，Main 已完成一轮独立 defect-first fresh review；本次收口不重复运行已覆盖门禁。
- **仓库/基线**：`E:\GameDevelop\PolyQuest`，`main @ 9c2ca481902806e3ff064cbdfba9268a4d4761fa`（`9c2ca48`）。
- **当前工作区**：保留 `Config/Automation/Presets/1.json`、全部 `Content/**` WIP，以及 `Source/PolyQuest/Private/Tests/WeaponEquipmentComponentAutomationTests.cpp`；这些路径不属于本阶段。
- **Outer**：`ue-stage-workflow`。
- **Primary**：`ue5-cpp-gameplay`。
- **Support**：`ue5-debug-validation`。
- **Route reason**：这是已有 GAS Health/GameplayEffect 反馈边界上的窄 C++ 生命周期扩展，不涉及 Blueprint/资产写入、相机系统重构或新伤害路径。
- **Execution route**：`manual/out-of-band Gemini`。
- **Plan explorers**：`0`（由 Main 完成只读定点探索）。
- **Implementation executors**：`1`（Gemini）。
- **Main parallel work**：`none`。
- **Contract owner**：Main；**implementation writer**：Gemini。
- **引擎依据**：`D:\UE\UE_5.8` 中现有 `APlayerCameraManager::StartCameraShake/StopCameraShake`、`bSingleInstance` 和项目当前 Camera Shake 生命周期实现。

## Objective And Player Value

当前玩家攻击已经有由 `AEnemyCharacter::HandleCombatImpactFeedback()` 发起的顿帧，但攻击者本地镜头没有对应的冲击反馈。完成后，玩家对敌人的有效 Small/Big/Launch 命中会分别得到三档镜头震动，并与现有顿帧同时发生；死亡命中也只反馈一次。

唯一生产触发边界：

```text
AEnemyCharacter::OnHealthAttributeChanged
    -> (同一 GameplayEffect Spec 的 Health 去重)
    -> AEnemyCharacter::HandleCombatImpactFeedback
    -> APlayerCharacter::TriggerAttackerImpactCameraShake
    -> attacker-specific tier resolver
    -> 共享 Camera Manager / 活动 Shake 生命周期
```

这里的 `HandleCombatImpactFeedback()` 是已存在的权威 Health-decrease 边界。不得从 Trace Task、Melee Resolver、Projectile Resolver、Projectile Actor 或任意 Tick 另起通知路径。

## Frozen Runtime Contract

### Player API And Camera Ownership

在 `PlayerCharacter.h/.cpp` 增加一个窄的 C++-only 公共方法：

```cpp
void TriggerAttackerImpactCameraShake(EHitReactionTier ReactionTier);
```

契约：

- 不使用 `UFUNCTION`，不暴露 Blueprint，不增加 Delegate、Tag、Config 或网络接口。
- 入口检查 `HasAuthority()`、`IsActorBeingDestroyed()`、ASC 有效性和现有 `State.Status.Dead`；不满足条件直接返回。
- 保留现有 `SmallHitFeedbackCameraShakeClass`、`BigHitFeedbackCameraShakeClass`、`LaunchHitFeedbackCameraShakeClass` 字段名和序列化路径；它们只表示 Player 受击/防御侧的配置，不再在注释或行为上声称同时覆盖攻击者反馈。
- 新增三个独立的 `EditDefaultsOnly` attacker 配置字段，名称固定为 `SmallAttackerImpactCameraShakeClass`、`BigAttackerImpactCameraShakeClass`、`LaunchAttackerImpactCameraShakeClass`，分类放在 `Combat|Feedback|AttackerImpact`；不新增 `UFUNCTION`、Config、Tag、Delegate 或网络接口。
- `TriggerHitFeedbackCameraShake(ReactionTier)` 只能读取既有受击字段；`TriggerAttackerImpactCameraShake(ReactionTier)` 只能读取 attacker 字段，禁止从另一组字段回退或隐式借用。
- 允许把同一 `UCameraShakeBase` 资产类分别填入两组字段；资产复用不等于字段复用。新 attacker 字段为空时 fail-closed，不得因为受击字段有值而产生攻击震屏。
- 为避免复制逻辑，可抽出一个只接收“已解析 Shake class”的最小私有启动 helper；受击和攻击路径继续共用 `PlayerCameraManager`、活动 Shake 弱引用、同档重启/换档停止和清理状态，不新增第二个运行时 Shake 通道。
- `Data.Reaction.Small/Big/Launch` 分别映射到各自路径的同名三档；`None`、`Invalid` 或对应路径缺少类时不启动、不替换当前活动 Shake，并保留按路径区分的缺失配置告警状态（如现有实现需要）。
- 保留同档 `bSingleInstance` 重启、换档停止旧实例、Originating `PlayerCameraManager` 弱引用，以及 `UnPossessed()/EndPlay()` 的现有清理。
- 测试专用配置 seam 必须把受击字段与 attacker 字段分开设置；不得用一个 seam 同时写入两组字段来掩盖耦合。

### Enemy Forwarding Boundary

在 `EnemyCharacter.cpp`：

- 增加对 `Character/Player/PlayerCharacter.h` 的实现文件 include。
- 保留现有 `Instigator` 获取、`UCombatTeamAgent` 检查和 `Team.Player` 精确过滤。
- 通过 `Cast<APlayerCharacter>(InstigatorActor)` 识别真正 Player；过滤通过后调用上述桥接方法。
- 调用桥接方法不改变既有 Hit-Stop、ImpactSound、Blood Niagara 的顺序或参数；这些仍由 Enemy 目标侧拥有。
- 保留 `OnHealthAttributeChanged()` 的 `EffectSpec.GetModifiedAttribute(Health)` 去重和 lethal 先反馈、后 `SetDeadState()` 的顺序。
- Player-team 但不是 `APlayerCharacter` 的来源继续得到既有目标反馈，不得到 Camera Shake；非 Player team 完全不进入该路径。
- 不增加 HitResult 必需条件；既有声音/Hit-Stop 的位置回退语义保持不变。

### Exactly-Once And Exclusions

每个被接受的 Health-damaging GE Spec 最多触发一个 attacker Shake：

- 同一 Spec 的多个 Health modifiers 只触发一次；不同 Spec 即使共享 Context 也各自触发一次。
- 首次 lethal Health decrease 在 Enemy 进入 Dead 清理前触发一次；后续 Dead-state 回调静默。
- Miss、无效/死亡/销毁目标、Guard/Parry absorption、Poise-only GE、直接 Attribute 写入、非 Player 来源、取消和 teardown 均不震屏。
- `None` 与多 Reaction Tag 导致的 `Invalid` 是合法 no-shake，不得选择“最高档”或隐式回退到 Small。

## Approved Change Paths

Gemini 只可修改以下四个路径：

1. `E:\GameDevelop\PolyQuest\Source\PolyQuest\Public\Character\Player\PlayerCharacter.h`
2. `E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\Character\Player\PlayerCharacter.cpp`
3. `E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\Character\Enemy\EnemyCharacter.cpp`
4. `E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\Tests\CombatHitFeedbackAutomationTests.cpp`

共享契约归属：`PlayerCharacter.h/.cpp` 的 Camera Shake/ASC/Dead 生命周期由 Main 负责；`EnemyCharacter.cpp` 的 Health/Team/feedback 边界由 Main 负责。Gemini 只能实现上述冻结调用，不得改变所有权或公共契约。

如需修改 `EnemyCharacter.h`、Resolver/Trace/Projectile 文件、Build.cs、uproject、Config、Gameplay Tags、Input、Content 或任何未列路径，必须停止并把证据交回 Main，不得绕过计划。

## Automation Contract

在现有 `CombatHitFeedbackAutomationTests.cpp` 中增加独立套件：

```text
PolyQuest.Combat.AttackerImpactCameraShake
```

复用现有 `FCombatAutomationFixture`、transient Player/Enemy/Controller 和三种 `TestHitFeedbackCameraShake`；不得新建平行 fixture 或修改 Resolver 请求结构。测试使用带 Player Instigator 和有效 Hit Context 的 transient、带动态 Reaction Tags 的 GE Spec，直接覆盖共同的 `Enemy Health delegate -> HandleCombatImpactFeedback` 边界；既有 Melee/Projectile Resolver 套件作为未改动的交付回归门。

最低断言集合：

- Small、Big、Launch 精确选择对应测试 Shake class。
- 字段解耦：仅配置受击字段时，Enemy health damage 的 attacker 路径不启动；仅配置 attacker 字段时，attacker 路径按三档映射；两组字段分别配置不发生隐式回退。
- Player 自身受击/Parry 路径仍只读取受击字段；attacker 字段为空或更换时不改变该路径结果。
- 同档命中重启同一实例并只增加一次 start 计数；换档停止旧实例并启动新实例。
- None/Invalid 不增加计数、不替换活动实例；既有 Hit-Stop 行为不回归。
- 单一 GE Spec 多个 Health modifiers exactly-once；独立 Specs 各自一次。
- 首次 lethal 在 Enemy Dead 前完成一次；Dead 后再次 Health GE 不再启动。
- Poise-only、直接 base Attribute 写入、非 Player source、Guard/Parry absorption 不启动。
- 无本地 Controller、Player Dead、Destroy/UnPossess/EndPlay 后不产生或不保留 stale Shake。
- 现有 Hit-Stop 请求计数、Small/Big/Launch 时序与 Camera Shake 共存。

不得把测试数量写死为某个历史数字；交接记录实际运行的 suite/result。

## Execution Order And Static Gate

Gemini 执行顺序：

1. 读取最新 `AGENTS.md`、本 `plan.md`，并用 CodeGraph 定点确认 `OnHealthAttributeChanged -> HandleCombatImpactFeedback`、`TriggerHitFeedbackCameraShake` 和直接 callers/callees。
2. 在 Player 头文件加入三个 attacker 字段、最小声明/测试 seam，并把既有字段注释收敛为受击/防御语义。
3. 在 Player cpp 为两组字段建立独立解析路径，抽取必要的最小共享启动 helper；保持桥接、ASC/authority/dead 检查和既有清理生命周期。
4. 在 Enemy cpp 保持 Player 类型识别和单一转发调用，不能借此改变既有反馈副作用和 lethal 顺序。
5. 在现有测试文件中加入字段解耦断言及必要的 transient GE helper；不修改生产 Resolver/GE 路径。
6. 阅读最终 diff，运行 `git diff --check`，并在可用时对四个 C++ 文件运行 Rider `lint_files`/`get_file_problems`；完成严格 defect-first self-review，重点确认没有 attacker -> received 字段回退。
7. 交接时列出 changed paths、调用链核对、静态检查结果、未运行的用户门禁、self-review findings 和 remaining risks，然后停止。

Gemini 不得编译、启动 Editor、运行 Automation/PIE、打包、stage、commit、reset、清理 WIP 或修改项目文档。

## User-Owned Validation Gates

实现交接后由用户负责：

1. 手动编译 `PolyQuestEditor`（VS2022，Development Editor）。
2. Editor readback：确认 `BP_Player` 的受击字段和 attacker 字段分别存在且可独立配置。v1 允许两组字段引用同一组资产：
   - `/Game/_FeedBack/Camera/CS_PlayerHit_Small`
   - `/Game/_FeedBack/Camera/CS_PlayerHit_Big`
   - `/Game/_FeedBack/Camera/CS_PlayerHit_Launch`
   受击三字段和 attacker 三字段各自读回为对应资产类，并确认三者是有效 `UCameraShakeBase` 且 `bSingleInstance=true`；不新增、复制或重绑资产。若 attacker 字段未配置，必须记录为未通过该门禁，不能以旧版共享字段验证替代。
3. 运行新增 focused suite 及现有相关 Hit Feedback、Melee、Projectile、Guard/Parry 回归套件。
4. 在 `/Game/Maps/Scene01` PIE 验证：实际 melee 与 Bow/projectile 的三档命中、顿帧共存、lethal 单次反馈、Miss/Defense/Poise-only 排除，以及 UnPossess/Destroy/teardown 清理。

Source 静态检查、CodeGraph、`git diff --check` 和 Gemini self-review 不能代替上述编译、Editor readback、Automation 或 PIE 证据。

## Documentation And Review Closeout

只有用户门禁完成且 Main review 通过后：

- `ARCHITECTURE.md`：记录 Enemy 目标侧反馈只负责转发 Player attacker impact，PlayerCameraManager 和三档 Shake 仍由 Player 拥有；不再写“Enemy has no Camera Shake path”这类与转发契约冲突的描述。
- `ROADMAP.md`：更新实际完成基线，移除活动路线中的完整 07B4 完成块，保持 `TODO-03A7 -> TODO-05A -> TODO-05B -> TODO-03C` 顺序，并登记真实验证债务。
- `ROADMAP-archive.md`：追加 TODO-07B4 详细 closeout、证据类别、排除项和未关闭债务。
- `README.md`：只更新必要的公开状态和已证实证据。
- `plan.md`：保留本阶段 closeout，直到下一阶段获准后再替换。

Main 对最终四个批准文件做一轮独立 defect-first fresh review，范围为批准文件及受影响符号一跳直接 callers/callees；重点检查 exactly-once、lethal ordering、authority/local-controller、Dead/teardown、weak reference、None/Invalid 和防御排除。普通收口不追加第二轮 adversarial review，也不派独立 Reviewer。

## Non-Goals

- 不新增或修改 Gameplay Tag、GameplayEffect、GameplayCue、Damage 路径、Resolver、Trace Task、Projectile、Hit-Stop 数值或全局相机变换。
- 不新增 attacker 专用 Camera Shake 资产、反馈 Dispatcher、第二个运行时 Shake 通道或 Enemy camera shake；本阶段明确允许且要求新增三项 attacker 配置字段。
- Gemini 不修改 Content/Config/Blueprint/Montage/AnimBP/Niagara/音频/Input/Build.cs/uproject；用户可在 Editor 的 `BP_Player` 默认值中独立填写新增 attacker 字段（允许复用现有三项 Camera Shake 资产），这不属于 Gemini 的文件改动范围。
- 不改变 Lock-On、Bow targeting、装备、Poise、Stance Break、Death、AI 或多人/复制边界。
- 不清理、回滚、覆盖或提交任何既有 WIP。

## Closeout Record

- **Implementation**：Player 受击与攻击者命中 Shake 使用两组独立的三档字段；Enemy 只在既有权威 Health/Team 反馈边界转发 ReactionTier；两路共用 PlayerCameraManager、活动实例和 UnPossessed/EndPlay 清理，不增加第二条伤害或反馈通道。
- **User evidence**：用户确认 `PolyQuest.Combat.AttackerImpactCameraShake` focused Automation 再次 `Success`，并确认 Scene01 PIE 通过。Automation 覆盖字段解耦、三档映射、同档/换档、exactly-once、lethal、排除项和 teardown。
- **Static/review evidence**：Gemini 报告 Rider error-level 检查无诊断与 `git diff --check` 通过；CodeGraph/Code-review-graph 仅作结构/影响辅助。Main 完成一轮独立 defect-first fresh review，字段隔离修复已核对，未发现 P0/P1/P2 blocker；不追加第二轮 adversarial review。
- **Unverified gates**：本记录没有独立的手动 `PolyQuestEditor` 编译日志或 `BP_Player` 六字段 Editor readback；不从 Automation/PIE 推断它们，已在 `ROADMAP.md` 登记具体 closure trigger。
- **Scope/commit**：阶段提交只包含四个批准 Source/test 路径与本阶段文档；`AGENTS.md`、Config、Content/**、`WeaponEquipmentComponentAutomationTests.cpp` 和其他 WIP 排除在外。本次收口父基线为 `9c2ca48`；最终提交 hash 以 Git 历史为准，不在本记录中自引用。
