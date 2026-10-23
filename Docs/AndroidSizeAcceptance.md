# Android 体积优化包验收

记录时间：2026-10-22（北京时间）。离线检查于 13:00 完成；13:04 起执行首轮真机检查。用户已决定暂缓缓存清理。

## 当前结论

离线检查与首轮真机冒烟检查通过，但完整画质、玩法和性能验收尚未完成。r6 已安装在小米 23013RK75C 上，并停留在开始菜单。此次未改源码、配置或 APK，也未清除手机应用数据。桌面渲染没有重试，资源保护阈值未变。

## 首轮真机结果

- 设备：小米 23013RK75C，Android 15 / API 35，SM8475，支持 arm64-v8a；内存页大小 4096 字节。截图分辨率为横屏 2400×1080。本机不构成 16 KB 页运行验证。
- 安装前未发现同包名应用，因此本次实际是首次安装，无法验收旧存档迁移。使用 `adb install -r` 安装成功，用时约 30.20 秒，没有执行构建附带的卸载脚本。
- 设备内 `base.apk` 的 SHA-256 与已验证 r6 完全一致。
- 两次冷启动成功，开始菜单与起始房间可见。Android Activity 的 TotalTime 分别为 1603 ms 和 1152 ms；这不是游戏加载完成或可操作时间。
- 实际触控通过：开始游戏、右侧视角拖动、左侧摇杆移动、暂停、打开藏品页、返回游戏。开始/退出、藏品及“尚未发现”等采样中文正常，未覆盖所有文本。
- 发送过挖掘手势，但截图没有足够证据证明地形被挖开，挖掘不计为通过。没有完成飞行、结局或全部关卡流程。
- 首次进程的检查窗口约 15.6 分钟，采样中 PID 保持一致，应用退出信息未发现异常退出记录，应用进程日志未匹配到 FATAL EXCEPTION / Fatal signal / ANR in / OutOfMemoryError。随后主动停止一次应用以验证第二次冷启动，没有清除数据。此记录不代表持续帧率或长期稳定性测试。
- 四次 PSS 快照约 1.15–1.22 GiB；没有同机基准包对照，不能据此宣称运行内存下降或性能提升。
- 首次进程日志出现 392 条 Adreno shader compilation failed 和 17 条 failed to link shaders 消息；第二次启动已采集日志中未再出现。另有图形缓冲格式错误、configrules.bin.png 缺失和设备服务告警。起始房间仍可渲染，原因与后续场景影响尚未确定，不能直接归因为优化，也不能将它们标成已解决。

证据目录：`Saved/Validation/SizeOptimization/AndroidDevice-20261022-130425/`。总结果为 `device_acceptance_result.json`，包含设备、安装、两次启动、日志统计、内存样本及未验收项。截图 `01_launch.png` 至 `12_second_launch.png` 保留实际操作过程。

验收包：`Builds/MobileVerticalSlice/AndroidSizeOptimized-20261021-r6/BrainMining-Android-Shipping-arm64.apk`。

- 文件大小：949,506,648 字节（905.52 MiB），比 v1.12 基准小 38.43%。
- APK SHA-256：`9CE9D5E63F10517847F51050710D73EE539B76DE0834389A512694C5F73FC869`。
- 包名：`com.YourCompany.BrainMining`。
- 启动 Activity：`com.epicgames.unreal.SplashActivity`。
- 最低 API 26，目标 API 36；原生 ABI 为 `arm64-v8a`。
- 基准和 r6 都是 versionCode 13、versionName 1.12，不能仅凭界面版本号区别两者。以安装来源和 APK 哈希识别。

## 已完成的离线检查

| 检查 | 结果 | 限制 |
|---|---|---|
| aapt 读取 APK 清单 | 通过 | 不等同于设备实际支持全部图形特性 |
| apksigner 校验 | 通过，v2 签名 | 当前证书为 Android Debug |
| 与基准包比较包名和证书 SHA-256 | 一致 | 设备上的实际安装包仍需单独核实 |
| zipalign `-c -P 16 -v 4` | 通过 | ZIP 对齐不能代替原生库 ELF 检查 |
| 4 个原生库 ELF LOAD 段对齐 | 全部满足至少 16 KB 对齐，偏移与虚拟地址同余 | 仅静态检查，未在 16 KB 页设备执行 |
| r6 文件哈希 | 与已验证构建一致 | 画面和输入需真机检查 |

工具：本机 Android SDK Build Tools 36.1.0。证据：`Saved/Validation/SizeOptimization/AndroidReadiness-20261022-130037/`，包含 `result.json`、清单、签名、ZIP 对齐和 `elf_alignment.json`。原生库为 libUnreal、libpsoservice、libswappy、libvkquality。

## 可复用的设备验收顺序

1. 使用 USB 连接测试手机，开启 USB 调试并允许本机授权；确认 `adb devices -l` 中状态为 `device`。有多个设备时明确使用目标序列号。
2. 记录手机型号、系统/API、ABI、页大小、可用存储和显示分辨率，确认 API 至少 26 且支持 arm64-v8a。记录设备上现有包的版本；离线基准相同并不代表手机上现有包一定相同。
3. 优先保留数据覆盖安装 r6。不要直接执行构建附带的 `Install_*.bat`：实际脚本会先卸载已有应用并删除游戏目录。
4. 若覆盖安装提示证书不匹配或版本冲突，停止并保留错误，不自动卸载或清除存档。
5. 启动游戏，检查进程与前台界面，记录启动错误和游戏进程日志。保留首次启动与后续启动的测量区别。
6. 完成下表中的场景与玩法验收，截图并记录设备条件及问题。安装成功不作为运行或画面通过。

以下命令中的“设备序列号”必须替换为真实目标；本轮已按此方式完成安装和启动，保留命令用于复现：

```powershell
adb -s 设备序列号 install -r "E:\UE5\UEProjects\BrainMining\Builds\MobileVerticalSlice\AndroidSizeOptimized-20261021-r6\BrainMining-Android-Shipping-arm64.apk"
adb -s 设备序列号 shell am start -W -n com.YourCompany.BrainMining/com.epicgames.unreal.SplashActivity
```

`adb install -r` 的用途为重装并保留应用数据；应用自身的存档兼容性仍需实际验证。

## 真机验收记录

| 项目 | 观察内容 | 当前状态 |
|---|---|---|
| 安装与启动 | 安装成功，进入菜单及主场景，无崩溃/ANR | 首轮通过：两次启动、起始房间及检查窗口内未发现崩溃；非全关卡稳定性验收 |
| UI 与中文字体 | 菜单、卡牌、按钮、文本无缺字或丢失，触控区域正确 | 部分通过：开始/暂停/藏品页和采样中文；卡牌及完整文本待验 |
| 主场景及六个原生动态材质 | 地面、脚印、矿物提示、路线和采集物材质正常，无默认灰材质 | 待执行 |
| Office / Museum | 近景文字与纹理仍可辨认，正常游玩距离画面可接受 | 待执行 |
| 细胞 Android LOD | 轮廓、孔洞、近景和相机运动无明显破损；碰撞与交互正常 | 待执行 |
| 天空与结局场景 | HDRI 排除未造成错误天空、光照或资源缺失；结局流程可完成 | 待执行 |
| 移动、挖掘、飞行及触控 | 输入、视角和关键反馈正常，必要时核查已失败的喷气点火震动用例 | 移动、视角、菜单触控通过；挖掘未取得确证，飞行/震动待验 |
| 保存与再次启动 | 已有进度可读取，新进度可保存并重新加载 | 再次启动通过；存档读写及迁移待验 |
| 性能对比 | 相同设备、设置、镜头/路线与温度条件记录帧时间和内存；不从包大小推断性能收益 | 仅记录 PSS 快照，未作帧时间或同机基准比较 |

逻辑回归既有结果为 19 项通过、1 项失败（喷气点火震动反馈）。不能将本轮静态检查写成该失败已解决。

## 正式发布前的单独事项

当前 APK 的 Android 清单包含 `debuggable`，使用 Android Debug 证书；基准包也是如此。这不阻止本轮测试，但它仍是验收候选，不能直接标记为正式发布完成。后续正式发布需配置发布签名、关闭调试标记、按发布渠道要求调整版本，并重新验收最终产物。不要在文档或日志中记录签名私钥或口令。

官方参考：
- https://developer.android.com/tools/adb
- https://developer.android.com/tools/apksigner
- https://developer.android.com/guide/practices/page-sizes
