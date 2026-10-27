"""Create editable pickup subtitles for all twelve stable collectible IDs.

Run in Unreal with PIE stopped. Saves only /Game/Subtitles/Collectibles assets;
no levels are modified. Existing subtitle playback handles queuing and voice.
"""
import json
from pathlib import Path
import unreal

# These IDs are the existing catalogue/inventory IDs, not the order in a table.
PICKUP_LINES = [
    (0, '来自天堂的马桶', '马桶堵住了然后上涌……有点味道的想法'),
    (1, '电脑里的蚂蚁窝', '蚂蚁一直挖掘 涌现出了复杂的通路'),
    (2, '化学试剂', '化学元素通过反应产生新的物质 进而形成大千世界'),
    (3, '肉鸽', '肉鸽游戏很适合做连携 算是"1+1>2"的涌现吧'),
    (4, '奇怪的计算器', '“1+1>2” 从这个点出发想想“涌现”吧'),
    (5, '损坏的磁带', '把磁带扯出来……这算一种涌现吗'),
    (6, '音符', '简单的几个音符能涌现出复杂的旋律'),
    (7, '老旧的相片', '回忆涌上心头……这也是一种涌现'),
    (8, '喷发中的可乐', '将曼妥思投入可乐的话 可乐就会“涌现”出来'),
    (9, '「GPT」之花', 'AI的思考不断涌现 就如同不断向下延伸的根'),
    (10, '罐子里的萤火虫', '萤火虫的同步闪烁：随机中怎样涌现出秩序？'),
    (11, '散落的彩带', '大量的彩带喷涌而出 也很涌现'),
]

assert not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world(), 'Stop PIE first'
folder = '/Game/Subtitles/Collectibles'
tools = unreal.AssetToolsHelpers.get_asset_tools()
rows = []
for item_id, name, text in PICKUP_LINES:
    asset_name = f'DA_CollectiblePickup_{item_id:02d}'
    path = folder+'/'+asset_name
    segment = unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else None
    if segment is None:
        factory = unreal.DataAssetFactory()
        factory.set_editor_property('data_asset_class', unreal.SubtitleSegment)
        segment = tools.create_asset(asset_name, folder, unreal.SubtitleSegment, factory)
    assert isinstance(segment, unreal.SubtitleSegment), path
    segment.set_editor_property('segment_id', f'收集物拾取_{item_id:02d}')
    line = unreal.SubtitleLine()
    line.set_editor_property('text', text)
    line.set_editor_property('audio', None)
    lines = [line]
    if item_id == 11:
        followup = unreal.SubtitleLine()
        followup.set_editor_property('text', '继续往前走走')
        lines.append(followup)
    segment.set_editor_property('lines', lines)
    assert unreal.EditorAssetLibrary.save_loaded_asset(segment), path
    rows.append(dict(item_id=item_id, name=name, id=str(segment.segment_id), text=text, asset=path, no_audio_file=True))

out = Path(unreal.Paths.project_saved_dir())/'Validation/CollectibleSubtitles'
out.mkdir(parents=True, exist_ok=True)
(out/'installed.json').write_text(json.dumps(dict(passed=True, segments=rows), ensure_ascii=False, indent=2), encoding='utf8')
unreal.log('COLLECTIBLE_SUBTITLES_INSTALLED 12 editable segments, no level packages saved')
