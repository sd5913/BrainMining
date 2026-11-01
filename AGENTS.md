# 打包与 Steam 发布约定（所有 AI 助手先读）

本项目有 **Demo** 和 **正式版 Full** 两个 Steam 应用。完整流程见 [Docs/MainGamePackaging.md](Docs/MainGamePackaging.md)。

- 用户说“打包”“出包”“上传”时，先从本次对话确认目标是 **Demo / 正式版 / 两者**。对话已明确时继续执行，不重复询问；确实没说明时先问清楚，不能根据上一次配置或 ZIP 名猜测。非 Steam 离线包也需明确。
- Demo：AppID `5524430`，DepotID `5524431`。正式版：AppID `5506030`，DepotID `5506031`。新版本沿用对应 Depot，更新 BuildID / manifest；不要每次创建新 Depot。
- Steam 发布包必须同时具备 **正常 Steam 成就 + 完整 PCSize 压缩**。运行 `python Scripts/configure_steam.py --app-id <目标AppID>`，然后 `package_main_game.ps1 -CustomConfig SteamPC`。普通 `PCSize` 包不包含 Steam 成就适配器，不能作为 Steam 交付包替代。
- **正式版不继承 Demo 的游戏存档、收藏进度或成就进度。** 正式版从独立新档开始；成就定义、名称、说明及隐藏规则可相同，进度必须隔离。不得把 Demo 的本地成就档案或存档迁入正式版。保留已发布 Demo 的兼容性，不删除用户旧档。
- 两种包使用不同的输出/报告/ZIP 路径，保留旧成品。不要只改文件名或手工替换 AppID 后就宣称是新构建；配置、编译、cook、成品运行信息、SteamPipe VDF 中的目标必须一致。共享源配置不可同时切换 AppID 并行打包。
- 发布前检查 `GeneratedEngine.ini`、`GeneratedDeviceProfiles.ini`、`BRAINMINING_WITH_STEAM=1`、11 个成就 API 名、资源完整性、成品运行和包体大小。约 1.18 GB ZIP 是 1.6.4 的参考值；明显增大先查 PCSize 配置继承，不要归因于 Steam DLL。
- 上传时保留外层 `Windows/` 目录，启动路径为 `Windows/WOoOW.exe`。上传成功不等于设为 `default`，设为 `default` 也不等于首次公开发售。按用户授权范围执行并报告实际 BuildID、分支状态、测试结果及剩余操作。
- 正式版/Demo 后台成就定义分别配置并发布；不能只检查本地 INI。测试不得重置真实玩家成就、修改其旧档或使用 GM/QA 行为解锁真实成就。`-MiningQA` 必须配独立 `-UserDir`，自动场景测试使用 `-NoSteam`。
- 不把 Steam 密码、验证码或登录令牌写入项目、脚本、文档、日志或交付包。
