# MainGame 金蛋结尾衔接

MainGame 博物馆仍使用原有拾取条件：两套机关已解锁，玩家处于「小」状态并走到金蛋旁。拾取成功后打开 `/Game/EndingEmergence/Maps/Lvl_EndingEmergence_Demo`，由目标地图正常创建玩家与游戏模式。

切关只在本次拾取从未完成变为已完成时触发，限于 `BM_Campaign_MainGameMuseum` 区域。独立博物馆测试地图仍保留原有完成与重置行为。结束地图目录加入 `DirectoriesToAlwaysCook`，不改变项目已有的地图选择规则。

关闭编辑器并构建后，可运行 `Scripts/verify_museum_ending.ps1`。原生回归测试 `BrainMining.Campaign.GoldenRewardEndingTravel` 使用真实 MainGame 与结束地图，检查未解锁、距离不足、未缩小三种情况不触发传送，以及拾取后目标地图、角色大小和输入状态。报告保存在 `Saved/Validation/MuseumEnding`。
