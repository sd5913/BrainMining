# MainGame 三矿区通路 V5

当前关卡已更新至 [V6](MainGameRoutesV6.md)。本文保留 V5 的生成与验证记录，矿点数量和跑酷平台尺寸以 V6 为准。

主关卡：`/Game/MainGame/Maps/Lvl_MainGame`。几何源文件位于 `Art/MainGame/RoutesV5`；办公室、回声/牌局/电视房、博物馆的源地图不修改。

- 第一区：保留原主路线及四条分支，改为切向分岔、不规则九边截面和宽窄起伏；加入 16 处由岩层本体构成的地面石笋、顶壁钟乳石。
- 第二区：哑光蓝紫色岩壁；两处上升空间，每处五个从地底伸出的不可破坏落脚平台；出口收束为向上挖掘的土层。移除原贯穿洞厅的溶洞石柱。仅本矿区配置环境点光源，并复用 `NS_DustMote` 漂浮粒子。
- 第三区：哑光暗红色岩壁；三段半径 5.5 米、各 1.1 圈和下降 11 米的紧凑螺旋。两处较大空间分别复用原溶洞石柱、神经岩石生成器。终点向下挖开土层，穿过博物馆顶壁落入房间。
- 神经岩石另外保留原生成器的细分枝（两个可挖实体），避免通路整体简化吞掉枝条；第二区的 13 个隐藏环境灯使用柔和蓝紫色、强度 180。
- 三种普通墙壁保留棕色、蓝紫色、暗红色基色，粗糙度 1、金属度与镜面反射 0。保留低多边形轮廓，叠加柔和的世界坐标颗粒与微小表面凹凸；普通泥土附近复用原苔藓土色函数，形成不规则的柔和过渡。
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

## 土层与碎屑修复

普通堵塞层恢复为房间连接处原有的 `DirtTransition/MI_ClayBlockage`，硬质炸弹层仍使用区别材质。`build_routes_v5_clay_relief.py` 从原 V5 源文件构建两侧不规则起伏，并沿洞壁向外延伸真实可挖土块；`validate_routes_v5_clay_geometry.py` 校验闭合和自交；`apply_routes_v5_clay_relief.py` 在备份后安装，保留矿点宿主引用和三片区域路线。首次完整生成后也需按此顺序执行这三个步骤。

碎屑继续使用原有 `MiningDebrisBurst` 和混凝土碎块资源。`diagnose_routes_v5_debris.py` 复现了池复用时的物理位置滞留：对象显示为可见，但其碎块仍留在上一处命中附近。修复通过物理传送同步复用组件的位置。`verify_routes_v5_debris_reuse.py` 使用初始矿镐和真实鼠标输入，检查当前命中周围的碎块位置、运动、材质并截图；不能再以组件的可见标记作为画面验证结论。

本次修复保存了 19 个普通矿区堵塞层的起伏、908 个沿墙延伸的实体土块，以及博物馆顶洞土层的凹凸。导出采用 UE 可挖体的顺时针外表面约定；普通堵塞层继续分块，房间连接处使用同一份旧泥土苔藓材质。六组真实镐击（含重复/异地复用）共 24 项检查通过，每次命中附近实测 31–42 个运动碎块；土层凸出、挖掘、重置及 139 个矿点校验 16 项通过；完整关卡运行回归 67 项通过。截图位于 `Saved/Validation/MainGameRoutesV5/DebrisFixed` 和 `ClayOverview`。

## 随机土块与岩壁协调

后续调整取消四圈、十二方向的土块槽位。采用连续随机距离、随机方向和随机簇，距离服从指数衰减；每侧延伸上限独立变化，远处土块同时缩小，顶点扰动打破重复轮廓。地面凸出仍受限。当前共 934 块，距泥土端面 0–1、1–2、2–3、3 米以上分别为 514、287、114、19 块。

在 Blender 后台依次运行 `build_routes_v5_clay_relief.py`、`validate_routes_v5_clay_geometry.py`，再执行 `run_routes_v5.ps1 -Script apply_routes_v5_organic_walls.py`。更新器复用现有 19 个土块 Actor，不替换矿点宿主、不改原泥土材质。三区原墙壁材质原位更新，因此外壳、连接通路、神经岩石和新挖出的表面共用处理。墙面色调参数仍是原始值；仅通过自然明暗、细颗粒和局部土色过渡协调材质。

`capture_routes_v5_organic_walls.py -Render`（通过同一 runner 调用）记录三区真实游戏截图和镐击碎屑；输出目录为 `Saved/Validation/MainGameRoutesV5/OrganicWalls`。`verify_routes_v5_clay_relief.py` 检查土块凸出、实际挖掘与重置、矿点宿主；`verify_main_game_routes_v5.py` 检查路线净空与平台玩法。

本次分布调整后，38 份实体网格闭合、自交检查通过；保存关卡的土块检查 16 项通过；完整运行回归 67 项通过，其中三区分别检查 63、69、103 处胶囊净空。原泥土材质文件哈希未变化，139 个矿点引用保持有效。
