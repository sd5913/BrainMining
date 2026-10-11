# 喷气背包过热条

按住空格使用项目已有的喷气背包。持续推力默认 5 秒从白色空条填满；松开时逐渐降温，满热后整条变红，停止上升推力并正常下落。过热锁定 3 秒，红色热量条从右往左逐渐退回白色，退回期间始终无法起飞，松开、重复按空格都无法绕过。红色完全退完才恢复飞行，空格仍按住则继续飞行。

过热条位于 `Content/UI/MiningGUI/WBP_MiningHUD` 的 `JetpackHeatBar`，在 G/V 图标下面。可在 Designer 中移动、缩放。独立子控件 `WBP_JetpackHeat` 内的 `HeatBar` 图片使用 UI 材质 `MI_JetpackHeat`，可改 ColdGreen（绿）、WarmYellow（黄）、HotRed（红），也可调 Saturation（饱和度，默认 1.1；1 为原色，越大越鲜艳）。材质输出的透明度通道只读取 Alpha；白色原图的黑色段间隙转换成透明遮罩，保留分段形状，避免颜色发灰。`PreviewHeat`（0–1）和 `bPreviewOverheated` 可预览各状态。没有添加数字或字体。

实际飞行设置来自关卡里唯一的 `MiningTunnelGenerator` 的 **Flight Settings**。没有生成器的关卡可调整 Character 的 Character Movement → Flight Settings。

已开启 `BP_ExternalMiningTestCharacter` 和 `BP_FirstPersonCharacter` 中已有的 Hold-to-Fly 设置，使当前外部挖掘关卡和挖掘实验关卡也能使用空格飞行。角色原有飞行速度和所有镐子、双钻机组件位置均保留。

- `Enable Heat`：是否启用过热。
- `Heat Up Seconds`：持续使用多久满热，默认 5 秒。
- `Passive Cool Seconds`：松开后从满条冷却到空条所需时间，默认 2 秒。
- `Overheat Lock Seconds`：满热后禁止使用时间，默认 3 秒。

热量由移动组件维护，UI 只显示 `JetpackHeat` 与 `IsJetpackOverheated()`。关闭或更换 UI 不会解除冷却，也不影响原有飞行的碰撞、速度、玩家缩放和钻机冷却。

`SourceAssets/JetpackUI` 保留 G.zip 的原始两张图片；材质通过 UV 选取有效条形区域，不改变图片源文件。彩色图保留作调色参考，填充采用白色分段形状和绿色→黄色→红色的可编辑颜色。

`Scripts/create_jetpack_heat_ui.py` 只增加新子控件与新资产，已有布局和道具面板保留。编辑前的文件备份位于 `Saved/Jetpack/Backup`。

Scripts/update_jetpack_cooling_ui.py 只更新现有热量材质，保留条形外观与 HUD 布局。Scripts/verify_jetpack_heat.py 在最新关卡中验证空格飞行、满热阻断、3 秒逐渐退回、冷却后恢复，并保存实际游戏截图。修改前的源文件和现有 UI 资产备份位于 Saved/Jetpack/FourLevelsCoolingBackup。仪表盘方案尚未实施。
