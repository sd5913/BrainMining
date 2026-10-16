"""Cut and restore each new cave wall in PIE without touching authored assets."""
import unreal,json,time,traceback,sys
from pathlib import Path
ROOT=Path(unreal.Paths.project_dir()).resolve();sys.path.insert(0,str(ROOT/'Scripts'));import main_game_room_entrances_geometry as geometry
cfg=json.loads((ROOT/'Config/MainGameRooms.json').read_text());OUT=ROOT/'Saved/Validation/MainGameRoomEntrances';ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem);levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);report={'passed':False,'walls':[]};state={'start':time.monotonic(),'busy':False}
def finish(error=None):
 report.update(passed=error is None,error=error);(OUT/'rock_mining.json').write_text(json.dumps(report,indent=2));unreal.unregister_slate_post_tick_callback(callback);levels.editor_request_end_play()
def tick(dt):
 if state['busy']:return
 state['busy']=True
 try:
  assert time.monotonic()-state['start']<80,'PIE startup timeout';world=ed.get_game_world()
  if not world:return
  halls=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.MiningExhibitHall)
  if not halls or not halls[0].get_editor_property('bReady'):return
  rocks=list(unreal.GameplayStatics.get_all_actors_of_class(world,unreal.GeneratedMiningRock))
  for i,s in enumerate(cfg['generated_connections']):
   rock=next(a for a in rocks if a.get_actor_label()=='LINK_'+s['id']+'_NaturalRock');c=rock.get_component_by_class(unreal.DynamicMeshComponent);before=rock.triangle_count;x=(s['a'][0]+s['b'][0])/2;p=unreal.Vector(x,s['a'][1],geometry.floor_at(s,x)+135);end=p+unreal.Vector(0,1000,0);hit=unreal.MiningDesignerLibrary.find_campaign_wall(rock,p,end)
   assert hit.distance(end)>1,(s['id'],'No inward-facing wall collision');volume=unreal.GeometryScript_MeshQueries.get_mesh_volume_area(c.get_dynamic_mesh())[1];assert volume>0,(s['id'],'Inverted terrain solid')
   assert rock.mine_blast(hit,105,.05,51000+i),(s['id'],'Wall does not accept normal excavation');assert rock.successful_cuts>0 and rock.triangle_count!=before,(s['id'],'Cut did not change geometry')
   rock.reset_rock();assert rock.successful_cuts==0 and rock.triangle_count==before,(s['id'],'Reset did not restore wall');report['walls'].append({'id':s['id'],'triangles':before,'positive_volume':volume,'wall_cut_and_reset':True})
  finish()
 except Exception:finish(traceback.format_exc())
 finally:state['busy']=False
assert not ed.get_game_world();assert levels.load_level('/Game/MainGame/Maps/Lvl_MainGame');unreal.EditorPythonScripting.set_keep_python_script_alive(True);callback=unreal.register_slate_post_tick_callback(tick);unreal.MiningGUIEditorLibrary.start_floating_preview()
