# MainGame 挖掘命中特效

镐子有效命中使用可复用的碎石池，并产生一团短促尘土；钻机命中保留尘土、烟雾和火花。资源与钻机强度在 `BP_MainGameCharacter` 的 `MiningToolEffects` 组件中配置，模型与动作仍由各自的第一人称组件管理。

弯曲通路中的生成点可能更靠近相邻房间的管理原点。`AMiningDrillDustBurst` 现在与碎石一样带有 `BM_PlayerEffect` 标记，区域管理器不会将其归入其他区域并暂停 Tick。未修改镐子、钻机的挖掘数值或关卡材质。

已有性能约束保留：碎石池最多四组、每组最多 64 块；钻机接触特效最多 12 组，并按寿命销毁；尘土、烟雾和火花使用实例化平面。区域切换期间仍正常更新和消退，挖掘重置会回收碎石并清除接触特效。

补充修复：原实现的 `SpawnImpact` 只在钻机模式创建尘雾，镐子在暗色矿区只剩与墙同色的碎土块。现在镐子每次有效命中会生成约 16 个尘土实例，寿命约 0.48–0.81 秒，沿撞击面法线散开；不生成钻机火花或烟雾。空气挥镐、硬度完全阻挡、被锁定的岩层均不触发有效命中事件。所有尘雾共用最多 12 组的上限，并保留 `BM_PlayerEffect` 标记。

`Scripts/verify_main_game_lowpoly_fx.py` 使用正常的 `bm.Mining.LazyRuntimeChunks=1`、真实装备拾取和鼠标输入检查四区普通层及堵塞层；第一区先用初始镐子验证，后续区域通过真实购买升级覆盖更高硬度。它同时检查镐子尘雾、钻机完整接触效果、后续帧运动和重置清理，并截取实际游戏画面。新记录在 `Saved/Validation/MainGameRoutesV4/FX`。

2026-10-16：新版通路保存后完成 16 组真实输入场景，102 项检查全部通过。普通层和堵塞层都产生镐子尘雾，钻机保留烟雾与火花，后续帧粒子持续运动，重置后无残留；`FX/runtime.json` 的 `completed` 与 `passed` 均为 `true`。

`Scripts/inspect_main_game_mining_fx.py` 检查角色配置，`Scripts/inspect_main_game_mining_fx_runtime.py` 记录实际命中状态。`Scripts/verify_main_game_mining_fx.py` 覆盖四区普通可挖层、堵塞层及两种工具，验证真实切割、碎石可见性、粒子 Tick 和位移、池容量及重置清理。验证使用 PIE 副本，隔离入口、定位和购买升级用于检查所有硬度的堵塞层，不写入关卡或玩家配置。

记录目录为 `Saved/Validation/MainGameMiningFX`。`reproduced_before.json` 保留修复前的复现：区域一后段钻机云团存在，但 Tick 已关闭；两处采样的镐子碎石仍能正常生成。

2026-10-15 验收：编译成功；四区共 16 个真实命中场景、78 项脚本检查通过，覆盖两种工具和两种岩层。尘土、烟雾和火花保持更新，尘土实例在后续帧持续位移，重置清理通过。原生 `BrainMining.Startup.DebrisPoolReuse` 通过；其临时测试世界清理产生一条无 world context 的日志警告，已保留在 `native.json`。关卡、玩家蓝图和 C4 特效模型的文件哈希保持一致。最终汇总为 `acceptance.json`。
