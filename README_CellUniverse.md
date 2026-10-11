# 细胞宇宙 Demo

第一人称、可挖掘的结尾流程原型。独立地图：`/Game/CellUniverse/Maps/Lvl_CellUniverse_Demo`。

## 试玩

双击项目根目录 `Play_CellUniverse_Demo.cmd`，或在 UE Content Browser 打开 `Content/CellUniverse/Maps/Lvl_CellUniverse_Demo` 后 Play。
启动器默认使用 `C:\UE_5.8`；其他电脑可设置环境变量 `UE_EDITOR` 指向自己的 `UnrealEditor.exe`。
源码更新后先构建 `BrainMiningEditor / Win64 / Development`。没有修改项目默认启动地图。

| 操作 | 按键 |
| --- | --- |
| 移动、观察 | WASD、鼠标 |
| 挖穿边界 | 按住左键，靠近至准星变青 |
| 上升、下落 | 按住空格、松开空格 |
| 从头重玩 | R |
| 切割范围调试预览 | F1 |

沿悬空岩脊上的金色痕迹前进。每段尽头只有发亮的局部开口可挖，岩脊和边框不可挖。
可以离开宽 6.5 米的岩脊向下观察，再按住空格飞回来；路面下方没有铺满空间的隐形地板。
下方有细胞器、红细胞、组织、地貌，以及上下环绕的星球、星系和漂浮岩块。各段底部保留远低于岩脊的安全边界，最深在路面下 360 米。
无须新学按键，也无须在编辑器开启自由飞行。移动、飞行和挖出的洞均保留真实碰撞。

## 十一段内容与节奏（V02）

1. 细胞内：细胞核、核仁、细胞器、纤维轮廓和第一层细胞膜。
2. 血液：浮动红细胞；随前进由大缩小。
3. 组织：细胞群、肌肉纤维、毛细血管。
4. 生物体：有头、触角、六条腿和鞘翅的小虫，缓慢缩小。
5. 大陆：山脉、雪峰、河流、海岸；下方还有大块陆地和海面。
6. 星球：直径 840 米的巨大星球延伸至路面下方，保留卫星和近处小岩块作为参照。
7. 星系：直径约 1.42 公里的三旋臂星系，保留太阳和小星球作跨阶段参照。
8. 宇宙：多组远处星系、连接的丝状结构；星系分布在上下左右，近处岩块与远处天体形成视差。
9. 细胞膜：重复开场的膜纹理和细胞核，星空仍在；真实切割触发材质波纹亮度反馈。
10. 巨人的血液：洞口附近再次出现巨大红细胞；左侧细胞轮廓内保留星系，确认嵌套关系。
11. 巨人：舌面、上下牙齿、嘴部轮廓和最后的堵塞。成功切割推进“挖 / 挖！ / 挖！！”，穿过后抵达桌面场景。

| 阶段 | 路线长度 | 可挖开口（宽 × 高） |
| --- | ---: | ---: |
| 细胞内 | 36 m | 4 × 3.6 m |
| 血液 | 48 m | 4.8 × 4 m |
| 组织 | 56 m | 6 × 4.8 m |
| 生物体 | 70 m | 8 × 6.5 m |
| 大陆 | 100 m | 14 × 11 m |
| 星球 | 145 m | 22 × 18 m |
| 星系 | 170 m | 30 × 24 m |
| 宇宙 | 220 m | 42 × 32 m |
| 细胞膜 | 65 m | 6.5 × 5.5 m |
| 巨人的血液 | 105 m | 5.5 × 5 m |
| 巨人 | 70 m | 8.5 × 8.5 m |

路线共 1085 米，纯步行与挖掘目标约 3–4 分钟，停留观察和下探飞行会更长。移动速度保持原样。
前 35% 的段落保持地标大小，之后的缩小受距离和最短时间双重限制；星球、星系、宇宙主体保持完整尺寸，不再迅速缩成小球。
小尺度段的开口上下、左右都有不可挖边框，飞行也不能从边界下方绕过。
字幕使用中文，前段对话仍是时间驱动草稿，快速穿过时可能跳过部分对话。

这是流程和尺度辨识 Demo：采用长短不同的相连展示段落，角色、镜头及碰撞体不缩放；尚不是全环境无缝分形缩放。
宇宙区域隐藏了房间外壳的渲染，以保留远处和下方的完整天体；仍有较远的侧面、顶部和底部碰撞边界，演示不是无限空间。
当前细胞膜为可布尔切割的平面厚膜，弧形纤维作空间暗示；波纹是材质亮度动画，没有软体物理。
末尾保留口腔出口及桌面接入点，尚未加入真人拍摄、配音、从嘴里喷出的镜头或全局路线回放。场景使用程序化基础模型，供下一轮确定美术和节奏。

## 复用与实现

- `MiningPrototypeGameMode` 继续提供原有 `BP_FirstPersonCharacter`、`BP_FirstPersonPlayerController` 和 Enhanced Input。
- `MiningComponent` 负责射线、距离限制、连续挖掘、R 重置及掉落恢复；新增 `OnFieldReset` 通知，让演示表现同步重置。不重绑这些输入。
- `DiggableRock` 负责所有出口的真实球形布尔减法、网格预算及碰撞重建。不用“点击后整面墙消失”代替挖掘。
- `MiningCharacterMovementComponent` 负责原有空格上升和松开下落；只有这个演示场景主动启用飞行。
- `MiningCellUniverse` 负责十一段几何、装饰动画、字幕和进度，专用 GameMode 只替换 HUD。
- 进度必须同时满足前一处岩壁成功切割、角色实际跨过该处出口。侧壁和顶部封闭，不能绕过关卡。
- 场景、材质均在 `Content/CellUniverse`；已有地图、蓝图、飞行和挖掘参数资产未改动。

## 调整与生成

关卡中选中 `Cell Universe - eleven suspended scale stages`，在 `Stages` 中逐段修改长度、空间宽高、路面下方深度、开口宽高/厚度、缩放终值与最短变化时间，再点击 `Build Demo` 并保存。
`Apply Expanded Preset` 恢复上表预设，之后仍需点击 `Build Demo`。`Built Stages` 和 `Stage Starts` 是生成时保存的快照，单独编辑参数不会造成运行时进度与旧碰撞错位。
该按钮只重建此 Actor 创建的边界和装饰，修改关卡前应停止 Play。保持演示 Actor 的缩放为 1。
门框和通道尺寸、配色、模型摆放在 `MiningCellUniverse.cpp`；材质表达式在 `Scripts/create_cell_universe.py`。

`create_cell_universe.py` 用于在新工程副本中首次生成资产：若地图已经存在或编辑器有未保存资产，会拒绝覆盖。
`upgrade_cell_universe.py` 用于明确升级已有 Demo：先将旧地图备份到 `Saved/Backups/CellUniverse-时间戳`，再重建本 Demo 的生成内容。不会覆盖其他地图。
运行示例（PowerShell，路径按机器调整）：

```powershell
& 'C:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' (Join-Path $PWD 'BrainMining.uproject') "-ExecutePythonScript=$(Join-Path $PWD 'Scripts\create_cell_universe.py')" -unattended -RenderOffscreen '-DisablePlugins=AllToolsets,ModelContextProtocol'
```

## 已验证

编辑器目标编译通过；`BrainMining.CellUniverse.FullFirstPersonRoute` 在重新加载保存地图后通过。
测试使用真实 W、鼠标、Space、左键、R 输入：行走与转向、飞行与松手下落、顶部碰撞、空中挖掘、未挖膜阻挡、十一段连续步行开洞、终点完成和整场重置；还覆盖离开悬空岩脊下落、在下方空间飞行返回，以及局部开口上下不能绕过。
整段通行测试不传送角色、不关闭碰撞、不直接改进度。V02 报告：`Saved/Validation/CellUniverseV02/index.json`。
`BrainMining.CellUniverse.ScaleVistas` 单独检查星球、星系、宇宙及返回细胞时的地标显示，确认巨型天体不会遮挡其他段落，星球尺寸保持不变；这项表现测试使用指定观察位置。
已有 `BrainMining.Flight` 三项回归测试也通过，报告：`Saved/Validation/CellUniverseFlight/index.json`。
运行截图：`Saved/CellUniversePreview/`。这些本机验证产物不提交。

```powershell
& 'C:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' (Join-Path $PWD 'BrainMining.uproject') /Game/CellUniverse/Maps/Lvl_CellUniverse_Demo '-ExecCmds=Automation RunTests BrainMining.CellUniverse' '-TestExit=Automation Test Queue Empty' "-ReportExportPath=$(Join-Path $PWD 'Saved\Validation\CellUniverseV02')" -unattended -RenderOffscreen -NoSound '-DisablePlugins=AllToolsets,ModelContextProtocol'
```
