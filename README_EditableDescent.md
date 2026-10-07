# 可编辑的向下挖掘关卡

关卡：`/Game/MiningPrototype/Maps/Lvl_MiningDescent_Editable`。

原来的 `Lvl_MiningDescent` 和 `Lvl_MiningLab` 保留不替换。生成脚本在新关卡已存在时拒绝覆盖，手动修改不会因为重新打开地图而被重建。

## 场景结构

START → 通道 A（12°下坡）→ ROOM 1 → 通道 B（18°下坡）→ ROOM 2 → 通道 C（22°下坡）→ ROOM 3。

尺寸单位为厘米。通道内部宽 600、高 440；三段长度分别为 700、900、1000。每段两端预留 60 的无岩石过渡区，可挖岩体之间不互相重叠。房间保留空地，供后续加入矿物、事件、装置和敌人。此图不把原雾室组件当成完整房间系统，也没有新增气体玩法。

## 在大纲中调整

建议先用 `File → Save Current Level As` 另存一份，再做较大的结构改动。

- 所有新增对象都在 `Descent` 文件夹。`00_START` 到 `06_ROOM_3` 对应七段。
- 名字以 `MOVE_WHOLE_SECTION` 结尾的 TargetPoint 是整体移动手柄。移动或旋转它，附属墙壁、可挖岩体、标识和灯光一起跟随。
- 展开手柄的子对象，可以单独移动墙、改变普通墙体的 Scale、替换静态网格、调整灯光或删除装饰。没有合并成不可拆的整块模型。
- 名字含 `DIGGABLE_EDIT_RockSize` 的对象是可挖岩体。调整 Details → Mining → Rock Size，不要非均匀缩放它或它的父手柄；父手柄的 Scale 保持 1,1,1。
- 这套结构不是自动衔接的样条工具。拉长通道时，需要相应调整地板、顶板、两侧墙、填料和下一间房的位置；改变坡度后也要对齐相邻入口，并实际走一遍。
- 普通填料关闭了 FogChamber。不要随意打开它，否则会在填料内部额外生成一个原型空腔。

## 剖面查看

完成时只在编辑器中临时隐藏屋顶和近侧墙，方便看到里面。Play 时这些墙和碰撞仍存在，不是开放侧面的假通道。

在编辑器底部 Cmd 输入以下命令切换剖面：

```text
py "E:/UE5/UEProjects/BrainMining/Scripts/toggle_descent_cutaway.py"
```

也可以在大纲中用眼睛按钮显示或隐藏 `Roof_EDITOR_HIDE`、`NearWall_EDITOR_HIDE` 对象。隐藏眼睛只用于编辑器查看，不要修改 Actor Hidden In Game 或关闭碰撞来模拟剖面。

重新打开地图后，如果想一键回到全图剖面视角，在 Cmd 运行：

```text
py "E:/UE5/UEProjects/BrainMining/Scripts/show_editable_descent.py"
```

该命令仅设置编辑器可见性、相机位置并保存当前新关卡，不改变场景布局。

## 材质与预留空间

材质实例位于 `/Game/MiningPrototype/Materials/EditableDescent`。打开 `MI_` 资产可调整 BaseColor、Roughness、Glow，不必改材质图。固定围岩为蓝灰色，可挖填料为赭色；发光门框和地面细线没有碰撞。

房间文字、门框、地面线条都是独立对象，可自由替换。几何均为灰盒，便于先调整空间，再替换美术。

## 运行与限制

- 打开此图，Play；WASD 移动，鼠标观察，按住左键挖掘，Space 跳跃，R 重置。
- 挖洞后地面可能留有球形切割产生的岩石边缘；不要只盯着远处继续挖，可以调整瞄准清理附近岩石或跳过去。这是现有挖掘原型的地面形态，不是所有开口都已自动整平成路。
- 没有替换项目默认启动地图，也没有更新旧的 Windows 可执行包；旧包仍是之前的雾室原型。
- 现有代码在下降超过初始位置 2000 cm 后重置。本图总下降约 800 cm，不触发；扩展得更深之前要先修改这条测试逻辑。
- 一次挖掘仍只修改命中的一块岩体，因此这些独立岩段用房间隔开，不建议直接重叠可挖岩体。
- 当前挖掘和碰撞仍是原型同步流程，不新增存档、矿物、检查点或真实坍塌。

## 文件与备份

- `Scripts/create_editable_descent.py`：首次创建脚本，不用于覆盖已有手工修改。
- `Saved/Validation/editable_descent_layout.json`：初始布局、对象路径和尺寸记录。
- `Saved/EditableDescentBackup/2026-10-04`：原地图保存前、保存用户修改后的备份，以及原场景对象清单。

## 验证记录（2026-10-04）

- 在真实 UE 编辑器中完成几何检查：7 个分段手柄、3 块可挖岩体、119 个独立场景对象；附件跟随、固定墙碰撞、挖掘及重置均通过。
- PIE 实测使用正常角色移动、瞄准挖掘和跳跃，依次进入三个房间并到达末端，没有沿途传送或关闭碰撞。三段分别挖了 4、5、5 次，并跳过一次残岩边缘。
- 验证 R 对应的重置逻辑能恢复三块岩体并将玩家送回起点。
- 本次测试最大单次挖掘耗时约 4.94 ms，仅是当前电脑、当前灰盒和当前切割次数下的记录，不代表正式关卡性能上限。
- 报告保存在 `Saved/Validation/editable_descent_geometry.json` 和 `Saved/Validation/editable_descent_runtime.json`。

如需复测，在打开的新关卡中先执行几何检查，完成后再 Play 执行运行检查。两条命令分别是：

```text
py "E:/UE5/UEProjects/BrainMining/Scripts/verify_editable_descent.py"
py "E:/UE5/UEProjects/BrainMining/Scripts/verify_editable_descent_runtime.py"
```

不要同时运行它们。运行检查会接管本次 PIE 的角色约十几秒，完成后自动停止控制；不会修改保存的地图。几何检查会重置编辑器中的三块岩体，先保存希望保留的编辑内容。静态碰撞检查应在真实编辑器内执行，不使用未初始化关卡物理场景的无界面 Python commandlet 代替。
