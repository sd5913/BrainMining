"""Render the actual runtime collectible meshes, in an unsaved neutral studio."""
import unreal,time,json,traceback
from pathlib import Path
OUT=Path(unreal.Paths.project_dir())/'Saved/Validation/ScenePolish';OUT.mkdir(parents=True,exist_ok=True)
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem);api=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
helper=unreal.MiningGUIEditorLibrary
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
s={'phase':0,'after':0,'started':time.monotonic()};report={'passed':False,'images':[]}
def finish(error=None):
 report.update(passed=error is None,error=error);(OUT/'collectible_visual.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
 levels.editor_request_end_play();s.update(phase=99,after=time.monotonic()+3)
def tick(dt):
 if s.get('busy') or time.monotonic()<s['after']:return
 s['busy']=True
 try:
  now=time.monotonic();phase=s['phase']
  if phase==99:
   if ed.get_game_world():return
   unreal.unregister_slate_post_tick_callback(handle);levels.load_level('/Engine/Maps/Entry');unreal.SystemLibrary.quit_editor();return
  assert now-s['started']<240,'Studio render timeout'
  if phase==0:
   assert levels.load_level('/Engine/Maps/Entry');w=ed.get_editor_world()
   w.get_world_settings().set_editor_property('default_game_mode',unreal.MiningCollectionGameMode)
   field=api.spawn_actor_from_class(unreal.MiningCollectionField,unreal.Vector());field.use_authored_placements=True
   field.authored_placements=[unreal.MiningCollectionPlacement(item_id=i,transform=unreal.Transform(location=unreal.Vector(0,(i-4)*500,61))) for i in [1,4,8]]
   floor=api.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(0,0,-5));floor.static_mesh_component.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Cube'));floor.set_actor_scale3d(unreal.Vector(60,60,.1))
   floor.static_mesh_component.set_material(0,unreal.load_asset('/Game/Rooms/CardDuel/Materials/M_Dark'))
   api.spawn_actor_from_class(unreal.PlayerStart,unreal.Vector(-600,0,100))
   for p,color,intensity in [((-210,-200,330),(1,.87,.72),4200),((-120,260,180),(.55,.72,1),2400),((160,30,260),(1,.65,.4),3500)]:
    l=api.spawn_actor_from_class(unreal.PointLight,unreal.Vector(*p));l.light_component.set_intensity_units(unreal.LightUnits.CANDELAS);l.light_component.set_intensity(intensity);l.light_component.set_source_radius(45);l.light_component.set_attenuation_radius(1500);l.light_component.set_light_color(unreal.LinearColor(*color,1))
   camera=api.spawn_actor_from_class(unreal.CameraActor,unreal.Vector(-330,150,160));camera.tags=['ScenePolishStudioCamera'];camera.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(camera.get_actor_location(),unreal.Vector(0,0,58)),False)
   c=camera.get_component_by_class(unreal.CameraComponent);c.set_field_of_view(40)
   pp=c.get_editor_property('post_process_settings')
   for key,value in [('auto_exposure_min_brightness',6.),('auto_exposure_max_brightness',6.),('auto_exposure_bias',0.),('auto_exposure_apply_physical_camera_exposure',False),('bloom_intensity',.1)]:
    pp.set_editor_property('override_'+key,True);pp.set_editor_property(key,value)
   c.set_editor_property('post_process_settings',pp);c.set_editor_property('post_process_blend_weight',1.)
   helper.start_floating_preview();s.update(phase=1,after=now+6);return
  w=ed.get_game_world()
  if not w:return
  if phase==1:
   field=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.MiningCollectionField)[0]
   if len(field.items)!=3:return
   pc=unreal.GameplayStatics.get_player_controller(w,0);camera=unreal.GameplayStatics.get_all_actors_with_tag(w,'ScenePolishStudioCamera')[0]
   field.set_actor_tick_enabled(False);unreal.WidgetLayoutLibrary.remove_all_widgets(w)
   unreal.SystemLibrary.execute_console_command(w,'showhud 0');unreal.SystemLibrary.execute_console_command(w,'r.ScreenPercentage 100')
   pc.set_view_target_with_blend(camera,0);pc.show_mouse_cursor=False
   pawn=unreal.GameplayStatics.get_player_pawn(w,0);pawn.set_actor_hidden_in_game(True);pawn.set_actor_tick_enabled(False)
   s.update(field=field,pc=pc,items={x.item_id:x for x in field.items},index=0,phase=2);return
  if phase==2:
   ident=[1,4,8][s['index']]
   for i,item in s['items'].items():
    item.set_actor_hidden_in_game(i!=ident);item.get_editor_property('body').set_simulate_physics(False);item.get_editor_property('body').set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION);item.set_actor_location(unreal.Vector(0,0,57 if i!=8 else 45),False,True)
   s.update(phase=3,after=now+3);return
  if phase==3:
   name=['ant_computer','calculator','cola'][s['index']]+'.png'
   unreal.SystemLibrary.execute_console_command(w,'HighResShot 1600x1000 filename="'+str(OUT/name)+'"')
   s.update(phase=4,after=now+3,capture_started=time.time());return
  if phase==4:
   name=['ant_computer','calculator','cola'][s['index']]+'.png'
   assert (OUT/name).stat().st_mtime>=s['capture_started']-1,'No fresh viewport screenshot'
   report['images'].append(name)
   s['index']+=1
   if s['index']==3:finish()
   else:s['phase']=2
 except Exception:finish(traceback.format_exc())
 finally:s['busy']=False
handle=unreal.register_slate_post_tick_callback(tick)
