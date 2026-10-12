# main 六区体验整合

2026-10-11，Asia/Shanghai。默认地图：/Game/Showcase/Maps/Lvl_MainExperiences。双击 Play_Main.cmd，或执行 Scripts/Play_Main.ps1；进入编辑器可加 -Editor。源码变动后先编译 BrainMiningEditor。

## 本次结构

六个体验区实际保存在同一张 Persistent Level 内，没有运行时流送子关卡。大厅入口与 F4 菜单移动现有角色，保留本轮金币、藏品、升级以及区域进度。区域之间暂不铺设连续路线，电视逃生也不自动接细胞宇宙。

| 区域 | 采用内容 |
| --- | --- |
| 矿道与矿区 | Shen-Yilang 的 Lvl_ExternalMining_Demo，保留外部通路、引导层及可挖矿体 |
| 藏品、矿物宝物与金币 | Collection V0.3 的环境线索和三维图鉴，加同区相连的九类矿物宝物庭院 |
| 回声 → 卡牌 → 逃生 | 最新巨人牌室、涨水、电脑内部通道与挖穿电视 |
| 办公室 | 最新两台电脑、两台打印机、独立任务、加速与喷发 |
| 文思博物馆 | 新版 ScreenBridge、电路、压力与空间拖拽机关 |
| 细胞宇宙 | 十一段尺度变化、挖掘与飞行结局原型 |

办公室电脑和逃生电视均保留。旧版小卡牌房、旧版文思和旧版办公室不放进新主关卡。源地图与开发工具保留，方便后续维护。

## 操作与共用系统

六区主关卡的共用角色为 "/Game/Showcase/Blueprints/BP_SharedMiningCharacter"。它保留原角色的移动、飞行、挖掘、升级与冷却配置，并单独保存适配主关卡的镐子、双钻机挂载和照明。调整工具摆放请编辑这个蓝图的 Mount / Model；Motion 仍由动画驱动。工具使用第一人称渲染，矿灯使用宽角头灯与低强度近身补光；回声与逃生区仍按原规则关闭矿灯，离区自动恢复。

- WASD / 鼠标 / 左键 / 空格 / V：沿用共用角色的移动、观察、挖掘、飞行和钻机。各玩法内已有的禁挖或鼠标交互状态继续生效。
- F4：打开六区菜单；F4 / Esc 关闭。自由控制角色时可打开，交互中可先退出，或直接 F6 回大厅。
- F6：关闭当前交互，返回大厅；大厅各入口附近按 E 进入对应区域。
- E：收集藏品、领取已暴露超过 50% 的矿物宝物、操作办公室等原有对象。
- I：全区共用三维藏品图鉴，I / Esc 关闭。
- 按住 Tab：升级面板；松开关闭。图鉴不再使用 Tab。
- R：沿用当前玩法的重置语义；挖掘重置限制在当前区域，不复活已经领取的宝物，也不清空金币。藏品区的 R 仍重置本轮藏品与其岩层。

金币使用单一 GameInstance 账户，矿物领取只结算一次。Tab 面板提供镐子、喷气背包、炸弹、钻机四条养成线，各四级；价格、效果和矿物四档价值采用飞书《HUD、宝物与养成》的规则。只能按顺序购买，金币不足、重复购买和满级不会扣费，跨区保留金币与全部等级。角色蓝图的 Progression 组件可配置每级价格、效果类型、数值及说明，具体规则见 Docs/TreasureProgression.md。

藏品单独记录在图鉴，不自动出售。卡牌奖励保持原玩法用途，未转换成金币。暂未新增磁盘存档，关闭游戏或重新开始 PIE 会重开一轮。价格与数值仅供当前展示，整体经济、叙事和其余规则冲突后续再调整。

### 矿石体验入口

进入“藏品、矿物宝物与金币”后，正前方是两排各九块的可挖岩石，身后是藏品探索区。沿中间通道靠近岩块，按左键挖掘；矿物露出严格超过 50% 后，瞄准并按 E 领取统一金币。外层岩块不直接显示完整矿物，需要先挖开。

同种矿物从小到大对应四档固定价值。默认尺寸为该矿物基准模型的 55% / 70% / 85% / 100%，可在宝物配置的 SizeMultipliers 中修改；这组尺寸是展示默认值，文档没有指定厘米数。镐子四级的50%翻倍奖励只影响领取收益，不改变矿物大小或基础价值。

2026-10-12 修复了导入后残留的入口挡墙，同时调整出生朝向与双向指示牌。墙体复制后名称会带后缀，生成脚本现在按名称前缀识别并移除它，避免重建地图时再次堵路。

## 来源

| 来源 | 核对提交 | 保留内容 |
| --- | --- | --- |
| 原 main | ff1bab3 | 挖掘、移动、飞行、原有地图与编辑工具 |
| integration/feature-showcase | 58675b5 | 展示接口、九类宝物、金币与矿物光效 |
| Huang-Weiyu | e20e34b | 矿道工具和旧样例作为开发资源保留；新主地图矿区采用 Shen 版本 |
| CHENHongfan | bc362d2 | 镐子与双钻机、碎屑、灰尘、升级 UI、冷却、背包热量 |
| Shen-Yilang | c07db84 | 四设备办公室、文思博物馆与电路、字幕、外部矿区 |
| linziyi | 69e9117 | 收集、回声、巨人卡牌、涨水电视逃生、细胞宇宙 |

原独立入口仍可通过 Scripts/Play_Main.ps1 -Experience Mine / Journey / WenSi / Office / Ending / Collection / Spatial / Manual / Chain 打开，供开发回归使用。这些独立地图保留自身按键规则；默认 Overview 才是本次六区地图。

## 实现与复现

- Scripts/create_main_experiences.py 在专用 UE 编辑器中通过 -ExecutePythonScript 运行，复制场景并重新绑定内部 Actor 引用。已有目标地图时拒绝覆盖。不能改用 Python commandlet，因为 Actor 复制依赖完整编辑器的选择系统。
- Scripts/create_shared_mining_character.py 记录共享角色的初始配置，已有蓝图时拒绝覆盖，后续美术调整直接保存在蓝图中。
- Source/BrainMining/Showcase/MiningExhibitHall.* 管理分区启停、共享角色、入口和升级；MiningWalletSubsystem.h 保存本轮金币。
- 未激活区域隐藏并停止玩法推进；办公室动态物体随所属设备归区，涨水计时离区暂停。全区图鉴独立于藏品区的推进状态。
- Saved/MainExperiencesManifest.json 记录源地图、复制数量和出生点；Saved/SixAreasPreview 保存试玩截图。
- 自动化入口：BrainMining.SixAreas.PersistentWorldAndEconomy，验证同一 World / Pawn、六区版本与引用、真实 E / I / Tab / F4 / F6 交互、矿物结算、升级扣费和跨区保持。
- BrainMining.SixAreas.SharedCharacterPresentation 验证镐头与双钻头在 4:3、16:10、16:9 视野内、V / 左键钻机旋转、到期收起及回声区往返后矿灯恢复；截图保存在 Saved/SharedTools。
- Scripts/verify_main_treasure_access.py 使用完整编辑器的 -ExecutePythonScript 运行，验证从正常区域入口步行到矿石、真实左键挖掘、E 领取金币及离区返回后的状态；不直接传送到矿物旁。报告和截图位于 Saved/Validation/MainTreasureAccess。

## 验证记录

- 2026-10-12 矿石入口专项：修复前，正常入口步行在挡墙处失败；修复后，18 件矿物生成、入口可见、步行抵达、左键挖出铜矿并领取 11 金币、离区返回保留领取状态和同一角色均通过。已检查入口与挖开矿物的实际截图。本次仅修改地图、生成脚本与验证脚本，无 C++ 变更。

- UE 5.8.2：BrainMiningEditor Win64 Development 与 BrainMining Win64 Shipping 编译均通过。日志：Saved/six-areas-build.log、Saved/six-areas-shipping.log。
- 最终专项 6/6 通过，无警告、无失败。报告：Saved/Automation/SixAreasFinal/index.json。
- 图鉴展示灯光与提示布局修复后，核心跨区用例再次 1/1 通过，无警告；Editor 与 Shipping 均重新编译通过。报告：Saved/Automation/SixAreasCatalogueUI/index.json。
- 共享角色工具与矿灯修复后，展示、钻机持续时间/冷却、旋转轴心专项 3/3 通过，六区完整跨区用例 1/1 通过，均无警告；Editor 与 Shipping 编译通过。报告：Saved/Automation/SharedTools/index.json、Saved/Automation/SharedCharacterIntegration/index.json；编译日志与实际挖掘截图：Saved/SharedTools。
- 验证了六区共用同一 World / Pawn、无流送子关卡、真实快捷键、矿物结算与镐子扣费、跨区保留，以及办公室设备、博物馆两种电路解法、十一段结局的实际挖掘与阶段推进。
- 在新主关卡完成了完整巨人牌局、奖励、洪水、电视挖掘、失败重试与出口条件验证；逐个匹配了主地图矿体与 Shen 源地图的局部网格拓扑和空间平移。
- 宝物生成、暴露判定及旧展示步行收集回归通过。已检查六区和全局菜单、图鉴的试玩截图；启动脚本语法与 git diff HEAD --check 通过。
- 此前独立地图的回归记录在 Saved/Automation/MainIntegrationFocused 和 MainIntegrationMenu。当前尚未 Cook 或生成分发包。

## Git 状态

实现整合在 main，源分支不修改。插件使用源码构建，不纳入其他机器生成的 DLL/PDB。
