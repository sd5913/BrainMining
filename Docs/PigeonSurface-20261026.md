# 肉鸽洞外线索、土块衔接与钻机恢复

- 正式 MainGame 的肉鸽线索改到 Branch_03 泥土封堵的主通路一侧。GM「肉鸽」落点位于封堵前约 3 米的地面上，视角对准泥土入口，不再落在封堵后的支洞内。
- 外侧七片羽毛直接查询实际土层网格，避免运行时碰撞尚未生成时定位失败。支撑点深入土面 8 cm，按羽片端点计算外侧间隙；挖掉支撑后仍会飘落、渐隐。
- 泥土中心保留既有 `MI_ClayBlockage` 的质感，岩壁使用原 `M_BlueViolet`。使用正常土层已有的低多边形土块方法，在封堵两侧生成大小错落、嵌入墙面的可挖实体。土块归属于同一矿区，标签为 `BM_PigeonSoilClods`。原来的生成流程因这块封堵属于 bomb-only 而跳过了它。
- 土块采用稀疏布局：两侧合计 19 块（此前 82 块），去掉碎小颗粒，增加块间留白，集中在封堵附近 1.8 米内。墙面土块的深度半径改为 16–25 cm（再按距离轻微缩放），中心位于岩壁表面附近，参照正常土块增加凸起和侧面体积；地面块限制凸起，保留落脚空间。使用原材质边缘，没有额外颜色过渡或贴花。
- 钻机有效恢复倍率从配置值的 0.30 降为 0.15，默认每秒 3 → 1.5；升级后的配置也按相同比例减半。消耗、容量、无限能量逻辑保持现有规则。

## 验证与复现

修改地图前备份到 `Saved/Backups/PigeonSurface-*`。定点生成脚本为 `Scripts/apply_pigeon_surface.py`，不会重建其他矿道；重复运行替换同一个土块 Actor。

```powershell
& Scripts/Build_Safe.ps1
& Scripts/Run_MainGameCollectionScript.ps1 -Script apply_pigeon_surface.py
& Scripts/Run_PigeonSurfaceChecks.ps1 -RunName Runtime
& Scripts/Run_PigeonSurfaceChecks.ps1 -RunName Native -Native
```

运行测试使用隔离 UserDir。`gm_collectible` 调用实际开发菜单的传送方法；先截取真实落点，再进行固定视角检查、等待、挖除支撑和土块碰撞检查。此验证不是全流程通关，也未更新 Windows 发布 ZIP。

Editor Development 编译通过。`Saved/Validation/PigeonSurface/Native1/Native/index.json` 的 9 项 Collection / CollectionAudio / Progression 测试通过，其中两项含测试世界清理警告。立体稀疏土块版本的运行报告与截图位于 `Saved/Validation/PigeonSurface/VolumeClods`，此前稀疏布局截图保存在 `SparseClods`。
