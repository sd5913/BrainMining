# MainGame 老鼠博物馆与画中细胞入口

两套机关仍控制老鼠洞入口。旧洞口从 64 cm 提高到 80 cm，以适配当前 0.38 倍、约 67 cm 高的玩家胶囊；普通大小仍不能通过。玩家变小进入后，后墙正对入口展示鼠鼠版《吻》，两侧放置微型金鼠展台。原金蛋及其底座在运行时隐藏、关闭碰撞；走到原金蛋位置不再完成或切关。

母鼠裙装中一个低调的红色圆环是细胞入口，大小接近其他纹样、仅略大。贴图 UV 中心 `(0.6374, 0.5656)`，画布 126 × 168 cm，图案半径约 2.9 cm，准星靠近半径 4.3 cm。平时保留原画，靠近时出现细白描边；描边与交互共用此坐标。只有已解锁、缩小、在洞内、视线未被遮挡且准星靠近圆环时，才显示「E 进入画中的圆环」。主流程接入博物馆控制器的 E 键路由，独立地图使用按键轮询。按 E 后镜头用 2.25 秒加速吸入，禁止重复触发，切换至 `/Game/EndingEmergence/Maps/Lvl_EndingEmergence_Demo`。新地图正常生成玩家，恢复常规大小和输入。

主线仅在 `BM_Campaign_MainGameMuseum` 区域切关。独立谜题测试地图使用同样的画中圆环交互，动画结束后标记完成并恢复操作，仍可重置。重置、退出区域与销毁均清理镜头与输入锁。

画作源文件在 `SourceAssets/MouseMuseum/T_MouseKiss.png`，内置 image_gen 的完整提示词记录在同目录 `Generation.txt`。导入脚本为 `Scripts/import_mouse_museum.py`，目标资源目录 `/Game/MouseMuseum`。

Art/CelPaintShader 中的原材质以原包路径 `/Game/CelPaint` 导入，专用实例为 `/Game/MouseMuseum/MI_EndingOilPaint`。细胞与血管分别使用 0.85、0.70 后处理权重，可在 AEndingEmergence 的 Painterly 分类调节。细胞阶段作用于角色镜头，血管阶段作用于血管镜头，并清除另一个镜头的效果。已有的 EnterBrain 转场在 1.25 秒内淡出血管油画，进入神经元阶段前清除；所有神经元、拉远电影和后续结局不使用。重启结局会恢复细胞画风。

验证：先运行 `Scripts/Build_Safe.ps1`，再执行 `Scripts/Run_MouseMuseumChecks.ps1 -ImportAssets`，最后执行 `Scripts/Run_MouseMuseumChecks.ps1 -Screenshots`。原生测试 `BrainMining.Campaign.MousePaintingEndingTravel` 验证真实地图切换、旧金蛋不触发、瞄准、重复进入、取消与输入恢复；`BrainMining.Ending.PainterlyStageScope` 验证各阶段的后处理范围。结果在 `Saved/Validation/MouseMuseum`。

2026-10-18 04:24 实测：最终增量编译成功；两项测试成功、0 失败。真实控制器 E 输入完成切关，实际角色胶囊可通过洞口，细胞和血管两个相机持有 CelPaint 后处理，离开血管时两个相机均移除该材质；重启可恢复。Gallery、Cell、Blood、Neurons 四张截图已人工检查。测试记录的 5 条既有警告来自联网探测超时、缺失的收集册预览材质以及原地图静态网格移动性，与本次入口断言无关。

吸入动画的推进必须先于博物馆的 IsInteractionOpen 判断，因为该判断把 IsLookInputIgnored 也视作界面打开；否则吸入自己持有的视角锁会暂停其动画。测试对 E 后切关增加了 12 秒独立超时，防止此问题回归。

## 2026-11-03：寻找画上圆圈任务

玩家解开机关、缩小并实际进入最后的老鼠房时，左侧现有引导栏立即增加「找到画上奇怪的圆圈」。仅进入主博物馆或解锁老鼠洞不会提前触发。原有进入房间三秒后的画作字幕提示保留，任务不等待这条字幕才显示。

任务在圆圈的正常交互成功、吸入镜头建立后完成，沿用任务栏勾选、淡出及完成提示音。仅看向圆圈、被物体遮挡的交互或不符合原入口条件的尝试不会完成；重复交互、取消吸入后重新进入也不会重复产生完成通知。后续 2.25 秒吸入和结尾切关保持原有流程。

`UMiningSaveGame` 与 `UMiningSaveSubsystem` 新增 `MuseumCircleTaskStarted`、`MuseumCircleTaskCompleted`，随现有存档保存和恢复。旧存档没有这些字段时默认为未触发，实际进入老鼠房后正常补发；旧结尾检查点 `Checkpoint=5` 视为已完成。开始新游戏时清空任务状态；保留收藏不保留本轮任务完成状态。正式版和 Demo 继续使用各自已有存档命名规则，不迁移玩家旧档。结尾首次保存检查点时已包含任务完成状态。

文案使用 `UI.Museum.CircleTask`，提供简体中文、英文及繁体中文。源文件为 `SourceAssets/Localization/Game.json`，修改后通过 `Scripts/build_localization.ps1` 生成三语运行资源。

专项成品验证入口：

```powershell
python Scripts/verify_museum_circle_task.py <成品Windows/WOoOW.exe> <全新验证输出目录>
```

脚本使用 `-NoSteam -MiningQA -RenderOffscreen` 与输出目录内独立的 `-UserDir`。机关解锁和接近房间使用明确的 QA 场景准备；任务触发、HUD、保存退出再继续、E 键交互及结尾切关走实际游戏路径。输出 `results.json`、状态记录与四张任务/结尾截图，不代表完整人工通关或真实 Steam 成就验收。

原生回归入口为 `BrainMining.Campaign.MouseCircleTaskSave`、`BrainMining.Campaign.MousePaintingEndingTravel` 和 `BrainMining.Campaign.MobileMousePaintingEndingTravel`。前者只在内存中验证任务字段序列化；后两者覆盖进入条件、实际交互、去重与结尾到达。其后处理断言已按现有血管权重及淡出流程校正：转场中验证淡出范围，神经元阶段验证两个镜头均已清除。本文记录验证范围，实际通过状态以本次运行报告为准。
