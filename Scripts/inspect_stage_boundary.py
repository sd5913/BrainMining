import unreal,json
from pathlib import Path
L=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);assert L.load_level('/Game/MainGame/Maps/Lvl_MainGame')
aa=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors();r={'regions':[],'seam':[],'escape':[]}
for a in aa:
 if isinstance(a,unreal.MiningExternalRegion):
  r['regions'].append({'name':a.get_actor_label(),'p':a.get_actor_location().to_tuple(),'size':a.rock_size.to_tuple(),'shell':str(a.generated_shell),'material':str(a.shell_material)})
 if isinstance(a,unreal.MiningRoomEscape):
  r['escape'].append({'name':a.get_actor_label(),'position':a.get_actor_location().to_tuple()})
 name=a.get_actor_label();p=a.get_actor_location()
 if ('LINK_' in name or 'Collar' in name or 'MINE_' in name) and 32000<p.x<34500:
  c,e=a.get_actor_bounds(False);r['seam'].append({'name':name,'class':a.get_class().get_name(),'p':p.to_tuple(),'center':c.to_tuple(),'extent':e.to_tuple(),'tags':[str(t) for t in a.tags]})
out=Path(unreal.Paths.project_dir())/'Saved/Validation/StageBoundary';out.mkdir(parents=True,exist_ok=True);(out/'layout.json').write_text(json.dumps(r,ensure_ascii=False,indent=2),encoding='utf8');unreal.SystemLibrary.quit_editor()
