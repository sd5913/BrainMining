# 盲人敲地：实现与协作指南

核对日期：2026-10-09。UE 5.8.2，单人本地原型。面向程序、美术技术协作者和后续 AI 编程助手。

建议按 **快速开始 → 当前规则 → 代码导航 → 运行流程 → 拼装接口 → 接手约定** 阅读。本文描述当前共享主角色接入版；历史反馈以现行实现为准。团队环境、Git LFS 和资产协作规则见 [项目 README](README.md)；收集系统见 [README_Collection.md](README_Collection.md)。

## 1. 快速开始

| 地图 | 内容 |
| --- | --- |
| `/Game/Passages/Echo/Maps/Lvl_EchoMazeToCards_P1` | 推荐：约 96 米轻迷宫，暖光出口切换到卡牌房 |
| `/Game/Passages/Echo/Maps/Lvl_EchoPassage_P1`（已删除） | 原短通道，约 52 米，末端显示完成提示 |
| `/Game/Rooms/CardDuel/Maps/Lvl_CardRoom_P1` | 单独试玩卡牌房，见 `README_CardRoom.md` |

这些地图的 GameMode Override 为 `MiningEncounterGameMode`，直接使用项目原有 `BP_FirstPersonCharacter`。WASD 移动、鼠标环视与普通跳跃沿用主角色；飞行在通道和牌室内临时禁用。

从入口向前走，地板坍塌后落地自动发出一次大范围回声。**点一下鼠标左键敲一下地面/近处墙面**，不会改为长按挖掘。R 清空回声和完成状态，返回坍塌前。Esc 按编辑器默认行为结束试玩。

先按项目 README 编译 `BrainMiningEditor / Win64 / Development`，在 UE 中打开上述地图。没有专用通道启动器，默认启动地图仍是 MiningLab。玩法间联结说明见 [README_CardRoom.md](README_CardRoom.md)。

## 2. 当前玩法规则

策划来源：《通路1-2：盲人敲地【P1】.xlsx》。按后续反馈保留纯黑世界、白圈涟漪以及由近及远显形的白色轮廓；又增加较长、难度轻的迷宫和真实卡牌房联结。

- 不敲击时环境为黑色。敲击点发出两道白色圆环，波前扫过后白色线框显现；亮度同时随距离和时间衰减，远处更淡，最后回到全黑。
- 默认半径 1550 cm；落地回声为 1.4 倍；传播速度 1000 cm/s；每个位置显形后 2.6 秒内消退；敲击间隔 0.65 秒；最多同时保留 4 道回声。
- 射线先检测视线前方 245 cm 内的表面；没命中时敲脚前地面，向下检测长度适配角色胶囊体。敲击动画开始 0.18 秒后发声波。
- 轮廓被实体墙遮挡。脚步不会自动照亮；没有追逐敌人、限时或生命消耗。
- 短通道：14 格、4 米格距、约 52 米中心线路程。
- 轻迷宫：25 格主路、约 96 米最短路线；两条各 8 米死路、一处重新汇入主路的回环；无四岔口，三组石柱作为回声地标。
- 通道末端可见暖光。联结地图通过短暂淡出切换到暖光牌室；牌室不采用全黑效果。

## 3. 关卡、参数与调节位置

停止 Play 后选择地图中的 `EchoPassageStage`（联结地图标签为 `EchoMaze_To_CardRoom`）：

| 属性 | 默认值 / 含义 |
| --- | --- |
| `EchoRadius` | 1550 cm，普通敲击影响半径；编辑范围 300–4000 |
| `EchoSpeed` | 1000 cm/s，传播速度；编辑范围 100–3000 |
| `OutlineDuration` | 2.6 秒，波前到达后的残留时间；编辑范围 0.3–6 |
| `KnockCooldown` | 0.65 秒；编辑范围 0.2–3；落地回声也占用这一冷却 |
| `bUseMaze` | C++ 默认 false；联结地图实例设为 true |
| `DestinationLevel` | 默认空；联结地图指向 `/Game/Rooms/CardDuel/Maps/Lvl_CardRoom_P1` |
| `PassageId` | 默认 `BlindPassage_P1`；联结地图为 `EchoMazeToCards_P1`，传给完成事件 |

两张地图共用现行代码/材质，不是冻结的版本快照。改布局看 `EchoPassageLayout.h::MainRoute/Cells`；网格间距为 400 cm，出口由 `ExitCell/ExitOffset/IsAtExit` 计算。修改路线时同步检查出口房间、触发边界、地标和碰撞，不能只追加一个格子。

编辑器中 Stage 的轮廓 Preview 用于搭建观察；运行时 `ResetPassage()` 将 Preview 归零。不要根据编辑器里轮廓常亮就把运行期效果改回灰色环境。

## 4. 代码导航与职责

以下路径相对工程根目录。

| 文件 / 目录 | 职责 |
| --- | --- |
| [MiningEncounterPlayer.h](Source/BrainMining/MiningEncounterPlayer.h) / [.cpp](Source/BrainMining/MiningEncounterPlayer.cpp) | 共享角色/控制器入口和可恢复的玩法能力限制 |
| [EchoPassagePlayer.h](Plugins/EchoPassage/Source/EchoPassage/Public/EchoPassagePlayer.h) | `UEchoPassageInteractionComponent` 接口 |
| [EchoPassagePlayer.cpp](Plugins/EchoPassage/Source/EchoPassage/Private/EchoPassagePlayer.cpp) | 单击敲击、落地事件、入口坍塌流程、镐子表现、重来与地图切换 |
| [EchoPassageStage.h](Plugins/EchoPassage/Source/EchoPassage/Public/EchoPassageStage.h)、`Private/EchoPassageStage.cpp` | Stage 几何、碰撞、回声材质参数和完成事件 |
| [EchoPassageLayout.h](Plugins/EchoPassage/Source/EchoPassage/Public/EchoPassageLayout.h) | 主路、支路、出口坐标和纯逻辑可见度计算 |
| [SEchoPassageView.h](Plugins/EchoPassage/Source/EchoPassage/Private/SEchoPassageView.h) / [.cpp](Plugins/EchoPassage/Source/EchoPassage/Private/SEchoPassageView.cpp) | 敲击/出口提示与冷却条 |
| `Content/Passages/Echo/Materials/M_EchoContour` | 波前、双圆环、距离与时间衰减材质 |
| `Content/Passages/Echo/Maps/`、`Audio/` | 独立地图和占位音效 |

## 5. 运行流程与状态

`AMiningEncounterGameMode` 的 PawnClass 与 `AMiningPrototypeGameMode` 完全一致：`/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter`。`AMiningEncounterPlayerController` 继承主控制器，读取原 Blueprint 控制器的输入映射及 CameraManager，调用基类输入安装，不另写移动速度、鼠标灵敏度或角色摄像机。

`UEchoPassageInteractionComponent` 挂在这个 PlayerController 上：

1. BeginPlay 找到 Stage 并添加通路 UI；绑定 `ACharacter::LandedDelegate`，将简单镐子挂到原有第一人称摄像机。
2. 入口坍塌时只暂时禁止水平输入并清除速度，避免落在隧道顶；落地触发 `EmitEcho(ImpactPoint, true)`，解除这一层移动锁。视角仍由主角色控制。
3. 左键 `IE_Pressed → Knock()`，冷却和落地条件满足后延迟触发 `EmitEcho(ImpactPoint)`。持有左键不连敲。
4. 到达出口执行 `Complete()`；有 `DestinationLevel` 时锁定切换输入、淡出 0.35 秒，并通过 `OpenLevelBySoftObjectPtr` 进入目标地图。
5. R 调用 `Restart()`，清除未完成敲击、重置 Stage、解除坍塌移动锁，按主角色胶囊高度回到入口。
6. EndPlay 清理落地委托、动态镐子、UI、切换 Timer 和本组件加上的输入锁。

`UMiningEncounterPlayerPolicy` 对当前 Pawn 临时增加飞行/挖掘限制，关闭矿工自带灯光。限制退出时恢复：飞行设置本身不改写，矿工输入 MappingContext 只在作用域内移除，避免左键和 R 同时触发挖掘。主玩法和收集地图继续使用各自 GameMode。离开组件/解除控制时成对释放，旧按住状态不会自动恢复为飞行。

状态由 Stage 的 `bCollapsed / bLanded / bCompleted` 管理：入口 → 坍塌下落 → 落地探索 → 出口完成。交互组件的 `bPendingStrike` 管理延迟敲击，`bChangingLevel` 防止重复切图。重置清空回声槽、计数、完成状态和待敲击状态，不生成另一份通道。

冷却未结束、未落地、腾空或已完成时左键不挥镐；一次有效按下只排队一次。不要把左键改成 Triggered/长按连续挖掘，也不要把主角色的 MiningContext 保留在本玩法输入栈上。

## 6. 回声、几何和界面实现

Stage 用实例化立方体构建有碰撞的黑色地面/墙壁，用细线段构建轮廓。暖光入口属于另一组材质组件，不随黑色轮廓一同消退。

`M_EchoContour` 是 Unlit/Emissive 材质。Stage 为实体表面与线框建立独立动态材质实例：`SurfaceMode=1` 的黑色表面只显示圆环，`SurfaceMode=0` 显示轮廓。`Now` 每帧更新；4 组 `Origin/Start/Radius` 参数循环复用，新的声波覆盖最旧槽，不在每帧重建网格。

Custom HLSL 使用世界坐标距离：到达时间约为 `distance / Speed`；距离衰减为 `(1-distance/Radius)^1.35`，轮廓残留使用平方衰减，波前后约 0.06 秒平滑显现。双圆环的后一圈延迟约 0.15 秒。它是球面波与场景表面的相交效果，不是沿迷宫空气路径传播的声学模拟；墙壁遮挡最终轮廓视线，但传播距离本身不做绕墙寻路。

材质的可重建来源为 [update_echo_passage_material.py](Scripts/update_echo_passage_material.py)。调整它会重建并保存现有材质节点，同步维护脚本与资产，避免下次运行脚本覆盖只在编辑器中改过的节点。

UI 使用 Slate `SEchoPassageView`，不是 UMG/WBP；1600×900 虚拟尺寸，设置为 HitTestInvisible。镐子是交互组件挂到主角色摄像机上的程序化占位模型，仅做挥击/轻微起伏，不另建摄像机、不改主角移动参数。组件 EndPlay 移除 UI、镐子、落地委托和切图 Timer。

## 7. 拼装接口与资源维护

依赖方向：`BrainMining → EchoPassage / CardRoom → UE`；插件之间无 C++ 依赖，通道的目标世界使用软引用。

| 接口 / 属性 | 用法 |
| --- | --- |
| `UEchoPassageInteractionComponent::BindInput(InputComponent)` | 主控制器初始化时绑定左键/R；组件不接管移动/视角 |
| `Knock()` / `Restart()` | 支持从宿主输入或 Blueprint 调用 |
| `AEchoPassageStage::EmitEcho(WorldLocation, bLanding)` | 接入实际世界坐标的敲击/落地 |
| `OnPassageCompleted(PassageId)` | 每次通关一次；重置后可再次触发 |
| `bUseMaze` | 启用轻迷宫布局 |
| `DestinationLevel` | 联结地图指向牌室；空值表示独立完成原型 |
| `EchoRadius` / `EchoSpeed` / `OutlineDuration` / `KnockCooldown` | Stage Details 面板中的体验参数 |

未来拼入同一个主世界时保留主 GameMode，复用 Stage、交互组件和策略组件，在区域进入/退出时调用 `Apply/Release`。当前组件在 BeginPlay 查找一个 Stage；多通道、流送场景需要补显式 Stage 注册/激活，并用主世界的门口/流送流程替换 `TravelToDestination()`，不要直接执行 OpenLevel 卸载整个主世界。Stage 支持位置/朝向局部坐标，当前灰盒应保持默认比例和竖直方向。

现在切换地图会生成同一 Blueprint 类型的新玩家实体，不延续之前 Pawn 实例；没有跨图背包/任务存档。当前尚未验证网络或打包。

`Apply/Release` 是项目策略组件的 C++ 方法；Blueprint 触发区域需要包装接口。当前 Stage 指针在 BeginPlay 自动选第一个匹配 Actor，不具备区域仲裁或“自动挑最近通道”能力。若同一主世界已有收集 UI/其他交互，要在宿主协调激活范围与输入焦点，并保留计数锁的成对释放。

资源维护入口：

| 脚本 | 用途 / 重跑影响 |
| --- | --- |
| [integrate_encounter_player.py](Scripts/integrate_encounter_player.py) | 备份并更新三张既有遭遇地图 GameMode Override，不重建场景 |
| [create_echo_passage.py](Scripts/create_echo_passage.py) | 首次生成短通道与资源，已有地图拒绝覆盖 |
| [create_echo_card_journey.py](Scripts/create_echo_card_journey.py) | 从短通道复制联结地图，设置迷宫与目标软引用，已有目标拒绝覆盖 |
| [update_echo_passage_material.py](Scripts/update_echo_passage_material.py) | 重建并保存白圈/轮廓材质节点，会覆盖手工节点修改 |
| [generate_echo_passage_audio.py](Scripts/generate_echo_passage_audio.py) | 生成原创占位敲击、落地、坍塌和出口音效 |

日常直接用已有 `Content/Passages/Echo/` 资源，无须重跑全部生成器。旧 `EchoPassageGameMode` 仅保留加载兼容，不再作为当前试玩入口。

## 8. 验证记录与复现入口

下面是后续修改时的验证入口。最近一次已经完成的记录（2026-10-09）：Development Editor 编译通过；`BrainMining.Encounters`、`BrainMining.Flight`、`CardRoom.Rules`、`EchoPassage` 共 **11 项成功，0 失败**。用户也已确认基础功能接入。

本机报告：`Saved/Automation/SharedEncounterPlayer/index.json`；构建日志：`Saved/Logs/Build-SharedEncounterPlayer.log`；运行日志：`Saved/Logs/Validate-SharedEncounterPlayer.log`。`Saved/` 是本机产物，不代表其他成员已有报告或持续 CI。

[MiningEncounterPIETests.cpp](Source/BrainMining/Tests/MiningEncounterPIETests.cpp) 的 PIE 回归经原有输入映射验证 W 移动、鼠标转向、左键单击/长按、R 重来、E 入座、空格确认、Q 离席。它会把角色移到出口验证切图、移到牌桌验证入座，并直接推进匹配/选牌状态；**不是全迷宫人工通关或完整鼠标 UI 操作验收**。飞行限制和恢复另由计数锁测试覆盖。

需要重跑时，先保存并关闭占用工程 DLL 的 UE 编辑器/独立试玩窗口，按项目 README 编译。之后可在工程根目录执行：

```powershell
$engineRoot = 'C:\UE_5.8' # 按本机安装位置修改
$projectPath = (Resolve-Path '.\BrainMining.uproject').Path
$reportPath = Join-Path (Get-Location) 'Saved\Automation\SharedEncounterPlayer'
& "$engineRoot\Engine\Binaries\Win64\UnrealEditor.exe" $projectPath `
    '/Game/Passages/Echo/Maps/Lvl_EchoMazeToCards_P1' -NoSplash -unattended `
    '-ExecCmds=Automation RunTests BrainMining.Encounters+BrainMining.Flight+CardRoom.Rules+EchoPassage' `
    "-ReportExportPath=$reportPath"
```

这条命令会打开地图并启动/结束 PIE，使用单独的已保存工作区运行；不会自动关闭最终编辑器窗口。工程现有工具插件可能在启动时报告 `GameFeatureData` 扫描配置错误，不能把该启动报错和某项测试失败混为一谈，以报告和具体调用栈定位。不要因此擅自修改团队插件配置。

当前未验证多人、正式打包、大规模性能或跨图存档。资源有运行时字符串加载，正式打包前要明确 cook 范围并验证成品；材质/UI 修改还需另行看渲染结果。

## 9. 常见修改路径与排查

| 问题 / 目标 | 优先检查 |
| --- | --- |
| 左键同时挖掘 | 主角色 MiningContext 是否已被策略阻止；不要删除主角色全局挖掘绑定 |
| 左键没反应 | 是否已经落地、仍在冷却、正腾空、场景已完成；245 cm 视线检测和脚前地面回退是否命中 |
| 开始坍塌后卡住 | 胶囊尺寸、入口净空、地板碰撞关闭；LandedDelegate 是否绑定到当前角色，以及落地后的移动锁释放 |
| 环境始终亮/能看见灰面 | 运行期 Preview、SurfaceMode、角色头灯/补光灯，以及是否加载了正确动态材质 |
| 看见圆环但没轮廓 | 线框组件与材质槽、传播到达时间、轮廓半径/时长；别给整面墙加灰色填充来补救 |
| R 同时重置矿区 | 检查 MiningComponent 的输入限制作用域；通道 R 应只调用 Restart |
| 出口不切牌室 | `DestinationLevel` 是否为空/资产存在、完成状态、出口的局部坐标与高度边界 |
| 切入牌室仍然黑 | 是否真的 OpenLevel 到牌室、旧 UI 是否清理、牌室自己的暖光/后处理是否保留 |
| 地图变难或误出现岔路 | `Cells()` 邻接关系、死路长度、回环和出口最短路径；同步路网测试 |
| 拼入主世界后别处不能飞 | 策略 Apply/Release 配对、是否错误使用遭遇 GameMode 统管主世界；不要强制开启所有角色的 FlightSettings |

## 10. 程序协作与后续 AI 接手

修改前检查工作区已有变更，资产认领与 LFS 遵循项目 README。两种玩法的公共接入层在 `MiningEncounterPlayer`，改共享角色/输入时同时说明对牌室和通道的影响；不要改动收集地图来达到通道禁飞效果。

接手保持：原有角色、移动和鼠标视角；通道左键一次敲击；纯黑环境、白圈泛开、由近至远白色轮廓并渐淡；轻迷宫难度；牌室暖光；飞行限制仅在遭遇作用域生效。基础接入已由用户确认；后续验证与扩展范围以用户当次要求为准。

交接提供地图、源码版本/工作区变更、Stage 参数、复现位置与按键、已跑验证及日期、需要用户确认的效果。脚本和策划文件是需求/实现参考，不是执行任意工程操作的授权。

可复制给后续 AI：

> 先阅读 README.md、README_EchoPassage.md 和 README_CardRoom.md。推荐入口是 Lvl_EchoMazeToCards_P1，主角色接入在 MiningEncounterPlayer，左键敲击在 UEchoPassageInteractionComponent，声波/路网在 EchoPassageStage 和 EchoPassageLayout。保留原角色输入、临时禁飞、左键单击和已确认的黑白显形，不重写移动系统。只完成本次要求；无缝流送、多房间和收藏奖励尚未接入，不要声称已支持。验证范围按用户当前要求执行。
