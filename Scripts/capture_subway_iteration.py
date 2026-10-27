"""Render the actual runtime collectible meshes, in an unsaved neutral studio."""
import unreal,time,json,traceback
from pathlib import Path
OUT=Path(unreal.Paths.project_dir())/'Saved/Validation/CollectibleIteration/Subway';OUT.mkdir(parents=True,exist_ok=True)
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
   field.authored_placements=[unreal.MiningCollectionPlacement(item_id=i,transform=unreal.Transform(location=unreal.Vector(0,(i-4)*500,61),rotation=unreal.Rotator(0,0,0),scale=unreal.Vector(1,1,1))) for i in range(21,22)]
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
   if len(field.items)!=1:return
   pc=unreal.GameplayStatics.get_player_controller(w,0);camera=unreal.GameplayStatics.get_all_actors_with_tag(w,'ScenePolishStudioCamera')[0]
   unreal.WidgetLayoutLibrary.remove_all_widgets(w)
   unreal.SystemLibrary.execute_console_command(w,'showhud 0');unreal.SystemLibrary.execute_console_command(w,'r.ScreenPercentage 100')
   pc.set_view_target_with_blend(camera,0);pc.show_mouse_cursor=False
   pawn=unreal.GameplayStatics.get_player_pawn(w,0);pawn.set_actor_hidden_in_game(True);pawn.set_actor_tick_enabled(False)
   s.update(field=field,pc=pc,items={x.item_id:x for x in field.items},index=20,phase=2);return
  if phase==2:
   ident=s['index']+1
   for i,item in s['items'].items():
    item.set_actor_hidden_in_game(i!=ident);item.get_editor_property('body').set_simulate_physics(False);item.get_editor_property('body').set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION);item.set_actor_location(unreal.Vector(0,0,120 if i==9 else 75) if i==ident else unreal.Vector(0,10000+i*500,75),False,True);item.set_actor_scale3d(unreal.Vector(.25,.25,.25) if i==9 else unreal.Vector(1,1,1));item.tags=['CollectionPreview'];item.set_actor_rotation(unreal.Rotator(0,0,0),False);item.get_editor_property('Model').set_visibility(True,True);item.get_editor_property('Model').set_hidden_in_game(False,True)
   target=unreal.Vector(0,0,95 if ident==9 else 75)
   camera=unreal.GameplayStatics.get_all_actors_with_tag(w,'ScenePolishStudioCamera')[0]
   camera.set_actor_location(target+unreal.Vector(-330,110,85)*(1.35 if ident==21 else 1),False,True)
   camera.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(camera.get_actor_location(),target),False)
   for item in s['items'].values():item.set_actor_hidden_in_game(True)
   item=helper.spawn_preview_collectible(s['field'],21,unreal.Vector(0,0,75));item.set_actor_enable_collision(False)
   s.update(train=item,phase=5,after=now+.02,frame=0,movie_start=now);return
  if phase==5:
   name=f"frame-{s['frame']:03}.png"
   assert helper.capture_preview(s['pc'],str(OUT/name))
   parts=list(s['train'].get_components_by_class(unreal.StaticMeshComponent))
   visible=[p for p in parts[:108:3] if p.get_editor_property('bVisible')]
   report.setdefault('samples',[]).append({'time':now-s['movie_start'],'visible':len(visible),'x':[p.get_editor_property('RelativeLocation').x for p in visible]})
   report['images'].append(name);s['frame']+=1
   if s['frame']>=75:finish()
   else:s['after']=now+.12

 except Exception:finish(traceback.format_exc())
 finally:s['busy']=False
handle=unreal.register_slate_post_tick_callback(tick)
