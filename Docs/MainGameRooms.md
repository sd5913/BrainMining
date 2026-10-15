# MainGame 原关卡房间整合

MainGame 的顺序为：初始房间及硅质岩区 → `Lvl_PopupClose1` → 溶洞岩区 → `Lvl_EchoMazeToCards_P1` → 石笋岩区 → `Lvl_WenSi_MirrorMuseum1` → 神经岩区。

三个房间使用 Always Loaded 子关卡直接引用原地图，未复制房间 Actor 到主关卡。修改并保存原房间地图后，重新打开 MainGame 即会载入更新；也可以通过 Levels 面板编辑对应子关卡。原地图可以独立运行，其 GameMode 不会替换 MainGame 的 GameMode。入口/出口位置发生变更时，需要同步调整主关卡的过渡段与 `MiningExhibitHall.Areas`，引用本身不会自动推断新的门口位置。

配置在 `Config/MainGameRooms.json`。当前分区特色路线见 [MainGameRoutesV3.md](MainGameRoutesV3.md)：区域二净上升 45 米，其出口之后的 Echo/牌局/电视房间、区域三、镜像博物馆及区域四比首次装配方案整体上移 100 米。电视逃生的源坐标由运行时偏移修正，原独立关卡仍按零偏移运行。四区景观、32 处引导堵塞层及 165 处矿物奖励保留，碎晶重新贴合新地形（185,593 个）。初始房间、8 米喷气竖井和加宽入口保持本次改造前的位置。原三段预留连接区已由三个完整房间替代。

六个接口现在使用不规则拱形岩洞衔接，替代最初的方形管道。内侧是可挖的普通岩石实体，外侧有封闭、不可挖的硬岩壳与矿区封边；中央地面连续。三个房间的入口土层贴近原房间门口，挖开即可看到房间，小游戏也在跨过实际门口后才激活。泥土表面是有厚度和起伏的实体，沿用 MainGame 深色苔藓泥土材质，使用原生挖掘和分区 R 重置。三个小游戏出口各有流程门：办公室需炸开原出口，Journey 需完成电视逃生，博物馆需完成镜像谜题；原有 F7 跳过小游戏功能保留。最后挖掘区需要走到出口才完成整条路线。

运行时由 `MiningCampaignRooms.cpp` 为子关卡设置区域管理，并在 Journey 所在子关卡创建原生逃生流程控制器。共用 MainGame 的同一玩家、控制器、钱包、四项升级、炸弹、钻机和喷气背包；房间收益进入共享钱包。办公室仍保留实体炸弹拾取与炸门流程，但不会重新锁住玩家已持有的炸弹能力。非活动区域的 Echo/TV 不会抢占矿区 R 重置。博物馆的缩放及房间专用能力规则在离开时恢复。

原始地图保持未修改；博物馆的两个封闭端墙及全局雾仅在 MainGame 运行实例中抑制，主关卡补充两处门框。各房间的原有灯光与流程保留，矿区的哑光材质及玩家身体隐藏继续使用 MainGame 设置。

房间曝光随空间位置生效。`MiningCampaignRooms.cpp` 每次运行读取三个源地图的后处理设置，为其建立局部后处理组件，并在该次 MainGame 实例中关闭原来的全局后处理体积。室内体积按房间实际门槛限制 X 范围，边缘使用 1.5 米混合；它不会随小游戏激活状态开关。六处岩洞衔接各有独立的自适应曝光区，采用 EV100 -2 至 4、2 米边缘混合，并使用亮部测光与局部明暗压缩，避免室内固定曝光压黑低照度的矿区通路。该范围只控制曝光，不新增光源。源房间以后保存的新曝光设置仍会在下次运行 MainGame 时载入。

自然接口版本还会在源关卡 BeginPlay 后移除办公室原来的短方形门廊实例，并启用房间灯光的阴影，防止补光穿过土层照亮矿洞。该处理只作用于带有自然接口标记的 MainGame 运行实例，独立房间地图保留原样。

入口精修配置仍在 `Config/MainGameRooms.json` 的 `generated_connections` 与 `room_activation_x`，几何源由 `Scripts/main_game_room_entrances_geometry.py` 定义。一次性脚本是 `Scripts/refine_main_game_room_entrances.py`；它不会重建原有矿区或矿物。精修前备份与结果记录在 `Saved/Validation/MainGameRoomEntrances` 所指向的备份目录，修改后的接口使用 `Scripts/verify_main_game_room_entrances_saved.py` 与 `Scripts/verify_main_game_room_entrances.py` 验证，另继续运行原接口挖掘、重置及房间玩法检查。

一次性装配脚本为 `Scripts/integrate_main_game_rooms.py`，配置 `installed=true` 后禁止重复执行。首次完整备份：`Saved/Backups/MainGame_RoomIntegration_20261015-012746`，装配记录在 `Saved/Validation/MainGameRooms/authoring.json`。重新生成整个地形前应按当前平移和接口配置更新生成流程，避免使用早期四区坐标覆盖已整合地图。

验证入口：`Scripts/verify_main_game_linked_rooms.py` 检查原子关卡引用、六个可挖接口、办公室真实输入流程及共享经济；原生自动化 `BrainMining.MainGame.FullCardAndTelevisionEscape` 检查牌局胜利、涨水、失败重试、检查点及 15 段电视挖掘。镜像房间运行原生实体谜题验证。StartRoom、通路景观、实体泥土及保存碰撞回归脚本已支持新坐标。测试在 PIE 副本使用隔离入口和部分定位操作，不代表真人从出生点连续挖完整图。

首次装配验收（V3 改造前）：325 项脚本检查及原生牌局/电视完整流程测试通过；办公室内部 31 项、镜像谜题内部 35 项均无失败，四区 669 处通路采样及 40 处景观碰撞回归通过。历史记录为 `Saved/Validation/MainGameRooms/acceptance.json`，各回归报告快照在同目录 `Regression`，三个引用房间的运行画面为 `08_office_room.png`、`09_echo_cards_room.png`、`10_mirror_museum.png`。当前 V3 验收位于 `Saved/Validation/MainGameRoutesV3`。`Scripts/summarize_main_game_rooms.py` 汇总的是首次装配验收文件。

本轮自然接口精修验收：204 项脚本检查、六处新增岩壁的真实挖掘/恢复、三个房间同视角进入前后图像比较，以及原生牌局/电视完整流程测试均通过。平均亮度最大变化约 1.50%，办公室 31 项和镜像谜题 35 项内部验证无失败。最终记录为 Saved/Validation/MainGameRoomEntrances/acceptance.json，汇总入口为 Scripts/summarize_main_game_room_entrances.py。上述 325 项记录属于初次房间整合，本段记录对应最新入口及曝光精修。

接口地面由原有矿区实际碰撞采样后生成缓坡，六处旧不可挖外壳开口及封边同步适配；新增可挖岩壁采用与原生地形一致的面朝向，外壳在运行时启用双面碰撞。三个后续矿区的复位落点也对齐实际地面，矿区内部路线起终点保持不变。复用几何模块的重建脚本会刷新模块缓存。维护接口时依次使用 fit_main_game_natural_cave_floors.py、refit_main_game_natural_cave_solids.py、refit_main_game_entrance_soil.py、fit_main_game_natural_cave_backing.py，并重新验证保存状态和 PIE 通行。

局部曝光体积的父 Box 必须启用查询碰撞；仅设置无碰撞的空间 Box 无法实际贡献后处理。这里使用 OverlapAllDynamic 且关闭重叠事件，在保持可通行的同时提供后处理范围查询。四个原矿区可挖主体、165 处矿物及 187,491 个碎晶布局未重建，三个源房间地图的文件哈希与本轮修改前一致。

本轮修正了房间前后通路被室内固定曝光压黑的问题：室内范围收回实际门槛，六处接口独立自适应曝光，并结合亮部测光和局部明暗压缩，同时保留暗处岩壁及亮房间的细节。重新启动编辑器后，108 项脚本检查、24 个双向/挖掘前后视角、三个室内同视角激活对比，以及原生牌局/电视流程均通过。三个室内激活前后亮度最大变化约 1.19%。原四矿区主体三角数、32 处引导层和矿物布局校验保持一致。验收记录为 Saved/Validation/MainGamePassageExposure/acceptance.json；验证入口为 Scripts/verify_main_game_passage_exposure.py 和 Scripts/measure_main_game_passage_visibility.ps1。
