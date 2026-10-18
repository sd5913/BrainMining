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

正式关卡双击 `Play_MainGame.cmd`；旧预览入口为 `Play_MainGame_CollectionsPreview.cmd`。按 E 拾取、I 打开图鉴、M 查看路线。Ctrl+F10 打开开发者区域跳转；F7 可跳过办公室、盲人通路等玩法房间，不能跳过矿道本身。

上面的生成、合入命令只用于尚未包含收集物的正式地图。正式合入后不要再次烘焙同一份地形；生成脚本会在改动预览前检查并拒绝重复布置。日常复测只运行 `verify_main_game_collections.py -Render`，它在存在成功合入报告时直接验证正式 MainGame。

报告和截图保存在 `Saved/Validation/MainGameCollections`。`authoring.json` 记录解析后的位置、土层绑定、改动的预览地形和源地图 SHA-256；`runtime.json` 记录真实 PIE 挖掘与 E 键拾取结果。

2026-10-18 本轮验证：5 项原生检查、收集测试房 122 项运行检查通过；MainGame PIE 的 83 项检查通过，涵盖 12 类藏品的真实 E 键拾取、统一库存、土层遮挡、数字随挖掘消失、蚂蚁持续移动、白描边材质共享和旋转后的天堂支路漏水。电视舞台检查使用已完成电视玩法的测试状态，复用原舞台彩带，不代表重新通关了整个电视关卡。

天堂房间使用仅覆盖支路室内的曝光体积；复制水管的缺失材质使用已有 `M_HeavenPipe` 补齐。`Scripts/main_game_collection_dressing.py` 已加入生成流程；`refine_main_game_collections.py` 可对本次预览重做这两项修补，无需重新切割地形。最终画面复查运行 `Run_MainGameCollectionScript.ps1 -Script capture_main_game_collections.py -Render`。

本轮逐件截图由运行检查生成：`03_item_04_before.png`、`dug_04.png` 可查看数字线索挖掘前后，`08_item_10_before.png` 为荧光线索。`capture_main_game_collections.py` 专门复查椅面磁带正面并验证拾取；本轮已通过，生成 `review_cassette.png` 和 `visual.json`。

测试使用独立 UserDir 并禁用游戏存档。区域跳转辅助函数只存在于 Editor 模块且只允许 PIE 世界，不改变正式关卡解锁规则。

## 与地形修改合并

当前正式地图已包含收集物。后续改地形时应保留收集字段、其 `AuthoredPlacements` 和 `BM_MainGame_Collections` 所属支路；若替换了主岩石 Actor，需要重新核对藏品的 `HostRock` 绑定与线索表面，再跑正式地图验证。不要直接重复烘焙同一份地形。

以下是首次合入流程，仅适用于尚未包含收集物的地形基底：

1. 先合入同事最新的地形和源房间修改。
2. 保留本任务的 C++、网格与材质资产、配置和脚本，再运行生成脚本。它按现有路线比例和实际碰撞表面重新定位藏品，并重新绑定土层，不依赖旧生成岩石的名字。
3. 检查新预览中的物体、线索、支路入口和原路线通行情况。
4. 检查通过后，将这一份基于最新地形生成的预览另存为正式 MainGame。不要用早先的预览覆盖同事之后修改的地图。

椅子、盲人通路的小台子和房间名称发生变化时，应先更新配置。找不到可靠锚点时，脚本报错中止，不会悄悄把物体放在旧坐标。

天堂房间沿用现有几何和浅水逻辑；其支路会在预览地形中开口。正式合入时必须以最新地形重新生成，不能只复制藏品坐标而遗漏通路开口。
