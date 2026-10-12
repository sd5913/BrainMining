# 全流程试玩关卡（连续版）

入口：`/Game/Campaign/Maps/Lvl_FullJourney`。双击 `Play_FullJourney.cmd` 或 `Play_Main.cmd`；编辑器默认也打开这张图。源码更新后编译 `BrainMiningEditor / Win64 / Development`。

## 流程与场景

挖掘 01 → 电脑弹窗 → 挖掘 02 → 盲人敲地、巨人卡牌与电视逃生 → 挖掘 03 → 文思博物馆 → 宇宙细胞结局。

所有场景拼在同一张 Persistent Level，沿实体门洞与走廊步行衔接。电脑、卡牌逃生与博物馆完成后打开出口门；正常跨区只切换玩法控制和提示，保留玩家位置与速度。F4 回访、F6 救援仍是主动传送快捷键。

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

跨房间保留挖掘状态、库存、图鉴与升级账户。藏品不是过关门槛。退出游戏或停止 PIE 后本轮结束，尚无跨启动存档。原金币账户保留，但测试路线没有金币庭院，因此从零开始不会自然获得升级金币。

- M：自由探索时打开地图；M / Esc 关闭。地图暂停游戏，显示当前朝向、三段矿道主路、高度图、出口及线索位置。房间显示流程位置，尚无内部平面图。先退出其他交互界面再打开。
- I：图鉴；Tab：升级面板；V：钻机。
- F4：回访已到达区域；F6：返回本段入口，保留本轮状态。
- F7：跳过当前电脑房、盲人／卡牌／电视房或博物馆，到下一段入口，保留库存和挖掘进度。矿区与最终宇宙细胞不跳过；先关闭地图再使用。
- R：沿用房间重试规则；矿区重置当前矿段岩层，不清空图鉴。

## 后续重新拼装

各成员修改并保存独立源地图后，可以再次运行 `Scripts/create_full_journey.py`。配置在 `Config/CampaignLayout.json`；`continuous: true` 使用连续版生成器，`mine_scale` 控制矿区尺寸，截取范围与 2＋1＋1 分配也在配置里。连续版围绕世界坐标固定的回声/卡牌/电视场景自动推算其他房间位置，旧版 `offset` 字段仅用于 `continuous: false`。修改顺序或房间门洞结构时，需要同步调整 `Scripts/create_continuous_campaign.py` 的接点。

```powershell
$engineRoot = 'C:\UE_5.8'
$projectRoot = (Get-Location).Path
& "$engineRoot\Engine\Binaries\Win64\UnrealEditor.exe" "$projectRoot\BrainMining.uproject" /Engine/Maps/Entry "-ExecutePythonScript=$projectRoot\Scripts\create_full_journey.py" -CampaignRebuild -unattended -nosplash -nullrhi
```

运行前保存并关闭编辑器。`-CampaignRebuild` 会先把当前成品图、配方和上次清单备份到 `Saved/CampaignBackups/`；也可改用 `-CampaignOutput=/Game/Campaign/Maps/Lvl_FullJourney_V02` 生成新图。生成器只在成功完成后保存成品，不保存源地图。依赖完整编辑器的 Actor 复制功能，不能使用 Python commandlet。

重新生成会替换成品图中的手工修改，应把要保留的调整写回源图、配方或生成脚本。不要对成品中的 MiningExternalRegion 再执行原始全矿区生成操作，否则会覆盖裁切结果。若改变源矿道模型，需要同步更新中心线与阻塞层布局，并复查藏品埋藏和路线。

生成清单为 `Saved/FullJourneyManifest.json`，记录源图 SHA-256、接点与尺寸。成品管理 Actor 为 `CAMPAIGN_ContinuousFlow_EditAreasHere`。各 Area 的 MapRoute 为规划主路，不随玩家挖掘实时改变；仅更新路线可运行 `Scripts/update_campaign_navigation.py`，同样先备份。

## 验证

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
