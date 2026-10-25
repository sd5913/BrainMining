# MainGame 房间背景音乐

2026-10-25：房间音乐接入 `/Game/MainGame/Maps/Lvl_MainGame`。后续按反馈移除电脑房新增音乐，卡牌对战保留房间原曲。

| 场景与状态 | 曲目 | 资源 |
| --- | --- | --- |
| 电脑房活动区域 | 不播放新增背景音乐 | — |
| 回声完成后的卡牌房探索、入座、匹配、对手到场、选择与攻击结算 | Enchanted Journey | M_Room_CardExplore_Loop |
| 逃生 Warning / Flooding，包括电视内部阶段 | Run Amok | M_Room_Escape_Loop |
| 博物馆及其鼠鼠画廊 | Meditation Impromptu 01 | M_Room_Museum_Loop |

五首均为 Kevin MacLeod 的作品，原曲从 Incompetech 官方目录与 MP3 地址下载，页面署名为 CC BY 4.0。原始 MP3、官方曲目元数据、页面证据及源文件与加工文件 SHA-256 位于 `SourceAssets/RoomMusic`。随包署名位于 `Content/RoomMusicCredits/CREDITS.txt`，列出作者、曲名、来源、许可证链接和修改说明。

## 播放规则

- 使用现有共享玩家 `UMiningAudioComponent`，按 Hall 的真实活动区域选曲，独立房间测试地图不在本次接入范围内。
- 回声通道未完成时不提前播放卡牌音乐；选牌与结算间保持同一个音乐组件，不在每手牌重播。
- 主流程 Dreamy Flashback 进入房间时淡出到零并保留播放进度，返回矿区时恢复音量。资源使用 PLAY_WHEN_SILENT，避免房间往返时反复停止、重新创建声音。
- 卡牌胜利淡出战斗音乐，给胜利音效留空；进入逃生警告阶段切入 Run Amok，警告到洪水不重新起播。
- 逃生失败淡出，重试恢复；成功离开逃生时淡出。跨区后释放停止的房间播放组件。
- 博物馆进入画中结局地图时，房间音乐随旧玩家销毁；结局既有 Light Thought var 1 不变。
- 所有新曲使用 SC_Music，服从总音量、音乐音量、全局音频开关及原有工具工作时的音乐压低规则。
- 只在第一次需要某首曲目时加载，导入资源使用 PRIME_ON_LOAD；已停止的播放组件销毁，声音资产由共享组件缓存。

## 音频加工与混音

保留原曲音乐内容，裁去首尾低于阈值的空白，转为 48 kHz 双声道 PCM16，制作 1.5 秒等功率循环交叠，循环端点各 5 ms 淡化。离线目标 RMS 0.13，峰值限制 0.85；动态较大的原曲受峰值限制不会强行压缩到目标 RMS。

运行时在原有 `bm.Audio.MusicVolume` 基础上进行平衡：卡牌房（含对战）×1.8、博物馆 ×2.5、逃生 ×1。保留原曲动态，并继续服从用户音乐音量设置。

资源目录 `/Game/Audio/RoomMusic` 已加入 AlwaysCook，署名目录已加入 NonUFS 随包文件列表。

## 重建与验证

1. `python Scripts/prepare_room_music.py` 下载缺失原曲并生成 WAV、manifest 和署名。
2. `Scripts/RoomMusic_Safe.ps1 -Mode Import` 导入五首 SoundWave，核对哈希、立体声、Music 分类与保存结果。
3. `Scripts/Build_Safe.ps1 -EngineRoot E:\UE5\UE_5.8 -MaxParallelActions 2` 编译。
4. `Scripts/RoomMusic_Safe.ps1 -Mode Test` 运行 `BrainMining.RoomMusic` 与既有 `BrainMining.AudioPilot` 回归。

新增自动化检查全部五首资源，以及共享玩家实际音乐组件在电脑房、回声、探索、战斗、胜利、逃生警告、洪水、失败、重试、逃离、博物馆和回矿区间的切换；同时覆盖音量归零恢复、全局关闭、组件释放和战斗回合间组件复用。测试为无声模式，不能替代混音试听或完整打包验证。

2026-10-25 验证完成：五首 SoundWave 导入成功，导入进程退出码 0；源 WAV 格式、哈希、峰值、首尾接点和非静音检查通过。编辑器 Development 编译成功（155.26 秒）；`BrainMining.RoomMusic` 两项与既有 `BrainMining.AudioPilot` 八项共 10 项测试全部通过，0 失败、0 测试警告。报告位于 `Saved/Validation/RoomMusicTests/index.json`，导入报告为 `Saved/RoomMusicImport.json`。尚未逐场景人工试听或验证本轮打包版。

后续反馈验证：`Saved/Validation/HUDMusic/Native/index.json` 共 11 项通过（包含完整卡牌与电视逃脱流程），0 失败；逃脱测试记录了网络探测超时和缺省完成存档不存在的警告。新音效 `/Game/Audio/Rooms/S_TVEscapeVictory` 来自用户提供的 `quotvictoriousquot-sound-effect.mp3`，长约 3.20 秒，在舞台彩带首次触发时播放一次，使用 SC_Effects。打包必须资源清单加入该音效及主流程音乐，房间音乐必需清单缩减为实际使用的三首。历史导入的办公室和战斗音频资产保留，但不再被播放路由选中。

实际运行验证：`Scripts/verify_hud_music_feedback.py` 使用隔离存档和明确的区域跳转测试，`Saved/Validation/HUDMusic/Runtime3/results.json` 的 17 项检查通过。电脑房、博物馆往返均保留主流程播放实例并恢复音量；游戏混音输出录音 RMS 分别约 0.0100、0.0108，确认非静音。升级面板普通/满级截图已核对。后台音频仅通过测试进程的 INI 覆盖开启，不改变项目或用户设置。本轮尚未重新导出 Shipping 包。
