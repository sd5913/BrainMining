# 藏品拾取音效

12 种藏品共用 `AMiningCollectionField::TryCollectTarget` 的成功拾取入口，库存增加后调用玩家音频组件 `PlayCollectible(ItemId)`。未挖出、未瞄准、重复拾取和图鉴打开时不会触发。主地图、独立收集测试地图及外部注册的彩带奖励使用同一入口。矿石/金币/装备的旧音效保留。

| ID | 藏品 | 音效设计 | 资源 |
| --- | --- | --- | --- |
| 0 | 来自天堂的马桶 | 短水声与轻柔铃音 | S_Collect_Toilet |
| 1 | 电脑里的蚂蚁窝 | 短电子运转声与细小点击 | S_Collect_AntComputer |
| 2 | 化学试剂 | 轻碰玻璃与气泡 | S_Collect_Reagent |
| 3 | 肉鸽 | 三下渐弱扑翼，以布料录音拟音 | S_Collect_Pigeon |
| 4 | 奇怪的计算器 | 双按键与柔和确认音 | S_Collect_Calculator |
| 5 | 损坏的磁带 | 机械卡扣、短故障声与合盖 | S_Collect_Cassette |
| 6 | 音符 | 三个柔和上行拨弦音 | S_Collect_Note |
| 7 | 老旧的相片 | 纸张滑动与轻落 | S_Collect_Photo |
| 8 | 喷发中的可乐 | 开启轻响、气泡与短喷溅 | S_Collect_Cola |
| 9 | 「GPT」之花 | 轻微反向音色接柔和绽放音 | S_Collect_Flower |
| 10 | 同步闪光的萤火虫 | 三个同音高的短闪烁脉冲 | S_Collect_Fireflies |
| 11 | 散落的彩带 | 纸片散开与轻庆祝音 | S_Collect_Ribbon |

所有音效是短促的非循环 2D Effects 音效，遵循全局音频开关和音效音量，不随拾取物隐藏或其展示 Actor 销毁而中断。不同物品连续拾取不共享冷却，普通宝藏原有的通用拾取冷却不变。缺失专属资源时回退至原有 `S_Collect`，未知物品编号保持静音。

音源复用项目已保存并带授权证据的 Kenney Interface、Impact、Sci-fi、Casino、RPG 音效，以及 rubberduck 的水声包，均为 CC0。源文件复制到 `SourceAssets/CollectionAudio/Originals`，来源、哈希及加工参数见同目录 `manifest.json`，许可证见 `Licenses`，打包署名文件位于 `Content/CollectionAudioCredits/CREDITS.txt`。

制作时统一为 48 kHz 单声道 PCM16，4.5 kHz 低通，峰值不超过 0.48，目标 RMS 0.11；实际短音效约 0.44–0.73 秒，运行时增益 0.65。波形校验不能替代游戏内试听。

生成：`python Scripts/prepare_collection_audio.py`。

导入：在 Unreal Python commandlet 中执行 `Scripts/import_collection_audio.py`，生成 `/Game/Audio/Collectibles` 下 12 个 SoundWave。导入报告为 `Saved/CollectionAudioImport.json`，成功标记为 `COLLECTION_AUDIO_IMPORT_OK: 12`。

编译后运行 `BrainMining.CollectionAudio` 自动化测试：检查全部资源、Effects 路由、非循环与时长，并用共享玩家角色实际调用收集成功路径验证 12 种拾取、失败及重复交互、快速连续收集和全局禁音。执行结果以实际测试报告为准。

2026-10-19 验证完成：12 个 SoundWave 全部导入，编辑器 Development 编译通过；`BrainMining.CollectionAudio` 的 2 项测试通过，0 失败、0 测试警告。报告位于 `Saved/Validation/CollectionAudioRuntime/index.json`。该测试以无声方式验证实际拾取路径与音效派发，音频波形另外检查格式、哈希、峰值、时长和淡化端点；最终听感仍需游戏内试听，尚未验证打包版。
