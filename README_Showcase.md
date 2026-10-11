# 功能整合展示馆

> 此文档记录旧版“大厅 + 流送展区”。当前默认地图和启动脚本已改为 `Lvl_ContinuousShowcase`，详见 [六区连续地图](README_ContinuousShowcase.md)。旧关卡仅保留为回归参考。

## 打开与操作

原始分支：integration/feature-showcase，现已整合到 main。入口地图：/Game/Showcase/Maps/Lvl_FeatureShowcase。本文保留旧展示大厅的设计与验证记录；当前版本与整合状态见 [main 整合说明](README_MainIntegration.md)。

先按项目 README 编译 `BrainMiningEditor Win64 Development`，再打开入口地图并 Play。
也可从工程根目录执行：

```powershell
.\Scripts\Play_Showcase.ps1
.\Scripts\Play_Showcase.ps1 -Editor
.\Scripts\Play_Showcase.ps1 -EngineRoot 'E:\UE5\UE_5.8'
```

| 操作 | 行为 |
| --- | --- |
| 大厅目录按钮 | 进入指定展区 |
| F2 | 在大厅开关目录；展区内提示返回方式 |
| WASD / 鼠标 | 使用原第一人称角色移动和观察 |
| 展台附近 E | 进入对应编号展区 |
| F5 | 卸载后重新加载当前展区，完全重置本区玩法 |
| F8 | 从任意展区返回大厅，包括图鉴暂停、入座、弹窗和文思编辑期间 |
| 各玩法原按键 | 保持原定义，见屏幕右上方提示 |

F2/F5/F8 由展示关卡专用输入预处理器接收，避免被 Slate 图鉴或弹窗吞掉。
停止 PIE 时会注销，不影响原有地图或编辑器快捷键。
打开图鉴、卡牌入座、弹窗桌面或文思编辑时，展示馆的大块提示自动隐藏，仅保留右下角不拦截鼠标的 F5/F8 提示，避免挡住原玩法按钮。

## 十一个展区

| 编号 | 展示内容 | 原地图 |
| --- | --- | --- |
| 01 | 基础挖掘、碰撞、雾室 | Lvl_MiningLab |
| 02 | 按住空格飞行、房间推进、分块矿道 | Lvl_MiningDescent_AutoRooms |
| 03 | 空洞、隐藏入口、支路 | Lvl_MiningHollowBranches_Demo |
| 04 | 多矿区与主房间串联 | Lvl_MiningHollowChain_Demo |
| 05 | 二维钢笔矿道样例 | Lvl_MiningManual_Demo |
| 06 | 三维隧道、连续岩体、统一基岩、三维地图 | Lvl_MiningSpatial_Demo |
| 07 | 四类藏品、五件物品、物理释放、三维图鉴 | Lvl_Collection_V03 |
| 08 | 坍塌入口、黑暗回声迷宫 | Lvl_EchoMazeToCards_P1 |
| 09 | 入座、四轮卡牌演出、占位奖励 | Lvl_CardRoom_P1 |
| 10 | 点击关闭弹窗、加速生成、实体窗口喷发 | Lvl_PopupClose |
| 11 | 思考两个物体、交换属性词、打开出口 | Lvl_WenSi |

每区入口播放独立字幕数据资产，位于 `/Game/Showcase/Subtitles/`，可用“工具 → 字幕工具”修改。
挖掘实验室保留原跳跃设置；飞行主要在矿区和收集区体验，不强行修改原地图参数。
二维钢笔、三维隧道、矿业策划台属于离线制作工具，在停止 Play 后打开对应源地图使用。

## 关卡组织与隔离

这是一个持久展示大厅，加一个按需装载的体验区，始终使用同一个游戏世界。
十一处实体展台是体验入口，不是把十一张大地图同时铺在一个常驻场景内。
这种分区避免原型的全局曝光、绝对坐标、全世界 Actor 搜索及输入绑定互相影响，也控制岩体内存。
切换不调用 `OpenLevel`，大厅、控制器、参观记录始终保留；原地图以动态流送实例装载，不修改原地图文件。

- 所有展区使用原 `BP_FirstPersonCharacter`。文思仅在本次体验中调整胶囊尺寸、步高与速度，以保持谜题约束。
- 进入/退出时重建角色，让关卡生成器在 BeginPlay 正确提供人物比例、飞行和挖掘参数，不将收集区参数带入其他地图。
- 回声、卡牌、弹窗、文思作用域禁挖、禁飞并关闭矿工灯光；离开后恢复。
- 回声和卡牌交互组件仅在相应展区创建，退出时销毁 UI、计时器、镐子及入座锁。
- 收集退出会关闭图鉴、解除暂停、移除输入上下文，并销毁动态物品、场地、图鉴模型。
- 电脑退出会清理桌面 UI、恢复镜头和输入。展示馆还跟踪并销毁该体验产生在持久层的临时 Actor，包含飞出的窗口。
- 字幕切换会停止当前语音、计时器及排队字幕，不把上一房间的提示带进下一房间。
- 大厅曝光限定在大厅盒形范围内，不污染体验区。

回声出口会卸载回声区、装载卡牌区，保留原来的“回声 → 卡牌”体验，但不离开展示世界。
卡牌、收集、文思和弹窗可以记录本次试玩的完成状态；地形样例只记“已体验”，不伪造通关目标。
每次进入或 F5 都重新开始该展区。目录的参观/完成标记保留至结束本次 Play，不是持久存档。
卡牌奖励仍是本地占位奖励，不接入收集图鉴，不宣称已经完成正式奖励和剧情推进系统。

## 合并来源与保留策略

| 来源 | 本次核对提交 | 整合内容 |
| --- | --- | --- |
| origin/main / CHENHongfan | ff1bab3 | 挖掘公共底座、角色、矿区、策划台 |
| Huang-Weiyu | e20e34b | 二维/三维编辑、多矿区、统一外围、三维地图 |
| Shen-Yilang | b6c3eda | 字幕、弹窗、文思 |
| linziyi | 32065ff | 收集、回声、卡牌、共享角色策略 |

公共 Build.cs 合并依赖，编辑器模块同时注册地形工具和字幕工具；几何辅助函数使用 Huang 分支的独立命名，避免 Unity 编译冲突。
`Lvl_MiningSpatial_Demo` 使用与三维工具同步的 Huang 版本。Shen 同名版本保留在原分支，另外备份到本机 `Saved/IntegrationSources/Shen-Yilang/`，未把两个二进制地图强行合并。
没有恢复或删除本地 stash，也没有改各成员分支。EchoPassage 源码插件不携带编译得到的 DLL/PDB，首次使用需编译。

为让回声资产在渲染/打包路径使用正确材质，补存了三个材质的 Instanced Static Mesh 使用标记。
电脑窗口的质量设置移到实例启动阶段，避免在原生 CDO 构造期访问尚未初始化的物理引擎。

## 实现入口

- `Source/BrainMining/Showcase/MiningShowcase.h/.cpp`：展区配置、流送状态、角色适配、快捷键、完成事件。
- `Source/BrainMining/Showcase/MiningShowcaseWidget.cpp`：中文目录和展区 HUD。
- `Scripts/create_feature_showcase.py`：创建大厅、十一处展台和字幕资产；已存在地图时拒绝覆盖。
- `Scripts/finalize_feature_showcase.py`：保存回声材质 ISM 标记及展示大厅原生组件。
- `Source/BrainMining/Tests/MiningShowcaseTests.cpp`：共享角色、逐区进入退出、清理、暂停、重入与回声联结回归。

新增展区可编辑 Director 的 Exhibits 数组，填写 ID、说明、源地图、交互类型、颜色与字幕 ID；新交互类型需要在 Director 中增加适配。
当前十一处实体展台由创建脚本生成；修改配置数量后也需同步展台布局与测试断言。
不要把只适用于一个实例的原型直接多开；若下一步要做连续主世界，需要进一步消除各玩法的绝对坐标和全局查找。

## 验证入口与边界

```powershell
$editor = 'E:\UE5\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
& $editor "$PWD\BrainMining.uproject" '/Game/Showcase/Maps/Lvl_FeatureShowcase' `
  -unattended -RenderOffscreen -NoSplash -NoSound -NoLiveCoding `
  '-DisablePlugins=AllToolsets,ModelContextProtocol' '-EnablePlugins=GameFeatures' `
  '-ExecCmds=Automation RunTests BrainMining+CardRoom+EchoPassage' `
  '-TestExit=Automation Test Queue Empty' "-ReportExportPath=$PWD\Saved\Automation\ShowcaseFullRegression"
```

自动化包含真实 PIE 的十一展区往返、共享角色与输入、收集图鉴暂停退出、卡牌入座退出、弹窗桌面输入、文思解法及编辑退出、重复进入收集、实际 Slate F5/F8 快捷键事件和回声出口到卡牌。
测试会打开地图、启动和结束 PIE，请勿与有未保存工作的编辑器共用同一工程 DLL。
截图在 `Saved/ShowcasePreview/`；测试报告在 `Saved/Automation/ShowcaseFullRegression/`。测试报告而不是进程退出码决定成功与否。

本阶段交付编辑器及独立游戏窗口演示，不交付已 Cook 的 Windows 包。
原型中使用本机中文字体和运行时字符串加载的资源，正式分发前仍需处理字体授权/打包、明确 cook 范围、验证音频和成品包。
自动化通过不等同于完整人工通关、全部特效审美验收或正式性能保证。

## 本次验证结果（2026-10-10，Asia/Shanghai）

- UE 5.8.2：`BrainMiningEditor Win64 Development` 和 `BrainMining Win64 Shipping` 编译通过；Shipping 编译不代表已完成 Cook 或打包。
- 完整 `BrainMining+CardRoom+EchoPassage` 回归：68 项通过、0 失败、0 未执行，耗时约 87 秒。
- 其中 3 项测试带已有测试场景警告：收集测试独立世界缺少 WorldContext、空洞区域网格超过 128 单元、旧版空间编辑器关闭 ITF。具体信息保留在测试报告中。
- 已检查目录、收集图鉴和弹窗桌面的实际 PIE 截图；模态界面不再被展示馆提示遮挡。
- `git diff --cached --check` 与 `git lfs fsck --objects` 通过；没有未解决的合并冲突。
- 整合改动尚未 commit 或 push；各原分支和三个原有 stash 未修改。
