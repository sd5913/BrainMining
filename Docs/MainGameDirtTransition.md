# MainGame 人物隐藏与泥土过渡

当前关卡已使用 `Docs/MainGameRoutesV2.md` 的立体通路。新岩壁共重烘焙 1,380 块实体土块，入口保留 22 块，共 1,402 块；泥土材质统一和侧壁、顶部增加 30% 厚度的规则继续保留。当前几何与检查在 `Saved/Validation/MainGameRoutesV2/Clay`，下文中的早期数量和报告是升级前的历史记录。

MainGame 专属角色蓝图的两个骨骼身体组件设置为游戏中隐藏，并关闭身体阴影。隐藏设置不向子组件传播，摄像机、镐子、钻机、炸弹以及角色碰撞继续工作；隐藏时仍更新骨骼，保持头部摄像机插槽稳定。共享角色、FullJourney 和独立 StartRoom 未修改。

四个挖掘区共 64 个堵塞块，以及 StartRoom 新连接处使用 `MainGame/Materials/DirtTransition/MI_ClayBlockage`。堵塞块周围增加了真正的低多边形薄土块：四个矿区共 1,332 块，与可挖岩壁做布尔并集；入口另有 22 块可挖土块，使用独立的原生 GeneratedMiningRock 保存。原 64 个堵塞核心保持原形状与分块，普通岩壁网格增加约 8.5 万三角形。土块半径约 13–34 cm，侧壁与顶部外凸已增加约 30%，现在约 8–23 cm；地面附近保持原有限高，避免挡路。四区各自保留原有岩壁主题。

立体土块具有真实网格轮廓、侧面厚度和碰撞。矿区土块属于承载岩壁的同一可挖网格，按现有镐子、钻机和爆破布尔逻辑一起挖除；R 使用更新后的源网格恢复。入口土块也可挖、可重置，后面的不可挖封口仍保留。土块没有使用始终悬浮的装饰 Actor。实体、平面泥土斑块和堵塞核心统一使用同一泥土颜色函数、纹理尺度、粗糙度及镜面反射；斑块内部不再混入岩壁底色，仅边缘保留抗锯齿过渡。连接通道木框沿用原木材材质。

材质实例中 `MudReach` 控制薄泥痕延伸距离（cm），`MudCellSize` 控制材质碎块尺度，`MudDepth` 控制细微法线凹凸，`MudDark` / `MudLight` 控制颜色。实体厚度和尺寸由烘焙脚本控制，顶点颜色红减绿的差值标记泥土实体；其余岩壁保留原配色。四区每区 32 段范围覆盖其 16 个堵塞块。

统一材质及加厚脚本为 `Scripts/refine_main_game_clay.py`：在当前岩层中只沿原土块法线拉伸已有泥土顶点，保持三角形数、网格连通性和分块边界；地面土块不增高。`MudSpecular` 统一泥土镜面反射，原岩壁区域继续使用自身 Specular 参数。新备份及验收信息见 `Saved/Validation/MainGameClayRefinement`。脚本带防重复标记，避免重复加厚。

生成脚本：`Scripts/apply_main_game_dirt_transition.py`。材质先在空场景构建，表达式及实例参数批量设置后只刷新一次，以降低大型关卡的内存峰值；随后加载 MainGame 赋材质并保存。源码在 `Art/MainGame/Materials/DirtTransition.hlsl`，原岩壁材质父资产和共享泥土材质保持不变。

实体烘焙脚本为 `Scripts/bake_main_game_3d_dirt.py`，专项检查为 `Scripts/verify_main_game_3d_dirt.py`，报告和侧面截图在 `Saved/Validation/MainGameDirt3D`。脚本根据实际碰撞表面定位，将封闭土块与 Actor 自有源网格合并；仅处理完全位于既有分块边界内的土块，因此保留空间索引及分块边界。入口通过项目原生封闭 OBJ 导入功能初始化可挖源网格。不要在已烘焙地图上重复叠加；需要重做时从 `Saved/Backups/MainGame_3DDirt_*` 恢复对应地图。重新生成矿区普通岩层会覆盖这些几何修改，需要重新烘焙。

验证脚本：`Scripts/verify_main_game_dirt_transition.py`，覆盖隐藏身体、工具显示与拾取、四区碰撞/挖掘/重置及 125 处矿物；截图及报告在 `Saved/Validation/MainGameDirt`。完整入口与竖井回归继续使用 `Scripts/verify_main_game_start_room.py`。本次编辑器验证因系统提交内存不足，使用临时 `-noraytracing` 启动参数，未修改项目渲染配置。

修改前备份在 `Saved/Backups/MainGame_DirtTransition_*`；最早的 `20261014-181237` 同时保留本次隐藏身体之前的 MainGame 地图和角色蓝图。

2026-10-14：42 项泥土及人物显示专项检查通过，入口、竖井、区域往返和经济系统的 64 项回归通过，共 106 项。连接封口的 52 个碰撞采样点通过。保存后的 64 个矿区堵塞块、新增入口堵塞层和 8 个入口岩壁组件使用新材质，9 个木框组件保持原木材；125 处矿物和所有受保护源资产保持不变。最终记录见 `Saved/Validation/MainGameDirt/acceptance.json`。

随后改为实体土块：保存并重载后，33 项实体几何检查及 66 项入口完整流程回归通过，共 99 项。五个代表点测得实际凸出约 12.8–16.3 cm，验证了碰撞、挖掉土块与支撑、源网格重置；入口还验证真实 R 恢复 22 块土块。52 处不可挖封口采样通过，125 处矿物及受保护源资产保留。新版本验收记录为 `Saved/Validation/MainGameDirt3D/acceptance.json`。

本次统一泥土材质并加厚后，保存重载的 33 项实体检查及 66 项流程回归再次全部通过。五个代表点实际凸出约 16.7–21.2 cm，相比修改前均增加约 30%；六个材质实例的泥土颜色、尺度、法线深度与镜面反射参数一致，使用同一父材质。地面限高、网格拓扑、125 处矿物和受保护源资产均保持。当前验收记录为 `Saved/Validation/MainGameClayRefinement/acceptance.json`，修改前完整备份为 `Saved/Backups/MainGame_ClayMatch_20261014-210345`。
