# 空心矿区原型：编辑与测试

本阶段实现的是**两间房之间的可编辑空心矿区**，不是完整随机主场景生成器。原有整面直连、空间和通道截面编辑模式保持可用；没有替换原地图。

可编辑开放/隐藏支路与洞室已在独立地图 `Lvl_MiningHollowBranches_Demo` 扩展，操作见 `README_HollowBranches.md`。本页仍描述原始两房主路演示。

多房间串联现已支持，示例为 `Lvl_MiningHollowChain_Demo`，操作见 `README_HollowChain.md`；无需迁移本页的旧单矿区示例。

## 从哪里打开

- 新地图：`/Game/MiningPrototype/Maps/Lvl_MiningHollow_Demo`。
- 项目：`E:\UE5\UEProjects\BrainMining\BrainMining.uproject`。
- 可以直接 Play。移动和挖掘沿用原来的操作，M 显示开发者地图；其他小地图操作见 `README_AutoRooms.md`。
- 地图出生点在 A 房间，朝向 B 房间。圆形主路初始就是空的，紫色岩塞需要挖穿；主路侧面的棕色普通岩石也能挖。
- 只有矿区整体外壳和房间外壳是不可破坏基岩，**内部主路没有单独的基岩套管**。
- 这只是白盒玩法原型，不是最终美术。旧打包程序没有更新。

## 最常用的三个编辑对象

Outliner 的 `HollowDemo/EDIT` 文件夹内：

| 对象 | 用途 |
| --- | --- |
| `ROOM_A_START_MOVE_AND_RESIZE`、`ROOM_B_DESTINATION_MOVE_AND_RESIZE` | 移动房间；`Room Size` 改房间内部尺寸。位置锚点是地板中心。 |
| `REGION_EDIT_Path_Radius_Plugs_RockBounds` | 调整岩区宽度/上下余量、主路路径/半径、岩塞位置和厚度。 |
| `HOLLOW_GENERATOR_Preview_and_Build` | 切换剖面/游戏预览、手动重新生成，以及原有分块/角色/地图设置。 |

不要编辑 `GENERATED_*` 或 `PREVIEW_*`，它们会在重新生成时替换。

## 具体怎么调

### 1. 岩区整体大小

选中 REGION，在 Details 中：

- `Region Width`：Y 方向整体宽度，示例 2600 cm。
- `Rock Above`：起点房间地板上方，主路直径之外的顶部岩层余量，示例 500 cm。
- `Rock Below`：终点房间地板下的岩层余量，示例 400 cm。
- X 方向长度跟随两个房间相对的墙面间距；移动 B 房间即可拉长/缩短岩区。
- `Resolved Size` 是实际生成尺寸，`Region Status` 是生成结果或错误原因。

### 2. 主路形状

- 在 REGION 上选择 `EditablePassagePath` 样条组件，拖动视口内的中间控制点。
- `Passage Radius` 调整全程统一半径，示例 350 cm；本阶段为 16 边近似圆的 Y/Z 横截面和线性连接。
- 两端自动跟随房间朝向 +X/-X 的墙面，以及房间地板高度。中间点保留手工编辑位置，不会随房间改动被重排。
- `Fit Path Between Rooms` **会把手工路径重新初始化为五个直连点**。需要重画路径时才点；支持撤销。
- 改动会自动重新生成。想批量调整多个字段，可在生成器关闭 `Auto Rebuild`，完成后点 `Rebuild Tunnels` 或 REGION 的 `Rebuild Region`。

### 3. 岩塞

在 REGION 的 `Plugs` 数组中逐项编辑：

- `Position`：沿整条主路实际长度的比例；0.25 表示四分之一处，不是固定 X 坐标。
- `Thickness`：沿路径的厚度，单位 cm，示例 240 cm。
- `Enabled`：暂时禁用而不删除这项。
- 可以添加/删除数组项，也可以全部删除得到完整空心主路。
- 紫色是同一可挖网格里的体积区域，不是另外叠一层薄片；挖出的新切面仍按世界空间位置区分紫色/普通岩石。
- 同一个岩塞跨过路径拐点时，底层会拆成多个材质体积；Details 中的材质体积数不等于岩塞数。

### 4. 剖面、颜色和照明

- 生成器的 `Show Section`：剖面预览，不影响游戏中的碰撞和完整地形。
- `Section Axis = 1` 是沿 Y 切开，`Section Position` 是世界坐标切面位置；路径侧偏后可微调切面以看清空洞。
- `Show Game Geometry`：查看完整游戏几何；`Show Layout`：布局预览。
- `MI_HollowRegionRock` 位于 `/Game/MiningPrototype/Materials/HollowRegion`，可调 `OrdinaryColor`、`GuideColor`、`Glow`、`PatternScale`、`Roughness`。
- 材质的 `GuideCount`、`GuideStart*`、`GuideEnd*` 是生成器运行时填写的参数，不要手填；替换岩石材质时需要保留这些参数和体积遮罩逻辑。
- 保留原有角色头灯 4、近身补光 1，角色缩放 0.7。房间灯和独立场景灯也可编辑。

## 本阶段明确的限制

- 正好两间主房间、一个推进段。进入 B 房间后整段一起冻结，不能继续在完成段挖掘；三个岩塞不是三个推进段。
- B 房间位于 A 房间的 +X 方向，墙面间距 1000–24000 cm，地板不高于 A。
- 2–12 个路径点，每段 X 至少前进 100 cm，不允许向上或倒走；Y 偏移不超过该段 X 的 70%，坡度不超过 `Maximum Slope`。
- 圆形开口必须放得进房间，主路距离岩区侧面及上下边界至少保留 100 cm 普通岩石。
- 半径 150–1000 cm；岩塞厚 50–1000 cm，距房间口至少 300 cm，岩塞间空路至少 250 cm。
- 最多 12 个岩塞，跨路径节点后的材质体积最多 16 个。
- 不兼容同时启用旧 `MiningLayoutSpace` 空间或旧通道截面控件；在各自旧地图继续使用那些功能。
- **还没有实现**随机主路、随机支路、变化半径、倒走分支、收集物/事件房分布。下一阶段再在已验证的岩区底座上接入。

如果修改超出限制，会显示 `NOT APPLIED` 并保留上一次成功地形，而不是生成断裂通道。此时编辑控件和地形可能暂时不一致，应按状态提示修正。

## 性能与小地图

- 新岩区沿 X/Y/Z 三轴分块；示例产生 16 个块，`Chunk Length` 为 1600 cm。
- 仍为**每块最多 30,000 三角形、每推进段最多 128 块**，没有上调预算。
- 只对受到挖掘影响的块做布尔和碰撞更新；跨块挖掘保留原有原子提交与保守后台简化。
- 更宽的岩区、更密的块、更多同时命中的块仍可能增加内存和耗时；预算相同不等于所有场景帧率保证相同。
- 开发者地图增加棕色岩区、蓝色主路和紫色岩塞。显示的是**生成布局快照**，并非实时挖掘后形状；挖穿岩塞后仍保留其设计位置标记。

## 复现验证

仅在非 Play 状态下，从编辑器底部 `Cmd` 输入：

```text
py E:/UE5/UEProjects/BrainMining/Scripts/verify_hollow_region.py
py E:/UE5/UEProjects/BrainMining/Scripts/verify_hollow_editor.py
py E:/UE5/UEProjects/BrainMining/Scripts/run_hollow_region_runtime.py
```

分别验证生成/碰撞/参数编辑与失败回退，自动生成及撤销重做，以及真实角色行走与 `TryMine` 挖穿三塞。不要同时运行这些脚本；它们只用于本演示地图，几何测试会保存恢复后的演示状态，不适合直接运行在已经重新策划过的关卡上。

`Scripts/setup_hollow_region.py` 是一次性创建脚本；地图已存在时不会覆盖你的编辑。原生自动化测试为 `BrainMining.HollowRegion.*`；报告位于 `Saved/Validation`。
