# 盲人敲地：实现与协作指南

核对日期：2026-10-10。UE 5.8.2，单人本地原型。面向程序、美术技术协作者和后续 AI 编程助手。

建议按 **快速开始 → 当前规则 → 代码导航 → 运行流程 → 拼装接口 → 接手约定** 阅读。本文描述当前同关卡巨人房间版；历史反馈以现行实现为准。团队环境、Git LFS 和资产协作规则见 [项目 README](README.md)；收集系统见 [README_Collection.md](README_Collection.md)。

## 1. 快速开始

| 地图 | 内容 |
| --- | --- |
| `/Game/Passages/Echo/Maps/Lvl_EchoMazeToCards_P1` | 推荐：约 96 米轻迷宫，挖开透光墙后直接进入同关卡巨人牌室 |
| `/Game/Passages/Echo/Maps/Lvl_EchoPassage_P1`（已删除） | 原短通道，约 52 米，末端显示完成提示 |
| `/Game/Rooms/CardDuel/Maps/Lvl_CardRoom_P1` | 单独试玩卡牌房，见 `README_CardRoom.md` |

这些地图的 GameMode Override 为 `MiningEncounterGameMode`，直接使用项目原有 `BP_FirstPersonCharacter`。WASD 移动、鼠标环视与普通跳跃沿用主角色；按住空格使用项目已有飞行，松开后下落。

从入口向前走，地板坍塌后落地自动发出一次大范围回声。**点一下鼠标左键敲一下地面/近处墙面**，通道中不会连续挖掘；到透光洞口附近则切换为原有长按左键挖墙。R 清空回声和完成状态、修复洞口墙、退出牌局并返回坍塌前。Esc 按编辑器默认行为结束试玩。

联结地图的牌室领取奖励后进入涨水/电视逃生（见 [卡牌房间 V17](README_CardRoom.md)）。此后 R 只重试当前逃生检查点，不再返回通道；自动复活同样保留牌局胜利与奖励，进入电视后从屏幕前恢复当时的水位重试。

先按项目 README 编译 `BrainMiningEditor / Win64 / Development`，在 UE 中打开上述地图。没有专用通道启动器，默认启动地图仍是 MiningLab。玩法间联结说明见 [README_CardRoom.md](README_CardRoom.md)。

## 2. 当前玩法规则

策划来源：《通路1-2：盲人敲地【P1】.xlsx》。按后续反馈保留纯黑世界、白圈涟漪以及由近及远显形的白色轮廓；又增加较长、难度轻的迷宫和真实卡牌房联结。

- 不敲击时环境为黑色。敲击点发出两道白色圆环，波前扫过后白色线框显现；亮度同时随距离和时间衰减，远处更淡，最后回到全黑。
- 默认半径 1550 cm；落地回声为 1.4 倍；传播速度 1000 cm/s；每个位置显形后 2.6 秒内消退；敲击间隔 0.65 秒；最多同时保留 4 道回声。
- 射线先检测视线前方 245 cm 内的表面；没命中时敲脚前地面，向下检测长度适配角色胶囊体。敲击动画开始 0.18 秒后发声波。
- 轮廓被实体墙遮挡。脚步不会自动照亮；没有追逐敌人、限时或生命消耗。
- 短通道：14 格、4 米格距、约 52 米中心线路程。
- 轻迷宫：25 格主路、约 96 米最短路线；两条各 8 米死路、一处重新汇入主路的回环；无四岔口，三组石柱作为回声地标。
- 通道末端是真实透光小孔，原本无法容纳角色。长按左键挖大孔洞后进入暖光巨人牌室；两种玩法共存于同一个 Level，玩家实例不重建。沿左侧的纸箱、尺子与书本螺旋上桌，或使用原有飞行；靠近桌面光圈按 E 入座。

## 3. 关卡、参数与调节位置

停止 Play 后选择地图中的 `EchoPassageStage`（联结地图标签为 `EchoMaze_To_CardRoom`）：

| 属性 | 默认值 / 含义 |
| --- | --- |
| `EchoRadius` | 1550 cm，普通敲击影响半径；编辑范围 300–4000 |
| `EchoSpeed` | 1000 cm/s，传播速度；编辑范围 100–3000 |
| `OutlineDuration` | 2.6 秒，波前到达后的残留时间；编辑范围 0.3–6 |
| `KnockCooldown` | 0.65 秒；编辑范围 0.2–3；落地回声也占用这一冷却 |
| `bUseMaze` | C++ 默认 false；联结地图实例设为 true |
| `bRoomInSameLevel` | 联结地图为 true，跳过旧暖光占位房与切图流程 |
| `DestinationLevel` | 联结地图为空；仅为旧的独立跨图流程保留 |
| `PassageId` | 默认 `BlindPassage_P1`；联结地图为 `EchoMazeToCards_P1`，传给完成事件 |

两张地图共用现行代码/材质，不是冻结的版本快照。改布局看 `EchoPassageLayout.h::MainRoute/Cells`；网格间距为 400 cm，出口由 `ExitCell/ExitOffset/IsAtExit` 计算。修改路线时同步检查出口房间、触发边界、地标和碰撞，不能只追加一个格子。

编辑器中 Stage 的轮廓 Preview 用于搭建观察；运行时 `ResetPassage()` 将 Preview 归零。不要根据编辑器里轮廓常亮就把运行期效果改回灰色环境。

### 当前版本：台灯垂直舞台光束 V11（2026-10-10）

所有环境尺寸先按普通书房建立，再统一乘 **40**。保持项目原角色尺寸，因此约 180 cm 的玩家相当于巨人参照下约 4.5 cm 的迷你角色。不能再为增加巨物感单独把杯子、铅笔或书架放大；优先调整机位、距离和物件摆放。

| 物件 | 普通尺度参照（cm） | 游戏世界尺寸（cm） |
| --- | --- | --- |
| 房间 | 深 432.5 × 宽 500 × 高 300 | 17300 × 20000 × 12000 |
| 书桌 | 深 90 × 宽 180 × 高 75；板厚 3 | 3600 × 7200 × 3000；板厚 120 |
| 座椅 | 深 50 × 宽 50；椅面高 45、椅背高 95 | 2000 × 2000；椅面 1800、椅背 3800 |
| 书柜 | 深 35 × 宽 110 × 高 210 | 1400 × 4400 × 8400 |
| 桌上书本 | 22 × 30；单本厚 3.2–3.7 | 880 × 1200；单本厚 128–148 |
| 杯子 | 直径 8、高 10 | 直径 320、高 400 |
| 铅笔 | 总长约 19、杆径 0.75 | 总长约 770、杆径 30 |
| 台灯 | 桌上高 45、灯罩直径 20 | 高 1800、灯罩直径 800 |

- 地图仍是 `Lvl_EchoMazeToCards_P1`，通道、洞口和房间在同一个 Level。洞口位置 `(6400,3600,190)` 保持；房间范围 X=6400..23700、Y=-6400..13600、Z=0..12000，局部曝光边界同步更新。
- 桌中心 `(12600,3600,3000)`。原 U 形矮平台及其过粗文具已替换为正常高脚矩形桌。左右书柜靠后墙，位于主视野范围；桌边的书堆、杯子、笔记本、尺子和长尾夹均采用同一比例。
- 牌局 Stage 位置 `(12600,3600,2773)`、Scale=2，沿用原玩法局部坐标：上桌引导点约 `(12060,3600,3003)`，入座视点 `(12060,3600,3149)`，`SeatPitch=20`。这里只整体迁移交互台面，不缩放主角色、移动速度或玩法卡牌。
- 对手保留原造型和 `OpponentScale=10`（总比例 20），椅子中心后移到 `(16000,3600)`。椅面仍高 1800，上面叠八本厚 200 的大开本书，书堆顶高 3400；玩偶根位置 `(16000,3600,3550)`，身体底部落在书堆上。`OpponentBaseLocal=(1700,0,388.5)`，入座俯仰改为 20°，可仰视看到完整躯干、双臂和头部；保留视线跟随、呼吸和表情。
- **台灯舞台**：底座仍在舞台右侧 `(12900,4900,3028)`，灯口移到正上方 `(12450,3600,4400)`，灯罩摆正，光源 `(12450,3600,4372)` 垂直向下（Pitch=-90°）照向 `(12450,3600,3004)`。铰接灯臂连到罩顶。聚光内/外角为 29°/31°，桌面光池过渡收窄；`M_StudyLampShaftV11` 提供暖色、边缘更亮的可见锥形光束，上端裁切在灯口下方、下端落在桌面，半径约 340→800。光束无碰撞、不投影；相机位于光束内部时透明强度降至 18%，避免洗白卡牌。原悬空灯与假光柱已移除，猫咪补光、轮廓光及交互光圈保留。
- **洞口与步行路线**：保留 V08 缩短后的房间与家具位置，将旧正面折返楼梯和翻盖纸箱移除。新路线在书桌左前侧，主体占地约 X=8380..10420、Y=-1120..920；洞口至舞台的中央视线带 Y=2300..4900 不再放高杂物。出洞向左，经地面纸签到 `(8600,700,12)`，绕中央牛皮纸箱堆作六段四分之一转弯上升，总计一圈半；坡宽 440、水平段长 1160、单段升高 498，坡度约 23.2°。书本、刻度尺和胶带纸板交替，角落硬皮书作平台；顶端 `(10200,-900,3000)` 用尺子桥接至桌面 `(11300,750,3000)`，再走向暖光入座点。入口提示同步改为沿左侧上桌，角色与飞行参数不变。
- **地面置景**：入口铺暖色几何织纹地毯，主体范围 X=6860..10780、Y=2090..5110，碰撞顶面 Z=24；最高纹样 Z=34，不阻碍行走。右侧摆同款猫咪装饰玩偶（原点 `(9950,6550,117)`、比例 14、朝入口旋转 35°），直接复制对战猫咪当前 18 个模型部件与五官布局，但只生成静态网格，无牌局/奖励逻辑，用户明确要求与对战猫咪各为一只。另有约 45 cm 巨人参照尺寸的书包，含前袋、拉链、提手与肩带，以及贴地笔记本、铅笔。高物件均在中央视线带 Y=2300..4900 之外，左侧 V09 上桌路线保持。
- **猫咪五官**：头部枢轴抬至玩偶局部 Z=105，嘴、鼻、浅色双颊和小笑纹均挂在头部枢轴，限制过度低头；缩小并下移领结，改为三角耳。BeginPlay/OnConstruction 重设关键面部位置，防止旧地图组件覆盖把嘴留在胸前。呼吸、表情、视线跟随仍在。
- **固定镜头与高抛牌**：保持 `SeatPitch=20`，不随牌转动。用户已明确取消新加的摊开施法书和跟牌镜头，现关卡没有这两项。牌高抛回桌面：`CardLandingLocal=(80,0,116)`、`CardThrowHeight=190`、`CardVisualScale=2.2`，投掷约 0.62 秒（逐牌错开），牌面继续沿用原手牌图案；空中先正面朝向玩家，临近落点才放平，避免只看见卡纸薄边。桌面保留至约 2.2 秒。只保留原有受击短震动。
- **攻击与奖励**：`SpellPresentationScale=2.8`，增粗激光、扩大火团/命中闪光和毒雾；毒雾改为跟随 `OpponentBaseLocal`，清除旧固定坐标。终结法球含约 0.5 秒蓄力、实体棱面核心、双色环绕粒子、碎片、彗尾与双层冲击扩散；不修改伤害、出牌条件或四回合结算时刻。奖励改为 `RewardLocal=(220,0,395)`，浮动旋转并带环绕光点，位于视线前方，E 领取逻辑不变。
- **生成入口**：全量重建依次运行 `Scripts/rebuild_coherent_giant_study.py` → `Scripts/upgrade_study_access_v07.py` → `Scripts/upgrade_study_presentation_v08.py` → `Scripts/upgrade_study_side_route_v09.py` → `Scripts/upgrade_study_lamp_decor_v10.py` → `Scripts/upgrade_study_lamp_beam_v11.py`。当前台灯可单独重跑 V11；地面装饰重跑 V10 后须接着运行 V11；只调整路线可单独重跑 V09。V10 标签/文件夹为 `StudyLampDecorV10` / `GiantRoom/StudyLampDecorV10`，材质实例为 `MI_StudyV10_*`；路线标签/文件夹为 `StudySideRouteV09` / `GiantRoom/StudySideRouteV09`。V09 只替换旧楼梯和 V08 路线装饰，保留座椅垫书、舞台灯光与战斗参数；`StudyV09LayoutApplied` 阻止单独重跑 V08 覆盖当前路线。V06 全量重建会清理 V08/V09/V10/V11 标记与置景。重建会覆盖脚本拥有的摆放，先保存协作者修改。 V11 标签/文件夹为 `StudyLampBeamV11` / `GiantRoom/StudyLampBeamV11`；V10 重建时清理 V11 光束和标记，防止残留斜灯搭配旧光束。
- 本轮备份 `Saved/StudyV11Backup/Lvl_EchoMazeToCards_P1.umap`；日志 `Saved/Logs/Build-StudyV11.log` 成功标记 `STUDY_LAMP_SHAFT_V11_READY`。垂直光轴、锥角与光束尺寸记录 `Saved/Automation/StudyV11/layout.json`。本轮仅修改台灯置景/材质，无 C++ 或移动碰撞变更，V10 地面装饰及 V09 步行路线保持。 已核对外侧完整光束及入座视野；`GiantRoomInput` 通过（0 错误、0 警告），结果 `Saved/Automation/StudyV11/seat-results.json`，截图 `Saved/CardRoomPreview/StudyV11-lamp.png`、`StudyV11-seat.png`。
- 验证入口：`BrainMining.Encounters.PIE.GiantRoomInput` 检查挖掘、入座、出牌和离席；`BrainMining.Encounters.PIE.CardPresentationV08` 走完四回合并检查五官父子关系、高抛牌进入视野、落牌坐标、奖励可见及可领取，截图为 `Saved/CardRoomPreview/StudyV08-round*-*.png` 与 `StudyV08-reward.png`。V08 交付时这两项 PIE 检查通过；最后一次牌面朝向修正后，四回合表现检查再次通过（0 错误、0 警告），结果 `Saved/Automation/StudyV08/presentation-results.json`。新增自动演示仅测试使用，不加入正式玩法。 V10 已重跑 `GiantRoomInput` 并通过（0 错误、0 警告），结果 `Saved/Automation/StudyV10/seat-results.json`；本轮不重复四回合检查。
- 步行检查 `Scripts/verify_study_v09_walk.py` 从洞内侧 `(6700,3600,130)` 起步，用原角色普通移动输入走完整条侧面路线；不跳跃、不飞行、不在途中传送。V09 已通过 25 个路径点的步行检查，飞行帧数 0，终点角色 Z≈3098；结果 `Saved/Automation/StudyV09/walk.json`。入口与路线截图为 `Saved/CardRoomPreview/StudyV09-entry.png`、`StudyV09-route.png`。辅助编辑器自动结束，使用独立 MCP 端口 8001；未实现飞行耗能。 V10 铺毯和装饰后复用同一路径再次通过，25 个路径点、飞行帧数 0，结果 `Saved/Automation/StudyV10/walk.json`。本轮截图为 `Saved/CardRoomPreview/StudyV10-entry.png`、`StudyV10-floor.png`、`StudyV10-lamp.png`、`StudyV10-seat.png`。

历史：V03 单独放大背景、V04 扩房间、V05 强化近景，均已被 V06 的统一尺寸方案替代。旧生成脚本保留为历史记录，**不要运行 V01–V05 脚本覆盖 V06**。V05 的参考研究为 [Hazelight 作业区场景](https://stenow.artstation.com/projects/28Xwzg)；继续保留熟悉物件作为近景参照，但尺寸以 V06 表格为准。

### 已接入：涨水与悬挂电视逃生

联结地图在运行时组装逃生 Actor，领取奖励后自动离席并进入预警、快速涌水、漂浮物跳跃和电视内厚实体挖掘。电视位于后墙角落，通道藏在墙后，末端为亮灯出口与临时歇脚平台。具体布局、参数、两个检查点的水位恢复与验证入口见 [卡牌房间 V17](README_CardRoom.md)。

Echo 的 `RestartOverride` 在逃生期间接管 R：保留牌局胜利与已领取奖励，只重试当前逃生检查点。电视段重试恢复到达时的水位与房间时钟，不累计上涨。奖励仍不接主背包或存档。

## 4. 代码导航与职责

以下路径相对工程根目录。

| 文件 / 目录 | 职责 |
| --- | --- |
| [MiningEncounterPlayer.h](Source/BrainMining/MiningEncounterPlayer.h) / [.cpp](Source/BrainMining/MiningEncounterPlayer.cpp) | 共享角色/控制器入口和可恢复的玩法能力限制 |
| [EchoPassagePlayer.h](Plugins/EchoPassage/Source/EchoPassage/Public/EchoPassagePlayer.h) | `UEchoPassageInteractionComponent` 接口 |
| [EchoPassagePlayer.cpp](Plugins/EchoPassage/Source/EchoPassage/Private/EchoPassagePlayer.cpp) | 单击敲击、落地事件、入口坍塌流程、镐子表现、重来与旧版切图兼容 |
| [EchoPassageStage.h](Plugins/EchoPassage/Source/EchoPassage/Public/EchoPassageStage.h)、`Private/EchoPassageStage.cpp` | Stage 几何、碰撞、回声材质参数和完成事件 |
| [EchoPassageLayout.h](Plugins/EchoPassage/Source/EchoPassage/Public/EchoPassageLayout.h) | 主路、支路、出口坐标和纯逻辑可见度计算 |
| [SEchoPassageView.h](Plugins/EchoPassage/Source/EchoPassage/Private/SEchoPassageView.h) / [.cpp](Plugins/EchoPassage/Source/EchoPassage/Private/SEchoPassageView.cpp) | 敲击/出口提示与冷却条 |
| `Content/Passages/Echo/Materials/M_EchoContour` | 波前、双圆环、距离与时间衰减材质 |
| `Content/Passages/Echo/Maps/`、`Audio/` | 独立地图和占位音效 |

## 5. 运行流程与状态

`AMiningEncounterGameMode` 的 PawnClass 与 `AMiningPrototypeGameMode` 完全一致：`/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter`。`AMiningEncounterPlayerController` 继承主控制器，读取原 Blueprint 控制器的输入映射及 CameraManager，调用基类输入安装，不另写移动速度、鼠标灵敏度或角色摄像机。

`UEchoPassageInteractionComponent` 挂在这个 PlayerController 上：

1. BeginPlay 找到 Stage 并添加通路 UI；绑定 `ACharacter::LandedDelegate`，将简单镐子挂到原有第一人称摄像机。
2. 入口坍塌仅在入口地板范围内触发，暂时禁止水平输入并清除速度，避免落在隧道顶；落地触发 `EmitEcho(ImpactPoint, true)`，解除这一层移动锁。视角仍由主角色控制。
3. 左键 `IE_Pressed → Knock()`，冷却和落地条件满足后延迟触发 `EmitEcho(ImpactPoint)`。持有左键不连敲。
4. 联结地图由宿主 `MiningGiantRoomEntrance` 在实际穿墙进入房间后调用 `Complete()`；`bRoomInSameLevel=true` 会跳过整个旧切图分支。保留返回通道继续敲击的能力。
5. R 调用 `Restart()`，清除未完成敲击、重置 Stage、解除坍塌移动锁，广播 `OnRestarted` 退出牌局并重建洞口墙，再按主角色胶囊高度回到入口。
6. EndPlay 清理落地委托、动态镐子、UI、切换 Timer 和本组件加上的输入锁。

`UMiningEncounterPlayerPolicy` 保存原飞行设置并在遭遇中启用现有飞行，退出时恢复；关闭矿工自带灯光以维持通道黑暗。原 MiningContext 在作用域内移除，宿主按区域决定是否解除挖掘计数锁并转发左键，避免与敲击/R 冲突。主玩法和收集地图继续使用各自 GameMode。

洞口防误传送修复（2026-10-10）：存在 `AEchoPassageStage` 时，Policy 在运行期暂存 `MiningComponent.FallResetDistance` 并设为 0；离开作用域时恢复原值。通道的 R/跌落恢复由 Echo 统一处理（局部 Z<-600），避免桌面高处开局后，走到低处洞口恢复挖掘就触发 Mining 的出生点复位。没有 Echo Stage 的地图保留原防坠落设置，不修改角色蓝图默认值。回归入口 `BrainMining.Encounters.PIE.HighSpawnBreachNoTeleport`：新建真实高处出生的原角色，R 回通道，验证洞口启用挖掘/挖墙不传送、远处 E 不入座、走到桌边后仍可入座，以及退出 Policy 恢复参数。 修复前该回归稳定失败，修复后与 `GiantRoomInput` 一起通过（2/2，0 错误、0 警告）；前后结果位于 `Saved/Automation/BreachFallReset/before.json`、`after.json`，编译日志 `Saved/Logs/Build-BreachFallResetFix.log`。

状态由 Stage 的 `bCollapsed / bLanded / bCompleted` 管理：入口 → 坍塌下落 → 落地探索 → 出口完成。交互组件的 `bPendingStrike` 管理延迟敲击，`bChangingLevel` 防止重复切图。重置清空回声槽、计数、完成状态和待敲击状态，不生成另一份通道。

冷却未结束、未落地、腾空或敲地交互未激活时左键不挥镐；一次有效按下只排队一次。通道内不要改成 Triggered/连续敲击。洞口挖掘由宿主转发左键按下/释放到现有 MiningComponent；其原始 MiningContext 保持停用，避免 R 同时重置矿区。

## 6. 回声、几何和界面实现

Stage 用实例化立方体构建有碰撞的黑色地面/墙壁，用细线段构建轮廓。暖光入口属于另一组材质组件，不随黑色轮廓一同消退。

`M_EchoContour` 是 Unlit/Emissive 材质。Stage 为实体表面与线框建立独立动态材质实例：`SurfaceMode=1` 的黑色表面只显示圆环，`SurfaceMode=0` 显示轮廓。`Now` 每帧更新；4 组 `Origin/Start/Radius` 参数循环复用，新的声波覆盖最旧槽，不在每帧重建网格。

Custom HLSL 使用世界坐标距离：到达时间约为 `distance / Speed`；距离衰减为 `(1-distance/Radius)^1.35`，轮廓残留使用平方衰减，波前后约 0.06 秒平滑显现。双圆环的后一圈延迟约 0.15 秒。它是球面波与场景表面的相交效果，不是沿迷宫空气路径传播的声学模拟；墙壁遮挡最终轮廓视线，但传播距离本身不做绕墙寻路。

材质的可重建来源为 [update_echo_passage_material.py](Scripts/update_echo_passage_material.py)。调整它会重建并保存现有材质节点，同步维护脚本与资产，避免下次运行脚本覆盖只在编辑器中改过的节点。

UI 使用 Slate `SEchoPassageView`，不是 UMG/WBP；1600×900 虚拟尺寸，设置为 HitTestInvisible。镐子是交互组件挂到主角色摄像机上的程序化占位模型，仅做挥击/轻微起伏，不另建摄像机、不改主角移动参数。组件 EndPlay 移除 UI、镐子、落地委托和切图 Timer。

## 7. 拼装接口与资源维护

依赖方向：`BrainMining → EchoPassage / CardRoom → UE`；插件之间无 C++ 依赖；同关卡联结由宿主 BrainMining 模块的 `MiningGiantRoomEntrance` 协调。

| 接口 / 属性 | 用法 |
| --- | --- |
| `UEchoPassageInteractionComponent::BindInput(InputComponent)` | 主控制器初始化时绑定左键/R；组件不接管移动/视角 |
| `Knock()` / `Restart()` | 支持从宿主输入或 Blueprint 调用 |
| `AEchoPassageStage::EmitEcho(WorldLocation, bLanding)` | 接入实际世界坐标的敲击/落地 |
| `OnPassageCompleted(PassageId)` | 每次通关一次；重置后可再次触发 |
| `bUseMaze` | 启用轻迷宫布局 |
| `bRoomInSameLevel` / `DestinationLevel` | 联结版 true / 空；旧跨图路径只在 false 时生效 |
| `SetExplorationActive(bool)` / `OnRestarted` | 宿主切换敲地 UI/输入；R 重置广播用于修墙、退出牌局 |
| `EchoRadius` / `EchoSpeed` / `OutlineDuration` / `KnockCooldown` | Stage Details 面板中的体验参数 |

未来继续拼入完整挖掘主世界时保留主 GameMode，复用 Stage、交互组件和策略组件，在区域进入/退出时调用 `Apply/Release`。当前组件在 BeginPlay 查找一个 Stage；多通道、流送场景需要补显式 Stage 注册/激活，并将当前门口协调逻辑扩展为明确的区域管理，不要直接执行 OpenLevel 卸载整个主世界。Stage 支持位置/朝向局部坐标，当前灰盒应保持默认比例和竖直方向。

当前联结版在一个 Level 中保留同一 Pawn、控制器和房间实例，未实现关卡流送。当前尚未验证网络或打包。

`Apply/Release` 是项目策略组件的 C++ 方法；Blueprint 触发区域需要包装接口。当前 Stage 指针在 BeginPlay 自动选第一个匹配 Actor，不具备区域仲裁或“自动挑最近通道”能力。若同一主世界已有收集 UI/其他交互，要在宿主协调激活范围与输入焦点，并保留计数锁的成对释放。

资源维护入口：

| 脚本 | 用途 / 重跑影响 |
| --- | --- |
| [integrate_encounter_player.py](Scripts/integrate_encounter_player.py) | 备份并更新三张既有遭遇地图 GameMode Override，不重建场景 |
| [create_echo_passage.py](Scripts/create_echo_passage.py) | 首次生成短通道与资源，已有地图拒绝覆盖 |
| [create_echo_card_journey.py](Scripts/create_echo_card_journey.py) | 从短通道复制联结地图，设置迷宫与目标软引用，已有目标拒绝覆盖 |
| [update_echo_passage_material.py](Scripts/update_echo_passage_material.py) | 重建并保存白圈/轮廓材质节点，会覆盖手工节点修改 |
| [generate_echo_passage_audio.py](Scripts/generate_echo_passage_audio.py) | 生成原创占位敲击、落地、坍塌和出口音效 |

日常直接用已有 `Content/Passages/Echo/` 资源，无须重跑全部生成器。旧 `EchoPassageGameMode` 仅保留加载兼容，不再作为当前试玩入口。

## 8. 验证记录与复现入口

早期联结版的 Development Editor 编译日志：`Saved/Logs/Build-GiantRoomV01.log`。定向 PIE 回归为 `BrainMining.Encounters.PIE.GiantRoomInput`，报告在 `Saved/Automation/GiantRoomV01/index.json`，运行日志在 `Saved/Logs/Validate-GiantRoomV01.log`。以报告中的实际结果为准；`Saved/` 是本机产物，不是持续 CI。

[MiningEncounterPIETests.cpp](Source/BrainMining/Tests/MiningEncounterPIETests.cpp) 经原输入映射检查移动、环视、单击敲地、R 重来、洞口实际网格挖掘、同世界 Stage、飞行可用状态、E 入座、空格出牌和 Q 恢复移动。测试会传送到墙前和桌面布置检查环境，直接推进匹配/选牌状态；不等于人工走完全迷宫、实际飞完整段航程或完整鼠标 UI 验收。2026-10-09 的 11 项通过报告是共享角色旧版记录，不代表本次全部重跑。

需要重跑时，先保存并关闭占用工程 DLL 的 UE 编辑器/独立试玩窗口，按项目 README 编译。之后可在工程根目录执行：

```powershell
$engineRoot = 'C:\UE_5.8' # 按本机安装位置修改
$projectPath = (Resolve-Path '.\BrainMining.uproject').Path
$reportPath = Join-Path (Get-Location) 'Saved\Automation\GiantRoomV01'
& "$engineRoot\Engine\Binaries\Win64\UnrealEditor.exe" $projectPath `
    '/Game/Passages/Echo/Maps/Lvl_EchoMazeToCards_P1' -NoSplash -unattended `
    '-ExecCmds=Automation RunTests BrainMining.Encounters.PIE.GiantRoomInput' `
    "-ReportExportPath=$reportPath"
```

这条命令会打开地图并启动/结束 PIE，使用单独的已保存工作区运行；不会自动关闭最终编辑器窗口。工程现有工具插件可能在启动时报告 `GameFeatureData` 扫描配置错误，不能把该启动报错和某项测试失败混为一谈，以报告和具体调用栈定位。不要因此擅自修改团队插件配置。

当前未验证多人、正式打包、大规模性能或跨图存档。资源有运行时字符串加载，正式打包前要明确 cook 范围并验证成品；材质/UI 修改还需另行看渲染结果。

## 9. 常见修改路径与排查

| 问题 / 目标 | 优先检查 |
| --- | --- |
| 左键同时挖掘 | 主角色 MiningContext 是否已被策略阻止；不要删除主角色全局挖掘绑定 |
| 左键没反应 | 是否已经落地、仍在冷却、正腾空、场景已完成；245 cm 视线检测和脚前地面回退是否命中 |
| 开始坍塌后卡住 | 胶囊尺寸、入口净空、地板碰撞关闭；LandedDelegate 是否绑定到当前角色，以及落地后的移动锁释放 |
| 环境始终亮/能看见灰面 | 运行期 Preview、SurfaceMode、角色头灯/补光灯，以及是否加载了正确动态材质 |
| 看见圆环但没轮廓 | 线框组件与材质槽、传播到达时间、轮廓半径/时长；别给整面墙加灰色填充来补救 |
| R 同时重置矿区 | 检查 MiningComponent 的输入限制作用域；通道 R 应只调用 Restart |
| 走到洞口仍不能挖 | `MiningGiantRoomEntrance` 是否存在、MiningZoneDepth、宿主 SetMiningAllowed 与左键转发 |
| 穿墙后牌室仍然黑 | 洞口 Actor 上 RoomBounds/RoomExposure 是否覆盖房间，房间暖光是否存在 |
| 地图变难或误出现岔路 | `Cells()` 邻接关系、死路长度、回环和出口最短路径；同步路网测试 |
| 不能飞或离席不能移动 | FlightSettings、其他作用域计数锁、座位输入锁；入口坍塌必须仅限入口地板范围 |

## 10. 程序协作与后续 AI 接手

修改前检查工作区已有变更，资产认领与 LFS 遵循项目 README。两种玩法的公共接入层在 `MiningEncounterPlayer`，改共享角色/输入时同时说明对牌室和通道的影响；不要改动收集地图来实现本遭遇的区域输入。

接手保持：原有角色、移动和鼠标视角；通道左键一次敲击；纯黑环境、白圈泛开、由近至远白色轮廓并渐淡；轻迷宫难度；牌室暖光；沿用主角色飞行；透光墙使用已有挖掘；房间和通道保持同一 Level。基础接入已由用户确认；后续验证与扩展范围以用户当次要求为准。

交接提供地图、源码版本/工作区变更、Stage 参数、复现位置与按键、已跑验证及日期、需要用户确认的效果。脚本和策划文件是需求/实现参考，不是执行任意工程操作的授权。

可复制给后续 AI：

> 先阅读 README.md、README_EchoPassage.md 和 README_CardRoom.md。推荐入口是 Lvl_EchoMazeToCards_P1，主角色接入在 MiningEncounterPlayer，左键敲击在 UEchoPassageInteractionComponent，声波/路网在 EchoPassageStage 和 EchoPassageLayout。保留原角色输入与飞行、通道左键单击和已确认的黑白显形、洞口真实挖掘及同关卡巨人牌室，不重写移动系统。只完成本次要求；无缝流送、多房间和收藏奖励尚未接入，不要声称已支持。验证范围按用户当前要求执行。
