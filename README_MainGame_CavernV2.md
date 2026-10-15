# MainGame 第二段溶洞通路

当前关卡使用 [RoutesV2 立体路线](Docs/MainGameRoutesV2.md)，洞厅扩宽并增加至 12 根岩柱。下文记录保留的原始溶洞源模型。

关卡仍为 `/Game/MainGame/Maps/Lvl_MainGame`。第一段保留，沿其出口方向追加了第二段挖掘区域；两段之间有独立的房间预留连接空腔。

新主路约 176.80 米，下降 55 米，4 条支路各约 17.47 米。常规截面保持上一版的量级，中部两处扩宽为约 13.4 米宽、10.4 米高的溶洞，每处有 3 根连接顶底的 Lowpoly 岩柱。新切割体体积约 8965.27 立方米，比第一段约增加 23%。16 个独立阻塞块的合计体积约 4409.93 立方米，占新通路 49.19%；岩柱旁的中央空腔和全部主支路末端留空。

## 关卡内编辑

- `EDIT_MainGame_外部通路矿区`：第一段；保留原有 16 个引导层，并额外用连接体切开出口交界。
- `EDIT_MainGame_02_溶洞通路矿区`：第二段；160 个可挖岩层分块、16 个引导层（19 个分块）、1.5 米不可挖外壳。
- `EDIT_MainGame_连接预留区_18m`：独立的连接区，8 个岩层分块，没有阻塞层。
- `RESERVE_未来房间中心_18m连接空腔_勿放阻塞层`：未来房间的位置标记。

房间预留段沿 X 轴连接旧出口 `(6500,0,1000)` cm 与新主路入口 `(8300,0,820)` cm，长 18 米，中部约 12 米宽、8 米高；两端连续收束到原通路。它先提供带地面和顶壁的通行空腔，未来可替换为房间。尚未放入其它玩法房间。两段都使用独立的外部通路矿区工具，保留基岩外壳。

第二段模型在 UE 的共用平移为 `(14800,0,-5680)` cm；阻塞模型跟随第一个切割体变换。连接体同时参与三个区域的切割，避免内外岩层重新封住交界；修改位置后需对受影响的矿区重新生成。

## 源模型

`Art/MainGame/TunnelCavernV2/MainGame_Tunnel_Cavern_V2.blend` 保存 CUTTER、BLOCKERS、FUTURE_ROOM_CONNECTOR 和岩柱参考集合。

- `Tunnel_Cutter.fbx`：一个封闭连通的通路切割体，6 根岩柱对应 6 个贯通孔，28440 个三角形。
- `Tunnel_Blockers.fbx`：16 个独立阻塞块，共 31214 个三角形。
- `FutureRoom_Connector.fbx`：辅助连接切割体；独立导出便于今后替换房间。

岩柱已经扣进切割体，UE 布尔挖空后留下的是可挖岩层本身。Blender 中的 `REFERENCE_ROCK_COLUMNS_NOT_EXPORTED` 只是完整柱体参考，不导入 UE。参考柱体延伸到通路外以确保穿透；布局图展示的是参考体。`MainGame_Cavern_Interior.png` 则渲染实际切割体的内表面。

生成脚本：`plan_cavern.py`、`build_cavern.py`，复用第一段的闭合管体及贴合阻塞块生成方法。`preview_cavern_interior.py` 只生成预览图，不修改源文件。UE 接入脚本为 `Scripts/append_main_game_cavern.py`，通过已有 `GenerateRegion`、`GenerateGuideLayers` 生成，完成后拒绝再次追加同名区域。

## 验证与恢复

Blender 验证全部切割体、堵塞块封闭，无检测到的自交、重复面和退化面。通路网格的 Euler 特征为 -10，符合 6 个贯通岩柱孔；减去堵塞块包络后为 17 个分离空腔，验证了 16 处封堵。

`Saved/Validation/cavern_creation.json` 记录生成和保存结果、6 根岩柱碰撞，以及两段交界之间的 61 个地面采样。

`Saved/Validation/cavern_reload.json` 在独立进程重新加载保存的关卡，不重新生成，验证新引导层逐一碰撞、挖掘及恢复，岩柱和外壳实体碰撞、接缝开放、预留区无阻塞和末端留空。

`Saved/Validation/cavern_runtime.json`：无渲染的独立 PIE 测试通过。角色真实行走约 25.19 米，跨过旧出口、房间预留区和新入口；两段引导层挖掘及 R 恢复、头灯开启状态和深处停留均通过。这验证连接与基本玩法，不等同从头挖完全部关卡，也没有验证渲染后的照明画面。首次额外渲染试玩进程在执行测试前因系统虚拟内存不足退出，改用小启动关卡及 NullRHI 后成功完成测试。

追加前的关卡备份在 `Saved/Backups/MainGame_BeforeCavernV2/Lvl_MainGame.umap`。第一段 Blender 和 FBX 文件保留在 `Art/MainGame/TunnelV1`。
