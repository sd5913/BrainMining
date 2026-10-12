# FullJourney 的 Blender 空间编辑

`FullJourney_Editable.blend` 是从当前 UE 关卡导出的空间快照。包含可挖岩层，以及已加载房间的静态模型、道具和相对位置。交互逻辑、灯光效果、运行时生成内容和 UE 材质图不包含在模型快照中。

当前主矿道来自外部通道模型的裁切；入口、出口和出生点封口是 **FullJourney 中独立的 GeneratedMiningRock**，没有合并进原始 Blender 通道模型。V2 对衔接块和附近主岩层做一致的局部形变。房间仍是源关卡实例，修改房间源关卡会同步影响 FullJourney。

## 在 Blender 中修改矿道

1. 打开 `FullJourney_Editable.blend`。`EDIT_Terrain` 是可编辑岩层，`REF_Rooms_and_Props` 是禁止选择的房间参照集合。对象属性中保存了对应 UE Actor 的路径。
2. 修改需要调整的岩块，保持封闭体和对象身份。相邻岩块的共用边界要一起移动；不要把全部地形合并成一个对象，也不要删除内部封口面。保留房间入口净空。
3. 选择修改的岩层对象。在 Blender 文本编辑器中打开项目 `Scripts/blender_campaign_exchange.py`，保持 `MODE='export'` 并运行。结果写到 `Edited/`，只输出选中的岩层。
4. 在 UE 中打开 FullJourney，通过 Python 执行 `Scripts/import_campaign_edited_rocks.py`。它替换对应 Actor 的几何，重新生成可挖网格、碰撞和必要的分块。修改纳入撤销事务，不会自动保存地图。
5. 试玩修改区域后保存。大幅移动通路时，还需调整 UE 中矿物、藏品、出生点和区域触发范围；模型文件不能推导这些玩法意图。

回导修改的是 FullJourney 的烘焙岩层，不会反向改写原通道切割体。再次运行整关卡生成脚本会重新生成这些岩层，因此应保留 Blender 工作文件和 `Edited` 中的修改，以便重新应用。

文件坐标已经统一：Blender 米坐标 = `(UE.X, -UE.Y, UE.Z) / 100`。配套脚本自动处理单位、轴向、对象变换和三角面，不要再额外旋转 90 度或缩放 100 倍。

## 更新空间快照

在 UE Python 中获取 FullJourney 的 MiningExhibitHall，并调用：

```python
unreal.MiningDesignerLibrary.export_campaign_layout(hall, unreal.Paths.project_dir()+'Art/Mining/FullJourneyExchange/Source')
```

然后用 Blender 后台运行：

```powershell
& 'C:/Program Files/Blender Foundation/Blender 3.2/blender.exe' -b --python Scripts/blender_campaign_exchange.py -- build Art/Mining/FullJourneyExchange
```

重新生成快照会替换 `FullJourney_Editable.blend`；未回导的 Blender 修改应另存文件。`Source/layout.json` 保存模型与 UE Actor 的对应关系，`Edited/edits.json` 保存本次选中的回导对象。

修改房间形状建议继续针对源房间关卡的模型进行 FBX 回导；改变房间在旅程中的分布需要同时调整 `Config/CampaignLayout.json`/关卡实例位移和衔接关系。不要把空间快照整体替换成一件静态网格，否则会失去可挖性与房间实例的编辑关系。

官方格式参考：[Blender OBJ 坐标与导入导出](https://docs.blender.org/manual/en/3.6/files/import_export/obj.html)、[UE FBX 静态模型流程](https://dev.epicgames.com/documentation/en-us/unreal-engine/fbx-static-mesh-pipeline-in-unreal-engine)。本项目使用配套 OBJ 脚本来保留可挖 Actor 的对应关系。
