# BrainMining：第一人称挖掘原型

UE 5.8.2 / Windows / C++ / 单机。基于原生 Dynamic Mesh 和球形布尔减法，不依赖 MCP 运行游戏，也不需要第三方体素插件。

## 运行

打开 `BrainMining.uproject`，默认进入 `/Game/MiningPrototype/Maps/Lvl_MiningLab`，点击 Play。
原始 `/Game/FirstPerson/Lvl_FirstPerson` 保留。测试地图单独指定 `MiningPrototypeGameMode`，其他地图的 GameMode 不变。

| 操作 | 按键 |
| --- | --- |
| 移动 / 观察 | WASD / 鼠标 |
| 跳跃 | Space |
| 连续挖掘 | 按住鼠标左键 |
| 回出生点并恢复岩体 | R |
| 切换实际切割范围预览 | F1 |

靠近棕色岩体，准星变青色时可以挖掘。目标是挖穿约 10 米深的岩体，走到背后的青色门框。
场地地板和边界不可挖。挖掘变化只存在于本次运行中，不写回关卡，不保存到存档。

## 可调参数

角色的 MiningComponent：

- Reach：400 cm。
- Radius：160 cm；原型优先保证第一人称角色通行。
- Interval：0.3 s；按住左键限频执行，不逐帧做布尔运算。
- InsetFraction：0.3；球心沿命中法线向岩体内偏移。
- DownwardOffset：40 cm；切割中心向下偏移，避免视线高度挖出的洞留下过高门槛。
- Show Preview：预览和实际切割使用同一套中心计算。

DiggableRock_Main：

- RockSize：1000 × 1000 × 600 cm。
- MaxTriangles：30000；超预算时拒绝编辑，R 可重置。
- RockMaterial：岩体材质。修改 RockSize 会重建初始岩体。
- 仅支持正数统一缩放；需要不同长宽高时修改 RockSize，不要非均匀缩放 Actor。

## 实现

- `Source/BrainMining/MiningComponent.*`：Enhanced Input、准星检测、频率限制、预览和安全重置。
- `Source/BrainMining/DiggableRock.*`：封闭岩体、复用切割球、FDynamicMesh3/FMeshBoolean、法线和同步碰撞重建。
- `Source/BrainMining/MiningHUD.*`：准星、操作提示、三角形和切割耗时。
- `Source/BrainMining/MiningPrototypeGameMode.*`：复用现有第一人称角色和控制器，指定原型 HUD。
- `Scripts/create_mining_lab.py`：生成测试地图的编辑器脚本；若地图已存在会拒绝覆盖。

布尔运算在副本上执行。结果必须通过网格有效性、封闭性和三角形预算检查才能替换当前岩体；失败时保留原网格和碰撞。
岩体使用 complex-as-simple 碰撞，不模拟自身刚体运动。挖断的悬空岩体不会自动掉落。

## 构建

关闭编辑器后运行（按实际引擎安装路径调整）：

```powershell
& 'E:\UE5\UE_5.8\Engine\Build\BatchFiles\Build.bat' BrainMiningEditor Win64 Development '-Project=E:\UE5\UEProjects\BrainMining\BrainMining.uproject' -WaitMutex -NoHotReloadFromIDE -MaxParallelActions=4
```

Windows Development 包输出到 `Builds/MiningPrototype/Windows/BrainMining.exe`。
MCP 和 AllToolsets 在项目描述中限定为 Editor；它们不会成为游戏运行依赖。命令行 Cook 额外禁用这两个编辑器工具插件，避免 AllToolsets 引入的 GameFeatures 资产扫描配置错误影响 Cook。

## 验证与边界

验证输出保存在 `Saved/Validation`，构建和打包日志在 `Saved/Logs`。

编辑器实机通行验证通过：使用真实第一人称角色和 CharacterMovement，6 次有效切割后，角色从 x=-200 行走至 x=1279，穿过岩体；最终 1664 个三角形，该轮单次编辑约 1.8–6.2 ms。报告为 `Saved/Validation/mining_runtime_report.json`。

几何验证覆盖：切割前的射线/胶囊阻挡、连续开洞后的射线/胶囊通行、重置碰撞、完全删除岩体、非法半径、非均匀缩放拒绝、旋转岩体和 100 次确定性压力操作。

本机无渲染几何压力测试：100 次请求，99 次成功，1 次因布尔结果无效被安全拒绝；最终 26486 个三角形。失败请求检查了网格计数和成功次数未被修改。
该测试平均每次约 30 ms，最大约 65 ms，包含复杂度很高的后期岩体；这不是游戏平均帧时间或帧率保证。

当前是技术原型，持续大量挖掘仍会增加同步切割/碰撞耗时。后续应根据真实玩法负载评估分块、简化或体素替代，不应直接扩展为无限地形。
尚未实现：矿物资源、经济系统、爆炸连锁、坍塌、多人同步及存档。

## 隐藏雾气房间

测试岩体的 `FogChamber` 组件现在启用了封闭空腔：400 × 480 × 380 cm，局部中心 `(0, 0, -90)`。房间地面比场地高 20 cm，角色可进入。靠近岩体本身不会触发特效，必须实际挖掘。

- 封闭：房间有内部积雾，外面看不到。
- 渗漏：挖掘面与房间间的剩余岩壁小于 `SeepDistance`（140 cm）时，从该挖掘面向外缓慢冒雾。每个渗漏点持续约 12 秒，最多保留 4 处。
- 喷涌：成功切割后，用当前网格的双面射线检查空气通道，并连接历史切割空腔；只有房间真正连到岩体外部才释放。仅切割球理论范围接触房间、或单独开出封闭内部空腔都不算挖通。
- 外溢：第一次破壁在实际连通处喷出雾气，默认 4 秒内衰减为持续少量外溢；继续挖掘不会重复满强度爆发。
- 重置：R 同时恢复岩体、封闭房间、释放状态并清除旧雾粒子。

可在关卡中选择 `DiggableRock_Main` → `FogChamber`，调整 `RoomSize`、`RoomOffset`、`SeepDistance`、`BurstDuration` 和 `FogMaterial`；尺寸、位置更改后点击岩体 `Reset Rock`。房间每边至少 100 cm，且各方向至少保留 10 cm 岩壁，否则禁用空腔并提示参数错误。

效果采用原生 C++ 管理的最多 192 个实例化软粒子，程序化材质提供软边、细节扰动和深度淡化，无外部贴图或 Niagara 依赖。喷流粒子中心会做碰撞检测；这不是体积流体模拟，粒子边缘不能精确贴合所有洞口，也不会模拟气体绕过复杂弯道、按洞口面积分配多出口流量或气压守恒。目前只从首次连通的出口释放。

- `Source/BrainMining/FogChamberComponent.*`：空腔、连通关系、渗漏点、状态机和雾粒子。
- `Scripts/setup_fog_chamber.py`：创建雾材质并配置测试地图；会重建专用 `M_ChamberMist` 材质，不改其他材质。
- `Scripts/verify_fog_chamber.py`：编辑器几何回归验证。
- `Scripts/verify_fog_runtime.py`：新启动的 PIE 中验证初始状态、真实角色进入/穿出、压力衰减和重置。

本机验证结果：初始房间 24 个三角形；靠近时实测剩余岩壁约 96 cm 会渗漏，仍剩约 3 cm 时不会提前喷发；内部孤立切割不触发，连接外部后才触发。真实角色通过 4 次有效切割走过房间并穿出岩体，压力正常衰减，R 清除旧粒子并恢复封闭状态。报告位于 `Saved/Validation/fog_chamber_geometry.json` 和 `fog_chamber_runtime.json`。
