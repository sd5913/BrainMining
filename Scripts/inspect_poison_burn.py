import unreal,json
from pathlib import Path
s=unreal.load_asset('/Game/Passages/Echo/ArtP2/VFX/NS_P2_Poison')
h=unreal.MiningNiagaraEditorLibrary
r={'system':h.inspect_object(s),'renderers':[]}
for o in unreal.ObjectIterator(unreal.NiagaraSpriteRendererProperties):
 p=unreal.SystemLibrary.get_path_name(o)
 if 'NS_P2_Poison' in p:
  r['renderers'].append({'path':p,'properties':h.inspect_object(o)})
out=Path(unreal.Paths.project_dir())/'Saved/Validation/Playtest17/poison-inspect.json'
out.write_text(json.dumps(r,indent=2),encoding='utf8')
unreal.SystemLibrary.quit_editor()
