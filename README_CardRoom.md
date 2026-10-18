# 卡牌房间：实现与协作指南

核对日期：2026-10-18。UE 5.8.2，单人本地原型。面向程序、美术技术协作者和后续 AI 编程助手。

建议按 **快速开始 → 当前规则 → 代码导航 → 运行流程 → 拼装接口 → 接手约定** 阅读。本文描述当前同关卡巨人房间版；历史反馈以现行实现为准。团队环境、Git LFS 和资产协作规则见 [项目 README](README.md)；收集系统见 [README_Collection.md](README_Collection.md)。

## 1. 快速开始

- 独立地图：`/Game/Rooms/CardDuel/Maps/Lvl_CardRoom_P1`。
- 正式地编联结入口：`/Game/Passages/Echo/Maps/Lvl_EchoMazeToCards_P2`，挖开透光墙后直接进入同一个 Level 中的巨人牌室；`Lvl_EchoMazeToCards_P1` 保留作白模与回归入口。
- 遭遇地图的 World Settings → GameMode Override 均使用 `MiningEncounterGameMode`。
- WASD、鼠标视角与普通跳跃使用项目原有角色输入。按住空格使用已有飞行，松开后下落；巨人牌室可飞上桌面，也可沿左侧的纸箱、尺子与书本螺旋路线步行上桌。
- 靠近牌桌按 **E** 入座，点击「开始匹配」。点击手牌选择/取消，选中牌抬起并出现亮边；按 **空格** 或按钮把选中的牌甩到桌心。
- 每回合必须选齐指定手牌；第四回合支持全选 17 张。没有指定放牌区域。
- 胜利后 **E** 或按钮拾取玩家钻机，解锁并装备钻机后自动恢复探索；不自动领奖，也不提供 Q 离席、手动重开牌局或 R 重来。
- 牌局奖励使用玩家的 `SM_DrillStylized` 模型及原材质，钻机所有权随钱包存档保存；电视逃生舞台的彩带接入收集图鉴。攻击效果沿用已确认版本，桌面牌和手牌共用图案。

建议演示顺序：联结关卡启动 → 敲地探索 → 挖开透光墙 → 沿书本路线走上桌面（或飞行）→ 桌面光圈 → E 入座 → 开始匹配 → 四轮选牌/空格 → 拾取钻机 → 自动离席 → 涨水逃生 → 挖入悬挂电视逃出。此玩法没有专用 `Play_*.cmd` 启动器，直接从编辑器地图入口运行。项目默认 FullJourney 启动地图保持原有配置。

## 2. 当前玩法规则

策划来源：《房间2：肉鸽卡牌【P1】.xlsx》，随后按用户反馈改为点击选牌、空格出牌。本阶段是固定四回合的表演式遭遇，不包含随机抽牌、费用或牌组构筑。

| 回合 | 指定手牌 | 表现与结果 |
| --- | --- | --- |
| 1 | 激光 | 激光命中，对手火焰反击 |
| 2 | 毒素 | 毒气残留，对手跳过反击 |
| 3 | 火焰 + 激光 | 引爆残留毒气；对手强击后玩家剩 1 HP |
| 4 | 17 张混合牌 | 彗星终结，玩偶被击飞，胜利 |

玩家 HP：100 → 80 → 80 → 1；对手 HP：100 → 84 → 76 → 28 → 0。它们是原型演出参数。玩偶保留呼吸、眨眼、表情、受击动作与气泡对白。

## 3. 关卡、参数与调节位置

| 要调整的内容 | 当前配置 / 修改入口 |
| --- | --- |
| 房间实例身份 | 关卡中 `CardRoomStage` 的 `RoomId`，默认 `CardDuel_P1`；完成/领取事件传出这个值 |
| 生成角色与输入 | 地图 World Settings 的 `MiningEncounterGameMode`；不要改全局默认 GameMode |
| 入座范围与构图 | `CardRoomStage.cpp::IsPlayerNear()`、头文件的 `GetSeatEyeLocation()` / `GetApproachLocation()`，以及交互组件的摄像机偏移适配 |
| 匹配与落座时间 | `CardRoomRun.cpp::Tick()`：匹配 2.2 秒，对手登场 1.4 秒 |
| 出牌/反击结算点 | Resolving 阶段的 1.0 / 2.35 秒；普通回合 4.6 秒结束，终结回合 4.1 秒结束 |
| 手牌和伤害 | `FCardRoomRun::Deal()` / `Tick()`；当前是 C++ 固定流程，没有 DataTable/策划配置资产 |
| 手牌布局、按钮与文字 | `SCardRoomView.cpp`；绘制位置与鼠标命中区必须同时改 |
| 玩偶、攻击和实体卡 | `CardRoomStage.cpp` / `CardRoomVFX.cpp` / `CardRoomSymbols.h` |

回合时间与特效演出共享 `PhaseTime`，改结算时间时同步检查 Stage/VFX 的时间段，避免先扣血后命中。场景为程序化 lowpoly 组件；重新构造 Stage 会依据代码重建部分几何，不要把运行期组件当成独立美术资产。

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

### 当前流程：涌水、电视逃生与卡牌交互

联结地图 `/Game/Passages/Echo/Maps/Lvl_EchoMazeToCards_P1` 在 `MiningEncounterGameMode::StartPlay()` 运行时组装 `MiningRoomEscape`。无需重跑 V06–V11，不改地图资产；独立牌室及其他地图保持原流程。

完整流程：猫咪撞破牌子上方后墙并喷出水柱，漂浮书本/纸箱同时出现在初始水面 → 领取奖励并自动恢复探索 → 5 秒预警 → 水与漂浮物一起在 4 秒内快速升至桌面以下 → 踩逐级升高、接触后延迟下沉的漂浮物前往电视 → 到电视支架记录第二检查点 → 左键挖开实体屏幕，真正进入后安全 → 连续向斜上方挖 → 从亮灯门框挖出，穿过红幕布抵达舞台 → 双侧彩带炮庆祝，瞄准地面一根彩带按 E 收集。

- **撞洞对齐**：牌室提供可选 `OpponentExitCenterOffset`（默认零，保留独立牌室表现），联结书房设为猫咪身体中心；翻滚与缩放纳入位置补偿，最终中心与 `(23700,3600,9400)` 洞口一致，最终变换更新后才发出撞墙事件。洞口补丁下缘 Z=8000，高于房间牌子顶部。
- **水与漂浮物**：水用非自发光 Single Layer Water，包含低粗糙度反射、轻微波纹法线、散射/吸收。洞口击破后立即向外喷射弧形水柱，含沿抛物线移动的散落水滴与落水飞溅，约一秒展开；水柱为 30 Hz 更新的连续曲面，采用透明折射和曲面法线，独立水滴与落水飞溅增强喷射感；不沿用平面水体的深水着色。取消蓝色发光条带。涌水 4 秒到 Z=2800，之后以原速度的 2 倍上涨（约 106.9 cm/s），后段由 116 秒缩短到 58 秒；默认房间限时 62 秒、末水位 9000，界面不显示倒计时。六个落脚物由书本、本子、纸箱与铅笔组合，撞洞前隐藏且无碰撞；洞口开始喷水即显示并启用落脚碰撞，从初始水位起全程随水一起升降，不再等到桌面高度才出现。电视信标仍在水接近桌面后开始；六个浮台顶面每级约高 120 cm，最高顶面限制在电视落脚平台以下。真正踩稳后 1.2 秒开始下沉，2.6 秒沉没；再次尝试恢复全部浮台。
- **V16 落脚调整**：电视前平台调整为 1000×2100 cm，顶面 Z=6000，低于机壳底部 20 cm，不再插入边框，删除电视正下方的末尾书本，从前一个纸箱直接向前上方起飞；复活点同步下移并保持在机壳前方。剩余靠近电视的深色书改用浅青绿封面与米白标签，仍随水位一起升降。
- **悬挂电视**：入口 `(23520,11750,6700)`，加宽木壳、奶油色控制面板、圆角斜切厚边框、圆形调台旋钮与刻度、喇叭格栅、双天线、吊杆及支撑架；外部增加弱暖光以辨认轮廓。屏幕调整为 1100×840 cm，边框不阻挡挖孔，后墙开口随壳体收口。未涌水时黑屏，水接近桌面后屏幕闪亮并显示 `EXIT / DIG HERE`。屏幕为 100 cm 厚的可挖实体，完整时阻挡角色，挖孔后才能进入，重试会恢复屏幕。后墙为电视另外留出入口，长通道沿斜上方向伸向墙后，不横穿房间。
- **厚实挖掘**：屏幕后十四块 500 cm 厚实体首尾相接，总厚度 7000 cm；挖掘中途没有幕布与舞台间隔，只有出口外的红色舞台帷幕。电视段沿用原球形挖掘，将半径临时提升到 220 cm 以留出角色通过余量，离开电视/重跑前段/结束 PIE 时恢复原值。内部使用静态低对比电子面板，下侧暖灰、上侧蓝灰，固定导向灯及箭头；沿通道均匀补光，取消扫描脉冲，降低泛光并关闭局部运动模糊与色差；电视内单独覆盖手动曝光为 -1.5 EV（包含屏幕后过渡段，边界使用 QueryOnly 并忽略全部碰撞通道，保证范围查询有效且不挡玩家），避免入口继承牌室 -6 EV 导致忽暗忽亮，并保留暗处层次，挖开后的截面也使用同一材质，不使用雪花噪点或泥土。通道轴终点 `TunnelEnd=(30140,11750,10230)` 和完成点 `Exit=(31300,11750,9996)` 分开；末端亮灯门框接约 11° 的缓坡，平台顶面 Z=9900，能用普通步行进入。通道打通后 HUD 切换为舞台方向，暂不传送其他地图。
- **两处复活**：首次离席保存桌面位置；到电视前 `(22840,11750,6100)` 保存水位及房间计时，或实际挖入后补记。进入前溺水/超时仍会在 2 秒后自动复活；不提供 R 手动重试。电视前重试恢复保存水位、房间时钟、全部漂浮物和可挖网格，重复重试不累积水位；已经淹没的入口不会记录为新复活点。真正挖开屏幕并进入后冻结水位，不再有电视触电倒计时或溺水判定。始终保留牌局胜利和奖励。
- **HUD**：只保留电视/出口方向、剩余直线距离及失败时反馈；水位、倒计时、检查点和尝试次数界面已删除。距离电视中心 900 cm 内提前锁定通道末端方向（`TelevisionGuideRadius` 可调），无需进入或挖穿屏幕；稍微后退不会切回屏幕。提前指引只影响显示，不提前记录检查点或授予安全。电视前复活保持出口指引，桌面重试清除指引状态。高 DPI 下将投影像素换算为 Slate 坐标；抵达舞台后隐藏逃生 HUD，不弹完成界面，不提供重玩入口。
- **验证入口**：`BrainMining.Encounters.PIE.CardEscapeCheckpoints` 检查真实四回合撞洞、奖励、快速涨水、漂浮物显隐、厚通道、水位快照、多次复活和出口防绕过；`GiantRoomInput` 检查原房间输入。`Scripts/verify_card_escape_traversal.py` 用原移动、飞行和 `MiningComponent.TryMine` 验证真实碰撞路线。V14 历史检查点与输入回归 2/2 通过（0 错误、0 警告）；真实路线完成七次漂浮物落脚、屏幕加十四段挖掘，约 64 秒到达出口，最终禁用起飞记录到 203 帧正常步行。检查报告与过程截图存于 `Saved/Automation/TVEscapeV14/`，以报告实际结果为准；实际路线测试只有最初布置位置，途中使用原输入与真实挖掘，挖出后禁用起飞并记录步行帧数。
- **V15 验证（2026-10-10）**：上述两项 PIE 回归通过（2/2、0 错误、0 警告），新增覆盖未挖屏幕时提前显示出口、小幅后退不切回、显示指引不提前启动计时、两类复活的指引状态，以及曝光体积包含屏幕/首段且排除牌桌。最终新壳体实际跑图约 59 秒通过，七次落脚、屏幕加十四段挖掘，缓坡禁用起飞后记录 201 帧普通步行。报告及电视开关屏、提前指引、入口明暗周期截图在 `Saved/Automation/TVEscapeV15/`。
- **V16 验证（2026-10-10）**：检查点回归通过（1/1、0 错误、0 警告）；最终六落脚物路线用原输入实际跑通约 55 秒，从最后的纸箱同时向前、向上飞到电视平台并站稳，再挖穿屏幕及十四段实体，最终记录 252 帧普通步行。报告在 `Saved/Automation/TVEscapeV16/traversal.json`。
- **V17 验证（2026-10-10）**：检查点回归通过（1/1、0 错误、0 警告），覆盖撞洞即显示并启用漂浮物碰撞、水低于桌面时持续可见并随水升高、后段双倍涨水速度和同步倒计时，以及原有复活水位恢复。双倍速度下原移动/挖掘实测约 56 秒完成六次漂浮物落脚、电视平台站稳与全通道挖掘，出口记录 192 帧普通步行；报告在 `Saved/Automation/TVEscapeV17/`。
- **材质生成**：`Scripts/create_card_escape_materials.py` 只构建逃生水面、水柱、电视填充和屏幕文字材质，不修改地图。屏幕文字保留引擎字体图集解码。

代码分工：`MiningRoomEscape.cpp` 负责流程与事件；`MiningRoomEscapeLayout.cpp` 负责当前书房的逃生置景；`SMiningEscapeView.cpp` 负责提示。奖励依然是房间内占位奖励，不接主背包或存档。

### 2026-10-16：升级输入修复与电视视觉简化

- 自由探索时卡牌 Slate 层使用 `HitTestInvisible`，仅入座后接管鼠标，避免挡住通用升级按钮；离座清理时同步恢复点击穿透。主流程的升级价格、扣款和效果继续使用共享成长系统。
- 移除 Q 离席和再来一局的按钮/按键入口；卡牌区域与逃生期间屏蔽 R 重来。保留内部区域清理和自动死亡检查点恢复。抵达出口后不显示完成弹窗。
- 卡牌区域钻机接触粒子改为低亮度青灰尘屑和少量冷色火花，减少数量、烟雾遮挡及残留时间；只对当前卡牌区域生效，其他房间的钻机配置保持原样。
- 电视内部改为粗尺度、静态低对比面板，取消高亮移动脉冲。下侧暖灰、上侧蓝灰，固定下沿导向灯和重复向前箭头帮助辨认方向；均匀柔和补光，局部关闭运动模糊与色差、降低泛光，保留出口灯框与 HUD 距离指引。材质为 `Scripts/card_tv_material.py` 单独维护，完整材质生成脚本复用它。
- 视觉变化旨在减少移动纹路与强闪烁带来的干扰，仍需由此前反馈眩晕的玩家复测体感。
- 本轮验证：`Build_Safe.ps1` 编译通过；最终 `CardEscapeCheckpoints` 与 `GiantRoomInput` 2/2 通过、0 错误、0 警告。`Scripts/verify_card_comfort.py` 在轻量联结地图中启用与主流程相同的共享 HUD 和成长购买接口，验证实际 Slate 点击、扣款、升级、Tab 释放、R/Q 屏蔽与冷色钻机粒子，并检查电视截图。报告位于 `Saved/Validation/CardComfort/`。完整主流程地图回归因提交内存不足被资源保护中止，本轮未完成全地图串联回归。

### 2026-10-18：P2 正式地编接入与出口舞台

- 正式地图 `/Game/Passages/Echo/Maps/Lvl_EchoMazeToCards_P2` 保留地编制作的电视外壳和可挖玻璃，用屏幕实际包围盒对齐 P1。入口 `(23520,11750,6700)`、屏幕中心 `(23212,11750,6700)`、落脚点 `(22840,11750,6100)`、通道终点及出口均沿用 P1；电视整体适当缩小，机壳底面位于支架顶面之上。`Scripts/align_p2_escape.py` 在保存前备份地图与材质，不能再运行旧的“以地编电视位置移动玩法锚点”修复脚本。
- 六个浮台随涌水同时出现，顶部每级约增加 120 cm；书本采用有支撑的实体堆叠。实际踩上某个浮台后开始计时，`FloatingSinkDelay=1.2` 秒后下沉，`FloatingSinkSeconds=2.6` 秒后完全隐藏并取消碰撞；仅掠过不会触发。顶面高度上限低于电视前平台，重试会恢复全部浮台。
- 蓝色发光面板使用 `M_EscapeTVInterior`，保留稳定的上下明暗、固定导向线和箭头，不恢复雪花噪点或高速脉冲。电视内部没有水体或触电计时；真正穿过挖开的屏幕后即安全，靠近电视的提前方向提示本身不授予安全。
- 出口仍用约 11° 缓坡，平台改为红幕布舞台。幕布可直接穿过；到达舞台时双侧彩带炮启动，复用收集玩法 `AMiningCollectionEffects` 的五色彩带池，共 640 片。地上只有一根可收集彩带，复用 `AMiningCollectible` 的 ItemId 11、正常瞄准/E 拾取与图鉴库存。已有共享收集场时接入它；独立 P2 地图则创建不生成额外藏品的收集入口。
- 删除逃生水位、倒计时等上方信息面板，以及盲人段/卡牌段的下方说明字幕和房间介绍；保留卡牌选择、必要交互与方向距离提示。
- 验证入口：`Scripts/Run_CardP2Checks.ps1 -Mode Align|Native|Visual`，每次只运行一个 UE 工作负载并执行资源保护。Native 检查 P1/P2 完整牌局、沉没与复活、安全入电视和舞台；Visual 使用摆位夹具检查真实挖掘射线、材质、幕布、彩带特效和 E 拾取，不替代全程人工跑酷验收。报告、截图与资源记录在 `Saved/Validation/CardP2Flow/`。
- 本轮编译通过；独立存档的 P1、P2 逃生和 GiantRoomInput 三项回归均通过，仅有首次运行尚无自动存档的警告。P2 渲染交互检查已通过：正常挖掘切开屏幕，开挖后角色胶囊沿屏幕入口及上斜通道两段路径通过，无电视外壳或墙面阻挡；蓝色通道、安全入电视、幕布无碰撞、640 片彩带及 E 拾取入库均已验证。最终舞台曝光截图已复核，地板不再过曝，地面收藏彩带清晰可见。报告与截图位于 `Saved/Validation/CardP2Flow/`；这些是自动化及定点交互检查，尚未替代完整人工跑酷手感验收。

### 2026-10-18：试玩修正（取代上文 P2 美术电视接入）

- P2 恢复 P1 旧版悬挂电视、绿色发光可挖屏幕和连续蓝色通道，保留出口红幕布舞台。`Scripts/restore_p2_classic_tv.py` 先备份地图，再移除 P2 替换机壳/玻璃、恢复程序化预览；不要再运行 `align_p2_escape.py` 或旧美术电视安装脚本覆盖它。
- 电视屏幕和十四块通道实体带 `CardEscapeTelevision` 标签；`MiningComponent::MineTelevision` 使正常矿镐/钻机射线在这些实体上使用原球形切割，最小半径 220 cm。其他矿区继续使用现有镐击形状和成长平衡，避免通用镐击更新让电视出现浅而尖锐的卡脚孔洞。
- 水仍按原有速度上涨，但最多停在 Z=5850（电视平台顶面 Z=6000 下方 150 cm）。到达电视检查点后不再因房间倒计时失败；掉回水中仍会溺水。进入屏幕后继续保持安全。
- 漂浮箱底面延伸到水下 65 cm，书堆底部也与水接触；逐级升高的顶面、跟水运动、踩踏延迟下沉与复活恢复保持。
- 台灯光圈与入座范围统一为中心 `(12450,3600,3003)`、水平半径 800 cm，虚线圈同步扩大；固定落座视点不变。
- P2 绿色尺子曾与三块隐藏的旧顶层书本碰撞相交（`Corner book pages 6`、`Corner book cover 6`、`Supporting hardback 6`），现已关闭这些隐藏物件的碰撞。旁边刷子的粗凸包也侵入路线，改用本地图独立的 `ArtP2/Collision/SM_Brush_Exact` 精确网格碰撞；不修改其他地图共享的原刷子资源。修复后三条行走带、双向、简单/复杂查询共 12 次角色胶囊检测均无额外阻挡。
- 点击开始匹配后通过 `USubtitleSubsystem::PlayStatusLine` 使用通用字幕样式显示“正在匹配中……”，2.2 秒后显示“匹配成功！”并进入对手登场阶段。删除左上回合数字和进度条，保留双方 HP。
- 奖励模型改为玩家钻机，取消奖励法球发光覆盖和自动领取。领取前 V 与钻机升级项锁定；领取后解锁并装备，重试不会重复发奖或丢失钻机。能力所有权记录在 Wallet 和 SaveGame；独立无卡牌流程的旧地图保持原默认钻机可用行为。
- 实时四回合渲染检查通过：两条通用字幕已截图确认；胜利时仍锁定钻机，按 E 后才解锁并装备；旧电视在水位达到桌面后亮起绿色屏幕。对应 `pickup.json` 与 `08-matching.png` 至 `11-green-television.png`。
- 本轮 Development Editor 编译成功；`CardEscapeCheckpoints`、`P2CardEscapeCheckpoints`、`GiantRoomInput` 三项独立进程回归均通过且无测试警告。P2 渲染检查以实际钻机射线和角色胶囊扫掠穿过屏幕与十四段实体通道（146 个路径点、158 次尝试），确认安全区、水位上限、出口红幕、双侧彩带炮和 E 收集一根彩带。尺子路线 12 次胶囊查询均无额外阻挡。报告与截图见 `Saved/Validation/CardP2Flow/`；自动检查使用定点移动和测试用无限钻机能量，不代表人工完整跳跳乐手感验收。

### 2026-10-18：MainGame 接入与机器人大改版

- MainGame 的 Journey AlwaysLoaded 引用已从 P1 换为 P2，配置 `Config/MainGameRooms.json` 同步更新；主关卡的矿区和接口不重建。源 P2 不保存宿主的 100 米偏移。
- 地面拼装玩偶和对手改为用户提供的 `SourceAssets/CardRobot/character.fbx`。`ACardRoomRobot` 分别控制头、双臂、双脚、螺丝眼和眉毛，具备待机、空降、反击、受击、中毒、惊慌和打飞动作；机器人沿原轨迹到达墙洞后消失，不再提前启用旧兔子物理掉落。
- 电视出口幕布复用收集场景的平滑双侧开启逻辑；到达幕布前先开幕，继续走到舞台中央前方才触发彩带炮。第一波彩带从炮口发射，不在开启瞬间凭空铺满空中。彩带仍由 E 拾取一根。
- 后墙圆钟移动到左侧 Y=-1500，避开牌子上方的墙洞和打飞终点。
- 导入、安装、接入和验证脚本为 `import_card_robot.py`、`install_card_robot.py`、`link_main_game_card_p2.py`、`verify_card_robot.py`；验证产物在 `Saved/Validation/CardRobot/`。这些脚本按阶段使用，不批量执行旧 P2 安装器。
- 验证：Development Editor 编译通过；MainGame 定向原生回归确认 P2 引用、机器人对手和完整逃生逻辑，0 失败（附带已有网络探测超时及收藏目录预览材质缺失警告）。机器人实时四回合渲染检查通过；P2 实际钻机射线连续挖掘、双侧幕布位移、门口不喷彩带、走上舞台后喷发、E 收集彩带均通过。截图包含 `CardRobot/opponent-Arrival.png` 和 `CardP2Flow/05a-curtain-opening.png`。测试使用隔离定位及部分前置装备设置，不代表人工从主关卡出生点连续通关。

## 4. 代码导航与职责

以下路径相对工程根目录；头文件看接口，cpp 看行为。

| 文件 / 目录 | 职责 |
| --- | --- |
| [MiningEncounterPlayer.h](Source/BrainMining/MiningEncounterPlayer.h) / [.cpp](Source/BrainMining/MiningEncounterPlayer.cpp) | 项目接入层：共用 GameMode、继承主控制器的遭遇控制器、临时角色能力限制 |
| [CardRoomPlayer.h](Plugins/CardRoom/Source/CardRoom/Public/CardRoomPlayer.h) | `UCardRoomInteractionComponent` 的交互入口 |
| [CardRoomPlayer.cpp](Plugins/CardRoom/Source/CardRoom/Private/CardRoomPlayer.cpp) | 入座/离席、输入和 UI 生命周期、座位摄像机演出 |
| [CardRoomRun.h](Plugins/CardRoom/Source/CardRoom/Public/CardRoomRun.h)、`Private/CardRoomRun.cpp` | `FCardRoomRun`：无世界依赖的规则、阶段推进、选择与确认 |
| [CardRoomStage.h](Plugins/CardRoom/Source/CardRoom/Public/CardRoomStage.h)、`Private/CardRoomStage.cpp` | `ACardRoomStage`：房间、玩偶、会话、完成事件 |
| [SCardRoomView.h](Plugins/CardRoom/Source/CardRoom/Private/SCardRoomView.h) / [.cpp](Plugins/CardRoom/Source/CardRoom/Private/SCardRoomView.cpp) | Slate 卡牌、按钮、对白、选择交互 |
| [CardRoomVFX.cpp](Plugins/CardRoom/Source/CardRoom/Private/CardRoomVFX.cpp) | 桌面实体牌图案和攻击表现 |
| [CardRoomSymbols.h](Plugins/CardRoom/Source/CardRoom/Private/CardRoomSymbols.h) | 手牌/实体牌共用图案几何与配色 |
| `Content/Rooms/CardDuel/` | 地图、材质与占位音效 |

## 5. 运行流程与状态

```text
Exploring → E 入座 → Seated → 开始匹配 → Matching → Arrival
    ↑                                                  ↓
                       Choosing ⇄ Resolving（四轮） → Victory → Collected → 自动恢复探索
```

`FCardRoomRun` 是规则权威：`Hand` 保存当轮手牌，`Staged` 保存已选索引，`CanConfirm()` 要求处于 Choosing 且全部指定手牌选齐。重复确认、越界选牌、错误阶段输入不会推进战斗。UI 不直接改 HP。

`ACardRoomStage::Tick()` 推进 Run 并驱动玩偶、音效和效果；`bWinSent` 防止一局内重复发送胜利事件。领取经过 `Run.Collect()` 的阶段检查，成功后才发 `OnCollectibleClaimed`。内部场景重置仍清理演出标志，但正式卡牌交互不再暴露 Q 离席或按钮重玩。

界面是自绘 Slate `SLeafWidget`，不是 UMG/WBP。以 1600×900 虚拟画布缩放，`ToVirtual()`、`CardRect()` 和绘制共用坐标规则。`UCardRoomInteractionComponent` 创建/移除 Viewport UI，处理输入与座位状态；不承担规则计算。

## 6. 角色复用与表现实现

`AMiningEncounterGameMode` 直接生成 `/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter`，与 `AMiningPrototypeGameMode` 使用同一 Blueprint Class。角色的 `ABrainMiningCharacter`、`UMiningCharacterMovementComponent`、第一人称摄像机继续工作。

`AMiningEncounterPlayerController` 继承 `ABrainMiningPlayerController`，读取原 `BP_FirstPersonPlayerController` 的输入映射和 CameraManager 配置，调用基类 Enhanced Input 设置。插件不再轮询 WASD 或独立计算鼠标角度，也不再生成专用房间角色。

`UMiningEncounterPlayerPolicy::Apply(Pawn)` 保存原飞行设置并启用现有飞行，隐藏矿工头灯/补光灯。`Release()` 恢复原设置和灯光。原 MiningContext 暂停使用，由宿主在洞口/房间自由探索时转发左键到已有 MiningComponent，入座时禁止挖掘。普通挖掘和收集地图不使用这个 GameMode。

洞口防误传送修复（2026-10-10）：存在 `AEchoPassageStage` 时，Policy 在运行期暂存 `MiningComponent.FallResetDistance` 并设为 0；离开作用域时恢复原值。通道的 R/跌落恢复由 Echo 统一处理（局部 Z<-600），避免桌面高处开局后，走到低处洞口恢复挖掘就触发 Mining 的出生点复位。没有 Echo Stage 的地图保留原防坠落设置，不修改角色蓝图默认值。回归入口 `BrainMining.Encounters.PIE.HighSpawnBreachNoTeleport`：新建真实高处出生的原角色，R 回通道，验证洞口启用挖掘/挖墙不传送、远处 E 不入座、走到桌边后仍可入座，以及退出 Policy 恢复参数。 修复前该回归稳定失败，修复后与 `GiantRoomInput` 一起通过（2/2，0 错误、0 警告）；前后结果位于 `Saved/Automation/BreachFallReset/before.json`、`after.json`，编译日志 `Saved/Logs/Build-BreachFallResetFix.log`。

入座时组件保存位置、朝向、移动模式和碰撞状态，只增加自己的一层移动/视角锁，按实际主角色摄像机偏移对齐座位。离席、组件销毁时恢复。空格不会在座位上触发跳跃。自由探索时使用原角色转向手感，座位中保留固定镜头和受击晃动。

手牌与桌面实体牌共用 `CardRoomSymbols.h` 的火焰、毒滴骷髅和闪电图案。桌面图案网格必须保持朝上的法线与正确三角面绕序，否则看起来会重新变成白纸。现行攻击保留柔边火焰/烟团、激光和彗星光尾；用户已确认保留，不要按中途撤回的反馈恢复早期特效。

两种玩法的插件只负责玩法交互，不拥有角色移动系统。`UMiningEncounterPlayerPolicy::Apply/Release` 是 C++ 接入接口；如未来需要由蓝图区域触发，应先补蓝图包装或统一区域管理，不能假定它现在已是 BlueprintCallable。

## 7. 拼装接口与资源维护

依赖方向为 `BrainMining → CardRoom / EchoPassage → UE`。两个 Runtime 插件互不依赖，也不反向依赖项目模块。`CardRoomGameMode` 旧类仅用于旧地图加载兼容，当前地图请使用 `MiningEncounterGameMode`。

1. 当前推荐联结版：EchoStage、CardRoomStage 和 MiningGiantRoomEntrance 放在 passage 的同一 Level，保留同一玩家实例。原独立牌室仍是常规比例的参考地图。
2. 以后合并到主世界：保留主 GameMode，在主 PlayerController 上挂 `UCardRoomInteractionComponent`，调用 `BindInput(InputComponent)`，在进入/离开玩法时成对调用角色策略 `Apply/Release`。当前组件在 BeginPlay 查找一个 Stage，适用于一张地图一个遭遇；流送或多个房间时应改成显式 Stage 注册/激活，不能直接把本 GameMode 设成整个主世界的 GameMode。
3. UI 层通过 `Interact()`、`ConfirmCards()`、`LeaveSeat()` 进入组件，再驱动 Stage/Run。组件销毁会移除自己的 Viewport UI。
4. 监听 `ACardRoomStage::OnRoomCompleted(RoomId)`：每局胜利一次。监听 `OnCollectibleClaimed(RoomId)`：本局领取一次。真正物品 ID、数量、持久化及跨局去重由主游戏决定。
5. 联结版 `bRoomInSameLevel=true`、`DestinationLevel` 为空，入口 Actor 协调敲地/挖掘/牌室 UI 与局部曝光。旧 `FromEcho=1` 淡入逻辑仅为兼容保留；联结版不调用 OpenLevel，也不需要流送。

| 接口 | 层级 / 语义 |
| --- | --- |
| `Interact()` / `LeaveSeat()` / `ConfirmCards()` | 交互组件，BlueprintCallable；管理角色和 UI，再调用规则 |
| `BeginEncounter()` / `RestartEncounter()` / `CollectReward()` | Stage，BlueprintCallable；单独调用不等同于完整入座/离席 |
| `OnRoomCompleted(RoomId)` | 胜利通知，每局一次，跨局去重由主系统负责 |
| `OnCollectibleClaimed(RoomId)` | 钻机领取通知，由 MiningRoomEscape 解锁并装备；不携带收藏品 ID/数量 |

**和收集玩法的边界：** 牌局奖励是钻机能力，由 Wallet/SaveGame 保存所有权；它不是收藏品。电视出口舞台通过 `AMiningCollectionField` 收集一根彩带（ItemId 11）并增加其 `Inventory`。不能直接把 `RoomId` 当作收集 ItemId。

脚本入口：

- [integrate_encounter_player.py](Scripts/integrate_encounter_player.py) 只更新三张遭遇地图 GameMode；首次备份到 `Saved/EncounterSharedPlayerBackup/`，不重建场景 Actor。
- [create_card_room.py](Scripts/create_card_room.py) 用于首次搭建，目标地图存在会拒绝覆盖。素材更新使用 `create_card_room_vfx_materials.py` 或 `generate_card_room_audio.py`，先阅读脚本的覆盖行为。
- 日常使用已有 `Content/Rooms/CardDuel/` 资产，不需要重跑生成器。更新程序生成的资源时同步脚本与资产。

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
| 角色变了或移动手感不同 | 地图 GameMode、DefaultPawnClass、主控制器 Blueprint 输入配置；不要加回独立 WASD/鼠标轮询 |
| 桌面飞不上去或座位误挖 | 原飞行设置/输入、座位移动锁、入口 Actor 的 SetMiningAllowed；入座必须禁止挖掘 |
| E 无法入座 | 距离/高度门限、当前 Run.Phase、Stage 是否是本房间实例 |
| 空格不出牌 | 是否 Choosing、是否全部选齐；Slate 焦点和控制器绑定 |
| 座位太低或挡住牌桌 | 主角色摄像机实际偏移、座位目标视点、入座碰撞禁用和离席恢复；不要改主角色全局身高 |
| Q 后不能移动或转头 | 入座前移动模式、移动/视角锁的成对释放、GameOnly 和鼠标捕获恢复 |
| 桌面牌又变白 | 图案材质、网格绕序、法线、图案面位置，不只检查手牌 Slate |
| 改伤害/增回合 | 同步 Deal/Tick、UI 文案、玩偶/VFX 时间段和规则测试；目前固定四回合 |
| 重复发奖励 | Stage 本局去重，以及宿主对 RoomId/会话的跨局去重；不要每帧监听胜利状态就发奖 |

## 10. 程序协作与后续 AI 接手

先区分本次文件与已有挖掘/收集改动。`.umap/.uasset` 按项目 README 认领，避免同时编辑。修改共享角色、MiningComponent 或移动组件时说明受影响范围，不把其他系统的既有改动归入本任务。

接手时保持：复用原角色/移动/视角；本玩法启用既有飞行，同关卡挖墙进入巨人牌室；点击选中、空格甩向桌心；手牌和实体牌都有图案；保留现行攻击表现、玩偶呼吸表情与暖光；胜利后手动领取钻机能力，电视出口舞台可收集彩带。多房间与无缝流送是后续集成工作，当前组件只自动寻找一个 Stage。

交接至少包含地图、工作区/提交版本、复现按键、变更文件、已完成验证及日期、待人工确认项。后续验证与扩展范围以用户当次要求为准。

可复制给后续 AI：

> 先阅读 README.md、README_CardRoom.md 和 README_EchoPassage.md，检查已有改动。推荐正式联结地图是 Lvl_EchoMazeToCards_P2，P1 保留作白模回归，独立牌室参考是 Lvl_CardRoom_P1，主角色入口在 MiningEncounterPlayer，规则在 CardRoomRun，交互在 UCardRoomInteractionComponent。保留现有选牌、空格出牌、图案和攻击表现；只完成本次指定需求。不要改全局 GameMode、重建既有关卡或擅自接入收集奖励。需要测试时先遵从用户当前对验证范围的要求。
