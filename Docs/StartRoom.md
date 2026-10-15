# StartRoom — 矿工卧室

独立关卡：`/Game/StartRoom/Maps/Lvl_StartRoom`。不改变现有主流程入口。

## MainGame 接入

MainGame 通过 Always Loaded 子关卡引用本源地图，沿用 MainGame 角色和控制器。出生点位于卧室，镐子和喷气背包仍需瞄准按 E 独立拾取；挖开原出口后，通过下层短通路进入竖井，按住空格上升 8 米，再从上层通路步行进入第一区。竖井已由原来的 16 米缩短，不需要先购买升级；没有楼梯、梯子或可踩踏横梁，普通跳跃不能到顶。

上层通路与第一区原入口的接缝使用不可挖基岩封口墙，覆盖矿区整个入口截面并与原外壳重叠。仅保留与通路一致的 3 米宽、3.3 米高开口，开口内用 1.8 米厚的可挖堵塞层封住，沿用矿区引导岩层材质。挖开堵塞层后，周围基岩和通路四壁继续阻止玩家离开地图；R 会恢复这块堵塞层。

MainGame 的炸弹、钻机与 Tab 升级面板在拿齐两件初始装备后开放；独立 StartRoom 仍保持原来的教学工具限制。第一区内按 R 恢复本区及卧室出口的岩层，保留已拾取装备、金币和升级；按 F6 返回卧室入口，其它区域仍返回本段入口。

只在 MainGame 实例中禁用源房间末端的 FutureConnectionCap 与全局曝光体积，源地图无需开洞或改灯光。房间内模型和拾取物仍在源子关卡中编辑，连接段位于 MainGame/StartRoomConnection。首次整合脚本为 `Scripts/integrate_main_game_start_room.py`，缩短竖井及封口调整为 `Scripts/refine_main_game_start_room.py`，专项游戏验证为 `Scripts/verify_main_game_start_room.py`，报告位于 `Saved/Validation/MainGameStartRoom`。整合前备份位于 `Saved/Backups/MainGame_StartRoom*`。

2026-10-14 首次接入验证：Editor Development 完整编译通过；35 项配置检查、52 项 MainGame 游戏检查及 32 项独立 StartRoom 回归通过。原 16 米版本未升级背包约 3.74 秒达到竖井出口高度，离开竖井前最高热量约 89%；普通跳跃无法到达平台。验证覆盖真实 E、鼠标、W、空格、G、V、Tab、R 输入，升级购买、模型和挥镐表现、区域往返与装备保持。原 125 处矿物布局、StartRoom 源地图及 FullJourney 地图的哈希保持不变。独立房间的验证脚本改为持续挖掘实际阻挡胶囊的岩层，以适配现有镐子的三击破坏规则。

2026-10-14 缩短与封口调整：保存并重载关卡后，64 项 MainGame PIE 检查通过。8 米竖井未升级背包约 1.96 秒到达出口，离开竖井前最高热量约 53.5%。新增堵塞层阻挡步行、真实镐子挖掘、挖开后的完整胶囊通行、52 处封口与通道内缘碰撞采样、R 恢复封堵均通过；125 处矿物、初始装备、金币及三种能力升级检查通过。StartRoom 和 FullJourney 源地图保持原哈希。调整记录见 `refinement.json`，游戏报告见 `runtime.json`。

首次在读取 FullJourney 参照资源后启动 PIE 时发生提交内存耗尽；最终 MainGame 游戏检查使用只加载 MainGame 的全新编辑器进程通过。没有进行发布包 Cook 或长期性能测试。

房间包含木床、床垫与毯子、床头柜、咖啡杯、矿工安全帽、工作服与靴子、补给箱、矿石样本、矿道图和暖色矿灯。东侧是唯一出口，使用现有 `DiggableRock` 封堵，墙壁、地板与天花板不可挖。出口后接一小段封闭矿道，末端预留后续关卡连接。

出生时挖掘和喷气飞行锁定。走到出口附近，瞄准地上的装备按 E：镐子解锁左键挖掘，喷气背包解锁按住空格上升。两件装备可以任意顺序拾取；拾取后消失且不会重复领取。R 复用地形重置，保留本次试玩已解锁装备。重新开始关卡恢复首次拾取状态。该独立教学关卡暂不启用 G/V/Tab。

复用 SharedMiningCharacter、镐子模型、MiningComponent、MiningCharacterMovementComponent 的飞行／热量逻辑、MiningInteractionHUDWidget、DiggableRock，以及项目现有木材、布料、金属和岩石材质。资源检查未发现独立喷气背包网格，因此背包外观使用现有基础网格与项目材质组装，功能使用已有喷气系统。

新增可复用 `MiningStarterPickup` 支持 Pickaxe／Jetpack 类型；交互采用相机 Visibility 射线，默认范围 260 cm，有墙体遮挡时无法拾取。`MiningStartRoom` 管理本关卡的嵌套挖掘／飞行锁，退出关卡时释放自己持有的锁。蓝图均保存在 StartRoom/Blueprints。装备视觉子 Actor 可替换，交互逻辑不依赖模型。

场景 Actor 按 StartRoom/Bed、Dressing、Supplies、Equipment、Exit、Timber、Lighting 和 Gameplay 分组，均可在关卡中独立编辑。

创建脚本：`Scripts/create_start_room.py`，拒绝覆盖已有关卡。运行验证：`Scripts/verify_start_room.py`，仅改变 PIE 世界；报告与游戏截图在 `Saved/Validation/StartRoom`。

2026-10-14：完整 Editor Development 编译通过，32 项 PIE 检查通过，包含先取背包再取镐子、真实 E／空格／鼠标输入、热量、挖开出口后的胶囊通行和 R 重置。场景共 187 个 Actor。FullJourney、MainGame、共享角色蓝图和原镐子网格的 SHA-256 前后相同。验证期间临时关闭后台降帧，结束后恢复原设置。
