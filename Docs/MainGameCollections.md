# MainGame 收集物布置

布置配置：`Config/MainGameCollections.json`。生成脚本：`Scripts/apply_main_game_collections.py`。

正式地图为 `Content/MainGame/Maps/Lvl_MainGame.umap`，启动入口为 `Play_MainGame.cmd`。收集物使用正式地图内的一份统一配置与库存，电视舞台继续提供第 12 类彩带。办公房、盲人通路和天堂马桶的源地图保持独立。

2026-10-18 已基于同事最新地形完成正式合入，并直接在 `/Game/MainGame/Maps/Lvl_MainGame` 通过 83 项带渲染 PIE 检查，含全部 12 类 E 键拾取。此次验证中的五份地图文件均未被运行测试修改。磁带验证从椅子正面进行，与最终直立朝向一致。

原预览 `/Game/CollectionIntegration/Maps/Lvl_MainGame_CollectionsPreview` 保留用于对照。`promote_main_game_collections.py` 会核对生成时的五份源地图哈希、备份正式地图，再通过 UE Save As 保存到正式 MainGame；`promotion.json` 记录备份与保存前后哈希。源地图有更新时拒绝覆盖。

| 区域 | 藏品 | 布置与线索 |
| --- | --- | --- |
| 第一段通路 | 化学试剂、蚂蚁电脑、计算器 | 局部彩色污迹；沿挖开土层爬行的蚂蚁；单个数字 1 即一个碎晶实例，尺寸约为原碎晶两倍，225 cm 核心范围按宝石密度散布，挖除支撑土层当帧消失 |
| 办公房入口右侧 | 磁带 | 蓝天白云墙前的 `SM_KitchenChair_01a`；向下检测椅面，直立在坐面上，正面朝椅子前方；缩放 0.25（前版一半） |
| 紫色通路 | 肉鸽、根系花朵 | 羽毛与枝条引向肉鸽；微光花头露在土外，根系埋在土里 |
| 盲人通路两个死路 | 音符、合照相框 | 原有小台子；共享通路的回声材质参数，白描边随敲击显现并消退 |
| 电视后的舞台 | 彩带 | 复用 `MiningRoomEscape` 已有舞台奖励，不新增第二份彩带或收集系统 |
| 红色通路 | 天堂马桶房、喷涌可乐、萤火虫罐 | 支路水管与可挖土层；紧贴罐身的 64×64×106 cm 地下空腔和贯通向上的气泡孔；沿弯曲、不等距轨迹漂浮的绿色闪烁粒子 |

## 预览与验证

关闭其他 UE / 编译进程后运行：

```powershell
& Scripts/Build_Safe.ps1
& Scripts/Run_CollectionExpansionChecks.ps1 -Mode Upgrade
& Scripts/Run_MainGameCollectionScript.ps1 -Script apply_main_game_collections.py
& Scripts/Run_MainGameCollectionScript.ps1 -Script promote_main_game_collections.py
& Scripts/Run_MainGameCollectionScript.ps1 -Script verify_main_game_collections.py -Render
```

正式关卡双击 `Play_MainGame.cmd`；旧预览入口为 `Play_MainGame_CollectionsPreview.cmd`。按 E 拾取、I 打开图鉴、M 查看路线。Ctrl+F10 打开开发者区域跳转；GM 窗口中的“跳过当前玩法房间”可跳过办公室、盲人通路等房间，不能跳过矿道本身。

上面的生成、合入命令只用于尚未包含收集物的正式地图。正式合入后不要再次烘焙同一份地形；生成脚本会在改动预览前检查并拒绝重复布置。反馈修正后的日常复测使用 `Scripts/Run_CollectionFeedbackChecks.ps1 -Mode Render`，读取更新后的位置报告并验证正式 MainGame。

报告和截图保存在 `Saved/Validation/MainGameCollections`。`authoring.json` 记录解析后的位置、土层绑定、改动的预览地形和源地图 SHA-256；`runtime.json` 记录真实 PIE 挖掘与 E 键拾取结果。

2026-10-18 正式合入首轮验证：5 项原生检查、收集测试房 122 项运行检查通过；MainGame PIE 的 83 项检查通过，涵盖 12 类藏品的真实 E 键拾取、统一库存、土层遮挡、数字随挖掘消失、蚂蚁持续移动、白描边材质共享和旋转后的天堂支路漏水。电视舞台检查使用已完成电视玩法的测试状态，复用原舞台彩带，不代表重新通关了整个电视关卡。

天堂房间使用仅覆盖支路室内的曝光体积；复制水管的缺失材质使用已有 `M_HeavenPipe` 补齐。`Scripts/main_game_collection_dressing.py` 已加入生成流程；`refine_main_game_collections.py` 可对本次预览重做这两项修补，无需重新切割地形。最终画面复查运行 `Run_MainGameCollectionScript.ps1 -Script capture_main_game_collections.py -Render`。

本轮逐件截图由运行检查生成：`03_item_04_before.png`、`dug_04.png` 可查看数字线索挖掘前后，`08_item_10_before.png` 为荧光线索。`capture_main_game_collections.py` 专门复查椅面磁带正面并验证拾取；本轮已通过，生成 `review_cassette.png` 和 `visual.json`。

测试使用独立 UserDir 并禁用游戏存档。区域跳转辅助函数只存在于 Editor 模块且只允许 PIE 世界，不改变正式关卡解锁规则。

## 与地形修改合并

### 2026-10-18 反馈修正

已拉取 `bd54d62`，保留本地卡牌房 P2 修改，并将远端字幕触发子关卡接回正式 MainGame。地图包含以下调整：

- 蚂蚁电脑移至第一矿区 Branch_02，计算器移至 Branch_04，萤火虫罐移至第三矿区 Branch_04，利用已有泥土引导增加死路奖励。相应的线索和 GM 落脚点一同迁移。
- 蚂蚁电脑埋深约 480 cm，计算器约 530 cm，萤火虫约 480 cm，肉鸽约 440 cm；可乐位于地表下 460 cm，保留贴身空腔及连续气泡孔。计算器、萤火虫沿原死路换到另一面矿壁，与最近宝石核心分别相隔约 1396 cm、1141 cm，线索也避开已有宝石线索。
- 化学污渍使用与测试房相同的斑块、滴痕算法，通过有范围限制的贴花投射到原始及挖开后的矿壁。动态矿壁不支持普通 Mesh Overlay 渲染通道，因此不使用该通道。数字 1 从表面延伸到计算器核心，按土层支撑消失。
- 花朵根系横向扩大 1.65 倍、纵向扩大 1.55 倍；挖开根冠即可拾取。蚂蚁电脑拾取后蚂蚁在 2.5 秒内渐隐。正式分块地形使用面向玩家的肉鸽飞出路径。
- 音符与相框保留原回声材质和扩散/消退，准星选中时叠加通用白色描边，移开立即取消。10 月 18 日新反馈要求消除黄边：卡牌源房间的有界后处理不再扩展到整个 Journey 区域；下层回声迷宫增加局部中性黑白分级并关闭色差。紫色上层入口与卡牌房不受黑白区域影响。
- 天堂房采用连续球面方向的天空材质、红色支路岩壁和短水龙头；盲人入口改为紫色岩壁并补齐顶部、前方遮挡及诱导泥土。电视舞台出口增加连续照明。
- Esc 打开暂停菜单：收集界面、退出游戏；退出需再次确认。图鉴底栏使用拖动/滚轮横向滚动，区分已收集与未发现，并预留舞台彩带条目。Ctrl+F10 的「GM · 收集物附近」可逐件跳转。
- 博物馆气泡取消叉号；气球靠近天花板，系绳仍固定在台子；第二罩子为实心金色并显示「注意！内有珍品」。镜子采用固定镜面的偏轴投影，跟随玩家位置和缩放；画中圆环使用通用 E 提示。细胞油画参数加强，仅作用于开场细胞阶段，血管不再使用；结局不再启动 Cosmic BGM，场景环境音保留。

10 月 18 日追加修复：蚂蚁线索取消黑色占位物和预挖孔；计算器正面朝向线索入口，挖出后保持静态可拾取，避免碰撞重建时被物理冲量抛走；天堂水龙头与原管路接头对齐。卡牌左侧首个尺子坡道恢复 Actor 与组件碰撞，钻机奖励放大到 2.5 倍并复用通用白描边与 E 交互，胜利文本走通用字幕。最后一个电视前漂浮平台不下沉，电视内恢复旧版青蓝电路材质，钻机切割同时处理接缝两侧并为脚底留通行空间。舞台使用覆盖整个飞行周期的连续彩带发射，唯一可收集彩带从炮口飞出、落地后开放拾取。矿镐沿柄转向使长镐尖朝前，升级动作延长至 1.15 秒并加大抬升和转动；喷气背包松开推进后在空中立即按地面 35% 的速度冷却。

本轮脚本：`apply_gameplay_followup.py`（备份后定点修改，不重新生成地形或宝石），`align_pickaxe_striking_head.py`（带标记的幂等朝向修复），`Run_GameplayFollowupChecks.ps1`（Native / Collections / Visual），`verify_gameplay_followup_tv.py`（真实挖掘与胶囊碰撞遍历 70 米电视通道）。本轮报告与截图在 `Saved/Validation/GameplayFollowup`，修改前资产在 `Saved/Backups/GameplayFollowup20261018`。

本轮验证：18 项原生自动化、89 项正式 MainGame 收集流程、10 项画面/交互检查、34 项电视通道流程检查通过。正式关卡的 12 类收集品均通过 E 拾取进入同一图鉴；电视通道以真实钻机切割 15 段填充，并用玩家胶囊扫掠通过 146 个路径点，再完成彩带落地与拾取。此项验证覆盖挖掘碰撞与检查点流程，不等同于完整人工跳台通关。原生测试中两项有临时测试世界销毁警告，无失败。编译通过，测试使用独立 UserDir，未写入玩家存档。`capture_gameplay_followup_screen.py` 可单独复核电视亮屏颜色。

验证入口：`Run_CollectionFeedbackChecks.ps1 -Mode Native`（原生、菜单和 GM）、`-Mode Render`（挖掘与真实 E 拾取）、`-Mode Rooms`（菜单和房间画面）、`-Mode Portal`（通用 E 提示及真实进入细胞房）。报告位于 `Saved/Validation/CollectionFeedback`。地图修补脚本会先备份；`apply_collection_feedback.py` 拒绝对已修补地图重复切割。`finish_collection_feedback_lighting.py` 为下坠前的紫色入口增加有阴影、半径 700 cm 的局部照明，下方回声迷宫不在照明范围内。

反馈修正最终复测通过：9 项原生自动化（含全部 12 个 GM 跳转、跨地图菜单恢复）、88 项正式 MainGame 运行检查、14 项房间检查、4 项画中入口/细胞房检查。瞄准和移开音符的独立画面对照也通过。化学污渍最终画面见 `01_item_02_before.png`；入口提示见 `museum_common_e_prompt.png`；细胞房见 `ending_stronger_cell_paint.png`。构建成功，验证未写入玩家存档。

当前正式地图已包含收集物。后续改地形时应保留收集字段、其 `AuthoredPlacements` 和 `BM_MainGame_Collections` 所属支路；若替换了主岩石 Actor，需要重新核对藏品的 `HostRock` 绑定与线索表面，再跑正式地图验证。不要直接重复烘焙同一份地形。

以下是首次合入流程，仅适用于尚未包含收集物的地形基底：

1. 先合入同事最新的地形和源房间修改。
2. 保留本任务的 C++、网格与材质资产、配置和脚本，再运行生成脚本。它按现有路线比例和实际碰撞表面重新定位藏品，并重新绑定土层，不依赖旧生成岩石的名字。
3. 检查新预览中的物体、线索、支路入口和原路线通行情况。
4. 检查通过后，将这一份基于最新地形生成的预览另存为正式 MainGame。不要用早先的预览覆盖同事之后修改的地图。

椅子、盲人通路的小台子和房间名称发生变化时，应先更新配置。找不到可靠锚点时，脚本报错中止，不会悄悄把物体放在旧坐标。

天堂房间沿用现有几何和浅水逻辑；其支路会在预览地形中开口。正式合入时必须以最新地形重新生成，不能只复制藏品坐标而遗漏通路开口。
