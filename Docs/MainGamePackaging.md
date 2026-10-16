# MainGame Windows 打包

使用 UE 5.8 与 Windows 64 位 Shipping 配置。启动地图为 `/Game/MainGame/Maps/Lvl_MainGame`；金蛋拾取后进入 `/Game/EndingEmergence/Maps/Lvl_EndingEmergence_Demo`。

## 重新打包

保存地图和资源，关闭游戏与 Unreal 编辑器，然后在项目根目录运行：

```powershell
./Scripts/package_main_game.ps1
```

默认输出到 `Builds/MainGame-Windows-时间戳/Windows`。可通过 `-EngineRoot` 指定引擎位置，通过 `-OutputDirectory` 指定输出目录。`-SkipBuild` 和 `-SkipCook` 仅用于已确认程序、配置和资源没有改变的续跑，正常发布不要使用。

本次交付目录为 `Builds/MainGame-Windows-20261016/Windows`，Shipping 文件约 1.29 GiB（31 个文件）。

## 范围与易漏内容

- 两张入口地图由 `MapsToCook` 指定。起始房、电脑房、博物馆等子关卡及其引用资源由 Cook 自动收集。不要只复制 MainGame 的 `.umap`。
- 金蛋结尾通过原生代码路径换关，必须保留结束地图。结尾贴图、声音、材质、涂鸦，以及炸弹、钻机、碎屑、博物馆材质等动态加载内容由 `DirectoriesToAlwaysCook` 显式收集。
- 结尾人群使用已有的 `EndingEmergence/Crowd/Lite`。不要把整个 `EndingEmergence/Crowd` 目录强制加入打包，否则会额外带入制作阶段的源角色、骨骼和 8K 贴图，增加包体和烘焙内存；必要的真实引用仍由 Cook 自动收集。
- 中文界面依赖字体与国际化数据，保留中文和英文资源。音效来源说明随包保留。
- 只会排除未引用的资源。蓝图、软引用或默认类引用仍可能带入额外地图/素材，清理这些依赖需要另行评估，不能直接删除源资源。

## 交付与运行

分发整个 `Windows` 目录（或它的完整压缩包），用户解压后启动根目录的 `BrainMining.exe`。不能只发送 EXE，也不要移动或删除旁边的 `BrainMining`、`Engine`、`.pak`、`.utoc`、`.ucas` 文件。

目标设备使用 Windows 64 位与支持项目 DX12/SM6 渲染设置的显卡，建议更新显卡驱动。运行库安装程序会随包提供；目标电脑缺少 Microsoft Visual C++ 运行库时安装随包的 x64 运行库。接收者不需要安装 Unreal 编辑器。

Shipping 版本不提供开发控制台和完整开发日志。本次已完成：Shipping 构建、1612 个资源 Cook、IoStore 容器生成、默认启动进程检查、主地图和结尾地图及工具资源在容器清单中的检查。由于验证机在运行截图时处于 Windows 锁屏状态，未进行鼠标键盘通关操作；发布前仍应在解锁桌面后检查首次启动、中文 UI、工具拾取、挖掘与碰撞、炸弹/钻机/碎屑、房间切换，以及金蛋到结尾的跳转。

## 空间与构建

首次构建会编译 C++ 与 Windows 着色器，耗时明显长于后续增量打包。脚本要求输出盘至少有 20 GB 空余，并把派生缓存放在项目的 `Saved/DerivedDataCache`；实际应根据资产规模预留更多空间。资源监控会停止自身启动的任务以避免内存耗尽，不会关闭其他应用。

当前脚本限制烘焙器可用核心数和资源编译并发，以适应本机的内存余量。UE 命令行 Cook 不采用 `PercentageUnusedShaderCompilingThreads` 的百分比限制，因此不能只依靠该项控制内存。限制并发不降低最终贴图质量。

依赖审计：`Scripts/audit_main_game_package.py`。打包请求、资源用量和报告位于 `Saved/Validation/MainGamePackage`。后续脚本运行的 UAT 日志位于其 `UAT` 子目录。
