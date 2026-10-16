# MainGame 立体通路与景观

此文档记录 V2 历史布局。当前 MainGame 已采用分区特色路线，见 [MainGameRoutesV3.md](MainGameRoutesV3.md)；当前源目录、端点与房间位置以配置文件为准。

新源模型在 `Art/MainGame/RoutesV2`；原四区模型保留。当前使用的源目录由 `Config/MainGameRoutes.json` 指定。

四区内部路线的局部入口和出口保持固定。三个小游戏整合后，四个挖掘区与 StartRoom、8 米喷气竖井整体平移，原三处 18 米预留连接区由房间引用及六个可挖接口替代；当前世界坐标及管理器 Entry/Exit 见 `Config/MainGameRooms.json`，接入说明见 `Docs/MainGameRooms.md`。各区入口前 9 米及出口前 12 米沿用原中心线与截面。内部主路约 289–297 米，累计上升约 19 米，包含上升洞厅、一圈下降约 22 米的螺旋、横向回折和四条弯曲支路；整区净下降仍为 55 米。

每区有两处开阔空间，第一段约 16.4 米宽，溶洞与神经岩区约 20 米宽，石笋区约 19 米宽。溶洞布置 12 根通顶岩柱，石笋区 20 根石笋，神经岩区 8 组带分枝的岩石。景观在选择位置时避开主路中央，并保持邻近实体之间的间隔。它们通过从通路切割体扣除实体体积形成，属于可挖岩层，随原生 R 恢复。

源模型每区有 16 个堵塞体槽位。2026-10-15 调整后，各区交错保留 8 个，合计 32 个；入口教学堵塞层单独保留。禁用项保存在原生 GuideLayers 配置内，R 重置不会重新生成被移除的层。大洞厅完整留空，保留的堵塞层位于相邻收窄段。弯曲堵塞体边缘嵌入普通岩层；源模型的封闭/封堵检查仍是原始 16 槽位版本的几何证据，当前实际堵塞层数量由保存关卡和运行检查确认。支路只在不可挖外壳以内延伸。

当前共 165 处矿物：普通可挖层 145 处，引导层 20 处，包含 44 处洞顶矿物。相对 10-14 版本，每区新增 10 处普通侧壁矿物，原 125 处奖励配方仍保留，升级价格不变。新增使用各区最低阶、最低面额宝石，额外可得 302,020 金币（原总预算 10,759,329 的约 2.8%）。四区总量分别为 62、55、22、26。完整埋藏、表面碎晶、内密外疏和挖出后收取逻辑继承原系统；当前统计在 `Saved/Validation/MainGamePolish/minerals.json`。重新生成地形或堵塞层后必须重建矿物 Host 引用。

泥痕影响范围现为每区 16 段，跟随 8 个保留堵塞层。四区实体泥土的几何与增加 30% 的侧壁、顶部厚度保留；StartRoom 接口加宽后重新布置 38 块实体泥土，贴合新的侧壁和顶板。普通岩石改为棕灰色，引导层、泥痕和凸起统一使用较深泥土与斑块状苔藓。MainGame 独立父材质的 Roughness 固定 1，Specular、Metallic、Emissive 固定 0；岩壁、堵塞层、不可挖外壳和三处连接区均使用此材质族。

通路固定灯光共移除 13 处，包括入口/出口、竖井/连接段和四区洞厅补光。保留玩家头灯、矿物本身的挖出提示，以及原 StartRoom 的 5 处灯光。

StartRoom 至区域一连接口：最后 7.3 米改为 6.4 米宽、4.8 米高的过渡口。原窄管侧壁、顶板和装饰边框在此段撤出，入口土层加宽至 6.8 米并与外侧硬边界重叠；玩家可向左右挖开出口。区域一不可挖外壳的局部开口同步扩宽，外围四块封边及新过渡段的地面、侧壁、顶板和肩墙保持实体碰撞，防止出界。普通地形源、165 处宝石布局及其 Host 引用保持不变。配置为 `Config/MainGameJunction.json`，一次性编辑脚本为 `Scripts/widen_main_game_junction.py`，局部外壳修改需要在整体重新生成矿区后重新应用；当前无需重新生成。备份与记录在 `Saved/Validation/MainGameJunction/authoring.json`。

接口加宽后的验收共 121 项：79 项初始房间至矿区完整流程、33 项实体土块、9 项保存碰撞检查通过。实测横向 -240、-120、0、120、240 cm 五条胶囊路线，清除土层后可通行，原生 R 在五处均恢复封堵；入口自然岩石边缘可用常规尺寸挖掘继续拓宽。52 处外围封边采样通过。报告与截图索引在 `Saved/Validation/MainGameJunction/acceptance.json`，修改前地图备份为 `Saved/Backups/MainGame_WideJunction_20261015-005239`。

## 重建与验证

1. 用后台 Blender 运行 `Scripts/build_main_game_routes_v2.py`，得到四区中心线、FBX、Blender 源文件及封闭/封堵检查结果。可用 `--stage=TunnelV1` 等参数单独生成一区。
2. 停止 PIE，打开 MainGame，执行 `Scripts/apply_main_game_routes_v2.py`。脚本先保存并完整备份地图及泥土材质，再调用原生外部通路生成器；每完成一区保存并写入进度日志，支持中断后继续。
3. 执行 `Scripts/finish_main_game_routes_v2.py` 更新材质范围并烘焙实体泥土；禁止在已烘焙版本上重复执行。
4. 对新生成的旧外观地形执行 `Scripts/apply_main_game_polish.py`（一次性进度日志，不可在已完成版本上重复应用），按 `Config/MainGameTuning.json` 减半堵塞层并调整材质和灯光；然后执行 `Scripts/rebuild_main_game_dense_minerals.py`，通过埋藏、线索和密度检查才替换原 Field 并保存。原 `rebuild_main_game_route_minerals.py` 为 125 处历史版本，不应覆盖当前布局。
5. 执行 `Scripts/verify_main_game_routes_v2.py` 检查保存重载、固定端点、胶囊通路、地面与顶壁、景观碰撞、挖掘及重置，并保存 PIE 景观截图。原 StartRoom 流程检查和 3D 土块检查继续适用。

编辑器生成进度、备份路径和验证结果在 `Saved/Validation/MainGameRoutesV2`。旧源地图、FullJourney、独立 StartRoom 和共享角色均作为受保护资产核对哈希。

10-15 本次调整的材质源码是 `Art/MainGame/Materials/DryRockMoss.hlsl`，配置为 `Config/MainGameTuning.json`。进度、矿物分配和保存核对在 `Saved/Validation/MainGamePolish`；修改前完整备份为 `Saved/Backups/MainGame_Polish_20261015-001103`。

2026-10-15 调整后验收：40 项路线检查、33 项实体土块检查、66 项入口/经济流程检查、45 项保存状态检查全部通过，共 184 项。仍通过 669 处通路采样和 40 处景观碰撞检查，矿物 165 处通过完整埋藏与碎晶检查，受保护资产哈希一致。当前验收汇总为 `Saved/Validation/MainGamePolish/acceptance.json`；下方 10-14 记录为历史版本。

随后苔藓外观精修：去掉按土块单元分配的离散底色，土色改为连续变化；苔藓使用旋转、扭曲的多尺度噪声，混入细碎纹理并降低绿色对比度，边缘渐淡。覆盖堵塞体、壁面泥痕及实体凸起，保持全哑光。`Scripts/refine_main_game_moss.py` 只更新现有材质族，`Scripts/preview_main_game_moss.py` 在四区固定位置保存堵塞体正面与泥土凸起的前后对比；结果在 `Saved/Validation/MainGameMoss`，材质编译无错误，地图内容哈希在此次材质修改前后相同。备份为 `Saved/Backups/MainGame_Moss_20261015-003948`。

2026-10-14 最终验收：40 项新路线检查、33 项实体泥土检查和 66 项 StartRoom/经济流程回归全部通过，共 139 项。四区 669 处胶囊通行与顶底采样、三处连接区的连续地面，以及全部 40 处景观实体碰撞均通过。原经济配方与受保护资产哈希不变。验收记录为 `Saved/Validation/MainGameRoutesV2/acceptance.json`；修改前完整备份为 `Saved/Backups/MainGame_RoutesV2_20261014-232623`。

通行检查覆盖规划路线的几何采样，不等同真人连续挖完整图。全图碰撞审计在 PIE 副本中临时关闭附近加载，检查结束恢复；StartRoom 完整流程和实体泥土验证使用原有附近加载。截图在当前临时 `-noraytracing` 编辑器会话采集，项目渲染配置未修改。
