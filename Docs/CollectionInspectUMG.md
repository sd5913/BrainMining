# 藏品检视 UMG 编辑入口

在内容浏览器打开 `/Game/CollectionPrototype/UI/WBP_CollectionInspect`，切到“设计器”。
这是实际游戏使用的控件蓝图，布局不会在运行时重新生成。

- `LayoutCanvas`：1600×900 默认设计画布，外层 ScaleBox 适配屏幕。
- `PreviewPanel`：移动/缩放整个 3D 预览区，画面和操作区域一起移动。
- `InfoPanel`：右侧信息整组位置；内部可分别移动 `ItemName`、`ItemDescription`、`DescriptionDivider`。
- `TitleImage`：左上标题。
- `CloseButton`：右上返回按钮，含图标与点击区域。
- `CollectionBar`：底部整组收藏槽、前后翻页按钮。
- `ItemButton0`～`ItemButton3`：单个收藏槽位置和大小，图案与标签跟随按钮，额外白色选中框已移除。
- `PreviewHelp`：旋转缩放说明。

选中控件后，在 Canvas Panel Slot 中调整 Position X/Y、Size X/Y，或者直接拖动。
请保留动态控件的变量名；图标、文本、字体、颜色和布局可以改。

旋转、滚轮缩放、双击/F复位、左右箭头/按钮切换、返回/ESC关闭仍连接现有藏品系统。
未收集槽显示未知图案，已收集槽显示名称；预览使用现有 SceneCapture 和动态材质。
探索时的小提示 HUD 仍沿用原 Slate，打开检视后由 UMG 显示。

CollectionField 实例的 Collection > UI > Catalogue Widget Class 可指定自定义派生控件蓝图。
默认指向 WBP_CollectionInspect。编辑器生成函数只创建缺失资产，不覆盖已有设计器修改。

