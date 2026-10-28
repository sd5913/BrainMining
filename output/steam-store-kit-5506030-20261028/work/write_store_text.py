from pathlib import Path
import csv, json, html, shutil

root=Path(__file__).resolve().parents[1]
texts=root/'01_商店文案';texts.mkdir(exist_ok=True)
docs=root/'10_填写说明与核验';docs.mkdir(exist_ok=True)
def write(path,body):
    p=root/path;p.parent.mkdir(exist_ok=True,parents=True);p.write_text(body.strip()+'\n',encoding='utf-8-sig')

shorts={
'schinese':'《挖哦》是一款以“涌现”为灵感的第一人称挖掘探索游戏。深入脑海，挖开岩层，发现奇怪藏品，闯进不按常理运作的房间。在探索与解谜中寻找联系，让关于这款游戏的点子一点点浮现。',
'english':'Dig through a mind in search of a game idea. WOoOW is a surreal first-person digging adventure: carve through rock, uncover odd collectibles, and explore rooms with unexpected rules as you discover what "emergence" might mean.',
'tchinese':'《挖哦》是一款以「湧現」為靈感的第一人稱挖掘探索遊戲。深入腦海，挖開岩層，發現奇怪藏品，闖進不按常理運作的房間。在探索與解謎中尋找聯繫，讓關於這款遊戲的點子一點點浮現。'
}
sections={
'schinese':[
('灵感还没出现？那就进脑袋里挖一挖。','Game Jam 的主题公布了：“涌现”。是时候起身，出去挖挖灵感了。\n\n《挖哦》是一款第一人称挖掘探索游戏。你身处脑袋之中，拿起工具，一路挖开岩层、发现藏品、穿过奇怪的房间。过程中的遭遇与发现，都是你对“涌现”的思考。'),
('把前方挖成一条路','挥动矿镐，让岩层在一次次敲击中改变形状。循着矿物和环境中的线索继续深入，收集资源、升级工具，再用新的工具打开更多可能。前路不只是被发现的，也可以被你挖出来。'),
('走进不按常理运作的房间','矿道另一端，可能是关不完弹窗的办公室，也可能是需要靠敲击辨认方向的黑暗通路。尝试房间各自的规则，经历尺度失常的遭遇、卡牌交锋与关于想法的谜题，再把这些看似无关的片段连接起来。'),
('收藏那些奇怪的小念头','留意矿壁和角落里的线索，把藏在岩层中的物件一点点挖出来。收进图鉴，旋转端详，看看它们会让你联想到什么。它们既是藏品，也是这趟灵感旅程留下的痕迹。'),
('让点子在探索中涌现','从熟悉的房间走向陌生的空间，不断前进、观察、尝试。你不是在寻找一道标准答案，而是在亲手走过一个点子逐渐成形的过程。'),
('游戏特色','• 第一人称挖掘：开凿岩层，收集资源，逐步获得并升级工具。\n• 奇遇与谜题：穿过风格和规则各异的手工设计场景。\n• 藏品与图鉴：追寻环境线索，发掘并观察奇怪物件。\n• 低多边形美术：暖色房间、幽暗矿道与超现实空间。\n• 单人探索体验：跟随自己的好奇心，一路挖掘灵感。')
],
'english':[
('No inspiration yet? Try digging around inside your head.','The Game Jam theme is "Emergence." Time to get up and dig for an idea.\n\nWOoOW is a first-person digging adventure set inside a mind. Pick up your tools, carve through rock, uncover curious objects, and step into unexpected rooms. Every encounter is another way to explore what emergence might mean.'),
('Dig your own way forward','Swing your pickaxe and reshape the rock around you. Follow mineral deposits and clues in the environment, gather resources, and upgrade your tools. The way ahead is something you can uncover—and something you can make.'),
('Discover rooms with unexpected rules','A tunnel might lead to an office full of persistent pop-ups, or a dark passage you navigate by striking its surfaces. Learn each room’s rules, encounter strange changes of scale, take part in a card confrontation, and puzzle over the connections between ideas.'),
('Collect curious little thoughts','Look closely at the rock and the corners of each space. Follow the clues, dig out unusual objects, and add them to your collection journal. Rotate and inspect them: what do they remind you of? Each one is a small trace of your search for inspiration.'),
('See an idea emerge','Keep moving, looking, and experimenting as familiar rooms give way to unfamiliar spaces. Rather than searching for one correct answer, you are playing through the process of an idea taking shape.'),
('Features','• First-person digging: carve through rock, gather resources, and acquire and upgrade tools.\n• Curious encounters and puzzles across handcrafted spaces with changing rules.\n• Environmental clues, unusual collectibles, and an inspectable collection journal.\n• Simple low-poly art, from warm rooms to shadowy tunnels and surreal spaces.\n• A single-player journey guided by curiosity and the search for inspiration.')
],
'tchinese':[
('靈感還沒出現？那就進腦袋裡挖一挖。','Game Jam 的主題公布了：「湧現」。是時候起身，出去挖挖靈感了。\n\n《挖哦》是一款第一人稱挖掘探索遊戲。你身處腦袋之中，拿起工具，一路挖開岩層、發現藏品、穿過奇怪的房間。過程中的遭遇與發現，都是你對「湧現」的思考。'),
('把前方挖成一條路','揮動礦鎬，讓岩層在一次次敲擊中改變形狀。循著礦物和環境中的線索繼續深入，收集資源、升級工具，再用新的工具打開更多可能。前路不只是被發現的，也可以被你挖出來。'),
('走進不按常理運作的房間','礦道另一端，可能是關不完彈出視窗的辦公室，也可能是需要靠敲擊辨認方向的黑暗通路。嘗試房間各自的規則，經歷尺度失常的遭遇、卡牌交鋒與關於想法的謎題，再把這些看似無關的片段連接起來。'),
('收藏那些奇怪的小念頭','留意礦壁和角落裡的線索，把藏在岩層中的物件一點點挖出來。收進圖鑑，旋轉端詳，看看它們會讓你聯想到什麼。它們既是藏品，也是這趟靈感旅程留下的痕跡。'),
('讓點子在探索中湧現','從熟悉的房間走向陌生的空間，不斷前進、觀察、嘗試。你不是在尋找一道標準答案，而是在親手走過一個點子逐漸成形的過程。'),
('遊戲特色','• 第一人稱挖掘：開鑿岩層，收集資源，逐步獲得並升級工具。\n• 奇遇與謎題：穿過風格和規則各異的手工設計場景。\n• 藏品與圖鑑：追尋環境線索，發掘並觀察奇怪物件。\n• 低多邊形美術：暖色房間、幽暗礦道與超現實空間。\n• 單人探索體驗：跟隨自己的好奇心，一路挖掘靈感。')
]}
for lang,body in shorts.items():
    assert len(body)<=300,(lang,len(body))
    write(f'01_商店文案/{lang}_short.txt',body)
    plain='\n\n'.join(title+'\n'+text for title,text in sections[lang])
    bb='\n\n'.join('[h2]'+title+'[/h2]\n'+text for title,text in sections[lang])
    write(f'01_商店文案/{lang}_about_plain.txt',plain)
    write(f'01_商店文案/{lang}_about_bbcode.txt',bb)

write('01_商店文案/游戏名称与基本信息.txt','''推荐基础名称
简体中文：挖哦
繁體中文：挖哦
English：WOoOW

副标题是可选项，不默认更改现有 Steam App 名称：
简体中文：挖哦：关于“涌现”的灵感
English：WOoOW: Inspired by Emergence
此前英文副标题仅为讨论建议；最终是否采用仍由团队决定。

应用类型：Game
主类型建议：Adventure / 冒险
次类型建议：Casual / 休闲、Indie / 独立（仅勾选后台提供且符合定位的选项）
已知玩法：单人；第一人称；挖掘、资源与工具升级、场景探索、谜题、藏品图鉴。
计划平台：Windows PC。Android 素材不代表 Steam 自动提供移动端。
开发者、发行商、发行日期、价格、支持邮箱和网站：请使用团队真实资料，未代为编造。
如本次只是准备商店页，可先保存草稿；发行日期与是否发布 Coming Soon 由团队确认。
''')

tags=[
('Exploration','探索','核心活动是推进、观察、发现不同空间'),
('Mining','采矿','挖掘岩层、矿物及工具成长是主要玩法'),
('First-Person','第一人称','游戏的实际观察与操作视角'),
('Surreal','超现实','身处脑海、尺度变化和跨空间奇遇'),
('Puzzle','解谜','房间交互与想法相关谜题'),
('Adventure','冒险','以连续场景和奇遇串联体验'),
('Collectathon','收集马拉松','沿环境线索发现藏品并收入图鉴；若团队不希望突出收集可后移'),
('3D','3D','三维场景'),
('Stylized','风格化','明确的简洁低多边形视觉'),
('Atmospheric','氛围','矿道、房间及声音反馈塑造空间氛围'),
('Funny','欢乐','无厘头藏品及反常识房间带来幽默感'),
('Destruction','破坏','玩家实际挖开、改变岩体'),
('Singleplayer','单人','当前为单人本地体验'),
('Indie','独立','独立游戏定位'),
('Casual','休闲','作为较低优先级定位；不承诺所有段落都无难度')]
with (texts/'tags_priority.csv').open('w',encoding='utf-8-sig',newline='') as f:
    w=csv.writer(f);w.writerow(['priority','steam_tag_english','中文参考','选择依据']);w.writerows((i+1,*row) for i,row in enumerate(tags))
write('01_商店文案/标签与类型填写建议.txt','建议顺序（英文名称方便在 Tag Wizard 中检索；中文标签名称以后台实际显示为准）：\n\n'+'\n'.join(f'{i+1}. {en} / {cn}：{why}' for i,(en,cn,why) in enumerate(tags))+'''

优先保证前五项准确，再根据 Tag Wizard 的实际可选项调整。
没有必要凑满 20 个。本包推荐 15 项，避免弱相关标签稀释定位。
不建议：Open World、Survival、Crafting、Roguelike、Deckbuilding、Multiplayer、Psychological Horror。
原因：现有主流程为手工设计的连续探索；卡牌房是遭遇玩法，不能据此宣传为随机肉鸽牌组构筑游戏；截图较暗也不等于恐怖游戏。
''')

write('01_商店文案/功能与语言选项.txt','''可依据现有玩法选择：
Single-player / 单人。
Windows：已有 PC 工程和视频；请用准备发布的 Windows 构建最终验收。

语言建议：
简体中文、English、繁體中文：项目已有 UI 与字幕文本资源，可作为拟支持语言。
勾选前请在最终上传的 Steam 构建中逐项确认界面和字幕。商店页翻译不等于游戏支持该语言。
Full Audio / 完全音频：本包没有提供三语真人配音证据，不要因有音效或字幕就勾选。
英语/繁中商店文案已经准备；现有截图和视频仍主要显示中文 UI，不伪装为已经重新录制的外语版本。

暂不承诺，验证后再勾选：
Steam Achievements：工程已有 8 项定义和适配代码，后台定义及真实 Steam 解锁仍需验收。
Steam Cloud：没有云存档上线证据。
Controller Support：没有完整手柄导航与全流程验收证据。
Steam Deck：没有 Deck 实机或兼容性验收证据。
VR、联机、创意工坊、远程同乐：没有相应功能证据。

09_原始LOGO与配置参考 中的成就 CSV 是已有工程的参考，不是商店页的导入表，也不代表成就已经在 Steam 后台设置完成。
''')

write('01_商店文案/系统需求_待实测.txt','''以下是核对表，不是可以直接对外发布的硬件承诺。
Minimum 与 Recommended 均应来自实际 Windows Shipping 包测试，不用开发机配置冒充最低配置。

64-bit：当前目标为 Windows 64 位构建，核对实际上传包后勾选。
操作系统：待实测确认 Windows 版本与补丁要求。
处理器：待最低/推荐配置测试。
内存：待测试（不要在没有数据时直接填写 8 GB / 16 GB）。
显卡：待最低/推荐配置测试。
DirectX：项目配置默认使用 DX12；请核对实际打包后端及所需功能级别，不能据此直接推断最低显卡。
存储空间：以最终安装目录实占空间、解压/更新余量核定。
声卡：按实际需要填写；无特殊需求可保持默认说明。
附注：最终确认键鼠操作、字幕语言、存档行为与闪光/快速运镜等用户体验提示。

未验证硬件前，保留这些项目为待确认，不要把本文件的“待实测”直接粘贴到公开商店页。
''')

write('01_商店文案/团队需确认的信息.txt','''明天填写前，请补齐这些无法从宣传素材确定的真实信息：
1. 正式名称是否保留“挖哦 / WOoOW”，是否启用副标题。
2. 开发者和发行商的公开名称、支持邮箱及可公开的网站/社媒地址。
3. 定价/是否免费、预计发行时间、是否先发布 Coming Soon；不替团队设定价格或日期。
4. Windows 最低与推荐配置，以及最终安装所需空间。
5. 简中/英文/繁中最终发布包的界面、字幕和语音实际支持情况。
6. Steam 成就、云存档、手柄与 Deck 等功能的真实验证状态。
7. 内容调查：根据完整最终构建作答，包括风格化爆炸、暗场、闪光与任何素材中的成人内容；本包不作年龄评级。
8. AI 内容调查：已知本包部分商店背景画面由生成式 AI 辅助制作；LOGO 为团队原图直接合成。游戏内美术、声音、叙事和本地化是否使用 AI，以及是否存在运行时生成，请由团队依实际情况补齐。不能仅凭这份商店包替整个游戏作否定声明。
9. 已有作品、音效、贴图、视频音乐等的使用权，依团队实际授权记录确认。

AI 说明可参考的已知部分（请结合完整游戏情况补充后使用）：
中文：部分商店宣传画面使用生成式 AI 辅助制作，并结合游戏现有视觉素材进行排版；游戏 LOGO 使用团队提供的原始设计。
English: Some store promotional artwork was prepared with the assistance of generative AI and composed using visual references from the game. The game logo uses the original design supplied by the team.
以上说明只覆盖本次商店物料；不替代完整内容调查。
''')

write('05_视频/视频填写说明.txt','''建议顺序：
1. 01_gameplay_trailer_1920x1080_30fps.mp4
   中文标题：实机玩法预告
   English title: Gameplay Trailer
   类别建议：Gameplay（已抽查开头及多处内容，主要为真实玩法剪辑；请最终通看确认）
   时长约 1:44。来源为“宣传片.mp4”，H.264 + AAC、1080p、30 fps，保留原编码文件。
   封面：01_trailer_poster_1920x1080.png，直接来自该视频 3 秒处。

2. 02_extended_gameplay_1920x1080_30fps.mp4
   中文标题：流程实机演示
   English title: Extended Gameplay
   类别建议：Gameplay
   时长约 1:30。来源为“实机.mp4”，由 HEVC 50 fps 转为 H.264 30 fps / AAC 双声道，未改内容顺序或另配声音。
   封面：02_extended_poster_1920x1080.png，来自该源视频 65 秒处。

视频中的中文 UI/字幕没有被替换成英文。不要把现有中文内容标成专门重新制作的英文配音/英文 UI 预告片。
上传后等待 Steam 转码，并检查画面、声音、缩略图和顺序。自定义预告片封面必须使用对应视频本身的帧，本包已按此方式制作。
本包不包含“开始购买”“虚假下载”等营销按钮。
''')

write('06_介绍配图/使用说明.txt','''这些 GIF 均截取自团队提供的真实视频，未生成或改画玩法：
01_digging.gif：宣传片 3–7 秒；8 fps，宽 616 px。
02_office.gif：实机视频 16–20 秒；8 fps，宽 616 px。

可在“关于这款游戏”的挖掘段、奇遇房间段插入；也可只用文字。
每张均小于 5 MB，两张总计约 5.6 MB。上传到 Steam 描述编辑器后插入，切勿粘贴本地文件路径当作网页图片地址。
视频原有 HUD 中包含中文；英文商店页如需纯英语演示，请用对应语言的最终构建重录。当前长描述即使不插图，也已经可用。
''')

write('07_实机截图/顺序与原图说明.txt','''建议按文件名前缀 01–07 的顺序上传。
七张均来自桌面文件夹的原始 2560×1440 截图；只复制和改文件名，不裁掉 HUD、不生成玩法、不添加文案。
前两张展示挖掘与矿洞，随后依次展示办公室、回声、巨人房间、博物馆和红色矿洞，覆盖主要场景差异。
至少先上传 01–05；有位置时上传全部七张。
“介绍.png”是带大段文字的宣传图，没有混入实机截图栏。
对成人内容/全年龄展示的标记请按实际最终内容判断，不以文件名代替审核。
''')

sources=[
('Steam 图形资产总览','https://partner.steamgames.com/doc/store/assets'),
('商店图形资产','https://partner.steamgames.com/doc/store/assets/standard'),
('游戏库资产','https://partner.steamgames.com/doc/store/assets/libraryassets'),
('图标','https://partner.steamgames.com/doc/store/assets/community'),
('描述文字','https://partner.steamgames.com/doc/store/page/description'),
('标签','https://partner.steamgames.com/doc/store/tags'),
('预告片','https://partner.steamgames.com/doc/store/trailer'),
('图形资产规则','https://partner.steamgames.com/doc/store/assets/rules'),
('内容调查','https://partner.steamgames.com/doc/gettingstarted/contentsurvey'),
('编辑与保存','https://partner.steamgames.com/doc/store/page')]
write('10_填写说明与核验/官方来源.txt','核对日期：2026-10-28（香港时间）\n\n'+'\n\n'.join(n+'\n'+u for n,u in sources))
write('10_填写说明与核验/制作依据与限制.txt','''本包没有登录、修改、保存或上传 Steamworks 后台；是供团队手动填写的本地资料包。
所有公开文案均为建议稿，未擅自设置价格、日期、开发者信息或硬件配置。
玩法依据：用户在本聊天中的介绍、“介绍.png”、七张实机截图、两段视频，以及项目中的 README_Collection.md、Docs/MainGameCollections.md、README_CardRoom.md、README_EchoPassage.md 和 Docs/LocalizationAndAchievements.md。
不宣传无限随机世界、生存建造、肉鸽构筑、多人、真人三语配音或未经验收的 Steam 功能。
采用素材文件夹当前 logo2.png，不改字形/笔画/纹理。只裁去外围透明空白并等比缩放、合成。
已有背景与当前 Steam 竖版背景使用内置 image_gen 制作；Steam 竖版补图为无字背景，LOGO 由原 PNG 后期合成。
电脑正面右/下边缘使用之前确认的局部修正，避免将选中白描边当作实际材质；游戏库壁纸同样处理。
七张实机截图保持原像素；预告片封面直接提取对应视频原帧；图文展示动画只由原视频截取。
GIF 为优化页面加载做了尺寸/帧率调整，未生成玩法。
本地 index.html 是内部预览和复制工具，不是 Steam 后台或已经发布的网页。
''')

mapping=[
('Header Capsule','02_商店图片/header_capsule_920x430.png','920×430'),
('Small Capsule','02_商店图片/small_capsule_462x174.png','462×174'),
('Main Capsule','02_商店图片/main_capsule_1232x706.png','1232×706'),
('Vertical Capsule','02_商店图片/vertical_capsule_748x896.png','748×896'),
('Page Background（可选）','02_商店图片/page_background_1438x810.png','1438×810'),
('Library Capsule','03_游戏库图片/library_capsule_600x900.png','600×900'),
('Library Header','03_游戏库图片/library_header_920x430.png','920×430'),
('Library Hero','03_游戏库图片/library_hero_3840x1240.png','3840×1240'),
('Library Logo','03_游戏库图片/library_logo_1040x720.png','1040×720，透明 PNG'),
('Shortcut Icon','04_图标/shortcut_icon_256x256.png','256×256 PNG；另附 512 版'),
('App Icon','04_图标/app_icon_184x184.jpg','184×184 JPG')]
with (docs/'图片上传对照表.csv').open('w',encoding='utf-8-sig',newline='') as f:
    w=csv.writer(f);w.writerow(['Steam字段','相对路径','尺寸']);w.writerows(mapping)
write('00_先读我.txt','''挖哦 / WOoOW — Steam 商店页手动填写包
AppID：5506030
制作日期：2026-10-28（香港时间）

先双击 index.html：查看图片、文案和上传对照，复制短简介与长介绍。
未上传、未保存到 Steam 后台；原有“挖哦素材”文件不被覆盖。

推荐填写顺序：
1. 基本信息：01_商店文案/游戏名称与基本信息.txt。
2. Description：选择语言，复制 *_short.txt 和 *_about_plain.txt。先以简中为主，再填英文与繁中。
   *_about_bbcode.txt 仅适合支持 BBCode 的源码/旧编辑模式；普通可视化输入框不要直接粘贴标签代码。
3. Tags：参照 tags_priority.csv，推荐 15 个标签按顺序设置，先保证前五项准确。
4. Graphical Assets：按照 10_填写说明与核验/图片上传对照表.csv 选择 02、03、04 文件夹里的文件。
5. Screenshots：07_实机截图 按前缀顺序上传，至少五张；全部七张均为原图。
6. Trailers：05_视频 先 01 后 02，按相同前缀选对应原帧封面；上传后等平台转码。
7. 关于游戏配图：06_介绍配图 是两张可选真实玩法 GIF，可在可视化编辑器上传插入。
8. 补齐团队真实信息：价格、发行日期、开发/发行商、支持邮箱、实测配置，核对功能/语言与内容调查。
9. 保存草稿，打开 Preview changes in store 检查；确认后再自行发布或提交审核。

已准备：商店四种胶囊图、商店页背景、四种库素材、两种快捷图标及 App 图标、七张实机截图、两段视频及其封面、两张介绍 GIF、简中/英文/繁中短简介与长介绍、标签/类型/功能/语言建议、待确认字段与素材核验表。
08_备用推广图 仅为未来活动备用，不是本次商店页必填槽位。
09_原始LOGO与配置参考 保留原始 LOGO 和项目已有成就定义参考，不应当把参考 CSV 当作商店页导入文件。
配置需求未实测，不把猜测值写成已验证的最低/推荐要求。
''')

payload={'appid':5506030,'draft_only':True,'titles':{'schinese':'挖哦','english':'WOoOW','tchinese':'挖哦'},'short_descriptions':shorts,'about_plain':{lang:'\n\n'.join(t+'\n'+p for t,p in sec) for lang,sec in sections.items()},'tags_recommended':[r[0] for r in tags],'pending_fields':['developer','publisher','support_email','release_date','price','minimum_requirements','recommended_requirements','final_language_validation','steam_feature_validation','content_survey'],'source_urls':[u for _,u in sources]}
(texts/'store-copy.json').write_text(json.dumps(payload,ensure_ascii=False,indent=2),encoding='utf-8')

assetcards=''.join(f'<article><img src="{html.escape(path)}" alt="{html.escape(name)}"><h3>{html.escape(name)}</h3><p>{html.escape(dim)}</p><a href="{html.escape(path)}">打开文件</a></article>' for name,path,dim in mapping if 'logo' not in path.lower())
langnames={'schinese':'简体中文','english':'English','tchinese':'繁體中文'}
textcards=''
for lang in shorts:
    textcards+=f'<section class="copy"><h2>{langnames[lang]}</h2><h3>短简介 <small>{len(shorts[lang])} 字符</small></h3><textarea id="short-{lang}" rows="4">{html.escape(shorts[lang])}</textarea><button onclick="copyText(\'short-{lang}\')">复制短简介</button><h3>关于这款游戏</h3><textarea id="about-{lang}" rows="18">{html.escape(payload["about_plain"][lang])}</textarea><button onclick="copyText(\'about-{lang}\')">复制长介绍</button></section>'
taghtml=''.join(f'<span>{i+1}. {html.escape(en)} · {html.escape(cn)}</span>' for i,(en,cn,_) in enumerate(tags))
shothtml=''.join(f'<a href="07_实机截图/{f.name}"><img src="07_实机截图/{f.name}" alt="{f.stem}"></a>' for f in sorted((root/'07_实机截图').glob('*.png')))
sourcehtml=''.join(f'<li><a href="{u}">{n}</a></li>' for n,u in sources)
doc=f'''<!doctype html><html lang="zh-CN"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>挖哦 · Steam 商店资料包</title>
<style>:root{{color-scheme:dark}}*{{box-sizing:border-box}}body{{margin:0;background:#11171b;color:#e9e7df;font:16px/1.75 "Microsoft YaHei",system-ui,sans-serif}}main{{max-width:1240px;margin:auto;padding:32px 26px 80px}}a{{color:#eebd6b}}h1{{font-size:44px;margin:8px 0}}h2{{font-size:28px;margin-top:48px}}h3{{margin:12px 0 5px}}small,.muted{{color:#a5b1b7}}.hero{{width:100%;border-radius:14px}}.notice{{padding:18px 22px;background:#283038;border-left:4px solid #eebd6b;border-radius:6px}}.grid{{display:grid;grid-template-columns:repeat(auto-fit,minmax(250px,1fr));gap:20px}}article,.copy{{padding:18px;background:#1d252b;border:1px solid #344047;border-radius:10px}}article img{{width:100%;height:190px;object-fit:contain;background:#101417}}article p{{margin:0}}textarea{{width:100%;padding:16px;background:#10171c;color:#e7e8e9;border:1px solid #46525c;border-radius:6px;font:15px/1.8 "Microsoft YaHei",sans-serif;resize:vertical}}button{{background:#eebd6b;color:#25211a;border:0;border-radius:5px;padding:10px 18px;font-weight:bold;cursor:pointer}}.copy{{margin:22px 0}}.tags span{{display:inline-block;border:1px solid #5d5350;padding:5px 12px;margin:4px;border-radius:5px}}.shots img{{width:100%;border-radius:6px}}video{{width:100%;max-height:500px;background:#000;border-radius:8px}}.flow{{padding:20px;background:#1d252b;border-radius:10px}}.pair{{display:grid;grid-template-columns:1fr 1fr;gap:20px}}.pair img{{width:100%}}@media(max-width:700px){{.pair{{grid-template-columns:1fr}}h1{{font-size:32px}}}}</style>
<main><p class="muted">本地资料包 · APP 5506030 · 2026-10-28</p><h1>挖哦 / WOoOW</h1><p>明天可按字段复制、逐项上传。原始 LOGO 保留，实机截图与宣传画面分开。</p><div class="notice">这是内部预览，不是已发布的 Steam 商店页。后台尚未写入。开发者、价格、日期、配置及功能验收由团队确认。</div><p><a href="00_先读我.txt">先读我</a>　<a href="10_填写说明与核验/图片上传对照表.csv">上传对照表</a>　<a href="01_商店文案/团队需确认的信息.txt">待确认信息</a></p><img class="hero" src="02_商店图片/main_capsule_1232x706.png" alt="主胶囊图">
<h2>填写顺序</h2><div class="flow">基本信息 → 三语描述 → 标签 → 商店与库图片 → 七张截图 → 视频与封面 → 补齐真实信息 → 保存草稿并预览</div>
<h2>图片上传位置</h2><div class="grid">{assetcards}</div><h3>透明库 LOGO</h3><p><a href="03_游戏库图片/library_logo_1040x720.png">library_logo_1040x720.png</a>，原始透明 PNG 等比缩放；上传后在 Steam 预览中调整位置。库壁纸中的方块角色位于中心安全区域附近，另附安全框预览供检查。</p>
<h2>三语文案</h2>{textcards}<h2>标签建议</h2><div class="tags">{taghtml}</div><p>前五项优先。卡牌房不等于牌组构筑或肉鸽；不要为凑数量添加弱相关标签。</p>
<h2>实机截图顺序</h2><div class="grid shots">{shothtml}</div><p>这七张是原始 2560×1440 图片的复制件，没有生成或重画玩法。</p>
<h2>视频</h2><h3>1. 实机玩法预告</h3><video controls preload="none" poster="05_视频/01_trailer_poster_1920x1080.png" src="05_视频/01_gameplay_trailer_1920x1080_30fps.mp4"></video><h3>2. 流程实机演示</h3><video controls preload="none" poster="05_视频/02_extended_poster_1920x1080.png" src="05_视频/02_extended_gameplay_1920x1080_30fps.mp4"></video>
<h2>可选的真实玩法 GIF</h2><div class="pair"><img src="06_介绍配图/01_digging.gif" alt="真实挖掘片段"><img src="06_介绍配图/02_office.gif" alt="真实办公室片段"></div><p>把它们上传到 Steam 描述编辑器后插入相应段落；不要粘贴本地路径。</p>
<h2>仍需团队确认</h2><p>开发/发行商名称、支持邮箱、发行日期、价格、实测系统需求、最终构建语言与 Steam 功能、内容调查。详见文本文件；本包不将推测写成已完成的功能。</p>
<h2>官方要求</h2><ul>{sourcehtml}</ul><p class="muted">只有准确的字段和原始玩法，才能让页面与实际游戏一致。制作与核验记录位于 10_填写说明与核验。</p></main>
<script>function copyText(id){{const x=document.getElementById(id);x.focus();x.select();if(navigator.clipboard&&window.isSecureContext){{navigator.clipboard.writeText(x.value).catch(()=>document.execCommand('copy'));}}else{{document.execCommand('copy');}}}}</script></html>'''
(root/'index.html').write_text(doc,encoding='utf-8')
print(json.dumps({'short_lengths':{k:len(v) for k,v in shorts.items()},'text_files':len(list(texts.iterdir())),'asset_slots':len(mapping)},ensure_ascii=True))
