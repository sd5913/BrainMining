# 准心、E 交互与金币反馈

共享玩家控制器自动创建 `UMiningInteractionHUDWidget`，不需要修改关卡或重新生成蓝图资源。它独立于升级面板与能力栏，在矿区和小游戏的自由探索阶段使用同一套显示。

- 准心：1920×1080 参考尺寸下，白色圆点直径 4，外层深色描边直径 6；跟随画面尺寸缩放，保持屏幕正中心。
- 交互：准心下方显示动作文字，下一行是浅色 E 键帽。复用藏品、矿物、办公室、文思谜题、牌桌和大厅入口的交互条件，不改变按键行为或新增长按。
- 金币：金币图形后显示本次入账的 `+数量`，轻微弹出、上浮，1.2 秒后消失；相邻两次入账间隔不超过 0.35 秒时合并数字。
- 菜单、图鉴、操作电脑和入座期间隐藏探索准心与 E 提示；真实金币入账仍可显示。读入余额、购买扣款和零收益不会触发金币反馈。

钱包 `AddCoins` 在余额更新后发布 `OnCoinsAdded`，参数为实际增加金额；共享钱包的矿物、打印机与电脑奖励自动接入。旧版独立矿物展区通过 `ShowCoinGain` 接入。金币图形与准心由 Slate 矢量绘制，无额外贴图依赖。

视觉参数集中在 `MiningInteractionHUDWidget.cpp` 的 `NativePaint`，时间与合并窗口在 `MiningInteractionHUDWidget.h` 的 `FMiningCoinFeedback`。新增交互来源接入 `ResolveInteractionAction`，须与实际交互的可用条件一致。

验证：先编译 `BrainMiningEditor`，运行自动化测试 `BrainMining.HUD.CoinFeedback`，再在独立编辑器进程执行 `Scripts/verify_interaction_hud.py`。后者打开全流程关卡，验证真实 E 输入、菜单切换、钱包入账、反馈消失，并保存游戏截图与报告到 `Saved/Validation/InteractionHUD`；仅修改 PIE 临时状态，结束后退出测试编辑器。
