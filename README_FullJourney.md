# 全流程试玩关卡（连续版）

入口：`/Game/Campaign/Maps/Lvl_FullJourney`。双击 `Play_FullJourney.cmd` 或 `Play_Main.cmd`；编辑器默认也打开这张图。源码更新后编译 `BrainMiningEditor / Win64 / Development`。

当前成品采用固定预制布局：所有玩家每次开局的宝石位置、价值与碎晶坐标完全相同。布点工具只在编辑阶段运行并保存进地图；游戏启动仅加载这些坐标，挖掘时只移除失去岩层支撑的碎晶。下方历史记录中的“随机”指离线布点工具，不是每次开局重新随机。

宝石和碎晶的统一生成工具见 [Docs/MineralAuthoring.md](Docs/MineralAuthoring.md)。支持完整重建、保留主体与表面线索只重建内部碎晶、只读校验；提供 UE Python API 和独立进程入口。主矿道大改后先更新路线与岩层 Tag，再使用此入口，不要重新执行下方历史迁移脚本。

2026-10-13 内部线索修订：保留已有表面碎晶，扩大内部线索带厚度，避免一镐清空整条细线索。131 处矿点均按距主体 0–80、80–160、160–225 cm 的球壳体积校验密度，要求内层高于中层、中层高于外层。编辑迁移入口为 `Scripts/update_mining_interior.py`，保存报告为 `Saved/MiningInteriorBake.json`。复杂岩块的慢速切割改为后台计算，完成时统一提交岩层、碰撞和碎晶支撑变化；通用镐子在回声通道使用黑底白色线框，在卡牌房门口恢复原材质。

内部密度在重新加载的 PIE 地图中再次核对：近层/中层最低 1.55，中层/外层最低 1.40；第一矿点约为 35、22、16 枚/立方米。24 次普通镐击采样中，18 次命中实体的操作全部完成，其余为空挥；主线程镐击耗时最大 28.17 ms，后台提交延迟最大 169.68 ms（非整场帧率保证）。捕获的矿点 42 切割失败来自布尔交界未焊合；后台仅修复贴合原岩面/切割面的裂缝，闭合性、有效性及实际空腔回归通过。共用镐子材质生成入口 `Scripts/shared_pickaxe_materials.py` 必须在启用渲染的编辑器中运行，不能用 `-nullrhi` 替代着色器编译；内部碎晶离线迁移不再改动这两份材质。

## 流程与场景

挖掘 01 → 电脑弹窗 → 挖掘 02 → 盲人敲地、巨人卡牌与电视逃生 → 挖掘 03 → 文思博物馆 → 宇宙细胞结局。

三段矿区与连接通道保存在 Persistent Level；四间玩法房间直接引用源地图，作为 Always Loaded 子关卡一起加载。沿实体门洞与走廊步行衔接，无切图或加载传送。电脑、卡牌逃生与博物馆完成后打开出口门；正常跨区只切换玩法控制和提示，保留玩家位置与速度。F4 回访、F6 救援仍是主动传送快捷键。

电脑房在首次进入时自动开始办公计时；靠近电脑或打印机、瞄准后按 E 操作。返回房间不会重开已启动或已完成的轮次。房间除了门口接点，还使用区域边界接入玩法，避免从侧面挖入后没有激活。

三段矿区来自沈一郎的 `/Game/ExternalMining/Maps/Lvl_ExternalMining_Demo`。本地 `origin/Shen-Yilang` 的 c07db84 与当前 main 中该源地图的 Git 对象一致。连续版将副本按 0.5 比例缩小，裁去所选路线外的大块地形，开通两端门洞并加入藏品土层。源地图不被保存或覆盖。

| 矿段 | 原主路截取范围 | 缩小后主路本体 | 藏品与线索 |
| --- | --- | --- | --- |
| 挖掘 01 | 4–126 米 | 61 米 | 天堂马桶：漏水水管；蚂蚁的电脑：蚂蚁钻洞 |
| 挖掘 02 | 174.5–279 米 | 52.25 米 | 化学试管架：土层化学品污渍 |
| 挖掘 03 | 395–459.6 米 | 32.3 米 | 歪头肉鸽：羽毛与枝条 |

矿道本体合计约 145.55 米，是上一版的一半；新增的门口和连接走廊另计。角色和玩法房间保持原尺寸。每份矿体仍保留原 20 个阻塞层的引用，但裁切范围外的网格已清空。

## 挖掘与导航

矿区衔接层使用白色可挖岩体。短连接段裁出匹配的完整轮廓，生成带凹凸切面的通道内壁，再裁除原矿体及相邻新增体块间的重叠部分；较大的新增网格使用空间分块。外围裁切边界保持贴合，初始入口后方封闭为厚土层。关卡土体仍有有限外边界；房间完成门属于玩法门，不是可挖岩层。

藏品土层按原矿道的实际墙面定位，藏品中心从线索面向内约 7.8 米（比上一版加深 2.5 米）。挖通肉鸽所在土层后触发一次鸟群振翅飞出，保留可收集的歪头肉鸽；拼装版的飞行寻路支持旋转土层，洞外飞行段受碰撞约束。

四件藏品按 2＋1＋1 埋在矿道侧面的可挖土层内。先发现环境线索，沿线索挖开土层，藏品充分露出后瞄准按 E 收集。未挖开时不能隔着土领取。没有 CACHE 标牌或展示台。水管延伸进土里，蚂蚁沿路线爬入洞口，化学污渍分布在不同深度，羽毛会随支撑土层被挖走而掉落。

跨房间保留挖掘状态、库存、图鉴与升级账户。藏品不是过关门槛。退出游戏或停止 PIE 后本轮结束，尚无跨启动存档。矿区中的 131 颗矿物提供共享金币，已接入远端更新的四线养成和物理掉落。

- M：自由探索时打开地图；M / Esc 关闭。地图暂停游戏，显示当前朝向、三段矿道主路、高度图、出口及线索位置。房间显示流程位置，尚无内部平面图。先退出其他交互界面再打开。
- I：图鉴；Tab：升级面板；V：钻机。
- F4：回访已到达区域；F6：返回本段入口，保留本轮状态。
- F7：跳过当前电脑房、盲人／卡牌／电视房或博物馆，到下一段入口，保留库存和挖掘进度。矿区与最终宇宙细胞不跳过；先关闭地图再使用。
- R：沿用房间重试规则；矿区重置当前矿段岩层，不清空图鉴。

## 后续重新拼装

房间内部模型和玩法：直接修改并保存源地图，再重新打开全流程图／开始游戏即可取得最新内容，不需要重新拼装。主图中的 Levels 面板可查看四个源子关卡；编辑时把目标源关卡设为 Current Level，避免把新物件误放到主图。贯通门洞、自动启动及区域管理是运行时的主流程覆盖，独立源图保持原玩法；编辑器中可能仍显示源图的封闭墙，实际游戏会应用开门覆盖。

源地图分别为 `Lvl_PopupClose`、`Lvl_EchoMazeToCards_P1`、`Lvl_WenSi_MirrorMuseum`、`Lvl_CellUniverse_Demo`，完整路径见 `Config/CampaignLayout.json`。更换整个源地图、改变房间门洞位置、调整矿区地形或顺序时，再运行 `Scripts/create_full_journey.py`。配置在 `Config/CampaignLayout.json`；`continuous: true` 使用连续版生成器，`mine_scale` 控制矿区尺寸，截取范围与 2＋1＋1 分配也在配置里。连续版围绕世界坐标固定的回声/卡牌/电视场景自动推算其他房间位置，旧版 `offset` 字段仅用于 `continuous: false`。修改顺序或房间门洞结构时，需要同步调整 `Scripts/create_continuous_campaign.py` 的接点。

```powershell
$engineRoot = 'C:\UE_5.8'
$projectRoot = (Get-Location).Path
& "$engineRoot\Engine\Binaries\Win64\UnrealEditor.exe" "$projectRoot\BrainMining.uproject" /Engine/Maps/Entry "-ExecutePythonScript=$projectRoot\Scripts\create_full_journey.py" -CampaignRebuild -unattended -nosplash -nullrhi
```

运行前保存并关闭编辑器。`-CampaignRebuild` 会先把当前成品图、配方和上次清单备份到 `Saved/CampaignBackups/`；也可改用 `-CampaignOutput=/Game/Campaign/Maps/Lvl_FullJourney_V02` 生成新图。生成器只在成功完成后保存成品，不保存源地图。依赖完整编辑器的 Actor 复制功能，不能使用 Python commandlet。

重新生成会替换成品图中的手工修改，应把要保留的调整写回源图、配方或生成脚本。不要对成品中的 MiningExternalRegion 再执行原始全矿区生成操作，否则会覆盖裁切结果。若改变源矿道模型，需要同步更新中心线与阻塞层布局，并复查藏品埋藏和路线。

生成清单为 `Saved/FullJourneyManifest.json`，记录源图 SHA-256、接点与尺寸。成品管理 Actor 为 `CAMPAIGN_ContinuousFlow_EditAreasHere`。各 Area 的 MapRoute 为规划主路，不随玩家挖掘实时改变；仅更新路线可运行 `Scripts/update_campaign_navigation.py`，同样先备份。

## 本次迁移

`Scripts/link_campaign_rooms.py` 可对旧版成品图做一次房间引用迁移，同时更新低面数碎晶资源；它会先备份主图，只保存主图，不保存四张源地图。新版完整生成器只复制裁切矿区，房间直接引用，不再复制房间 Actor。

## 碎晶与新版博物馆修订

碎晶是岩层中的矿物包裹体：领取只隐藏宝石模型，碎晶组件从宝石模型脱离，保留世界坐标；仅当实际岩体不再覆盖其支撑点时移除。恢复岩层会恢复对应碎晶，但不会补发已领取宝石。

分布围绕宝石核心约 3.1 米，外疏内密，碎晶支撑点最小间距 32 厘米；表面约 3～5 枚提示，内部约 40 枚，并补充通向核心的稀疏线索。稀有度和价值档扩大随机埋深上限，实际落点必须通过完整埋藏检查；薄岩层会回退到较浅的可用位置。碎晶改为宽顶、倒角的 38 三角面模型，取消原来的拉长缩放；保持矿物色相，修正正面绕序，提高面色、降低粗糙度，并采用至少 0.9 的同色发光；深色矿物的碎晶提高明度，煤类仍保留暗色。

FullJourney 的博物馆改为直接引用新版 `Lvl_WenSi_MirrorMuseum`（镜面／词语交换玩法）。共享角色支持缩放、穿透和浮力，退出时恢复原胶囊与尺寸，回访仍保留交换得到的词语；房间的球、奖励门与天平按子关卡偏移运行。出口连接和结局整体后移 9.5 米，避免与新房间重叠。源地图保持独立可玩。

`Scripts/upgrade_campaign_crystals_museum.py` 用于迁移已有主图：备份主图与碎晶资源，替换房间引用、调整出口，并在原矿物落点基础上更新埋深与碎晶；不重建三段矿区，也不改经济数量和价值。记录在 `Saved/CampaignCrystalMuseumUpgrade.json`。

## 验证

### 2026-10-12 碎晶岩层归属与镜面博物馆

Editor Development 编译通过。`Saved/Automation/CrystalMuseumFinal/index.json` 四项全部通过：矿区接缝与步行不传送、六处真实挖取与领取、镜面博物馆集成、四处房间步行入口与真实 E 交互。镜面玩法另执行 35 项物理／词语／重置检查，0 失败，记录在 `Saved/Validation/WenSiMirror/runtime_checks.txt`。入口测试的一条警告来自 UE 联网探测超时，无玩法断言失败。

碎晶回归明确要求拾取前仍有碎晶，拾取后数量完全不变；其后挖除剩余支撑，数量才归零。检查了六处 Buried／Exposed／Collected 截图，修复了碎晶面的反向剔除。最终明度调整后单独复核六处矿脉，报告为 `Saved/Automation/CrystalGlowFinal/index.json`。本轮随机埋深约 1.2～4.4 米，矿物数量和经济总值保持不变。迁移前备份在 `Saved/CampaignBackups/crystals-museum-*`，四间源房间及原矿区地图均通过 SHA-256 不变检查。


### 2026-10-12 源房间引用与碎晶修订

合并远端 `4d9a12c`，保留四线养成、四档价值和物理掉落，并兼容全流程分区矿脉。旧的本地矿物版本已先提交为 `84daeae`，可追溯恢复。

四个玩法控制器实际位于对应源子关卡；主图启动前应用区域管理、自动办公、实体通道和入口覆盖。四张房间源图及矿区源图的 SHA-256 在迁移前后保持一致，记录于 `Saved/CampaignRoomReferences.json`。玩法模型修改不再要求复制房间 Actor。

Editor Development 编译通过；`Saved/Automation/RoomReferencesFinal/index.json` 的 13 项全部通过，覆盖完整电脑计时、实际卡牌电视逃生、十一段结局、房间入口交互、来源引用、矿区接缝、鸟群、F7、M 地图、九类价值规则及三段白岩／引导层的六处真实挖取。碎晶检查覆盖 8 面模型、支撑挖除、脱落宝石不带走碎晶，以及宝石领取后继续挖墙仍会清理碎晶。测试警告来自 UE 的联网探测与遥测请求失败，没有玩法断言错误。

连续多轮 PIE 批次曾出现显存预算提示；全新编辑器进程的 `Saved/Automation/RoomCrystalFresh/index.json` 再次通过矿物测试，最新六处截图中未再出现该提示。没有做完整资源 Cook、发布包或长期性能压测。下方为此前版本的验证记录。

### 2026-10-12 矿物与晶体线索

成品矿区新增 131 颗矿物，使用现有九种矿物模型。Mine01 只含 1／2／3，Mine02 只含 4／5／6，Mine03 只含 7／8／9；第一、二、三段的白岩／引导层数量分别为 34／20、21／10、30／16。全部矿物埋入现有可挖岩体，未新增挡路土块。暴露超过 50% 后瞄准按 E 领取，共享金币只入账一次；回访、F6、恢复岩体都不会补发矿物。

矿物只取用户表中四档整数价值。同种矿物的线性尺寸从基准的 0.70 倍随价值增长到 1.35 倍，最高档直径约为最低档的 1.93 倍。每处矿体有 25～35 枚同色碎晶，分散在约 3 米宽的岩面及通向矿物的内部岩层中。碎晶使用独立的 8 三角面片状模型，不阻挡玩家或挖掘射线。支撑点仍在岩体中时保留，挖掉支撑后移除；领取宝石之后也继续检查支撑。碎晶锚定世界坐标，不跟随脱落宝石翻滚。宝石露出后带局部光源，同时最多启用玩家附近四盏灯。

经济按每级费用分别支付计算，不计电脑房奖励或未来镐子翻倍收益。固定种子下的基础收入如下：

| 区域／阶段 | 数量 | 基础总价值 | 预期升级费用 | 类型相对权重 |
| --- | ---: | ---: | ---: | --- |
| Mine01 前期 | 24 | 100 | 50 | 1／2／3：70／28／2 |
| Mine01 后期 | 30 | 430 | 250 | 1／2／3：45／35／20 |
| Mine02 前期 | 8 | 3,850 | 2,500 | 4／5／6：75／24／1 |
| Mine02 后期 | 23 | 29,850 | 22,500 | 4／5／6：45／40／15 |
| Mine03 前期 | 8 | 175,000 | 135,000 | 7／8／9：95／5／0 |
| Mine03 后期 | 16 | 1,545,000 | 1,285,000 | 7／8／9：20／70／10 |
| Mine03 末端储备 | 22 | 8,995,000 | 7,000,000 | 7／8／9：5／15／80 |

最后一行按用户确认，暂代尚未存在的第 4 矿区，供钻机 3／4 级预算使用。常规价值档权重为 50／28／16／6，末端储备改为 20／25／30／25。预算不足以放入某档时会排除该档，因此表中是抽样相对权重，不承诺实际数量完全符合百分比；高价发现倾向每个阶段后部。

本次已合并远端的 16 项升级、共享账户和矿物物理掉落，实现说明见 `Docs/TreasureProgression.md`。矿物配置统一使用 `ValuesBySize`／`SizeMultipliers`，全流程原有预算和随价值增大的比例保持不变。

调参入口是 `Config/CampaignMinerals.json`：价值、区域、阶段范围、权重、预算、种子和间距都在这里。`Scripts/add_campaign_minerals.py` 可单独备份并更新成品地图的矿物布点，避免重建地形；完整拼装脚本也会自动调用它。具体落点与实际数量报告在 `Saved/CampaignMineralsManifest.json`。UE 编辑器模块关闭 Unity 编译，避免已有编辑器测试的同名辅助函数在合并编译时冲突。

编译通过；`Saved/Automation/CampaignMinerals/index.json` 中矿区接缝、矿物经济与六处真实挖取、旧宝物曝光检测和权重规则共 4 项通过。矿物测试检查三段区域隔离、四档价值、随价值增大的尺寸、全部初始埋藏、白岩／引导层均可采集、真实 E 入账、禁止重复领取、回访与重置不补发，以及取走宝石后碎晶仍可见。首次截图检查发现材质实例化支持缺失，随后启用并保存材质，并增加表面碎晶的露出部分。最终保存后 `Saved/Automation/CampaignMineralsFinalSurface/index.json` 的矿物经济与挖取测试再次通过，无测试错误或警告；外观已检查 `Saved/CampaignMineralPreview` 中 Buried／Exposed／Collected 三阶段实机截图。

2026-10-12 合并远端 main 的弹窗、炸弹和背包 UI 更新：保留进入电脑房自动启动与贯通出口，并为缩小后的电脑房补上两端各 2 米的门廊，与现有矿道接齐。UE 编辑器目标编译通过；`Saved/Automation/MainMerge20261012/index.json` 的 ContinuityAndClues、WalkEntryAndInteraction、FlowCollectionAndEnding、FracturedCavityAndCollision 四项全部通过。接缝测试增加门口地面连续性检查，流程测试运行实际电脑计时与新版喷涌完成条件。

2026-10-12 白色衔接岩层／加深藏品／鸟群／F7 修订：`Saved/Automation/CampaignWhiteRock/index.json` 中 8 项全部通过，包括完整计时、四件真实 E 收集、卡牌电视逃生、结局和地图。随后加强白色内壁凹凸、改为蚂蚁贴实际岩面爬行并整理快捷键提示；最终报告 `Saved/Automation/CampaignWhiteRockFinal/index.json` 中 BirdsDepthAndSkip、ContinuityAndClues、MiningEntrancePresentation、WalkEntryAndInteraction 四项再次通过，无错误。新增检查覆盖未挖通不飞鸟、挖通后仅触发一次鸟群、飞行位置变化、深埋藏品、F7 对三间小游戏生效而对矿区及结局无效，以及可见蚂蚁贴在真实岩面上。截图包括 `Saved/CampaignPreview/BirdFlight.png`、`Clue_*.png`、`Final_Mine*.png`。独立源地图 SHA-256 保持一致；最终外观调整前的地图在 `Saved/CampaignBackups/white-surface-20261012-150424/Previous.umap`。

2026-10-12 土层与房间触发修订：编译、地图重建及 `BrainMining.Campaign` 全部 7 项测试通过，报告为 `Saved/Automation/CampaignSoilAndRooms/index.json`。新增 `WalkEntryAndInteraction` 从各入口前方通过角色移动走过门口，检查四个房间激活、电脑真实 E 进入和退出、博物馆真实 E 生成思维卡，并在出生点向六个方向检测且实际挖掘土体。流程测试取消直接调用电脑启动函数，改为验证跨入电脑房后自动启动及原有完整计时。旧地图备份为 `Saved/CampaignBackups/continuous-20261012-140412/Previous.umap`。五张独立源地图 SHA-256 保持一致。下方记录属于此前版本。

2026-10-12 已在 UE 5.8.2 编译并生成连续版成品。六处接缝胶囊通行、跨区不移动玩家、三段原矿体实际挖掘、土层重置隔离与四件初始埋藏检查通过。`BrainMining.Campaign` 包括六处接缝的胶囊碰撞、跨区不移动玩家、藏品初始埋藏与线索、挖开后真实 E 收集、完整卡牌/电视逃生、结局、M 地图和入口落点检查。流程测试会移动玩家到检查点，并模拟部分完成信号；不等同真人从头步行挖完所有矿道。截图输出到 `Saved/CampaignPreview`。



本次验证记录：

| 检查 | 结果与报告 |
| --- | --- |
| 四件藏品挖开后 E 收集、图鉴、房间完成信号与十一段结局 | 通过；`Saved/Automation/CampaignContinuous/index.json` 的 FlowCollectionAndEnding |
| 完整卡牌与电视逃生 | 通过；同报告的 FullCardAndTelevisionEscape |
| M / Esc 地图、暂停时钟、操作恢复与出口一致 | 通过；同报告的 NavigationMap |
| 三份矿区来源、0.5 比例与阻塞层引用 | 通过；同报告的 SourceGeometry |
| 六处实体接缝、原矿体真实挖掘、藏品埋藏、跨区不移动、重置只作用当前矿段 | 通过；`Saved/Automation/CampaignContinuousFinal/index.json` 的 ContinuityAndClues |
| 三个入口碰撞与四处线索实机截图 | 通过；最终报告的 MiningEntrancePresentation |
| 原六区大厅同角色与经济系统 | 通过；最终报告的 PersistentWorldAndEconomy |

第一次报告保留了修正前的接缝失败记录，以最终报告的 ContinuityAndClues 为准。最终报告无失败；画面检查记录了两条 UE 网络连通性探测超时，不影响上述游戏断言。五张独立源地图的 SHA-256 在重建前后均一致。截图为 `Saved/CampaignPreview/Clue_0.png` 至 `Clue_3.png`、`Final_Mine*.png` 与 `Map_Area*.png`。


## 2026-10-13：碎晶性能与洞穴观感

- 碎晶/宝石覆盖检查按岩体的 DynamicMesh 共享空间树。网格替换、挖掘、重置、异步简化提交都会使缓存失效；只刷新与矿点范围相交的变化岩块。暴露度不再重复采样，支撑状态未变时不重建碎晶实例。
- 煤炭线索采用暖琥珀色，锡矿线索采用冷银蓝色，均保留轻微自发光。宝石领取不会删除尚有岩层支撑的碎晶。
- 共用角色的头灯为暖色柔和光锥，使用低对比度静态透镜环纹，无额外动态光源与投影阴影；盲人回声玩法仍按房间设计关闭头灯。
- 回声/卡牌房间保留共用角色的飞行时长、热量、冷却和升级；不再强制无限飞行。房间敲击通过委托驱动通用镐子，不生成第二把方块镐子。
- FullJourney 的主岩层与引导岩层均改为深色暖矿层。12 条连接通道的四面岩体采用相同连续形变，形成起伏拱顶和轻微曲折，并固定两端接缝；碰撞与可挖几何同步。
- 原位资源迁移：`Scripts/upgrade_campaign_cave_presentation.py`。每次先备份地图/既有材质，通道用 `BM_SoftPassageV1` 标记避免重复形变，源房间地图用 SHA-256 校验。
- 可重复性能采样：编辑器打开 FullJourney 后运行 `Scripts/profile_campaign_mining.py` 并启动普通 PIE。脚本仅挖掘 PIE 副本，输出 Saved 下 MiningProfileBefore/After.json。

验证结果：`Saved/Automation/CaveRevisionFinal/index.json` 中五项测试全部 Success（关卡连续通行、三矿区六组挖掘/拾取、共用角色隔离、飞行热量、覆盖缓存失效）。同一第一矿区矿点、12 次半径 100cm 切割的编辑器 PIE 采样：P95 帧间隔 266ms → 27.15ms，峰值 297ms → 60.12ms，超过 50ms 的采样帧 14 → 2。统计对象为 Slate 帧间隔，非整场游戏帧率保证；报告见 `Saved/MiningPerformanceComparison.json`。外观截图在 `Saved/CavePresentationPreview` 和 `Saved/CampaignMineralPreview`。


## 2026-10-13：集中头灯、自然洞口与 Blender 空间工作文件

- 头灯内/外锥角改为 12° / 30°，收紧透镜图案边缘，保留轻微环纹。
- 碎晶厚度增加，切面顶点明暗范围扩大，使用独立的碎晶材质分支、较低粗糙度和略强发光。近核以 160 cm 为范围，数量约为修改前两倍；外围仍为稀疏样本和通往表面的线索。
- 131 处宝石全部增加埋深。洞口形变后会重新校准线索方向；少数被形变压浅的矿点重新寻找真实实心岩层，保持稀疏的表面线索。
- 入口外侧、主矿道内侧和出生点封口均有局部岩面形变；被原先布尔裁切留下的薄片会重新烘焙成封闭实体。地形仍是独立可挖 Actor，并未反写原始通道 Blender 切割体。
- 碎晶覆盖刷新改为逐帧轮转、约 4 ms 软预算，避免近核加密后邻近矿点集中在同一帧更新。藏品也使用支持分块岩层的共享覆盖查询。
- 大范围切割后的碎晶验收以实际残存岩层为准：已移除支撑的碎晶必须消失，仍有支撑的碎晶必须保留。布尔失败不会通过直接删除碎晶来伪装挖掘成功。
- Blender 工作文件与流程说明：`Art/Mining/FullJourneyExchange/FullJourney_Editable.blend`、同目录 `README.md`。104 个有几何的可挖地形对象独立保留；房间静态参照按源 Actor 分组。坐标、身份映射及选中岩块的回导由配套脚本处理。

性能复核：近核加密、地形细化后，未分帧版本 P95 为 50.69 ms；轮转预算版本降为 22.20 ms，峰值 64.92 ms。碎晶覆盖刷新峰值 56.33 → 10.45 ms。采样为同一矿点 12 次半径 100 cm 切割的编辑器 PIE 帧间隔，不包含截图和模型导出。报告：`Saved/JunctionsPerformanceComparison.json`。Blender 带位移 OBJ 回导的包围盒误差为 0.00026 cm，并通过实际开挖验证。
最终验收：`Saved/Automation/JunctionsAccepted/index.json` 的 ContinuityAndClues、MineralEconomyAndExcavation、CoverageCacheInvalidation 均为 Success。覆盖 131 处初始埋藏检查、三矿区六组真实开挖/按 E 拾取、领取后保留尚有岩层支撑的碎晶及继续挖掘移除支撑。超大清理球在薄壁处可能被布尔有效性保护拒绝；测试另用普通镐击尺寸直接验证残存碎晶，拒绝的切割保留岩石和碎晶。
