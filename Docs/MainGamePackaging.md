# MainGame Windows 打包

使用 UE 5.8 与 Windows 64 位 Shipping 配置。启动地图为 `/Game/MainGame/Maps/Lvl_MainGame`；博物馆结尾入口通往 `/Game/EndingEmergence/Maps/Lvl_EndingEmergence_Demo`。

## 重新打包

保存地图和资源，关闭游戏与 Unreal 编辑器，然后在项目根目录运行：

```powershell
./Scripts/package_main_game.ps1
```

默认输出到 `Builds/MainGame-Windows-时间戳/Windows`。脚本优先使用 `UE_ENGINE_ROOT`，然后检查本机常用的 UE 5.8 安装路径；也可通过 `-EngineRoot` 指定引擎位置，通过 `-OutputDirectory` 指定输出目录。`-SkipBuild` 和 `-SkipCook` 仅用于已确认程序、配置和资源没有改变的续跑，正常发布不要使用。

每次打包使用独立的时间戳目录，不覆盖先前交付。

## 范围与易漏内容

- 两张入口地图由 `MapsToCook` 指定。起始房、电脑房、博物馆等子关卡及其引用资源由 Cook 自动收集。不要只复制 MainGame 的 `.umap`。
- 金蛋结尾通过原生代码路径换关，必须保留结束地图。结尾贴图、声音、材质、涂鸦，以及炸弹、钻机、碎屑、博物馆材质等动态加载内容由 `DirectoriesToAlwaysCook` 显式收集。
- 结尾人群使用已有的 `EndingEmergence/Crowd/Lite`。不要把整个 `EndingEmergence/Crowd` 目录强制加入打包，否则会额外带入制作阶段的源角色、骨骼和 8K 贴图，增加包体和烘焙内存；必要的真实引用仍由 Cook 自动收集。
- 中文界面依赖字体与国际化数据，保留中文和英文资源。音效来源说明随包保留。
- 卡牌机器人的部件通过 C++ 路径动态加载，必须保留 `/Game/Rooms/CardDuel/Robot` 的 `DirectoriesToAlwaysCook` 配置。
- BloodCirculation 血细胞网格与血管/细胞材质实例同样通过 C++ 路径加载，必须保留这些目录的强制 Cook 配置。打包脚本会检查实际地图依赖及新谢幕、血管、封口膜、标题透视材质，避免把已被替换的旧布景误当作必需资源。
- 应用图标原图保存在 `SourceAssets/Branding/ApplicationIcon.png`，Windows 多尺寸图标位于 `Build/Windows/Application.ico`；交付前需核验启动 EXE 与 Shipping EXE 的所有图标组。
- 矿物线索贴花 `M_BuriedMineralGlow` 通过运行时代码加载，必须保留 `/Game/MainGame/Materials` 的强制 Cook 配置；缺失时引擎默认贴花会在岩壁上显示大片亮块。打包脚本现在检查最终 IoStore 容器内是否包含该材质，缺失直接报错；运行时代码也会隐藏无材质的线索贴花。
- 后处理不能为兼容移动端而清空桌面着色器。`repair_android_postprocess_materials.py` 现从原始制作脚本恢复描边及五种结局效果，只在 `SHADING_PATH_MOBILE` 分支降级。最终包检查包含这六种材质；Windows SM5/SM6 Cook 已通过，移动端需另行验证。
- 只会排除未引用的资源。蓝图、软引用或默认类引用仍可能带入额外地图/素材，清理这些依赖需要另行评估，不能直接删除源资源。

## 交付与运行

分发整个 `Windows` 目录（或它的完整压缩包），用户解压后启动根目录的 `BrainMining.exe`。不能只发送 EXE，也不要移动或删除旁边的 `BrainMining`、`Engine`、`.pak`、`.utoc`、`.ucas` 文件。

目标设备使用 Windows 64 位与支持项目 DX12/SM6 渲染设置的显卡，建议更新显卡驱动。运行库安装程序会随包提供；目标电脑缺少 Microsoft Visual C++ 运行库时安装随包的 x64 运行库。接收者不需要安装 Unreal 编辑器。

Shipping 版本不提供开发控制台和完整开发日志。按 2026-10-19 试玩反馈，Ctrl+F10 开发者菜单、主流程区域和收集物附近的 GM 跳转现已保留；未打入包的独立测试地图入口会禁用。Esc 暂停、收集界面和退出确认保留。本轮收集记录会从 MainGame 带到结局谢幕，“再玩一次”会清空本轮收集和装备升级。当前没有完整的跨次游玩存档流程，不能将试玩包当作已验证的断点续玩版本。

发布前应检查首次启动、中文 UI、工具拾取、挖掘与碰撞、炸弹/钻机/碎屑、房间切换，以及博物馆到结尾的跳转。启动和菜单冒烟检查不等于完整人工通关；具体交付验证范围见当次报告。

## 空间与构建

首次构建会编译 C++ 与 Windows 着色器，耗时明显长于后续增量打包。脚本要求输出盘至少有 20 GB 空余，并把派生缓存放在项目的 `Saved/DerivedDataCache`；实际应根据资产规模预留更多空间。资源监控会停止自身启动的任务以避免内存耗尽，不会关闭其他应用。

当前脚本限制烘焙器可用核心数和资源编译并发，以适应本机的内存余量。UE 命令行 Cook 不采用 `PercentageUnusedShaderCompilingThreads` 的百分比限制，因此不能只依靠该项控制内存。限制并发不降低最终贴图质量。

依赖审计：`Scripts/audit_main_game_package.py`。打包请求、资源用量和报告位于 `Saved/Validation/MainGamePackage`。后续脚本运行的 UAT 日志位于其 `UAT` 子目录。

## 2026-10-18 试玩交付

交付文件：`Builds/MainGame-Windows-20261018-225622.zip`，1,499,634,864 字节。解压后包含 `Windows/BrainMining.exe`、试玩说明与构建信息，共 46 个文件。

Shipping 编译、Cook、Stage、Pak/IoStore 与 Archive 均成功。Cook 完成 2,126 个资源包，0 错误、2 警告；12 项关键地图和动态加载资源容器检查通过。ZIP 全文件 CRC、文件清单及大小与交付目录一致，SHA-256 保存在压缩包旁的 `.sha256.txt` 文件中。

本次补充机器人动态资源的强制 Cook 路径，并将炸弹物理材质应用延后到 Launch，解决 Cook 时引擎尚未初始化的错误。电脑控制工具访问超时后，用户选择直接接收包自行试玩，因此未完成实际启动、菜单操作或通关验证。详细交付记录见 `Saved/Validation/MainGamePackage/delivery.json`。

## 2026-10-19 矿物发光修复包

交付文件：`Builds/MainGame-Windows-MineralGlowFix.zip`，1,407,085,561 字节。请解压到新目录运行，避免误用旧版本。

旧包容器清单确实缺失 `M_BuriedMineralGlow`，新包容器检查确认已包含；原有材质资源和发光参数未改动。Shipping 构建与 2,138 个资源包 Cook 成功（0 错误、2 警告），ZIP 全文件 CRC 与交付目录一致。`BrainMining.Pickaxe.MineralGlowRange` 回归通过，覆盖正常材质显示、范围调整、关卡光效关闭以及缺失材质时隐藏默认贴花。未执行修复包的人工画面复测。验证与校验码记录位于 `Saved/Validation/MineralGlowFix`，ZIP 旁提供 SHA-256 文件。

## 2026-10-19 包体体验与钻机修复

交付文件：`Builds/MainGame-Windows-PlaytestFix-20261019.zip`，1,443,272,625 字节，46 个文件。Shipping 编译、Cook、Stage 和 Archive 成功；Cook 0 错误、2 警告，18 项关键容器资源存在，ZIP 全文件 CRC、清单及尺寸验证通过。SHA-256：`536ab89a288148eacab3135ae7a2794b997b7c5c6fa29514e5789af14b433ecd`。

包括本轮 21 项反馈、撤回钻机下探保护、主场景低位钻机、结局原始模型偏移与透明度复位。移除 MainGame 初始化授予炸弹的遗留逻辑，炸弹与钻机分别在电脑房和卡牌房拾取后解锁。主场景实际输入和渲染、办公室炸弹出口、MainGame 卡牌／电视逃脱、结局钻挖至视频与谢幕回归通过。本轮最终包未重新人工连续通关。详细记录：`Docs/PackagedPlaytestFeedback-20261019.md`，交付校验：`Saved/Validation/PackagePlaytest/delivery.json`。本地源码未提交或推送。
