# 宝石与碎晶离线生成工具（供设计师与 AI 接入）

统一 Python API：`Scripts/campaign_minerals_api.py`。批处理入口：`Scripts/bake_campaign_minerals.py`。
必须在本项目的 Unreal **编辑器 Python** 中执行；普通 Python 只能运行配方检查。C++ 编辑器接口在 `UMiningDesignerLibrary`，需要先编译 `BrainMiningEditor / Win64 / Development`。

## 三种操作

| 操作 | Python API | 适用情况 |
| --- | --- | --- |
| 完整重建 | `field, report = rebuild(recipe=...)` | 改了矿道形状、岩层、矿区位置、路线或经济分布；重新扫描岩面、放置并埋藏宝石、生成表面和内部碎晶 |
| 只重建碎晶 | `field, report = rebuild_clues(preserve_surface=True)` | 宝石和地形不变，只更新碎晶；默认保留所有原表面碎晶，重新生成内部线索 |
| 只校验 | `report = validate()` | 检查已存布局是否仍适配当前地形，不替换布局、不保存 |

`rebuild_clues(preserve_surface=False)` 会重新投射表面碎晶，但仍保持宝石的坐标与价值。岩层被大改、移动或替换之后，优先完整重建；只重建碎晶不会替你修复宝石坐标或旧 Host 引用。

所有生成都发生在编辑阶段。固定配方、相同地形和路线得到固定结果，最终坐标保存在 `MiningTreasureField.AuthoredPlacements`。运行时读取已存坐标，不重新抽取位置或价值。编辑器内部使用固定种子采样，这不等于玩家开局随机。

## 已打开编辑器中的调用

停止 PIE，把主图 Persistent Level 设为 Current Level。在 UE Python 控制台、Python 插件或 Unreal MCP 的 Python 执行入口调用：

```python
import sys, unreal
from pathlib import Path
root = Path(unreal.Paths.project_dir()).resolve()
sys.path.insert(0, str(root / 'Scripts'))
import campaign_minerals_api as minerals

# 根据当前地形完整重建；此调用不加载别的地图、不保存、不退出编辑器。
field, report = minerals.rebuild(recipe=root / 'Config/CampaignMinerals.json')
assert report['ok'], report['errors']
print(report['deposits'], report['fragments'], report['layout_sha256'])

# 也可以仅校验，或仅刷新内部碎晶：
report = minerals.validate()
# field, report = minerals.rebuild_clues(preserve_surface=True)
```

默认要求当前世界只有一个 `MiningExhibitHall`，且其所在 Level 只有一个预制 `MiningTreasureField`。存在多个时必须显式传 `hall=...`、`field=...`，避免改错对象。允许尚无矿物 Field，完整重建会创建它。已有 Field 会保留 Actor 身份和外部引用。

API 不保存地图。预览通过后由调用方保存；推荐先另存新图或备份原 `.umap`。想要自动备份、保存和 JSON 报告，使用下方独立进程入口。不要在游戏 `BeginPlay`、挖掘回调或每帧调用这些编辑器工具。

## 批处理与自动保存

先保存并关闭正在编辑同一项目的 UE。下面命令在项目根目录执行；默认 **只校验**，只有 `-MineralsSave` 才写地图。

```powershell
$projectRoot = (Get-Location).Path
& 'C:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' "$projectRoot\BrainMining.uproject" /Engine/Maps/Entry "-ExecutePythonScript=$projectRoot\Scripts\bake_campaign_minerals.py" -MineralsMode=validate -unattended -nullrhi -nosound
```

完整重建并保存：把 `-MineralsMode=validate` 换成 `-MineralsMode=rebuild -MineralsSave`。

| 参数 | 默认值与意义 |
| --- | --- |
| `-MineralsMap=/Game/...` | `Config/CampaignLayout.json` 的 `map`；要读的主图 |
| `-MineralsRecipe="C:/.../recipe.json"` | `Config/CampaignMinerals.json`；也接受相对项目根目录的路径 |
| `-MineralsMode=validate/rebuild/clues` | 默认 `validate` |
| `-MineralsSave` | 校验成功后保存；不指定时生成结果随独立编辑器退出而丢弃 |
| `-MineralsOutput=/Game/...` | 可另存新主图；默认覆盖输入主图，不能覆盖配置里的玩法源地图 |
| `-MineralsRefreshSurface` | 仅 `clues` 模式：重建表面碎晶；默认完整保留表面 |

批处理报告固定写入 `Saved/MineralAuthoring/latest.json`。自动化调用方先确认报告的 `started_utc` 属于本次启动，再检查 **`ok == true`**；要求保存时还要检查 **`saved == true`**。UE 进程退出码本身不足以证明脚本成功。报告包含矿点数量、碎晶数量、布局 SHA-256、每处内部密度及错误。失败报告附 traceback；开始执行时先将报告标为未成功，避免途中崩溃遗留上次成功状态。

写图前备份输入地图、已有输出地图和经济配方到 `Saved/CampaignBackups/mineral-authoring-*`。生成先在临时 Field 上完成，埋藏与碎晶校验全部通过后才替换原 Field 数据；失败保留原 Field。保存失败时检查文件锁，并利用报告中的备份恢复磁盘文件。不会重新生成材质或改写源房间、岩层网格。

## 关卡大改后必须提供的数据

1. 主关卡要有 `MiningExhibitHall`，每个矿区 Area 设置 `Kind=Mining`、唯一 `Tag` 和至少两个 `MapRoute` 点。路线是**世界坐标**，按玩家前进方向排列；移动矿区后必须更新路线。当前命名如 `BM_Campaign_Mine01`。
2. 可挖岩层必须是 `ADiggableRock` 或 `AGeneratedMiningRock`，具有对应矿区 Tag，并有有效、闭合的可挖网格和可见性碰撞。普通 Blender 导入的 StaticMeshActor 不会自动变成可挖岩层。使用现有 `ImportCampaignRock`/Blender 回导流程更新已有可挖 Actor。
3. 配方 `bands[].area` 对应 Tag 去掉 `BM_Campaign_` 后的名字。`range` 是沿 MapRoute 弧长的 0～1 区间；不是世界 X 坐标。更换路线后同步调整这些区间。
4. 岩层要有足够厚度和可用体积容纳宝石与内部碎晶；生成器不会为了达到数量自动加土、改通道或穿透空洞。如果报点位不足，检查 Tag、路线、碰撞、厚度，然后调整预算和间距。
5. 先运行只校验，再完整重建。最后检查出生点、第一处线索、稀有宝石、房间门口，并进行挖掘与拾取验证。生成器不负责房间拼接、通道重建或主路更新。

## 可调配方和当前碎晶规则

经济配置：`Config/CampaignMinerals.json`。

- `seed`：固定的离线经济分配种子。修改它会改变生成结果，保存后所有玩家仍相同。
- `values`：ID 1～9 的四档正整数价值；每组严格递增。
- `value_weights`：四档价值的相对权重。
- `bands`：区域、路线区间、矿物 ID、类型权重、预算和最小选点间距（cm）。可按矿区增删阶段；`upgrade_cost` 仅是策划说明，不参与当前生成。
- `budget` 是该段可生成的价值上限；不能整除时允许小额余量，实际总额写入报告。`spacing` 限制候选点，埋深适配可能使最终位置略有变化。

碎晶算法在 `Source/BrainMiningEditor/Private/MiningCampaignMinerals.cpp` 的 `RefineCampaignCrystalClues`，当前采用已经确认的视觉规则，暂未把其常量扩展为外部配置：表面目标 14 枚、范围约 135 cm；通向主体的内部层间距约 24 cm，宽度逐渐增加；核心半径 225 cm，内部最小间距 17 cm；内部碎晶中心周围六方向 18 cm 的点需埋在岩体中。

校验按 **内部**碎晶（排除表面提示）的 0–80、80–160、160–225 cm 球壳体积计算密度，要求中层至少为外层 **1.40 倍**、内层至少为中层 **1.55 倍**，且外层非空。无法满足就报错，不保存不合格布局。修改算法时保留此校验合同，必要时同时更新合同和测试，并重新编译。

每个内部碎晶的 `FragmentSupports` 就是其三维中心。表面碎晶为了露出岩面，显示中心略微朝外，支撑点位于岩面内侧约 5 cm。运行时实际岩层提交后，只移除支撑点不再被实体岩层覆盖的碎晶。拾取宝石不会删除尚有支撑的碎晶。

## 底层接口与旧脚本

`unreal.MiningDesignerLibrary` 已暴露：

- `find_campaign_mineral_sites(hall)`：按路线扫描当前岩面，返回候选点；稳定排序岩层以消除遍历顺序影响。
- `prepare_campaign_minerals(field)`：调整埋深并生成碎晶。**不能当成幂等刷新入口反复调用**，旧实现根据当前深度继续加深。统一 `rebuild()` 总从新扫描的候选点开始。
- `refine_campaign_crystal_clues(field, preserve_surface)`：保持主体不动、刷新碎晶。
- `validate_campaign_minerals(field)`：返回 JSON 字符串，检查价值档、埋藏、Host、支撑点、表面线索及内部密度。

底层修改函数失败可能留下部分临时数据；优先调用有 staging 保护的 Python API。`Scripts/add_campaign_minerals.py::populate(world, hall)` 已转发到新 API，完整关卡生成器自动使用同一流程。其旧独立启动方式已改为明确报错，避免误运行破坏性旧路径。

`update_mining_interior.py`、`upgrade_campaign_crystals_museum.py` 等是历史定向迁移，不是通用工具；新接入不要将它们当作生成入口。特别是博物馆迁移脚本还会改房间连接。材质/碎晶模型更新使用专门美术脚本，需要渲染时不要用 `-nullrhi`；本工具只改坐标布局，所以支持 `-nullrhi`。

## 验证入口

普通 Python：`python Scripts/test_campaign_mineral_plan.py`。
UE 集成：把批处理命令中的脚本换成 `Scripts/verify_mineral_authoring.py`，不添加保存参数。默认在真实主图中试生成一个矿点；增加 `-MineralAuthoringFull` 则使用完整经济配方重建所有矿点。检查失败保留原布局、重复生成一致、表面保持和临时地图保存后重读；完成后删除测试地图，不保存主图。报告：`Saved/MineralAuthoring/integration.json`。

交给其他 AI 的提示可以直接写：**先读 `Docs/MineralAuthoring.md`，更新地形对应的 MapRoute 和岩层 Tag，再通过 `campaign_minerals_api.rebuild()` 或批处理入口重新烘焙；校验通过后保存主图，并报告布局 SHA-256。不要在运行时生成，不要调用历史迁移脚本，不要修改源玩法房间。**

### 2026-10-13 验证记录

- Editor Development 编译成功，4 项独立 Python 配方测试通过。
- 当前主图只读检查：131 处宝石、188,717 枚碎晶，埋藏、支撑点和内部密度全部通过。
- `-MineralAuthoringFull` 全量集成通过：完整配方生成 131 处矿点、151,490 枚碎晶；连续两次重建的布局 SHA-256 一致。仅刷新碎晶时主体/表面保持、重复刷新一致、临时地图保存后重读一致，全部通过。
- 注入生成失败后，原 Field 身份和布局 SHA-256 保持不变。测试主图 SHA-256 前后不变，测试地图已清理。
- 全量预演生成的是重新扫描岩面后的新布局，并未覆盖当前主图。数量差异来自重选位置与埋深，两个布局均独立通过内部密度合同；最终美术观感仍需新关卡中的实际挖掘预览。
