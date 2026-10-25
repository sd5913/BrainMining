import unreal,json
from pathlib import Path
unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level('/Game/MainGame/Maps/Lvl_MainGame')
a=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors();r=[]
for x in a:
 if isinstance(x,(unreal.PostProcessVolume,unreal.HeavenToiletWater)):
  d={'name':x.get_name(),'label':x.get_actor_label(),'pos':list(x.get_actor_location().to_tuple()),'bounds':[list(v.to_tuple()) for v in x.get_actor_bounds(False)]}
  if isinstance(x,unreal.PostProcessVolume):d.update(unbound=x.unbound,priority=x.priority,min=x.settings.auto_exposure_min_brightness,max=x.settings.auto_exposure_max_brightness,bias=x.settings.auto_exposure_bias)
  else:d.update(radius=x.radius)
  r.append(d)
Path(unreal.Paths.project_dir(),'Saved/Validation/Feedback7/volumes.json').write_text(json.dumps(r,ensure_ascii=False,indent=2),encoding='utf8')
unreal.SystemLibrary.quit_editor()
