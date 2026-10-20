"""Remove only the persistent card completion wall, then verify the saved exit in PIE."""
import unreal,json,sys,shutil,datetime,time,traceback
from pathlib import Path
root=Path(unreal.Paths.project_dir()).resolve();sys.path.insert(0,str(root/'Scripts'))
import campaign_minerals_api as minerals
OUT=root/'Saved/Validation/CardExitGate';OUT.mkdir(parents=True,exist_ok=True)
L=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);A=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
E=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem);H=unreal.MiningGUIEditorLibrary
r={'passed':False,'checks':[]};s={'start':time.monotonic(),'next':0,'busy':False}
def check(ok,name):assert ok,name;r['checks'].append(name)
def run():
 check(L.load_level('/Game/MainGame/Maps/Lvl_MainGame'),'Load latest MainGame map')
 actors=A.get_all_level_actors();hall=next(a for a in actors if isinstance(a,unreal.MiningExhibitHall))
 areas=list(hall.areas);index=next(i for i,a in enumerate(areas) if str(a.tag)=='BM_Campaign_MainGameJourney')
 gate=areas[index].progress_gate
 check(gate is not None and gate.get_actor_label()=='LINK_Journey_CompletionGate','Identify the card-room completion wall')
 check(gate.get_level()==hall.get_level(),'Wall belongs to the persistent map')
 position=gate.get_actor_location();r['removed_actor']=gate.get_path_name();r['position']=position.to_tuple()
 before={a.get_path_name() for a in actors};removed=gate.get_path_name()
 mineral_layout=minerals._layout(minerals.get_field(hall))
 other_gates=[a.progress_gate.get_path_name() if a.progress_gate else None for a in areas]
 backup=root/'Saved/Backups'/('CardExitGate-'+datetime.datetime.now().strftime('%Y%m%d-%H%M%S'));backup.mkdir(parents=True)
 shutil.copy2(root/'Content/MainGame/Maps/Lvl_MainGame.umap',backup/'Lvl_MainGame.umap')
 areas[index].set_editor_property('progress_gate',None);hall.set_editor_property('areas',areas)
 check(A.destroy_actor(gate),'Delete wall actor')
 check({a.get_path_name() for a in A.get_all_level_actors()}==before-{removed},'Exactly one actor removed')
 check(minerals._layout(minerals.get_field(hall))==mineral_layout,'All previous mineral edits preserved')
 check(L.save_current_level(),'Save MainGame')
 check(L.load_level('/Game/MainGame/Maps/Lvl_MainGame'),'Reload saved map')
 hall=next(a for a in A.get_all_level_actors() if isinstance(a,unreal.MiningExhibitHall))
 check(hall.areas[index].progress_gate is None,'Saved card exit has no completion gate')
 check(all((a.progress_gate.get_path_name() if a.progress_gate else None)==other_gates[i] for i,a in enumerate(hall.areas) if i!=index),'Other room gates preserved')
 H.start_floating_preview()
 while not E.get_game_world():yield .2
 yield 8
 w=E.get_game_world();pc=unreal.GameplayStatics.get_player_controller(w,0);pawn=pc.get_controlled_pawn()
 opening=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.MiningOpening)[0];opening.start_game()
 while not opening.get_editor_property('bFinished'):yield .2
 check(H.prepare_collection_preview_area(pc,index,False),'Activate card room without completing it')
 yield 2
 hall=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.MiningExhibitHall)[0];hall.set_actor_tick_enabled(False)
 escape=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.MiningRoomEscape)[0]
 check(escape.phase!=unreal.MiningEscapePhase.ESCAPED,'Check exit before completion')
 check(hall.areas[index].progress_gate is None,'Runtime does not restore the wall')
 for y in [-120,0,120]:
  for z in [-70,0,70]:
   p=position+unreal.Vector(0,y,z)
   hit=unreal.SystemLibrary.line_trace_single(w,p-unreal.Vector(100,0,0),p+unreal.Vector(100,0,0),unreal.TraceTypeQuery.ECC_VISIBILITY,True,[pawn],unreal.DrawDebugTrace.NONE,True)
   check(not hit or not hit.to_tuple()[0],'Former wall is clear at '+str((y,z)))
 room=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.MiningStartRoom)[0];room.set_actor_tick_enabled(False)
 for light in pawn.get_components_by_class(unreal.LightComponent):
  if light.get_name() in ['MiningLight','MiningFillLight']:light.set_visibility(True)
 pawn.get_component_by_class(unreal.CharacterMovementComponent).disable_movement()
 eye=position+unreal.Vector(-400,0,20);offset=pawn.get_actor_eyes_view_point()[0]-pawn.get_actor_location()
 pawn.set_actor_location(eye-offset,False,True);pc.set_control_rotation(unreal.MathLibrary.find_look_at_rotation(eye,position+unreal.Vector(500,0,0)));pc.set_view_target_with_blend(pawn,0)
 H.resize_preview_viewport(pc,1280,720);yield 1
 check(H.capture_preview(pc,str(OUT/'open_exit_before_completion.png')),'Capture open exit')
 r.update(passed=True,backup=str(backup));finish()
def finish(error=None):
 if error:r['error']=error
 (OUT/'result.json').write_text(json.dumps(r,ensure_ascii=False,indent=2),encoding='utf8')
 unreal.unregister_slate_post_tick_callback(handle);L.editor_request_end_play()
 cleanup={'at':time.monotonic()+2}
 def close(dt):
  if E.get_game_world() or time.monotonic()<cleanup['at']:return
  unreal.unregister_slate_post_tick_callback(cleanup['handle']);unreal.SystemLibrary.quit_editor()
 cleanup['handle']=unreal.register_slate_post_tick_callback(close)
def tick(dt):
 if s['busy'] or time.monotonic()<s['next']:return
 s['busy']=True
 try:
  assert time.monotonic()-s['start']<180,'Timeout'
  s['next']=time.monotonic()+next(gen)
 except StopIteration:pass
 except Exception:finish(traceback.format_exc())
 finally:s['busy']=False
gen=run();handle=unreal.register_slate_post_tick_callback(tick);unreal.EditorPythonScripting.set_keep_python_script_alive(True)
