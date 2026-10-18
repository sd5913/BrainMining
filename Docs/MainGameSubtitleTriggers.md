# MainGame 字幕触发子关卡

打开 `/Game/MainGame/Maps/Lvl_MainGame`，在“窗口 → 关卡”面板中找到
`Lvl_MainGame_SubtitleTriggers`，右键选择“设为当前关卡”。保持 MainGame
及其源房间可见，即可对照完整场景摆放字幕触发盒。

该子关卡使用 **Always Loaded**，位置和旋转偏移为零、缩放为一。
它只保存字幕触发盒，不包含主场景的复制品。原 MainGame 中的
`字幕触发_StartRoom` 已迁入，字幕 ID、世界位置和触发范围保持不变。
电脑房源关卡自带的 `0001` 触发盒仍归其源关卡管理。

字幕工具新放置的 Actor 会进入当前编辑关卡。每次打开项目后，请先确认
当前关卡是 `Lvl_MainGame_SubtitleTriggers`，并在大纲的“关卡”列检查归属。
日常摆放触发盒后，保存并提交该子关卡即可。

首次安装修改 MainGame 是为了保存子关卡引用并移出原有触发盒。
协作者需要同时同步 MainGame 和新子关卡。后续新增其他子关卡或修改流送配置，
仍可能再次修改 MainGame。

如需在另一个检出中重建此结构，可在已保存、停止试玩的编辑器中运行
`Scripts/create_subtitle_trigger_sublevel.py`。脚本可以重复运行，不会复制已有触发盒。
