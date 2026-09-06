# Post-E Presentation Polish：基线重整与架构文档校准

## 阶段定位与基线

- 状态：基线重整与文档收口；不是新的运行时实现阶段，也不把已存在的体验提交重新包装成一次新验证的功能阶段。
- 日期：2026-09-06。
- cwd：`E:\GameDevelop\PolyQuest`；分支：`main`；当前代码基线：`b69dabed5b7c5cb541a00581623f3b7563236720`。
- E 的历史收口已保存在 `ROADMAP-archive.md:1342-1351`；Archive Preflight：PASS。替换本记录不会覆盖 E 的历史证据。
- 工作树非 clean，包含用户-owned `Content/**`、Config、Blueprint/AnimBP/Montage/地图及其他 WIP；这些变更全部保留，不执行 reset、checkout、批量删除或 `git add -A`。
- 当前回合的实现执行者：0；执行路线：none。此前由 Gemini 产生的提交只作为基线事实和执行者报告来源，不改变 Main 对架构、文档、验证解释和提交的所有权。

## 基线变更范围

`04a5197..b69dabe` 之间新增 9 个已提交变更。按 `git diff --stat`，累计涉及 18 个 Source/文档路径，约 `+2106/-22`；因此不能概括为“只有材质和后处理”。

### 已改变的运行时表现契约

- `2496bb2`：Player 原生视线遮挡透视驱动、MPC 参数更新、SpringArm 碰撞行为和 `APlayerCharacter::Tick()` 更新路径。
- `ce85338`：`UWeaponEquipmentComponent` 将角色 CustomDepth/Stencil 同步到动态武器显示组件。
- `dbf7d59`：Player Vital HUD 重构、Ghost Health Buffer，以及 `PolyQuest.Build.cs`/Widget 公开表面变化。
- `a394508`：耐力衰竭表现和 PlayerController/HUD 生命周期接入。
- `3ba4f4b`、`c557fff`、`5d88d1d`：残血边缘、受击/耐力拒止微震、血条闪光等 HUD 反馈。
- `b69dabe`：`UCameraModifier_FovPunch` 与 Enemy 血条自动隐匿/三态渐隐。

`94ea3d8` 的启动图、封面和 `AGENTS.md` 规则同步属于文档/治理变更，单独看待，不与运行时表现混为一类。

### 未见被改写的核心边界

在上述提交的 Source diff 中，未见 GAS/ASC 所有权、唯一 `FMeleeHitResolver` 伤害路径、Gameplay Tag/Input taxonomy、StateTree AI 或 Persistence 合同被重写。结论仅是 source/static 范围判断，不替代 Editor、编译或 PIE 证据。

这些提交仍触及共享 Player、Controller、Widget、Camera Modifier、Equipment Component 和 Build.cs 表面，统一归入 `TODO-07B: Feedback, Presentation Retune, And Demo Polish` 伞形范围；不能标记为纯材质改动，也不能据此宣称架构风险为零。

## ARCHITECTURE.md 校准结论

Gemini 提出的三类事实修正全部有源码依据，已按以下规则落盘：

1. Enemy 血条字段统一为 `AutoFadeDelay`、`FadeOutDuration`、`TargetHealthPercent`。
2. FOV Punch 休眠判定统一为 `FMath::IsNearlyZero(CurrentPunchOffset, 0.005f)`。
3. 相机 Modifier 查找统一为 `FindCameraModifierByClass(UCameraModifier_FovPunch::StaticClass())`。

对表达方式采取“同意原则、不照抄原稿”的处理：

- 保留 `bAutoFadeEnabled` 条件；自动隐匿通过 `RenderOpacity` 驱动，不写未经源码或 Editor readback 证明的 `SelfHitTestInvisible` 固定可见性承诺。
- 将空闲门描述为 `UpdateAutoFade()` 的短路；`NativeTick()` 仍调用各更新函数，`UpdateShake()`/`UpdateHitFlash()` 各自依据计时器无操作。
- 删除“零 CPU 开销”“防止叠加”“保证 headless/PIE”等宣传或方案辩护语句，保留可观察的状态、所有权、调用和数值契约。
- FOV Punch 的累计范围写成 `[-3.0f, 0.0f]`，恢复函数和阈值按当前实现记录，不把 `FInterpTo` 夸写成独立物理模型。

本次只清理最近新增的血条/FOV Punch 段落；旧章节中与既有稳定契约绑定的防御性措辞不做无关重写。

## 证据矩阵

### 已有 source/static 证据

- Git 状态、提交历史、`04a5197..b69dabe` 限定 diff 与 `git diff --stat`。
- 直接源码核对：`EnemyHealthBarWidget.h/.cpp`、`CameraModifier_FovPunch.h/.cpp`、`PlayerCharacter.cpp/.h`。
- `.code-review-graph` 以当前 HEAD `b69dabe` 建图；一次限定影响雷达返回 12 个变更文件、59 个函数/类、63 条受影响流程和 59 个测试缺口。风险分数仅作导航提示，不是缺陷结论。
- `.codegraph` 定向查询确认字段声明、FOV 调用点和 Find-or-Add 代码；图谱不替代运行时证据。

### 本回合未执行的门禁

- 未运行 `PolyQuestEditor (Development Editor)` 编译、Rider lint、Automation、Editor readback、PIE/视觉、网络或 packaging；本回合只修改文档。
- 用户此前确认的 E Automation/Scene01 PIE 仍只覆盖 E 的批准 Source/Test 路径，不扩展覆盖这 9 个后续提交。
- Gemini 报告的编译、Rider、资产或视觉结果继续按执行者报告分类，不能改写为 Main/user 本回合收据。

### 文档检查

- 本回合修改后运行 `git diff --check`。
- 只检查文档一致性和路径范围；不因文档检查成功而宣称运行时通过。

## 需要保留的风险与关闭条件

- `APlayerCharacter::Tick()` 现在承载视线遮挡更新，多个 HUD Widget 也有持续 Tick；关闭条件是 Scene01 PIE 下的生命周期/性能检查，并覆盖 Pawn 重生、UnPossess、EndPlay 和关卡切换。
- `CameraBoom->bDoCollisionTest = false` 改变了相机碰撞语义；关闭条件是用户确认固定相机、遮挡透视和近景碰撞行为符合产品意图，或另立相机阶段修正。
- MPC 是运行时共享状态；关闭条件是 BeginPlay/EndPlay、销毁和地图切换后的参数恢复 readback/PIE 证据。
- `PolyQuest.Build.cs` 和新增公开 Widget/Camera 表面需要 Development Editor 编译收据；没有该收据前不宣称 clean authored baseline 或 packaging readiness。
- Widget 的 Health/Stamina 绑定、Controller 回调和装备显示 CustomDepth 仍依赖用户-owned Blueprint/材质/后处理资产；关闭条件是冻结 manifest 后的 Editor readback 与目标 PIE 视觉验证。
- `AGENTS.md` 的治理变更来自 `94ea3d8`，本回合不改写；若规则与后续执行冲突，先由 Main 单独确认治理基线。

## 当前路线与下一阶段

- `TODO-07B` 吸收本基线中的相机、血条、耐力和武器显示表现变化；这些提交已落在代码历史中，但没有一个共同的本阶段验收收据。
- 下一 player-facing slice 仍为 `TODO-07A2-A: Prepared Skill Readiness/Cooldown HUD v1`：1-4 灰底白字方框，冷却时灰色遮罩和白色圆饼旋转/收缩指示；ASC 与 `UWeaponEquipmentComponent` 是唯一状态来源，不新增技能图标、背包、CommonUI 或第二套冷却计时。
- `TODO-07A2-A` 尚未开始；开始前需另写新的实施计划、冻结批准路径、提供 Gemini handoff，并由用户承担编译、Editor readback 和 Scene01 PIE 门禁。
- `TODO-03C: Ranged Enemy v1` 仍是独立敌人路线；HUD 切片不是它的前置，也不因本次基线重整改变其依赖顺序。
- 不新增泛化“健康度审查”占位 TODO。若优先清理上述风险，应建立只覆盖命名 compile/readback、Widget/Camera teardown 和具体性能证据的窄阶段，并为每项风险写出关闭收据。

## 本回合提交边界

- 允许修改：`ARCHITECTURE.md`、`plan.md`、`ROADMAP.md`、`README.md`（仅同步当前指针和证据边界）。
- 明确排除：`AGENTS.md`、全部 `Content/**`、Config、Blueprint、AnimBP、Montage、地图、Source 运行时代码和任何 `.uasset/.umap`。
- 用户已批准本次文档维护提交；仅按显式路径暂存上述四个文档，继续排除所有用户-owned WIP、Config、Content 和运行时代码。提交不代表新的运行时编译、Editor readback 或 PIE 验收。
