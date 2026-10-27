"""Install the authored subtitle plan through the live Unreal Editor (PIE stopped)."""
import json
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.project_dir())
MAIN = '/Game/MainGame/Maps/Lvl_MainGame'
SUB = MAIN + '_SubtitleTriggers'
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
assert not editor.get_game_world(), 'Stop PIE first'
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages(), 'Save maps first'
assert levels.load_level(MAIN)
world = editor.get_editor_world()
stream = unreal.GameplayStatics.get_streaming_level(world, SUB)
assert isinstance(stream, unreal.LevelStreamingAlwaysLoaded)
unreal.EditorLevelUtils.make_level_current(stream)
sublevel = stream.get_loaded_level()

PLAN = [
    ('StartRoom', 'StartRoom', ['12点了，是时候了']),
    ('LeaveStartRoom', '离开初始房', ['到处看看 也许就知道怎么做了']),
    ('OfficeGameplay1', '电脑弹窗玩法1', ['工作是一个很能引发共鸣的话题', '电脑屏幕弹出越来越多的弹窗 办公室里的打印机开始喷出越来越多的纸', '不断关闭这些弹窗', '不断处理那些文件']),
    ('OfficeGameplay2', '电脑弹窗玩法2', ['但是最后，完全干不完啊！', '他们飞得到处都是']),
    ('OfficeGameplay3', '电脑弹窗玩法3', ['有些枯燥无聊啊']),
    ('OfficeGameplay4', '电脑弹窗玩法4', ['别管那个了吧']),
    ('EchoMaze', '盲人敲地玩法', ['盲人可以靠声音来让这个房间显现出来', '敲击黑暗的环境 周围的轮廓会暂时显现', '感觉在哪里见过类似的东西呢']),
    ('Cards1', '卡牌玩法1', ['卡牌之间的连携可以打出强大的效果', '比如元素之间的反应']),
    ('Cards2', '卡牌玩法2', ['这种卡牌玩法会不会太常见了呢？']),
    ('RisingWater', '水涌玩法', ['水？上涌的水…不断地跳']),
    ('Museum1', '文思玩法1', ['当你看到一个东西 一句话就会浮现在你的脑海里', '如果有两个这样的东西 就可以交换他们的描述']),
    ('Museum2', '文思玩法2', ['会不会太难了', '也许需要设计一个方法跳过这一部分', '比如同时按住"SKIP"四个键三秒']),
    ('Museum3', '文思玩法3', ['真的不是很喜欢解谜啊 算了吧']),
    ('PickaxeUpgrade1', '镐子升级1', ['矿镐升级可以让挖掘更轻松']),
    ('PickaxeUpgrade2', '镐子升级2', ['有点感觉']),
    ('PickaxeUpgrade3', '镐子升级3', ['这样似乎可以']),
    ('PickaxeUpgrade4', '镐子升级4', ['我快要想到了']),
]
segments = []
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
for name, segment_id, texts in PLAN:
    path = '/Game/Subtitles/DA_' + name
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        segment = unreal.load_asset(path)
    else:
        factory = unreal.DataAssetFactory()
        factory.set_editor_property('data_asset_class', unreal.SubtitleSegment)
        segment = asset_tools.create_asset('DA_' + name, '/Game/Subtitles', unreal.SubtitleSegment, factory)
    assert segment
    segment.set_editor_property('segment_id', segment_id)
    lines = []
    for text in texts:
        line = unreal.SubtitleLine()
        line.set_editor_property('text', text)
        line.set_editor_property('audio', None)
        lines.append(line)
    segment.set_editor_property('lines', lines)
    assert unreal.EditorAssetLibrary.save_loaded_asset(segment)
    segments.append(segment)

all_actors = actors.get_all_level_actors()
junction = next(a for a in all_actors if a.get_actor_label() == 'STARTROOM_Junction_DiggableBlockage')
cards = next(a for a in all_actors if isinstance(a, unreal.CardRoomStage))
museum = next(a for a in all_actors if isinstance(a, unreal.WenSiMirrorPuzzle))
museum_origin = museum.get_actor_location()

def place(segment_id, location, extent):
    found = [a for a in actors.get_all_level_actors() if isinstance(a, unreal.SubtitleTrigger)
             and a.get_level() == sublevel and str(a.get_editor_property('segment_id')) == segment_id]
    assert len(found) <= 1
    actor = found[0] if found else actors.spawn_actor_from_class(unreal.SubtitleTrigger, location)
    actor.set_actor_location(location, False, False)
    actor.set_actor_label('字幕触发_' + segment_id)
    actor.set_folder_path('Subtitles/PositionTriggers')
    actor.set_editor_property('segment_id', segment_id)
    actor.set_editor_property('tags', ['BM_SubtitlePlan'])
    actor.get_component_by_class(unreal.BoxComponent).set_box_extent(unreal.Vector(*extent))
    assert actor.get_level() == sublevel
    return actor

# Region one's entry is overridden by the bedroom spawn. Anchor to the actual
# soil junction instead, on its mining-side landing beyond the tunnel mouth.
leave = place('离开初始房', junction.get_actor_location() + unreal.Vector(400, 0, -140), (110, 340, 280))
# The authored card approach is 75 cm left of the tabletop stage, 115 cm above it.
card_intro = place('卡牌玩法1', cards.get_actor_transform().transform_location(unreal.Vector(-75, 0, 115)), (850, 900, 500))
# Cover the complete ceiling opening, below the activation plane so the falling player enters it.
museum_entry = place('文思玩法1', museum_origin + unreal.Vector(-360, 0, 160), (250, 250, 180))
# The endpoint is the enclosed mouse-painting alcove, behind the shrink-only aperture.
museum_end = place('文思玩法3', museum_origin + unreal.Vector(2320, -350, 100), (110, 110, 100))

directors = [a for a in actors.get_all_level_actors() if isinstance(a, unreal.MainGameSubtitleDirector)]
assert len(directors) <= 1
director = directors[0] if directors else actors.spawn_actor_from_class(unreal.MainGameSubtitleDirector, unreal.Vector())
assert director.get_level() == sublevel
director.set_actor_label('字幕事件_MainGame')
director.set_folder_path('Subtitles/Events')
# Campaign visibility management must never suspend this cross-room coordinator.
director.set_editor_property('tags', ['BM_SubtitlePlan', 'BM_PlayerEffect'])
director.set_editor_property('segments', segments)
director.set_editor_property('museum_entry', museum_entry)
director.set_editor_property('museum_end', museum_end)
director.set_editor_property('museum_bounds', unreal.Box(museum_origin + unreal.Vector(-1200, -900, -100), museum_origin + unreal.Vector(2800, 900, 800)))
director.set_editor_property('museum_hint_seconds', 300.0)

removed = []
dirty_source_levels = []
for actor in actors.get_all_level_actors():
    if isinstance(actor, unreal.SubtitleTrigger) and str(actor.get_editor_property('segment_id')) == '0001':
        removed.append(actor.get_path_name())
        level = actor.get_level()
        if level not in dirty_source_levels:
            dirty_source_levels.append(level)
        assert actors.destroy_actor(actor)
for level in dirty_source_levels:
    package = level.get_outer()
    path = package.get_path_name().split('.')[0]
    assert unreal.EditorLoadingAndSavingUtils.save_map(package, path)
if unreal.EditorAssetLibrary.does_asset_exist('/Game/Subtitles/DA_0001'):
    assert unreal.EditorAssetLibrary.delete_asset('/Game/Subtitles/DA_0001')
assert unreal.EditorLoadingAndSavingUtils.save_map(sublevel.get_outer(), SUB)

triggers = [a for a in actors.get_all_level_actors() if isinstance(a, unreal.SubtitleTrigger) and a.get_level() == sublevel]
assert len(triggers) == 5
for segment in segments:
    assert all(line.audio is None for line in segment.lines)
report = dict(passed=True, segments=len(segments), lines=sum(len(s.lines) for s in segments),
    no_audio=True, removed=removed, director=director.get_path_name(),
    triggers=[dict(id=str(a.get_editor_property('segment_id')), location=list(a.get_actor_location().to_tuple()),
                   extent=list(a.get_component_by_class(unreal.BoxComponent).get_unscaled_box_extent().to_tuple()),
                   path=a.get_path_name()) for a in triggers])
out = ROOT / 'Saved/Validation/SubtitleTriggers/plan_installed.json'
out.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
unreal.log('MAIN_GAME_SUBTITLE_PLAN ' + json.dumps(report, ensure_ascii=False))
