"""Update only the requested chemistry subtitle asset, preserving its timing/audio."""
import unreal,json
from pathlib import Path
path='/Game/Subtitles/Collectibles/DA_CollectiblePickup_02'
asset=unreal.load_asset(path)
assert isinstance(asset,unreal.SubtitleSegment)
lines=list(asset.lines)
assert len(lines)==1
lines[0].set_editor_property('text','化学元素通过反应产生新的物质 进而形成大千世界')
asset.set_editor_property('lines',lines)
assert unreal.EditorAssetLibrary.save_loaded_asset(asset)
out=Path(unreal.Paths.project_dir())/'Saved/Validation/NarrativeOct27'
out.mkdir(parents=True,exist_ok=True)
(out/'chemistry.json').write_text(json.dumps({'passed':True,'asset':path,'text':str(asset.lines[0].text)},ensure_ascii=False,indent=2),encoding='utf8')
unreal.SystemLibrary.quit_editor()
