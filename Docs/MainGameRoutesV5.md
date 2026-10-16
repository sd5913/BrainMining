# MainGame 三矿区通路 V5

主关卡：`/Game/MainGame/Maps/Lvl_MainGame`。几何源文件位于 `Art/MainGame/RoutesV5`；办公室、回声/牌局/电视房、博物馆的源地图不修改。

- 第一区：保留原主路线及四条分支，改为切向分岔、不规则九边截面和宽窄起伏；加入 16 处由岩层本体构成的地面石笋、顶壁钟乳石。
- 第二区：哑光蓝紫色岩壁；两处上升空间，每处五个从地底伸出的不可破坏落脚平台；出口收束为向上挖掘的土层。移除原贯穿洞厅的溶洞石柱。仅本矿区配置环境点光源，并复用 `NS_DustMote` 漂浮粒子。
- 第三区：哑光暗红色岩壁；三段半径 5.5 米、各 1.1 圈和下降 11 米的紧凑螺旋。两处较大空间分别复用原溶洞石柱、神经岩石生成器。终点向下挖开土层，穿过博物馆顶壁落入房间。
- 神经岩石另外保留原生成器的细分枝（两个可挖实体），避免通路整体简化吞掉枝条；第二区的 13 个隐藏环境灯使用柔和蓝紫色、强度 180。
- 三种普通墙壁材质均为纯色、粗糙度 1、金属度与镜面反射 0。几何保留逐三角形法线，表面变化来自模型本身。
- 办公室出口第一块堵塞层及第二区、三区的部分堵塞层使用深灰蓝硬质材质，Actor 标签为 `BM_BombOnly`。镐子和钻机均拒绝破坏；炸弹可以破坏。瞄准时显示炸弹提示。
- MainGame 炸弹容量为 1；冲击半径 700 → 350 cm，挖掘半径 520 → 260 cm，视觉缩放 1.25 → 0.625。维持原补充时间。
- 矿镐碎屑继续使用项目原有的 `MiningDebrisBurst` 和八个混凝土碎块模型，以被敲击岩层材质着色；补充延迟加载岩层的材质回退和首次命中时的池初始化。

博物馆改造仅在 MainGame 的 `SuppressedSourceActors` 中屏蔽洞口穿过的顶板，并在主关卡内补齐洞口四周。旧侧面连接封闭，原东侧终点墙保留。区域转换使用顶洞处向下穿越的高度，避免玩家尚在顶洞上方时提前进入博物馆。

原有 139 个矿点重新落在新通路岩壁上；四种矿石数量、区域权重、矿镐材料配方与升级效果保持原配置。

## 生成与检查

`Scripts/main_game_routes_v5_plan.py` 配置路线、分支、平台及景观位置。后台 Blender 运行 `Scripts/build_main_game_routes_v5.py`，可通过 `-- --stage=TunnelV1`、`TunnelCavernV2`、`TunnelStalagmiteV3` 单独生成。它复用原闭合岩层/堵塞体管线及旧景观生成器；不会覆盖 V4。

`Scripts/validate_routes_v5_blender.py` 检查通路净空、平台上方空间和地面连接。每区另有实体拓扑、自交、九个被堵塞层隔开的空气区域检查。

在关闭交互编辑器后运行 `Scripts/run_routes_v5.ps1 -Script apply_main_game_routes_v5.py` 导入并安装，脚本只接受尚未安装 V5 的 MainGame。重做安装前先从记录的备份恢复关卡；不要重复运行旧四矿区/金币预算的布局脚本。

验证入口：

- `Scripts/run_routes_v5.ps1 -Script verify_main_game_routes_v5.py`：完整关卡碰撞、平台、硬质层工具限制、真实炸弹、向下挖土及博物馆落地转换。
- 同一命令增加 `-Render`：临时低画质渲染及截图。
- `Scripts/run_routes_v5.ps1 -Script verify_routes_v5_fx.py`：真实鼠标敲击产生原有材质碎屑。
- `BrainMining.Bomb.BombOnlyLayers`：普通岩层、生成岩层、分块岩层的炸弹专用限制与真实体积变化。
- 原材料养成验证继续使用 `Scripts/Run_MaterialProgressionChecks.ps1`。

报告写入 `Saved/Validation/MainGameRoutesV5`。该文档描述设计和验证入口，最终执行结果以报告中的 `passed` 为准。

2026-10-16 验证：C++ 编辑器构建成功；9 项原生自动化测试全部通过；最终保存关卡的渲染运行检查 72 项通过，包括碰撞净空、十个平台、喷气起落、真实炸弹及博物馆落地转换。原有碎屑输入/材质检查 26 项通过，139 个矿点的保存后校验、31 项真实输入/材料消耗/Tab 升级回归通过。渲染截图使用临时低画质设置；这些自动检查不代替完整人工试玩的手感调整。

可单独运行 `Scripts/routes_v5_neural_detail.py`（Blender 后台）从保存的 V5 源文件导出原神经枝条，再通过 `Scripts/run_routes_v5.ps1 -Script polish_main_game_routes_v5.py` 幂等更新细节及灯光。完整生成和安装流程也已包含这一步。
