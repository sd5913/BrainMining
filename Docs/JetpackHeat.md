# 喷气背包过热条

按住空格使用项目已有的喷气背包。持续推力默认 5 秒从白色空条填满；松开时逐渐降温，满热后整条变红，停止上升推力并正常下落。过热锁定 3 秒，期间松开、重复按空格都无法绕过。恢复时清空热量，空格仍按住则继续飞行。

过热条位于 `Content/UI/MiningGUI/WBP_MiningHUD` 的 `JetpackHeatBar`，在 G/V 图标下面。可在 Designer 中移动、缩放。独立子控件 `WBP_JetpackHeat` 内的 `HeatBar` 图片使用 UI 材质 `MI_JetpackHeat`，可改绿色、黄色、红色。`PreviewHeat`（0–1）和 `bPreviewOverheated` 可预览各状态。没有添加数字或字体。

实际飞行设置来自关卡里唯一的 `MiningTunnelGenerator` 的 **Flight Settings**。没有生成器的关卡可调整 Character 的 Character Movement → Flight Settings。

已开启 `BP_ExternalMiningTestCharacter` 和 `BP_FirstPersonCharacter` 中已有的 Hold-to-Fly 设置，使当前外部挖掘关卡和挖掘实验关卡也能使用空格飞行。角色原有飞行速度和所有镐子、双钻机组件位置均保留。

- `Enable Heat`：是否启用过热。
- `Heat Up Seconds`：持续使用多久满热，默认 5 秒。
- `Passive Cool Seconds`：松开后从满条冷却到空条所需时间，默认 2 秒。
- `Overheat Lock Seconds`：满热后禁止使用时间，默认 3 秒。

热量由移动组件维护，UI 只显示 `JetpackHeat` 与 `IsJetpackOverheated()`。关闭或更换 UI 不会解除冷却，也不影响原有飞行的碰撞、速度、玩家缩放和钻机冷却。

`SourceAssets/JetpackUI` 保留 G.zip 的原始两张图片；材质通过 UV 选取有效条形区域，不改变图片源文件。彩色图保留作调色参考，填充采用白色分段形状和绿色→黄色→红色的可编辑颜色。

`Scripts/create_jetpack_heat_ui.py` 只增加新子控件与新资产，已有布局和道具面板保留。编辑前的文件备份位于 `Saved/Jetpack/Backup`。
