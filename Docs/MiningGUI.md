# 挖掘 GUI（UMG 可编辑版）

运行关卡后由本地 `BrainMiningPlayerController` 创建 `/Game/UI/MiningGUI/WBP_MiningHUD`，不依赖关卡蓝图。原来的准星、挖掘和镐子效果继续保留。为了直接点击 UI，鼠标默认显示；按住鼠标右键转动视角，左键仍用于挖掘。控制器的 `bEnableGUICursor` 可关闭此鼠标设置，方便以后接入其他输入方案。

## 在 UE 编辑

在内容浏览器打开 `Content/UI/MiningGUI/WBP_MiningHUD`，进入 **Designer（设计器）**。右上金币栏和升级面板使用右上锚点，技能栏使用左下锚点；画布透明，使用项目原有 DPI 缩放。设计器里显示升级面板以方便编辑，运行时默认收起。

| 资源 | 可编辑内容 |
|---|---|
| `WBP_MiningHUD` | 布局、锚点、金币文字、加号、面板位置 |
| `WBP_AbilityG` / `WBP_AbilityV` | 技能图标、红色使用态、按键图、大小与间距 |
| `WBP_UpgradePickaxe` / `WBP_UpgradeVortex` / `WBP_UpgradeBlast` | 环、中心图标、锁定图层、三个独立绿色环段、整块金币按钮和价格文字 |
| `Art/T_*` | 从 GUI.zip 拆出的透明 PNG 图层 |

所有布局均存为 Widget Blueprint 的组件树，运行代码不重建布局。UI 图片使用 UI 无损压缩、不生成 mip、不流送，保持清晰。原图 PNG 位于 `SourceAssets/MiningGUI`，可重导入。制作脚本只拆分原图和给既有环段替换绿色，不更改原 ZIP。

## 演示交互

- G/V 将对应技能标为已使用，显示原图的红色斜杠；重复按键保持使用态。未解锁技能通过 `SetAbilityAvailable` 隐藏。
- 点击金币栏加号展开/收起三张卡片。默认第三张显示独立锁定图层。
- 每张卡片整个金币按钮可点，每点一次增加一段绿色，最多三段。锁定图层在演示阶段不阻止按钮，用于独立展示状态；后续程序决定是否允许升级及何时解锁。
- 这里只管理显示状态，不检查金币、不扣金币、不改角色能力、不存档。金币栏和三个价格区域均留空，游戏 GUI 不显示任何占位数字。

## 程序对接

控制器的 `MiningGUI` 提供运行中的 HUD 引用。HUD 蓝图有可绑定事件：

- `OnAbilityRequested(Ability)`：G/V 输入请求。
- `OnUpgradeRequested(ItemIndex, RequestedLevel)`：0 镐子、1 旋风、2 爆破的升级请求。
- `OnLevelChanged(ItemIndex, Level)`、`OnPanelChanged(bOpen)`：显示状态通知。

程序接管时关闭 HUD 的 `bPreviewAbilities`，关闭三张卡片的 `bPreviewUpgrades`。事件仍触发，由程序验证后调用 `SetAbilityUsed`、`SetAbilityAvailable`、`SetItemLevel`、`SetItemUnlocked`；金币使用 `SetCoinDisplay(FText)`，价格使用卡片的 `SetPriceDisplay(FText)`。等级设置限于 UI 的 0–3 个环段，不实现任何经济算法。

数值与字体后续一起由程序接入。HUD 的 `CoinText` 和卡片的 `PriceText` 保留为空的可编辑 TextBlock，可通过它们的 `SetFont(FSlateFontInfo)` 设置正式字体。

## Q 弹动画

使用带阻尼的弹簧运动：展开先冲过目标再回弹，关闭缩回加号方向，按下按钮压缩、释放反弹，升级和技能使用时图标轻弹。不是线性淡入淡出。蓝图 **Class Defaults（类默认值） → GUI / Motion** 可调整刚度 `SpringStiffness` 和阻尼 `SpringDamping`（HUD 为 `PanelSpring*`）。阻尼较小更弹，刚度较大更快。移动只作用于渲染变换，保留设计器的布局位置。

原开发调试信息可在 `MiningHUD.bShowDeveloperOverlay` 重新打开，默认隐藏以避免覆盖 GUI；原准星保留。

## 重新制作与验证

先用带 Pillow 的 Python 运行 `Scripts/prepare_mining_gui_art.py <GUI.zip路径>`，再在 UE Python 执行 `Scripts/import_mining_gui.py`。导入脚本保留已有资产的人工编辑，不自动覆盖。

运行验证脚本 `Scripts/verify_mining_gui.py` 会在 PIE 中检查 UI、输入、按钮、环段、程序接口、动画和原挖掘，并输出 `Saved/Validation/MiningGUI` 的 JSON 与实际游戏截图，不保存测试状态到资产。
