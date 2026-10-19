# Ending Emergence Audio Integration

本批为结局房 `EndingEmergence` 接入了首版免费音频资源。所有加工后的 WAV 统一为 48 kHz、PCM16；音乐保留立体声，环境与一次性音效转换为单声道。

2026-10-19：结局统一使用 Kevin MacLeod 的 `Light Thought var 1`。从初始细胞探索一直循环到视频和感谢界面；场景转场不重新开始音乐。原曲保存在 `Originals/Light Thought var 1.mp3`，官方页面和曲目元数据保存在 `Licenses/Pages/LightThoughtVar1.*`。许可证为 CC BY 4.0，完整署名在本目录的 `CREDITS.txt`，游戏感谢界面也显示作者、来源、许可链接及加工说明。

原曲截取前 152 秒，去掉末尾较长的淡出，使用 1.8 秒线性交叉淡化制作 150.2 秒循环，首尾各增加 5 ms 淡化以消除导入时的接点警告；48 kHz、立体声 PCM16，归一化峰值 0.70，运行时音乐增益 0.45，启动淡入 0.8 秒。最终听感需在游戏中确认。原有环境音、交互音效和视频原声保留。

## Runtime assets

| Asset | 用途 | 分类 | 循环 |
| --- | --- | --- | --- |
| `M_EE_LightThought_Loop` | 结局房全流程背景音乐：Light Thought var 1 | Music | 是 |
| `M_EE_Cosmic_Loop` | 保留的旧版宇宙音乐，当前结局流程不使用 | Music | 是 |
| `M_EE_Main_Loop` | 保留的旧版主旋律素材，当前结局流程不使用 | Music | 是 |
| `M_EE_Inner_Loop` | 保留的内部探索备选音乐，当前结局流程不使用 | Music | 是 |
| `M_EE_Drill_Loop` | 保留的钻掘段备选音乐，当前结局流程不使用 | Music | 是 |
| `M_EE_Resolve_Loop` | 保留的最终收束备选音乐，当前结局流程不使用 | Music | 是 |
| `A_EE_Cell_Loop` | 细胞内部、脑区与纸面过渡环境 | Ambience | 是 |
| `A_EE_Blood_Loop` | 血管推进环境 | Ambience | 是 |
| `A_EE_Drill_Loop` | 结尾钻掘循环 | Ambience | 是 |
| `S_EE_ScaleWhoosh_01/02` | 城市、地球、宇宙的尺度切换 | Effects | 否 |
| `S_EE_NeuralPulse_01` | 神经元标题形成与跃迁强化 | Effects | 否 |
| `S_EE_DrillHit_01` | 钻掘命中 | Effects | 否 |
| `S_EE_DrillDebris_01` | 每四次钻掘命中的碎屑尾声 | Effects | 否 |
| `S_EE_PaperFlight` | 纸片进入画面的飞行声 | Effects | 否 |
| `S_EE_PaperLand` | 纸片落桌与最终纸张落定 | Effects | 否 |
| `S_EE_FinalComputer` | 电脑噪声与收束点击的组合 | Effects | 否 |
| `S_EE_FinalResolve` | 备用最终收束音 | Effects | 否 |

## Runtime wiring

- `Initialize` 准备环境；随后 `RestartDemo` 启动一次 `M_EE_LightThought_Loop`，避免初始化时重复播放。
- 进入血管时切换到 `A_EE_Blood_Loop`，音乐组件保持播放进度。
- 所有电影镜头、钻掘段、纸面段、视频和感谢界面保持同一个音乐组件；`SetMusic` 对正在播放的同一曲目只调整增益，不重启。
- 音乐使用 Music 声音分类，遵循游戏音乐音量设置；2D/UI 声音可在收集册暂停世界时继续播放。
- 进入感谢界面不停止音乐；重玩结局时清理旧组件并从头播放，离开结局地图时停止并销毁。

## Rebuild / reimport

1. 运行 `python Scripts/prepare_ending_audio_free_sources.py`，重新生成 `SourceAssets/EndingEmergence/AudioFreeSources/Wav`、`manifest.json` 和 `CREDITS.txt`。
2. 在 Unreal Editor commandlet 中运行 `Scripts/import_ending_audio_free_sources.py`，导入到 `/Game/EndingEmergence/Audio`。
3. 运行 `Scripts/Build_Safe.ps1 -EngineRoot E:\\UE5\\UE_5.8` 编译编辑器目标。

只更新本次曲目：运行 `python Scripts/prepare_ending_audio_free_sources.py --light-thought-only`；导入前设置环境变量 `ENDING_AUDIO_ASSET=M_EE_LightThought_Loop`，然后执行上述导入脚本，避免重导入其他资产。移除该环境变量恢复全量导入。

素材来源与 SHA-256 记录在 `SourceAssets/EndingEmergence/AudioFreeSources/manifest.json`，许可证与页面快照在 `SourceAssets/EndingEmergence/AudioFreeSources/Licenses`。

## 本次验证

2026-10-19：新音乐与 12 个藏品音效全部导入，导入进程返回 0，最终导入日志无循环接点警告。编辑器 Development 编译成功。实际结局地图在 640×360 D3D12 离屏、音频混音器开启的环境中运行 `BrainMining.EndingEmergence.MusicContinuity`，1 项通过、0 失败、0 测试警告。检查启动、正确曲目/循环/音量分类、跳到循环尾部后的持续播放、血管与各镜头交接、感谢界面及重玩组件清理。报告位于 `Saved/Validation/EndingMusicRuntime/index.json`。这是自动化组件和播放状态验证，尚未人工完整试听或验证打包版。
