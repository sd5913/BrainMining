# StartRoom — 矿工卧室

独立关卡：`/Game/StartRoom/Maps/Lvl_StartRoom`。不改变现有主流程入口。

房间包含木床、床垫与毯子、床头柜、咖啡杯、矿工安全帽、工作服与靴子、补给箱、矿石样本、矿道图和暖色矿灯。东侧是唯一出口，使用现有 `DiggableRock` 封堵，墙壁、地板与天花板不可挖。出口后接一小段封闭矿道，末端预留后续关卡连接。

出生时挖掘和喷气飞行锁定。走到出口附近，瞄准地上的装备按 E：镐子解锁左键挖掘，喷气背包解锁按住空格上升。两件装备可以任意顺序拾取；拾取后消失且不会重复领取。R 复用地形重置，保留本次试玩已解锁装备。重新开始关卡恢复首次拾取状态。该独立教学关卡暂不启用 G/V/Tab。

复用 SharedMiningCharacter、镐子模型、MiningComponent、MiningCharacterMovementComponent 的飞行／热量逻辑、MiningInteractionHUDWidget、DiggableRock，以及项目现有木材、布料、金属和岩石材质。资源检查未发现独立喷气背包网格，因此背包外观使用现有基础网格与项目材质组装，功能使用已有喷气系统。

新增可复用 `MiningStarterPickup` 支持 Pickaxe／Jetpack 类型；交互采用相机 Visibility 射线，默认范围 260 cm，有墙体遮挡时无法拾取。`MiningStartRoom` 管理本关卡的嵌套挖掘／飞行锁，退出关卡时释放自己持有的锁。蓝图均保存在 StartRoom/Blueprints。装备视觉子 Actor 可替换，交互逻辑不依赖模型。

场景 Actor 按 StartRoom/Bed、Dressing、Supplies、Equipment、Exit、Timber、Lighting 和 Gameplay 分组，均可在关卡中独立编辑。

创建脚本：`Scripts/create_start_room.py`，拒绝覆盖已有关卡。运行验证：`Scripts/verify_start_room.py`，仅改变 PIE 世界；报告与游戏截图在 `Saved/Validation/StartRoom`。

2026-10-14：完整 Editor Development 编译通过，32 项 PIE 检查通过，包含先取背包再取镐子、真实 E／空格／鼠标输入、热量、挖开出口后的胶囊通行和 R 重置。场景共 187 个 Actor。FullJourney、MainGame、共享角色蓝图和原镐子网格的 SHA-256 前后相同。验证期间临时关闭后台降帧，结束后恢复原设置。
