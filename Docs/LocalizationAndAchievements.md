# 三语本地化与 Steam 成就实施方案

更新日期：2026-10-29。工程为 BrainMining，游戏目标为 WOoOW，当前引擎为安装版 UE 5.8。

## 实现边界

简中 `zh-Hans`、英文 `en`、繁中 `zh-Hant` 共用正式游戏内容和存档。显示文案进入 UE FText 字符串表；藏品 ID、房间 ID、字幕段 ID 和成就 API 名称保持不变。普通 PC、Android 和 SteamPC 共用成就规则，只有 SteamPC 游戏构建启用 Steam 适配器。

已迁移 541 条三语文案，包括菜单/设置/暂停、地图及正式区域说明、交互提示、矿镐升级、图鉴、卡牌、办公室弹窗、文思谜题、结局提示和 47 个正式字幕资产中的 54 条字幕。数量代表已进入目录的词条，不能替代整个游戏的逐屏验收。

## 2026-10-29 HUD 与起飞标语修复

左上角库存名称创建时曾将 FText 转成 FString，再转换成普通文本，导致切换语言后保留创建时的中文；现在直接保存字符串表 FText。四种矿石的独立 HUD 标签使用首字母大写的 Copper、Iron、Malachite、Diamond，句子中使用的原有小写矿石名保持正常英文语法。

右上角升级摘要保留原有外观，增加材料区宽度；名称与需求数量之间留出 16 单位空隙，两个文本区分别按可用尺寸缩小并裁剪，避免英文名称和 Need N more 相互覆盖。左上角名称和计数也各自限制在分配区域，适应长矿石名与大数值。

墙面 MiningWallSign 支持显式 LocalizationKey；现有地图中保存的“按住空格起飞”和“按住空格键起飞”自动对应三语词条，无需重写地图。英文为 Hold Space to fly，繁中为“按住空白鍵起飛”。显示绑定实时本地化文本，并按标牌范围缩放；其他自定义标牌保留原文。

新增 BrainMining.Localization.MineralHUDAndFlightSign 回归：创建实际 WBP_MiningHUD 后依次切简中、英文、繁中，检查原有库存标签更新、升级材料名称与既有墙面文案。启用渲染时还导出两种分辨率、初始和高级升级需求，以及三语标牌预览。

本轮最终 Editor 编译通过（`hud-sign-build-final.log`），541 条三语目录与 8 项 Python 测试通过，三语 locres 已重新编译。NullRHI 回归 10 项通过、0 失败、0 未运行，其中 2 项含预期的隔离存档警告（`HUDSignFinalTests/index.json`）。D3D11 独立 HUD/标牌渲染回归 1 项通过、0 警告，导出并核对 15 张新鲜图片（`HUDSignRenderTests/index.json`、`HUDSignRenderUser/Saved/Validation/LocalizationAchievements/HUDRenders`）。英文 720p 四材料列表、繁中 1080p 列表、简中初始列表及英文/繁中标牌已查看，文字完整且名称与数量分离。

上述图片用于独立控件文本排版验证，不代表正式关卡中的灯光、墙面位置或所有 HUD 材质效果。早期把开场测试放入离屏渲染环境时遇到硬件光标加载错误；最终按渲染与 NullRHI 分开验收，以两个最终报告为准。本轮未重新打包发布版本，原有 OverCrowd DLL/PDB 哈希保持不变。

推送前合并了远端截至 `496a44fe` 的博物馆及办公室更新，保留其弹窗内容顺序、六种样式和未读消息计数规则，并为新增内容补充 20 条三语文本。下文原有打包、截图及字体证据对应合并前的 453 条文本；新增文案和合并后的界面仍需再次进行完整视觉验收。

合并后已通过 473 条目录校验、7 项 Python 测试、三语资源重新编译及 Editor Development 编译（`Saved/Validation/LocalizationAchievements/merge-editor-build.log`）。还核对了 35 条弹窗还原源文后与远端定义完全一致，以及新增弹窗译文已进入三语 locres。此次未重新生成 Shipping 包。

普通 PC 测试包已完成 Cook、封装、资源检查和启动检查，三语主菜单切换及偏好保存已实测。尚未标记为“完整发版验收通过”：真实 Steam AppID/后台解锁、Android 构建、完整流程与其余界面布局仍需验证。

## 本地化架构

| 层 | 实现 | 责任 |
| --- | --- | --- |
| 翻译源 | `SourceAssets/Localization/*.json` | 每个稳定键包含三语文本；中文改稿后同步复核译文 |
| 生成器 | `Scripts/localize_game.py` | 检查键和占位符，生成 CSV 与 PO；检查代码字面量引用 |
| UE 文本表 | `Content/Localization/Tables/Game.csv` | 表 ID 为 `BrainMining.Game`；用 `MiningText::Get` 获取 FText |
| 编译资源 | `Config/Localization/Game.ini`、`Content/Localization/Game` | GatherText、导入 PO、编译 locres；随包分发 |
| 语言服务 | `UMiningLocalizationSubsystem` | 自动检测、用户选择、保存、切换与变更事件 |

首次进入且没有手动偏好时，自动模式优先读取 Steam 的游戏语言，随后使用系统语言；未支持的语言回退英文。玩家手动选择最高优先，保存到 GameUserSettings。`schinese` 对应简中，`tchinese` 对应繁中，台湾/香港/澳门中文归入繁中。界面仅展示简中、英文、繁中三个选项，自动模式下高亮实际检测到的语言；内部继续兼容 `auto` 偏好。主菜单在开始与退出之间提供同样式的“设置”按钮，复用游戏内 Esc 菜单的设置面板，语言切换统一放在设置内。关闭主菜单设置后恢复主菜单焦点、光标与开场输入锁。

主菜单设置调整于 2026-10-27 完成完整 Editor 编译与链接（80 项构建操作成功，日志 `Saved/Validation/LocalizationAchievements/opening-settings-build-repaired.log`）。当时内存不足，未运行生命周期测试和截图；2026-10-28 已补跑通过生命周期测试，以及主菜单、主菜单设置、游戏内设置、暂停、图鉴三语共 15 张截图。首次隔离用户目录记录为 `preference:auto`、`resolved:zh-Hans`，截图中仅出现三个具体语言选项。该结果不代替 Steam 客户端语言检测验收。

## 2026-10-28 本地化补漏

新增 `SourceAssets/Localization/Interface.json`，增加 63 条三语词条，目录从 473 条增至 536 条。主要修复：

- 地图标题、图例、流程标签、距离和返回提示改用本地化文本；六个正式区域按稳定 Tag 绑定三语标题和说明，避免地图资产保存的中文在英文模式继续显示。自定义区域保留作者原文。
- 地图按文本宽度缩放，并支持显式换行；右栏根据藏品数量调整行距，距离信息下移，修复第五条藏品与距离提示重叠。
- 铜、铁、孔雀石、钻石统一从本地化文本取名，覆盖正式地图延迟创建矿石的路径；自定义矿石名保留。拾取奖励、收集提示、鸟群离场和列车方向提示接入三语。
- 教学完成提示和收集提示保留 FText 格式历史；提示生成后再切语言，已排队或正在显示的内容随语言更新。
- 主菜单设置的返回提示明确为“返回主菜单”；音频说明及署名补齐三语。修正 31 处英文词内弯引号，避免中文字形使英文缩写出现异常间距。
- 目录校验覆盖条件表达式和数组中的 UI 文本键；新增回归测试检查遗漏键，以及跨语言切换时已有任务提示和矿石名称更新。

本轮证据位于 `Saved/Validation/LocalizationAchievements`：

| 验证项 | 结果与证据 |
| --- | --- |
| 三语目录与生成文件 | 536 条通过 `localize_game.py --check`；三语 CSV、PO、manifest、archive、locres 已重新生成，GatherText 成功，`gather.log` |
| Editor 编译 | 完整构建和最终地图修复后的增量构建均成功，`interface-build.log`、`interface-build-final.log` |
| Python 回归 | 8 项通过，包含条件表达式引用校验 |
| 最终 UE 回归 | 9 项全部通过，0 失败、0 未运行；其中 2 项包含隔离测试存档缺失的预期警告，`InterfaceTestsFinal/index.json`、`interface-tests-final.log` |
| 运行画面 | 主菜单、两种设置入口、暂停、图鉴三语 15 张流程通过；增加地图后的 18 张流程也曾通过，并据此发现和修复区域标题与右栏重叠问题 |
| 最终地图视觉复验 | 21 张流程加入房间地图与区域文本检查；两次完整编辑器运行及一次独立游戏运行均被内存保护终止，尚未取得最终地图成功截图。`RuntimeScreens/report.json` 中旧的 18 张成功结果不代表最终修复通过；必须同时核对报告时间与图片时间 |

最新主菜单设置英文返回提示、英文图鉴缩写已查看实际画面。最终地图修复已编译且最终自动化回归通过，但地图视觉复验仍待内存允许后完成。本轮未重新生成 Shipping/Android 发布包，也未进行 Steam 后台操作。旧原型 `SMiningCollectionView` 的其他展示文案及部分演示场景文本仍需单独迁移；不宣称全工程所有字符串已完成本地化。

普通游戏切换语言和数字区域格式。PIE 使用游戏本地化预览，最后一个本地化实例退出时恢复编辑器原有预览设置。PIE 的语言预览是进程共享状态，多窗口 PIE 不用于验证不同窗口的独立语言。

UI 尽量保留 FText。必须生成字符串/富文本时按文本修订号重建，避免切换语言后显示缓存中文。占位符使用 `FText::Format`，不通过拼接中文句子表达语法。文思描述使用 `This {Noun} is {Property}.` 等模板，保留同一个可拖拽属性控件。

字幕按实际语言计算时长：中文按字符，英文按词组，并保留标点停顿。切换语言时重新开始当前句的计时/合成声音，不清除待播队列；句间停顿保持。未来增加真人配音时需另外设计每语言音频引用，当前字幕不等于已有三语真人配音。

### 日常改稿

```powershell
python Scripts/localize_game.py
python Scripts/localize_game.py --check
python Scripts/localize_game.py --import-po PATH_TO_REVIEWED_PO --culture en
powershell -File Scripts/build_localization.ps1 -RunTests
powershell -File Scripts/run_subtitle_localization.ps1 -Mode verify
powershell -File Scripts/verify_localized_popup_art.ps1
```

以 JSON 为源；不要只改生成的 CSV/PO。PO 导入适用于已有语言的译文更新。字幕键当前采用 `Subtitle.<资产名去掉DA_>.<三位行号>`，资产重命名、插入或重排行必须视为一次键迁移并复核翻译。现有旧字幕安装脚本仍可能写入原始文本，重跑后先更新 JSON、生成目录，再执行 `run_subtitle_localization.ps1 -Mode apply`。正式 PC 打包已增加 verify 门禁，未迁移或源文不一致时拒绝继续。

### 增加第四种语言

1. 为全部 JSON 词条添加新语言值，并审校术语、标点和占位符。
2. 扩充生成器的语言集合、`UMiningLocalizationSubsystem` 的语言映射/选项和测试。
3. 在 `Config/Localization/Game.ini` 中增加 CulturesToGenerate，在 DefaultGame.ini 中增加 CulturesToStage；更新构建脚本的资源检查列表。
4. 检查字体覆盖、换行、按钮扩张、输入设备提示与阅读时长。日语/韩语不能直接照搬英文分词节奏；RTL 语言还需要布局方向专项适配。
5. Steam 支持该语言时扩展 `configure_steam.py` 的语言映射，并维护 Steam 后台成就译文与商店语言声明。

业务逻辑不需为每种语言复制。当前三语集合仍由上述几处显式配置管理，尚未做任意语言的运行时插件注册。

## 成就规则与存档

| API 名称（发布后保持稳定） | 显示名 | 解锁依据 |
| --- | --- | --- |
| ACH_FIRST_COLLECTION | 第一件灵感 | 首次获得藏品 |
| ACH_COLLECTION_5 | 灵感正在聚集 | 五种不同藏品；重复获取不累计 |
| ACH_COLLECTION_COMPLETE | 灵感收藏家 | 首发集合 ID 0–22 全部获得 |
| ACH_PICKAXE_UPGRADE | 再锋利一点 | 矿镐达到等级 1 |
| ACH_PICKAXE_MAX | 挖到底 | 矿镐达到等级 4 |
| ACH_CARD_ROOM_CLEAR | 组合的力量 | 卡牌房完成事件 |
| ACH_MUSEUM_CLEAR | 换个想法 | 正常通过博物馆出口进入结局 |
| ACH_CAMPAIGN_COMPLETE | 挖哦！ | 正式结局完成事件 |

`MiningAchievementState` 只负责规则；`MiningAchievementSubsystem` 接收游戏事件并保存；`MiningAchievementProvider` 对接平台。矿石兑换藏品 12–15 分别要求累计 25、15、10、5 个对应矿石。全收集要求固定为首发集合，未来加藏品不会使已发布成就失效。

成就档案独立于流程存档，采用 `BrainMiningAchievements_v1_<owner>` 槽，包含唯一藏品集合、最高矿镐等级、已解锁 ID 与修订号。主档/pending/backup 恢复选择合法最高修订；先落盘再同步，损坏且无法恢复时保留文件并停止写入。开始新游戏不清除账户成就。

同一 owner 的有效流程档可补发藏品、升级和完成结局。旧档没有可靠证据证明已完成卡牌/博物馆时不猜测补发。无 owner 的旧档只允许迁入 local/qa；不自动认领到 Steam。local、Steam A、Steam B 之间不搬运成就或待同步队列。加载不匹配的流程档仍可游玩，但该轮不产生平台成就；新建正常流程恢复资格。

新存档另存 `HasVerifiedAchievementCollection` 与 `AchievementCollectionIds`，只记录当前账户已通过正常流程获得的藏品。玩家可以保留图鉴开始新游戏，但保留的展示内容不会在重启后被误认为当前账户的有效成就证据。此规则也防止 GM 图鉴在新流程中洗成正常记录。

PIE、commandlet、自动化测试、试玩地图不计成就。GM 物品授予、跳关、跳过玩法会取消本轮资格；`-MiningQA` 必须带独立 UserDir，使用 qa 档案且不接 Steam。测试不能污染开发者真实成就。

## Steam 接入与发布

项目使用现有 UE OnlineSubsystemSteam，不引入新的收费插件。只向 UE 成就接口写入 `100.f`；不把 5/23 之类的进度传给 WriteAchievements。已核对本地 UE 5.8 实现：正值被当作解锁，不是部分进度。若未来需要进度条，单独设计 Steam Stat/进度提示。

同步顺序为：校验 AppID 和 SteamID → 查询八项定义/远端缓存 → 计算本地已解锁且远端未完成的差集 → 写入 → 再次查询确认。失败时保留本地记录，按退避重试；超时/过期回调忽略，回调阶段再次校验账号。Steam 本身也有离线缓存，本地账本只增加解锁，不对远端执行清除或回退。

用户已提供正式 AppID `5506030`，`Config/Custom/SteamPC/DefaultEngine.ini` 中的 `SteamDevAppId` 与 `ExpectedAppId` 均已配置为该值。普通 PC/Android 运行仍不依赖 Steam；SteamPC 运行时会校验实际 AppID 与该值一致。配置完成不代表 Steam 后台定义已发布或真实解锁已验证。禁止用 Spacewar 480 作为正式验证。

```powershell
python Scripts/configure_steam.py --app-id 5506030
python Scripts/configure_steam.py --check
powershell -File Scripts/package_main_game.ps1 -CustomConfig SteamPC -PlanOnly
powershell -File Scripts/package_main_game.ps1 -CustomConfig SteamPC
```

`Docs/SteamAchievementDefinitions.csv` 与 `Config/SteamAchievementDefinitions.json` 是后台录入/核对材料，含八项 API 名和三语名称/描述；不是宣称 Steam 支持任意 CSV 自动导入。脚本不会操作后台或上传构建。

后台配置顺序：创建同名成就（客户端可设置）→ 录入三语显示文本 → 上传解锁/未解锁图标 → 保存并发布配置 → 上传测试分支构建 → 用有权限的测试账号从 Steam 库启动。API 名与代码必须完全一致。真实 Steam overlay、重启后的状态、离线后联网、重复触发以及两账号隔离需要在自己的 AppID 下验收。

SteamPC 必须重新编译并 Cook，打包检查 steam_api64.dll 且拒绝分发 steam_appid.txt。安装版 UE 的共享构建环境不允许通过游戏 TargetRules 的 EnablePlugins/DisablePlugins 切换插件，因此项目在 Windows 上保留 Steam 插件声明，普通 PC 使用 Null OSS、bEnabled=False，且不编译游戏侧 Steam 适配器。普通 PC 包可能仍带引擎 Steam SDK 库，但不要求安装/登录 Steam。Android 由项目 PlatformAllowList 排除整个 Steam 插件。若要求普通 Windows 包连 Steam DLL 也完全不含，需要另行维护发行描述文件或源码引擎独立构建，不能绕过 UBT 的共享构建校验。

安装版引擎的 Shipping OSS 使用编译时 `UE_PROJECT_STEAMSHIPPINGID` 处理重启；仅设置 SteamDevAppId 不能保证双击 EXE 自动拉起正确应用。当前发布路径要求从 Steam 库启动；若要支持直接 EXE 自动拉起，需另外验证源码引擎重编或启动器方案，不能只在游戏模块随意加宏就声称生效。

## 验证证据与未完成项

报告均在 `Saved/Validation/LocalizationAchievements`。截至 2026-10-27，已取得以下证据：

| 验证项 | 结果与证据 |
| --- | --- |
| 三语目录与 Steam 定义 | 453 条三语文案、8 项三语成就定义校验通过；未写入 Steam 后台 |
| Python 回归 | 7 项通过 |
| UE 核心自动化 | 7 项通过，包含账户隔离、存档恢复、图鉴证据、语言切换与字幕节奏；`Tests/index.json` 中 6 项成功、1 项带预期存档警告成功，0 失败 |
| 字体覆盖 | SourceHanSansCN Medium/Bold 与结局 Xiaolai 对全部 453 条三语文案检查无缺字；`font-coverage.json` |
| 正式字幕资产 | 47 个资产中的 54 行字符串表引用 apply/verify 通过 |
| 办公室实体弹窗渲染 | D3D11 实际渲染测试通过；三种窗口乘三种语言共 9 张截图已检查背景、颜色、换行；`RenderTests/index.json`、`popup-language-contact.png` |
| 正式地图 PIE 界面 | 主菜单/设置/暂停/图鉴三语共 12 张实际截图已生成并检查；`RuntimeScreens/report.json`、`RuntimeScreens/contact.png`；预览约 1280×720，不代表 1080p、Android 或所有图鉴条目均已验收 |
| 普通 Windows Shipping | 编译及链接成功；`win64-shipping-build.log` |
| 最终按钮修复 | 中文标签按可用空间缩放，悬停/按下使用按钮样式前景色；Editor 与普通 PC Shipping 重新编译通过；`final-selector-build.log` |
| 普通 PC 完整包 | Cook、IoStore 封装、归档及包内资源检查成功；`PCPackageFinalReport`、`pc-package-final-run.log` |
| 发布包运行观察 | 已实际启动、切换简中/英文/繁中、进入正式起始房间，并确认重启保留英文/繁中偏好；最终通过窗口关闭退出，进程返回 0；`pc-package-runtime-close-resources.csv` |
| SteamPC Windows Shipping | 编译及链接成功；`win64-steampc-shipping-build.log`；该历史构建使用 AppID 0，仅证明可编译；配置为 5506030 后尚未重新构建验收 |
| Android arm64 Shipping | 使用 NDK r27c 完成编译检查，因两处已有 Clang 错误失败，未生成本次发布包；`android-shipping-build.log` |

两种 Windows 配置使用同一输出文件名，会相互覆盖。当前最终输出为普通 PC 配置，测试包入口为 `Saved/Validation/LocalizationAchievements/PCPackage/Windows/WOoOW.exe`；复制测试包时必须复制整个 Windows 目录。

首次隔离 Cook 输出成功，但 UAT 在封装时追加 Windows 子目录，导致找不到清单。第二次使用以 Windows 结尾的 CookOutputDirectory 后完整打包通过。指定该参数时应提供完整平台目录，例如 `Saved/Validation/LocalizationAchievements/PCCookFinal/Windows`。

随后只修改了原生语言按钮排版与悬停颜色，未改资源或序列化字段；最终 Editor/Shipping 编译通过后，将最新普通 PC Shipping EXE 放回已验证的包内，并核对源文件与包内文件 SHA256 一致（`selector-package-hashes.json`）。发布包中的最终按钮已再次通过实际窗口检查。SteamPC 的编译证据早于这次共用 UI 微调，真实 AppID 下的最终构建及联调仍待完成。

前几次交互观察达到运行守卫时限后被终止，不能当作正常退出测试；最后一次使用窗口关闭退出，返回 0。关闭确认弹窗的繁中文案已观察到，但通过该弹窗退出的完整点击流程未另行验收。运行观察只覆盖上述画面与行为，不能替代完整流程测试。12 张 PIE 图片生成于最后按钮修复之前，最终按钮效果以发布包实际窗口观察为准。

Android 的两个阻塞点为 `Source/BrainMining/StartRoom/MiningStartRoom.cpp:139` 与 `Source/BrainMining/Showcase/MiningExhibitHall.cpp:176`：查找函数在 for 循环体内无条件返回，Clang 的 `-Werror,-Wunreachable-code-loop-increment` 拒绝编译。已对照 HEAD 确认两处原写法在本次改动前存在，本轮未改动这些无关函数。没有降低编译器检查或修改全局 Android SDK 设置；本次构建仅在进程内选择已安装的 NDK 27.2.12479018。

需要继续验收：

- 三语实际场景截图仍需覆盖升级、文思、卡牌、办公室游戏场景与结局，以及 PC 1080p 和 Android 横屏。`Scripts/capture_localization_runtime.ps1` 已成功运行主菜单/设置/暂停/图鉴共 12 张截图流程，检查了本次画面中的文字与换行。开场前两张捕获时品牌 Logo 尚在进入显示过程，第三张已显示；这些帧不能作为开场动画或资源加载时序验收。办公室英文已增加固定区域缩放，但仍需检查最小可读字号。
- 开场保留用户提供的“挖哦 / WOoOW”品牌 Logo；署名保持原名。办公室实体弹窗改用三张共享的 Slate 渲染目标，把三语正文绘制到原有空白窗口皮肤；飞散窗口和地面实例共用材质，切语言时重绘一次。原 PhysicalPopup 资产保留但正常游戏用本地化材质覆盖。其余旧式窗口皮肤中的英文是原画的一部分。需检查实际灯光下的颜色与可读性。
- `Scripts/audit_achievement_collectibles.py` 已验证正式保存地图：18件静态放置、4件矿石奖励、1件逃生庆典动态生成，首发集合无缺项。这只证明配置来源存在，矿石产量和每条路线的可达性仍需游玩验收。
- 普通 PC 包已生成并启动，仍需完整关卡验收；SteamPC 自有 AppID 构建、真实解锁与 Android 包尚未完成。
- AppID `5506030` 已配置；仍需确认已发布的后台定义、图标与测试账号/分支访问。无需在聊天中提供账号密码或 API 密钥。

资源守卫拒绝或终止的运行只记为未通过，不调整安全阈值绕过，不终止无关应用。已有 OverCrowd DLL/PDB 用户修改保持不动。

## 剩余验收步骤

| 场景 | 操作与通过条件 |
| --- | --- |
| 手动语言偏好 | 依次选择三种语言，退出重启后仍保持选择；使用无语言偏好的独立用户目录分别验证系统语言和 Steam 游戏语言，自动模式不作为玩家可选项显示 |
| 活动界面切换 | 打开设置、图鉴、升级和暂停界面后切语言，已显示文字同步更新；重进房间仍使用当前语言 |
| 排版与谜题 | 在 1080p、720p 和 Android 横屏检查英文最长文案、繁中缺字；文思可拖动属性与英文语序均正确；卡牌数值和点击区域未被遮挡 |
| 字幕与结局 | 播放中、手动暂停时和句间切语言，检查当前行计时、队列、音效与完成回调；走完结局并核对署名和退出入口 |
| Steam 正常解锁 | 专用测试账号从 Steam 库启动未解锁状态的构建，正常取得首件藏品；检查客户端状态，退出重启后仍已解锁；重复拾取不重复计数 |
| 离线同步 | 已登录并成功初始化的测试环境断网，正常达成另一项成就；重启后本地记录仍在，同一账号恢复联网后确认远端状态 |
| 账户与旧档 | A 账号进度不解锁 B 账号成就；无 owner 旧档和 local 档不自动迁入 Steam；保留展示图鉴新开流程后重启也不补发未验证藏品 |
| 禁止测试污染 | PIE、QA、GM 获得藏品/跳关不影响真实 Steam 成就；QA 存档目录独立；不要使用真实玩家档案做破坏性恢复测试 |
| 全收集与关卡 | 实际走完 23 件藏品来源及矿石兑换，验证矿石产量充足；卡牌、博物馆、结局成就在正式完成事件触发 |
| 发布包 | 从空安装目录启动对应版本，检查三语资源、字幕和办公室实体窗口；普通 PC 无 Steam 客户端仍可运行；Android 真机启动并验证触控 |

真实 Steam 验收将使用 AppID `5506030`，仍需已发布的八项后台定义，以及可用测试分支/测试账号权限。只需确认准备状态，不需要密码或密钥。完整场景截图及 cook 在资源守卫允许后执行；没有新鲜成功报告的检查保持未通过状态。

## 官方参考与本地核对

- Valve：Stats and Achievements、ISteamUserStats，核对配置发布、SetAchievement/StoreStats 和离线缓存语义（2026-10-27 查询）。
- 本地 UE 5.8：OnlineAchievementsInterface、OnlineAchievementsSteam、OnlineSubsystemSteam.cpp 的 GetRelaunchSettings，以及 TextLocalizationManager 的游戏预览 API。
