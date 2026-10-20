# 2026-10-20 结尾血管与细胞房调整

已将 main 同步到 `7f08a8d6`，同步前的本地修改保存在 `Saved/Backups/EndingArt-before-sync-20261020-160132/`，并已合回工作区。

## 正式地图

`/Game/EndingEmergence/Maps/Lvl_EndingEmergence_Demo`

- 只替换细胞房布景，导入 `/Game/EndingEmergence/Maps/Lvl_EndingEmergence_Demo2` 中的 37 个模型对象，保留源模型、材质、世界位置、旋转和比例。
- 在 World Outliner 的 `Ending_Edit/01_Cell/Demo2` 中继续调整布景。新对象仍挂在细胞段的显隐层级内，不会残留到后续镜头。
- 删除原有红色圆门，使用 Demo2 的 `SM_Thrombus3` / `MI_thrombus` 制作真正可挖掘的血栓门。它使用闭合的原模型网格，镐子直接切割新模型，不叠放旧门或静态模型。
- 大脑、街景、镜头轨迹、后续尺度变化、视频和谢幕保留现状。MainGame 地图未修改。

## 漂浮操作

仅在结尾细胞房启用无重力移动。W/S 沿视线前后移动，A/D 横移，抬头或低头即可改变移动高度。松开后逐渐减速，保留碰撞与沿墙滑动。

空格和 Ctrl 不控制升降。进入血管时结束此移动模式，恢复既有的血管自动前进和 W 加速逻辑；不修改主流程喷气背包规则。

## 血管美术

- 管壁源自 `/Game/BloodCirculation/Mesh/BloodVessel/SM_Vein`。源模型本身有完整弯曲路径，因此抽取其中连续管段，保留内外壁及 UV，适配为 `/Game/EndingEmergence/Meshes/SM_EE_BloodCirculationVeinSegment`。
- 30 个连续 spline 管段沿原有 241 个路径点铺设；入口管口与新房间的开口少量重叠，避免露缝。旧管壁和肋骨仅保留必要碰撞/流程锚点，关闭其渲染，不会在重播时重新露出。
- 130 个血细胞替换成 `/Game/BloodCirculation/Mesh/Cell/SM_RedBloodCell`。按原有尺寸归一化比例，不改变种子位置、脉动流速、绕流偏移、循环回收或镜头跟随。
- `M_Vein` 开启 SplineMeshes 用途，保证打包后使用正确材质。

一次性导入脚本：`Scripts/import_ending_demo2_cell.py`。它拒绝覆盖已导入地图；不要用旧生成脚本重建正式地图。
血管管段适配脚本：`Scripts/adapt_ending_blood_mesh.py`，也拒绝覆盖已有派生资产。
运行验证：`Scripts/verify_ending_blood_cell_art.py`，输出到 `Saved/Validation/EndingArt/`。

## 验证结果

- Development Editor 编译通过。
- PIE 交互验证通过 34 项检查（`Saved/Validation/EndingArt/runtime.json`）：仅 WASD/视角的悬浮操作、无重力停留、松手减速、空格/Ctrl 不升降、新门实际镐击切割、穿门切换、血管自动前进、W 加速和到达大脑。
- 路径为原 241 点、血细胞原始流动种子 130 个；与原数学路径的最大数值误差约 0.00095 cm。
- 37 个 Demo2 布景对象已导入；装饰使用 NoCollision 配置，只有外壁和可挖门保留碰撞。
- 验证启动通过 `-ddc=InstalledNoZenLocalFallback` 绕过本机 Zen 缓存线程崩溃，不修改项目运行配置。
- 截图：`01_cell_room.png`、`03_clot_excavated.png`、`05_blood_flow.png`、`06_brain_arrival.png`，均在 `Saved/Validation/EndingArt/`。
