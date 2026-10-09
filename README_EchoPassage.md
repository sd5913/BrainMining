# 盲人敲地 / 回声通路 P1

当前版本：共享主角色接入版（2026-10-09）。UE 5.8.2，单人本地原型。

## 试玩入口

| 地图 | 内容 |
| --- | --- |
| `/Game/Passages/Echo/Maps/Lvl_EchoMazeToCards_P1` | 推荐：约 96 米轻迷宫，暖光出口切换到卡牌房 |
| `/Game/Passages/Echo/Maps/Lvl_EchoPassage_P1` | 原短通道，约 52 米，末端显示完成提示 |
| `/Game/Rooms/CardDuel/Maps/Lvl_CardRoom_P1` | 单独试玩卡牌房，见 `README_CardRoom.md` |

这些地图的 GameMode Override 为 `MiningEncounterGameMode`，直接使用项目原有 `BP_FirstPersonCharacter`。WASD 移动、鼠标环视与普通跳跃沿用主角色；飞行在通道和牌室内临时禁用。

从入口向前走，地板坍塌后落地自动发出一次大范围回声。**点一下鼠标左键敲一下地面/近处墙面**，不会改为长按挖掘。R 清空回声和完成状态，返回坍塌前。Esc 按编辑器默认行为结束试玩。

## 视觉与布局规则

策划来源：《通路1-2：盲人敲地【P1】.xlsx》。按后续反馈保留纯黑世界、白圈涟漪以及由近及远显形的白色轮廓；又增加较长、难度轻的迷宫和真实卡牌房联结。

- 不敲击时环境为黑色。敲击点发出两道白色圆环，波前扫过后白色线框显现；亮度同时随距离和时间衰减，远处更淡，最后回到全黑。
- 默认半径 1550 cm；落地回声为 1.4 倍；传播速度 1000 cm/s；每个位置显形后 2.6 秒内消退；敲击间隔 0.65 秒；最多同时保留 4 道回声。
- 射线先检测视线前方 245 cm 内的表面；没命中时敲脚前地面，向下检测长度适配角色胶囊体。敲击动画开始 0.18 秒后发声波。
- 轮廓被实体墙遮挡。脚步不会自动照亮；没有追逐敌人、限时或生命消耗。
- 短通道：14 格、4 米格距、约 52 米中心线路程。
- 轻迷宫：25 格主路、约 96 米最短路线；两条各 8 米死路、一处重新汇入主路的回环；无四岔口，三组石柱作为回声地标。
- 通道末端可见暖光。联结地图通过短暂淡出切换到暖光牌室；牌室不采用全黑效果。

## 实现导航

以下路径相对工程根目录。

| 文件 / 目录 | 职责 |
| --- | --- |
| `Source/BrainMining/MiningEncounterPlayer.h/.cpp` | 共享角色/控制器入口和可恢复的玩法能力限制 |
| `Plugins/EchoPassage/Source/EchoPassage/Public/EchoPassagePlayer.h` | `UEchoPassageInteractionComponent` 接口 |
| `Plugins/EchoPassage/Source/EchoPassage/Private/EchoPassagePlayer.cpp` | 单击敲击、落地事件、入口坍塌流程、镐子表现、重来与地图切换 |
| `Plugins/EchoPassage/Source/EchoPassage/Public/EchoPassageStage.h`、`Private/EchoPassageStage.cpp` | Stage 几何、碰撞、回声材质参数和完成事件 |
| `Plugins/EchoPassage/Source/EchoPassage/Public/EchoPassageLayout.h` | 主路、支路、出口坐标和纯逻辑可见度计算 |
| `Plugins/EchoPassage/Source/EchoPassage/Private/SEchoPassageView.h/.cpp` | 敲击/出口提示与冷却条 |
| `Content/Passages/Echo/Materials/M_EchoContour` | 波前、双圆环、距离与时间衰减材质 |
| `Content/Passages/Echo/Maps/`、`Audio/` | 独立地图和占位音效 |

## 主角色接入与生命周期

`AMiningEncounterGameMode` 的 PawnClass 与 `AMiningPrototypeGameMode` 完全一致：`/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter`。`AMiningEncounterPlayerController` 继承主控制器，读取原 Blueprint 控制器的输入映射及 CameraManager，调用基类输入安装，不另写移动速度、鼠标灵敏度或角色摄像机。

`UEchoPassageInteractionComponent` 挂在这个 PlayerController 上：

1. BeginPlay 找到 Stage 并添加通路 UI；绑定 `ACharacter::LandedDelegate`，将简单镐子挂到原有第一人称摄像机。
2. 入口坍塌时只暂时禁止水平输入并清除速度，避免落在隧道顶；落地触发 `EmitEcho(ImpactPoint, true)`，解除这一层移动锁。视角仍由主角色控制。
3. 左键 `IE_Pressed → Knock()`，冷却和落地条件满足后延迟触发 `EmitEcho(ImpactPoint)`。持有左键不连敲。
4. 到达出口执行 `Complete()`；有 `DestinationLevel` 时锁定切换输入、淡出 0.35 秒，并通过 `OpenLevelBySoftObjectPtr` 进入目标地图。
5. R 调用 `Restart()`，清除未完成敲击、重置 Stage、解除坍塌移动锁，按主角色胶囊高度回到入口。
6. EndPlay 清理落地委托、动态镐子、UI、切换 Timer 和本组件加上的输入锁。

`UMiningEncounterPlayerPolicy` 对当前 Pawn 临时增加飞行/挖掘限制，关闭矿工自带灯光。限制退出时恢复：飞行设置本身不改写，矿工输入 MappingContext 只在作用域内移除，避免左键和 R 同时触发挖掘。主玩法和收集地图继续使用各自 GameMode。离开组件/解除控制时成对释放，旧按住状态不会自动恢复为飞行。

## 对接接口与拼装说明

依赖方向：`BrainMining → EchoPassage / CardRoom → UE`；插件之间无 C++ 依赖，通道的目标世界使用软引用。

| 接口 / 属性 | 用法 |
| --- | --- |
| `UEchoPassageInteractionComponent::BindInput(InputComponent)` | 主控制器初始化时绑定左键/R；组件不接管移动/视角 |
| `Knock()` / `Restart()` | 支持从宿主输入或 Blueprint 调用 |
| `AEchoPassageStage::EmitEcho(WorldLocation, bLanding)` | 接入实际世界坐标的敲击/落地 |
| `OnPassageCompleted(PassageId)` | 每次通关一次；重置后可再次触发 |
| `bUseMaze` | 启用轻迷宫布局 |
| `DestinationLevel` | 联结地图指向牌室；空值表示独立完成原型 |
| `EchoRadius` / `EchoSpeed` / `OutlineDuration` / `KnockCooldown` | Stage Details 面板中的体验参数 |

未来拼入同一个主世界时保留主 GameMode，复用 Stage、交互组件和策略组件，在区域进入/退出时调用 `Apply/Release`。当前组件在 BeginPlay 查找一个 Stage；多通道、流送场景需要补显式 Stage 注册/激活，并用主世界的门口/流送流程替换 `TravelToDestination()`，不要直接执行 OpenLevel 卸载整个主世界。Stage 支持位置/朝向局部坐标，当前灰盒应保持默认比例和竖直方向。

现在切换地图会生成同一 Blueprint 类型的新玩家实体，不延续之前 Pawn 实例；没有跨图背包/任务存档。当前尚未验证网络或打包。

## 资源维护与验证

- `Scripts/integrate_encounter_player.py` 更新三张现有地图的 GameMode Override，先备份，不重建场景。
- `Scripts/create_echo_passage.py`、`create_echo_card_journey.py` 用于首次生成，已有目标地图会拒绝覆盖。
- `Scripts/update_echo_passage_material.py` 更新纯黑/白圈/白色轮廓材质；`generate_echo_passage_audio.py` 生成占位音效。
- `EchoPassage.Route` 验证短路/迷宫连通、死路、回环、出口和路线长度；`EchoPassage` 的回声测试验证到达、范围和消退；`BrainMining.Encounters` 验证共享角色和限制作用域；`BrainMining.Flight` 回归飞行。
- 人工检查：入口坍塌 → 落地回声 → 左键单击/长按 → 敲墙 → 消退回黑 → R 重来 → 暖光切图 → E 入座 → Q 离席。
