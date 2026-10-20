# 结局场景地编

打开 `/Game/EndingEmergence/Maps/Lvl_EndingEmergence_Demo`。结局仍使用这张地图，主流程入口不变。

## 找到可编辑物体

所有道路、房屋和设施仍组成同一个完整街景，可以在场景中同时看到和调整整体布局。大纲搜索 `EDIT_街景总览相机`，右键使用驾驶（Pilot）查看整个街区，再退出驾驶进行编辑；细胞房有 `EDIT_细胞房总览相机`。两台总览相机仅用于编辑，不参与结局镜头。初始编辑视图隐藏了与街景共用坐标的脑区和远景图层，避免其他阶段遮住楼房；运行时由原流程恢复显隐。

在世界大纲搜索 `EDIT_`，或按下面的标签搜索。选中后按 F 聚焦，使用 W / E / R 移动、旋转、缩放；在详情面板替换 Static Mesh 和 Materials。完成后保存当前关卡。

| 大纲名称包含 | 内容 |
| --- | --- |
| `EDIT_街景_CityAssetBuildings` | 每栋独立楼房 |
| `EDIT_街景_CityGround` | 街区地面 |
| `EDIT_街景_Crosswalk` / `StopLine` / `LaneDash` | 四向斑马线、停止线、车道虚线 |
| `EDIT_街景_CityDressProp` | 候车亭、路树、路灯、信号灯、长椅、护栏、井盖和排水沟 |
| `EDIT_街景_Sidewalk` / `Curb` | 人行道和路缘 |
| `EDIT_细胞房_EllipsoidalCellInterior` | 细胞房曲面外壳，包含可行走碰撞 |
| `EDIT_细胞房_CellDress` | 细胞核、核仁、线粒体及嵴、高尔基体 |

它们都是普通 StaticMeshActor。可复制、删除，复制品保留原来的场景归属。新增装饰建议复制同一区域的 `EDIT_` Actor，再替换模型和材质。只从内容浏览器拖入全新 Actor，不会自动获得结局分段显隐；需要把它附加到同一区域的现有 `EDIT_` Actor 下。

Actor 的附件父节点仍属于 `涌现 · 那个下午 / Ending Emergence`。这些不可见节点保留原有细胞呼吸、街景显隐、镜头与退场时机。请保留附件关系；移动独立 Actor 即可，不必移动总控制器或其内部组件。转换后细胞房和街景在编辑视口中均可见，运行后自动恢复分段显示。

## 保存与保护

2026-10-20 马路段改为约 120 米见方的紧凑十字街区，20 栋 Japanese City 楼房（含四栋完整的模块高楼）；Quick City 的混凝土、钢构、道路和铺装用于地面与候车亭。新街景共有 149 个可编辑 Actor，按 `Road`、`Buildings`、`Props` 分文件夹。斑马线以同一路口坐标布置，宽 11.2 米，分别位于中心两轴的 ±8.5 米处，停止线位于后方。旧卡通建筑、棕榈、霓虹方块和道路灯带已从街景移除。细胞房、结局控制器、人群与电影镜头继续沿用原流程。

`Scripts/apply_ending_street_dressing.py` 是本次一次性地编脚本，修改前在 `Saved/Backups/EndingStreet-*.umap` 保存原地图；已应用的关卡拒绝重复覆盖。后续请直接编辑保存的 Actor。`Scripts/verify_ending_street_dressing.py` 在独占编辑器 PIE 中检查场景分段显隐并导出路口、街景和总览截图，报告位于 `Saved/Validation/EndingStreet`。

本次同时修复人群实例的街区定位：OverCrowd 创建首个 HISM 根组件后重新应用街区变换，避免人群落在世界原点。人数、动画和镜头轨迹保持原设置。UE 5.8 Editor 编译通过；地编保存重开、防重建验证与 `EditableScenery` 自动化通过；带画面的街景回归确认四处斑马线贴地、原人群对齐街区、街景随城市镜头显示并在地球段退场。本次未重新打 Windows 包。

正式镜头仍由 `EndingEmergence.cpp` 和 `EndingScaleMotion.cpp` 驱动，已核对与地编替换前提交 `3902aab3` 一致。先前 `02-intersection`、`03-overview`、`04-street-details` 截图使用临时巡检机位，不能作为正式镜头轨迹参考；现验证脚本默认只使用正式机位，显式传入 `-EndingStreetInspection` 才启用这些额外角度。`record_ending_street_review.py` 从最后一个可跳跃的神经元开始，用真实 Space 输入触发正式的大脑离场和街景上升，并记录两个转场前后的连续画面，输出位于 `Saved/Review/EndingStreet-20261020`，不保存运行中的任何机位调整到关卡。

- 位置、旋转、缩放、模型、材质及增删都保存在关卡里。运行和重启结局不会把 Actor 恢复到生成默认值。
- 控制器的 `Editable Scenery` 为已启用的只读状态。`Build Demo`、`Apply City Buildings`、`Apply Cell Visuals` 在此状态下直接跳过，不再重建。
- 旧 `create_ending_emergence.py`、`apply_ending_city.py`、`apply_ending_cell.py`、`apply_ending_organic_art.py`、`refresh_ending_rooms.py` 会在修改资源前拒绝处理这张地图；`ENDING_REBUILD=1` 也不会绕过保护。
- 再次调用 `ConvertSceneryToEditableActors` 或运行迁移脚本不会重建，也不会补回已删除的物体。
- 转换生成的细胞房网格和持久化材质实例位于 `/Game/EndingEmergence/Editable/Bake_*`。修改某一物体的颜色可从它的 Materials 槽定位实例；需要独立配色时先复制材质实例。
- 提交地编结果时提交 `.umap` 及本次修改/新增的 `.uasset`。首次迁移必须同时提交 `Editable` 资源和代码改动。

## 流程边界

此次转换覆盖街道、楼房、街边装饰、开场细胞房外壳和内部细胞器。人群动画、可挖出口膜、血管路线、神经元交互、地球/宇宙镜头和纸面/视频结尾继续由现有流程管理。它们没有被改成自由地编 Actor。

细胞房出口、出生位置、血管入口和电影镜头仍有既定坐标。装饰可以自由调整；大幅移动房间外壳或封住出口，需要同步调整流程设计并实机检查通行。房间外壳是持久化静态网格，可以替换或用 UE 建模工具编辑；它不再通过按钮生成新形状。

## 程序维护与验证

旧地图需要迁移时，在干净的编辑器进程运行 `Scripts/convert_ending_scenery.py`。脚本在首次转换前备份原地图到 `Saved/Backups/EndingEditable-*.umap`。转换保留原组件作为隐藏的动画与流程节点，关闭其渲染和碰撞；实际显示与碰撞由可编辑 Actor 承担。

关闭其他 UE 工作进程后可执行：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File Scripts/Build_Safe.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File Scripts/Run_EndingEditable.ps1 -Mode Convert
powershell -NoProfile -ExecutionPolicy Bypass -File Scripts/Run_EndingEditable.ps1 -Mode Verify
powershell -NoProfile -ExecutionPolicy Bypass -File Scripts/Run_EndingEditable.ps1 -Mode Runtime -Render
```

`Verify` 在独占编辑器进程中验证拖动、旋转、缩放、模型/材质替换、复制、删除、保存重开和防重建。启动前备份地图，进程结束后通过 `finally` 恢复原文件，并校验 SHA256；不要单独运行验证 Python 脚本。`Runtime` 检查可编辑 Actor 的显隐与偏移保持，并回归细胞/血管/脑区、尺度转场和钻掘至视频结尾。报告在 `Saved/Validation/EndingEditable`。

2026-10-19 验证：UE 5.8 Editor 编译成功；610 个可编辑 Actor 的保存重开、变换、换模型/材质、增删和防覆盖检查通过。`EditableScenery`、`OrganicArtRoute`、`ScaleMotion`、`TitleDrillReality` 四项带画面/音频回归均通过，报告归档为 `runtime-four-tests.json`。最终整景编辑视图另经保存重开检查，并复测 `EditableScenery` 和 `OrganicArtRoute` 通过（`Runtime/index.json`）；本次未验证打包版。原有通行测试改为检测实际挖掘开口，再瞄准视线内可挖的孔洞边缘，避免对着已打通的洞空挥；地球截图取样时间也改为跟随当前镜头长度。

### 街景夜间版本与行人连续性（2026-10-20）

`EndingStreetNightV1` 在现有 120m 街区内增加 8 个临街店面、原包发光窗户/招牌、暖色路灯、候车亭灯箱和绿化配电设施。新增 Actor 放在 `Ending_Edit/02_City/NightStreet`，继续直接附着 `02_CityAndPeople`，随原流程显隐。夜景材质位于 `/Game/EndingEmergence/StreetNight`，不改 JapaneseCity 原始资源。应用脚本 `apply_ending_street_night.py` 保存前备份地图与人群材质，已应用的地图拒绝再次执行。

行人沿原有路线连续移动，取消在正负 42m 边界的循环传送；有限长度的结局镜头不再回收行人。动态 VAT 批次改为 ISM，避免每帧移动后仍按旧 HISM 分组剔除。1201 人和 9 批次数量不变，实例数据 0/1 保留动画相位/速度，2–4 为上衣 RGB，5–7 为裤子 RGB。上衣约 85% 为黑白灰，约 15% 为彩色，裤子以黑灰为主。配色使用独立固定随机流，不改变原有位置、速度、相位；材质以烘焙色辨认服装区域，保留肤色和头发。镜头轨迹和大脑到马路的衔接代码未改。

`BrainMining.EndingEmergence.CrowdContinuity` 实际推进全部行人 40 秒，覆盖原循环边界，验证无位置跳变、城市原点、配色数量以及重播不增加人数。

`refine_ending_street_corners.py` 在四个 (±800cm, ±800cm) 街角各放置一组 3.2m 休憩区：小树与树池、发光导视牌、灯带与盲道砖，共 28 个可编辑 Actor（四个路口街角不摆长椅），存于 `CornerGardens` 文件夹。区域避开 ±600cm 道路边界和斑马线。四盏路口灯启用动态投影，全部 9 个人群批次参与动画阴影，其余店铺、灯箱、路灯光源继续关闭投影。正式镜头未修改。
