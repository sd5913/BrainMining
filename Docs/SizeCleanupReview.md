# 磁盘清理审阅清单

## 缓存清理尝试：2026-10-22 12:56（北京时间）

用户明确要求清理剩余缓存。此次范围仅限冻结清单内 49 个 `CachedAssetRegistry` 目录，共 7,660,306,884 字节（7.13 GiB）。已核验路径、祖先与子项重解析点、文件类型、Git 跟踪状态及字节数，并记录 205 个报告文件的 SHA-256；r6 APK 校验通过，无活动 UE 编辑器或构建进程。

删除命令仍被自动审批拒绝，仅返回 `blocked by policy`，命令未启动。本次删除 0 字节，事后确认 49 个目录全部仍在。用户授权已明确，不需要再次确认。证据：`Saved/Validation/SizeOptimization/cache_cleanup_preflight.json` 与 `cache_cleanup_blocked.json`。以下为早先复核记录。

## 最新复核：2026-10-22 01:40（北京时间）

用户再次报告删除，但冻结清单的剩余状态未变：37 个历史构建目录均不存在；49 个 `CachedAssetRegistry` 目录仍在，文件大小均与基准一致，合计 7,660,306,884 字节（7.13 GiB）。无法从此次检查确定用户删除的是哪个其他目录。

E 盘可用空间为 151,419,670,528 字节（141.02 GiB），比上次观测增加 1.35 GiB，不能将这次整卷增量归因于上述缓存。49 个测试运行目录和上次记录的报告路径仍在。r6 APK 大小及 SHA-256 通过校验，V15 保留构建仍缺失。证据：`Saved/Validation/SizeOptimization/storage_cleanup_manual_verification_r3.json`。

## 上次复核：2026-10-22 01:28（北京时间）

冻结清单内 37 个历史构建目录已全部不存在，清理前文件逻辑大小合计 62,167,505,430 字节（57.90 GiB）。此前剩余的四个历史构建也已删除。49 个测试索引缓存仍全部存在，共 7,660,306,884 字节（7.13 GiB），对应运行目录及报告文件均存在。

E 盘可用空间为 149,971,259,392 字节（139.67 GiB），较上次复核增加 5.36 GiB，较清理前最近观测增加 71.21 GiB；这些都是整卷变化，不等于清单专属收益。r6 APK 大小及 SHA-256 再次通过校验。V15 保留构建仍缺失；其余已检查的保留构建与细胞备份存在。

最新证据：`Saved/Validation/SizeOptimization/storage_cleanup_manual_verification_r2.json`。以下保留原冻结清单与第一次复核历史，不作为当前存在状态。

这是冻结的只读清单的可读版本，不会执行删除。只涵盖下表明确列出的目录；本次新生成的优化候选包不在此清单中。

来源：`Saved/Validation/SizeOptimization/storage_plan.json`。复核：`storage_plan_validation.json`。
共 86 个目录，69,827,812,314 字节，约 65.03 GiB。

保留的原有构建：`Android`, `AndroidMainGameV15`, `AndroidMainGameV29`, `AndroidRemoteSync20261021`, `AndroidRemoteSync20261021v112`, `AndroidTapPlayCompatibility`。

原始美术、源码、Git/LFS、恢复备份和共享引擎不在清理范围内。测试项只列资产索引缓存子目录，测试报告本身保留。

用户已于 2026-10-22 明确确认此冻结清单。自动删除曾被工具策略拦截，命令未启动；随后用户报告已手动删除。以下为 2026-10-22 01:18（北京时间）的复核结果；原始冻结清单保持不变，下表不代表当前目录均存在。

- 原清单中 33 个历史构建目录已不存在，清理前文件逻辑大小合计 57,218,356,166 字节（53.29 GiB）。
- 尚存 4 个历史构建目录：`AndroidStandaloneApk`、`AndroidMainGameV1`、`AndroidControlsV3`、`AndroidControlsV2`，合计 4,949,149,264 字节（4.61 GiB）。
- 49 个测试索引缓存目录均仍在，合计 7,660,306,884 字节（7.13 GiB）；每个对应测试运行目录均存在，并能找到报告文件。
- 原定保留的 `AndroidMainGameV15` 在原位置缺失，回收站中未找到匹配项；其余五个原有保留构建、细胞恢复备份和 r6 APK 均存在。r6 APK 的大小及 SHA-256 与验证基准一致。
- E 盘可用空间由 68.46 GiB 增至 134.31 GiB，观测增量 65.85 GiB。此为整个卷在两次观测间的变化，不能全部归因于冻结清单内的删除。

证据：`Saved/Validation/SizeOptimization/storage_cleanup_manual_verification.json` 与 `storage_cleanup_v15_recovery_check.json`。本次复核未继续删除任何文件。

## 历史构建

37 个目录，约 57.90 GiB。

| 项目内相对路径 | GiB |
|---|---:|
| `Builds/MobileVerticalSlice/AndroidWenSiInteractionFix` | 3.154 |
| `Builds/MobileVerticalSlice/AndroidWenSiTouchFix` | 3.154 |
| `Builds/MobileVerticalSlice/AndroidRemoteSync20261020` | 3.154 |
| `Builds/MobileVerticalSlice/AndroidCollectionMenuFix` | 2.640 |
| `Builds/MobileVerticalSlice/AndroidEndingFix` | 2.640 |
| `Builds/MobileVerticalSlice/AndroidCardRewardFix` | 2.640 |
| `Builds/MobileVerticalSlice/AndroidCardFix` | 2.640 |
| `Builds/MobileVerticalSlice/AndroidMiningFix` | 2.640 |
| `Builds/MobileVerticalSlice/AndroidMobileUI` | 2.461 |
| `Builds/MobileVerticalSlice/AndroidStandaloneApk` | 0.930 |
| `Builds/MobileVerticalSlice/AndroidStandaloneApkRetry` | 1.228 |
| `Builds/MobileVerticalSlice/AndroidEndingRoom` | 1.261 |
| `Builds/MobileVerticalSlice/AndroidMainGameV28` | 1.248 |
| `Builds/MobileVerticalSlice/AndroidMainGameV27` | 1.241 |
| `Builds/MobileVerticalSlice/AndroidMainGameV26` | 1.250 |
| `Builds/MobileVerticalSlice/AndroidMainGameV25` | 1.241 |
| `Builds/MobileVerticalSlice/AndroidMainGameV24` | 1.241 |
| `Builds/MobileVerticalSlice/AndroidMainGameV23` | 1.241 |
| `Builds/MobileVerticalSlice/AndroidMainGameV22` | 1.241 |
| `Builds/MobileVerticalSlice/AndroidMainGameV21` | 1.241 |
| `Builds/MobileVerticalSlice/AndroidMainGameV20` | 1.242 |
| `Builds/MobileVerticalSlice/AndroidMainGameV19` | 1.240 |
| `Builds/MobileVerticalSlice/AndroidMainGameV17` | 1.240 |
| `Builds/MobileVerticalSlice/AndroidMainGameV16` | 1.240 |
| `Builds/MobileVerticalSlice/AndroidMainGameV12` | 1.163 |
| `Builds/MobileVerticalSlice/AndroidMainGameV11` | 1.163 |
| `Builds/MobileVerticalSlice/AndroidMainGameV10` | 1.163 |
| `Builds/MobileVerticalSlice/AndroidMainGameV9` | 1.163 |
| `Builds/MobileVerticalSlice/AndroidMainGameV8` | 1.163 |
| `Builds/MobileVerticalSlice/AndroidMainGameV7` | 1.151 |
| `Builds/MobileVerticalSlice/AndroidMainGameV6` | 1.151 |
| `Builds/MobileVerticalSlice/AndroidMainGameV5` | 1.151 |
| `Builds/MobileVerticalSlice/AndroidMainGameV4` | 1.151 |
| `Builds/MobileVerticalSlice/AndroidMainGameV1` | 1.151 |
| `Builds/MobileVerticalSlice/AndroidControlsV3` | 1.264 |
| `Builds/MobileVerticalSlice/AndroidControlsV2` | 1.264 |
| `Builds/MobileVerticalSlice/AndroidDelivery` | 0.354 |

## 隔离测试资产索引缓存

49 个目录，约 7.13 GiB。

| 项目内相对路径 | GiB |
|---|---:|
| `Saved/Validation/Gamepad/20261019-134832/User/Intermediate/CachedAssetRegistry` | 0.141 |
| `Saved/Validation/Gamepad/20261019-135020/User/Intermediate/CachedAssetRegistry` | 0.141 |
| `Saved/Validation/Gamepad/20261019-143455/User/Intermediate/CachedAssetRegistry` | 0.141 |
| `Saved/Validation/Gamepad/20261019-143942/User/Intermediate/CachedAssetRegistry` | 0.141 |
| `Saved/Validation/Gamepad/20261019-144200/User/Intermediate/CachedAssetRegistry` | 0.141 |
| `Saved/Validation/Gamepad/20261019-144623/User/Intermediate/CachedAssetRegistry` | 0.141 |
| `Saved/Validation/Gamepad/20261019-200940/User/Intermediate/CachedAssetRegistry` | 0.141 |
| `Saved/Validation/Gamepad/20261019-222513/User/Intermediate/CachedAssetRegistry` | 0.142 |
| `Saved/Validation/Gamepad/20261019-222800/User/Intermediate/CachedAssetRegistry` | 0.142 |
| `Saved/Validation/Gamepad/20261019-222941/User/Intermediate/CachedAssetRegistry` | 0.142 |
| `Saved/Validation/Gamepad/20261019-223253/User/Intermediate/CachedAssetRegistry` | 0.142 |
| `Saved/Validation/Gamepad/20261019-225617/User/Intermediate/CachedAssetRegistry` | 0.142 |
| `Saved/Validation/Gamepad/20261019-225911/User/Intermediate/CachedAssetRegistry` | 0.142 |
| `Saved/Validation/Gamepad/20261019-232513/User/Intermediate/CachedAssetRegistry` | 0.142 |
| `Saved/Validation/Gamepad/20261019-233032/User/Intermediate/CachedAssetRegistry` | 0.142 |
| `Saved/Validation/Gamepad/20261020-015624/User/Intermediate/CachedAssetRegistry` | 0.142 |
| `Saved/Validation/Gamepad/20261020-015920/User/Intermediate/CachedAssetRegistry` | 0.142 |
| `Saved/Validation/Gamepad/20261020-024549/User/Intermediate/CachedAssetRegistry` | 0.142 |
| `Saved/Validation/Gamepad/20261020-030817/User/Intermediate/CachedAssetRegistry` | 0.142 |
| `Saved/Validation/Gamepad/20261020-032542/User/Intermediate/CachedAssetRegistry` | 0.142 |
| `Saved/Validation/Gamepad/20261020-043633/User/Intermediate/CachedAssetRegistry` | 0.149 |
| `Saved/Validation/Gamepad/20261020-062853/User/Intermediate/CachedAssetRegistry` | 0.149 |
| `Saved/Validation/Gamepad/20261020-064253/User/Intermediate/CachedAssetRegistry` | 0.149 |
| `Saved/Validation/Gamepad/20261020-071216/User/Intermediate/CachedAssetRegistry` | 0.149 |
| `Saved/Validation/Gamepad/20261020-105721/User/Intermediate/CachedAssetRegistry` | 0.149 |
| `Saved/Validation/Gamepad/20261020-110005/User/Intermediate/CachedAssetRegistry` | 0.149 |
| `Saved/Validation/Gamepad/20261020-112604/User/Intermediate/CachedAssetRegistry` | 0.149 |
| `Saved/Validation/Gamepad/20261020-113423/User/Intermediate/CachedAssetRegistry` | 0.149 |
| `Saved/Validation/Gamepad/20261020-114022/User/Intermediate/CachedAssetRegistry` | 0.149 |
| `Saved/Validation/Gamepad/20261020-115127/User/Intermediate/CachedAssetRegistry` | 0.149 |
| `Saved/Validation/Gamepad/20261020-131620/User/Intermediate/CachedAssetRegistry` | 0.149 |
| `Saved/Validation/Gamepad/20261020-135742/User/Intermediate/CachedAssetRegistry` | 0.149 |
| `Saved/Validation/Gamepad/20261020-141800/User/Intermediate/CachedAssetRegistry` | 0.149 |
| `Saved/Validation/Gamepad/20261020-151025/User/Intermediate/CachedAssetRegistry` | 0.149 |
| `Saved/Validation/Gamepad/20261020-151428/User/Intermediate/CachedAssetRegistry` | 0.149 |
| `Saved/Validation/Gamepad/20261020-193030/User/Intermediate/CachedAssetRegistry` | 0.148 |
| `Saved/Validation/Gamepad/20261020-194240/User/Intermediate/CachedAssetRegistry` | 0.148 |
| `Saved/Validation/Gamepad/20261020-221553/User/Intermediate/CachedAssetRegistry` | 0.148 |
| `Saved/Validation/Gamepad/20261020-222028/User/Intermediate/CachedAssetRegistry` | 0.148 |
| `Saved/Validation/Gamepad/20261020-223452/User/Intermediate/CachedAssetRegistry` | 0.148 |
| `Saved/Validation/Gamepad/20261020-234943/User/Intermediate/CachedAssetRegistry` | 0.148 |
| `Saved/Validation/Gamepad/20261020-235634/User/Intermediate/CachedAssetRegistry` | 0.148 |
| `Saved/Validation/Gamepad/20261021-005430/User/Intermediate/CachedAssetRegistry` | 0.148 |
| `Saved/Validation/Gamepad/20261021-010107/User/Intermediate/CachedAssetRegistry` | 0.148 |
| `Saved/Validation/Gamepad/20261021-082513/User/Intermediate/CachedAssetRegistry` | 0.148 |
| `Saved/Validation/Gamepad/20261021-083442/User/Intermediate/CachedAssetRegistry` | 0.148 |
| `Saved/Validation/Gamepad/20261021-091914/User/Intermediate/CachedAssetRegistry` | 0.148 |
| `Saved/Validation/Gamepad/20261021-102330/User/Intermediate/CachedAssetRegistry` | 0.148 |
| `Saved/Validation/Gamepad/20261021-103009/User/Intermediate/CachedAssetRegistry` | 0.148 |
