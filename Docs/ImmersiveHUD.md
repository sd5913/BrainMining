# HUD 倾斜与外侧白边

普通 HUD 直接修改现有控件：右上金币和升级弹窗略向内倾斜，左下道具向相反方向倾斜，右侧钻机能量条也有轻微倾斜。准心和屏幕中央喷气背包条保持原来的位置。

**只有弹出的道具升级面板有底板。** `WBP_MiningHUD / UpgradePanel / UpgradeBackplate` 是图标、文字、价格按钮后面的半透明深色衬托，Z Order 为 −10，不参与点击；随整个弹窗一起出现和消失，自动覆盖四个升级项。

金币和道具图标的白边由原图 Alpha 轮廓生成，沿靠屏幕外侧约 1.4 像素的边缘提亮，保持原来的颜色和透明范围，不增加填充区域或新底板。道具正常/使用状态切换时同步替换材质的 Artwork 参数。

材质在 `UI/MiningGUI/Immersive`：`M_ArtworkOuterRim` 控制原图白边，`MI_CurrencyRim` 与两种道具白边实例可调整 PixelStep / OuterSide；`MI_UpgradeBackplate` 的 BackdropOpacity 默认 0.42，可调整升级底板透明度。

位置和大小仍由原有 UMG 插槽决定，只修改 Render Shear。升级点击仍走原来的购买逻辑。脚本 `Scripts/style_immersive_hud.py` 可重复应用，备份与验证在 `Saved/Validation/ImmersiveHUD`。
