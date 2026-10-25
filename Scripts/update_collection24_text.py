"""Update editable pickup subtitles only; inventory IDs remain unchanged."""
import json,unreal
from pathlib import Path
lines={12:'收集到了25个铜矿！',13:'收集到了15个铁矿！',14:'收集到了10个孔雀石！',15:'收集到了5个钻石！',17:'小小鱼缸里也有复杂生态。',18:'钱生钱~财富涌现！',19:'几种滋味碰在一起，口感也有了新的层次。',20:'一生二，二生三，三生万物……',22:'一句话很轻，无数句话涌在一起，却能改变世界，也能压垮一个人。'}
report=[]
for Id,text in lines.items():
 name=f'DA_CollectiblePickup_{Id:02d}';folder='/Game/Subtitles/Collectibles'
 asset=unreal.load_asset(folder+'/'+name) if unreal.EditorAssetLibrary.does_asset_exist(folder+'/'+name) else None
 if not asset:
  factory=unreal.DataAssetFactory();factory.set_editor_property('data_asset_class',unreal.SubtitleSegment)
  asset=unreal.AssetToolsHelpers.get_asset_tools().create_asset(name,folder,unreal.SubtitleSegment,factory)
 asset.set_editor_property('segment_id',f'收集物拾取_{Id:02d}')
 line=unreal.SubtitleLine();line.set_editor_property('text',text);asset.set_editor_property('lines',[line])
 assert unreal.EditorAssetLibrary.save_loaded_asset(asset)
 report.append(dict(id=Id,asset=asset.get_path_name(),lines=[str(x.text) for x in asset.lines]))
out=Path(unreal.Paths.project_saved_dir())/'Validation/Collection24';out.mkdir(parents=True,exist_ok=True)
(out/'subtitles.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
unreal.SystemLibrary.quit_editor()
