# 道具资源提交与拉取

本次缺材质的直接原因：提交 `5381db4` 含风格化钻机的模型、材质球和 Normal，但没有提交材质使用的 BaseColor、Roughness、Metallic 三张贴图。三个资产和相关新源码仍在 GitHub Desktop 的 main 暂存记录中，Pull 后尚未恢复。材质引用无法在工作目录中解析。

此外，BombMotion、拿起动画、粘性炸弹、倒计时灯光和准心切换代码也没有随资产提交。旧编辑器 DLL 暂时包含这些功能，源码重新编译后却会退回旧层级与行为。因此提交必须同时包含资源依赖和源码，不能靠当前机器的 Binaries 或缓存判断完整。

修复采取定向恢复暂存文件、三路合并源码。保留当前 SharedMining 角色重新调整的炸弹朝向，以及三个 100×100 准心；没有将旧暂存蓝图覆盖回来，也没有覆盖组员的升级、金币和交互实现。暂存记录保留，未整体 Pop。

发布前可在普通终端运行 `Scripts/check_tool_publish.ps1`（或 Python 版），核对 **HEAD 已提交的内容**，检查必要贴图、模型、材质、准心与源码是否完整。当前电脑的 pre-push 已在 Git LFS 上传前接入此检查，main 分支缺少这些文件时会阻止 Push。此本地 hook 不随 Git 克隆自动安装；其他成员可手动运行同一检查脚本。

在 UE Python 或命令行脚本模式中运行 `Scripts/verify_tool_asset_dependencies.py`，可以进一步检查资源依赖闭包、实际材质绑定、文件是否已加入 Git、以及 LFS 指针是否已下载成真实文件。资源需要先加入 Git 暂存区。检查报告在 `Saved/Validation/ToolDependencies`。

修改 UI 或道具组件时，先在 UE 保存对应蓝图和材质，再提交完整的资源、依赖及源码。若 Pull 前选择了 Stash，Pull 后还需要恢复暂存；出现蓝图二进制冲突时，先备份并核对组件参数，不要直接用旧蓝图覆盖新的图表逻辑。

修改前备份和用户参数快照在 `Saved/PullRepair/Before`、`Saved/PullRepair/live-before.json`。重新启动后的参数、材质及运行验证也保存在 `Saved/PullRepair`。
