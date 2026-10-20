"""Unsaved camera inside the original demonstration's unmodified vein mesh."""
import unreal,json,time,traceback
from pathlib import Path
OUT=Path(unreal.Paths.project_dir())/'Saved/Validation/Feedback11/Blood'
H=unreal.MiningGUIEditorLibrary;E=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem);L=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
s={'next':0,'busy':False,'start':time.monotonic()};r={'passed':False}
def run():
 assert L.load_level('/Game/BloodCirculation/Maps/Demonstration3')
 A=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
 actors=A.get_all_level_actors()
 c=next(c for a in actors for c in a.get_components_by_class(unreal.StaticMeshComponent) if c.static_mesh and c.static_mesh.get_name()=='SM_Vein')
 dm=unreal.DynamicMesh();unreal.GeometryScript_AssetUtils.copy_mesh_from_static_mesh(c.static_mesh,dm,unreal.GeometryScriptCopyMeshFromAssetOptions(),unreal.GeometryScriptMeshReadLOD())
 _,v,_=dm.get_all_vertex_positions(False);v=unreal.GeometryScript_List.convert_vector_list_to_array(v)
 _,uv,*_=getattr(dm,next(a for a in dir(dm) if a.replace('_','').lower()=='getmeshpervertexuvs'))(0);uv=unreal.GeometryScript_List.convert_uv_list_to_array(uv)
 rings={}
 for i,p in enumerate(v):
  if p.y<-5000 and 16.32415<=uv[i].x<=17.0827:rings.setdefault(round(uv[i].x,4),[]).append(p)
 centers=[sum(rings[k],unreal.Vector())/len(rings[k]) for k in sorted(rings)]
 tr=c.get_world_transform();eye=tr.transform_location(centers[1]);target=tr.transform_location(centers[-1])
 cam=A.spawn_actor_from_class(unreal.CameraActor,eye,unreal.MathLibrary.find_look_at_rotation(eye,target))
 cam.set_actor_label('SourceComparisonCamera');cam.get_component_by_class(unreal.CameraComponent).set_field_of_view(80)
 H.start_floating_preview()
 while not E.get_game_world():yield .2
 yield 8
 w=E.get_game_world();pc=unreal.GameplayStatics.get_player_controller(w,0)
 H.resize_preview_viewport(pc,1280,720)
 actors=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.Actor)
 cam=next(a for a in actors if a.get_actor_label()=='SourceComparisonCamera');pc.set_view_target_with_blend(cam,0)
 if pc.get_controlled_pawn():pc.get_controlled_pawn().set_actor_hidden_in_game(True)
 yield 2
 assert H.capture_preview(pc,str(OUT/'source_demo_original.png'))
 r['eye']=str(eye);r['target']=str(target)
 r['lights']=[{'actor':a.get_actor_label(),'component':light.get_name(),'intensity':light.intensity} for a in actors for light in a.get_components_by_class(unreal.LightComponent)]
 r['other_components']=[{'actor':a.get_actor_label(),'type':c.get_class().get_name(),'component':c.get_name()} for a in actors for c in a.get_components_by_class(unreal.SceneComponent) if isinstance(c,(unreal.SkeletalMeshComponent,unreal.ParticleSystemComponent))]
 r['passed']=True;finish()
def finish(error=None):
 if error:r['error']=error
 (OUT/'source_capture.json').write_text(json.dumps(r,ensure_ascii=False,indent=2),encoding='utf8')
 unreal.unregister_slate_post_tick_callback(handle);L.editor_request_end_play();unreal.SystemLibrary.quit_editor()
def tick(dt):
 if s['busy'] or time.monotonic()<s['next']:return
 s['busy']=True
 try:
  assert time.monotonic()-s['start']<120,'Timeout'
  s['next']=time.monotonic()+next(gen)
 except StopIteration:pass
 except Exception:finish(traceback.format_exc())
 finally:s['busy']=False
gen=run();handle=unreal.register_slate_post_tick_callback(tick);unreal.EditorPythonScripting.set_keep_python_script_alive(True)
