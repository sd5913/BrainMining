"""Align the exposed drip with the retained campaign elbow, and join the new pipe run."""
import unreal,json,traceback
from pathlib import Path
ROOT=Path(unreal.Paths.project_dir());report={'passed':False}
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);api=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
try:
 assert levels.load_level('/Game/MainGame/Maps/Lvl_MainGame')
 aa=list(api.get_all_level_actors());water=next(a for a in aa if isinstance(a,unreal.HeavenToiletWater));frame=water.spill_coordinate_frame
 world=lambda p:unreal.MathLibrary.transform_location(frame,unreal.Vector(*p))
 shift=1100 if water.actor_has_tag('BM_HeavenRoomClearance1100') else 0
 drip=next(a for a in aa if a.get_actor_label()=='COL_Heaven_ExposedClueDrip');drip.set_actor_location(world((-300-shift,125,184)),False,True)
 label='COL_Heaven_BuriedPipeConnection'
 a=next((a for a in aa if a.get_actor_label()==label),None) or api.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector())
 a.set_actor_label(label);a.tags=['BM_MainGame_Collections','BM_Campaign_MainGame03','BM_ScenePolishHeaven'];a.set_folder_path('MainGame/Collections/Heaven20261020')
 mesh=unreal.load_asset('/Game/HeavenPreview/Meshes/Pipes/SM_HeavenPipePart_009');bounds=mesh.get_bounding_box()
 start=world((-269-shift,125,220));end=world((181,29,220));a.set_actor_location((start+end)/2,False,True);a.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(start,end),False)
 a.set_actor_scale3d(unreal.Vector(start.distance(end)/(bounds.max.x-bounds.min.x),.181716,.181716));a.static_mesh_component.set_static_mesh(mesh)
 a.static_mesh_component.set_material(0,unreal.load_asset('/Game/CollectionPrototype/HeavenToilet/Materials/M_HeavenPipe'));a.static_mesh_component.set_collision_profile_name('NoCollision');a.set_actor_enable_collision(False)
 assert levels.save_current_level();report.update(passed=True,drip=list(drip.get_actor_location().to_tuple()))
except Exception:report['error']=traceback.format_exc()
(ROOT/'Saved/Validation/ScenePolish/clue_finish.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
levels.load_level('/Engine/Maps/Entry');unreal.SystemLibrary.quit_editor()
