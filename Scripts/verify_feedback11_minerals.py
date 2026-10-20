import unreal,json,time,traceback,math
from pathlib import Path
root=Path(unreal.Paths.project_dir());OUT=root/'Saved/Validation/Feedback11/Minerals';OUT.mkdir(parents=True,exist_ok=True)
H=unreal.MiningGUIEditorLibrary;E=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem);L=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
s={'next':0,'busy':False,'start':time.monotonic()};r={'passed':False,'checks':[]}
def check(ok,name):assert ok,name;r['checks'].append(name)
def pose(eye,target):
 pawn=s['pawn'];pawn.get_component_by_class(unreal.CharacterMovementComponent).disable_movement()
 offset=pawn.get_actor_eyes_view_point()[0]-pawn.get_actor_location()
 pawn.set_actor_location(eye-offset,False,True);s['pc'].set_control_rotation(unreal.MathLibrary.find_look_at_rotation(eye,target));s['pc'].set_view_target_with_blend(pawn,0)
def shot(name):check(H.capture_preview(s['pc'],str(OUT/(name+'.png'))),'Capture '+name)
def run():
 check(L.load_level('/Game/MainGame/Maps/Lvl_MainGame'),'Reload saved mineral changes')
 H.start_floating_preview()
 while not E.get_game_world():yield .2
 w=E.get_game_world();s['w']=w;s['pc']=unreal.GameplayStatics.get_player_controller(w,0);s['pawn']=s['pc'].get_controlled_pawn()
 yield 8
 pc=s['pc'];H.resize_preview_viewport(pc,1280,720)
 opening=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.MiningOpening)[0];opening.start_game()
 while not opening.get_editor_property('bFinished'):yield .2
 room=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.MiningStartRoom)[0];room.set_actor_tick_enabled(False)
 for light in s['pawn'].get_components_by_class(unreal.LightComponent):
  if light.get_name() in ['MiningLight','MiningFillLight']:light.set_visibility(True)
 hall=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.MiningExhibitHall)[0]
 field=next(a for a in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.MiningTreasureField) if a.get_editor_property('use_authored_placements'))
 pp=list(field.get_editor_property('authored_placements'));report=json.loads((root/'Saved/Validation/Feedback11/minerals.json').read_text(encoding='utf8'))
 check(report['passed'],'Targeted burial and supported-clue validation passed')
 index=next(i for i,a in enumerate(hall.areas) if str(a.tag)=='BM_Campaign_MainGame01');check(H.prepare_collection_preview_area(pc,index,False),'Activate early mine')
 hall.set_actor_tick_enabled(False);yield 2
 for row in report['moves']:
  p=pp[row['index']];target=p.surface.translation;check((target-unreal.Vector(*row['after']['surface'])).length()<.01,'Saved deposit '+str(row['index']))
  normal=unreal.MathLibrary.get_forward_vector(p.surface.rotation.rotator())
  if row['id']=='2':
   # Stand on the real nearby floor, then aim down at the copper surface.
   stands=[]
   for angle in range(0,360,45):
    stand=target+unreal.Vector(math.cos(math.radians(angle))*110,math.sin(math.radians(angle))*110,0)
    hit=unreal.SystemLibrary.line_trace_single(w,stand+unreal.Vector(0,0,150),stand-unreal.Vector(0,0,300),unreal.TraceTypeQuery.ECC_VISIBILITY,True,[s['pawn']],unreal.DrawDebugTrace.NONE,True)
    if not hit:continue
    h=hit.to_tuple();eye=h[5]+unreal.Vector(0,0,150)
    if h[0] and h[3]>10 and h[7].z>.65 and (target-eye).length()<300:stands.append(eye)
   check(bool(stands),'Copper has a walkable standing point within ground-level pickaxe reach')
   eye=min(stands,key=lambda p:(target-p).length())
   r.setdefault('copper_reach_cm',{})[str(row['index'])]=(target-eye).length()
  else:eye=target+normal*330+unreal.Vector(0,0,75)
  pose(eye,target);yield .7;shot('deposit_'+str(row['index']))
 ant=next(p for a in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.MiningCollectionField) for p in a.get_editor_property('authored_placements') if p.item_id==1)
 pose(ant.approach_transform.translation+unreal.Vector(0,0,100),ant.clue_transform.translation);yield .7;shot('ant_collection_clear')
 museum=next(i for i,a in enumerate(hall.areas) if str(a.tag)=='BM_Campaign_MainGame03')
 check(H.prepare_collection_preview_area(pc,museum,False),'Activate museum approach')
 pose(unreal.Vector(47020,11740,15520),unreal.Vector(47200,11750,15220));yield 1;shot('museum_downward_soil')
 r['passed']=True;finish()
def finish(error=None):
 if error:r['error']=error
 (OUT/'runtime.json').write_text(json.dumps(r,ensure_ascii=False,indent=2),encoding='utf8')
 unreal.unregister_slate_post_tick_callback(handle);L.editor_request_end_play()
 cleanup={'at':time.monotonic()+2}
 def close(dt):
  if E.get_game_world() or time.monotonic()<cleanup['at']:return
  unreal.unregister_slate_post_tick_callback(cleanup['handle']);L.load_level('/Engine/Maps/Entry');unreal.SystemLibrary.quit_editor()
 cleanup['handle']=unreal.register_slate_post_tick_callback(close)
def tick(dt):
 if s['busy'] or time.monotonic()<s['next']:return
 s['busy']=True
 try:
  assert time.monotonic()-s['start']<170,'Timed out'
  s['next']=time.monotonic()+next(gen)
 except StopIteration:pass
 except Exception:finish(traceback.format_exc())
 finally:s['busy']=False
gen=run();handle=unreal.register_slate_post_tick_callback(tick);unreal.EditorPythonScripting.set_keep_python_script_alive(True)
