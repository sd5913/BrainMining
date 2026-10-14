# MainGame 金币、能力升级与结晶

关卡 `/Game/MainGame/Maps/Lvl_MainGame` 已复用 FullJourney 的现有系统：共享钱包 `MiningWalletSubsystem`、玩家 `MiningProgressionComponent`、升级 HUD、`MiningExhibitController` 和 `MiningTreasureField`。

仍使用 `BP_MainGameCharacter` 和 `BP_MainGameGameMode`；后者以 FullJourney 的 `MiningExhibitGameMode` 为父类，并保留 MainGame 的角色。四段通路、普通岩层材质、泥土引导层、不可挖外壳和未来房间连接区保留。四条升级轨道为镐子、喷气背包、炸弹和钻机，各四级；价格与 FullJourney 一致。炸弹不依赖电脑房剧情解锁，初始装备教学完成后即可使用。

StartRoom 接入后，首次出生改在第一区之前的卧室：先按 E 拾取镐子和喷气背包，拿齐后开放炸弹、钻机和升级界面。现有金币、能力升级与 125 处固定矿物布局继续使用；卧室出口、8 米上升竖井和连接第一区的可挖堵塞层归第一区管理。连接处由不可挖基岩封口，挖开堵塞层后仍保持封闭边界。详情见 `Docs/StartRoom.md`。

MainGame 人物身体在游戏中隐藏，第一人称工具保留。四区 64 个堵塞块及卧室新连接处增加向周围岩壁散开的不规则泥土过渡，挖掘与重置沿用现有系统。参数与实现见 `Docs/MainGameDirtTransition.md`。

## 操作

- 左键挖掘；矿物暴露超过 50% 后瞄准按 E 拾取，所得金币进入共享钱包。
- 按住 Tab 打开升级面板，点击金币按钮按顺序购买；松开 Tab 返回挖掘。
- G 装备炸弹，左键投掷；V 装备／收起钻机；空格使用喷气背包。最高喷气等级可悬停，左 Ctrl 下降。
- R 使用既有岩层恢复规则；已经收取的矿物不会再次发放奖励。

## 结晶布局与经济配方

`Config/MainGameMinerals.json` 复用 `Config/CampaignMinerals.json` 的九种矿物、四档价值、类型权重和总经济预算。原末端储备分配到第四段，前三段各分前后两个阶段；按 MainGame 的实际路线重新选点。RoutesV2 固定布局为 125 处矿物、141,692 枚表面和内部碎晶；其中普通可挖层 105 处（84%），洞顶 44 处。玩家每次进入关卡读取保存布局，当前区域的矿物与碎晶按现有系统延迟初始化。新路线与景观见 `Docs/MainGameRoutesV2.md`。

四个区域 Tag 是 `BM_Campaign_MainGame01` 至 `BM_Campaign_MainGame04`，添加在对应普通岩层、引导层和外壳上。既有 `BM_MainGame_*` 标签保留。三个连接预留区归前一段，以适配既有的区域恢复规则。金币与升级在步行跨区时保留。

通用生成工具见 `Docs/MineralAuthoring.md` 和 `Scripts/campaign_minerals_api.py`。修改通路或重建引导层后，应确认生成的可挖 Actor 保留矿区 Tag，再更新管理器各 Area 的世界坐标 `MapRoute`，通过通用 API 重新布置并校验矿物。不要直接搬运 FullJourney 的矿物坐标或 Host 引用。

当前 MainGame 选点使用 `Scripts/rebuild_main_game_route_minerals.py`，在普通岩壁候选点以外增加真实洞顶射线采样，并保持每阶段奖励计划不变。统计及埋藏验收在 `Saved/Validation/MainGameRoutesV2/minerals.json`；通用 API 的默认选点仍适用于原 Campaign。

初次集成／更新脚本：`Scripts/integrate_main_game_economy.py`，在打开 MainGame 的 UE 编辑器中执行，禁止 PIE 时执行。它仅改 MainGame 资产，不修改 FullJourney 源关卡。布局通过埋藏、支撑点和内部密度校验后保存。

## 验证和恢复

- 完整 Editor Development 编译通过。
- 集成结果：`Saved/Validation/main_game_system_integration.json`。
- FullJourney 只读参照：`Saved/Validation/main_game_fulljourney_reference.json`。
- 真实游戏内检查：`Scripts/verify_main_game_systems_runtime.py`；报告和截图在 `Saved/Validation/MainGameSystems`。测试中的挖掘、测试金币与升级只作用于 PIE，不保存到关卡。
- 集成前备份：`Saved/Backups/MainGame_BeforeSystemIntegration`，包含地图和 MainGame 蓝图。

FullJourney 地图原 SHA-256：`70DD29A5840FDA1A082C17B56AF92280C33F72DB6574F4AE9C31F4C907A63340`，集成后核对相同。
