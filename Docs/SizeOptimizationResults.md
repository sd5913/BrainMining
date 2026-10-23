# 项目体积优化实施与验证报告

记录日期：2026-10-22（北京时间）。审计基准来自 2026-10-21。项目：`E:/UE5/UEProjects/BrainMining`。

## 1. 已经取得的结果

最终候选包：`Builds/MobileVerticalSlice/AndroidSizeOptimized-20261021-r6/BrainMining-Android-Shipping-arm64.apk`。

| 指标 | 基准 v1.12 | 优化候选 r6 | 变化 |
|---|---:|---:|---:|
| APK 字节数 | 1,542,144,800 | 949,506,648 | 减少 592,638,152 |
| APK MiB | 1470.70 | 905.52 | 减少 565.18 |
| 相对降幅 | — | — | 38.43% |
| 细胞资源压缩体积 MiB | 54.46 | 31.01 | 减少 23.45 |

APK SHA-256：`9CE9D5E63F10517847F51050710D73EE539B76DE0834389A512694C5F73FC869`。

这里测量的是 APK 实际文件长度。资源表使用 IoStore 压缩块长度，不拿源 uasset 大小代替下载体积，也不将这些数字直接解释为手机运行内存或帧率提升。

**用户已手动完成全部已列历史构建的清理。** 2026-10-22 01:40 再次复核确认冻结清单内 37 个构建目录均已不存在，原文件逻辑大小合计 57.90 GiB；49 个缓存目录仍在，文件大小与基准一致，共 7.13 GiB。E 盘可用空间现为 141.02 GiB，较删除前最近观测增加 72.56 GiB，但整卷增量不能全部归因于清单内删除。原定保留的 `AndroidMainGameV15` 仍在原位置缺失；r6 APK 与其余已检查保留产物正常。APK 缩小与磁盘空间回收是两项独立结果。

## 2. 实际资源变化

单位：MiB；按同一套容器压缩块统计口径对比。

| 资源目录 | 优化前 | 优化后 | 减少 |
|---|---:|---:|---:|
| Game/Office | 314.91 | 124.06 | 190.85 |
| Game/Museum | 165.80 | 60.41 | 105.39 |
| EnginePlugin/HDRIBackdrop | 100.06 | 0.00 | 100.06 |
| Game/Fab | 124.37 | 74.66 | 49.70 |
| Game/EndingEmergence | 98.53 | 68.50 | 30.03 |
| Game/Planet_Project | 33.65 | 5.76 | 27.90 |
| Game/Debris | 34.10 | 8.83 | 25.27 |
| Game/MainGame | 86.90 | 61.83 | 25.07 |
| Game/Fonts | 26.39 | 26.39 | 0.00 |
| Game/JapaneseCity | 75.01 | 75.01 | 0.00 |
| Game/BloodCirculation | 83.16 | 83.16 | 0.00 |
| Game/Countryside_Interior | 48.58 | 48.58 | 0.00 |
| Game/PopupClose | 22.40 | 22.40 | 0.00 |
| Game/Drill | 23.30 | 23.30 | 0.00 |

主要收益来自 Office/Museum 贴图、默认 HDRI、Fab 素材及地球/碎块贴图。字体体积保持不变；JapaneseCity、BloodCirculation、Countryside_Interior 等目录本轮基本没有收益，不能把一个全局纹理上限理解成所有纹理都会等比例缩小。

## 3. 已落地修改

### Android 纹理预算

`Config/DefaultDeviceProfiles.ini` 的 Android 基础配置加入纹理组上限：场景和武器颜色/法线为 2048；人物颜色为 2048，人物法线/高光为 1024；天空为 1024。UI、Lightmap 和桌面纹理配置未改，原始导入像素保留。关闭 mip 生成等特殊纹理设置可能绕过组上限，需要以实包结果为准。

### 细胞网格

原始 LOD0 保留 1,355,517 个三角形，新增 LOD1 为 100,890 个三角形，Android MinLOD 设为 1。桌面默认最小 LOD 仍为 0。Cook 启用 `r.StaticMesh.StripMinLodDataDuringCooking=1`，剥离目标平台最小渲染 LOD 以下的数据。

渲染面数约减少 92.56%，但整个资源压缩体积约减少 43.06%。本次保留碰撞设置和其他资产数据，不能按三角形比例预测整个资源的体积。原始资产备份：`Saved/Backups/SizeOptimization20261021/cell.uasset`。

### 冗余 Cook 输入和动态材质

`Config/Custom/AndroidSize/DefaultGame.ini` 由优化打包脚本通过 `-CustomConfig=AndroidSize` 启用，撤销 MainGame/Materials 整目录强制打包，并排除默认 HDRIBackdrop 的四个内容子目录。实包已确认默认 HDRI 和旧 T_HeavenClouds 不再出现。

动态材质不能只依赖场景引用。准备脚本生成了包含六个材质的 PrimaryAssetLabel；由于单靠标签实测仍遗漏三个材质，最终打包脚本每次扫描 Source 与 Plugins 的原生路径，并通过 `-PACKAGE=` 明确加入 Cook 入口。包括脚印、埋藏矿物发光、收集物材质及 EchoPassage 的蓝紫材质。最终六项均已在容器中逐项核对。

### 防止缓存和重复分发产物继续膨胀

`verify_gamepad.ps1` 使用 `-NoAssetRegistryCache`，保留测试隔离目录，避免每次再写约 152 MiB 的索引缓存；同时限制后台资产编译并发，适应当前机器内存条件。新的优化打包脚本不生成额外 Share APK 副本。

## 4. 为什么这次必须检查 APK 内部

本次实测发现 UE 5.8 Android 部署代码读取标准 `Saved/StagedBuilds/Android.obb`。只指定自定义 stagingdirectory，可能出现 Cook/Stage 成功、最终 APK 却仍含旧 OBB 的情况。最终脚本使用标准暂存位置，同时保留每轮独立 Cook 输出、归档和报告。

验证脚本逐一比对 APK 内五个 pak/utoc/ucas 文件与本轮暂存文件的 SHA-256，再检查两张关键地图、六个动态材质和冗余资源排除结果。体积对比还检查基准中的字体/UI 资源是否仍在。r6 所有上述检查通过。

r2 和 r5 是诊断包，已放置 `VALIDATION_FAILED.txt`，不要用于交付。r2 含旧 OBB，r5 缺少三个动态材质。应使用 r6；r6 仍需完成手机实机体验验收。

## 5. 测试与限制

2026-10-22 真机补充：用户连接小米 23013RK75C（Android 15 / API 35，arm64，4 KB 页）后，r6 已成功安装，设备内 APK 哈希与候选一致。两次冷启动、起始房间渲染、开始按钮、视角拖动、摇杆移动、暂停和藏品页返回均通过首轮检查。首次进程约 15.6 分钟检查窗口内未发现崩溃，随后主动停止并重启一次；没有清除数据。安装前无同包名应用，旧存档兼容性仍未测试。PSS 快照约 1.15–1.22 GiB，未做同机性能对照。首次进程有 392 条着色器编译失败及 17 条链接失败日志，第二次启动已采集日志中未复现，原因及其他场景影响待查。Office、Museum、细胞 LOD、结局与完整玩法仍未验收。详见 `Docs/AndroidSizeAcceptance.md` 和 `Saved/Validation/SizeOptimization/AndroidDevice-20261022-130425/device_acceptance_result.json`。以下“未连接设备”等描述保留为早先检查的历史状态，以本段为准。

2026-10-22 13:00 补充：用户决定暂缓缓存清理，进入 Android 包验收。使用 SDK Build Tools 36.1.0 完成 r6 的清单读取、v2 签名及 ZIP 对齐校验，均通过；4 个原生库 ELF LOAD 段满足至少 16 KB 静态对齐。基准与 r6 包名、签名证书、versionCode 13 和 versionName 1.12 相同。当前包及基准均为 Android Debug 签名并启用 debuggable，正式发布准备尚未完成。未发现连接的 Android 设备，可用物理内存约 4.0 GiB，未重复启动此前因资源不足失败的渲染。真机验收步骤见 `Docs/AndroidSizeAcceptance.md`，证据见 `Saved/Validation/SizeOptimization/AndroidReadiness-20261022-130037/result.json`。以下运行与画面限制仍然有效。

- 最终完整 Cook：0 个错误、2 个警告。警告为 MCP 插件提示和 Echo 通道既有 FText 初始化提示；未屏蔽资源缺失或 NeverCook 依赖错误。
- 容器及必需资源验证：通过；基准字体/UI 对比未发现缺项。
- 资源保护脚本：11 项回归检查通过；另用注入 WMI 失败验证可选工作集整理失败不再中断任务，并确认低内存时测试子进程确实退出。
- 独立回归：MainGameAssets、MainGameSurfaces、AtomicUpgrade 三项通过；Gamepad.Haptics.LifecycleAndEvents 失败，断言为“Real jet ignition emits its cue”。本次未改相应 C++ 代码，不能据此宣称该失败是优化导致，也没有把它当作全绿测试。
- 第二批独立逻辑回归 16 项全部通过：Flight 的 11 项、手柄上下文/死区/震动模式/偏好持久化的 4 项，以及 Mobile.EndingExcavation。两个完整测试批次合计 19 项通过、1 项失败。证据见 `Saved/Validation/SizeOptimization/regression_results.json`。
- 两个完成的测试隔离目录均未生成 CachedAssetRegistry 索引二进制，新的每轮约 152 MiB 缓存增长已被阻止。
- 第一轮合并回归在内存不足时被保护脚本终止，属于未完成的测试，不算通过。已完成的 Flight 等用例在原始日志保留。
- 关键场景画面：主地图预览和独立细胞 LOD 对照均因物理内存不足被保护脚本中止，没有生成有效验收截图。网格简化后的近景外观、材质细节及各场景画质仍待验收，不能把资源统计通过等同于视觉质量通过。
- 补充离屏验证：省去编辑器界面和 PIE 后重新尝试场景捕获。首次使用通用缓存的运行在 240 秒限时处结束；复用项目缓存的第二次运行在约 536 秒、可用物理内存降至 1.29 GiB 时被保护脚本中止。两次都停在基础着色器准备阶段，没有生成截图；保护阈值未降低，相关验证进程已退出。证据：`Saved/Validation/SizeOptimization/offscreen_attempts.json`。仍需释放足够内存后完成视觉验收。
- 未连接 Android 设备，尚未验证真机安装、启动、触控、各场景画质和帧率。桌面预览不能代替 Android 画质验收。

## 6. 手动清理后的复核结果

### 缓存清理尝试：2026-10-22 12:56

用户明确要求助手清理剩余 49 个缓存目录。专门针对这 7.13 GiB 缓存的预检全部通过，包括路径边界、重解析点、文件类型、Git 跟踪状态与文件大小；已记录 205 个报告文件的 SHA-256，r6 APK 校验通过。

随后删除命令再次被工具策略拒绝，返回 `blocked by policy`，未提供具体原因；命令未启动，删除 0 字节，49 个缓存目录仍在。本次不缺用户授权，未改换方式绕过限制。证据：`Saved/Validation/SizeOptimization/cache_cleanup_preflight.json` 与 `cache_cleanup_blocked.json`。

### 最新状态：2026-10-22 01:40

用户再次报告删除后，检查发现清单剩余状态未变：49 个缓存目录仍在，各目录文件大小与基准一致。E 盘可用空间增加 1,448,411,136 字节（1.35 GiB），达到 151,419,670,528 字节（141.02 GiB）；此次增量不能归因于这些尚在的缓存。49 个测试运行目录及上次记录的报告路径仍在，未比较报告文件内容。r6 APK 大小及 SHA-256 与基准一致。V15 保留项仍缺失。本次实际删除对象尚不能由冻结清单核对确定。

最新证据：`Saved/Validation/SizeOptimization/storage_cleanup_manual_verification_r3.json`。

### 上次状态：2026-10-22 01:28

用户再次报告删除后复核：37 个清单内历史构建已全部不存在，原文件逻辑大小共 62,167,505,430 字节（57.90 GiB）。此前剩余四个构建也已删除。49 个缓存仍在且大小与基准一致，共 7,660,306,884 字节（7.13 GiB），每个对应测试运行目录均能找到报告文件。

E 盘可用空间为 149,971,259,392 字节（139.67 GiB），较上次观测增加 5,760,360,448 字节（5.36 GiB），较最初观测增加 76,465,528,832 字节（71.21 GiB）；仅作为整卷观测值。r6 APK 仍为 949,506,648 字节，SHA-256 与基准一致。V15 保留项缺失尚未解决，其余已检查保留项均在。此次仅复核，未继续删除文件。

最新证据：`Saved/Validation/SizeOptimization/storage_cleanup_manual_verification_r2.json`。以下为审批及首次复核历史。

### 审批与首次复核历史

逐目录可读表格见 `Docs/SizeCleanupReview.md`；它与冻结的 JSON 清单使用相同的 86 个候选目录，没有新增删除范围。

只读清单：`Saved/Validation/SizeOptimization/storage_plan.json`，86 个候选目录，合计 69,827,812,314 字节（约 65.03 GiB）。其中历史构建 37 个目录、约 57.90 GiB；测试索引缓存 49 个目录、约 7.13 GiB。交付前复核全部目录仍在项目内，字节数与清单一致，结果见 `storage_plan_validation.json`。清单保留 v1.11、v1.12 及早期 Android/V15/V29/TapPlay 里程碑；实际执行前仍需复核候选目录未发生用途变化。

用户已于 2026-10-22 明确确认上述 86 个目录。再次复核所有路径与字节数，排除重解析点和 Git 跟踪文件，并检查保留构建、r6 APK 与细胞原始备份，全部通过。证据见 `Saved/Validation/SizeOptimization/storage_cleanup_preflight.json`。

自动审批随后再次拒绝删除命令，返回 `blocked by policy`，未提供更具体原因；命令未启动。该次拦截记录见 `Saved/Validation/SizeOptimization/storage_cleanup_blocked.json`。之后用户自行删除，助手仅执行只读复核，没有继续删除文件。

| 2026-10-22 01:18 复核项 | 结果 |
|---|---|
| 清单内已不存在的历史构建 | 33 个，清理前逻辑大小 57,218,356,166 字节 / 53.29 GiB |
| 剩余历史构建 | 4 个，4,949,149,264 字节 / 4.61 GiB |
| 剩余测试缓存 | 49 个，7,660,306,884 字节 / 7.13 GiB |
| 剩余清单合计 | 53 个，12,609,456,148 字节 / 11.74 GiB |
| E 盘可用空间 | 144,210,898,944 字节 / 134.31 GiB |
| 与删除前最近观测相比 | 增加 70,705,168,384 字节 / 65.85 GiB；非清单专属释放量 |
| 优化 r6 APK | 949,506,648 字节，SHA-256 与本报告基准一致 |

剩余四个构建是 `AndroidStandaloneApk`、`AndroidMainGameV1`、`AndroidControlsV3` 和 `AndroidControlsV2`。49 个缓存的测试运行目录均仍存在，均能找到报告文件；未对报告内容进行逐字节比较。

**保留项异常：** `Builds/MobileVerticalSlice/AndroidMainGameV15` 在原位置缺失，回收站中未找到匹配项，尚未恢复。其余五个原有保留构建、r6 APK 和细胞原始备份均存在。整卷可用空间还受清单外删除、回收站及其他进程写入影响，不将 65.85 GiB 全部记为本清单的收益。

完整复核证据见 `Saved/Validation/SizeOptimization/storage_cleanup_manual_verification.json`；V15 查找结果见 `storage_cleanup_v15_recovery_check.json`。冻结清单 SHA-256 未变。源码、美术源文件、恢复备份、Git/LFS 和共享引擎仍不在清理范围内。

## 7. 后续仍值得分析的部分

1. Office/Museum 优化后仍合计约 184.47 MiB，下一步应按实际镜头距离逐张判断能否降到 1024，不宜直接对整个项目继续降低质量。
2. BloodCirculation、JapaneseCity、Countryside_Interior 的总量基本不变，需核对各纹理的 mip、纹理组及压缩格式，而不是假设上限没有配置成功。
3. 主地图约 52.68 MiB，已成为最大的单个游戏资源；进一步优化涉及地图序列化和场景结构，需要独立评估玩法与编辑流程。
4. 细胞资源仍约 31.01 MiB，需单独拆分碰撞、渲染和其他序列化数据的贡献，再决定是否做碰撞代理或专用移动资产。
5. 字体约 26.39 MiB。只有建立完整中文文本覆盖清单后才适合做子集化，否则可能出现缺字。
6. Git LFS 预演仅约 2.3 GB 候选，优先级低于历史构建/测试缓存；本轮没有执行 prune 或改写历史。

## 8. 复现与回滚

打包入口：`pwsh -NoProfile -File Scripts/Package_Android_SizeOptimized.ps1`。每次要求新 RunName；它复用已编译 Shipping 二进制，因此改动 C++ 后必须先重新编译。输出默认位于 `Builds/MobileVerticalSlice/AndroidSizeOptimized-*`。

验证证据目录：`Saved/Validation/SizeOptimization/AndroidSizeOptimized-20261021-r6`，包含 `request.json`、`result.json`、`package_integrity.json`、`comparison.json`、`android_iostore.csv`、`asset_comparison.csv`、UAT 日志和资源采样。

回滚步骤见 `Docs/SizeOptimization.md`。当前修改未提交 Git，没有创建分支。用户已手动清理部分历史构建；目录清理不由配置或资产回滚步骤恢复，V15 保留项缺失需单独处理。
