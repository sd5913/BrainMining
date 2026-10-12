# 炸弹道具

主线关卡 Lvl_MainExperiences 实际控制 /Game/Showcase/Blueprints/BP_SharedMiningCharacter，它已继承角色父类中的 BombTool、BombMount、BombModel。按 G 拿出炸弹，再按左键扔出一枚；扔完回到镐子。再次按 G 可收起，按 V 可切到现有钻机。G 收起钻机时保留其原有计时，无法借切换清除钻机冷却。按住 Tab 的升级界面、暂停和遭遇交互会阻止投掷输入。

飞出的炸弹使用 Chaos 刚体、重力、球形碰撞和连续碰撞检测，碰到地面或墙壁会反弹。三秒引信从实际投掷开始计时，碰撞不会重置引信。爆炸时模型立即消失。附近角色受到带距离衰减的向外冲量和抬升，随后由角色移动组件处理重力与碰撞；附近物理物体也受到径向冲量，实体墙会阻挡冲击。没有接入炸弹冷却、库存或伤害。

默认爆炸改为 /Game/Bomb/FX 的低多边形火团和烟团：火焰约半秒，低矮灰烟约两秒，短暂闪光；取消大型写实 Niagara 系统的默认绑定。参考包保留，Explosion Asset 允许美术另外指定 Niagara 系统。矿洞中的爆炸还会在多个岩面接触点生成带岩层材质的物理碎片和散开的扬尘。

炸弹与独立爆炸标记为 BM_PlayerEffect，展示大厅的区域隐藏规则跳过这些玩家道具。这样投掷途中跨过区域边界也不会突然隐形，爆炸的烟雾和闪光仍按自身生命周期结束。

爆炸按最终物理位置挖掘 ADiggableRock 和 GeneratedMiningRock，包括 Lvl_MainExperiences 的矿洞岩体及独立阻塞层。默认挖掘半径 520 cm，比基础镐子 160 cm 的半径约大 3.25 倍。破坏体由同一随机种子生成宽窄不一的裂解瓣和多边形断面，跨分块共享世界空间形状；底层沿用现有可靠的网格差集，不是多个光滑球洞的叠加。切割先验证封闭性和三角形预算，再同时更新受影响分块与碰撞；失败保留原岩体。冻结的矿道段保持原有权限限制。R 恢复原始地形。

爆炸前向四周采样岩面及材质，仅为成功挖开的岩体生成碎片。对于按世界位置区分引导层的材质，分离碎片时固定爆炸点的岩层颜色，避免碎片飞到别处后变色。碎片数量默认 72，上限 96；只碰撞地形，不阻挡玩家。

在 BP_SharedMiningCharacter 的组件中选 BombMount 可直接调手持位置、角度；BombModel 可更换模型和材质。运行时只切换模型可见性，不重写位置。投掷模型会居中到自身的碰撞体。其他关卡使用的 BP_FirstPersonCharacter 也继承同一套组件，但在主线中请修改 BP_SharedMiningCharacter。

在 BombTool → Mining / Bomb → Settings 可调 Fuse Seconds（默认 3 秒）、Throw Speed（1100 cm/s）、Throw Up Speed（220 cm/s）、Collision Radius（10 cm）、Mass（1.2 kg）、Blast Radius（700 cm）、Blast Velocity（1200 cm/s）、Blast Lift（300 cm/s）、Explosion Scale（1.25）。Explosion Asset 可换其他 Niagara 系统。

新增 Excavation Radius（520 cm，地形破坏半径，独立于冲击波 Blast Radius）、Crater Roughness（0.35，断面不规则程度）、Debris Count（72）、Debris Lifetime（3.5 秒）、Dust Strength（1.4）、Fire Strength（1）、Smoke Strength（1）。Last Excavated Rocks 和 Last Debris Count 记录最近一次爆炸结果，便于排查。

模型与材质位于 /Game/Bomb，原始 FBX 保存在 SourceAssets/Bomb/bomb.fbx。给定 GUI/8.png 原图完整导入为 T_BombAbility，并用于 WBP_AbilityG，放在左下方钻机 V 图标左边；图内已包含 G 键标记，所以原控件的重复按键文字隐藏。炸弹图标不显示红色禁止或冷却数字。热量仪表及四级升级 UI 保留。

2026-10-12 已替换桌面新版 bomb.fbx（11965 三角形），按材质槽名继承原来的 Stylized Metal Base、sparks-rot、Sparks-blau、材质四个资产，没有重新生成材质或纹理。保持 BombMount 的美术位置，只补偿新版模型约 0.04 cm 的中心差异。旧模型与 FBX 保存在 Saved/Bomb/BeforeNewModel。

Scripts/import_bomb_assets.py、copy_bomb_vfx.py、restore_bomb_ui.py 通过 UE Python 导入和恢复资产。Scripts/verify_bomb_tool.py 在当前主线关卡的临时测试场地发送真实 G、左键、V 和 Tab 输入，验证重力、碰撞、引信时长、近远距离冲量、连续使用和 UI 交互；结果和截图位于 Saved/Validation/Bomb，测试场地只在 PIE 中存在。改动前备份保存在 Saved/Bomb/Before。

后续风格化与矿洞修改的源文件备份为 Saved/Bomb/BeforeTerrain。Scripts/create_stylized_bomb_fx.py 创建独立爆炸材质和小型多面体烟团；create_bomb_debris_uv.py 生成专用碎片副本，保留矿洞中无 UV 白岩的外观，不改变镐子或钻机使用的原碎片资产；replace_bomb_model.py 替换 FBX 并保留材质。Scripts/verify_bomb_excavation.py 在主线第一个矿洞体验区验证真实投掷、岩层碎片、不规则挖掘、碰撞和 R 重置，结果位于 Saved/Validation/BombTerrain。BrainMining.Bomb.FracturedCavityAndCollision 验证破坏体封闭性、随机性、跨块体积一致、冻结段保护与重置。
