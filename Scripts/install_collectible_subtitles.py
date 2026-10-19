"""Create editable pickup subtitles for all twelve stable collectible IDs.

Run in Unreal with PIE stopped. Saves only /Game/Subtitles/Collectibles assets;
no levels are modified. Existing subtitle playback handles queuing and voice.
"""
import json
from pathlib import Path
import unreal

# These IDs are the existing catalogue/inventory IDs, not the order in a table.
PICKUP_LINES = [
    (0, '来自天堂的马桶', '有点恶心 但是是个很有意思的想法'),
    (1, '电脑里的蚂蚁窝', '蚂蚁一直朝着地心挖掘 挖出了复杂的通路'),
    (2, '化学试剂', '化学反应应该很有意思 但是我没怎么学过'),
    (3, '肉鸽', '肉鸽……肉鸽……'),
    (4, '奇怪的计算器', '“1+1>2” 如果是这样的话……'),
    (5, '损坏的磁带', '我记得小时候经常把里面的磁带扯出来'),
    (6, '音符', '几个音符就能组成具有感染力的旋律'),
    (7, '老旧的相片', '一看到这个就让我想起了什么……'),
    (8, '喷发中的可乐', '将曼妥思投入可乐……'),
    (9, '「GPT」之花', 'AI的思考就如同不断向下延伸的根'),
    (10, '同步闪光的萤火虫', '萤火虫的同步闪烁：随机中怎样涌现出秩序？'),
    (11, '散落的彩带', '漫天的彩带 喷出来了'),
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
    segment.set_editor_property('lines', [line])
    assert unreal.EditorAssetLibrary.save_loaded_asset(segment), path
    rows.append(dict(item_id=item_id, name=name, id=str(segment.segment_id), text=text, asset=path, no_audio_file=True))

out = Path(unreal.Paths.project_saved_dir())/'Validation/CollectibleSubtitles'
out.mkdir(parents=True, exist_ok=True)
(out/'installed.json').write_text(json.dumps(dict(passed=True, segments=rows), ensure_ascii=False, indent=2), encoding='utf8')
unreal.log('COLLECTIBLE_SUBTITLES_INSTALLED 12 editable segments, no level packages saved')
