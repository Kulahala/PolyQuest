# TODO-07A2-A: Prepared Skill Readiness/Cooldown HUD v1 实施计划（修订版）

## 1. 阶段定位与基线

- **目标**：为 Prepared Ability Slots `1-4` 提供只读的 `Empty / Ready / Cooldown / Invalid` 状态显示，解决固定俯视角战斗中的快捷技能可读性问题。
- **基线**：`main @ 3915d7e`。
- **工作树**：非 clean；保留全部用户-owned `Content/**`、Config、Blueprint、AnimBP、Montage、地图、插件和其他本地 WIP。本阶段不依据状态数量扩大范围。
- **前置归档**：`TODO-07B9: Dungeon Multi-Floor Trigger & Visibility System v1` 已归档；本计划只承接新的 HUD 切片。
- **技能路由**：
  - Outer: `ue-stage-workflow`
  - Primary: `ue5-ui-umg-slate`
  - Support: `ue5-cpp-gameplay`
- **执行路线**：`manual/out-of-band Gemini`。Codex/Main 保留架构、计划、范围、验证解释、Fresh Review、文档、暂存和提交所有权；Gemini 只能实施下列冻结源/test 切片。

## 2. 冻结的运行时契约与非目标

- GAS、`UWeaponEquipmentComponent` 和现有输入路线继续作为唯一事实来源；Widget 不维护本地倒计时、不推测动画时长、不修改 Ability、GameplayEffect、GameplayTag 或 Input。
- 不新增 Input Action、技能图标迁移、背包/物品模型、技能树、拖拽换位、CommonUI、第二条 Ability 激活路径、全局 Subsystem 或网络复制契约。
- 每个槽位始终显示固定编号和灰底：`Empty` 仅保留编号；`Ready` 隐藏遮罩和扫光；`Cooldown` 显示灰色遮罩和白色扇形扫光；`Invalid` 保留编号和灰底并显示不可用遮罩、隐藏扫光。
- 玩家拥有 `State.Status.Dead` 时，技能栏保留在 Viewport，配置槽位进入 `Invalid`；空槽位仍为 `Empty`。移除 Dead 或重新 Possess 后立即重新读取 ASC 状态。
- UE 5.8 的 `GetCooldownTimeRemainingAndDuration` 基类实现按 Ability 的 Cooldown Tags 查询活动 GameplayEffect，Handle 参数不保证按 Spec 隔离。共享 Granted Tag 沿用 ASC 的共享冷却语义，不在本阶段迁移资产；Editor readback 必须记录共享是否有意。

## 3. 批准修改路径与资产边界

### Native / Test / Documentation

- 新增：
  - `Source/PolyQuest/Public/UI/PlayerSkillSlotWidget.h`
  - `Source/PolyQuest/Private/UI/PlayerSkillSlotWidget.cpp`
  - `Source/PolyQuest/Public/UI/PlayerSkillBarHUDWidget.h`
  - `Source/PolyQuest/Private/UI/PlayerSkillBarHUDWidget.cpp`
  - `Source/PolyQuest/Private/Tests/SkillBarHudAutomationTests.cpp`
  - `Source/PolyQuest/Private/Tests/TestPreparedSkillCooldownFixtures.h`
  - `Source/PolyQuest/Private/Tests/TestPreparedSkillCooldownFixtures.cpp`
- 修改：
  - `Source/PolyQuest/Public/Combat/Equipment/WeaponEquipmentComponent.h`
  - `Source/PolyQuest/Private/Combat/Equipment/WeaponEquipmentComponent.cpp`
  - `Source/PolyQuest/Public/Framework/PolyQuestPlayerController.h`
  - `Source/PolyQuest/Private/Framework/PolyQuestPlayerController.cpp`
- 收口后由 Main 更新：`ARCHITECTURE.md`、`ROADMAP.md`、本 `plan.md` 的验证和 closeout 记录。

### User-owned Editor manifest（不自动暂存）

首次保存前冻结并逐项 readback：

- `/Game/_UI/HUD/Skills/WBP_PlayerSkillBarHUD`
- `/Game/_UI/HUD/Skills/WBP_PlayerSkillSlot`
- `/Game/_UI/HUD/Skills/Materials/M_UI_SkillCooldownSweep`
- `/Game/BP/Game/BP_PlayerController` 的 `SkillBarHUDClass` 默认值

不得手工编辑 `.uasset/.umap`；上述资产属于用户-owned Content WIP，除非另行批准，不进入本阶段代码提交。

## 4. Native 实现契约

### `UWeaponEquipmentComponent`

- 增加只读 `TryGetPreparedSlotBinding(int32, TSubclassOf<UGameplayAbility>&, FGameplayAbilitySpecHandle&) const`。
- 接口先清空输出，再校验槽位范围、数组索引、Class/Handle、所属 ASC、组件自己的 grant 集合、Spec 是否存在/`PendingRemove`、以及 `Spec->Ability` Class 是否与槽位 Class 一致；行为与现有 `TryActivatePreparedSlot` 三重校验一致。
- 增加原生 `FOnPreparedSlotsChanged` 委托。只在 `EquipWeapon` 或 `TryEquipWorldPickup` 的逻辑事务最终提交后广播一次；不得在内部 `ApplyComposition`/`TeardownEquippedWeapons` 中广播中间空布局。失败并恢复旧组合不广播，最终恢复失败导致空组合时只广播一次清空状态；组件 `EndPlay` 不向已解绑 HUD 广播。
- 不暴露私有 Prepared 数组，不保存 `FGameplayAbilitySpec*` 给 Widget。

### `UPlayerSkillSlotWidget`

- 使用 `UCLASS(Blueprintable)` 和内部 `EPlayerSkillSlotDisplayState` 四态；核心更新接口只接收状态和归一化 `CooldownPercent`，不接收或保存倒计时。
- `CooldownPercent` 必须有限并 Clamp 到 `[0,1]`；`1` 为冷却开始满圆，`0` 为冷却结束。
- 在 `NativeConstruct` 中对 `CooldownSweepImage` 只创建一次 MID；后续只更新已有 MID 的 `CooldownPercent`，缺材质/MID/控件时隐藏扫光并 fail-closed。
- Headless test seam 仅放在 `WITH_DEV_AUTOMATION_TESTS` 下，用于注入可选控件、模拟更新和读取状态；不得成为生产 gameplay API。

### `UPlayerSkillBarHUDWidget`

- 持有 Equipment/ASC 弱引用、装备委托句柄和 `State.Status.Dead` 标签委托句柄；`BindToEquipmentAndASC` 先 `Unbind`，确认组件同属当前 Player 后注册委托并立即刷新。
- `RefreshAllSlots` 对四个槽位逐一取得当前绑定，然后从 `BoundASC->AbilityActorInfo.Get()` 取得有效 ActorInfo，验证 ActorInfo 的 ASC、Owner、Avatar；短暂读取当前 Spec 的 `Ability` 并调用 `GetCooldownTimeRemainingAndDuration`，不保留 Spec 指针。
- 状态映射固定为：有效绑定且 `Remaining > 0`/`Duration > 0` 为 `Cooldown`；有限且接近零为 `Ready`；无绑定、ActorInfo/Spec/Ability 无效、负 Duration、异常负 Remaining 或 NaN/Infinity 为 `Invalid`。`Duration <= 0` 不得显示 Cooldown。
- `NativeTick` 在已绑定且可见时每帧最多查询四个槽位，以获得连续权威剩余时间；无绑定立即返回，不创建本地计时器。只在状态或比例变化时写 MID，不宣称零 Tick 开销。
- `Unbind` 移除所有委托、清空弱引用/句柄并清理槽位；`NativeDestruct` 再次保证幂等清理。

### `APolyQuestPlayerController`

- 增加 `SkillBarHUDClass` 和瞬态 `SkillBarHUDInstance`，沿用现有 Vital HUD 的本地 Controller/非 Dedicated Server 防护。
- `EnsureHUDCreated` 幂等创建并以固定 Z-order 加入 Viewport；`BindToPawn` 从 `APlayerCharacter` 取得 ASC 和 `UWeaponEquipmentComponent`；`UnbindCurrentPawn` 解除技能栏后再处理既有属性/标签委托。
- `OnUnPossess`、`EndPlay`、`Destroyed` 移除技能栏、解除绑定并清空实例；重生/重新 Possess 必须复用同一实例并完整刷新。
- HUD 根节点为 `HitTestInvisible`，不改变现有 `GameAndUI` 输入/焦点所有权。

## 5. Editor 资产契约

- `WBP_PlayerSkillBarHUD` 父类为 `UPlayerSkillBarHUDWidget`，包含名称严格为 `Slot_1` 至 `Slot_4` 的四个 `WBP_PlayerSkillSlot`。
- `WBP_PlayerSkillSlot` 父类为 `UPlayerSkillSlotWidget`，绑定名称严格为 `BackgroundImage`、`SlotNumberText`、`CooldownOverlay`、`CooldownSweepImage`；四个控件同层覆盖，编号固定为 `1-4`。
- 根部使用 `SafeZone`，默认底部居中；每槽 `64x64`、间距 `8`、技能栏 Z-order `10`，根节点 `HitTestInvisible`。`CooldownOverlay` 初始灰色半透明，建议初始不透明度 `0.55`。
- `M_UI_SkillCooldownSweep` 为 UI 材质，标量参数固定为 `CooldownPercent`，从 12 点方向顺时针显示剩余扇区，白色输出；无材质或参数 readback 不得宣称视觉完成。
- `BP_PlayerController` 使用真实资产路径 `/Game/BP/Game/BP_PlayerController`，不得写成不存在的 `BP_PolyQuestPlayerController`。

## 6. 验证矩阵

### Focused Automation

- `PolyQuest.UI.SkillBarHUD.HeadlessDefense`：无 BindWidget、空弱引用、解绑和销毁后刷新不崩溃。
- `PolyQuest.UI.SkillBarHUD.SlotStateTransitions`：四态、可见性、编号和扫光/MID 状态正确。
- `PolyQuest.UI.SkillBarHUD.CooldownQueryStartRemainingExpiry`：使用 `TestPreparedSkillCooldownFixtures` 的真实 ASC/有限 Cooldown GE，验证开始满圆、中途比例、过期 Ready；覆盖共享 Tag 返回的 ASC 语义。
- `PolyQuest.UI.SkillBarHUD.InvalidAndFiniteInputDefense`：失效 Handle/Class、PendingRemove、缺 ActorInfo、NaN/Infinity、零/负 Duration、异常 Remaining 全部进入 Invalid，不误报 Ready/Cooldown。
- `PolyQuest.UI.SkillBarHUD.EquipmentTransactionAndLifecycle`：成功换装只收到一次最终通知；失败回滚无瞬态空布局通知；取消后 Cooldown 保留；Dead、UnPossess、重绑和最终清空无悬挂引用。

### User-owned gates

- Gemini 交付前：只做批准文件的静态检查、`git diff --check` 和实现自审，不编译、不写 Editor、不提交。
- 用户手动编译 `PolyQuestEditor (Development Editor)`。
- 用户按 manifest 完成 Blueprint/材质创建和 Editor readback，记录父类、绑定名、材质参数、`SkillBarHUDClass` 和共享 Cooldown Tag 语义。
- Scene01 PIE：验证四槽位的 Empty/Ready/Cooldown 开始/进行/结束、换装、Ability 取消、Dead、UnPossess/重新 Possess；确认 Vital HUD、输入和 `TODO-03C` 无回归。
- Main 在上述证据齐全后执行一轮有界 Fresh Review；将未完成的 compile/readback/资产证据按边界写入 `ROADMAP.md`，不得把静态证据写成 PIE 或视觉证据。

## 7. 停止条件、文档与提交边界

- 需要新增 Tag、Input、Config、Build.cs、未列出的源/资产、第二激活路径或改变 ASC/Equipment 所有权时立即停止并回报 Main。
- `ARCHITECTURE.md` 只记录通过验证的稳定 HUD/ASC 只读契约；`ROADMAP.md` 只同步里程碑、依赖和验证债务；不要在实现前写完成结论。
- 提交只允许批准的 Source/Test 和 Main 文档路径；排除所有用户-owned authored Content 与无关 WIP。不得使用 `git add -A`；必须等待用户明确提交批准。

## 8. 收口记录（2026-09-07）

- **实际交付**：批准的 11 个 Native/Test 文件已完成。`UWeaponEquipmentComponent` 提供最终组合通知、结构空槽查询与当前 Spec 查询；`UPlayerSkillSlotWidget` / `UPlayerSkillBarHUDWidget` 提供四态只读显示与权威冷却扫光；`APolyQuestPlayerController` 管理本地 HUD 创建、Pawn 重绑和 teardown；专项夹具与 `PolyQuest.UI.SkillBarHUD` 覆盖真实 ASC 冷却和 Equipment 生命周期。
- **Main 审查与窄修复**：两批有界 Fresh Review 发现并收口了两项状态/绑定缺陷。结构为空才显示 `Empty`，失效 Handle、`PendingRemove`、Spec 或 Class 不匹配显示 `Invalid`；Prepared 查询与激活路径均以 Ability Class 对照槽位 Class，避免显示与激活的校验口径分裂。未引入 Tag、Input、Config、Build.cs、资产写入、第二激活路径或 ASC/Equipment 所有权变化。
- **已确认验证证据**：用户确认当前 `PolyQuest.UI.SkillBarHUD` Focused Automation 为 `Success`，并确认 Scene01 PIE 通过。Main 对批准修复文件执行了 Rider 错误级检查（无诊断）及 tracked/untracked Source 的 whitespace 检查；Main 不把这些静态证据改述为编译、Editor readback 或视觉证明。
- **未关闭但非阻塞的 authored evidence**：没有单独归档的用户 `PolyQuestEditor (Development Editor)` 编译与直接 Editor readback。计划 manifest 的 Widgets 位于 `/Game/_UI/HUD/Skills/...`，而执行者报告写为 `/Game/_UI/HUD/Vitals/...`；实际包路径、父类、BindWidget 名、`CooldownPercent`、`SkillBarHUDClass` 与共享 Cooldown Tag 语义须由一次可追溯 readback 统一，详见 `Debt-07A2-A-AuthoredReadback`。
- **提交与归档边界**：候选提交仅包含本计划批准的 11 个 Source/Test 文件、Main 文档 `ARCHITECTURE.md`、`ROADMAP.md`、`plan.md`，以及用户明确批准的项目策略维护 `AGENTS.md`；明确排除全部 `Content/**`、Config、Blueprint、地图、导入资源和其他用户 WIP。当前 `plan.md` 保留为最近阶段交接；下一份正式计划替换它前再对 `ROADMAP-archive.md` 执行归档预检。
