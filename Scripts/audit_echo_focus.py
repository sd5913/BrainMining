"""Compare collectible focus with the unchanged echo presentation in PIE only."""
import unreal,time,json,traceback
from pathlib import Path
ROOT=Path(unreal.Paths.project_dir());OUT=ROOT/'Saved/Validation/CollectionFeedback'
ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem);levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
s=dict(phase=0,next=0,start=time.monotonic(),details={})
def actors(cls):return unreal.GameplayStatics.get_all_actors_of_class(s['w'],cls)
def shot(name):assert unreal.MiningGUIEditorLibrary.capture_preview(s['pc'],str(OUT/(name+'.png')))
def finish(ok,error=''):
 s['done']=True;unreal.unregister_slate_post_tick_callback(handle)
 (OUT/'echo_focus_audit.json').write_text(json.dumps(dict(passed=ok,error=error,details=s['details']),ensure_ascii=False,indent=2),encoding='utf-8')
 if ed.get_game_world():levels.editor_request_end_play()
 clean={'next':time.monotonic()+2,'loaded':False}
 def close(dt):
  if ed.get_game_world() or time.monotonic()<clean['next']:return
  if not clean['loaded']:s.clear();levels.load_level('/Engine/Maps/Entry');clean.update(loaded=True,next=time.monotonic()+1);return
  unreal.unregister_slate_post_tick_callback(clean['handle']);unreal.SystemLibrary.quit_editor()
 clean['handle']=unreal.register_slate_post_tick_callback(close)
def tick(dt):
 if s.get('busy') or s.get('done'):return
 s['busy']=True
 try:
  now=time.monotonic();ph=s['phase']
  if now-s['start']>240:raise RuntimeError('Timeout '+str(ph))
  if now<s['next']:return
  if ph==0:
   assert levels.load_level('/Game/MainGame/Maps/Lvl_MainGame');unreal.SystemLibrary.execute_console_command(ed.get_editor_world(),'mining.SaveEnabled 0');unreal.MiningGUIEditorLibrary.start_floating_preview();s.update(phase=1,next=now+6);return
  if ph==1:
   w=ed.get_game_world()
   if not w:return
   s['w']=w;hall=actors(unreal.MiningExhibitHall)[0]
   if not hall.ready:return
   for a in actors(unreal.MiningOpening):a.start_game()
   s.update(pc=unreal.GameplayStatics.get_player_controller(w,0),pawn=unreal.GameplayStatics.get_player_pawn(w,0),phase=2,next=now+5);return
  if ph==2:
   for a in actors(unreal.MiningOpening):
    if not a.finished:a.start_game();s['next']=now+.5;return
   assert unreal.MiningGUIEditorLibrary.prepare_collection_preview_area(s['pc'],0,False)
   s['pawn'].get_component_by_class(unreal.CharacterMovementComponent).disable_movement()
   field=actors(unreal.MiningCollectionField)[0];p=next(p for p in field.authored_placements if p.item_id==2)
   front=p.clue_transform.translation;normal=unreal.MathLibrary.transform_direction(p.clue_transform,unreal.Vector(-1,0,0))
   s['pawn'].set_actor_location(front+normal*230+unreal.Vector(0,0,-30),False,True);s['pc'].set_control_rotation(unreal.MathLibrary.find_look_at_rotation(s['pawn'].get_actor_eyes_view_point()[0],front))
   s.update(phase=21,next=now+2);return
  if ph==21:
   shot('reagent_stain_review')
   assert unreal.MiningGUIEditorLibrary.prepare_collection_preview_area(s['pc'],3,False)
   s['pawn'].get_component_by_class(unreal.CharacterMovementComponent).disable_movement()
   field=actors(unreal.MiningCollectionField)[0];item=next(a for a in field.items if a.item_id==6);echo=actors(unreal.EchoPassageStage)[0]
   p=item.get_actor_location();s['pawn'].set_actor_location(p+unreal.Vector(0,220,45),False,True);s['pc'].set_control_rotation(unreal.MathLibrary.find_look_at_rotation(s['pawn'].get_actor_eyes_view_point()[0],p))
   pp=next(c for c in field.get_components_by_class(unreal.PostProcessComponent) if c.get_name()=='CollectionSilhouette')
   contour=next(c for c in echo.get_components_by_class(unreal.InstancedStaticMeshComponent) if c.get_name()=='EchoContours').get_material(0)
   contour.set_scalar_parameter_value('Preview',.6)
   settings=[]
   for a in actors(unreal.Actor):
    entries=[('Volume',a.settings)] if isinstance(a,unreal.PostProcessVolume) else []
    entries += [(c.get_name(),c.settings) for c in a.get_components_by_class(unreal.PostProcessComponent)]
    entries += [(c.get_name(),c.post_process_settings) for c in a.get_components_by_class(unreal.CameraComponent)]
    for name,v in entries:
     row={'actor':a.get_name(),'component':name}
     for k in ['scene_fringe_intensity','white_temp','white_tint','color_saturation','color_gain','scene_color_tint']:
      row[k]=str(v.get_editor_property(k));row['override_'+k]=v.get_editor_property('override_'+k)
     row['blendables']=str(v.get_editor_property('weighted_blendables'));settings.append(row)
   s['details']['post_processes']=settings
   s['details']['shaders']={o.get_path_name():o.get_editor_property('code') for o in unreal.ObjectIterator(unreal.MaterialExpressionCustom) if 'M_EchoContour' in o.get_path_name() or 'M_CollectionSilhouette' in o.get_path_name()}
   s.update(field=field,item=item,echo=echo,pp=pp,phase=3,next=now+2);return
  if ph==3:
   assert s['field'].target==s['item']
   assert s['item'].get_component_by_class(unreal.DynamicMeshComponent).get_editor_property('render_custom_depth')
   shot('echo_audit_focus_on');s['pp'].set_editor_property('enabled',False);s.update(phase=4,next=now+1);return
  if ph==4:
   shot('echo_audit_collection_pp_off');unreal.SystemLibrary.execute_console_command(s['w'],'r.SceneColorFringeQuality 0');s.update(phase=5,next=now+1);return
  if ph==5:
   shot('echo_audit_fringe_off');unreal.SystemLibrary.execute_console_command(s['w'],'r.SceneColorFringeQuality 1');s['pp'].set_editor_property('enabled',True);r=s['pc'].get_control_rotation();s['pc'].set_control_rotation(unreal.Rotator(pitch=r.pitch,yaw=r.yaw+25,roll=r.roll));s.update(phase=6,next=now+1);return
  if ph==6:
   assert s['field'].target!=s['item']
   assert not s['item'].get_component_by_class(unreal.DynamicMeshComponent).get_editor_property('render_custom_depth')
   shot('echo_audit_aim_away');finish(True)
 except Exception:finish(False,traceback.format_exc())
 finally:s['busy']=False
handle=unreal.register_slate_post_tick_callback(tick)
