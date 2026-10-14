# BrainMining 项目导航（供开发者与 AI 快速定位）

核对日期：2026-10-13。所有文件链接相对仓库根目录；`/Game/...` 为 UE 内容浏览器中的资源包路径，对应 `Content/...`。本页集中提供入口，具体操作以链接文档为准。

## 先看这里

| 要找的内容 | 当前入口 | 说明 / 操作手册 |
| --- | --- | --- |
| 主流程、三段挖掘与房间拼接 | `Lvl_FullJourney` | [FullJourney](README_FullJourney.md)；[流程配置](Config/CampaignLayout.json) |
| 盲人通道 → 卡牌房 | `Lvl_EchoMazeToCards_P1` | [盲人通道](README_EchoPassage.md)、[卡牌房](README_CardRoom.md) |
| 新结尾房「涌现·那个下午」 | `Lvl_EndingEmergence_Demo` | [新结尾房](README_EndingEmergence.md)；[独立启动](Play_EndingEmergence_Demo.cmd) |
| 天堂马桶独立半球大厅 | `Lvl_HeavenToilet` | [收集玩法说明](README_Collection.md)；[独立启动](Play_HeavenToilet.cmd)；[地图](Content/CollectionPrototype/HeavenToilet/Maps/Lvl_HeavenToilet.umap) |
| 博物馆 / BABA-3D / 文思镜像博物馆 | `Lvl_WenSi_MirrorMuseum` | [主流程与源房间关系](README_FullJourney.md)；[WenSi 源码](Source/BrainMining/WenSi/) |
| 第一个电脑房 / 弹窗办公室 | `Lvl_PopupClose` | [办公室](README_PopupOffice.md) |
| 宝石和碎晶生成工具 | **MineralAuthoring** | [完整接入文档](Docs/MineralAuthoring.md)；[Python API](Scripts/campaign_minerals_api.py) |
| 在 Blender 修改主矿道及房间布局参考 | FullJourneyExchange | [UE ↔ Blender 交换说明](Art/Mining/FullJourneyExchange/README.md) |
| 挖掘卡顿、性能回归 | Performance | [性能说明与复测](Docs/Performance.md)；[运行性能采样](Scripts/profile_runtime_performance.py) |
| 编译、拉取资源、启动游戏 | 项目 README | [环境与 Git LFS](README.md) |

**当前入口以配置为准：** [DefaultEngine.ini](Config/DefaultEngine.ini) 的 `EditorStartupMap` 和 `GameDefaultMap` 均为 FullJourney；房间接入看 [CampaignLayout.json](Config/CampaignLayout.json)，启动参数看 [Play_Main.ps1](Scripts/Play_Main.ps1)。一些旧 README 的“默认地图”描述属于当时的版本，不代表现在的入口。

**新旧结尾房尚未统一：** 新结尾房资源已存在，但当前 `CampaignLayout.json` 的 `Ending` 和 `Play_Main.cmd -Experience Ending` 仍指向 `Lvl_CellUniverse_Demo`。要单独体验新结尾房，请使用 `Play_EndingEmergence_Demo.cmd`。后续接入主流程时，应一起检查配置、拼装结果和启动脚本；仅修改新结尾房不会自动替换主流程中的旧房间。

## 关卡资源索引

### 主流程与玩法源关卡

| 用途 | UE 包路径 / 文件 | 备注 |
| --- | --- | --- |
| 当前主流程 | `/Game/Campaign/Maps/Lvl_FullJourney` · [文件](Content/Campaign/Maps/Lvl_FullJourney.umap) | [Play_Main.cmd](Play_Main.cmd) / [Play_FullJourney.cmd](Play_FullJourney.cmd) |
| 外部矿道源场景 | `/Game/ExternalMining/Maps/Lvl_ExternalMining_Demo` · [文件](Content/ExternalMining/Maps/Lvl_ExternalMining_Demo.umap) | 主流程三段矿区的来源，主图中的矿道已烘焙；不是修改源图就自动同步所有岩层 |
| 盲人通道与卡牌联合关卡 | `/Game/Passages/Echo/Maps/Lvl_EchoMazeToCards_P1` · [文件](Content/Passages/Echo/Maps/Lvl_EchoMazeToCards_P1.umap) | 主流程 `Journey` 房间 |
| 独立卡牌房 | `/Game/Rooms/CardDuel/Maps/Lvl_CardRoom_P1` · [文件](Content/Rooms/CardDuel/Maps/Lvl_CardRoom_P1.umap) | 独立调试入口，不等同于联合关卡 |
| 电脑房 | `/Game/PopupClose/Maps/Lvl_PopupClose` · [文件](Content/PopupClose/Maps/Lvl_PopupClose.umap) | 主流程 `Office` 房间 |
| 镜像博物馆 | `/Game/WenSi/Maps/Lvl_WenSi_MirrorMuseum` · [文件](Content/WenSi/Maps/Lvl_WenSi_MirrorMuseum.umap) | 主流程 `Museum` 房间 |
| 新结尾房 | `/Game/EndingEmergence/Maps/Lvl_EndingEmergence_Demo` · [文件](Content/EndingEmergence/Maps/Lvl_EndingEmergence_Demo.umap) | 新版独立演示，接入状态见上方说明 |
| 旧细胞宇宙结尾 | `/Game/CellUniverse/Maps/Lvl_CellUniverse_Demo` · [文件](Content/CellUniverse/Maps/Lvl_CellUniverse_Demo.umap) | 目前主流程 `Ending` 仍使用此图 |
| 藏品收集原型 V0.3 | `/Game/CollectionPrototype/Maps/Lvl_Collection_V03` · [文件](Content/CollectionPrototype/Maps/Lvl_Collection_V03.umap) | 四类任务藏品，与可售卖矿物/宝石区分 |

FullJourney 的四个玩法房间通过 Always Loaded 源子关卡接入。修改房间时先定位对应源关卡；修改矿道、房间位置或衔接处时，还要检查主图岩层、路线、入口和触发区域。[重新拼装说明](README_FullJourney.md)包含两者的区别。

### 旧展示与原型地图

这些地图仍可用于调试，但不要据此判断当前主流程的布局或能力规则。

| 地图 / 目录 | 用途与说明 |
| --- | --- |
| [Lvl_MainExperiences](Content/Showcase/Maps/Lvl_MainExperiences.umap) | 旧六区总览；`Play_Main.cmd -Experience Overview`；[整合说明](README_MainIntegration.md) |
| [Lvl_ContinuousShowcase](Content/Showcase/Maps/Lvl_ContinuousShowcase.umap) | 旧连续展示；[连续展示](README_ContinuousShowcase.md)、[宝物展区](README_TreasureShowcase.md) |
| [Lvl_FeatureShowcase](Content/Showcase/Maps/Lvl_FeatureShowcase.umap) | 旧功能展示；[说明](README_Showcase.md) |
| [Lvl_WenSi](Content/WenSi/Maps/Lvl_WenSi.umap)、[Lvl_WenSi_ScreenBridge](Content/WenSi/Maps/Lvl_WenSi_ScreenBridge.umap) | 文思其他原型，当前主流程使用 MirrorMuseum |
| [MiningPrototype/Maps](Content/MiningPrototype/Maps/) | MiningLab、MiningSpatial、MiningManual、MiningSpaces、MiningShapes、MiningDescent、MiningHollow 等挖掘原型；对应文档见下文 |
| [MiningPrototype/Maps/Legacy](Content/MiningPrototype/Maps/Legacy/) | 历史版本；有与上一层同名的地图，必须确认完整包路径 |

旧 `Lvl_EchoPassage_P1` 已删除，查找盲人通道请使用 `Lvl_EchoMazeToCards_P1`。旧启动脚本或文档中的地图名不保证仍有对应资源，运行前检查 `Content`。

## 常用工具与修改入口

### MineralAuthoring：固定宝石与碎晶布局

先读 [Docs/MineralAuthoring.md](Docs/MineralAuthoring.md)。这是编辑阶段的离线工具；玩家开局读取保存好的布局，每次游玩的宝石和碎晶坐标相同。

| 入口 | 用途 |
| --- | --- |
| [campaign_minerals_api.py](Scripts/campaign_minerals_api.py) | UE 编辑器 Python 统一 API：`rebuild()` 完整重建、`rebuild_clues(preserve_surface=True)` 保留宝石和表面线索刷新内部碎晶、`validate()` 只校验 |
| [bake_campaign_minerals.py](Scripts/bake_campaign_minerals.py) | 独立 UE 进程批处理；默认只校验，显式 `-MineralsSave` 才保存；用法与备份规则见接入文档 |
| [CampaignMinerals.json](Config/CampaignMinerals.json) | 固定配方、矿区段落、价值预算、间距和种子 |
| [MiningCampaignMinerals.cpp](Source/BrainMiningEditor/Private/MiningCampaignMinerals.cpp) | 编辑器生成算法、埋深、碎晶分布和内部密度校验；碎晶密度常量当前在此，并非全部由 JSON 配置 |
| [MiningDesignerLibrary.h](Source/BrainMiningEditor/Public/MiningDesignerLibrary.h) | C++ 编辑器接口声明 |
| [MiningTreasure.cpp](Source/BrainMining/Showcase/MiningTreasure.cpp)、[MiningTreasureCampaign.cpp](Source/BrainMining/Showcase/MiningTreasureCampaign.cpp) | 运行时矿物、已存布局、碎晶表现与挖除逻辑 |
| [verify_mineral_authoring.py](Scripts/verify_mineral_authoring.py) | UE 集成验证；完整模式与结果判定见接入文档 |
| [test_campaign_mineral_plan.py](Scripts/test_campaign_mineral_plan.py) | 不依赖 UE 的配方规划测试 |

关卡地形大改后：先更新矿区 Tag、`MapRoute` 和可挖岩层，再校验并完整重建。只刷新碎晶不会修复宝石旧位置或失效岩层引用。API 本身不保存地图；生成、校验和保存是独立步骤。批处理报告位于本地 `Saved/MineralAuthoring/latest.json`，检查本次报告的 `ok`，要求保存时还需检查 `saved`。

当前规则与用户要求：内部越靠近宝石越密集，并有按球壳体积计算的密度校验；拾取宝石不清除仍附着岩层的碎晶。内部碎晶以中心点、表面碎晶以内侧支撑点判断岩层是否仍存在。具体约束和修改位置均在接入文档。

### 主流程拼接、岩层与 Blender

| 入口 | 用途 |
| --- | --- |
| [CampaignLayout.json](Config/CampaignLayout.json) | 主图、流程顺序、源房间、位置及矿段配置 |
| [create_full_journey.py](Scripts/create_full_journey.py) → [create_continuous_campaign.py](Scripts/create_continuous_campaign.py) | 全流程拼装入口与连续布局实现；操作前先读 [FullJourney](README_FullJourney.md) |
| [link_campaign_rooms.py](Scripts/link_campaign_rooms.py) | 源玩法房间子关卡链接相关工具 |
| [update_campaign_navigation.py](Scripts/update_campaign_navigation.py) | 主流程导航更新 |
| [FullJourneyExchange/README](Art/Mining/FullJourneyExchange/README.md) | UE 导出、Blender 编辑、回导流程；`EDIT_Terrain` 可编辑岩层、`REF_Rooms_and_Props` 房间和道具参考 |
| [MiningCampaignExchange.cpp](Source/BrainMiningEditor/Private/MiningCampaignExchange.cpp) | UE 侧关卡交换接口，如 `export_campaign_layout` |
| [blender_campaign_exchange.py](Scripts/blender_campaign_exchange.py) | Blender 侧交换工具 |
| [import_campaign_edited_rocks.py](Scripts/import_campaign_edited_rocks.py) | 将修改后的岩层回导到匹配的可挖 Actor；保存行为见交换说明 |
| [TunnelWideV2/centerlines.json](Art/Mining/TunnelWideV2/centerlines.json) | 当前主流程使用的矿道中心线数据 |
| [TunnelFill50V3/placement_plan.json](Art/Mining/TunnelFill50V3/placement_plan.json) | 当前主流程使用的填充岩层布局数据 |
| [MiningCampaignGeometry.cpp](Source/BrainMiningEditor/Private/MiningCampaignGeometry.cpp)、[MiningCampaignJunctions.cpp](Source/BrainMiningEditor/Private/MiningCampaignJunctions.cpp) | 拼装岩层、房间与通道衔接的编辑器实现 |

交换流程可把静态岩层及房间/道具的形状、位置导出作 Blender 编辑参考；它不等于把 UE 玩法逻辑、灯光、材质图和运行时 Actor 一并转换成可往返的模型。当前衔接岩层有独立 Actor，并非全部合入原始矿道模型。大幅改形后需重新检查可挖 Actor、碰撞、导航路线、房间入口和 MineralAuthoring 数据。

### 玩法实现与诊断

| 内容 | 源码 / 脚本入口 | 文档 |
| --- | --- | --- |
| 主流程与房间能力接入 | [MiningExhibitHall.cpp](Source/BrainMining/Showcase/MiningExhibitHall.cpp)、[MiningCampaignRooms.cpp](Source/BrainMining/Showcase/MiningCampaignRooms.cpp) | [FullJourney](README_FullJourney.md) |
| 盲人通道 | [EchoPassage 插件](Plugins/EchoPassage/Source/EchoPassage/)；[联合关卡构建](Scripts/create_echo_card_journey.py) | [EchoPassage](README_EchoPassage.md) |
| 卡牌房 | [CardRoom 插件](Plugins/CardRoom/Source/CardRoom/)；[独立关卡构建](Scripts/create_card_room.py) | [CardRoom](README_CardRoom.md) |
| 镐子等工具视觉、黑白线条效果 | [MiningToolEffectsComponent.cpp](Source/BrainMining/MiningToolEffectsComponent.cpp) | [工具美术](Docs/StylizedTools.md)、[镐子修订](Docs/PickaxeRevision.md) |
| 新结尾房 | [EndingEmergence.cpp](Source/BrainMining/EndingEmergence.cpp)、[构建脚本](Scripts/create_ending_emergence.py)、[测试](Source/BrainMining/Tests/EndingEmergenceTests.cpp) | [EndingEmergence](README_EndingEmergence.md) |
| 旧结尾房 | [MiningCellUniverse.cpp](Source/BrainMining/MiningCellUniverse.cpp)、[构建脚本](Scripts/create_cell_universe.py) | [CellUniverse](README_CellUniverse.md) |
| 博物馆 | [WenSi 源码目录](Source/BrainMining/WenSi/)、[主流程博物馆测试](Source/BrainMining/Tests/MiningMirrorCampaignTests.cpp) | [FullJourney](README_FullJourney.md) |
| 挖掘命中与岩层几何 | [MiningComponent.cpp](Source/BrainMining/MiningComponent.cpp)、[DiggableRock.cpp](Source/BrainMining/DiggableRock.cpp)、[GeneratedMiningRock.cpp](Source/BrainMining/GeneratedMiningRock.cpp)、[MiningChunkGeometry.cpp](Source/BrainMining/MiningChunkGeometry.cpp) | [Performance](Docs/Performance.md) |
| 性能与挖掘稳定性 | [运行性能采样](Scripts/profile_runtime_performance.py)、[主流程挖掘采样](Scripts/profile_campaign_mining.py)、[稳定性验证](Scripts/verify_mining_stability.py) | [Performance](Docs/Performance.md) |
| 工具资源提交前检查 | [check_tool_publish.ps1](Scripts/check_tool_publish.ps1)、[资源依赖校验](Scripts/verify_tool_asset_dependencies.py) | [ToolAssetPublishing](Docs/ToolAssetPublishing.md) |

## README 与专题文档总览

### 当前主流程与房间

- [README.md](README.md)：环境、编译、启动与 Git LFS。
- [README_FullJourney.md](README_FullJourney.md)：当前主流程、源房间联动、拼装与矿物布局。
- [README_EchoPassage.md](README_EchoPassage.md)：盲人通道。
- [README_CardRoom.md](README_CardRoom.md)：巨人卡牌房、电视与逃生。
- [README_PopupOffice.md](README_PopupOffice.md)：电脑弹窗房、炸弹能力和出口。
- [README_EndingEmergence.md](README_EndingEmergence.md)：新结尾房、构建、验证与独立启动。
- [README_CellUniverse.md](README_CellUniverse.md)：旧细胞宇宙结尾。
- [README_Collection.md](README_Collection.md)：四类藏品与收集原型。

### 宝石、能力、界面与表现

- [MineralAuthoring](Docs/MineralAuthoring.md)：宝石和碎晶离线生成、API、配方与校验。
- [TreasureProgression](Docs/TreasureProgression.md)：九种矿物、四档价值与升级。
- [MiningGUI](Docs/MiningGUI.md)：HUD 与升级界面。
- [InteractionHUD](Docs/InteractionHUD.md)：交互提示。
- [JetpackHeat](Docs/JetpackHeat.md)：飞行热量、消耗与冷却规则。
- [README_PlayerFlight.md](README_PlayerFlight.md)：基础飞行操作；当前限制同时参考 JetpackHeat 和主流程接入。
- [DrillMode](Docs/DrillMode.md)：钻机能力。
- [BombTool](Docs/BombTool.md)：炸弹能力。
- [StylizedTools](Docs/StylizedTools.md)：工具美术与准星。
- [PickaxeRevision](Docs/PickaxeRevision.md)：镐子模型修订。
- [ToolAssetPublishing](Docs/ToolAssetPublishing.md)：工具资源、依赖和发布检查。
- [Performance](Docs/Performance.md)：性能改动、采样与复测方法。

### 矿道编辑与美术源数据

- [README_ExternalMining.md](README_ExternalMining.md)：外部闭合通路模型与矿区生成。
- [README_MiningDesigner.md](README_MiningDesigner.md)：UE「工具 → 矿业策划台」。
- [README_SpatialMine.md](README_SpatialMine.md)：三维隧道编辑器与全局基岩。
- [三维隧道编辑器简明操作手册](Docs/三维隧道编辑器简明操作手册.md)：编辑器操作速查。
- [README_ManualMine.md](README_ManualMine.md)：手绘路径与空间编辑。
- [FullJourneyExchange](Art/Mining/FullJourneyExchange/README.md)：当前 UE ↔ Blender 交换。
- [TunnelWideV2](Art/Mining/TunnelWideV2/README.md)、[建模需求](Art/Mining/TunnelWideV2/optimized_prompt_zh.md)：加宽矿道源模型及需求。
- [TunnelFill50V3](Art/Mining/TunnelFill50V3/README.md)：当前填充岩层方案。
- [TunnelBlockers](Art/Mining/TunnelBlockers/README.md)、[TunnelCutter](Art/Mining/TunnelCutter/README.md)：较早的岩层阻挡与矿道切割模型方案。

### 原型、旧展示与历史记录

- [README_MiningPrototype.md](README_MiningPrototype.md)：MiningLab 早期挖掘原型。
- [README_EditableDescent.md](README_EditableDescent.md)：可编辑下降矿道。
- [README_AutoRooms.md](README_AutoRooms.md)：自动房间与分段挖掘连接。
- [README_HollowRegion.md](README_HollowRegion.md)：双房间中空矿区。
- [README_HollowChain.md](README_HollowChain.md)：串联中空矿区。
- [README_HollowBranches.md](README_HollowBranches.md)：分支与侧室。
- [README_MainIntegration.md](README_MainIntegration.md)：旧六区总览大厅。
- [README_ContinuousShowcase.md](README_ContinuousShowcase.md)：旧连续展示。
- [README_Showcase.md](README_Showcase.md)：早期功能展示。
- [README_TreasureShowcase.md](README_TreasureShowcase.md)：旧连续地图中的宝物展区。
- [SyncShenYilang](Docs/SyncShenYilang.md)：历史分支整合记录，不是当前同步操作指令。

## 后续 AI 的定位顺序

1. 确认用户指的是主流程、玩法源房间还是独立演示；优先用上面的完整包路径定位，检查当前配置和地图资产。
2. 阅读对应专题文档，再打开工具或源码。UE 编辑器 Python 工具需要编译后的 `BrainMiningEditor`；不要用普通 Python 执行依赖 `unreal` 的脚本。
3. 改矿道先处理岩层、路线和房间衔接，再运行 MineralAuthoring。不要把宝石/碎晶离线生成移到开局、挖掘回调或每帧中。
4. `Scripts` 中的 `create_*`、`upgrade_*`、`update_*` 脚本有各自适用地图和保存行为。尤其 `update_mining_interior.py`、`upgrade_campaign_crystals_museum.py`、`upgrade_campaign_cave_presentation.py`、`upgrade_campaign_junctions_v2.py` 是特定改版脚本，不应当作日常矿物重建入口。
5. 按修改范围验证；更多入口见 [Scripts](Scripts/)、[运行时测试](Source/BrainMining/Tests/)。`Saved` 下的备份与验证报告是本地产物，不保证其他机器已有。
6. 新增、重命名、删除关卡或更换主流程引用时同步更新本索引，尤其保持“新结尾房 / 主流程实际结尾房”的接入状态准确。
