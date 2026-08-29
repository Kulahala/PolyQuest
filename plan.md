# TODO-03A3E: World Pickup Interaction Prompt v1

> 本阶段建立现有世界武器拾取物的本地交互提示。提示只表达当前可交互候选，不拥有输入、装备、GAS 或交易状态。实现由 Gemini 按冻结范围执行；Main 保留架构、验证解释、review、文档和提交所有权。

## Plan State

- **状态**：实现、用户确认的 Automation/PIE 门禁、Gemini 严格 self-review、Main 单轮 defect-first fresh review、文档收口与提交均已完成。
- **仓库/基线**：E:\GameDevelop\PolyQuest，main @ 397bea6。
- **当前工作区**：保留 Config/**、Content/** 及未批准 Source WIP；本阶段只收口下方批准的 10 个 Source/test 路径和项目文档。数量不作为提交边界或测试套件权威。
- **Outer**：ue-stage-workflow。
- **Primary**：ue5-world-interaction。
- **Support**：ue5-ui-umg-slate、ue5-cpp-gameplay、ue5-debug-validation。
- **Execution route**：manual/out-of-band Gemini。
- **Plan explorers**：0（由 Main 完成只读探索）。
- **Implementation executors**：1（Gemini）。
- **Main parallel work**：none。
- **Contract owner**：Main；**implementation writer**：Gemini。
- **引擎 API 依据**：D:\UE\UE_5.8 中确认的 OnCharacterMovementUpdated、Overlap delegate 和 SceneComponent::UpdateOverlaps API。

## Objective

在现有 AWorldWeaponPickup overlap、CanInteract 和最近候选装备路径上增加一个事件驱动的本地提示：

    Overlap / pickup 状态 / 玩家移动 / Dead 状态 / FormerOwner Timer
        -> APlayerCharacter 候选解析
        -> Player-owned 当前候选快照
        -> APolyQuestPlayerController
        -> 被动 UWorldInteractionPromptWidget

玩家按 E 时只使用与提示相同的当前候选快照；不在输入回调中重新扫描或隐式改选另一个拾取物。

## Current Source And Asset Facts

- APlayerCharacter::HandleInteractStarted 现在只消费 Player-owned 的 CurrentWorldPickupCandidate；候选失效时本次输入 Fail-Closed 并刷新提示，不在输入回调中重新扫描或隐式换选。
- UWeaponEquipmentComponent::TryEquipWorldPickup 仍是唯一世界拾取装备变更路径；不得让 UI 或候选解析直接改装备、ASC 或 GameplayEffect。
- AWorldWeaponPickup 的 InteractionSphere 是 QueryOnly、仅响应 Pawn overlap；CanInteract 已包含销毁、Definition、交互重入、玩家死亡和 FormerOwner 冷却条件。
- APolyQuestPlayerController 已拥有本地 PlayerVitalHUDWidget；UMG 和 Slate 已在 PolyQuest.Build.cs 中，无需新增模块依赖。
- UWeaponDefinition 是抽象 UDataAsset 基类；本阶段新增的显示名称不参与武器有效性、GAS 或交易判断。
- E:\GameDevelop\Test\Content\_GAME\BP\UI\HUD\WBP_InteractionPrompt.uasset 序列化为旧 Test 项目的 /Script/Test.InteractionPromptWidget，不能直接迁移为 PolyQuest 运行时资产。

## Frozen Product And Runtime Contracts

### Prompt text

- UWeaponDefinition 新增本地化 FText InteractionDisplayName。
- 有名称时显示：拾取 {0}。
- 为空时显示：拾取。
- 使用固定本地化 key：NSLOCTEXT("PolyQuest", "PromptWithWeapon", "拾取 {0}") 和 NSLOCTEXT("PolyQuest", "PromptFallback", "拾取")，再通过 FText::Format 组合；空文本合法，不增加名称清洗、Config 字段或 Gameplay Tag。

### Candidate ownership and arbitration

- APlayerCharacter 持有 TSet<TWeakObjectPtr<AWorldWeaponPickup>> WorldPickupCandidates，以及一个 TWeakObjectPtr<AWorldWeaponPickup> CurrentWorldPickupCandidate。
- 解析只遍历 Player-owned 集合，清理失效/销毁对象，调用 AWorldWeaponPickup::CanInteract(this) 做资格门。
- 保留当前最近距离和 Actor 名称字典序 tie-break 规则，不重写既有排序语义。
- AWorldWeaponPickup 提供 GetFormerOwnerRemainingTime(const APlayerCharacter* Requester) const；只有 FormerOwner.Get() == Requester 且冷却仍在时才返回剩余秒数，其他请求者一律返回 0.0f。
- 每次解析先 ClearTimer(FormerOwnerInteractionRefreshTimerHandle)，再记录所有仍属于该 Player 的冷却候选中的最早剩余时间；存在等待时设置一个一次性 Timer，没有等待时保持 Timer 清除/失效。
- 当前候选失效时，E 本次输入直接失败并刷新提示；不得重新扫描，也不得把另一个候选用于同一次输入。
- 交易成功、交易失败、pickup 状态改变或当前候选变更后刷新；TryEquipWorldPickup 仍是唯一 mutation。

### Event-driven refresh

- Pickup BeginOverlap/EndOverlap 注册或注销 Player。
- SetWeaponDefinition、InitializeDroppedPickup、BeginInteraction、EndInteraction、SetInteractionEnabled、EndPlay 通知重叠 Player。
- Player 在 BeginPlay 完成交互初始化，在任何现有体力 authority early return 之前绑定移动事件并进行一次 overlap seed。
- PossessedBy 幂等地重新 seed/refresh；UnPossessed 和 EndPlay 清集合、清 Timer、隐藏提示并解绑移动 delegate。
- 使用 UE 5.8 的 FCharacterMovementUpdatedSignature（float DeltaSeconds, FVector OldLocation, FVector OldVelocity）；只有位置确实改变且候选集合非空时刷新。
- 不在 Player 或 Controller Tick 中加入交互轮询；GetOverlappingActors 只允许用于初始化/重新 possession 的 seed。
- 复用现有 DeadStateTagChangedHandle 与 OnSprintRelevantTagChanged：Dead count > 0 时清理/隐藏，回到 0 时重新 seed/refresh；不增加第二个 Dead delegate。

### Widget and Controller ownership

- 新增 UWorldInteractionPromptWidget，继承 UUserWidget，只有一个必需的 BindWidget TextBlock，名称必须为 PromptText。
- Widget 提供 SetPromptText(const FText&)；即使测试或错误资产造成空绑定也必须安全返回。这里保留 BindWidget 而不是 BindWidgetOptional：PromptText 是本阶段的必需 Editor 契约，缺失应暴露资产配置错误，但不能造成 C++ 崩溃；Widget 仍不拥有输入、候选、GAS、装备、Timer 或 Tick。
- APolyQuestPlayerController 新增 InteractionPromptClass 和 Transient InteractionPromptInstance。
- 只有本地 Controller、非 Dedicated Server 创建；BeginPlay/OnPossess 幂等创建一个实例并加入 viewport 一次。
- 初始和隐藏状态为 Collapsed；显示状态为 HitTestInvisible，避免吞掉 E 输入。
- OnUnPossess 隐藏，EndPlay 移除并清空；缺少 class 只 warning 一次，不能阻断装备交互。
- 不改变 PlayerVitalHUDWidget、既有 InputMode 或 Controller 的 hit-stop Tick。

### Pickup lifecycle safeguards

- CanInteract 可增加 InteractionSphere 存在和 GetCollisionEnabled() != NoCollision 检查，但不得要求组件已注册。
- SetInteractionEnabled 调用 UpdateOverlaps 前必须检查 InteractionSphere->IsRegistered()；deferred FinishSpawning 前不得假定组件已注册。
- 通知重叠 Player 时使用 weak-pointer 快照，避免回调、销毁或 unregister 修改遍历中的集合。
- 不改变 StageDisplacedDrops、交易回滚、源 pickup 销毁或临时掉落物的既有顺序。

## Public/API Surface To Freeze

### WeaponDefinition

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|World Pickup")
    FText InteractionDisplayName;

### WorldInteractionPromptWidget

    void SetPromptText(const FText& InText);

    #if WITH_DEV_AUTOMATION_TESTS
    void SetTestPromptTextBlock(UTextBlock* InTextBlock);
    UTextBlock* GetTestPromptTextBlock() const;
    #endif

### PolyQuestPlayerController

    void ShowInteractionPrompt(const FText& InText);
    void HideInteractionPrompt();

    #if WITH_DEV_AUTOMATION_TESTS
    void SetTestInteractionPromptClass(TSubclassOf<UWorldInteractionPromptWidget> InClass);
    UWorldInteractionPromptWidget* GetTestInteractionPromptInstance() const;
    void TriggerTestEnsureInteractionPromptCreated();
    void TriggerTestShowInteractionPrompt(const FText& InText);
    void TriggerTestHideInteractionPrompt();
    #endif

这些接口均为最小 C++/Automation seam；不要增加 Blueprint 输入接口、通用交互接口、网络 RPC 或新的状态源。

### PlayerCharacter

    void RegisterWorldPickupCandidate(AWorldWeaponPickup* Pickup);
    void UnregisterWorldPickupCandidate(AWorldWeaponPickup* Pickup);
    void RefreshWorldPickupInteractionPrompt();
    void ClearWorldPickupInteractionState();
    AWorldWeaponPickup* GetCurrentWorldPickupCandidate() const;

新增的移动回调必须匹配引擎动态 delegate：

    UFUNCTION()
    void HandleCharacterMovementUpdated(float DeltaSeconds, FVector OldLocation, FVector OldVelocity);

FormerOwner 刷新 Timer、pickup state 通知和 teardown helper 必须是窄的内部 C++ 路径。

### WorldWeaponPickup

新增 Begin/End overlap 回调、重叠 Player weak set、通知 helper，以及精确签名为 GetFormerOwnerRemainingTime(const APlayerCharacter* Requester) const 的 private 冷却读取函数。该函数必须按请求者过滤并在冷却结束、世界无效或请求者不匹配时返回 0.0f；必要时使用 friend class APlayerCharacter；不得把这些内部状态扩大为 Blueprint API。

## Approved Change Paths

Gemini 只可修改以下路径：

1. E:\GameDevelop\PolyQuest\Source\PolyQuest\Public\Combat\Equipment\WeaponDefinition.h
2. E:\GameDevelop\PolyQuest\Source\PolyQuest\Public\UI\WorldInteractionPromptWidget.h
3. E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\UI\WorldInteractionPromptWidget.cpp
4. E:\GameDevelop\PolyQuest\Source\PolyQuest\Public\Framework\PolyQuestPlayerController.h
5. E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\Framework\PolyQuestPlayerController.cpp
6. E:\GameDevelop\PolyQuest\Source\PolyQuest\Public\Character\Player\PlayerCharacter.h
7. E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\Character\Player\PlayerCharacter.cpp
8. E:\GameDevelop\PolyQuest\Source\PolyQuest\Public\Combat\Equipment\WorldWeaponPickup.h
9. E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\Combat\Equipment\WorldWeaponPickup.cpp
10. E:\GameDevelop\PolyQuest\Source\PolyQuest\Private\Tests\WorldInteractionPromptAutomationTests.cpp

禁止修改 Config/**、Content/**、PolyQuest.uproject、Source/PolyQuest/PolyQuest.Build.cs、Gameplay Tags、输入资产、其他 Source 文件和项目文档。

## Automation Test Contract

新增套件名：

    PolyQuest.UI.WorldInteractionPrompt

至少覆盖：

- Native Widget 文本 setter、PromptText 注入和空绑定安全。
- 有名称/空名称两种固定提示文本。
- 最近候选、等距 Actor 名称 tie-break 和失效 weak pointer 清理。
- 无 WeaponDefinition、NoCollision、bInteractionInProgress、Dead、FormerOwner 冷却过滤。
- BeginOverlap/EndOverlap register/unregister。
- 玩家实际移动事件导致最近候选变化，且没有交互 Tick 扫描。
- FormerOwner 冷却结束后一次性 Timer 刷新；每次重新解析都会主动清除旧 Timer，当前无等待项时不得残留活动 Timer。
- E 使用缓存候选；缓存失效时不替换为第二候选。
- Controller 创建幂等、Collapsed/HitTestInvisible、UnPossess/EndPlay 清理。
- 现有 PolyQuest.Equipment.TransactionMatrix 回归；不复制或重写装备交易逻辑。

测试只使用原生 transient fixture 和 test-only seam，不依赖新 WBP、地图或导入资产。

## User-Owned Editor And Runtime Gates

用户在 Editor 中负责：

1. 新建 /Game/_UI/Interaction/WBP_WorldInteractionPrompt。
2. 将父类设置为 UWorldInteractionPromptWidget，并添加名称严格为 PromptText 的 TextBlock。
3. 在 /Game/BP/Game/BP_PlayerController 设置 InteractionPromptClass。
4. 至少一个现有 WeaponDefinition 填写 InteractionDisplayName，另准备一个空名称 Definition。
5. 保持 Content/Input/Actions/IA_Interact.uasset 和 Content/Input/IMC_Default.uasset 的 E 映射不变。
6. 不导入、复制、删除或重命名旧 Test Widget。

用户验证顺序：

- 手动 VS2022 编译 PolyQuestEditor（Development Editor）。
- Editor readback：WBP 父类、PromptText、Controller class、WeaponDefinition 名称和输入映射。
- Scene01 focused PIE：进入/离开范围、最近候选、同范围移动切换、等距 tie-break、空名称 fallback、FormerOwner 冷却、交易成功/失败、死亡、UnPossess、销毁和 teardown。
- 明确验证：缓存候选在 E 前失效时，不会偷偷装备另一个候选。

在用户证据到达前，不得把 Source 静态检查、CodeGraph、git diff --check 或 Gemini self-review 写成编译、Editor、Automation 或 PIE 证明。

## Static Review And Closeout

Gemini 交接时读取最终 diff，使用 CodeGraph 核对关键调用链，运行 `git diff --check`，并在可用时运行 Rider `lint_files` 或 `get_file_problems`，随后完成严格 defect-first implementation self-review。Gemini 未执行编译、Editor、Automation/PIE、打包或提交。

Main 在用户验证后完成一轮独立 defect-first fresh review，审查范围限定为本阶段批准文件及一跳直接调用边界；未发现 P0/P1/P2 blocker。普通阶段收口不追加第二轮 adversarial review。

验证和 review 通过后，Main 才更新：

- ARCHITECTURE.md：记录稳定的 Prompt ownership、Player 候选快照、事件驱动生命周期和装备边界。
- ROADMAP.md：将 TODO-03A3E 标记完成，更新下一阶段顺序，并登记实际存在的资产/验证债务。
- README.md：只补充必要的公开状态和证据摘要。
- plan.md：写入本阶段 closeout，直到下一阶段获准后再替换。

用户已明确批准本阶段提交。只按批准路径 staging；现有 Config/Content WIP 与未批准 Source WIP 不清理、不回滚、不纳入提交。

## Non-Goals

- 不复用旧 Test Widget，不导入或手工修改任何 uasset/umap。
- 不新增通用交互接口、库存/奖励系统、网络复制、RPC、GAS Ability、GameplayEffect、GameplayCue 或新的 Gameplay Tag。
- 不改变装备交易、回滚、掉落生成、Damage/Defense、Lock-On、Bow 或 Enemy AI。
- 不把提示刷新加入 Tick，不在 E 输入中重新扫描或自动换候选。
- 不修改 Build.cs、uproject、Config、输入资产、Blueprint 图、地图、动画、Niagara、音频或无关 WIP。

## Gemini Handoff Prompt (Historical Execution Record)

> 以下是已完成实现阶段的历史交接提示，仅用于追溯批准边界，不是当前执行指令。

你是 PolyQuest 的实现执行者 Gemini。

仓库 cwd：E:\GameDevelop\PolyQuest
基线：main @ 397bea6
引擎源码：D:\UE\UE_5.8

先读取 E:\GameDevelop\PolyQuest\AGENTS.md、E:\GameDevelop\PolyQuest\plan.md，以及本 TODO-03A3E 计划。

Outer: ue-stage-workflow
Primary: ue5-world-interaction
Support: ue5-ui-umg-slate, ue5-cpp-gameplay, ue5-debug-validation
Execution route: manual/out-of-band Gemini

Contract owner: Main
Implementation writer: Gemini

只实现本计划冻结的 Source/test slice，并且只修改 Approved Change Paths 中的 10 个路径。共享 ASC/AttributeSet、Gameplay Tag、Input、装备交易、生命周期和 UI ownership 契约由 Main 拥有；你不得改变它们。

执行顺序：

1. 先在头文件中完成最小 reflected API、forward declarations、Automation seams 和生命周期回调声明。
2. 实现被动 Prompt Widget 与 Controller 本地幂等创建/显示/隐藏/teardown。
3. 在 Player 建立候选集合、Resolver、CurrentWorldPickupCandidate、FormerOwner 一次性 Timer，并把初始化放在 BeginPlay 的 stamina early return 之前。
4. 将现有 E 处理改为只读缓存候选，再调用 CanInteract 和 TryEquipWorldPickup；失效时本次 no-op/refresh，不切换候选。
5. 在 Pickup 绑定 Begin/End Overlap、维护重叠 Player weak set，并覆盖所有列出的状态通知；UpdateOverlaps 前检查 IsRegistered。
6. 添加 PolyQuest.UI.WorldInteractionPrompt Automation 测试和最小 test-only seam。

固定文本必须使用以下 NSLOCTEXT key，再由 FText::Format 组合：

    NSLOCTEXT("PolyQuest", "PromptWithWeapon", "拾取 {0}")
    NSLOCTEXT("PolyQuest", "PromptFallback", "拾取")

FormerOwner getter 必须接收当前 Player 请求者并严格检查 FormerOwner.Get() == Requester；每次 Resolver 刷新前先 ClearTimer，无需等待时保持清除状态。PromptText 继续使用必需 BindWidget，setter 必须 null-safe。

禁止：

- 修改任何 Content、Config、文档、Build.cs、uproject、Gameplay Tags 或输入资产。
- 复用 E:\GameDevelop\Test 的 WBP_InteractionPrompt.uasset。
- 新建第二套装备或资格逻辑、通用交互接口、网络/RPC、GAS/GE/Cue、Tick 轮询。
- 清理、回滚、覆盖任何现有 WIP。
- 编译、启动 Editor、运行 Automation/PIE、打包或提交。

若需要未列出的文件、公开 API、Tag、Input、Config、资产或生命周期规则，立即停止并把证据返回 Main，不要自行绕过。

完成交接时列出：

- changed paths
- 每个路径的行为摘要
- CodeGraph、git diff --check、Rider 静态检查结果
- 未执行的编译/Editor/Automation/PIE 门禁
- strict self-review findings
- remaining risks

不要修改项目文档，不要提交。

## Closeout Record

### Scope accepted

本阶段最终只包含以下 10 个批准路径：

1. `Source/PolyQuest/Public/Combat/Equipment/WeaponDefinition.h`
2. `Source/PolyQuest/Public/UI/WorldInteractionPromptWidget.h`
3. `Source/PolyQuest/Private/UI/WorldInteractionPromptWidget.cpp`
4. `Source/PolyQuest/Public/Framework/PolyQuestPlayerController.h`
5. `Source/PolyQuest/Private/Framework/PolyQuestPlayerController.cpp`
6. `Source/PolyQuest/Public/Character/Player/PlayerCharacter.h`
7. `Source/PolyQuest/Private/Character/Player/PlayerCharacter.cpp`
8. `Source/PolyQuest/Public/Combat/Equipment/WorldWeaponPickup.h`
9. `Source/PolyQuest/Private/Combat/Equipment/WorldWeaponPickup.cpp`
10. `Source/PolyQuest/Private/Tests/WorldInteractionPromptAutomationTests.cpp`

实现保持以下边界：`APlayerCharacter` 持有弱候选集合和当前候选快照；Overlap、移动、拾取状态、FormerOwner 冷却、Dead、Possess 和 teardown 事件驱动同一解析器；Controller 只拥有本地被动 Prompt；`UWeaponEquipmentComponent::TryEquipWorldPickup` 仍是唯一装备变更路径。未增加 Tick 轮询、通用交互框架、Gameplay Tag、Input/Config 路由或第二套交易逻辑。

### Evidence

- **用户运行证据**：用户手动勾选当前全部 PolyQuest Automation 套件，25/25 为 `Success`；用户确认 focused `Scene01` PIE 覆盖提示显隐、最近候选切换、E 装备/失败交互、FormerOwner 冷却恢复、重新 Possess 与 teardown。
- **静态证据**：Gemini 报告 CodeGraph 调用链核对、Rider error-level 检查无诊断，`git diff --check -- Source/PolyQuest` 退出码为 0；warning-level 中的既有命名/风格提示不记为零警告。
- **复核证据**：Main 在用户修复与复测后完成一轮受控 defect-first fresh review，未发现 P0/P1/P2 blocker；未进行第二轮 adversarial review，也未派遣独立 Reviewer。
- **未声称证据**：本记录没有独立 Development Editor 编译日志或新的 Editor readback 记录；Automation/PIE 结果不替代该类证据。`Config/Automation/Presets/1.json` 不是完整套件清单，25/25 数量来自用户手动选择，不从该 preset 推断。

### Exclusions and remaining debt

- `Source/PolyQuest/Private/Tests/WeaponEquipmentComponentAutomationTests.cpp` 的 TransactionMatrix 路径修复不属于本阶段；未跟踪的 `Content/_Abilities/Weapon/LightSword/Guard/GA_Guard_Sowrd.uasset`、全部 Config/Content WIP 以及其他 Source WIP 均不纳入、不清理、不回滚。
- Automation 主要通过 transient test seam 驱动，真实 overlap/delegate 深度和“失效 E 快照绝不改变装备”的直接断言仍是非阻塞验证债务；只有未来出现交互回归或专门测试深度阶段时才关闭。
- TransactionMatrix 的本地 WIP 资产依赖保留到后续经批准的装备/资产基线阶段，并以用户 Editor readback 作为关闭条件。

### Documentation and commit boundary

- `ARCHITECTURE.md` 已补充 Prompt、候选快照、事件驱动刷新、Controller/Widget 与 Pickup 的稳定所有权契约。
- `ROADMAP.md` 已将 TODO-03A3E 标记完成，保留 `TODO-03A3E → TODO-07B4 → TODO-03A7 → TODO-05A → TODO-05B → TODO-03C` 顺序，并登记上述验证/资产债务；TODO-05C 继续退役。
- `README.md` 已补充公开状态和证据边界；`AGENTS.md` 已加入受控 fresh review 范围、证据短路和输出预算规则。
- 提交只包含上述 10 个 Source/test 路径与五份文档（`AGENTS.md`、`ARCHITECTURE.md`、`ROADMAP.md`、`README.md`、`plan.md`）；Config/Content WIP 与未批准测试改动明确排除。

阶段收口状态：用户已批准并完成本阶段提交；未暂存的 Config/Content/其他 Source WIP 仍保留在工作区，未被本次提交触碰。
