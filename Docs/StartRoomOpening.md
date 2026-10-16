# MainGame 开始房间开场

打开 `/Game/MainGame/Maps/Lvl_MainGame` 运行。玩家坐在床上看到提供的游戏标题、“开始游戏”和“退出游戏”。点击开始后，用 3.2 秒起身，同时标题飞到房间出口门面；随后恢复移动、视角、碰撞和游戏 HUD。

本次以 GitHub 最新 MainGame 为基础接入，保留协作者的地图、通路、音频和玩法更新。MainGame 区域初始化完成后，从 `MiningStartRoom` 的实际世界变换计算床边和门口位置，创建一次 `MiningOpening`。地图已有手工放置的开场 Actor 时沿用它及其参数。独立 StartRoom 与其它地图不自动播放开场。

标题资源为 `/Game/StartRoom/UI/T_OpeningTitle`，原图和透明处理版本在 `SourceAssets/Opening`。打包配置包含标题资源目录。无需手动运行安装脚本或覆盖 MainGame 地图。

验证：`BrainMining.Opening.Bootstrap` 检查最新地图无需预放开场对象、房间变换、资源加载、区域归属、防重复和独立关卡隔离；`BrainMining.Opening.BedToDoor` 在实际 MainGame 中检查菜单按钮、起身、标题落点以及控制权恢复。
