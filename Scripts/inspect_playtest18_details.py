"""Read saved assets used by the October 21 packaged feedback, without saving."""
import json, traceback
from pathlib import Path
import unreal

root=Path(unreal.Paths.project_dir())
out=root/'Saved/Validation/Playtest18';out.mkdir(parents=True,exist_ok=True)
r={}
try:
    r['subtitles']=[]
    for path in unreal.EditorAssetLibrary.list_assets('/Game/Subtitles',recursive=True,include_folder=False):
        a=unreal.load_asset(path)
        if isinstance(a,unreal.SubtitleSegment):
            r['subtitles'].append(dict(path=path,id=str(a.segment_id),lines=[str(x.text) for x in a.lines]))
    r['pickaxes']=[]
    for path in unreal.EditorAssetLibrary.list_assets('/Game/Pickaxe',recursive=True,include_folder=False):
        a=unreal.load_asset(path)
        if isinstance(a,unreal.StaticMesh):
            r['pickaxes'].append(dict(path=path,bounds=str(a.get_bounds()),materials=[str(x.material_interface) for x in a.static_materials]))
        if isinstance(a,unreal.Material):
            r[path]=[]
            for n in unreal.MaterialEditingLibrary.get_material_expressions(a):
                item=dict(type=n.get_class().get_name())
                for p in ['texture','constant','r','parameter_name','default_value']:
                    try:item[p]=str(n.get_editor_property(p))
                    except Exception:pass
                r[path].append(item)
    r['passed']=True
except Exception:
    r['error']=traceback.format_exc();unreal.log_error(r['error'])
(out/'details.json').write_text(json.dumps(r,ensure_ascii=False,indent=2),encoding='utf8')
unreal.SystemLibrary.quit_editor()
