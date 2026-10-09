# 镐子模型更新

`Content/Pickaxe/SM_Pickaxe` 已替换为桌面 `pickaxe.fbx` 中的新模型，保留原资源路径。FBX 里的两个网格合并成一个静态网格体，两个材质槽保留；新材质和嵌入贴图放在 `Content/Pickaxe/Revised`，可以直接在 UE 编辑。

模型与贴图源文件在 `SourceAssets/Pickaxe`。模型导出时存在位置偏移和旋转，因此在导入几何中对齐到原来的持镐姿态；没有缩放变形。导入平移和旋转已记录在资源的 FBX Import Data 和 `SourceAssets/Pickaxe/import_alignment.json` 中。OpenGL 法线贴图已翻转绿色通道适配 UE。

`CF_PickaxeSwing`、`MiningToolEffectsComponent`、摄像机挂载位置、网格组件旋转和缩放、挖掘命中时序均保留。更新前的镐子资源备份在 `Saved/PickaxeReplacement`；原动画及效果代码的 SHA256 校验保持一致。

可运行 `Scripts/verify_mining_gui.py` 和 `Scripts/verify_mining_tool_effects.py`，检查 GUI 交互、新镐子运行、原挥动曲线、挖掘、碎屑、冷却及复位。验证只操作临时 PIE 世界，不保存测试状态。
