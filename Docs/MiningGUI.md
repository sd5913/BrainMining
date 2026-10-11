# 挖掘 GUI（UMG 可编辑版）

运行关卡后由本地 `BrainMiningPlayerController` 创建 `/Game/UI/MiningGUI/WBP_MiningHUD`，不依赖关卡蓝图。平时鼠标隐藏，直接移动鼠标转动视角，左键用于挖掘。按住 **Tab** 展开升级面板、显示鼠标并锁住视角；松开 Tab 后面板自动回弹收起、鼠标隐藏，立即恢复视角操作，不需要额外点击画面。

升级期间取消原来持续按住的挖掘输入，点击 UI 或弹窗空白处不会同时挖掘，右键也不会抢走鼠标。松开 Tab 后需要新的一次左键按下才能继续挖掘。切出应用会关闭升级面板，避免视角锁住。已有挥镐动画、碎屑和移动设置继续保留。

## 在 UE 编辑

在内容浏览器打开 `Content/UI/MiningGUI/WBP_MiningHUD`，进入 **Designer（设计器）**。右上金币栏和升级面板使用右上锚点，技能栏使用左下锚点；画布透明，使用项目原有 DPI 缩放。设计器里显示升级面板以方便编辑，运行时默认收起。

| 资源 | 可编辑内容 |
|---|---|
| `WBP_MiningHUD` | 布局、锚点、金币文字、加号、面板位置 |
| `WBP_AbilityG` / `WBP_AbilityV` | 技能图标、红色使用态、按键图、大小与间距 |
| `WBP_UpgradePickaxe` / `WBP_UpgradeVortex` / `WBP_UpgradeBlast` | 环、中心图标、锁定图层、四个独立绿色环段、整块金币按钮和价格文字 |
| `Art/T_*` | 从 GUI.zip 拆出的透明 PNG 图层 |

所有布局均存为 Widget Blueprint 的组件树，运行代码不重建布局。UI 图片使用 UI 无损压缩、不生成 mip、不流送，保持清晰。原图 PNG 位于 `SourceAssets/MiningGUI`，可重导入。制作脚本只拆分原图和给既有环段替换绿色，不更改原 ZIP。

## 演示交互

- G/V 将对应技能标为已使用，显示原图的红色斜杠；重复按键保持使用态。未解锁技能通过 `SetAbilityAvailable` 隐藏。
- 按住 Tab 显示三张卡片，松开自动收回。金币栏的加号图案保留为装饰，不再切换弹窗。默认第三张显示独立锁定图层。
- 已解锁卡片的整个金币按钮可点，每点一次增加一段绿色，最多四段。上锁卡片的金币按钮禁用，不触发升级请求、不显示绿色环；程序调用 `SetItemUnlocked` 解锁后才能升级。面板关闭时全部升级按钮立即禁用，避免关闭动画期间或残留鼠标释放误触。
- 这里只管理显示状态，不检查金币、不扣金币、不改角色能力、不存档。金币栏和三个价格区域均留空，游戏 GUI 不显示任何占位数字。

## 程序对接

控制器的 `MiningGUI` 提供运行中的 HUD 引用，`bUpgradeMenuHeld` 表示当前 Tab 状态。通过控制器的 `SetUpgradeMenuHeld` 统一切换弹窗和输入状态；HUD 会接住取得焦点后的 Tab 松开事件，点击升级按钮不会吞掉松开操作。HUD 蓝图有可绑定事件：

- `OnAbilityRequested(Ability)`：G/V 输入请求。
- `OnUpgradeRequested(ItemIndex, RequestedLevel)`：0 镐子、1 旋风、2 爆破的升级请求。
- `OnLevelChanged(ItemIndex, Level)`、`OnPanelChanged(bOpen)`：显示状态通知。

程序接管时关闭 HUD 的 `bPreviewAbilities`，关闭三张卡片的 `bPreviewUpgrades`。事件仍触发，由程序验证后调用 `SetAbilityUsed`、`SetAbilityAvailable`、`SetItemLevel`、`SetItemUnlocked`；金币使用 `SetCoinDisplay(FText)`，价格使用卡片的 `SetPriceDisplay(FText)`。等级设置限于 UI 的 0–4 个环段，不实现任何经济算法。

数值与字体后续一起由程序接入。HUD 的 `CoinText` 和卡片的 `PriceText` 保留为空的可编辑 TextBlock，可通过它们的 `SetFont(FSlateFontInfo)` 设置正式字体。

## Q 弹动画

使用带阻尼的弹簧运动：按住 Tab 展开先冲过目标再回弹，松开时缩回加号方向，按下升级按钮压缩、释放反弹，升级和技能使用时图标轻弹。不是线性淡入淡出。蓝图 **Class Defaults（类默认值） → GUI / Motion** 可调整刚度 `SpringStiffness` 和阻尼 `SpringDamping`（HUD 为 `PanelSpring*`）。阻尼较小更弹，刚度较大更快。移动只作用于渲染变换，保留设计器的布局位置。

原开发调试信息可在 `MiningHUD.bShowDeveloperOverlay` 重新打开，默认隐藏以避免覆盖 GUI；原准星保留。

## 重新制作与验证

先用带 Pillow 的 Python 运行 `Scripts/prepare_mining_gui_art.py <GUI.zip路径>`，再在 UE Python 执行 `Scripts/import_mining_gui.py`。导入脚本保留已有资产的人工编辑，不自动覆盖。

运行验证脚本 `Scripts/verify_mining_gui.py` 会在 PIE 中检查真实游戏鼠标轴输入、Tab 按住/松开和 Slate 焦点、镜头锁定及恢复、按下按钮时释放 Tab、锁定和解锁升级、环段、动画与原挖掘，并输出 `Saved/Validation/MiningGUI` 的 JSON 与实际游戏截图，不保存测试状态到资产。


## 四级升级圆环

三个道具仍保留各自独立的卡片，每张卡片现在最多升四级。绿色环段按左上、右上、右下、左下顺序点亮。`GreenSegment1` 至 `GreenSegment4` 是可编辑 UImage，圆环材质在 `/Game/UI/MiningGUI/FourLevel`，原始 PNG 在 `SourceAssets/MiningGUIFourLevel`。原图透明通道与颜色保留，材质负责裁掉空白和旋转同一个绿色环段，未修改源图片。锁定卡片保留四段暗色环和锁，不接受升级。原中心图标、金币按钮、Tab 输入及弹簧动画保留。

运行 `Scripts/upgrade_gui_four_levels.py` 更新已有升级卡片；此脚本不重建 HUD 布局。
