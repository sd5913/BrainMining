# 运行性能与复测

2026-10-13，Asia/Shanghai。

## 直接试玩

双击 `Play_Main.cmd` 或 `Play_FullJourney.cmd`。现在默认以 1600×900 窗口运行，使用 Balanced 档：全局光照、反射、阴影为 High，其余画质沿用原设置。保留 Lumen 和硬件光追，不改变矿点、挖掘深度或碎晶密度。

```powershell
.\Play_Main.cmd -Quality Epic
.\Play_Main.cmd -Quality Balanced -Width 1920 -Height 1080
.\Play_Main.cmd -Profile
```

`-Quality Epic` 恢复这三组的原 Epic 档。`-Editor` 仅打开编辑器，不覆盖编辑器画质。`-Profile` 运行 1800 帧后自动退出，CSV 写入 `Saved/Profiling/CSV`；采样期间不限帧并关闭垂直同步。CPU/GPU 性能比较应在相同分辨率、位置和朝向下进行，排除启动和首次材质准备。

## 本次修改

- 重新编译最新源码。检查开始时，编辑器模块的修改时间为 10 月 12 日 21:19，部分挖掘源码为 10 月 13 日 08:37。下方基线已经重新编译，不是旧模块的测量结果。
- 分块岩体的同步切割软预算上限降至 3ms，包含准备阶段的已用时间。控制台变量 `bm.Mining.InteractiveBudgetMs` 可调整，仍受每块岩体自己的 `InteractiveCutBudgetMilliseconds` 限制。旧地图保存的 12ms 值不会绕过这个上限。
- 超出预算的切割继续使用已有后台队列，完成后统一提交网格、碰撞和碎晶。3ms 是可协作取消的布尔计算软预算，不是整个挖掘操作的硬耗时保证。
- 缓存碎晶支撑点的包围盒。变化范围未触及矿脉时立即返回，已经移除的支撑点不再重复查询，重置时仍完整恢复。
- 空间查询缓存命中时直接复用，不再每次扫描整个缓存；同一次变化失去支撑的碎晶批量移除，保留存活实例的对应关系。
- 增加 `Mining_Cut`、`Mining_Commit`、`Mining_TerrainListeners`、`Mining_CrystalSupport`、`Mining_Coverage`、`Mining_TerrainVersion` 的 Unreal Insights CPU 标记。岩体的 `LastTerrainNotificationMilliseconds` 记录最近一次分块提交通知的耗时。

## 挖掘对比

同一 FullJourney 第一矿点，18 次半径 100cm 的切割，每次沿内部推进 35cm。每轮对矿点关联的 11 个宿主岩体发起查询；这是一组可复现的多岩体压力操作，不等同于玩家单次普通挥镐。等待后台任务完成后再发起下一轮，场景只在 PIE 副本中改变。

相同 Epic 画质、948×499 PIE 视口，采集 Slate 回调间隔。它用于比较挖掘停顿，不代表独立游戏的 GPU 帧率。

| 指标 | 重新编译后的基线 | 本次代码优化后 |
| --- | ---: | ---: |
| 每轮调用平均耗时 | 28.51ms | 9.55ms |
| 每轮调用最大耗时 | 40.34ms | 13.90ms |
| 挖掘阶段帧间隔 P95 | 22.12ms | 21.98ms |
| 挖掘阶段超过 50ms 的采样帧 | 8 | 2 |
| 挖掘阶段最大帧间隔 | 63.34ms | 70.10ms |

调用开销明显下降，但最坏帧没有改善，P95 也基本持平；仍不能称为全流程稳定 60 FPS。后台切割完成时的同步碰撞和支撑检查仍有峰值，后续应根据上述 CPU 标记继续定位，避免用分帧显示造成洞口与碰撞不同步。

原始结果：`Saved/Performance/rebuilt_epic_baseline.json`、`Saved/Performance/optimized_epic.json`。`rebuilt_high_baseline.json` 记录只降低三组画质的对照，它没有明显改善小视口中的挖掘停顿。

复测时，在无其他 PIE 会话的 FullJourney 编辑器中执行 `Scripts/profile_runtime_performance.py`。脚本会启动 PIE、移动到固定矿点、采样并结束 PIE，恢复原画质和后台节流设置，不保存地图。环境变量 `BRAINMINING_PROFILE_LABEL` 指定 JSON 名称，`BRAINMINING_PROFILE_QUALITY` 可为 `epic`、`high` 或 `software`。`software` 仅供对照，额外关闭 Lumen 硬件光追，并不是默认画质。仅在专门启动的无人值守编辑器中设置 `BRAINMINING_PROFILE_EXIT=1`，让它完成后自动退出编辑器。

## 独立运行的渲染对比

重新编译后的同一版本，以编辑器可执行文件的 `-game` 模式运行 FullJourney，1600×900，出生点静止，不限帧、关闭垂直同步和声音。使用引擎 CSV 的最后 600 个有效帧，排除启动；不是完整打包版，也没有覆盖所有房间。

| 平均耗时 | Epic | Balanced |
| --- | ---: | ---: |
| 帧时间 | 7.57ms | 7.61ms |
| 游戏线程 | 4.16ms | 4.25ms |
| 渲染线程 | 7.56ms | 7.60ms |
| GPU | 5.12ms | 4.34ms |

Balanced 降低了 GPU 开销，但该观察点由渲染线程限制，整体帧时间没有改善。保留这个档位是为了降低渲染负担，不能把 GPU 耗时下降宣传成全场帧率提升。

## 验证范围和剩余限制

编辑器 Development 构建成功。以下检查通过：

- 碎晶批量移除、碎晶与地形同步提交、覆盖缓存失效：`Saved/Automation/PerformanceTreasure/index.json`，3 项通过、0 失败。
- 三段矿区的真实挖掘、暴露、拾取结算、领取后继续挖掘：`Saved/Automation/PerformanceMinerals/index.json`，1 项通过、0 失败，使用 NullRHI 隔离渲染，不能代替视觉性能测试。
- 后台命中不丢失：`Saved/PerformanceFocused.log` 中 `DeferredHitNotDropped` 的 Success 记录。
- 关卡通行与线索：`Saved/PerformanceRegression.log` 中 `ContinuityAndClues` 的 Success 记录。

报告含测试世界销毁及网络连通性检查的警告。最初的渲染综合批次、包含旧薄壁压力测试的批次均在导出完整报告前退出，原因未确定；不计作整个批次通过。完整矿物流程随后单独以 NullRHI 复测通过，碎晶相关测试也单独导出了完整报告。

本次保留原关卡常驻方式，没有重做关卡流送。引擎 5.8.2 的 `PipelineStateCache::IsPSOPrecachingEnabled` 在 `WITH_EDITOR` 构建中直接返回 false，因此编辑器及 `UnrealEditor -game` 的首次 PSO 卡顿不能通过项目开关解决。正式游戏构建默认启用 PSO 预缓存，实际覆盖率和首次加载仍需在完整 Cook/打包后验证。

本次不更新现有旧 Shipping 可执行文件或历史打包目录；使用上述主入口体验已编译的优化版本。
