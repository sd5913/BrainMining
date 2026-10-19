# MainGame 字幕触发子关卡

打开 `/Game/MainGame/Maps/Lvl_MainGame`，在“窗口 → 关卡”面板中找到
`Lvl_MainGame_SubtitleTriggers`，右键选择“设为当前关卡”。保持 MainGame
及其源房间可见，即可对照完整场景摆放字幕触发盒。

该子关卡使用 **Always Loaded**，位置和旋转偏移为零、缩放为一。
它只保存字幕触发盒，不包含主场景的复制品。原 MainGame 中的
`字幕触发_StartRoom` 已迁入，字幕 ID、世界位置和触发范围保持不变。

## MainGame 字幕表

字幕子关卡包含五个位置触发盒：StartRoom、离开初始房、卡牌玩法1、文思玩法1、文思玩法3。
其余字幕由同一子关卡的 `字幕事件_MainGame` 管理。每次游戏运行中，各事件段只排队一次；
重新进入房间、重试水涌或重复回声不会反复插入同一句旁白。

电脑房四段分别接在办公开始、弹窗/纸张喷发、炸弹道具喷出和离开房间强制回头处。
回声房段在玩家首次落地后播放。卡牌结束段接在胜利事件上，水涌开始时排队播放水涌段，
因此它会等卡牌结束段播完。镐子四段只在成功购买对应等级时触发，读取已有等级或购买失败不触发。

博物馆入口触发后，玩家在博物馆范围内的停留时间累计达到 300 秒、且尚未进入终点小房间，
才播放文思玩法2。离开博物馆和游戏暂停不计时；进入终点小房间后不再产生提示。
终点盒位于鼠标画作小房间内部，需要穿过缩小通道才会触发。

17 个字幕资产保存于 `/Game/Subtitles/DA_*`，均未指定音频，自动使用字幕系统的程序化语音。
事件管理器引用全部资产，保证打包 MainGame 时包含这些字幕。旧 `DA_0001` 和电脑房中的
`字幕触发_0001` 已删除。短暂的匹配状态提示不会打断表格字幕或清空其播放队列。

在 PIE 停止时通过 Unreal MCP 控制台运行 `py H:/PolyU/Spotlight/BrainMining/Scripts/install_main_game_subtitle_plan.py`
可重复应用内容和摆放。它仅保存字幕子关卡、字幕资产以及删除旧触发盒所需的电脑房源地图；
不会重新保存 MainGame 主地图。

运行验证脚本 `Scripts/validate_main_game_subtitle_plan.py` 会读取保存后的内容并启动 PIE，
在 PIE 副本中检查位置重叠、程序化语音、回声落地、卡牌和水涌的事件调度、博物馆计时与终点、
四次真实镐子升级购买和办公开始事件。结果位于 `Saved/Validation/SubtitleTriggers/plan_runtime.json`。

2026-10-18 验证：Editor Development 构建成功；MainGame PIE 上述检查全部通过；
`GeneratedVoice.Timing`、`GeneratedVoice.Variation`、`SharedStyle`、`ConfigPersistence` 四项字幕自动化测试通过。
`字幕事件_MainGame` 带有 `BM_PlayerEffect` 标记，避免区域管理器因其跨房间位置而暂停计时。
电脑房源关卡自带的 `0001` 触发盒仍归其源关卡管理。

字幕工具新放置的 Actor 会进入当前编辑关卡。每次打开项目后，请先确认
当前关卡是 `Lvl_MainGame_SubtitleTriggers`，并在大纲的“关卡”列检查归属。
日常摆放触发盒后，保存并提交该子关卡即可。

首次安装修改 MainGame 是为了保存子关卡引用并移出原有触发盒。
协作者需要同时同步 MainGame 和新子关卡。后续新增其他子关卡或修改流送配置，
仍可能再次修改 MainGame。

如需在另一个检出中重建此结构，可在已保存、停止试玩的编辑器中运行
`Scripts/create_subtitle_trigger_sublevel.py`。脚本可以重复运行，不会复制已有触发盒。
