# MainGame 挖掘关卡

关卡：`/Game/MainGame/Maps/Lvl_MainGame`。已使用独立的 **外部通路矿区生成器** 生成并保存；现在包含四段通路，相邻两段之间各有一处 18 米房间预留连接区。第二段说明见 [README_MainGame_CavernV2.md](README_MainGame_CavernV2.md)，第三段说明见 [README_MainGame_StalagmiteV3.md](README_MainGame_StalagmiteV3.md)，第四段说明见 [README_MainGame_NeuralV4.md](README_MainGame_NeuralV4.md)，下文记录第一段参数。关卡尚未放入其它玩法房间。

模型是重新设计的单段通路：主路约 171.25 米，下降 55 米，名义宽度约 3.3–9.8 米；4 条短支路长约 16.6–17.5 米。通路切割体为一个封闭连通实体，16 个独立堵塞块约占完整切割体空腔体积的 49.62%。主路最后约 10.2 米、各支路最后约 5 米保持空腔。

## UE 中使用

- `Content/MainGame/Maps`：新关卡。
- `Content/MainGame/Models/Tunnel`：通路切割体。
- `Content/MainGame/Models/Blockers`：16 个阻塞模型，保留共同坐标。
- `Content/MainGame/Blueprints`：独立角色和 GameMode，角色带头灯，`FallResetDistance=0`，正常向下走不会因低于出生点而回传。

选择关卡中的 `EDIT_MainGame_外部通路矿区`，在顶部 **工具 → 外部通路矿区生成器** 中调整参数。岩层尺寸为 125 × 44 × 70 米，基岩厚度 1.5 米，16 米分块；生成 120 个岩层分块和 16 个引导可挖层（共 17 个分块）。入口和出口同时穿透岩层、基岩外壳。

通路修改后用生成器重新生成矿区及引导层。只修改阻塞模型时可单独重新生成引导层。游戏中 `R` 恢复岩层与阻塞层，并回到出生点。

角色和 GameMode 从已有外部矿区测试蓝图复制到 MainGame；材质复用现有 Campaign 岩石、引导岩块和头灯材质，不修改其资源。

## Blender 源文件

`Art/MainGame/TunnelV1/MainGame_Tunnel_V1.blend` 包含 CUTTER、BLOCKERS 两个集合，保持共同原点。`Tunnel_Cutter.fbx` 只包含通路切割体，`Tunnel_Blockers.fbx` 只包含 16 个独立堵塞模型。`MainGame_Blockers_Placement.png` 为布局预览。

使用米制，UE 导入启用 Convert Scene Unit、关闭 Combine Meshes 和 Remove Degenerates。生成脚本为该目录的 `plan_main_game.py`、`build_main_game.py`；旧通路文件保持独立。

## 验证

Blender 检查：通路单一连通、封闭、正体积；通路和全部堵塞块无非流形边、重复面、退化面及检测到的自交。沿阻塞块内部抽查截面贴合；从通路减去同一原始包络得到 17 个空腔连通区，验证 16 处完整封堵；全部 5 个末端保持留空。

`Saved/Validation/main_game_creation.json`：独立创建与保存成功，入口/出口开放，出生点有实体地面。

`Saved/Validation/main_game_reload.json`：重新加载保存的关卡，不重新生成，16 个引导块逐一通过碰撞、挖掘、重置；主岩层挖掘、恢复和不可挖外壳碰撞通过。考虑入口/出口伸出矿区的部分后，矿区内部实际空腔的堵塞比例约 **53.92%**，处于提示词的 45%–55% 范围。

`Saved/Validation/main_game_runtime.json`：独立 PIE 使用 `BP_MainGameCharacter`，真实行走约 72 cm 后遇到岩塞，现有 TryMine 成功挖掘引导层，手动重置恢复；头灯开启，角色下降约 33.84 米后未回传。该检查验证基本玩法，不等同真人从头挖完全部通路。
