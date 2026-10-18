# MainGame 老鼠博物馆与画中细胞入口

两套机关仍控制老鼠洞入口。旧洞口从 64 cm 提高到 80 cm，以适配当前 0.38 倍、约 67 cm 高的玩家胶囊；普通大小仍不能通过。玩家变小进入后，后墙正对入口展示鼠鼠版《吻》，两侧放置微型金鼠展台。原金蛋及其底座在运行时隐藏、关闭碰撞；走到原金蛋位置不再完成或切关。

母鼠裙装中一个低调的红色圆环是细胞入口，大小接近其他纹样、仅略大。贴图 UV 中心 `(0.6374, 0.5656)`，画布 126 × 168 cm，图案半径约 2.9 cm，准星靠近半径 4.3 cm。平时保留原画，靠近时出现细白描边；描边与交互共用此坐标。只有已解锁、缩小、在洞内、视线未被遮挡且准星靠近圆环时，才显示「E 进入画中的圆环」。主流程接入博物馆控制器的 E 键路由，独立地图使用按键轮询。按 E 后镜头用 2.25 秒加速吸入，禁止重复触发，切换至 `/Game/EndingEmergence/Maps/Lvl_EndingEmergence_Demo`。新地图正常生成玩家，恢复常规大小和输入。

主线仅在 `BM_Campaign_MainGameMuseum` 区域切关。独立谜题测试地图使用同样的画中圆环交互，动画结束后标记完成并恢复操作，仍可重置。重置、退出区域与销毁均清理镜头与输入锁。

画作源文件在 `SourceAssets/MouseMuseum/T_MouseKiss.png`，内置 image_gen 的完整提示词记录在同目录 `Generation.txt`。导入脚本为 `Scripts/import_mouse_museum.py`，目标资源目录 `/Game/MouseMuseum`。

Art/CelPaintShader 中的原材质以原包路径 `/Game/CelPaint` 导入，专用实例为 `/Game/MouseMuseum/MI_EndingOilPaint`。细胞与血管分别使用 0.85、0.70 后处理权重，可在 AEndingEmergence 的 Painterly 分类调节。角色镜头和血管镜头都应用效果；EnterBrain 时立即移除，所有神经元、拉远电影和后续结局不使用。重启结局会恢复细胞画风。

验证：先运行 `Scripts/Build_Safe.ps1`，再执行 `Scripts/Run_MouseMuseumChecks.ps1 -ImportAssets`，最后执行 `Scripts/Run_MouseMuseumChecks.ps1 -Screenshots`。原生测试 `BrainMining.Campaign.MousePaintingEndingTravel` 验证真实地图切换、旧金蛋不触发、瞄准、重复进入、取消与输入恢复；`BrainMining.Ending.PainterlyStageScope` 验证各阶段的后处理范围。结果在 `Saved/Validation/MouseMuseum`。

2026-10-18 04:24 实测：最终增量编译成功；两项测试成功、0 失败。真实控制器 E 输入完成切关，实际角色胶囊可通过洞口，细胞和血管两个相机持有 CelPaint 后处理，离开血管时两个相机均移除该材质；重启可恢复。Gallery、Cell、Blood、Neurons 四张截图已人工检查。测试记录的 5 条既有警告来自联网探测超时、缺失的收集册预览材质以及原地图静态网格移动性，与本次入口断言无关。

吸入动画的推进必须先于博物馆的 IsInteractionOpen 判断，因为该判断把 IsLookInputIgnored 也视作界面打开；否则吸入自己持有的视角锁会暂停其动画。测试对 E 后切关增加了 12 秒独立超时，防止此问题回归。
