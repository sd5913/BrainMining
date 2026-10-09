# 余烬牌室 / 卡牌房间 P1

当前版本：共享主角色接入版（2026-10-09）。UE 5.8.2，单人本地原型。

## 试玩入口与操作

- 独立地图：`/Game/Rooms/CardDuel/Maps/Lvl_CardRoom_P1`。
- 通道联结入口：`/Game/Passages/Echo/Maps/Lvl_EchoMazeToCards_P1`，走出通道后淡出切换到牌室。
- 三张遭遇地图的 World Settings → GameMode Override 均使用 `MiningEncounterGameMode`。
- WASD、鼠标视角与普通跳跃使用项目原有角色输入。飞行在本玩法内禁用。
- 靠近牌桌按 **E** 入座，点击「开始匹配」。点击手牌选择/取消，选中牌抬起并出现亮边；按 **空格** 或按钮把选中的牌甩到桌心。
- 每回合必须选齐指定手牌；第四回合支持全选 17 张。没有指定放牌区域。
- **Q** 离席并重置这一局，恢复行走和鼠标环视；胜利后 **E** 或按钮领取本地占位奖励。
- 奖励不接入挖掘、收集图鉴、背包或存档。攻击效果沿用已确认版本，桌面牌和手牌共用图案。

## 设计规则

策划来源：《房间2：肉鸽卡牌【P1】.xlsx》，随后按用户反馈改为点击选牌、空格出牌。本阶段是固定四回合的表演式遭遇，不包含随机抽牌、费用或牌组构筑。

| 回合 | 指定手牌 | 表现与结果 |
| --- | --- | --- |
| 1 | 激光 | 激光命中，对手火焰反击 |
| 2 | 毒素 | 毒气残留，对手跳过反击 |
| 3 | 火焰 + 激光 | 引爆残留毒气；对手强击后玩家剩 1 HP |
| 4 | 17 张混合牌 | 彗星终结，玩偶被击飞，胜利 |

玩家 HP：100 → 80 → 80 → 1；对手 HP：100 → 84 → 76 → 28 → 0。它们是原型演出参数。玩偶保留呼吸、眨眼、表情、受击动作与气泡对白。

## 实现导航

以下路径相对工程根目录。

| 文件 / 目录 | 职责 |
| --- | --- |
| `Source/BrainMining/MiningEncounterPlayer.h/.cpp` | 项目接入层：共用 GameMode、继承主控制器的遭遇控制器、临时角色能力限制 |
| `Plugins/CardRoom/Source/CardRoom/Public/CardRoomPlayer.h` | `UCardRoomInteractionComponent` 的交互入口 |
| `Plugins/CardRoom/Source/CardRoom/Private/CardRoomPlayer.cpp` | 入座/离席、输入和 UI 生命周期、座位摄像机演出 |
| `Plugins/CardRoom/Source/CardRoom/Public/CardRoomRun.h`、`Private/CardRoomRun.cpp` | `FCardRoomRun`：无世界依赖的规则、阶段推进、选择与确认 |
| `Plugins/CardRoom/Source/CardRoom/Public/CardRoomStage.h`、`Private/CardRoomStage.cpp` | `ACardRoomStage`：房间、玩偶、会话、完成事件 |
| `Plugins/CardRoom/Source/CardRoom/Private/SCardRoomView.h/.cpp` | Slate 卡牌、按钮、对白、选择交互 |
| `Plugins/CardRoom/Source/CardRoom/Private/CardRoomVFX.cpp` | 桌面实体牌图案和攻击表现 |
| `Plugins/CardRoom/Source/CardRoom/Private/CardRoomSymbols.h` | 手牌/实体牌共用图案几何与配色 |
| `Content/Rooms/CardDuel/` | 地图、材质与占位音效 |

## 主角色复用方式

`AMiningEncounterGameMode` 直接生成 `/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter`，与 `AMiningPrototypeGameMode` 使用同一 Blueprint Class。角色的 `ABrainMiningCharacter`、`UMiningCharacterMovementComponent`、第一人称摄像机继续工作。

`AMiningEncounterPlayerController` 继承 `ABrainMiningPlayerController`，读取原 `BP_FirstPersonPlayerController` 的输入映射和 CameraManager 配置，调用基类 Enhanced Input 设置。插件不再轮询 WASD 或独立计算鼠标角度，也不再生成专用房间角色。

`UMiningEncounterPlayerPolicy::Apply(Pawn)` 临时阻止飞行和挖掘输入，隐藏矿工头灯/补光灯，让牌室使用自己的暖光。`Release()` 恢复灯光可见性并解除该作用域的限制。飞行使用计数锁，不修改 `FlightSettings.bEnabled`；挖掘锁临时移除自己的 MappingContext，避免左键/R 冲突。普通挖掘和收集地图不使用这个 GameMode，行为保持原有设置。

入座时组件保存位置、朝向、移动模式和碰撞状态，只增加自己的一层移动/视角锁，按实际主角色摄像机偏移对齐座位。离席、组件销毁时恢复。空格不会在座位上触发跳跃。自由探索时使用原角色转向手感，座位中保留固定镜头和受击晃动。

## 对接与拼装

依赖方向为 `BrainMining → CardRoom / EchoPassage → UE`。两个 Runtime 插件互不依赖，也不反向依赖项目模块。`CardRoomGameMode` 旧类仅用于旧地图加载兼容，当前地图请使用 `MiningEncounterGameMode`。

1. 独立地图接入：保留 Stage 和本地图 GameMode；通过地图软引用进入。
2. 以后合并到主世界：保留主 GameMode，在主 PlayerController 上挂 `UCardRoomInteractionComponent`，调用 `BindInput(InputComponent)`，在进入/离开玩法时成对调用角色策略 `Apply/Release`。当前组件在 BeginPlay 查找一个 Stage，适用于一张地图一个遭遇；流送或多个房间时应改成显式 Stage 注册/激活，不能直接把本 GameMode 设成整个主世界的 GameMode。
3. UI 层通过 `Interact()`、`ConfirmCards()`、`LeaveSeat()` 进入组件，再驱动 Stage/Run。组件销毁会移除自己的 Viewport UI。
4. 监听 `ACardRoomStage::OnRoomCompleted(RoomId)`：每局胜利一次。监听 `OnCollectibleClaimed(RoomId)`：本局领取一次。真正物品 ID、数量、持久化及跨局去重由主游戏决定。
5. 通道通过 `DestinationLevel` 软引用切换本地图，传入 `FromEcho=1`，牌室淡入。当前不传递玩家实体或背包状态，不是无缝流送。

## 脚本、验证与边界

- `Scripts/integrate_encounter_player.py` 只更新三张遭遇地图的 GameMode Override，首次备份到 `Saved/EncounterSharedPlayerBackup/`；不重建或覆盖场景 Actor。
- `Scripts/create_card_room.py` 是首次搭建脚本，已有地图时拒绝覆盖；日常编辑不要重复生成。
- `Scripts/generate_card_room_audio.py`、`create_card_room_vfx_materials.py` 分别维护占位音效与特效材质。
- 自动化：`CardRoom.Rules` 测全流程、选择合法性、组合、1 HP、领取和不同帧率；`BrainMining.Encounters` 测角色一致性与限制作用域；`BrainMining.Flight` 回归主玩法飞行。
- 人工验收：独立启动 → 行走环视 → E 入座 → 选牌/空格 → Q 离席 → 再入座；从通道切换后暖光和鼠标控制正常；切回挖掘地图飞行恢复。
- 当前没有多人复制、跨地图存档、正式奖励接入或打包验证。
