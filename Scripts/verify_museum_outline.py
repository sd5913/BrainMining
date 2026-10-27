"""Verify real mesh silhouettes and the antique screen in MainGame PIE."""
import unreal,time,pathlib,json,traceback
out=pathlib.Path(unreal.Paths.project_saved_dir())/'Validation/MuseumOutline'
ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem);helper=unreal.MiningGUIEditorLibrary
report={'passed':False,'checks':[]};state={'next':0}
def check(ok,label):
 assert ok,label
 report['checks'].append(label)
def run():
 while not ed.get_game_world():yield .2
 w=ed.get_game_world()
 def allof(c):return unreal.GameplayStatics.get_all_actors_of_class(w,c)
 while not allof(unreal.MiningExhibitHall):yield .2
 hall=allof(unreal.MiningExhibitHall)[0]
 while not hall.get_editor_property('bReady'):yield .2
 for o in allof(unreal.MiningOpening):o.start_game();o.confirm_new_game(True)
 while any(not o.get_editor_property('bFinished') for o in allof(unreal.MiningOpening)):yield .2
 pc=unreal.GameplayStatics.get_player_controller(w,0);pawn=unreal.GameplayStatics.get_player_pawn(w,0)
 index=next(i for i,a in enumerate(hall.areas) if str(a.tag)=='BM_Campaign_MainGameMuseum')
 check(helper.prepare_collection_preview_area(pc,index,False),'MainGame streamed museum active');hall.set_actor_tick_enabled(False);yield 2
 puzzle=allof(unreal.WenSiMirrorPuzzle)[0];origin=puzzle.get_actor_location();ex={a.get_editor_property('kind'):a for a in allof(unreal.WenSiMirrorExhibit)}
 check(ex[5].get_editor_property('art_model').get_name()=='SM_AntiqueScreen','Poly Haven antique painted screen assigned')
 pp=next(c for c in puzzle.get_components_by_class(unreal.PostProcessComponent) if c.get_name()=='MuseumSilhouette')
 check(pp.get_editor_property('enabled'),'Museum silhouette postprocess active')
 for k in (0,3,5,6,8,12):
  c=ex[k].get_editor_property('art_visual')
  check(c.get_editor_property('render_custom_depth') and c.get_editor_property('custom_depth_stencil_value')==1,'White silhouette enabled on rendered exhibit '+str(k))
  check(c.get_overlay_material() is None,'UV mask cannot cover surface '+str(k))
  check(c.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION,'Authored art adds no collider '+str(k))
 result=puzzle.validate_mirror_puzzle();report['puzzle_checks']=result;check(result.startswith('PASS'),result)
 # The silhouette must come from rendered surfaces, never the old hidden proxies.
 for k in (0,1,3,4,5,6,7,8,9,10,12):
  parts=[c for c in ex[k].get_components_by_class(unreal.StaticMeshComponent) if c.get_name().startswith('Part')]
  for c in parts:
   check(c.get_overlay_material() is None,'No face/UV overlay '+str(k)+'/'+c.get_name())
   expected=bool(c.static_mesh and c.is_visible() and c.get_editor_property('render_in_main_pass'))
   check(c.get_editor_property('render_custom_depth')==expected,'Only rendered silhouette '+str(k)+'/'+c.get_name())
 smoke=next(c for c in ex[4].get_components_by_class(unreal.NiagaraComponent) if c.get_name()=='MuseumSmoke')
 check(smoke.get_editor_property('render_custom_depth'),'Real Niagara plume writes silhouette')
 check(smoke.get_editor_property('custom_depth_stencil_value')==1,'Smoke shares museum stencil')
 sm=unreal.load_asset('/Game/WenSi/Museum/Refresh/Materials/M_MuseumSmokeSilhouette')
 check(sm.get_editor_property('allow_translucent_custom_depth_writes'),'Smoke material supports translucent custom depth')
 move=pawn.get_component_by_class(unreal.CharacterMovementComponent);move.set_movement_mode(unreal.MovementMode.MOVE_FLYING)
 def pose(eye,target):
  p=origin+unreal.Vector(*eye);offset=pawn.get_actor_eyes_view_point()[0]-pawn.get_actor_location();pawn.set_actor_location(p-offset,False,True);move.stop_movement_immediately();pc.set_control_rotation(unreal.MathLibrary.find_look_at_rotation(p,origin+unreal.Vector(*target)))
 for k,name,eye,target in [(4,'smoke',(0,-180,145),(160,-330,100)),(4,'smoke_later',(0,-180,145),(160,-330,100)),(1,'cover',(410,-80,170),(650,0,120)),(7,'glass_cover',(1160,230,190),(1340,320,120)),(9,'sphere',(1770,20,215),(1900,120,257)),(10,'mirror',(1200,-200,160),(1050,-340,135)),(5,'screen_solid',(760,0,150),(970,0,120)),(12,'cat',(1000,310,150),(1110,480,110)),(6,'horse',(1390,-200,140),(1500,-430,100)),(0,'rock',(410,-80,170),(650,0,100)),(3,'balloon',(-560,-160,280),(-450,-350,365)),(8,'metal',(1290,280,160),(1340,320,115))]:
  pose(eye,target);yield 1.6;check(helper.capture_preview(pc,str(out/(name+'.png'))),'Capture '+name)
 ex[5].set_editor_property('word',unreal.WenSiMirrorWord.PASSABLE);ex[5].refresh_art_model();yield 1
 c=ex[5].get_editor_property('art_visual');check(c.get_editor_property('render_custom_depth'),'Passable screen retains custom depth')
 m=c.get_material(0);check(m.get_editor_property('parent').get_name()=='M_MuseumPassable','Passable screen uses translucent depth-writing material')
 pose((760,0,150),(970,0,120));yield 1.5;check(helper.capture_preview(pc,str(out/'screen_passable.png')),'Capture passable screen')
 ex[4].set_editor_property('word',unreal.WenSiMirrorWord.PASSABLE);ex[4].refresh_art_model();yield 2
 check(smoke.get_editor_property('render_custom_depth'),'Passable smoke retains Niagara silhouette')
 pose((0,-180,145),(160,-330,100));yield 2;check(helper.capture_preview(pc,str(out/'smoke_passable.png')),'Capture passable smoke')
 report['passed']=True
 unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play();yield .5
 print('MUSEUM_OUTLINE_VERIFIED',report['passed'])
task=run()
def tick(dt):
 if time.monotonic()<state['next']:return
 try:state['next']=time.monotonic()+next(task)
 except StopIteration:
  (out/'validation.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8');unreal.unregister_slate_post_tick_callback(handle)
 except Exception:
  report['error']=traceback.format_exc();(out/'validation.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8');unreal.unregister_slate_post_tick_callback(handle);print(report['error'])
handle=unreal.register_slate_post_tick_callback(tick)
if not ed.get_game_world():helper.start_floating_preview()
