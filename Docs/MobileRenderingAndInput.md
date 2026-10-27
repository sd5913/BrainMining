# 移动端水面、结局与触控

## 水面

移动端及 `-MobileVerticalSlice` 预览使用 `/Game/Mobile/Water` 下的材质。PC 继续使用原有 SingleLayerWater 材质。

`Scripts/create_mobile_water_materials.py` 从天堂水面复制法线、流动和踩水波纹图，移除 SingleLayerWater 输出，创建普通半透明 Default Lit 水面。移动端材质启用双面和完整浮点精度，并分别提供水池、外流水、卡牌房洪水实例。该目录必须随包 Cook。

同一脚本还生成纯色诊断材质。它仅由 `-MiningQA` 下的 `water_fixture` 命令用于确认水面模型的可见性。卡牌房夹具必须先自然进入 Flooding 阶段，再冻结水位；普通游戏不会执行这条命令。

## 结局

移动端结局相机不强制 16:9，使用 MaintainYFOV 适配横屏。触控玩家默认启用舒适模式，暂停菜单的控制设置可以切换并保存该偏好。钻地阶段仍是自动推进镜头，摇杆提供加速和横向微调，屏幕提示说明这些操作。

## 触控

动作按钮首先命中，左下区域用于移动，其余可用区域允许开始转头。专门转头的手指优先且独占转向；没有这根手指时，挖矿和跳跃/喷气按钮允许边按边瞄准。交互、切换工具等按钮不附带转向。

滑动距离按界面短边归一化，转向幅度不再继承旧版鼠标输入倍率。灵敏度对应每个屏幕短边 `MobileLookSensitivity * 1440` 度。普通手势和按钮拖动分别有相当于 720 高度下 2、4 像素的启动阈值，开始拖动后不增加平滑延迟。

## 验证

1. 通过 `Scripts/ResourceGuard.ps1` 启动材质生成脚本，确认 `MOBILE_WATER_ASSETS_READY` 和输出资产齐全。
2. 使用 `Scripts/Build_Safe.ps1 -MaxParallelActions 1` 编译编辑器。保留资源保护阈值。
3. 运行 `BrainMining.Mobile.IconLayout`、`InputAndMenu`、`EndingProgression`、`GraphicsPolicy`、`GraphicsPersistence` 自动化测试。
4. 构建、验证并更新安装 Android APK，保留玩家数据。
5. 执行 `python Scripts/verify_mobile_repairs.py --serial <设备> --apk <APK> --output <新证据目录>`。脚本先验证已安装 APK 哈希，再使用隔离 QA 存档；退出时恢复正常启动。
6. 人工查看两处水面的超分开关截图、纯色诊断截图及钻地阶段画面。脚本通过不代替视觉验收和多指手感验收。

## 2026-10-25 验收记录

最终安装包为 `Builds/MobileVerticalSlice/AndroidSizeOptimized-20261025-MobileRepairs-04/WOoOW-Android-Shipping-arm64.apk`，SHA256 为 `1fb034fd6ad76e9945e3006d9891d80d41c5c834a8d270a77d1da675124eebb3`。已覆盖安装到 Redmi K60，保留玩家存档并恢复正常启动。

- 五项移动端逻辑测试通过，报告位于 `Saved/Validation/MobileRepairs-20261025/Tests-20261025-201305/Automation`。结局测试已按 10 月 23 日引入的细胞自动出口流程更新原有挖矿断言。
- 最终 Android 代码编译、APK 容器哈希、五个新水材质及两张地图的入包检查通过。
- 最终包真机自动化通过，证据位于 `Saved/Validation/MobileRepairs-20261025/device-03`。中心滑动实测转向约 12.66°；钻地阶段采集 19 个相机状态，最大翻滚约 6.98°，随后进入下一镜头。
- 已查看实际截图：天堂水面恢复波纹，卡牌房水面与水下仰视可见；超分开关均能显示；钻地画面覆盖手机宽屏；舒适镜头开关有效，说明文字自动换行，较低设置可通过滚动查看。
- 本次真机覆盖 K60 的 Vulkan 路径。其他机型和长时间多指操作的主观手感不在该自动化结论内。

最后两处设置文字排版修改已通过 Android 编译及真机验收；其后的 Win64 编辑器复编因可用物理内存低于脚本要求的 6 GB 而未启动，未降低保护阈值。上述编辑器逻辑测试发生在这两处排版修改之前。

打包脚本统一使用 `WOoOW` 目标名，并自动收集 `/Game/Mobile/Water` 的原生代码引用。本次 `MobileRepairs-03` 缓存复用尝试被资源校验判定失败，不作为交付；最终 `MobileRepairs-04` 使用完整资源烘焙，并通过全部校验。
