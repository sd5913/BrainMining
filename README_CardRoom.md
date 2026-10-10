# 卡牌房间：实现与协作指南

核对日期：2026-10-09。UE 5.8.2，单人本地原型。面向程序、美术技术协作者和后续 AI 编程助手。

建议按 **快速开始 → 当前规则 → 代码导航 → 运行流程 → 拼装接口 → 接手约定** 阅读。本文描述当前共享主角色接入版；历史反馈以现行实现为准。团队环境、Git LFS 和资产协作规则见 [项目 README](README.md)；收集系统见 [README_Collection.md](README_Collection.md)。

## 1. 快速开始

- 独立地图：`/Game/Rooms/CardDuel/Maps/Lvl_CardRoom_P1`。
- 通道联结入口：`/Game/Passages/Echo/Maps/Lvl_EchoMazeToCards_P1`，走出通道后淡出切换到牌室。
- 三张遭遇地图的 World Settings → GameMode Override 均使用 `MiningEncounterGameMode`。
- WASD、鼠标视角与普通跳跃使用项目原有角色输入。飞行在本玩法内禁用。
- 靠近牌桌按 **E** 入座，点击「开始匹配」。点击手牌选择/取消，选中牌抬起并出现亮边；按 **空格** 或按钮把选中的牌甩到桌心。
- 每回合必须选齐指定手牌；第四回合支持全选 17 张。没有指定放牌区域。
- **Q** 离席并重置这一局，恢复行走和鼠标环视；胜利后 **E** 或按钮领取本地占位奖励。
- 奖励不接入挖掘、收集图鉴、背包或存档。攻击效果沿用已确认版本，桌面牌和手牌共用图案。

建议演示顺序：独立启动 → 靠近牌桌 → E 入座 → 开始匹配 → 四轮选牌/空格 → 领取占位奖励 → Q 离席。此玩法没有专用 `Play_*.cmd` 启动器，直接从编辑器地图入口运行。默认 MiningLab 启动地图保持原有配置。

## 2. 当前玩法规则

策划来源：《房间2：肉鸽卡牌【P1】.xlsx》，随后按用户反馈改为点击选牌、空格出牌。本阶段是固定四回合的表演式遭遇，不包含随机抽牌、费用或牌组构筑。

| 回合 | 指定手牌 | 表现与结果 |
| --- | --- | --- |
| 1 | 激光 | 激光命中，对手火焰反击 |
| 2 | 毒素 | 毒气残留，对手跳过反击 |
| 3 | 火焰 + 激光 | 引爆残留毒气；对手强击后玩家剩 1 HP |
| 4 | 17 张混合牌 | 彗星终结，玩偶被击飞，胜利 |

玩家 HP：100 → 80 → 80 → 1；对手 HP：100 → 84 → 76 → 28 → 0。它们是原型演出参数。玩偶保留呼吸、眨眼、表情、受击动作与气泡对白。

## 3. 关卡、参数与调节位置

| 要调整的内容 | 当前配置 / 修改入口 |
| --- | --- |
| 房间实例身份 | 关卡中 `CardRoomStage` 的 `RoomId`，默认 `CardDuel_P1`；完成/领取事件传出这个值 |
| 生成角色与输入 | 地图 World Settings 的 `MiningEncounterGameMode`；不要改全局默认 GameMode |
| 入座范围与构图 | `CardRoomStage.cpp::IsPlayerNear()`、头文件的 `GetSeatLocation()` / `GetApproachLocation()`，以及交互组件的摄像机偏移适配 |
| 匹配与落座时间 | `CardRoomRun.cpp::Tick()`：匹配 2.2 秒，对手登场 1.4 秒 |
| 出牌/反击结算点 | Resolving 阶段的 1.0 / 2.35 秒；普通回合 4.6 秒结束，终结回合 4.1 秒结束 |
| 手牌和伤害 | `FCardRoomRun::Deal()` / `Tick()`；当前是 C++ 固定流程，没有 DataTable/策划配置资产 |
| 手牌布局、按钮与文字 | `SCardRoomView.cpp`；绘制位置与鼠标命中区必须同时改 |
| 玩偶、攻击和实体卡 | `CardRoomStage.cpp` / `CardRoomVFX.cpp` / `CardRoomSymbols.h` |

回合时间与特效演出共享 `PhaseTime`，改结算时间时同步检查 Stage/VFX 的时间段，避免先扣血后命中。场景为程序化 lowpoly 组件；重新构造 Stage 会依据代码重建部分几何，不要把运行期组件当成独立美术资产。

## 4. 代码导航与职责

以下路径相对工程根目录；头文件看接口，cpp 看行为。

| 文件 / 目录 | 职责 |
| --- | --- |
| [MiningEncounterPlayer.h](Source/BrainMining/MiningEncounterPlayer.h) / [.cpp](Source/BrainMining/MiningEncounterPlayer.cpp) | 项目接入层：共用 GameMode、继承主控制器的遭遇控制器、临时角色能力限制 |
| [CardRoomPlayer.h](Plugins/CardRoom/Source/CardRoom/Public/CardRoomPlayer.h) | `UCardRoomInteractionComponent` 的交互入口 |
| [CardRoomPlayer.cpp](Plugins/CardRoom/Source/CardRoom/Private/CardRoomPlayer.cpp) | 入座/离席、输入和 UI 生命周期、座位摄像机演出 |
| [CardRoomRun.h](Plugins/CardRoom/Source/CardRoom/Public/CardRoomRun.h)、`Private/CardRoomRun.cpp` | `FCardRoomRun`：无世界依赖的规则、阶段推进、选择与确认 |
| [CardRoomStage.h](Plugins/CardRoom/Source/CardRoom/Public/CardRoomStage.h)、`Private/CardRoomStage.cpp` | `ACardRoomStage`：房间、玩偶、会话、完成事件 |
| [SCardRoomView.h](Plugins/CardRoom/Source/CardRoom/Private/SCardRoomView.h) / [.cpp](Plugins/CardRoom/Source/CardRoom/Private/SCardRoomView.cpp) | Slate 卡牌、按钮、对白、选择交互 |
| [CardRoomVFX.cpp](Plugins/CardRoom/Source/CardRoom/Private/CardRoomVFX.cpp) | 桌面实体牌图案和攻击表现 |
| [CardRoomSymbols.h](Plugins/CardRoom/Source/CardRoom/Private/CardRoomSymbols.h) | 手牌/实体牌共用图案几何与配色 |
| `Content/Rooms/CardDuel/` | 地图、材质与占位音效 |

## 5. 运行流程与状态

```text
Exploring → E 入座 → Seated → 开始匹配 → Matching → Arrival
    ↑                                                  ↓
    └──── Q 离席并重置 ← Choosing ⇄ Resolving（四轮） → Victory → Collected
```

`FCardRoomRun` 是规则权威：`Hand` 保存当轮手牌，`Staged` 保存已选索引，`CanConfirm()` 要求处于 Choosing 且全部指定手牌选齐。重复确认、越界选牌、错误阶段输入不会推进战斗。UI 不直接改 HP。

`ACardRoomStage::Tick()` 推进 Run 并驱动玩偶、音效和效果；`bWinSent` 防止一局内重复发送胜利事件。领取经过 `Run.Collect()` 的阶段检查，成功后才发 `OnCollectibleClaimed`。再来一局会清理演出标志；Q 会返回 Exploring，而按钮重玩保留入座状态。

界面是自绘 Slate `SLeafWidget`，不是 UMG/WBP。以 1600×900 虚拟画布缩放，`ToVirtual()`、`CardRect()` 和绘制共用坐标规则。`UCardRoomInteractionComponent` 创建/移除 Viewport UI，处理输入与座位状态；不承担规则计算。

## 6. 角色复用与表现实现

`AMiningEncounterGameMode` 直接生成 `/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter`，与 `AMiningPrototypeGameMode` 使用同一 Blueprint Class。角色的 `ABrainMiningCharacter`、`UMiningCharacterMovementComponent`、第一人称摄像机继续工作。

`AMiningEncounterPlayerController` 继承 `ABrainMiningPlayerController`，读取原 `BP_FirstPersonPlayerController` 的输入映射和 CameraManager 配置，调用基类 Enhanced Input 设置。插件不再轮询 WASD 或独立计算鼠标角度，也不再生成专用房间角色。

`UMiningEncounterPlayerPolicy::Apply(Pawn)` 临时阻止飞行和挖掘输入，隐藏矿工头灯/补光灯，让牌室使用自己的暖光。`Release()` 恢复灯光可见性并解除该作用域的限制。飞行使用计数锁，不修改 `FlightSettings.bEnabled`；挖掘锁临时移除自己的 MappingContext，避免左键/R 冲突。普通挖掘和收集地图不使用这个 GameMode，行为保持原有设置。

入座时组件保存位置、朝向、移动模式和碰撞状态，只增加自己的一层移动/视角锁，按实际主角色摄像机偏移对齐座位。离席、组件销毁时恢复。空格不会在座位上触发跳跃。自由探索时使用原角色转向手感，座位中保留固定镜头和受击晃动。

手牌与桌面实体牌共用 `CardRoomSymbols.h` 的火焰、毒滴骷髅和闪电图案。桌面图案网格必须保持朝上的法线与正确三角面绕序，否则看起来会重新变成白纸。现行攻击保留柔边火焰/烟团、激光和彗星光尾；用户已确认保留，不要按中途撤回的反馈恢复早期特效。

两种玩法的插件只负责玩法交互，不拥有角色移动系统。`UMiningEncounterPlayerPolicy::Apply/Release` 是 C++ 接入接口；如未来需要由蓝图区域触发，应先补蓝图包装或统一区域管理，不能假定它现在已是 BlueprintCallable。

## 7. 拼装接口与资源维护

依赖方向为 `BrainMining → CardRoom / EchoPassage → UE`。两个 Runtime 插件互不依赖，也不反向依赖项目模块。`CardRoomGameMode` 旧类仅用于旧地图加载兼容，当前地图请使用 `MiningEncounterGameMode`。

1. 独立地图接入：保留 Stage 和本地图 GameMode；通过地图软引用进入。
2. 以后合并到主世界：保留主 GameMode，在主 PlayerController 上挂 `UCardRoomInteractionComponent`，调用 `BindInput(InputComponent)`，在进入/离开玩法时成对调用角色策略 `Apply/Release`。当前组件在 BeginPlay 查找一个 Stage，适用于一张地图一个遭遇；流送或多个房间时应改成显式 Stage 注册/激活，不能直接把本 GameMode 设成整个主世界的 GameMode。
3. UI 层通过 `Interact()`、`ConfirmCards()`、`LeaveSeat()` 进入组件，再驱动 Stage/Run。组件销毁会移除自己的 Viewport UI。
4. 监听 `ACardRoomStage::OnRoomCompleted(RoomId)`：每局胜利一次。监听 `OnCollectibleClaimed(RoomId)`：本局领取一次。真正物品 ID、数量、持久化及跨局去重由主游戏决定。
5. 通道通过 `DestinationLevel` 软引用切换本地图，传入 `FromEcho=1`，牌室淡入。当前不传递玩家实体或背包状态，不是无缝流送。

| 接口 | 层级 / 语义 |
| --- | --- |
| `Interact()` / `LeaveSeat()` / `ConfirmCards()` | 交互组件，BlueprintCallable；管理角色和 UI，再调用规则 |
| `BeginEncounter()` / `RestartEncounter()` / `CollectReward()` | Stage，BlueprintCallable；单独调用不等同于完整入座/离席 |
| `OnRoomCompleted(RoomId)` | 胜利通知，每局一次，跨局去重由主系统负责 |
| `OnCollectibleClaimed(RoomId)` | 占位奖励领取通知，不携带真实收藏品 ID/数量 |

**和收集玩法的边界：** 当前未调用 `AMiningCollectionField`，也不会增加其 `Inventory`。后续需要明确房间奖励到稳定 ItemId 的映射，以及由谁持有跨地图收藏状态，再对接领取事件；不能直接把 `RoomId` 当作收集 ItemId。

脚本入口：

- [integrate_encounter_player.py](Scripts/integrate_encounter_player.py) 只更新三张遭遇地图 GameMode；首次备份到 `Saved/EncounterSharedPlayerBackup/`，不重建场景 Actor。
- [create_card_room.py](Scripts/create_card_room.py) 用于首次搭建，目标地图存在会拒绝覆盖。素材更新使用 `create_card_room_vfx_materials.py` 或 `generate_card_room_audio.py`，先阅读脚本的覆盖行为。
- 日常使用已有 `Content/Rooms/CardDuel/` 资产，不需要重跑生成器。更新程序生成的资源时同步脚本与资产。

## 8. 验证记录与复现入口

下面是后续修改时的验证入口。最近一次已经完成的记录（2026-10-09）：Development Editor 编译通过；`BrainMining.Encounters`、`BrainMining.Flight`、`CardRoom.Rules`、`EchoPassage` 共 **11 项成功，0 失败**。用户也已确认基础功能接入。

本机报告：`Saved/Automation/SharedEncounterPlayer/index.json`；构建日志：`Saved/Logs/Build-SharedEncounterPlayer.log`；运行日志：`Saved/Logs/Validate-SharedEncounterPlayer.log`。`Saved/` 是本机产物，不代表其他成员已有报告或持续 CI。

[MiningEncounterPIETests.cpp](Source/BrainMining/Tests/MiningEncounterPIETests.cpp) 的 PIE 回归经原有输入映射验证 W 移动、鼠标转向、左键单击/长按、R 重来、E 入座、空格确认、Q 离席。它会把角色移到出口验证切图、移到牌桌验证入座，并直接推进匹配/选牌状态；**不是全迷宫人工通关或完整鼠标 UI 操作验收**。飞行限制和恢复另由计数锁测试覆盖。

需要重跑时，先保存并关闭占用工程 DLL 的 UE 编辑器/独立试玩窗口，按项目 README 编译。之后可在工程根目录执行：

```powershell
$engineRoot = 'C:\UE_5.8' # 按本机安装位置修改
$projectPath = (Resolve-Path '.\BrainMining.uproject').Path
$reportPath = Join-Path (Get-Location) 'Saved\Automation\SharedEncounterPlayer'
& "$engineRoot\Engine\Binaries\Win64\UnrealEditor.exe" $projectPath `
    '/Game/Passages/Echo/Maps/Lvl_EchoMazeToCards_P1' -NoSplash -unattended `
    '-ExecCmds=Automation RunTests BrainMining.Encounters+BrainMining.Flight+CardRoom.Rules+EchoPassage' `
    "-ReportExportPath=$reportPath"
```

这条命令会打开地图并启动/结束 PIE，使用单独的已保存工作区运行；不会自动关闭最终编辑器窗口。工程现有工具插件可能在启动时报告 `GameFeatureData` 扫描配置错误，不能把该启动报错和某项测试失败混为一谈，以报告和具体调用栈定位。不要因此擅自修改团队插件配置。

当前未验证多人、正式打包、大规模性能或跨图存档。资源有运行时字符串加载，正式打包前要明确 cook 范围并验证成品；材质/UI 修改还需另行看渲染结果。

## 9. 常见修改路径与排查

| 问题 / 目标 | 优先检查 |
| --- | --- |
| 角色变了或移动手感不同 | 地图 GameMode、DefaultPawnClass、主控制器 Blueprint 输入配置；不要加回独立 WASD/鼠标轮询 |
| 牌室仍能飞行或左键误挖 | 策略是否对当前 Pawn Apply；切换控制对象时是否释放旧 Pawn、限制新 Pawn |
| E 无法入座 | 距离/高度门限、当前 Run.Phase、Stage 是否是本房间实例 |
| 空格不出牌 | 是否 Choosing、是否全部选齐；Slate 焦点和控制器绑定 |
| 座位太低或挡住牌桌 | 主角色摄像机实际偏移、座位目标视点、入座碰撞禁用和离席恢复；不要改主角色全局身高 |
| Q 后不能移动或转头 | 入座前移动模式、移动/视角锁的成对释放、GameOnly 和鼠标捕获恢复 |
| 桌面牌又变白 | 图案材质、网格绕序、法线、图案面位置，不只检查手牌 Slate |
| 改伤害/增回合 | 同步 Deal/Tick、UI 文案、玩偶/VFX 时间段和规则测试；目前固定四回合 |
| 重复发奖励 | Stage 本局去重，以及宿主对 RoomId/会话的跨局去重；不要每帧监听胜利状态就发奖 |

## 10. 程序协作与后续 AI 接手

先区分本次文件与已有挖掘/收集改动。`.umap/.uasset` 按项目 README 认领，避免同时编辑。修改共享角色、MiningComponent 或移动组件时说明受影响范围，不把其他系统的既有改动归入本任务。

接手时保持：复用原角色/移动/视角；本玩法临时禁飞；点击选中、空格甩向桌心；手牌和实体牌都有图案；保留现行攻击表现、玩偶呼吸表情与暖光；奖励暂不接主系统。多房间与无缝流送是后续集成工作，当前组件只自动寻找一个 Stage。

交接至少包含地图、工作区/提交版本、复现按键、变更文件、已完成验证及日期、待人工确认项。后续验证与扩展范围以用户当次要求为准。

可复制给后续 AI：

> 先阅读 README.md、README_CardRoom.md 和 README_EchoPassage.md，检查已有改动。牌室地图是 Lvl_CardRoom_P1，主角色入口在 MiningEncounterPlayer，规则在 CardRoomRun，交互在 UCardRoomInteractionComponent。保留现有选牌、空格出牌、图案和攻击表现；只完成本次指定需求。不要改全局 GameMode、重建既有关卡或擅自接入收集奖励。需要测试时先遵从用户当前对验证范围的要求。
