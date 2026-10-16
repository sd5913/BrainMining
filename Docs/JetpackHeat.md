# 喷气背包热量仪表

当前 UI 为屏幕下方正中央的细弧形仪表槽，图层不透明度为 80%，平时隐藏。按住空格使用原有喷气背包时短暂淡入，松开后热量沿槽逐渐回落；降温期间继续显示，清空后停留 0.25 秒，再用 0.35 秒淡出。淡出未结束又使用背包时，从当前透明度平滑恢复。

弧形槽为空时呈深灰，填充沿上拱弧线从左往右推进，降温时向左回退。前 75% 为黄色，最后 25% 为红色，颜色直接分区，没有绿黄红渐变。进入红区后用提供的 G (1).png 显示轻微全屏警戒效果，离开红区后渐隐。警戒图保留原始 RGBA 与透明中心，不遮住鼠标操作。

持续推力默认 5 秒满热；松开后从满热冷却至零默认 2 秒。满热后保留原有 3 秒喷气锁定，填充在这 3 秒内回落，整个冷却期间无法喷气，松开或重复按空格均无法跳过。普通跳跃仍可使用。热量与冷却由移动组件管理，UI 不改变这些规则。

短按空格现在只普通跳跃；长按 0.22 秒或短按松开后 0.30 秒内再按才点火。只在实际点火后累计热量和触发仪表淡入，松手滞空不产热。过热会清除上升惯性及滞空，冷却后持续按住可重新从低推力起飞。具体手感参数见 [跳跃与喷气背包](../README_PlayerFlight.md)。

## 在 UE 中调整

- Content/UI/MiningGUI/WBP_MiningHUD → JetpackHeatBar：Designer 中调整位置、锚点和大小。默认锚点 (0.5, 1)，对齐 (0.5, 1)，位置 (0, -40)，画布 300×120。保持 2.5:1 的比例可维持当前弧线形状。
- WBP_JetpackHeat → HeatBar：可编辑独立子控件；Appearance → Render Opacity 默认 0.8；Class Defaults → GUI / Jetpack Heat 可改淡入、淡出、停留、警戒阈值和警戒透明度。
- JetpackGauge/MI_JetpackArc：YellowColor、RedColor、TrackColor、OutlineColor；Radius、Thickness、SweepDegrees 决定弧槽形状。Thickness 默认 0.035，比上一版更细。运行中的 WarningThreshold 来自子控件默认值，默认 0.75。
- WBP_MiningHUD → JetpackHeatWarning：全屏警戒图层，默认上下左右全屏锚点，Z Order=-100，位于主界面的金币、技能、热量仪表和升级弹窗下方。运行时透明度自动控制，Hit Test Invisible 不拦截输入。Art/T_JetpackHeatWarning 可重导入原图，源文件 SourceAssets/JetpackUI/JetpackHeatWarning.png。
- 子控件的 PreviewHeat（0–1）与 PreviewOverheated 可在 Designer 中预览填充及锁定边缘。没有新增数字或字体。

运行时间参数：GaugeFadeInSeconds=0.16、GaugeFadeOutSeconds=0.35、HideDelaySeconds=0.25、WarningFadeInSeconds=0.25、WarningFadeOutSeconds=0.45、WarningMaxOpacity=0.55。原图自身透明度最高约 0.47，实际屏幕叠加会比参数值轻。

实际飞行设置来自关卡唯一的 MiningTunnelGenerator 的 Flight Settings；没有生成器的关卡可改角色 Character Movement → Flight Settings。Enable Heat、Heat Up Seconds、Passive Cool Seconds、Overheat Lock Seconds 均保留。

## 资产和验证

Scripts/create_jetpack_arc_ui.py 创建弧槽材质、导入原始警戒图，更新现有热量子控件及 HUD，仅显式运行时改变槽位置。运行时代码不重建布局，四级升级图层和 Tab 交互保留。

Scripts/verify_jetpack_arc_ui.py 在最新主线关卡发送真实空格输入，验证显隐、75% 警戒、松开后的回落、警戒淡出、过热锁定、重新使用时淡入反转以及 Tab 交互，保存 Saved/Validation/JetpackArc 的报告与游戏截图。

修改前的源文件与 UI 资产备份在 Saved/Jetpack/TransientGaugeBackup；上一版四级升级和横向冷却条已提交为 51542b6。横向材质和原图仍保留在资产目录中以便回退。

设计参考：用户提供的《燕云十六声》截图与按需显隐要求；其他 3D 游戏的自动 HUD 设计可见 [《艾尔登法环》官方指南](https://en.bandainamcoent.eu/elden-ring/news/elden-ring-starter-guide-tips-know-playing-the-game)。本项目淡入淡出时长可编辑。
