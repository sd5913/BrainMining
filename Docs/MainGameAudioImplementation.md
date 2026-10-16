# MainGame 音频接入记录

日期：2026-10-16。目标：`/Game/MainGame/Maps/Lvl_MainGame`。

## 当前状态

已完成首轮 P0/P1 代码、19 个音源的加工与 UE 导入，并按用户要求统一四矿区配乐。**新 C++ 编译成功，8 项音频自动化测试全部通过，实际 MainGame 地图的 32 项运行检查全部通过，并完成 52.373 秒引擎输出录音的 12 项波形验证。**

此前内存不足阻止编译的问题已解除。本轮使用原有资源保护阈值完成编译与运行，没有降低门槛或结束用户的其它软件。修正了表面材质参数读取的一处初始化警告，并缓存环境音乐参数查询。

`Scripts/MainGameAudio_Safe.ps1 -Mode Import` 已成功保存并核对 19 个声音。导入命令行进程有此前已知的 CDO 物理材质初始化退出码 1；包装脚本只接受这个已知错误，并要求全新报告和 `MAIN_GAME_AUDIO_IMPORT_OK: 19` 标记，其他错误不放行。后续编译、自动化与实图运行分别提供独立验证。

## P0 代码

- `StartRoom/MiningStartRoom.cpp`：装备成功解锁并收取后，调用镐子／背包各自的提示音；失败、遮挡或已收取的物体不会进入该调用。只在 MainGame 配置启用这些新提示。
- `MiningAudioComponent` 与 `MiningAudioMainGame.cpp`：在 MainGame 按实际地面命中材质选择木板、石面、泥土或碎石脚步；木板复用原先五个变体，泥土／碎石各一个新独立录音并少量变调。落地按下降速度调整力度，再叠少量对应表面声。
- 岩壁和泥土分流：纯泥土使用 `MudCore`；混合材质按 `MudStart* / MudEnd* / MudReach` 与命中世界坐标计算泥土覆盖区。普通岩石部分仍使用原石击。也支持 Actor／Component 的 `BM_Audio_Wood / BM_Audio_Dirt / BM_Audio_Gravel` 标签。当前地图无需依赖编辑器 Actor 标签名称或手动改地图。
- 材质混合边缘按区域权重选择声音，不对着色器每个像素的噪声和顶点色逐像素求值；微小泥点仍可能归入石面，这是本轮近似的边界。
- `MiningComponent` 新增不可挖命中事件；MainGame 播放有冷却的短钝击，空挥不播放，避免把不可挖基岩当成功破裂。
- 钻机起停／持续声替换为真实工具录音派生；钻岩与钻土分别混合颗粒纹理。喷气使用 Kenney 推进器，停止用短泄压。低电量、过热警示保留现有明确提示。
- 炸弹在 MainGame 使用 CC0 爆炸与既有碎岩混合版本；投掷、黏附、引信、收取、金币、升级 UI 沿用已存在的事件，避免重复加声音。
- 新素材路径 `/Game/Audio/MainGame`；只有 MainGame 选择覆盖，旧 Pilot 资产保留。其余地图的工具音色不随这轮替换改变。

## P1 代码

- 根据实际 Hall 的活动区域分四矿区；第一区内使用 StartRoom 流送变换划出卧室，避免房间和矿道只有一个声音配置。
- 卧室是较低音量 Dreamy Flashback，四个矿区统一使用第一矿区的 Dreamy Flashback 及相同音乐音量。普通矿道保留风声，溶洞保留空间底声，石笋区增加滴水，神经岩保留较低洞穴底声与风声。
- 区域候选稳定半秒后才切换，卧室边界增加空间滞回；音量渐变，不在每帧创建播放器。矿区间复用同一个音乐播放器，不换曲或重新从头播放。最多保留一条音乐与三个环境播放器。
- 实际钻孔／喷气时降低背景音乐与环境层，保留关键工具反馈。
- 音乐进 Music、环境进 Ambience、工具进 Effects，继续受 F8 五路混音控制；循环资产使用静音时继续时间线的虚拟化设置。失去本地控制、关闭音频、组件停用和结束游戏清理环境播放器。
- Cave Theme 资产保留为候选，当前 MainGame 不播放。Deep Haze、EmptyCity 和两条 CC BY 爆炸未进入这轮运行选择。

## 四矿区统一配乐（2026-10-16）

按用户要求，第二至第四矿区统一与第一区域相同的配乐、增益和工具压低规则。四区使用 `M_DreamyFlashback_Loop`，默认音乐增益 0.12；钻孔／喷气时统一保留 65%，增益为 0.078。卧室仍为该曲目较轻的版本。F8 音乐／主音量继续控制最终输出。

自动化检查四区复用同一音乐播放器且增益一致，并检查没有叠加 Cave Theme。实图录音仍分别隔离三个后续矿区的音乐，检查实际输出及 Music 静音／恢复。环境层保留各区的风声、空间底声与滴水差异。

此前 Cave Theme 增益提高方案已被本方案替代；`Saved/AudioPreviews/MainGameMusic_BeforeAfter.wav` 属于此前方案的历史试听，不代表当前运行版本。音源文件与署名没有改动。

## 资源与署名

- `SourceAssets/MainGameAudio/Runtime/`：19 个 PCM16 / 48 kHz WAV。
- `SourceAssets/MainGameAudio/runtime_manifest.json`：源文件／成品哈希、长度、通道、剪辑与混合记录。
- `Content/Audio/MainGame/`：19 个已导入 SoundWave。
- `Content/MainGameAudioCredits/CREDITS.txt`：这轮实际选用的九个 CC0 来源与修改说明；旧配乐和碎岩素材继续保留 `AudioPilotCredits` 署名。
- `Config/DefaultGame.ini`：加入 MainGame 音源 AlwaysCook 和上述署名目录随包部署。尚未执行整包 Cook。
- [试听页面](../SourceAssets/MainGameAudio/Audition.html) 新增“接入版”，可与候选和旧版本分别比较。

## 验证结果与复现

1. `powershell -File Scripts/Build_Safe.ps1`：UE 5.8.2 Development Editor 编译成功。最终增量构建资源记录为 `Saved/Performance/build-resources-20261016-110830-237.csv`。
2. `powershell -File Scripts/MainGameAudio_Safe.ps1 -Mode Test`：8 成功、0 失败、0 测试警告。报告 `Saved/Automation/MainGameAudio/index.json`。包含原有五项与新增三项：MainGame 资产类别与循环、真实混合材质的表面分类、MainGame 角色的卧室／矿区切换及工具覆盖／清理。
3. `powershell -File Scripts/MainGameAudio_Safe.ps1 -Mode Capture`：实际 `/Game/MainGame/Maps/Lvl_MainGame`、实际 `BP_MainGameCharacter`，32 项检查全部通过。包括两件装备真实交互、重复拾取被拒绝、真实木板碰撞材质、F8 开关、实际钻岩回调、喷气循环、炸弹投掷与爆炸、四矿区环境切换、停用清理与恢复，以及后三个矿区与第一区域的音乐／增益一致性检查。报告 `Saved/MainGameAudioRuntime.json`，详细断言 `Saved/Logs/MainGameAudioCapture.log`。
4. `python Scripts/validate_main_game_audio_capture.py`：12 项波形检查全部通过。录音 `Saved/MainGameAudioRuntime.wav` 为双声道 48 kHz，52.373 秒，峰值 0.084839，削波采样数 0。主音量与单独音乐滑杆静音窗口的峰值及 RMS 均为 0，恢复均正常。隔离 Effects／Ambience／Voice 后，溶洞、石笋、神经岩窗口的音乐 RMS 分别为 0.00869、0.00947、0.00913，均高于设定的 0.008 回归门槛。结果 `Saved/MainGameAudioWaveformValidation.json`。事件同时记录实际经过时间和音频设备时钟，避免流送卡顿导致录音窗口错位；这些数值反映不同乐句的采样窗口，不能替代全曲主观听感评价。
5. 首轮实图检查发现测试站位超出钻孔距离，并误用 `MOVE_Flying` 测试喷气。已改为寻找真实可挖岩壁并移到正常射程，喷气前恢复 `MOVE_Falling`；未跳过工具使用限制，也未用模拟音频事件替代实际挖掘与爆炸。首轮 26 项检查曾连续两次通过，本次扩展为 32 项后重新通过。

Capture 关闭渲染但开启真实音频，是独立临时游戏局中的脚本操作与录音，不是图形界面验收或完整人工通关。测试临时移动人物、挖岩和解锁跨区，结束时还原音量与区域解锁值，不保存关卡。不能在重要试玩进度中调用该开发命令。启动日志仍包含引擎实验性 Python Toolset 初始化错误；本轮音频检查和进程正常完成，没有扩大修改到这些插件。

## 直接试玩与后续听感确认

在项目目录运行 `powershell -File Scripts/Play_Main.ps1 -Experience MainGame -Quality Low` 可明确启动这张地图。也可在编辑器打开 `/Game/MainGame/Maps/Lvl_MainGame` 后 Play。新增启动选项保留原默认 Campaign 入口；直接使用旧默认入口仍会进入 FullJourney。

试玩时检查卧室两次拾取、木板脚步、泥土与岩石敲击差异、持续钻孔、喷气及炸弹，再用 F8 调整五路音量。用耳机及扬声器确认长时间工具疲劳、同时喷气与钻孔时的遮盖，以及完整配乐循环接缝。波形无削波不等于主观响度或乐句接缝已经验收。全包 Cook／打包与完整人工通关尚未执行。

P2 的稀有矿物奖励、额外区域短提示、独立室内 room tone、抽象神经共鸣仍后置。没有新增吸矿、怪物或其它不在当前 MainGame 中的机制。
