# HUD 倾斜与外侧白边

普通 HUD 直接修改现有控件：右上金币和升级弹窗略向内倾斜，左下道具向相反方向倾斜，右侧钻机能量条也有轻微倾斜。准心和屏幕中央喷气背包条保持原来的位置。

`WBP_MiningHUD / UpgradePanel / UpgradeBackplate` 是图标、文字、价格按钮后面的半透明深色衬托，Z Order 为 −10，不参与点击；随整个弹窗一起出现和消失，自动覆盖四个升级项。矿石库存、任务和右上升级摘要另有各自的轻量底板；藏品快捷入口保持无框文字。

金币和道具图标的白边由原图 Alpha 轮廓生成，沿靠屏幕外侧约 1.4 像素的边缘提亮，保持原来的颜色和透明范围，不增加填充区域或新底板。道具正常/使用状态切换时同步替换材质的 Artwork 参数。

材质在 `UI/MiningGUI/Immersive`：`M_ArtworkOuterRim` 控制原图白边，`MI_CurrencyRim` 与两种道具白边实例可调整 PixelStep / OuterSide；`MI_UpgradeBackplate` 的 BackdropOpacity 默认 0.42，可调整升级底板透明度。

金币、道具和升级弹窗的位置及大小仍由原有 UMG 插槽决定，其倾斜通过 Render Shear 实现。升级点击仍走原来的购买逻辑。脚本 `Scripts/style_immersive_hud.py` 可重复应用，备份与验证在 `Saved/Validation/ImmersiveHUD`。

## 矿石阶段 HUD（2026-11-07）

右上升级摘要按当前游戏语言选择宽度：简体、繁体中文 382，英文 466；切换语言时原位更新，保持右侧锚点。满级继续使用 128 宽的圆环摘要。

左侧库存、任务和藏品入口共用 300 宽的纵向排布，间距 12，统一外侧边线和文字列。任务标记独立成列，文本换行后按实际高度推开下一行和藏品入口；没有任务时收起整块任务底板。

`LeftHUDStack` 使用普通 Canvas 排布，统一应用矿石统计原有的 `(0,2)` Render Shear，取消弧面投影和各块独立倾斜。矿石、任务与藏品入口继承同一倾斜角度，边线与文字基线相互平行。藏品入口仅显示文字，不增加框或底板。

左侧两块底板组成一个整体收窄的造型：矿石底板仅上边收窄（`TopTaperPixels=2 / BottomTaperPixels=0`），任务底板仅下边收窄（`0 / 2`）。中间相邻的两条边使用固定内边距，在共同倾斜后仍等距平行；任务增高换行时保持同一规则。`M_InsetBackplate` 的默认值为 `2 / 2`，右侧沿用原有造型。材质通过 `Scripts/style_hud_gradient.py` 更新。

原生测试 `BrainMining.Localization.MineralHUDAndFlightSign` 检查三种语言切换和满级布局。`Scripts/capture_hud_layout.py` 在真实 MainGame 的隔离 PIE 中检查早期任务、四种矿石、博物馆圆圈、满级和无任务状态，并输出三种语言截图。运行须带 `-NoSteam -MiningQA -UserDir=<独立目录>`；该脚本使用临时进度，不验证正常通关或真实 Steam 成就。
