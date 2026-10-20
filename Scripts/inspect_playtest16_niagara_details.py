import unreal,json
from pathlib import Path
s=unreal.load_asset('/Game/Variant_Horror/Blueprints/Light/Assets/NS_DustMote')
r=[]
for o in unreal.ObjectIterator(unreal.Object):
 p=unreal.SystemLibrary.get_path_name(o)
 if p.startswith(s.get_path_name()+':Minimal_1'):
  r.append({'path':p,'class':o.get_class().get_name(),'values':json.loads(unreal.MiningNiagaraEditorLibrary.inspect_object(o))})
(Path(unreal.Paths.project_dir())/'Saved/Validation/Playtest16/niagara_details.json').write_text(json.dumps(r,indent=2),encoding='utf8')
unreal.SystemLibrary.quit_editor()
