# BrainMining

UE 第一人称挖掘原型。仓库保存源码、资源、配置和编辑器脚本，不包含引擎、编译缓存或打包成品。

## 团队开发环境

- Windows 64 位；统一使用 Unreal Engine **5.8.2**。建仓机器的引擎 Changelist 为 `56702186`。
- Visual Studio 2022，安装“使用 C++ 的游戏开发”、MSVC C++ 工具链及适用于该引擎版本的 Windows SDK。
- 每位成员均需安装 Git 和 Git LFS，不要用下载 ZIP 代替首次克隆流程。
- 工程包含 `BrainMining` 运行时模块和 `BrainMiningEditor` 编辑器模块，首次使用需编译编辑器目标。

### 插件依赖

工程启用 `ModelingToolsEditorMode`、`StateTree`、`GameplayStateTree`、`AllToolsets` 和 `ModelContextProtocol`。
建仓机器的 UE 5.8.2 安装中，后两个插件均位于引擎 `Engine/Plugins/Experimental` 下，描述版本均为 `1.0`；`ModelContextProtocol` 的显示名称为 Unreal MCP。
这两个工具插件在工程描述中仅针对 Editor，不是打包后游戏的运行时依赖。

若提示插件缺失，先检查引擎版本和安装组件，不要直接删除 `.uproject` 的插件声明或上传整个引擎。
仓库没有附带引擎插件副本。新增第三方插件时应明确授权、版本和获取方式；无源码插件所需的 `Binaries` 不能一概忽略。

## 首次获取和启动

这是私有仓库。先由所有者在 GitHub 邀请协作者，接受邀请后在 PowerShell 中运行：

```powershell
git lfs install
git clone https://github.com/Seven39c5bb/BrainMining.git
Set-Location BrainMining
git lfs pull
git lfs fsck
```

关闭 Unreal Editor，按实际引擎路径设置 `$engineRoot`，编译编辑器目标：

```powershell
$engineRoot = 'E:\UE5\UE_5.8'
$projectPath = (Resolve-Path '.\BrainMining.uproject').Path
& "$engineRoot\Engine\Build\BatchFiles\Build.bat" BrainMiningEditor Win64 Development "-Project=$projectPath" -WaitMutex -NoHotReloadFromIDE
if ($LASTEXITCODE -ne 0) { throw 'Editor build failed; inspect the build output before opening the project.' }
& "$engineRoot\Engine\Binaries\Win64\UnrealEditor.exe" $projectPath
```

也可以右键工程文件生成 Visual Studio 工程文件，然后构建 `Development Editor / Win64`。生成的解决方案和 IDE 文件不提交。
默认地图为 `/Game/MiningPrototype/Maps/Lvl_MiningLab`，打开后点击 Play 验证。
首次编译、着色器编译和派生数据生成需要时间。仅做美术或蓝图且不准备安装编译环境的成员，需要团队另行提供与源码提交匹配的编辑器模块构建产物。

## 协作约定

1. `main` 保持可编译、可打开。功能修改使用短期分支，通过 Pull Request 合并；作者先确认编译及相关玩法验证结果。
2. 修改关卡、蓝图、材质等资产前，在团队沟通渠道认领具体文件。不同分支也不要同时修改同一个 `.uasset` 或 `.umap`。
3. 保存 UE 修改并提交本地工作，关闭编辑器后同步远程文件。不要在有未保存资产或未处理本地修改时直接拉取。
4. 小提交、早合并，不要强推共享 `main`，不要随意用“全部选择我方/对方”处理资产冲突。
5. 修改引擎版本、公共插件或全局配置前先沟通，不擅自升级团队开发环境。
6. 提交前检查 `git status` 和 `git diff --cached --stat`，确认没有密钥、本机工具配置、缓存或打包输出。

工作区干净时，创建功能分支的示例：

```powershell
git switch main
git pull --ff-only
git switch -c feature/mining-ui
```

完成验证后，只添加本次涉及的文件，提交、推送分支，在 GitHub 发起 PR。
PR 是团队约定；此文档不代表已经启用服务端分支保护或自动构建。

### LFS 和资源锁

`.uasset`、`.umap` 及选定的常见大型源素材格式已经配置 LFS。LFS 管理大文件，不会自动合并二进制资产，也不会自动锁定正在编辑的文件。
当前没有启用所有资源的 `lockable` 只读策略，避免团队尚未统一操作时阻止编辑器保存；默认先遵守文件认领规则。

团队也可使用显式 LFS 锁，路径相对于仓库根目录，例如：

```powershell
git lfs locks
git lfs lock "Content/MiningPrototype/Maps/Lvl_MiningLab.umap"
```

锁定成功后再编辑，完成提交、推送及团队交接后解锁：

```powershell
git lfs unlock "Content/MiningPrototype/Maps/Lvl_MiningLab.umap"
```

显式锁不是实时多人编辑，也不等于全员本地只读或服务端强制限制。所有成员仍需遵守规则；如需强制流程，先在团队机器上验证 `lockable` 和推送锁检查。
关注 GitHub 账户的 LFS 存储及下载用量，不要把资源指针当作完整资源或用普通 Git 绕过 LFS 上传大资源。

## 目录和提交范围

| 路径 | 规则 |
| --- | --- |
| `BrainMining.uproject` | 工程入口，提交 |
| `Source/` | C++ 及模块、目标构建规则，提交 |
| `Content/` | 资产和关卡，提交；UE 资产走 LFS |
| `Config/` | 默认配置，提交；检查敏感信息和机器路径 |
| `Scripts/` | 编辑器生成和检查脚本，提交；按用途人工执行，不批量运行生成脚本 |
| `Build/` | 审核保留构建资源；仅排除生成的 `FileOpenOrder` 日志目录 |
| `Binaries/`、`Intermediate/`、`DerivedDataCache/`、`Saved/` | 本机生成，不提交 |
| `Builds/` | 本项目打包输出，不提交 |
| `.codex/`、`.vscode/`、`.mcp.json`、`*.code-workspace` | 本机工具配置，不提交 |

忽略规则不会删除本地文件。打包成品及供非程序成员使用的编辑器构建产物需另行分发，并标明对应提交。
已有说明和脚本可能使用作者机器的绝对路径，运行前按自己的安装位置检查，尤其不要批量执行资产生成脚本。
没有为 UE 或第三方资产添加新的开源授权；公开仓库前必须另行审核素材、代码和插件的再分发权限。

## 功能说明

- [挖掘原型](README_MiningPrototype.md)
- [编辑式下降关卡](README_EditableDescent.md)
- [自动房间](README_AutoRooms.md)
- [空腔区域](README_HollowRegion.md)
- [空腔分支](README_HollowBranches.md)
- [挖掘设计工具](README_MiningDesigner.md)
- [玩家飞行](README_PlayerFlight.md)

团队接入验收：独立目录克隆、`git lfs fsck` 成功、编辑器目标编译成功、默认关卡打开并可 Play。每位成员应在自己的机器上执行；建仓不替代玩法验证。
