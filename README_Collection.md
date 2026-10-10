# 挖掘收集玩法：实现与协作指南

当前演示版本：**V0.3**。核对日期：2026-10-09。面向程序、美术技术协作者和后续 AI 编程助手。

玩家根据土面线索挖开通道，发现藏品，瞄准后按 E 收集，再在图鉴中旋转检视三维模型。系统复用项目已有第一人称角色、挖掘和飞行，在独立测试 Level 中运行。

首次接手建议按顺序阅读：**快速开始 → 当前规则 → 代码导航 → 实现细节 → 验证与接手约定**。本文件是收集玩法唯一的 README，统一维护当前规则、演示操作、版本差异和实现说明。

## 1. 快速开始

1. 按[项目 README](README.md)准备 UE **5.8.2**、C++ 工具链与 Git LFS，编译 `BrainMiningEditor / Win64 / Development`。
2. 在编辑器打开 `/Game/CollectionPrototype/Maps/Lvl_Collection_V03`，点击 Play。关卡已指定 `MiningCollectionGameMode`。
3. 本机也可双击 [Play_Collection_V03.cmd](Play_Collection_V03.cmd)。启动器默认引擎路径为 `C:\UE_5.8`；其他机器需修改启动器，或使用下文可配置命令。
4. 先拿左侧台座上的教学试管架，确认 E 收集、Tab 图鉴和鼠标拖动；再体验四个挖掘区。

| 输入 | 行为 |
| --- | --- |
| WASD / 鼠标 | 移动 / 观察 |
| 按住左键 | 使用原有挖掘组件连续切割土体 |
| 按住空格 / 松开 | 上升 / 下落，复用原有飞行移动 |
| E | 准星对准白色轮廓的藏品后收集 |
| Tab | 开关图鉴；图鉴内也可 Esc 或点击返回 |
| 图鉴内拖动 / 滚轮 | 双轴旋转 / 缩放模型 |
| 双击模型区域 / F | 恢复展示角度 |
| R | 重新生成本轮场地并清空收藏；图鉴打开时不执行 |
| Alt+F4 | 退出独立游戏窗口 |

默认编辑器及游戏启动地图仍为 `/Game/MiningPrototype/Maps/Lvl_MiningLab`，没有改成收集关卡。

建议向同事演示：先收集教学试管架并查看图鉴，再挖掉化学污渍所在土面，飞到鸟窝观察羽毛失去支撑后飘落，最后沿水管或蚂蚁线索找到洞内藏品。正数 RandomSeed 可固定演示布局。

## 2. 当前玩法规则

每轮四个区域随机排列四种藏品，另有一件完全露出的教学试管架：**总计 5 件、4 类**。完成提示按已获得的种类数判断，不要求先拿齐两件试管架。`Inventory[ItemId]` 保存每类数量，`UniqueCount()` 统计数量大于零的类别。

| 稳定 ItemId | 名称 | V0.3 初始状态 | 发现线索 |
| --- | --- | --- | --- |
| 0 | 天堂马桶 | 在封闭洞穴内完全露出、静置，无物理掉落 | 外露水管穿过土层，延伸至洞内 |
| 1 | 蚂蚁的电脑 | 在封闭洞穴内完全露出、静置，无物理掉落 | 入口蚂蚁爬入小洞，洞内还有蚂蚁 |
| 2 | 化学试管架 | 主区域中的一件埋在土中，清理后物理掉落；教学件静置 | 绿紫色污渍直接附着土体表面 |
| 3 | 歪头肉鸽 | 高处洞穴中露出并保留为可收集藏品 | 鸟窝、羽毛；挖通通道后鸟群飞出 |

“洞内完全露出”不等于站在门外就能看见或收集：前方仍有完整土墙，视线射线会被挡住。收集距离为 **400 cm**。物品必须 `bReleased && !bCollected` 且成为视线射线的首个命中物，才能高亮并被收集。

图鉴仅展示已收集模型，文字只有名称和简短介绍，不显示线索。打开时暂停单人世界并关闭角色输入；检视操作不会继续挖土或飞行。

当前是单机、单轮原型：R、结束 Play 或退出会丢失本轮收藏，没有存档、联网复制、交易或任务系统。策划来源为 `收集玩法【P1】.xlsx`，后续用户反馈已调整埋深、静置规则、描边、检视和线索行为；不要仅按最初表格或早期版本目标反向恢复旧表现。

## 3. 版本、关卡与参数

地图资产均在 `Content/CollectionPrototype/Maps/`。

| 关卡 | 生成分支 | 用途 |
| --- | --- | --- |
| `Lvl_Collection_V01` | `bDeepField=false`，`SpawnField()` 旧分支 | 最初短距离、四件埋藏物的流程验证 |
| `Lvl_Collection_V02` | `bDeepField=true`，深度 1100 cm | 较长挖掘路线，保留用于对照 |
| `Lvl_Collection_V03` | `bDeepField=true`，深度 750 cm | 当前同事演示版，深度参数较 V0.2 缩短约 32% |

**版本地图共用当前 C++ 实现，不是冻结的代码快照。** 例如 V0.2 也使用新污渍、羽毛逻辑，V0.1 也使用当前描边和检视界面；V0.1 的旧场地生成分支仍保留原先线索模型。

停止 Play 后选择 V0.3 的 `V03 demonstration collection rules` Actor：

| 属性 | 当前配置 / 作用 |
| --- | --- |
| `bDeepField` | V0.3 为 true；选择深层场地生成逻辑 |
| `ExcavationDepth` | 地图实例为 750 cm；C++ 默认仍是 1100，代码及编辑器限制 650–1800 |
| `RandomSeed` | 0 每轮随机；正数复现布局；`ActiveSeed` 可记录本轮实际种子 |
| `ExposureThreshold` | 0.65，控制埋藏试管架的释放资格，不用于静置洞穴藏品 |
| `DisplayVersion` | V0.3；只控制显示文本，不选择算法 |

布局使用世界坐标，+X 为挖掘深入方向，四区 Y 为 -900、-300、300、900 cm。岩体前表面 X=460；鸟区基准 Z=350，其余为 0。

设深度参数为 D：岩体长度 `D+600`，洞室中心 X=`460+D+250`，洞室尺寸为 560×440×460 cm；马桶、电脑、藏品鸽子 X=`460+D+290`。试管 X 约为 `460+D±35`，Y 也有少量随机偏移。因此 V0.3 的土墙到洞室入口约 **7.2 米**，到洞内藏品约 **10.4 米**；“750 cm 深度”不等于所有物品的实际距离。移动管理 Actor 本身不会平移整套布局，需要改生成坐标逻辑。

## 4. 代码导航与职责

| 文件 | 职责 / 先看哪些函数 |
| --- | --- |
| [MiningCollection.h](Source/BrainMining/MiningCollection.h) | 三个类及状态定义；`FCollectionAnt/Bird/Clue`；对蓝图和 Python 暴露的接口 |
| [MiningCollection.cpp](Source/BrainMining/MiningCollection.cpp) | 初始化、旧版生成、Tick、准星目标、收集、图鉴开关、预览与重置 |
| [MiningCollectionDeepField.cpp](Source/BrainMining/MiningCollectionDeepField.cpp) | `SpawnDeepField()`：四区、洞室、管道与线索；`TickAmbient()`、`FindBirdExit()`：蚂蚁及鸟群 |
| [MiningCollectionClues.cpp](Source/BrainMining/MiningCollectionClues.cpp) | `MakeFeather()`、`TickClues()`：羽毛低模、土层支撑、飘落及鸟窝木片清除 |
| [MiningCollectible.cpp](Source/BrainMining/MiningCollectible.cpp) | `BuildModel()`、名称简介、暴露测量、物理释放、CustomDepth 开关 |
| [SMiningCollectionView.cpp](Source/BrainMining/SMiningCollectionView.cpp) / [.h](Source/BrainMining/SMiningCollectionView.h) | 自绘 Slate HUD 和图鉴，布局、按钮命中、拖动、滚轮与键盘输入 |
| [MiningCollectionTests.cpp](Source/BrainMining/Tests/MiningCollectionTests.cpp) | 原生暴露/物理门限测试、角色及控制器复用测试 |

复用关系：`AMiningCollectionGameMode` 继承 `AMiningPrototypeGameMode`，继续使用 `/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter` 与 `BP_FirstPersonPlayerController`；仅将 HUD 换为普通 `AHUD`，由 Slate 显示收集界面。

挖掘继续调用 [MiningComponent](Source/BrainMining/MiningComponent.cpp) 和 [DiggableRock](Source/BrainMining/DiggableRock.cpp) 的真实动态网格切割；飞行使用 [MiningCharacterMovementComponent](Source/BrainMining/MiningCharacterMovementComponent.cpp)。洞室复用 [FogChamberComponent](Source/BrainMining/FogChamberComponent.cpp) 的反向内壁网格，收集关卡不设置其雾材质。

公共接口只为收集菜单增加了 `MiningComponent.CancelMining()`；构建增加 `SlateCore` 依赖。不要为收集玩法另写一套挖掘、角色或飞行系统，也不要把其他房间/插件的已有修改归到本功能。

## 5. 运行流程与状态

`AMiningCollectionField` 在 Tick 中等待玩家、`UEnhancedInputComponent`、`UMiningComponent` 和 LocalPlayer 就绪，然后仅初始化一次：

1. 创建临时 Enhanced Input Mapping Context，以优先级 50 绑定 E、Tab、R。
2. 只修改本关卡角色实例：挖掘 Reach=400、Radius=115、DownwardOffset=25，启用飞行；将挖掘组件的 `FallResetDistance` 设为 0，由收集场统一处理跌落重置。
3. 初始化四类库存、生成场地和藏品，创建检视模型、RenderTarget、灯光及 Slate 界面。

运行中的主要路径：

```text
原挖掘输入 → MiningComponent.TryMine → DiggableRock.MineSphere
                                        ↓ 成功切割改变 SuccessfulCuts
             ┌ 暴露采样 → 碰撞盒清空检查 → 试管物理释放
Field.Tick ──┼ 羽毛/鸟窝支撑检查 → 飘落或清除
             └ 鸟洞实际空气路径检查 → 一次鸟群演出

玩家视线 → RefreshTarget → 外轮廓 → E / CollectTarget → Inventory
                                                        ↓
                    Tab → 暂停与输入切换 → Slate 图鉴 + 3D Capture
```

`bReleased` 表示已满足收集前置状态，**不等于正在模拟物理**：静置洞穴物品生成时就置 true，使用 QueryOnly；埋藏试管释放后使用 QueryAndPhysics。

收集时先重新做视线检查，再关物理、关碰撞、隐藏 Actor、增加库存并清空 Target。不是立即 Destroy；`bCollected` 防止重复收集。`Items` 在本轮中保留这些引用。

运行时生成的岩体、藏品、装饰和演出 Actor 由 `Spawned` 统一登记；蚂蚁、羽毛、鸟群的临时状态用弱引用记录。新增场景对象应纳入这一生命周期，避免 R 后重复或遗留。

`ResetCollection()` 调用原有 `Mining.ResetField()`，销毁 Spawned，清空 Items/库存，重新生成布局和动画状态。角色低于初始高度 2000 cm 也触发此流程。图鉴打开时拒绝重置；EndPlay 恢复菜单状态并移除输入 Context 和 Slate 界面。

## 6. 关键实现细节

### 埋藏物暴露与释放

约每 0.12 秒检查一次，且只有宿主岩体 `SuccessfulCuts` 变化才重新采样。`MeasureExposure()` 在模型顶点中抽样约百个点，转到岩体局部坐标，使用 AABBTree + FastWinding 判断采样点是否处于实际土体之外。

达到 65% 仅代表可以尝试释放。`Release()` 还检查碰撞盒附近 27 个位置，在 -X 方向以 20 cm 步长、最多 200 cm 搜索清空位置，途中中心必须在空气里。成功后才开刚体，赋质量和轻微外向冲量。它不是从藏品到入口的完整路径规划；完整网格空气连通检查用于鸟群。不要用“挖了几次”替代真实暴露或强制给埋在土中的物体开启物理。

### 只在外侧显示的白色描边

`SetHighlighted()` 给模型开启 CustomDepth。`M_CollectionSilhouette` 在色调映射后比较 SceneDepth/CustomDepth，向外扩展约 2 像素，保留物体内部颜色，并抑制被场景遮挡的轮廓。

旧 `WhiteSilhouette` 网格和 `M_CollectionOutline` 资产仍在，但始终隐藏，不应重新启用白色膨胀外壳。项目当前 `r.CustomDepth=1`。后处理是 Field 上的 unbound 组件，其他系统若也向 CustomDepth 写入，需评估共用掩码；目前未用独立 stencil ID 区分类别。

### 图鉴与三维检视

界面是 `SLeafWidget`，**不是 UMG/WBP**。按 1600×900 虚拟尺寸缩放，绘制与鼠标命中都要保持一致。模型区域约占左侧 72% 宽度，右侧只显示名字和短简介。

预览是独立 `AMiningCollectible`，位于 `(0,0,-10000)`，使用同一个 `BuildModel()`。`USceneCaptureComponent2D` 只显示该模型，输出 1536×1024 RenderTarget，并有两盏展示灯。不是图片冒充模型，也不是旋转场地里的原物品。

旋转俯仰限制 ±80°，默认角度 yaw=-30°、pitch=-8°；默认观察距离 330 cm，滚轮范围 270–550 cm。以 `ResetPreview()` 的运行值为准，头文件中的成员初始值不是最终展示值。修改模型或角度会刷新 Capture，随后再刷新三帧；Field 允许暂停时 Tick，避免菜单切换后模型不更新。

打开图鉴会 `CancelMining()`、停挖掘 Tick、清飞行输入、停移动、禁 Pawn 输入、暂停世界并切到 UIOnly。关闭时恢复输入与游戏模式。改 UI 时务必保留这些恢复路径。

### 化学污渍

`M_CollectionStainedEarth` 直接作为试管区岩体材质，没有独立污渍 Actor 或贴花平面。Custom HLSL 使用世界坐标和每区 `TrailY` 参数，在土面画不规则绿紫泼洒与流痕。对应三角面被挖掉，该处污渍就不存在；新暴露的深层土面也可显示残留。这是程序化土体着色，不是液体流动或可保存的涂画系统。

材质节点及 HLSL 的可重建来源在 [create_collection_v3.py](Scripts/create_collection_v3.py)。调整污渍时同步脚本与生成资产，避免后续运行脚本覆盖仅在编辑器里改过的效果。

### 羽毛、鸟窝与鸟群

- 羽毛由双面 lowpoly 羽片和羽轴生成，初始入口 3 片、内部 4 片。使用 `FCollectionClue.Support` 作为土层支撑采样点，支持状态只在宿主有效切割后更新；同一个脏岩体共用一次查询树构建。
- 支撑消失后羽毛摇摆、旋转并下落，以球扫掠检查实际场景；合适的向上表面使其落地。落点支撑再被挖掉可重新下落。自进入下落状态起约 12 秒清理；不是落地后额外等待 12 秒。
- 外部鸟窝木片失去各自的支撑采样点后直接销毁；内部巢仍是静态装饰。支撑是点采样，不是整个接触面的结构力学模拟。
- 鸟区被切割后，`FindBirdExit()` 以实际土体网格做 65 cm 网格 BFS，每点检查中心及邻近空气采样，最多访问约 6000 节点。找到通往前方外侧的通道才触发 `FlockBursts=1`。
- 演出生成 8 只小鸟，按延迟沿该路径飞出并扇翅；保留原来的藏品鸽子。路径生成后不持续重规划，没有完整群体 AI 或玩家动态避让。R 后重新允许触发。

调试标签：`CollectionFeather`、`FeatherFalling`、`FeatherLanded`、`CollectionFlyingBird`、`CollectionAmbientAnt`。标签用于查询，不是独立状态机的权威来源。

蚂蚁共 18 只，入口与电脑旁各 9 只，使用循环路径位移、身体摆动和腿部旋转。入口小洞是真实切割；土层遮挡内部个体。当前没有寻路 AI。

## 7. 资源与生成脚本

模型在 C++ 运行时构建，没有四个独立的导入藏品模型资产；更换美术前注意游戏模型与预览模型共用构造逻辑。材质在 `Content/CollectionPrototype/Materials/`：

| 资产 | 用途 |
| --- | --- |
| `M_CollectionTint` | 藏品及低模装饰的 Tint 材质实例基础 |
| `M_CollectionEarth` / `M_CollectionStainedEarth` | 普通土体 / 化学污渍土体 |
| `M_CollectionSilhouette` | 当前外轮廓后处理 |
| `M_CollectionOutline` | 旧外壳方案保留资产，当前不显示 |
| `M_CollectionFloor` / `Bedrock` / `Accent` | 场地地板、隔墙、标识装饰；后两者完整名称也有 `M_Collection` 前缀 |

| 脚本 | 重跑行为 |
| --- | --- |
| [create_collection_v1.py](Scripts/create_collection_v1.py) | V0.1 已存在时直接拒绝执行；首次生成基础材质和旧地图 |
| [create_collection_v2.py](Scripts/create_collection_v2.py) | 补缺失的描边材质和 V0.2 地图；已有材质/地图保留，不自动更新其节点或摆放 |
| [create_collection_v3.py](Scripts/create_collection_v3.py) | **每次重建并保存污渍材质节点**；仅在 V0.3 不存在时复制 V0.2，并写入 750 cm 和版本号 |

正常协作直接使用仓库里的 `.umap/.uasset`，不需要重新生成。如果从无资产状态重建，先编译当前 C++，再按 V1 → V2 → V3 顺序运行。不要批量运行 `Scripts/` 下其他功能的生成器。重跑 V3 不会修正一个已经存在但参数被改过的 V3 地图；此时应在 UE 中明确编辑目标地图。

## 8. 构建、验证与结果解释

以下 PowerShell 命令从工程根目录运行。先保存并关闭占用该工程 DLL 的编辑器和试玩进程；这些命令启动独立验证进程。`$engineRoot` 按本机安装修改。

```powershell
$engineRoot = 'C:\UE_5.8'
$projectPath = (Resolve-Path '.\BrainMining.uproject').Path
$editorCmd = Join-Path $engineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$projectRoot = Split-Path $projectPath

& "$engineRoot\Engine\Build\BatchFiles\Build.bat" BrainMiningEditor Win64 Development "-Project=$projectPath" -WaitMutex -NoHotReloadFromIDE -MaxParallelActions=4
if ($LASTEXITCODE -ne 0) { throw 'Build failed; stop before testing.' }
```

当前版本集成验证（原有挖掘组件、实际视线和网格）：

```powershell
$verify = Join-Path $projectRoot 'Scripts\verify_collection_v3.py'
& $editorCmd $projectPath -unattended -nosplash -nullrhi '-DisablePlugins=AllToolsets,ModelContextProtocol' "-ExecutePythonScript=$verify" "-abslog=$projectRoot\Saved\Logs\CollectionV03Runtime.log"
$reportPath = Join-Path $projectRoot 'Saved\Validation\collection_v03_runtime.json'
$report = Get-Content -LiteralPath $reportPath -Raw | ConvertFrom-Json
$report | Select-Object passed, seconds, cuts, error
# 核对报告修改时间属于本次运行；旧报告不能证明新代码通过。
```

原生测试与画面检查分别执行：

```powershell
& $editorCmd $projectPath /Engine/Maps/Entry -unattended -nosplash -nullrhi '-DisablePlugins=AllToolsets,ModelContextProtocol' '-ExecCmds=Automation RunTests BrainMining.Collection' '-TestExit=Automation Test Queue Empty' "-ReportExportPath=$projectRoot\Saved\Validation\CollectionAutomation"

$capture = Join-Path $projectRoot 'Scripts\capture_collection_v3.py'
& $editorCmd $projectPath -unattended -nosplash -RenderOffscreen -NoSound '-DisablePlugins=AllToolsets,ModelContextProtocol' "-ExecutePythonScript=$capture" "-abslog=$projectRoot\Saved\Logs\CollectionV03Visual.log"
# 仅复核污渍可额外传 -StainsOnly。
```

生成器是另一类任务，使用 `-run=pythonscript -script=...`；PIE 集成与截图脚本使用上面的 `-ExecutePythonScript=...`，需要编辑器持续 Tick，不要混用。验证脚本会自行结束 PIE 并退出其所属编辑器进程，不应在有未保存工作的编辑器里直接运行。

命令行临时禁用两个工具插件，是为了避开既有工具插件扫描问题，不代表已修改 `.uproject`，也不要据此直接删除团队插件声明。依赖配置以项目 README 为准。

V0.3 最近一次本机记录：编译通过，`collection_v03_runtime.json` 为 passed，**52 项检查**通过，四类分别经过 6–7 次有效切割完成收集；包括羽毛支撑/下落/落地/重置、收集、图鉴、鸟群及原 MiningLab 回归。相关画面位于 `Saved/Validation/CollectionV03Visual/`。此前原生 `BrainMining.Collection` 两项通过。

这些是历史验证记录，不是 CI 保证。测试自动设置角色位置，截图脚本还直接切割土体准备场景，不等于真人完整通关或帧率测试。NullRHI 无法验证着色器和实际轮廓画面；改材质或 UI 必须看渲染截图或人工试玩。`Saved/` 中报告和截图是本机产物，其他机器需重跑；V1/V2 脚本的部分断言是旧规则，不能机械地全部当作当前验收标准。

## 9. 常见修改路径与排查

| 要改什么 / 遇到什么 | 优先检查 |
| --- | --- |
| 演示更快或更慢 | V03 Field 实例 `ExcavationDepth`；同步生成器默认值、文档和距离测试断言 |
| 增加第 5 类藏品 | 先整理硬编码的 4 类假设：IDs 洗牌、四区布局、Inventory、名称/简介/模型分支、UI 卡片/完成文案和测试；不要只加一个模型分支 |
| 替换藏品模型 | `BuildModel()`、模型单位/枢轴/朝向、碰撞盒尺寸、暴露采样与检视构图；模型正反面法线必须正确 |
| 污渍形状或颜色 | V3 生成器内 HLSL；每区材质实例 `TrailY`；同步生成资产 |
| 羽毛悬空或不掉 | Actor 所属 HostRock、Support 坐标、SuccessfulCuts 是否变化、`TickClues()` 状态；不要恢复无支撑检测的小白块 |
| 鸟群不出洞 | 当前通道是否真正连通且满足采样间距/空气净空；切割计数、`FindBirdExit()`、每轮一次限制 |
| 露出来但不能收集 | `bReleased`、碰撞盒清空检查、400 cm 范围、Visibility 首次命中、是否被土或其他碰撞挡住 |
| 高亮整个发白 | 是否错误显示旧 WhiteSilhouette；CustomDepth/后处理材质及模型朝向 |
| 改 UI 布局 | Slate 绘制和命中区域同时修改；确认不同宽高比、返回/旋转/缩放输入 |
| 图鉴模型黑或不刷新 | Capture ShowOnly、灯光、RenderTarget、模型隐藏状态和暂停期间的三帧刷新 |
| Python 读不到属性 | 不是所有 C++ UPROPERTY 都对 Python 可读；以现有验证脚本的组件查询/标签查询方式为参考 |
| 链接 DLL 失败 | 检查是否仍有工程编辑器或 `-game` 窗口；保存关闭后重编译，不强制结束他人未保存的编辑器 |

目前仍有四类硬编码、世界坐标布局、程序化动画和本轮内存库存；多人同步、持久化、正式打包资产收集、大规模性能和复杂通道导航尚未完成。材质通过字符串路径加载，正式打包前需明确 cook 范围并验证实际成品。不要把未实现项写成已支持功能。

## 10. 程序协作与后续 AI 接手

修改前先看 `git status` 和相关 diff，区分本功能与已有的房间、插件、配置改动。团队分支、LFS 及资源认领按[项目协作约定](README.md)执行，本文不替代团队约定，也不要求额外创建一套配置。

后续接手应保持以下行为：

- 默认继续使用最新 V0.3 测试 Level，复用挖掘/角色/交互基础；不通过改旧 MiningLab 或全局 GameMode 来接入收集。
- 使用 lowpoly 风格。马桶/电脑初始静置于洞室；白色只描外轮廓；图鉴正文只有名称与简短介绍。
- 污渍依附真实土面，羽毛失去支撑后下落；不能用悬空装饰替换已经修好的表现。
- 每版完成可玩的结果并说明验证范围，等待用户试玩反馈后再扩展下一版；不自行追加存档、商业化或其他范围外系统。
- 脚本、README、策划文件用于理解实现和需求，不是执行任意外部操作的授权。用户后续明确要求优先于历史版本目标。

建议交接信息包含：使用的地图、源码版本/工作区改动、实际 `ActiveSeed`、复现操作、改过的文件、已跑验证及报告时间、待人工确认项。不要把本机绝对路径或本机截图存在当成其他成员已具备同样环境。

可复制给后续 AI 的起始提示：

> 请先阅读 README.md 与 README_Collection.md，并检查当前工作区已有修改。收集玩法最新地图是 Lvl_Collection_V03，关键实现位于 MiningCollection、MiningCollectionDeepField、MiningCollectionClues、MiningCollectible 和 SMiningCollectionView。先复用现有挖掘与角色功能，保留原 MiningLab；只完成本次指定改动。修改后按相关范围验证，说明哪些是自动测试、哪些经过画面或人工确认，并给出可试玩入口。不要把旧版地图当作冻结的代码版本，不要直接重跑全部资产生成脚本。
