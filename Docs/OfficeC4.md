# 电脑弹窗房间的 C4 模型

办公室首次弹出的技能拾取物改用 `/Game/Bomb/SM_StickyBomb`，保留完整材质和 18,013 个三角形，显示比例为 1.6。网格按缩放后的中心补偿到球形物理体，球半径约 21.8 cm，保留弹出、重力、CCD、接近并瞄准后按 E 拾取的流程。

玩家离开房间后，电脑和打印机喷出的 C4 使用独立副本 `/Game/PopupClose/FX/SM_OfficeFinaleC4`。其 LOD1 为 721 个三角形，由组件强制使用；完整玩家模型不修改。所有喷发炸弹共享一个实例化组件及三份材质，不创建单独 Actor、刚体、倒计时组件或动态材质实例；关闭碰撞、阴影、距离场照明和 WPO，沿用 30 Hz 批量变换更新。

实例池从 384 降为 256。12 个喷口保持原位，喷发轮次由每秒 3→10 轮降为 2→7 轮；默认五秒喷发数量约减少三分之一。满池绘制几何约为 184,576 个三角形，比旧方案 229,632 个约少 20%。这只是几何预算比较，不代表已测得相同幅度的帧率提升。

常量 `PickupBombScale`、`BombBudget` 与轮次曲线在 `Source/BrainMining/PopupClose/PopupOfficeExit.cpp`。喷发持续时间继续使用房间 Actor 的 `PopupClose | Exit > Evacuation Bomb Seconds`。构建配置显式包含 `/Game/PopupClose/FX`，保证通过原生路径加载的简化模型参与打包。

资产生成流程是 `Scripts/prepare_office_c4.py` 创建副本，通过 StaticMeshTools 的 `generate_lods` 使用 `triangle_percents=[0.04]` 生成 LOD1，再运行 `Scripts/save_office_c4_lods.py` 保存并核验。原始 C4 和房间地图不改动。运行检查使用 `Scripts/verify_main_game_office_c4.py` 及原生自动化 `BrainMining.Office.ExitFlow`，记录位于 `Saved/Validation/OfficeC4`。测试中的位置调整与加速只影响 PIE 副本。

2026-10-15 验收：原生编译成功；MainGame 的 23 项脚本检查通过，默认五秒喷发实测共 264 颗，坍塌后实例归零；独立办公室完整流程原生测试通过，无错误或警告。重开编译后的编辑器验证 LOD 仍为 721 个三角形；原 C4、MainGame 地图、MainGame 玩家蓝图及 `Lvl_PopupClose1` 文件哈希保持不变。最终记录为 `Saved/Validation/OfficeC4/acceptance.json`。
