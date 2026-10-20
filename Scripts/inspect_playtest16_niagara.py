import unreal,json
from pathlib import Path
out=Path(unreal.Paths.project_dir())/'Saved/Validation/Playtest16'
H=unreal.MiningNiagaraEditorLibrary
system=unreal.load_asset('/Game/Variant_Horror/Blueprints/Light/Assets/NS_DustMote')
summary=json.loads(H.inspect_stack(system));report={'summary':summary,'emitters':[]}
for e in summary['emitters']:
 name=e['emitterName'];top=json.loads(H.inspect_stack(system,name));values={}
 for k in ['emitterUpdateScript','particleSpawnScript','particleUpdateScript']:
  for m in top.get(k,{}).get('modules',[]):
   values[k+':'+m['moduleName']]=json.loads(H.inspect_stack(system,name,top[k]['scriptName'],m['moduleName']))
 report['emitters'].append(dict(topology=top,values=values))
(out/'niagara_source.json').write_text(json.dumps(report,indent=2),encoding='utf8')
L=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);A=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
assert L.load_level('/Game/PopupClose/Maps/Lvl_PopupClose1')
cap=next(a for a in A.get_all_level_actors() if a.get_actor_label()=='POLISH_Office_PartitionFront')
cap.set_actor_location(unreal.Vector(-712.825475,-418.4408,184.93),False,True)
cap.set_actor_scale3d(unreal.Vector(.30,.10,3.9386));assert L.save_current_level()
unreal.SystemLibrary.quit_editor()
