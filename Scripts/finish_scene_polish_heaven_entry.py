"""Keep the imported retaining soil inside the campaign entrance, outside the enlarged sky."""
import unreal,json,traceback
from pathlib import Path
L=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);A=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
report={'passed':False,'soil':[]}
try:
 assert L.load_level('/Game/MainGame/Maps/Lvl_MainGame')
 water=next(a for a in A.get_all_level_actors() if isinstance(a,unreal.HeavenToiletWater));frame=water.spill_coordinate_frame
 for a in water.retaining_soil:
  p=unreal.MathLibrary.inverse_transform_location(frame,a.get_actor_location());p.x=300
  a.set_actor_location(unreal.MathLibrary.transform_location(frame,p),False,True)
  size=a.rock_size;size.y=660;a.rock_size=size;a.reset_rock()
  report['soil'].append({'label':a.get_actor_label(),'local':list(p.to_tuple()),'size':list(size.to_tuple())})
 water.rebuild_water();assert L.save_current_level();report['passed']=True
except Exception:report['error']=traceback.format_exc()
(Path(unreal.Paths.project_dir())/'Saved/Validation/ScenePolish/heaven_entry.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
L.load_level('/Engine/Maps/Entry');unreal.SystemLibrary.quit_editor()
