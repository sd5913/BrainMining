# 钻机能量和鼠标

钻机 V 可随时拿起或收起。拿起不会自行挖掘；按住左键消耗能量，松开后自然充能，切到其他道具同样充能。耗尽自动收起，恢复能量后即可再次拿起，没有使用倒计时或冷却倒计时。菜单占用输入时暂停资源变化。

当前 C++ 基础参数为 36 容量、每秒消耗 10；恢复速度为 DrillRecoveryRate × 0.15，默认每秒恢复 1.5（2026-10-26 再降为此前每秒 3 的 50%）。主角蓝图可覆盖容量和配置值；升级后的恢复速度也应用相同倍率。可在主角 Mining 组件的 Mining / Drill 分类调整 MaxDrillEnergy、DrillConsumptionRate、DrillRecoveryRate；旧的两个 Duration 参数仅用于旧关卡兼容，不再影响能量模式。

四级升级沿用 MiningProgressionComponent：恢复 ×1.5（20,000）、容量 ×1.5（100,000）、消耗 ×0.75（500,000）、无限能量（2,000,000）。

最后 25% 能量显示红色警戒状态，30% 到 25% 之间平滑过渡。恢复充能后反向过渡回青色。

`WBP_MiningHUD` 的 `DrillEnergyBar` 是独立可编辑 Image，默认右边居中，56 ×196，距右边 38。材质 `UI/MiningGUI/DrillEnergy/M_DrillEnergy` 的 Energy 参数由实际资源驱动，自下而上充能，自上而下消耗。未充满的格子保留暗色槽。拿起钻机时以 0.16 秒淡入，收起或切到其他道具时以 0.35 秒淡出，缓动方式与喷气背包一致；交互和菜单期间也会淡出。能量条隐藏期间照常充能。

小镐子鼠标原图保存在 `SourceAssets/DrillEnergy`。游戏硬件鼠标使用 `Content/Slate/Cursors/PickaxeCursor.png` 及 1.5×、2×版本，40像素基础大小，上方右侧镐尖为点击热点。DefaultEngine.ini 配置 Default 与 Hand 两种形状；仅游戏视口使用，编辑器保持自己的鼠标。Slate 原始 PNG 随游戏打包。

导入脚本只添加能量 Image，不重建已有 HUD。炸弹位置、三个准心的大小和其他原有槽位保持用户编辑的值。
