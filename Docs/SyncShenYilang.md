# 本地合并 Shen-Yilang 到 CHENHongfan

这次把队友已经上传的 `Shen-Yilang` 内容合入本地 `CHENHongfan`，保留此前的镐子、碎屑、GUI 和角色操作。没有执行 Push，没有修改队友分支。

## 为什么需要手工处理

双方修改了共用的模块依赖、空间分块代码和编辑器菜单，不能整份只选某一方。两个 Build.cs 取双方所需依赖；几何代码同时保留原有增量网格和队友的外部矿区实现；编辑器同时注册原有矿道工具、外部矿区生成器和字幕工具。

原有 `SplitSpatial` 和默认 `SetChunkedMesh` 仍遵守 128 块限制。外部矿区明确使用 `SplitExternalSpatial` / `SetChunkedMesh(..., true, true)`，支持其较大的网格。没有把所有旧矿区的容量规则一起放宽。

冲突关卡 `Lvl_MiningSpatial_Demo` 保留 CHENHongfan 中来自 Huang-Weiyu 的较新版本。队友的文思、外部矿区和弹窗玩法位于各自独立目录，正常保留。

## 如何查看新增内容

- 文思玩法：`/Game/WenSi/Maps/Lvl_WenSi`、`Lvl_WenSi_ScreenBridge`。
- 外部矿区：`/Game/ExternalMining/Maps/Lvl_ExternalMining_Demo`。
- 弹窗玩法：`/Game/PopupClose/Maps/Lvl_PopupClose`。
- 原有镐子和 GUI 演示仍在 `/Game/MiningPrototype/Maps/Lvl_MiningLab`，默认启动关卡未改变。

合入独立关卡让双方内容存在于同一个项目中；不同玩法关卡各自的 GameMode 和角色设置仍按其原来配置运行。

## 本地保留与检查

合并前的提交保留为本地分支 `codex/pre-shen-merge-20261010`，没有切换当前工作分支，也没有发布这个备份分支。

合并前的冲突文件、索引和 65 个原有功能文件的 SHA-256 清单保存在 `Saved/MergeBackups/ShenYilang-20261010`。暂存记录保留；近期的 46 个资源已核对，与当前分支对应文件相同。

验证结果（UE 5.8.3）：

- `BrainMiningEditor Win64 Development` 编译通过。
- 64 项 BrainMining 自动测试通过，0 失败；其中 3 项带有预期的容量限制或编辑器提示警告。
- 6 个双方关卡资源成功加载，包括原有 MiningLab、Spatial Demo，以及文思、外部矿区和弹窗关卡。
- 实际 PIE 运行中的 75 项 GUI/操作检查通过：Tab 按住和松开、鼠标与视角交接、G/V 使用状态、三段升级、上锁按钮、Q 弹反馈、镐子动画和碎屑触发。金币和价格区域仍为空。
- 合并后重新计算上述 65 个文件的 SHA-256，全部与合并前一致；原有 Spatial Demo 也保留原版本。现有 stash 未恢复、删除或替换。

编译日志保存在上述备份目录；自动测试和 GUI 实测报告保存在 `Saved/Validation/ShenMerge`。GUI 测试首次启动遇到测试回调重入和预览尚未完全初始化的问题；一次性测试入口增加重入保护并等待预览初始化后，所有检查通过。项目原有 GUI 测试脚本没有被改写。

之后继续在 CHENHongfan 编辑，UE 保存后执行本地 Commit；需要上传自己分支时再自行 Push origin。
