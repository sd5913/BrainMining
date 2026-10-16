# MainGame UI 清理

本轮按 UI 盘点编号删除以下显示内容。变更位于共享原生实现，使用这些实现的独立房间也同步生效。

| 编号 | 删除内容 |
| --- | --- |
| A01、A02、A03 | 全流程常驻区域说明框、快捷键说明、框内动态通知 |
| A11 | 已到达区域菜单及其 F4 入口；不再暂停游戏或切换鼠标输入 |
| C01、C04 | 回声房标题、版本说明、底部常驻操作说明 |
| D01 | 牌室标题、中英文副标题、右上版本角标及底板 |
| 卡牌房探索提示框（截图补充） | 删除探索阶段底部整块提示背景、挖墙/靠近牌桌/入座动态文案，以及 WASD、空格、R 操作说明；保留共享交互提示和牌局操作 |
| E01 | 逃生阶段任务文案，原底板缩小以容纳保留的时间信息 |
| E05、E07 | 逃生底部操作框、成功提示框 |
| E08 | 电视屏幕上的 EXIT / DIG HERE 世界空间文字组件及更新逻辑 |

保留地图、图鉴、矿石背包、音量设置、开发者跳转、回声阶段反馈与冷却、牌局操作与血量、逃生倒计时与检查点、水位条、目标标记、失败反馈。电视闪光与挖掘碰撞不受文字删除影响。其他关卡的独立玩法选择菜单仍保留。

编译与运行验证使用项目资源保护脚本。MainGame 镜像房回归检查 F4 不创建菜单、不暂停和不锁定输入，并覆盖 R 启动交换动画、落位后应用属性并关闭气泡、Q 关闭全部和 G 不再触发交换。完整逃生回归检查牌局与电视逃生仍可完成。

## 文思房交换动画与输入修复

- MainGame 通过控制器直接处理文思房 R/Q 按键；独立镜像关卡保留轮询入口。一次动画期间的重复输入不重复交换。
- R 触发红词沿上下两条弧线飞向对侧（0.7 秒）；落位时交换真实对象属性，更新后的气泡保留 0.3 秒，然后在 0.2 秒内淡出关闭。
- Q、谜题重置、离开房间会取消尚未提交的交换。真实碰撞拒绝交换时恢复原词并保留气泡；没有新增文字说明或失败原因提示。
- 修正把整个罩子的包围盒当作实体障碍的问题：逐部件使用实际碰撞形状检查，允许站在中空区域时交换，仍拒绝实体重叠和不安全的角色变大。
- 编译通过。MainGame 自动化成功，0 个测试错误、2 条已有音频性能警告；覆盖动画时序、属性效果、重复输入、取消、重置、离开与碰撞。报告：`Saved/Automation/RoomAudioMirror/index.json`。现有脚本仍把带警告成功计为失败，已核对引擎报告。
- 完整 MainGame 的有画面验证因内存余量不足被资源保护脚本终止，因此改用共享同一套实现的独立文思关卡进行绘制验证。独立关卡最终有画面回归成功（2026-10-16 17:59，Asia/Shanghai），0 个错误和警告；已逐帧检查红词飞行、落位更新、气泡淡出与完全关闭的截图。报告：`Saved/Automation/MirrorSwapStandaloneVisual/index.json`。最终编译资源报告：`Saved/Performance/build-resources-20261016-175811-560.csv`。
- 可使用测试 `BrainMining.WenSi.MirrorSwapVisual` 并传入 `-WenSiSwapScreenshots` 输出动画帧到 `Saved/MirrorSwapPreview`。测试使用 `-nosound`，未进行听感验收。

## 文思房按键调整（此前记录）

根据后续要求，将 MainGame 文思房的直接交换从 G 改为 R，并同步更新气泡提示与输入回归测试；Q 仍关闭所有气泡。

换键后的补充验证（2026-10-16 17:25，Asia/Shanghai）：
- `Scripts/Build_Safe.ps1` 编译通过，包含卡牌房探索提示框删除。资源报告：`Saved/Performance/build-resources-20261016-172332-057.csv`。
- `BrainMining.MainGame.MirrorMuseumAudio` 引擎测试为 Success，0 个错误、2 条音频控制台变量查询性能警告；实际输入检查覆盖 R 交换并关闭、不锁定移动或视角、G 不再交换、Q 关闭，以及仅选一个词和未选词时的操作。实体谜题与音效事件检查亦通过。
- `Saved/Automation/RoomAudioMirror/index.json` 已更新为本次报告，`succeededWithWarnings=1`、`failed=0`、`notRun=0`、`inProcess=0`。`RoomAudio_Safe.ps1` 仍因仅接受无警告成功而返回失败。验证使用 `-nullrhi -nosound`，未进行视觉或听感验收。

以下为换键之前的验证记录；镜像房报告已被上述补充验证覆盖。

## 验证结果（2026-10-16）

- `Scripts/Build_Safe.ps1`：编译通过。重新合并编译暴露了音频变量重名，已仅重命名局部查询指针，不改变音量逻辑。
- `BrainMining.MainGame.MirrorMuseumAudio`：Success，0 个错误；包含实际 G/Q/R 输入、F4 不打开区域菜单、实体谜题与音效事件检查。存在两条音频控制台变量频繁查询的性能警告，报告记为 `succeededWithWarnings=1`；现有 RoomAudio 脚本仅计入无警告成功，因此脚本返回失败，但引擎测试本身成功。报告：`Saved/Automation/RoomAudioMirror/index.json`。
- `BrainMining.MainGame.FullCardAndTelevisionEscape`：通过。报告：`Saved/Automation/RoomAudioMainGame/index.json`。
- `BrainMining.SixAreas.PersistentWorldAndEconomy`：失败，12 条断言涉及矿物拾取、金币收入、升级与余额；该测试中的 F4 菜单移除和输入恢复断言没有失败。未修改或修复这组经济逻辑。报告：`Saved/Automation/UICleanupHall/index.json`。
- `git diff --check`：通过。自动化使用 `-nullrhi -nosound`，覆盖运行状态与输入逻辑，未进行逐屏视觉验收。
