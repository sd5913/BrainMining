# MainGame 四段普通可挖岩层材质

关卡：`/Game/MainGame/Maps/Lvl_MainGame`。材质目录：`/Game/MainGame/Materials`。

- `MI_01_Silica`：通往电脑房，冷灰色平滑硅质岩，低对比矿物云纹、克制的哑光。
- `MI_02_DreamMineral`：通往盲人敲地与坠落区域，蓝紫色平滑岩面、轻微青灰矿脉。没有自发光，也尚未添加孢子。
- `MI_03_MuseumCrystal`：通往博物馆，暖灰与米白色平滑石材，柔和细脉和温润光泽。
- `MI_Guide_04_RoughStone`：第四段通往结局细胞房的普通岩壁，复用此前第四段引导层的红褐色岩石材质，带凹凸与深色纹理。旧 `MI_04_OrganicFlesh` 资产保留，已不用于该段墙壁。

四个实例共用 `M_MainGameStageRock`，在世界坐标中计算纹理，适用于动态挖掘网格和新产生的切面，不依赖模型 UV。普通岩层保留原有 Lowpoly 几何，没有位移或碰撞变化。相邻房间预留连接区沿用前一段主题；不可挖外壳保留原有材质。

64 个引导阻塞层已换为 `Art/MainGame/BlockersRockV2` 中的闭合岩石塞，沿用原先的通路路径、数量、变换和终点留空。两端为凹凸真实几何，边缘带沿壁延伸的石舌；四段填充体积分别为 47.01%、46.62%、45.13%、46.24%。每段仍为 16 个独立可挖对象，各自导出到一个 `Tunnel_Blockers.fbx`，切割体没有改变。

所有 64 个引导层恢复为原来的 `/Game/Campaign/Materials/M_CampaignGuideRock` 泥土材质。四个 `MI_Guide_*_RoughStone` 资产保留，其中第四个用于第四段墙壁。堵塞块端面的凹凸与沿壁伸展仍为真实几何，材质切换未改变模型或挖掘状态。

移除了 MainGame_V2/V3/V4 下的九盏无阴影区域点光源。入口/出口灯和玩家头灯保留。没有修改其他关卡、公共材质或角色蓝图。

实例可调整 Tint、MineralLight、Accent、Roughness、Specular、PatternScale、DetailScale、BumpDepth、VeinStrength。PatternScale 与 DetailScale 是每厘米的空间频率；当前所有主题的自发光为零。前三段普通岩层 Theme 取 0–2，第四段取 4，不建议修改。当前引导层使用独立的原泥土材质。

配置：`Config/MainGameRockThemes.json`。着色代码：`Art/MainGame/Materials/StageRockPattern.hlsl`。普通材质应用脚本：`Scripts/update_main_game_rock_themes.py`。材质源代码修改和去除灯光：`Scripts/revise_main_game_stone_surfaces.py`。阻塞块生成：`Art/MainGame/Materials/build_rough_guides.py`；导入并通过独立外部工具替换：`Scripts/replace_main_game_rough_guides.py`。在当前 MainGame 编辑器中执行 UE 脚本；不要同时用另一进程保存正在加载的资产。

操作前地图备份：`Saved/Backups/MainGame_BeforeRockThemes/Lvl_MainGame.umap`。验证报告：`Saved/Validation/main_game_rock_themes.json`。

本次操作前备份：`Saved/Backups/MainGame_BeforeSmoothRockRoughGuides`，包括地图和原材质目录。验证报告：`main_game_smooth_stone_revision.json`、`main_game_rough_guides.json`、`main_game_rough_guides_runtime.json`（均在 `Saved/Validation`）。四段均通过游戏内碰撞命中、实际挖掘、恢复初始几何和材质保留检查；运行检查不会保存游戏中的测试挖掘。

2026-10-14 材质分配调整：`Scripts/apply_main_game_mud_guides_stone_wall.py`。备份在 `Saved/Backups/MainGame_BeforeMudGuideStoneWall`，包含调整前地图与配置；验证报告为 `Saved/Validation/main_game_mud_guides_stone_wall.json`。配置中的 `guide_material` 同时用于后续阻塞块重建，防止再次生成时恢复为粗石材质。
