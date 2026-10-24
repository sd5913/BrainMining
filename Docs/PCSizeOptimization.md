# PC 包体优化

## 状态

新增独立 `PCSize` Cook/打包配置，默认 Windows 和 Android 配置保持原有行为。2026-10-24 已完成实际 Windows Shipping 双构建、包体统计和运行冒烟验证：ZIP 缩小 42.17%。完整连续通关尚未验证。

2026-10-24 首次实现验证：10 项 Python 自动测试通过；两个 PowerShell 脚本语法检查通过；Baseline/Optimized 参数隔离检查通过，扫描到 7 个完整原生材质入口。读取现存 MiningPrototype 的真实主容器和 global TOC 成功，分别识别 6,284 和 50 个压缩块；这些旧容器仅用于解析器验证，不作为当前游戏的体积基准。

首次启动因可用物理内存约 2.1 GiB 被资源保护阻止。重试 `PCSize-20261024-retry01` 完成基准构建后，优化阶段又因可用物理内存约 5.6 GiB 未达到 6 GiB 门槛而暂停；基准成果保留，运行检查结束、内存回升至 6.2 GiB 后继续优化 Cook。继续时使用 `package_main_game.ps1 -CustomConfig PCSize -SkipBuild` 复用已成功编译的同一 Shipping 执行文件，完成新的 Cook、Stage、Archive 和报告，未降低资源保护门槛。

## 本次实测结果

运行目录：`Builds/PCSize-20261024-retry01`。报告目录：`Saved/Validation/PCSize/PCSize-20261024-retry01`。

上述双构建实测基于拉取前的 `acbd4742` 加本次优化改动。随后已整合远端 `40ce83e4` 的玩法更新和新增打包资源清单，但尚未重新打包该版本；42.17% 的降幅和运行检查结果不代表更新后版本的实测结果。

| 口径 | Baseline | Optimized | 减少 | 降幅 |
| --- | ---: | ---: | ---: | ---: |
| ZIP 文件 | 1,807.31 MiB | 1,045.14 MiB | 762.17 MiB | 42.17% |
| 解压后发布目录 | 1,959.16 MiB | 1,192.70 MiB | 766.45 MiB | 39.12% |
| PAK/IoStore 容器 | 1,708.32 MiB | 942.06 MiB | 766.26 MiB | 44.85% |

优化 ZIP：`Builds/PCSize-20261024-retry01/Optimized/BrainMining-Windows.zip`，实际长度 1,095,913,518 字节。

SHA-256：`a0ff44bcf951043d9736cf90ff6ac04ed5a7d86ca682be4850b10e8fd1326082`。

主要压缩块归因收益：Office 246.25 MiB、Museum 138.61 MiB、HDRIBackdrop 99.60 MiB、Fab 45.01 MiB、Debris 38.49 MiB。此归因不等同于分别压缩文件夹的 ZIP 长度，详见 `size-comparison.json` 和两版 `asset-sizes.csv`。

优化包通过 418 项必需资源清单检查；HDRIBackdrop 内容未进入容器。两版 ZIP 逐文件 SHA-256 回读成功，发布目录未含 PDB/OBJ/LIB/EXP。两版 Shipping 执行文件和结局视频 SHA-256 相同。

参数生效证据：优化 Cook 日志记录 `Final: RDO On Lambda=10`，UnrealPak 日志记录 `method=Kraken, level=6=Optimal2`。`texture-cook-samples.json` 记录实际容器逻辑数据变化：4K 画框颜色贴图约 10.67 → 2.67 MiB、4K 画框/地毯法线约 21.33 → 5.33 MiB、8K 星球日间贴图约 21.33 → 1.33 MiB；标题 UI 的 2,458,115 字节保持一致。Cook 日志的 “Building texture” 尺寸来自源数据，不能直接作为最终入包尺寸。

运行验证使用 `Scripts/verify_pc_size_package.py`，两版 `Smoke02/results.json` 均通过 13 项检查、生成 11 张截图，覆盖中文菜单、进入主游戏、工具解锁、办公室/回声卡牌区域/第三矿区/博物馆切换、暂停菜单、结局细胞入口，以及街道/星球/宇宙镜头，最终正常退出。两版使用相同脚本、独立新存档及相同分辨率启动参数。

`visual-comparison-1.png` 至 `visual-comparison-3.png` 已逐张检查，在采样视点未见明显缺材质或场景布局回归。动态图像并非逐像素一致；卡牌截图仅覆盖入座入口，未验证完整战斗。没有进行完整连续通关、所有收集物交互、跨机器兼容或长时间性能测试，不能据此承诺完全无画质差异。`runtime-validation.json` 记录范围与证据，运行状态为 `packaged_smoke_verified`；完整通关仍为 pending。

## 运行

在项目根目录执行：

```powershell
pwsh -NoProfile -File Scripts/Package_PC_SizeOptimized.ps1
```

默认顺序生成 Baseline 与 Optimized 两个 Shipping 包，每个包都重新构建和 Cook，独立使用 Cook、Stage、Archive、日志和验证目录。默认引擎为 `E:/UE5/UE_5.8`，可用 `-EngineRoot` 指定。

```powershell
pwsh -NoProfile -File Scripts/Package_PC_SizeOptimized.ps1 -PlanOnly
pwsh -NoProfile -File Scripts/Package_PC_SizeOptimized.ps1 -Mode Baseline
pwsh -NoProfile -File Scripts/Package_PC_SizeOptimized.ps1 -Mode Optimized
```

`-PlanOnly` 只解析路径、检查原生材质入口并保存打包参数，不启动引擎，不表示 Cook 或打包成功。每次使用新的 RunName；失败产物不会自动删除。打包仍要求至少 6 GiB 可用物理内存、8 GiB 可用提交空间，且没有其他 Unreal 工作负载。不要关闭资源保护来强行启动。

输出位于 `Builds/PCSize-时间戳/{Baseline,Optimized}`。每个目录包含 Windows 文件夹、完整 ZIP 及 SHA-256。报告位于 `Saved/Validation/PCSize/PCSize-时间戳`。

## 优化内容

- 将 MainGame/Materials 的整目录强制 Cook 改为扫描 Source、Plugins 原生代码中的完整材质路径，并逐项传入 `-PACKAGE=`。其他目录规则保留，扫描结果缺失对应源资产时终止。
- 沿用安卓已验证的 HDRIBackdrop 内容子目录排除规则；若出现被引用资源禁止 Cook 的错误，必须处理依赖，不能忽略错误。
- Windows 场景/武器颜色和法线 2048、对应遮罩组 1024、角色颜色 4096、角色法线 2048、天空 2048。UI 保留引擎原有 16384 上限和 NoMipmaps，关闭 UI 组 RDO；Lightmap 与原始资产不改。
- Final 纹理编码使用 RDO Lambda 10，法线/遮罩/天空组使用更保守的 TLCA_Lowest。资源自身设置可能覆盖组设置，必须检查实际 Cook 结果。
- Kraken Shipping 压缩等级 6，中英文使用 EFIGSCJK 国际化数据集。
- 保留 DX11/SM5、DX12/SM6、Nanite、Lumen、运行库安装程序和桌面模型 LOD。

`-CustomConfig=PCSize` 同时传给 UAT 和 Cooker。本机 UE 5.8 的 UBT ConfigCache 从进程命令行解析 CustomConfig，UAT 据此读取 Stage/容器压缩配置；Cooker 据此读取资源和 Windows 纹理组配置。没有将 CustomConfig 传给 UBT 编译目标，因此不要求已安装引擎生成独立的引擎构建。这里的自定义设置仅用于构建/Cook，不依赖发布程序启动时加载 Custom 目录；源数据和默认运行时渲染配置不变。

## 验证和统计

沿用主流程脚本已有的地图、字体、工具、房间、后处理和结局依赖检查，增加本次原生材质入口与 HDRI 排除检查，验证结局视频与源文件一致。ZIP 逐文件 SHA-256 回读比对，并拒绝分发目录中的 PDB、OBJ、LIB、EXP。

`report_pc_package.py` 读取实际发布 `.utoc` 压缩块表。共享块按各资源逻辑字节覆盖比例分摊，每个压缩块只计一次；单资源分摊是归因值，不是该资源单独压缩后的长度。报告分别列出 ZIP 实际大小、发布目录实际大小、容器实际大小和唯一压缩块总量，元数据及对齐填充不会误算为资源压缩块。当前支持 UE TOC 版本 3–8，不支持的版本或损坏结构直接报错。

`Both` 模式会在优化报告中附带基准对比和移除资源列表。结构报告的 `gameplay_review` 保持 pending，运行和截图验证单独记录在 `runtime-validation.json`；不能仅凭包体结构验证认定画面、加载性能或完整通关已验证。

发布前使用相同画质、分辨率与相机位置比较：主菜单和中文、近景收集物、办公室、卡牌房、镜子房、矿物发光、镐子/炸弹/钻机、博物馆及完整结局。优先回调具体资产的分辨率/RDO，不一次性关闭渲染功能。

## 2026-10-24 最新藏品与肉鸽版

当前普通版 ZIP 为 1,904,661,599 字节。应用 PCSize 后，包含肉鸽扇翅更新的最终 ZIP 为 **1,097,293,012 字节**，减少 **42.39%**；解压后为 1,253,703,137 字节。

交付文件：`Builds/PCSize-20261024-PigeonFinal/Optimized/BrainMining-Windows.zip`。

SHA-256：`a3f56b064f89c53bddd425f6e2bed40056c6ed6e8f3e95cada62560f7ae496ef`。

本次先基于 `3cd9427` 完成独立 PCSize Cook 和发布验证，再编译肉鸽羽翼、悬停和拾取描边的原生代码修改。由于重新 Stage 时临时 Zen oplog 已不可用，最终分发沿用已验证的优化资源并替换新编译的 Shipping 主程序；逐文件散列确认只有该主程序变化，所有资源容器、Shader、DLL 均与本次 PCSize Cook 的成品一致。最终 ZIP 回读和 442 项必需资源检查通过。

可追溯报告：`Saved/Validation/PCSize/PCSize-20261024-Collection24`（Cook、DX11 电脑房、目录和旧存档兼容检查）以及 `Saved/Validation/PCSize/PCSize-20261024-PigeonFinal`（最终包、肉鸽和流程回归）。这是检查点与拾取/存档路径的验证，不代表连续完整通关或跨显卡测试。

实际 Cook 数据示例：出口标牌、窗户颜色纹理各从约 10.67 MiB 降至 2.67 MiB；标题 UI 仍为 2,458,115 字节。未改变默认项目配置，后续出小包仍需显式启用 PCSize。

## 报告脚本测试

```powershell
python -m unittest discover -s Scripts/tests -p test_pc_package_report.py
```

测试涵盖共享压缩块去重、跨块分摊、损坏 TOC、重复清单、ZIP 回读与覆盖保护、动态资源缺失和调试符号拦截。实包体积与画面验证另行执行。
