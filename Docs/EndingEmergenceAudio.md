# Ending Emergence Audio Integration

本批为结局房 `EndingEmergence` 接入了首版免费音频资源。所有加工后的 WAV 统一为 48 kHz、PCM16；音乐保留立体声，环境与一次性音效转换为单声道。

## Runtime assets

| Asset | 用途 | 分类 | 循环 |
| --- | --- | --- | --- |
| `M_EE_Cosmic_Loop` | 结局房全流程统一背景音乐，从初始区域持续到最终结尾 | Music | 是 |
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

- `Initialize` / `RestartDemo` 启动 `A_EE_Cell_Loop` 和 `M_EE_Cosmic_Loop`。
- 进入血管时切换到 `A_EE_Blood_Loop`，背景音乐继续使用 `M_EE_Cosmic_Loop`。
- 所有电影镜头、钻掘段、纸面段和最终桌面段都持续使用 `M_EE_Cosmic_Loop`，只切换环境层和一次性音效。
- 纸片飞行与落桌改用新素材；结尾第 14 镜头播放最终电脑收束并切到下午环境。
- 结局结束或重玩时停止和清理音乐、环境与语音组件。

## Rebuild / reimport

1. 运行 `python Scripts/prepare_ending_audio_free_sources.py`，重新生成 `SourceAssets/EndingEmergence/AudioFreeSources/Wav`、`manifest.json` 和 `CREDITS.txt`。
2. 在 Unreal Editor commandlet 中运行 `Scripts/import_ending_audio_free_sources.py`，导入到 `/Game/EndingEmergence/Audio`。
3. 运行 `Scripts/Build_Safe.ps1 -EngineRoot E:\\UE5\\UE_5.8` 编译编辑器目标。

素材来源与 SHA-256 记录在 `SourceAssets/EndingEmergence/AudioFreeSources/manifest.json`，许可证与页面快照在 `SourceAssets/EndingEmergence/AudioFreeSources/Licenses`。
