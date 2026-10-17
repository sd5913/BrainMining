"""Reference effects for cooking, verify level anchors, and capture the P2 art pass."""
import unreal,json,time
from pathlib import Path
A=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);L=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
OUT=Path(unreal.Paths.project_dir()).resolve()/'Saved/Automation/CountrysideP2';BASE='/Game/Passages/Echo/ArtP2'
assert L.get_current_level().get_outer().get_name()=='Lvl_EchoMazeToCards_P2'
actors=list(A.get_all_level_actors())
for key in ['ArcImpact','Poison','Ignition','Finale','Spark','Confetti','Muzzle']:
 label='P2 FX asset bank / '+key;a=next((a for a in actors if a.get_actor_label()==label),None)
 if not a:a=A.spawn_actor_from_class(unreal.NiagaraActor,unreal.Vector(0,0,-100000));a.set_actor_label(label);a.set_folder_path('GiantRoom/CountrysideP2/FXAssetBank')
 c=a.get_component_by_class(unreal.NiagaraComponent);c.set_editor_property('auto_activate',False);c.set_asset(unreal.load_asset(BASE+'/VFX/NS_P2_'+key));c.deactivate();a.set_actor_hidden_in_game(True);a.set_is_temporarily_hidden_in_editor(True)
stage=next(a for a in actors if isinstance(a,unreal.CardRoomStage))
for c in stage.get_components_by_class(unreal.SceneComponent):
 if c.get_name()=='Opponent':
  c.set_relative_location(stage.get_editor_property('opponent_base_local'),False,False);s=stage.get_editor_property('opponent_scale');c.set_relative_scale3d(unreal.Vector(s,s,s));c.set_visibility(True,True)
old=json.loads((OUT/'before.json').read_text(encoding='utf-8'));by={a.get_name():a for a in A.get_all_level_actors()}
def xyz(v):return [round(v.x,3),round(v.y,3),round(v.z,3)]
changed=[];missing=[]
for row in old:
 a=by.get(row['name'])
 if not a:missing.append(row['label']);continue
 r=a.get_actor_rotation()
 if xyz(a.get_actor_location())!=row['loc'] or xyz(a.get_actor_scale3d())!=row['scale'] or any(abs(x-y)>.01 for x,y in zip([r.pitch,r.yaw,r.roll],row['rotation'])):changed.append(row['label'])
assert not changed and not missing,(changed,missing)
(OUT/'anchor_validation.json').write_text(json.dumps(dict(original_actor_count=len(old),missing=missing,changed_transform=changed,passed=True),indent=2),encoding='utf-8')
assert L.save_current_level()
queue=[('01-overview',(8000,8300,5800),(14500,3600,3700)),('02-tabletop',(11300,6900,4300),(15400,3600,4900)),('03-plush-cat',(12500,3200,5150),(16000,3600,5000)),('04-walkable-route',(7600,1600,1900),(9500,-100,1800))]
state={'index':0,'time':time.monotonic(),'stage':0}
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
def tick(dt):
 now=time.monotonic()
 if now-state['time']<2:return
 if state['index']>=len(queue):
  unreal.unregister_slate_post_tick_callback(handle);unreal.log('P2_ART_CAPTURE_COMPLETE');return
 name,p,q=queue[state['index']]
 if state['stage']==0:
  unreal.EditorLevelLibrary.set_level_viewport_camera_info(unreal.Vector(*p),unreal.MathLibrary.find_look_at_rotation(unreal.Vector(*p),unreal.Vector(*q)));state['stage']=1
 elif state['stage']==1:
  path=str(OUT/(name+'.png')).replace('\\','/');state['task']=unreal.AutomationLibrary.take_high_res_screenshot(1600,900,path,delay=.5);state['stage']=2
 elif state['task'].is_task_done():state['stage']=0;state['index']+=1
 state['time']=now
handle=unreal.register_slate_post_tick_callback(tick)
