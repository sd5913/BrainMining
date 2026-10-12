# 风格化钻机与道具准心

2026-10-12，主体验关卡的实际角色是 `/Game/Showcase/Blueprints/BP_SharedMiningCharacter`。

## 模型与材质

新版桌面 `drill.fbx` 在独立 Blender 场景中处理，原文件与用户打开的 Blender 场景均未修改。材质采用黄漆外壳、深灰固定环和银灰金属钻头，包含色差、粗糙度变化及轻微表面细节。程序化材质烘焙成 BaseColor、Roughness、Metallic 和切线 Normal 四张贴图，UE 内使用标准 PBR 材质，不依赖 Blender 节点。法线绿通道在 UE 导入时翻转。

- UE 网格与材质：`/Game/Drill/Stylized/SM_DrillStylized`、`M_DrillStylized`。
- Blender 可编辑源文件：`Art/Drill/Stylized/Drill_Stylized.blend`，贴图已打包。
- 导出文件和贴图：`SourceAssets/Drill/Stylized`。
- 处理与导入脚本：`Scripts/prepare_stylized_drill.py`、`Scripts/import_stylized_drill.py`。

左右钻机以及 FirstPerson、ExternalMining、SharedMining 三个角色蓝图都使用新版网格。保留每个角色原有 Mount/Model 的位置、旋转与大小。准备后的模型轴线为本地 X，轴心为零，长度与旧模型一致；原有 `CF_DrillSpin` 逐字节保留。此版桌面 FBX 没有动画 Action，因此继续播放项目现有的旋转曲线。

随后按用户要求，将 `CF_DrillSpin` 改为 0 秒=0°、0.5 秒=360° 的两关键帧线性曲线，即每秒两圈。原导入数据的前后段斜率不同，虽是线性插值，仍会反复加减速；现在各段及循环边界均为 720°/秒。既有旋转轴、左右方向和输入控制不变。`Scripts/set_drill_uniform_spin.py` 可重建该曲线；修改前备份和验证记录在 `Saved/Drill/UniformSpinBefore` 与 `Saved/Drill/UniformSpin`。重导风格化模型时保留当时的曲线，不恢复旧曲线。

## 拿起过渡

镐子、炸弹和双钻机共用 `MiningToolEffects.ToolEquipDuration`，默认 0.22 秒，切换时从下方抬起并轻微转正。只写入 Motion 动画节点，视口摆放仍调整 Mount 或 Model。

炸弹新增 `BombMount → BombMotion → BombModel` 层级。物理投掷、粘附、三秒引信、倒计时灯光以及挖掘爆炸逻辑保留。G 收回炸弹和投掷后返回镐子都触发拿起过渡。

## 准心与过热叠层

`G.zip` 的 10、11、12 分别对应镐子、钻机和炸弹。源图保存在 `SourceAssets/Reticles`，资源位于 `/Game/UI/MiningGUI/Reticles`。UI 材质直接用灰度遮罩黑色背景，原始图案保留；镐子图案的原始 16 像素中心偏移由 UV 修正。

在 `/Game/UI/MiningGUI/WBP_MiningHUD` 的 Designer 中选中 `ReticlePickaxe`、`ReticleDrill` 或 `ReticleBomb` 调整大小。它们居中锚点与对齐均为 (0.5,0.5)，位置为零，默认画布 64×64；考虑原图留白后实际图案约 34–39 像素（1080p）。运行时根据炸弹装备/钻机模式自动切换；升级菜单及暂停时隐藏。旧 Canvas 准心仅在新 HUD 不存在时作为兼容备用。

HUD 内 `JetpackHeatBar.WarningMaxOpacity` 从 0.55 调至 0.68，可在 Designer 继续调整。过热图片仍为 `JetpackHeatWarning`，ZOrder=-100，低于准心与其他 HUD 控件，不拦截输入。原有 0.25 秒淡入、0.45 秒淡出、3 秒冷却和热量回落不变。

## 验证与备份

编辑器编译记录在 `Saved/DrillStylized/build.log`、`Saved/Reticles/build.log`。`Scripts/verify_stylized_drill.py` 验证主关卡实际角色、双钻机材质、拿起过渡、旋转轴稳定及可编辑姿态不变；记录在 `Saved/Validation/DrillStylized`。

`Scripts/verify_tool_reticles.py` 验证真实 V/G/Tab/Space 输入、准心切换与居中、菜单隐藏、过热叠层强度、层级与冷却淡出，记录在 `Saved/Validation/ToolReticles`。测试临时改动仅作用于 PIE，不保存到关卡。

修改前资源备份位于 `Saved/DrillStylized/Before`、`Saved/Reticles/Before`。这些改动未提交或 Push。
