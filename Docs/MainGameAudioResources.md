# MainGame 音频资源清单与免费音源候选

核对日期：2026-10-16。目标地图：`/Game/MainGame/Maps/Lvl_MainGame`。

本次已下载 12 个音源包／单曲，共 49,915,255 字节（约 49.9 MB），来自 Kenney、OpenGameArt、Incompetech 三个下载站点；另核对了一项 Freesound 上游录音。整理为 37 个 48 kHz / PCM16 WAV 候选，保留完整源文件、网页快照、授权文本、SHA-256 与加工记录。试听页同时列出此前版本的 47 个声音，便于比较。

**这份清单是接入前的资源复核基线。后续已生成并导入 19 个正式派生音源；最新代码与验证状态见 [MainGame 音频接入记录](MainGameAudioImplementation.md)。** 下文“已有”指复核时的代码／资产，不代表新版本已在 MainGame 运行验收。对声音风格的选择依据源站描述、录音类别和玩法用途，最终听感待确认。

## 入口与文件

- [直接试听新候选与现有版本](../SourceAssets/MainGameAudio/Audition.html)：按 P0、P1、备选、现有版本筛选，每次只播放一个声音。
- [候选 manifest](../SourceAssets/MainGameAudio/manifest.json)：37 个候选的原片段、裁剪时间、原始与成品哈希、处理方式、峰值与时长。
- [音源与许可清单](../SourceAssets/MainGameAudio/sources.json)：作者、来源页、直接下载地址、授权、获取时间与下载哈希。
- [候选署名文本](../SourceAssets/MainGameAudio/Licenses/CREDITS.txt)：投入游戏时按最终实际使用项整理。
- [技术检查结果](../SourceAssets/MainGameAudio/validation.json)：下载校验、解码和输出验证通过；无削波。
- `SourceAssets/MainGameAudio/Downloads/`：12 个完整原始下载文件；`Originals/`：选中的完整单条源音频；`Candidates/`：整理后的试听 WAV；`Licenses/Pages/`：素材页与许可页快照。
- 当前已接入音效继续位于 `SourceAssets/AudioPilot/Wav/`，来源见其 `manifest.json` 和 `Licenses/CREDITS.txt`。

## 依据 MainGame 修正范围

实际流程是：木板地面的矿工卧室 → E 拾取镐子与喷气背包 → 挖开出口 → 喷气上升 8 米 → 第一区普通矿道 → 第二区溶洞 → 第三区石笋 → 第四区神经岩。

四区共有 125 处固定矿物，四条升级轨道是镐子、喷气背包、炸弹、钻机。矿物暴露超过 50% 后瞄准按 E 收取，金币进入共享钱包；Tab 打开升级，G/V 切换工具，R 恢复岩层而不重复奖励。

证据：`Docs/MainGameSystems.md`、`Docs/MainGameRoutesV2.md`、`Docs/StartRoom.md`、`Config/MainGameMinerals.json`、`Scripts/integrate_main_game_economy.py`、`Scripts/create_start_room.py` 与相关 C++。本轮没有启动重型 UE 地图，也没有将历史报告中的测试算成本轮验证。

本清单不把旧 FullJourney 的卡牌战斗、办公室、博物馆、解谜房、结尾独白当作 MainGame 当前必需项。未发现连续吸矿玩法，因此不再列“吸取持续循环”。神经岩是当前地形风格，不能据名称推定已有生物心跳、敌人或 Boss。

## 核对出的具体缺口

1. `StartRoom/MiningStartRoom.cpp::Interact` 解锁装备并收走物体，没有调用专用拾取音。卧室拿到镐子和背包需要两个简短且可辨认的反馈。
2. `MiningAudioComponent.cpp::TickComponent` 的脚步固定使用 `S_Foot_concrete`；虽然旧库已有木脚步，当前没有按卧室木板／岩石／泥土进行表面选择。
3. `OnContact` 根据挥镐阶段选择轻／重敲击，没有按岩石和泥土材质分流。新泥土音源需要实际材质或 Actor 类型判定才能发挥作用。
4. `UpdateMusic` 只根据 `IsMiningArea()` 播放一条 `M_DreamyFlashback_Loop`，没有为卧室和四个矿区分曲／分层。卧室归第一区管理，不能仅靠 AreaKind 区分卧室与矿道。
5. 当前音频组件没有四矿区环境底声的播放管理。洞穴风、空间底声、滴水需要区域混合／空间播放逻辑。
6. 钻机、喷气、炸弹、E 收取已有事件声音；钻机和推进主声目前是项目合成试用声。新录音是替换或混合候选，不是修复所有事件缺失。
7. Tab 开关及购买成功／失败已有音频调用。不要因为换清单而重复加触发，造成双响。F8 设置存在，但其按钮不必每次拖动都发音。

## 优先级定义

- **P0：动作和操作反馈。** 首次教学要能听懂，连续挖掘／飞行不能刺耳，工具状态要可辨认。先补缺失事件和材质差异，再换已经工作的声音。
- **P1：空间与长期听感。** 区分房间、竖井和各矿区，让音乐不过度重复，处理切换与并发。
- **P2：风格备选和额外润色。** 稀有奖励、区域短提示、抽象共鸣等可以后置，不强行引入新玩法。

以下数量是建议最终变体数，不是宣称已下载的独立录音数。`C_` 名称在新 `Candidates/` 中；`S_`／`M_` 名称在既有 `AudioPilot/Wav/` 中，均省略 `.wav` 后缀。同一源音裁剪出的启动／停止属于派生片段，不算独立录音。

## P0：按接入价值排序的详细清单

| ID | 场景／触发 | 建议最终数量与长度 | 已有状态 | 本次对应音源／处理建议 |
|---|---|---|---|---|
| P0-01 | 卧室 E 拿起镐子，解锁挖掘成功后 | 1–2 条，0.3–0.8 秒 | 缺专用事件 | `C_PickaxePickup`；Workshop 工具碰撞，截取一次干净碰撞，不连续响 |
| P0-02 | 卧室 E 穿上背包，飞行解锁成功后 | 1–2 条，0.3–0.8 秒 | 缺专用事件 | `C_JetpackEquip`；Workshop 金属扣合候选，需与镐子区别 |
| P0-03 | 左键挥镐但未命中 | 3 条，0.15–0.4 秒 | 已有 | 保留 `S_Swing_00..02`，不额外下载同类声音 |
| P0-04 | 镐子敲到普通岩壁的第一击 | 4–6 条，0.1–0.5 秒 | 已有，按击打阶段区分 | 保留 `S_DigLight_00..04`，与新泥土接触声分流 |
| P0-05 | 后续重击岩壁 | 4–6 条，0.15–0.6 秒 | 已有 | 保留 `S_DigDeep_00..04`，不让音量随连续击打无限叠高 |
| P0-06 | 敲击卧室堵口、四区泥土引导层与厚土块 | 3–5 条，0.2–0.7 秒 | 缺材质分流 | `C_DirtDig` 为土壤纹理；本次仅取得 1 条独立铲土声，应混合现有轻击，不能把它称为完整五变体库 |
| P0-07 | 岩层实际破裂／开通 | 3–5 条，0.3–1 秒 | 已有破坏提交事件 | 保留 `S_DigBreak_00..02`；受保护外壳不能播放成功破裂音 |
| P0-08 | 击中不可挖基岩／封口墙 | 1–2 条，0.08–0.25 秒 | 未见独立拒绝分流 | `C_BedrockContact` 是金属短击占位备选；优先比较现有轻击的短干版本，避免让石墙听起来像铁板 |
| P0-09 | 卧室木板上走动 | 4–6 条，0.15–0.35 秒 | 木脚步资源已有，但当前未选用 | 优先启用既有 `S_Foot_wood_00..04`；新增 `C_FootWood_01..03` 供替换比较 |
| P0-10 | 矿道岩石表面走动 | 4–6 条，0.15–0.4 秒 | 当前用混凝土脚步 | `C_Foot_stone01` 与现有 5 条脚步比较；新包仅 1 条石面独立录音 |
| P0-11 | 泥土／碎石上走动 | 每类 3–5 条，0.2–0.5 秒 | 缺表面分流 | `C_Foot_mud02`、`C_Foot_gravel`；各 1 条独立录音，先做表面差异，后补足随机变体 |
| P0-12 | 竖井／坡道落地 | 2 档，0.2–0.6 秒 | 已有 `S_Land` | 现有落地声叠低音量泥土／碎石，不把每次轻微离地都当重摔 |
| P0-13 | 钻机从未装备进入装备状态 | 1 条，0.2–0.6 秒 | 已有合成启动 | `C_DrillStart`；真实长钻录音起音候选，需试听启动瞬态是否完整 |
| P0-14 | 钻机装备中但未接触岩壁 | 1–2 个循环，2–5 秒 | 已有合成怠速 | `C_DrillIdleAlt_00/01`；低音量科幻机械备选，不与实际钻孔一样响 |
| P0-15 | 钻机按下／持续转动 | 1 个循环，2–5 秒 | 已有工具状态逻辑 | `C_DrillMotor_Loop`；真实电钻录音中段，和旧合成声 A/B 比较 |
| P0-16 | 钻机真实命中岩石／泥土 | 每材质 1 个循环 | 已有接触合成层 | `C_DrillTexture_Loop` 加现有岩石碎粒或 `C_DirtDig`；它本身是电钻录音，不宣称是钻岩实录 |
| P0-17 | 钻机收起／退出有效状态 | 1 条，0.2–0.6 秒 | 已有合成停止 | `C_DrillStop`；真实录音尾段，状态退出时只播放一次 |
| P0-18 | 钻机低能量提醒 | 1 条，0.1–0.25 秒 | 已有合成提醒 | 保留 `S_DrillLow`，进入阈值提示；无需为了“找素材”替换可控提示音 |
| P0-19 | 空格启动喷气 | 1 条，0.15–0.4 秒 | 已有合成起音 | `C_JetStart`；Kenney 推进器起音候选 |
| P0-20 | 竖井上升／矿区飞行／升级后悬停 | 1 主循环 + 1 备选，3–6 秒 | 已有合成持续推进 | `C_JetThrust_00..02_Loop` 三选一；避免与电钻同时占据过多高频 |
| P0-21 | 松开推进、推力停止 | 1–2 条，0.2–0.7 秒 | 已有合成停止 | `C_JetVent_01..03`；原片各约 1.85 秒，正式停机建议截短，不拖尾盖住下一次动作 |
| P0-22 | 喷气过热锁定 | 1 提示 + 1 泄压 | 已有合成过热 | 保留 `S_JetOverheat` 的提示语义，叠一次低音量泄压；持续锁定期间不每帧重播 |
| P0-23 | 成功投掷炸弹 | 2 条，0.1–0.4 秒 | 已有 | 保留 `S_BombThrow`；没有库存时不能播放成功投掷声 |
| P0-24 | 炸弹首次接触墙／地面 | 2–3 条，0.05–0.25 秒 | 已有 | 保留 `S_BombStick`，必要时少量混合 `C_JetpackEquip` 的短金属接触 |
| P0-25 | 炸弹引信燃烧 | 1 循环，1–3 秒 | 已有合成循环 | 保留 `S_BombFuse_Loop`；新的长蒸汽声不宜直接当引信，爆炸／销毁时立即停止 |
| P0-26 | 炸弹实际爆炸破岩 | 2–3 条，0.5–1.5 秒 | 已有合成加碎岩 | 优先 `C_BombCrunch_00..02`（CC0）与现有碎岩混合；`C_BombLarge_BY/Small_BY` 仅为另种力度备选 |
| P0-27 | E 成功收取暴露的矿物 | 2–3 条，0.1–0.45 秒 | 已有，抑制重复金币声 | 保留 `S_Collect`；此处是一次收取，不需要持续吸附声 |
| P0-28 | 金币实际增加 | 1–3 条，0.1–0.35 秒 | 已有 | 保留 `S_Coin`，矿物收取时继续避免与 `S_Collect` 双响 |
| P0-29 | Tab 升级面板打开／关闭 | 各 1 条，0.05–0.2 秒 | 已有 | 保留 `S_UI_Open/Close`，不要重叠添加第二个界面监听 |
| P0-30 | 购买升级成功／失败 | 各 1–2 条，0.1–0.4 秒 | 已有 | 保留 `S_UI_Confirm/Reject`；声音须在购买结果确定后触发 |

P0 最先落地顺序：P0-01/02 装备教学 → P0-09/10/11 脚步表面 → P0-06 泥土挖掘 → P0-13..22 工具音色比较 → P0-26 爆炸比较。其它已有项以验证为主。

## P1：场景环境与配乐

| ID | 场景／用途 | 建议配置 | 候选与选择理由 | 当前差距 |
|---|---|---|---|---|
| P1-01 | 卧室初始气氛 | 低音量主题或安静空间，装备提示最清楚 | 先复用 `M_DreamyFlashback_Loop`；无需为了铺满声音而加入不存在的风扇／机器 | 需独立卧室范围，不能只看第一区标签；没有下载独立室内 room tone，非阻塞项 |
| P1-02 | 8 米竖井的空间底声 | 风声 1 层，喷气时降低环境声 | `C_Amb_Wind_Loop`；来自 60 秒 wind1，取 25 秒段做循环候选 | 需体积范围与低音量／滤波；源是风，不是室内实录 |
| P1-03 | 第一区普通矿道 | 稍干、较安静的底声 | 低音量 `C_Amb_Wind_Loop`，少量空间混响；主奏保留旧曲 | 缺环境管理；不建议在无滴水的每段通道布满水声 |
| P1-04 | 第二区溶洞 | 宽阔空间底声，局部滴水 | `C_Amb_Cavern_Loop`；源本身已有低风和滴水 | 避免与独立滴水层全程叠满；是否湿润需结合画面试听 |
| P1-05 | 第三区石笋 | 与溶洞共享底层，通过滴水密度和混响区别 | `C_Amb_Drips_Loop`；完整源 20.06 秒，候选约 19.81 秒 | 先用连续氛围，再按空间需求切单滴并置于具体点位 |
| P1-06 | 第四区神经岩 | 低音量空间底层，抽象声可选 | 延用洞穴底声；`C_Amb_NeuralAlt_00/01_Loop` 是可选风格实验 | 不直接加心跳／血流；forceField 来源可能太科幻，未定为首选 |
| P1-07 | 四区之间三个连接区 | 2–4 秒环境交叉渐变 | 复用前后区域声音，不需要额外下载 | 区域边界往返应有滞回／防抖，避免快速启停 |
| P1-08 | 中段探索音乐 | 1 条完整曲目，听感确认后设计循环 | `C_Music_Cave_Preview` → 完整 `Originals/cave_music/cave_music.ogg`，280 秒；源站定位神秘洞穴 | 20 秒试听片段不是成品音乐循环，需按乐句找接缝 |
| P1-09 | 后段推进音乐 | 1 首备选，不自动固定到第四区 | `C_Music_DeepHaze_Preview` → 完整 122.11 秒曲；官方描述逐渐推进 | 节奏是否干扰连续挖掘待试听；采用时加入 CC BY 4.0 署名 |
| P1-10 | 进入新区域／音乐切换 | 同时只保留两条过渡轨，短暂渐变 | 已有 Dreamy Flashback 与新 Cave Theme／Deep Haze 即可 | 需区分环境切换和音乐乐句切换，避免每次跨界重开歌曲 |
| P1-11 | 钻机 + 喷气 + 爆炸同时发生 | 重要瞬态清楚，连续噪声让位 | 使用本清单工具候选做叠加测试 | 新 WAV 的峰值处理不是最终混音；要在 MainGame 验证提示能否听见 |
| P1-12 | F8 设置／暂停／切区／R 恢复 | 清除不再有效的循环，恢复后按真实状态播放 | 复用当前音量设置和生命周期逻辑 | 新环境循环也要纳入暂停、音量、重置与退出清理 |

音乐首轮推荐顺序：保留 Dreamy Flashback 作为对照 → 试听 Cave Theme 是否更贴合洞穴 → 比较 Deep Haze 的推进感。EmptyCity 是偏暗／恐怖的风格备选，不默认把 MainGame 改成恐怖氛围。

## P2：不阻塞当前试玩的可选项

| ID | 可选反馈 | 当前素材与限制 |
|---|---|---|
| P2-01 | 高价值／稀有矿物收取更亮的提示 | 先用既有 `S_Collect/S_Coin` 少量音高／分层变化；依据真实矿物价值，不能每颗都播放长奖励曲 |
| P2-02 | 拿齐两件初始装备的完成提示 | 复用一次 `S_UI_Confirm`；与第二件装备音错开，不另占长音乐轨 |
| P2-03 | G/V 装备切换的机械动作声 | `C_PickaxePickup/C_JetpackEquip` 可剪短试用；别让钻机已有启动重复响 |
| P2-04 | R 恢复岩层／F6 返回入口 | 复用短 `S_UI_Close/Confirm` 作确认，现有工具循环先停止；这些属于控制反馈，勿与奖励音混淆 |
| P2-05 | 神经岩独特共鸣 | `C_Amb_NeuralAlt_00/01_Loop` 与 `C_Machine_Loop` 只是实验材料，机器感太强就弃用 |
| P2-06 | 更暗的音乐风格 | `C_Music_EmptyCity_Preview`，完整 100 秒、CC0；不列入默认配乐 |
| P2-07 | 写实重爆炸 | `C_BombLarge_BY/C_BombSmall_BY`；CC BY 3.0，额外署名；首轮可直接选已有 CC0 替代 |
| P2-08 | 独立卧室室内底噪和更多泥地变体 | 本轮未取得专门 room tone；泥地、碎石各仅 1 个新独立脚步样本。先用现有主题／安静空间与已有声音，定风格后再补录或补搜 |

## 已实际下载的免费音源

下载包的数量与候选 WAV 数不同。同一包会筛选多个片段；本轮不把未选用的激光、门和史莱姆音效纳入 MainGame 必需清单。

| 来源 ID | 原始资源／作者／网页 | 授权选择 | 本次用途 |
|---|---|---|---|
| workshop | [68 Workshop Sounds](https://opengameart.org/content/68-workshop-sounds) — bart | CC0 | 真实电钻、装备工具碰撞；原包 68 条，不是 68 条全用于项目 |
| kenney_scifi | [Sci-fi Sounds](https://kenney.nl/assets/sci-fi-sounds) — Kenney | CC0 | 引擎怠速、推进器、破岩爆炸、可选共鸣；原包 70 条 |
| steam | [Steam release sounds](https://opengameart.org/content/steam-release-sounds) — bart | CC0 | 喷气收尾、过热泄压，5 条中选 3 条 |
| steps | [Different steps on wood, stone, leaves, gravel and mud](https://opengameart.org/content/different-steps-on-wood-stone-leaves-gravel-and-mud) — TinyWorlds | CC0 | 3 木、1 石、1 泥、1 碎石；不选落叶脚步 |
| shovel | [Shovel Sound](https://opengameart.org/content/shovel-sound) — RavenWolfProds 原录音，themightyglider 编辑 | CC0 | 泥土挖掘纹理；核对 [Freesound 原作 #503672](https://freesound.org/people/RavenWolfProds/sounds/503672/) 同为 CC0 |
| wind | [wind1](https://opengameart.org/content/wind1) — Luke.RUSTLTD | CC0 | 下载页面的 wind1.wav，60 秒；其余 wind2..5 未下载 |
| dungeon | [Loopable Dungeon Ambience](https://opengameart.org/content/loopable-dungeon-ambience) — JaggedStone | CC0 | 94.31 秒洞穴低风与滴水组合 |
| drips | [Dripping water loop](https://opengameart.org/content/dripping-water-loop) — Independent.nu，qubodup 上传 | CC0 | 20.06 秒滴水环境层 |
| cave_music | [Cave Theme](https://opengameart.org/content/cave-theme) — Brandon75689，HaelDB 上传 | 选 CC0 | 280 秒洞穴探索音乐；页面同时提供 OGA-BY 3.0 与 CC0，快照保留两项信息 |
| deep_haze | [Deep Haze](https://incompetech.com/music/royalty-free/index.html?isrc=USUAN1100886) — Kevin MacLeod | CC BY 4.0 | 122.11 秒后段配乐备选；完整官方 MP3 已下载 |
| empty_city | [EmptyCity](https://opengameart.org/content/emptycity-background-music) — yd | CC0 | 100 秒暗色音乐备选；仅下载 OGG，不下载工程 ZIP |
| explosions | [2 High Quality Explosions](https://opengameart.org/content/2-high-quality-explosions) — Michel Baradari，qubodup 上传 | CC BY 3.0 | 两个爆炸力度备选；原包 info.txt 与网页授权一致 |

10 个来源采用 CC0，2 个来源采用需署名的 CC BY。Kenney 包内 License.txt、爆炸包 info.txt、Creative Commons 许可页面和全部来源页已保存。Deep Haze 另保留官方目录中的曲名、ISRC、文件名和时长条目；其官方许可页提供免费署名使用选项。

既有素材的许可没有变化：Dreamy Flashback 继续保留 Kevin MacLeod / CC BY 4.0 署名；Kenney 的 Impact/RPG/Interface 和 rubberduck 碎岩源继续按既有 CC0 记录；现有工具合成声仍标记为项目自产，不能改写为外部真实录音。

检索中 BigSoundBank 的电钻候选页面通过本机 Node 与 curl 获取均发生 TLS 连接失败，未下载、未列入已取得资源。真实电钻已由 Workshop 包补齐。本轮没有登录网站、支付费用或依赖登录后的下载。

## 音频整理与检查

生成脚本：`Scripts/prepare_main_game_audio_candidates.py`。它只读本轮下载和旧 AudioPilot 清单，输出候选、试听页、许可整理与检查报告，不写 `Content/` 或 UE 地图。

运行环境沿用项目已有 Python、numpy 和 `Saved/AudioPilotTools` 中的 soundfile；`.7z` 用 Windows 自带 `tar` 读取指定音频文件，不执行包内任何脚本。

处理是可追溯的：单次效果优先转单声道；环境／音乐保留原有声道；统一到 48 kHz / PCM16；去直流偏移；单次音短淡入淡出；循环候选最多做 250 ms 交叉渐变；峰值控制并限制增益放大量。风声源本来是单声道，保留模式不会凭空造成立体声。

37 个新候选全部可解码、无 NaN／Inf、无削波；所有 12 个下载 SHA-256 与记录匹配。音乐只加工 20 秒对比节选，完整原曲保留，未宣称音乐已经无缝循环。动态范围／感知响度／音色合适程度没有用峰值归一化替代人工判断。

试听时先比较现有与候选工具声音，再连续听 30–60 秒钻机与推进的循环，最后叠加音乐和环境声判断遮蔽。下一轮应先完成上述 MainGame 专属触发与材质选择，然后在真实地图验证；当前默认启动脚本仍是 FullJourney，不能用它的成功启动代替 MainGame 验收。
