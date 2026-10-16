# MainGame 房间音效接入记录

日期：2026-10-16。目标地图：`/Game/MainGame/Maps/Lvl_MainGame`。

## 当前交付状态

已经下载 6 个免费 CC0 素材包，选取并加工为 71 个声音，包含 64 个一次性音效、7 个循环。71 个 SoundWave 已通过当前打开的编辑器导入、单独保存到 `/Game/Audio/Rooms`，并在引擎内完成属性核验。

玩法接入的 C++ 已编译通过，**18 项自动化测试全部通过**。MainGame 的牌局／电视逃生及镜像馆实体解谜流程均已验证音效事件触发。当前可重新打开编辑器，在 MainGame 中试听。本轮自动化使用无声模式检查资源和逻辑，尚未完成逐房间听感、最终混音或打包验收。

三个矿区统一的 Dreamy Flashback 音乐保持原样。本批重点为房间玩法的动作、结果、危险和环境声音。

## 素材来源

以下为实际下载时的官方素材页面，下载日期为 2026-10-16。许可页面快照、原始压缩包、逐文件 SHA-256 与加工参数均已保留。

| 素材包 | 作者 | 实际用途 |
|---|---|---|
| [Interface Sounds](https://kenney.nl/assets/interface-sounds) | Kenney | 确认、拒绝、思考、交换、警告、拾取、重试 |
| [Casino Audio](https://kenney.nl/assets/casino-audio) | Kenney | 纸张处理、选牌、发牌、成组出牌 |
| [Impact Sounds](https://kenney.nl/assets/impact-sounds) | Kenney | 挖击回声、玻璃、陶瓷、落地、机关碰撞 |
| [Sci-fi Sounds](https://kenney.nl/assets/sci-fi-sounds) | Kenney | 激光、毒雾、火焰、终结、相位、机械门 |
| [30 CC0 SFX loops](https://opengameart.org/content/30-cc0-sfx-loops) | rubberduck | 机械运转、水流、办公室及房间底声 |
| [40 CC0 water / splash / slime SFX](https://opengameart.org/content/40-cc0-water-splash-slime-sfx) | rubberduck | 洪水起音、水声循环、淹没 |

来源页面所列许可均为 CC0-1.0；详情见 `SourceAssets/RoomAudio/sources.json` 与 `Licenses/`。打包随附说明为 `Content/RoomAudioCredits/CREDITS.txt`。打印机、办公室喷发、终结等采用分层设计，打印机声不是所展示打印机型号的现场录音。

## 已写入的触发逻辑

下表为已编译的触发逻辑，具体自动化覆盖范围见验证记录。需求编号对应 `MainGameRoomSfxRequirements.md`。

| 房间／需求 | 事件与音效 | 触发与限制 |
|---|---|---|
| 共享 G01–G03 | `S_Confirm`、`S_Reject`、`S_Open`、`S_Close` | 按业务成功／拒绝触发；分类使用现有 Effects/Ambience；全局音频开关及玩家音频组件停用时清理循环 |
| 办公室 O01–O04 | `S_OfficeNotify_00..02`、`S_Reply`、`S_Printer`、`S_Paper_00..02` | 新增任务、手动回复成功、实际出纸、成功处理纸张；电脑通知 0.35 秒、打印机 0.7 秒冷却，音源位于设备 |
| 办公室 O05–O08 | `S_OfficePanic`、`S_BombOffer`、`S_BombPickup`、`S_DoorBreak`、`S_Seal`、`S_Collapse` | 失控转折、炸弹出现／真正领取、实际破门、越过出口封闸和最终坍塌分别触发；领取不依赖首次解锁能力 |
| 办公室 O09–O12 | 开关界面、`S_Warning`、`S_OfficeBurst_Loop`、`A_Office_Loop`、复用 `S_CardFire` | 开始加速时短警告；喷发使用单一循环；尾声装饰爆炸限频，避免每颗炸弹都叠声 |
| 回声 E01–E04 | `S_EchoCrumble`、`S_EchoLanding`、`S_EchoKnock_00..02`、`S_EchoExit` | 坍塌、实际落地、有效敲击和通道完成；落地／敲击使用传入世界位置；敲击含固定回声尾 |
| 牌局 C01–C08 | `S_CardSelect_00..02`、`S_CardPlace`、`S_CardLaser`、`S_CardPoison`、`S_CardFire`、`S_CardImpact`、`S_LowHealth`、`S_CardFinale`、`S_CardVictory`、`S_CardReward` | 选择与全选使用成组反馈；确认失败拒绝；结算约 1 秒添加命中／组合爆燃；反击与濒危分开；胜利和领奖使用不同声音 |
| 牌局 C07、C09–C12 | `S_DoorBreak`、`S_Seat`、`S_Confirm`、`S_CardArrival`、`S_CardDraw`、`A_Giant_Loop` | 真正撞墙由逃生破口负责；入座、匹配、到场和发牌跟随状态；巨人空间低电平底声在洪水阶段退出 |
| 逃生 T01–T03 | `S_Warning`、`S_FloodStart`、`S_Water_Loop`、`S_Checkpoint` | 领奖后约 0.45 秒播危险提示；涨水开始才播放起音；循环强度随水位变化；到达真正检查点才确认 |
| 逃生 T04–T08 | `S_Glass_00..02`、`S_ElectricalWarning`、`S_Drown`、`S_ShortCircuit`、`S_Retry`、`S_Escape` | 电视屏幕命中替换通用石头接触层；最后 15 秒警告、最后 5 秒加速；失败类型不同；重试与成功停止原水声循环 |
| 逃生 T09–T10 | `S_Confirm`、`A_TV_Loop` | 实际穿过已挖通岩段给轻确认；建立电视检查点后启用低电平机器声 |
| 镜像馆 W01–W04 | `S_Thought`、共享开关与拒绝、`S_Swap` | 新对象真正放入思考槽后才播放；一次交换一次主确认；增长碰撞与玩家属性设置失败时拒绝 |
| 镜像馆 W05–W08 | `S_Pressure`、`S_Power`、`S_RewardDoor`、`S_Complete` | 压块实际到底、落底导电球条件成立、双锁首次成立、缩小玩家真正进入奖励凹室分别触发 |
| 镜像馆 W09–W15 | `S_Transform`、`S_Shrink`、`S_Grow`、`S_Phase`、`S_Float`、`S_Mechanism_Loop`、`S_Pottery`、`S_PowerOff`、`S_Reset`、`A_Museum_Loop` | 物体与玩家变化分远近；实际移动时刷新循环，停止后淡出；球首次落底发声；条件解除、整体重置分别提示 |

已有卧室装备、矿区挖掘、脚步、喷气和玩家炸弹音效继续复用。

## 播放与资源管理

- 新增 `UMiningRoomAudioSubsystem`；播放限定 MainGame 的 Game/PIE 世界，普通编辑器世界不会自动播放。
- 卡牌与回声插件通过委托连接主游戏音频；没有委托的独立插件场景继续使用原素材。
- 每个声源／事件独立冷却；5 组高频反馈各有 3 个不连续重复的变体。
- 一次性声音每资产并发上限 3，循环上限 2；循环按声源复用组件，未再刷新或声源失效后淡出销毁。
- 位置声使用 250 cm 内圈与 2400 cm 衰减范围；回声尾音是素材中的固定延迟，不是墙距声学模拟。
- 文件统一为 48 kHz、单声道 PCM16；峰值留有余量，循环接缝经过交叉淡化。
- 共 8,650,350 字节 WAV。新增目录已配置 AlwaysCook，Credits 已配置随包附带；尚未进行打包验证。

## 验证记录

已完成：

1. `Scripts/validate_room_audio.py`：71/71 WAV 哈希、格式、非静音、无削波、循环接缝、原始来源哈希、71 个资产源码引用及磁盘 uasset 存在检查通过。
2. `Scripts/import_room_audio.py`：在已有编辑器中成功导入并保存 71 个资源，报告 `Saved/RoomAudioImport.json`。
3. `Scripts/verify_room_audio_import.py`：引擎内 71/71 SoundWave 类型、声道、时长、分类、循环、并发与循环虚拟化检查通过，报告 `Saved/RoomAudioEngineValidation.json`。
4. `git diff --check`：通过。
5. `Scripts/Build_Safe.ps1`：通过。编译发现并修复电视屏幕音效分支的局部变量与成员同名问题。最终构建日志为 `C:/Users/11694/AppData/Local/UnrealBuildTool/Log.txt`，资源报告为 `Saved/Performance/build-resources-20261016-143701-741.csv`。

自动化测试：全部通过，共 18 项，失败 0 项，未运行 0 项。

| 运行入口 | 数量 | 覆盖 | 报告目录（位于 Saved/Automation） |
|---|---:|---|---|
| `Scripts/RoomAudio_Safe.ps1 -Mode Core` | 2 | 全部 71 个资产；事件冷却、独立声源、循环复用、停止、声源销毁、全局关闭与组件停用 | `RoomAudioCore` |
| `Scripts/RoomAudio_Safe.ps1 -Mode MainGame` | 1 | MainGame 牌局到电视逃生；发牌、领奖、洪水、淹没、重试、检查点、短路、逃脱事件 | `RoomAudioMainGame` |
| `Scripts/RoomAudio_Safe.ps1 -Mode Mirror` | 1 | MainGame 实体镜像谜题；交换、陶瓷落地、压力锁、导电锁、奖励门、最终完成、重置事件 | `RoomAudioMirror` |
| `Scripts/RoomAudio_Safe.ps1 -Mode Regression` | 13 | 8 项原音频测试（含三矿区统一音乐）、2 项卡牌规则、3 项回声规则 | `RoomAudioRegression` |
| `Scripts/RoomAudio_Safe.ps1 -Mode Office` | 1 | 独立办公室地图中真实领取炸弹、炸门、撤离和坍塌流程回归 | `RoomAudioOffice` |

每个报告目录包含 `index.json`。脚本检查结果数量、失败／未运行数与报告新鲜度，沿用项目资源保护，逐组运行。

MainGame 镜像馆测试首次运行时暴露了测试沿用旧 Campaign 后续房间的假设；MainGame 的镜像馆是最后一间，已改为返回矿区验证离区恢复，未修改游戏的主线规则。修正后通过。

测试在独立会话及 PIE 副本中运行，部分流程采用定位和推进状态以隔离事件；办公室完整流程回归使用独立办公室地图，不代表 MainGame 办公室全程实玩。通过事件计数只能证明逻辑触发，不能替代真实听感验收。下一步是 MainGame 按房间试听并调整混音。

## 尚未做完的细节

本批不宣称原清单的 68 条需求全部验收完毕。以下仍需后续完善：

- MainGame 逐房间人工实玩听验、最终混音和打包测试目前未完成。
- 打印机目前 1 个复合音；高速通知已有冷却和办公室喷发循环，尚无独立的每台打印机三档循环。
- 回声尾音尚未跟视觉脉冲实时变化；暖光洞口没有独立石膏材质层。
- 毒云失能反应暂复用毒音；纸牌落桌是成组声音；巨人底声未加入战斗自动压低。
- 洪水目前以整体水声为主，尚无独立破口空间水流层；电视机器声在检查点后播放，尚未作为远处入口导航。
- 镜像馆导电属性本身暂用通用变化音；导电机关成立已有独立声音。移动停止以淡出为主，陶瓷落地目前 1 个音色。
- UI 悬停、零散纸片落地、迷宫滴水、家具水花和玩偶非语言反应等 P2 点缀未加入。

## 文件与重建

`SourceAssets/RoomAudio/manifest.json` 是逐资产完整清单，包含来源包、原文件、层音量、时间偏移、变速、处理参数和最终哈希。

工具顺序：`fetch_room_audio_sources.py` 下载 → `prepare_room_audio.py` 加工 → 在 UE 内执行 `import_room_audio.py` → `verify_room_audio_import.py` 与 `validate_room_audio.py` 核验。

加工脚本需要 numpy 与 soundfile；本机使用已有 Codex Python 运行时及 `Saved/AudioPilotTools` 中的 soundfile。导入与引擎属性核验脚本使用 Unreal 内置 Python。
