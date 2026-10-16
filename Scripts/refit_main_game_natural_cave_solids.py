"""Rebuild only the six new collars, keeping a positive roof thickness at doors."""
import unreal,json,sys
from pathlib import Path
ROOT=Path(unreal.Paths.project_dir()).resolve();sys.path.insert(0,str(ROOT/'Scripts'));import main_game_room_entrances_geometry as geometry
import importlib;importlib.reload(geometry)
OUT=ROOT/'Saved/Validation/MainGameRoomEntrances';cfg=json.loads((ROOT/'Config/MainGameRooms.json').read_text());ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem);assert not ed.get_game_world();actors=list(unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors())
assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).set_current_level_by_name('Lvl_MainGame')
hall=next(a for a in actors if isinstance(a,unreal.MiningExhibitHall));hall.set_editor_property('tags',list(hall.tags)+(['BM_MainGame_NaturalRoomConnections'] if not hall.actor_has_tag('BM_MainGame_NaturalRoomConnections') else []))
def V(p):return unreal.Vector(*p)
for s in cfg['generated_connections']:
    prefix='LINK_'+s['id'];rock=next(a for a in actors if a.get_actor_label()==prefix+'_NaturalRock');path=OUT/(prefix+'_NaturalRock.obj');geometry.write_obj(path,*geometry.closed_tube(s));assert unreal.MiningDesignerLibrary.import_campaign_rock(rock,str(path));rock.reset_rock()
    shell=next(a for a in actors if a.get_actor_label()==prefix+'_BedrockShell');component=shell.get_component_by_class(unreal.DynamicMeshComponent);mesh=component.get_dynamic_mesh();mesh.reset();vertices,triangles=geometry.assert_closed(*geometry.closed_tube(s,True));o=shell.get_actor_location()
    unreal.GeometryScript_MeshEdits.append_buffers_to_mesh(mesh,unreal.GeometryScriptSimpleMeshBuffers(vertices=[V(p)-o for p in vertices],triangles=[unreal.IntVector(*t) for t in triangles]));unreal.GeometryScript_Normals.set_per_face_normals(mesh);assert unreal.GeometryScript_MeshQueries.get_is_closed_mesh(mesh);component.update_collision(False)
assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level();unreal.log('ROOM_NATURAL_SOLIDS_REFIT_SAVED')
