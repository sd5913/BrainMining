# 地下通路实心减去模型

`Descending_Tunnel_Cutter.blend` 打开后进入独立场景 `Descending_Tunnel_Cutter`，其中只有一个通路网格 `SM_DescendingTunnel_Cutter`。原始默认场景保留在另一个场景中。

`SM_DescendingTunnel_Cutter.fbx` 只导出最终通路网格，可导入 UE 作为矿区生成器的减去体。Blender 使用米；FBX 写入单位信息，UE 导入缩放使用 1。模型没有外围岩体、壁壳、房间、堵路体或隔断。主路与支路已经融合，所有端点封闭。

- 主路中心线长约 292.93 米，连续下降 88 米，具有多次横向蜿蜒和回折。
- 7 条支路按主路长度较均匀分布，长度约 31.84–49.41 米，各自下降 13–23 米。
- 主路直径约 4.5–5.7 米；支路与主路粗细接近，末端逐渐收束并采用圆润封口。
- 整体包围尺寸约 147.11 × 141.03 × 98.71 米。
- 最终网格约 8.21 万顶点、16.43 万三角面；缩放与旋转为单位变换。

`validation.json` 记录实际网格验证结果：单连通体、闭合流形、欧拉示性数 2，无开放边、重复面、退化面、法线绕序错误和检测到的自交；中心线全部严格下降。另在临时测试块上执行了真实的 EXACT 布尔差集，差集结果封闭且移除了约 9646 立方米体积，测试块已删除。

生成算法先检查非连接处的管段间距，再通过体素并集和表面平滑形成一个表面，最后优化网格。`centerlines.json` 保存中心线及半径，`generate_tunnel.py` 可在 Blender Python 中重新生成，`validate_tunnel.py` 和 `save_tunnel.py` 分别负责验证与导出。

用于挖空矿区时，将主路的两端伸出矿区不可挖外壳，减去体才会在外壳上形成入口与出口。当前通路的两端本身保持封闭，以满足布尔输入要求。

## Lowpoly 岩石版本

`Descending_Tunnel_Cutter_Lowpoly.blend` 与 `SM_DescendingTunnel_Cutter_Lowpoly.fbx` 为单独保存的风格化版本。当前场景 `Descending_Tunnel_Lowpoly` 只显示一个封闭实心减去体，原始平滑版本仍保留。

表面加入两种尺度的不规则岩石起伏，沿原表面法线的位移限制在 ±0.55 米内，再形成大块切面并采用逐面法线。最终约 4,766 顶点、9,528 三角面。主干路径、7 条支路和全部封口保留。几何起伏可传递到布尔差集后的洞壁；预览用的岩石色材质不会自动替代 UE 矿区的洞壁材质。

`Tunnel_Lowpoly_Preview.png` 展示整体，`Tunnel_Lowpoly_Detail.png` 展示分支处的切面与起伏。`validation_lowpoly.json` 记录新版的封闭性、连通性、自交及真实布尔差集验证。`stylize_lowpoly.py` 与 `save_lowpoly.py` 可重复生成、预览与导出。
