# 钻头模式

主关卡现已接入[宝物与养成规则](TreasureProgression.md)：V装备/收起，使用能量并在收起后恢复，满级无限能量。以下10秒使用/10秒冷却规则仅适用于未启用养成的独立地图。

在原有镐子挖掘角色上增加钻头，不替换角色、关卡或 GUI。

- 默认使用镐子。按 **V** 启用钻机，持续 10 秒后自动收起。
- 钻头模式下按住 **鼠标左键** 播放旋转动画并持续挖掘，松开停止。
- 切换武器有 0.22 秒的抬起动作；钻头运行时带轻微震动。
- 启用及收起会取消正在按住的挖掘，需要重新按下左键。按住 Tab 操作升级面板时会停止挖掘，暴露鼠标并锁住视角；松开恢复。
- 10 秒使用窗口结束后进入 10 秒冷却：V 图标显示原来的红色禁止版本，中央叠加 10→1 的倒计时。冷却结束恢复正常图标并隐藏数字。重复按 V 不能延长使用时间，也不能跳过冷却。
- **Q 不再切换武器**。G 的原有图标交互保留。只添加冷却数字，金币及价格位置继续留空。没有过热逻辑。

## 可编辑资源

`Content/Drill` 内的 `SM_Drill`、`M_Drill`、`M_Drill_Body`、新 FBX 嵌入的四张金属纹理以及 `CF_DrillSpin` 都可在 UE 编辑。2026-10-11 替换为桌面最新 FBX：两个网格合并，两个材质槽分别保留法线和金属/粗糙度纹理、两倍 UV 平铺以及源文件的底色。没有沿用旧版的漫反射贴图。保留 16 个旋转关键帧和 0.5 秒的连续旋转曲线，网格长度统一为 58.6 厘米。

原文件副本、规范朝向后的网格 FBX、提取的贴图、动画关键帧 JSON 和 CSV 保存在 `SourceAssets/Drill`。桌面的原始 FBX 不会被改写。`Scripts/prepare_drill.py` 和 `Scripts/import_drill.py` 用于重新准备和导入这些新资源。

当前 **`/Game/ExternalMining/Maps/Lvl_ExternalMining_Demo`** 关卡实际运行的角色是 **`/Game/ExternalMining/Blueprints/BP_ExternalMiningTestCharacter`**。该关卡的 `BP_ExternalMiningTestGameMode` 指定了这个角色。它与 `/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter` 是两个独立蓝图；修改其中一个不会自动修改另一个。其他关卡应先检查 World Settings 中 GameMode 的 Default Pawn Class，再编辑对应角色。

打开当前实际角色蓝图的 **视口**，展开 **First Person Camera** 下的道具组件：

- **DrillRightMount / DrillLeftMount**：右、左钻机的摆放位置。选中后可在视口中用 W 移动、E 旋转、R 缩放，也可以编辑右侧 Transform。修改会保存到蓝图，不会被运行时动画覆盖。
- **DrillRightModel / DrillLeftModel**：各自的 Static Mesh 和材质可直接替换；也可调整模型的局部位置、旋转和缩放。左模型原本 Y 为负缩放，用于镜像。
- **PickaxeMount / PickaxeModel**：同样用于镐子的摆放与更换模型。
- **PickaxeMotion / DrillRightMotion / DrillLeftMotion**：程序播放动画的内层组件；调摆放时选择 Mount 或 Model。

选中模型后按 **F** 可聚焦。蓝图视口显示镐子与双钻机用于编辑；运行游戏仍默认显示镐子，按 V 后才显示钻机。编辑正在运行的角色实例也可立即预览，但永久修改应在停止运行后的蓝图中保存。转动与抬起动作写入独立 Motion 子组件，Mount 和 Model 的美术调整保留。钻机旋转轴跟随 Model 的本地 X，不绕 Mount 的原点画圈。即使在视口里移动或倾斜 Model，轴心仍跟随模型。

选中 **MiningToolEffects**，在 **Mining > Drill > Animation** 下可调整 `DrillSpinAxis` 和 `DrillSpinPivot`。两者都采用网格局部坐标：当前模型轴向为 `(1,0,0)`，轴心为 `(0,0,0)`（FBX 原点）。替换成其他轴向或偏心原点的模型时可修改这两个参数。当前旋转曲线、左右镜像方向、材质和美术摆放均保留。

**MiningToolEffects** 的 `bDualDrills` 可关闭左侧运行显示，其他特效参数仍在该组件中。旧 `DrillLocation/Rotation/Scale` 和 `DrillMeshAsset` 已移到折叠的 **Legacy Runtime Pose** 分类，仅给其他类型 Pawn 的兼容路径使用。当前角色请使用可见组件调整。首次迁移保留了用户已保存的右侧 X=52.026047、左右旋转/缩放、灰尘强度 1.154663 与灰尘颜色，不重置为初始姿态。

冷却数字位于 **`/Game/UI/MiningGUI/WBP_AbilityV` 的 `AbilityContent > CooldownText`**，是可编辑的 UMG 文字层。角色 **Mining** 组件的 `DrillUseDuration` 和 `DrillCooldownDuration` 控制两个时长；`ActivateDrill` 返回是否成功启用，`GetDrillUseRemaining/GetDrillCooldownRemaining` 提供剩余时间供程序接入。

## 挖掘与碎屑

钻头继续调用现有挖掘系统，不另建地形算法。默认挖掘间隔为镐子的 0.4 倍，半径为 1.2 倍；角色 **Mining** 组件中的 Drill 参数可调。切回镐子会恢复关卡原有的间隔和半径。

每次真实挖掘成功，钻头随机喷出 50–62 块碎屑，镐子仍为 30–42 块。碎屑继续使用被挖表面的材质，并随机选择原有碎片网格，带物理喷溅和碰撞。钻头碎屑默认存活 1.2 秒，同时最多 6 组，避免连续挖掘时无限累积。空挥不产生土块。

钻头产生三层接触效果，默认每次约 19–29 个随机尘团、8–12 个翻页烟尘、10–14 个火星。尘团沿表面法线喷出并减速膨胀；烟尘缓慢上浮、扩散并渐隐；火星受重力影响，接触静态表面后反弹衰减。默认接触寿命为原值的 1.35 倍，烟尘约 1.5–2.2 秒，火星约 0.4–0.9 秒。最多保留 12 组接触效果，空挥不会产生任何接触粒子。每次数量和运动轨迹均随机变化。

`M_DrillDust`、`M_DrillSmoke`、`M_DrillSparks` 的材质节点均可编辑。烟尘使用用户提供 Realistic Starter VFX Pack Niagara 的 `T_Smoke_Wisp` 8×8 图集，火花使用 `T_Spark_1`；仅复制所需纹理，保留原特效项目。原细尘层保留 Concrete Debris VFX Pack 图集。使用实例化网格实现局部粒子轨迹，未复制整套 Niagara 系统。

组件的 `Mining > Drill > Dust` 下可调浓度 `DrillDustStrength`、火花量 `DrillSparkStrength` 及备用颜色。地表材质提供 `BaseColor`、`SoilColor`、`Color` 或 `Tint` 参数时，尘团自动跟随；否则使用备用泥土色。`Scripts/create_drill_contact_fx.py` 用于最初创建材质与冷却文字层；日常编辑直接修改资源和可见组件，无需重新运行生成脚本。

新增调节入口：实际角色蓝图中选中 **MiningToolEffects**，在 Details 搜索 `Drill`。**Dust** 分类中的 `DrillDustParticleCount` 控制细尘平均数量，`DrillDustSize` 控制尺寸；**Smoke** 分类中的 `DrillSmokeParticleCount`、`DrillSmokeStrength`、`DrillSmokeSize` 分别控制烟团数量、浓度和尺寸；**Sparks** 分类中的 `DrillSparkParticleCount`、`DrillSparkSize`、`DrillSparkSpeed`、`DrillSparkBrightness` 控制火星数量、尺寸、速度和亮度。**Effects** 分类的 `DrillContactLifetimeScale` 控制消散时长。数量设置为 0 可关闭相应层，关闭细尘或烟雾不会关闭火星。修改蓝图需 Compile/Save；运行中修改实例可预览但不会永久保存。

当前两个角色蓝图保存了比 C++ 默认值更明显的一套参数：细尘平均 30、烟团平均 12、烟雾浓度 1.5、烟团尺寸 1.4、火星平均 16、火星尺寸 2.5、火星亮度 24。平均数量上下随机浮动 20%。原有摆放、旋转、缩放与备用泥土颜色都保留。

用户最新导入的 `SM_Drill` 有三个材质槽：`drill_press_01`（元素 0）使用 `M_Drill_Body`，`Stylized Metal Base_002`（元素 1）使用 `M_Drill`，`Stylized Metal Base`（元素 2、原先缺材质的尖端）使用 `M_Drill_Body`。这两个材质保留用户接好的最新贴图。`OcclusionRoughnessMetallic` 混合贴图关闭 sRGB、使用 Masks 压缩，R/G/B 分别连接 AO/Roughness/Metallic。材质与网格位于 `/Game/Drill`，可以在 `SM_Drill` 编辑器的 Material Slots 中调整，或者在角色蓝图选中左右 `DrillModel` 后修改 Materials 的元素。

## 验证

`Scripts/verify_drill_revision.py` 在临时 PIE 世界中验证真实 V/左键输入、双钻机、可编辑位置、接触效果、挖掘量、Tab 鼠标交接及完整 10 秒使用/10 秒冷却；不保存关卡。运行结果和截图写入 `Saved/Validation/DrillRevision`。

改动保留在本地工作目录，未提交、未 Push，未切换分支，已有 stash 保留。可视组件迁移前的角色蓝图与源码备份保存在 `Saved/Drill/ViewportEditingBackup`，其中 `BP_FirstPersonCharacter-current-saved.uasset` 包含本次开始时用户在 UE 中尚未保存的参数调整。

2026-10-11 本次验证：UE 5.8.3 编辑器编译通过；钻机时限/冷却自动测试通过；42 项钻机游戏输入/UI 检查和 75 项原有升级面板检查通过。烟尘单独截帧并与隐藏接触粒子的画面对比，中央区域超过 13 万个像素明显变化，确认可见。原有 124 个资源中，仅 `WBP_AbilityV` 因新增冷却文字层改变，其余 123 个 SHA-256 不变。

随后可视化组件改动：编译通过，52 项实际游戏与编辑器组件检查通过，包括无须运行就存在的模型、镜像双钻机、直接编辑位置/比例/朝向与模型不被运行时覆盖、镐子动画、实际挖掘粒子、Tab 和完整冷却。迁移保存后另启编辑器读取验证，右侧 X=52.026047 的用户位置仍保留。

以上可视化测试使用 FirstPerson 角色，未覆盖 ExternalMining 关卡指定的独立角色。随后修正：通过 UE5.8 原生 MCP 将用户在 FirstPerson 蓝图中新调整的镐子与左右钻机 Mount/Model 的位置、旋转、缩放和模型/材质设置同步到实际 ExternalMining 角色，编译并仅保存该角色蓝图。没有更换 GameMode 或修改角色原有逻辑。当前 ExternalMining 关卡实际启动 PIE 后，确认生成 `BP_ExternalMiningTestCharacter_C`，24 项道具设置全部匹配保存后的蓝图。记录为 `Saved/Drill/active-character-runtime.json`，修改前备份为 `Saved/Drill/ActiveCharacterBackup`。

材质与效果加强验证：UE5.8.3 编译通过，镐子及左右钻机在两个蓝图中的位置、旋转、缩放保持不变。当前 ExternalMining 关卡真实挖掘测试测到每次 26–32 个细尘、13 团烟尘、16–17 颗火星；关闭烟尘后的独立火星也已验证。最终 63 项资源、参数及姿态检查通过。报告与截图在 `Saved/Validation/DrillMaterialFX`，修改前材质、贴图、网格、两个角色蓝图及相关源码备份在 `Saved/Drill/MaterialFXBackup`。未提交或 Push。

旋转轴修复验证：编辑器编译通过，旋转轴/轴心与原有钻机时限测试均通过。实际关卡 48 项检查通过，包括左右模型偏移后不绕挂载点公转、临时倾斜和移动 Model 后轴线仍稳定、连续旋转以及松开停止。轴线上测点的最大变化约 0.156 厘米，来自原有轻微震动。两个角色蓝图保存的道具变换保持不变，`SM_Drill` 和 `CF_DrillSpin` 资源与修改前逐字节相同。报告在 `Saved/Validation/DrillSpinAxis`，备份在 `Saved/Drill/SpinAxisBackup`；运行时测试改动没有保存到关卡或蓝图。
