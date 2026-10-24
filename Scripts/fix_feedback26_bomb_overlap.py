import unreal,json
from pathlib import Path
L=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);A=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
assert L.load_level('/Game/MainGame/Maps/Lvl_MainGame')
r={'removed':[],'adjusted':[]}
for a in list(A.get_all_level_actors()):
 if not a.actor_has_tag('BM_Polish32_BombSign') or not a.actor_has_tag('BM_Campaign_MainGame03'):continue
 if a.get_actor_label().endswith('_Back'):
  r['removed'].append(a.get_actor_label());A.destroy_actor(a);continue
 if not a.actor_has_tag('BM_SingleBombProjection'):
  a.set_actor_location(a.get_actor_location()+a.get_actor_forward_vector()*75,False,False)
  a.set_editor_property('tags',list(a.tags)+['BM_SingleBombProjection'])
 size=a.decal.decal_size;a.decal.set_editor_property('decal_size',unreal.Vector(110,size.y,size.z));r['adjusted'].append(a.get_actor_label())
assert len(r['adjusted'])==3
assert L.save_current_level()
r['passed']=True
Path(unreal.Paths.project_dir(),'Saved/Validation/PlayerFeedback26/bomb-overlap.json').write_text(json.dumps(r),encoding='utf8')
unreal.SystemLibrary.quit_editor()
