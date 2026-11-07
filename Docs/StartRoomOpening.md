# MainGame 开始房间开场

打开 `/Game/MainGame/Maps/Lvl_MainGame` 运行。玩家坐在床上看到提供的游戏标题、“开始游戏”和“退出游戏”。点击开始后，用 3.2 秒起身，同时标题飞到房间出口门面；随后恢复移动、视角、碰撞和游戏 HUD。

本次以 GitHub 最新 MainGame 为基础接入，保留协作者的地图、通路、音频和玩法更新。MainGame 区域初始化完成后，从 `MiningStartRoom` 的实际世界变换计算床边和门口位置，创建一次 `MiningOpening`。地图已有手工放置的开场 Actor 时沿用它及其参数。独立 StartRoom 与其它地图不自动播放开场。

开始菜单使用 `/Game/StartRoom/UI/T_OpeningTitle`，墙面材质 `M_OpeningTitleDecal` 独立使用 `/Game/StartRoom/UI/T_OpeningWallTitle`。两处均采用用户提供的图3配色（奶油色中文、亮金英文），1040×720 透明原图分别保存在 `SourceAssets/Opening/TitleTransparent.png` 和 `TitleWall.png`；保留原始像素，墙面投影按原图比例显示，并降低发光以保留颜色。菜单保留朝屏幕中央的透视；拾取矿镐后首次看向可挖墙面的显示条件不变。

主界面 Logo 的 `M_OpeningTitlePerspective` 材质带有轻微左下投影，使用按钮同款暖深棕色，方向背离画面中央光源。材质参数 `ShadowOffsetX/Y`、`ShadowOpacity` 和 `ShadowSoftness` 可调整偏移、强度和柔和度；阴影随 Logo 一起淡出，不作用于墙面。

资源已保存到项目，打包配置包含标题资源目录，无需手动安装或覆盖 MainGame 地图。需要重新导入时运行 Unreal Python 脚本 `Scripts/import_opening_logos.py`。`Scripts/verify_opening_logos.py` 在实际 MainGame 中检查菜单、正常矿镐拾取交互和墙面标题，并输出截图到 `Saved/Validation/OpeningLogos`；测试须使用 `-NoSteam -MiningQA` 和独立 `-UserDir`。

验证：`BrainMining.Opening.Bootstrap` 检查最新地图无需预放开场对象、房间变换、资源加载、区域归属、防重复和独立关卡隔离；`BrainMining.Opening.BedToDoor` 在实际 MainGame 中检查菜单按钮、起身、标题落点以及控制权恢复。
