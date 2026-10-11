# 连续玩法地图（六区 + 宝物支路）

分支：`integration/feature-showcase`。入口：`/Game/Showcase/Maps/Lvl_ContinuousShowcase`。

## 地图结构

六种玩法的场景 Actor 全部放在同一张地图的 Persistent Level 中，启动时就存在。没有运行时子关卡流送，没有选区目录，没有进入区域时的传送或角色重建。

另追加第七个宝物挖掘展区，从基础挖掘区西南开口沿支路步行到达。宝物配置 Actor 和展示场地保存在同一 Persistent Level；埋藏宝物与岩块在开局生成，不是进入区域时加载。操作与配置见 `README_TreasureShowcase.md`。

用同一个第一人称角色沿青色步道走进各区即可体验。区域边界只管理交互、UI、挖掘/飞行许可和角色灯光，不加载或卸载场景。离开后保留收集、挖掘、谜题和本次试玩的完成标记，不是持久存档。

此前的 2～6 项不再设展区：房间推进、空洞支路、多矿区串联、二维钢笔矿道、三维隧道。底层代码和原地图仍保留。

## 路线与玩法

出生点位于南北主步道，面向欢迎牌。世界 +Y 是北侧，+X 是各房间方向。

| 区域 | 怎么到达 | 玩法 |
| --- | --- | --- |
| 01 挖掘与雾室 | 从出生点向南，到挖掘场右转 | 左键挖掘，打通封闭雾室 |
| 02 收集图鉴 | 从出生点向北，第一个大入口 | 挖掘四类藏品，E 收集，Tab 三维图鉴，空格飞行 |
| 03 回声迷宫 | 穿过挖掘场旁的坡道，步行上坡 | 地板坍塌后左键敲击辨路，出口接实体回程步道 |
| 04 卡牌房 | 主步道最北端，直接穿门 | E 入座、点击选牌、空格确认、Q 离座 |
| 05 弹窗电脑 | 收集区继续向北的第二个入口 | 瞄准电脑按 E，点击关闭弹窗，触发实体窗口喷发 |
| 06 文思 | 弹窗区继续向北，卡牌房之前 | E 思考、G 编辑、交换词语、Q/R 放下思考；解谜后穿出口绕回主步道 |
| 07 宝物挖掘 | 挖掘区西南开口继续向南走 | 九种切面矿物、埋藏彩色光斑；挖出超过 50%，E 领取金币；金币跨区保留 |

回声出口不再自动切换卡牌地图；玩家从回程步道自行走去卡牌房。卡牌保留原型的入座/离座位置调整，这不是区域间传送。卡牌演示奖励仍未接入收集图鉴。

## 运行与快捷键

编译 `BrainMiningEditor Win64 Development` 后重新打开工程，默认进入连续地图。也可以从工程根目录运行：

```powershell
.\Scripts\Play_Showcase.ps1
.\Scripts\Play_Showcase.ps1 -Editor
```

- WASD/鼠标：移动与观察。
- F2：显示/隐藏路线及当前区提示，不打开目录、不锁住角色。
- F8：退出当前图鉴、桌面、文思编辑或卡牌入座，留在本区，不返回大厅。
- F5：重开当前区，不清空其他区。回声重开会回到迷宫入口；文思须先走回入口再重开，避免障碍物夹住角色。
- 各区离开时关闭本区 UI 并释放输入，保留场景与进度；卡牌主动离座按原玩法结束本轮。
- 跌出地图时恢复到最近的安全落脚点，不清空进度；这仅用于跌落保护，不是正常区域切换方式。

## 实现与边界

- `Source/BrainMining/Showcase/MiningContinuousShowcase.cpp`：区域检测、交互接管与退出、局部重置。
- `Scripts/create_continuous_showcase.py`：创建六区、门洞、坡道和步道，已存在时拒绝覆盖。
- `Source/BrainMiningEditor/Private/MiningShowcaseImport.cpp`：仅在制作时复制原房间 Actor 到 Persistent Level，过滤全局灯光、后期和出生点，不保存或修改源地图。
- 收集区保留原型世界原点，其余区域移位。文思通关判断使用相对坐标，避免平移后误通关。
- 弹窗自身全局天空/太阳在此地图禁用，使用局部灯光；喷发窗口定时回收，避免长时间试玩持续累积。
- 卡牌区通过共享角色相机恢复原型的曝光补偿（-6），离开后恢复为 0，不改变其他区域曝光。
- 旧 `Lvl_FeatureShowcase` 保留为回归参考，不是当前启动地图。

这是玩法整合灰盒，不是最终美术/剧情关卡。中文字体、音频、Cook 资源范围和成品包仍需分发前验证；编译 Shipping 不等于已打包。

## 自动化验证

`BrainMining.Showcase.Continuous.WalkAllSixRegions` 使用真实 PIE 角色移动沿步道跑完六区和迷宫，不靠传送跳过门洞或坡道。检查同一 Pawn、单 Persistent Level、零流送关卡、收集跨区保留、输入释放、文思平移后的解谜出口、回声出口不切图以及 F5/F8 行为。

结果目录：`Saved/Automation/ContinuousShowcase/`；截图：`Saved/ContinuousPreview/`。以 JSON 报告为准，不只看 UE 进程退出码。

### 验证记录（2026-10-10，Asia/Shanghai）

- Win64 Editor Development 与 Shipping 均编译通过，包括最终曝光和提示文字修正。
- 完整回归 `BrainMining+CardRoom+EchoPassage`：69 项通过、0 失败、0 跳过；其中 3 项带既有警告。报告：`Saved/Automation/ContinuousFullRegression/index.json`。
- 连续地图真实步行测试已通过：同一角色走完六区、完整迷宫及回程步道，并验证实际交互与跨区域状态保留。
- 最终曝光修正后专项回归 `BrainMining.Showcase.Continuous.Appearance+CardRoom+EchoPassage`：6 项通过、0 失败、0 警告。报告：`Saved/Automation/ContinuousAppearance/index.json`。该视觉测试使用测试内位置调整检查卡牌曝光及离区恢复，不替代真实步行测试。
- 已检查卡牌房最终截图；完整 69 项回归运行于最后曝光和提示文字修正之前，未宣称完整 70 项重新运行。
- 尚未执行 Cook 或成品打包；Shipping 编译结果不代表已验证分发包。
