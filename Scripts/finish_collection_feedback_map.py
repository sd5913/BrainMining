"""Finish the extended faucet retaining bank on an already reconciled map."""
import unreal,json,traceback
from pathlib import Path
root=Path(unreal.Paths.project_dir());out=root/'Saved/Validation/CollectionFeedback/map_finish.json'
report={'passed':False}
try:
 levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);assert levels.load_level('/Game/MainGame/Maps/Lvl_MainGame')
 ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem);world=ed.get_editor_world()
 water=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.HeavenToiletWater)[0]
 water.set_editor_property('outer_x',-300.)
 red=unreal.load_asset('/Game/MainGame/Materials/RoutesV5/M_DarkRed')
 for rock in water.retaining_soil:
  rock.set_editor_property('rock_material',red,notify_mode=unreal.PropertyAccessChangeNotifyMode.NEVER)
  for mesh in rock.get_components_by_class(unreal.DynamicMeshComponent):mesh.set_material(0,red)
 assert unreal.EditorLoadingAndSavingUtils.save_map(world,'/Game/MainGame/Maps/Lvl_MainGame')
 report['passed']=True
except Exception:report['error']=traceback.format_exc()
out.write_text(json.dumps(report,indent=2),encoding='utf-8');unreal.SystemLibrary.quit_editor()
