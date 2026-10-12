"""Observe real thrown-bomb materials while running the sticky-surface regression."""
import unreal,json,time,traceback
from pathlib import Path
root=Path(unreal.Paths.project_dir());out=root/'Saved/Validation/BombCountdown';out.mkdir(parents=True,exist_ok=True)
observed={};checks=[];started=time.monotonic();done=False
previous=root/'Saved/Validation/StickyBomb/runtime.json'
if previous.exists():previous.rename(previous.with_name('runtime-before-countdown.json'))
def watch(dt):
 global done
 if done:return
 try:
  world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
  if world:
   for bomb in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.MiningBombActor):
    c=bomb.get_component_by_class(unreal.MiningBombPresentationComponent);assert c,'Thrown bomb has presentation component'
    assert bomb.model.static_mesh.get_path_name()=='/Game/Bomb/SM_StickyBomb.SM_StickyBomb'
    li=bomb.model.get_material_index('BombFuseLamp');di=bomb.model.get_material_index('BombCountdownDisplay');assert li>=0 and di>=0
    lm=bomb.model.get_material(li);dm=bomb.model.get_material(di);assert isinstance(lm,unreal.MaterialInstanceDynamic) and isinstance(dm,unreal.MaterialInstanceDynamic)
    left=dm.get_scalar_parameter_value('RemainingSeconds');glow=lm.get_scalar_parameter_value('BlinkStrength');age=unreal.GameplayStatics.get_time_seconds(world)-bomb.launched_at
    assert abs(left-max(0,3-age))<.12,'Display tracks the throw-started fuse'
    key=bomb.get_name();data=observed.setdefault(key,{'samples':[],'material_names':[lm.get_path_name(),dm.get_path_name()]})
    samples=data['samples']
    if not samples or age-samples[-1]['age']>.045:samples.append({'age':age,'remaining':left,'glow':glow,'stuck':bomb.get_editor_property('bStuck')})
  if previous.exists():
   sticky=json.loads(previous.read_text());assert sticky['passed'],sticky.get('reason')
   assert len(observed)==4,len(observed)
   materials=[]
   for name,data in observed.items():
    values=data['samples'];assert len(values)>15,name
    assert min(s['remaining'] for s in values)<.25,name+': reaches end of fuse'
    assert max(s['remaining'] for s in values)>2.5,name+': starts from three'
    assert max(s['glow'] for s in values)-min(s['glow'] for s in values)>3,name+': lamps flash visibly'
    assert any(s['stuck'] for s in values),name+': countdown keeps running when attached'
    assert all(b['remaining']<=a['remaining']+.02 for a,b in zip(values,values[1:])),name+': no timer reset on contact'
    materials.append(data['material_names'][0]);checks.append(name+': correct mesh, isolated dynamic materials, decreasing countdown and blinking on attached surfaces')
   assert len(set(materials))==4,'Each bomb owns separate material instances';checks.append('Four bombs own independent lamp/display material instances')
   done=True;unreal.unregister_slate_post_tick_callback(watcher)
   (out/'runtime.json').write_text(json.dumps({'passed':True,'checks':checks,'sticky_checks':len(sticky['checks']),'bombs':observed},indent=2))
  elif time.monotonic()-started>150:raise RuntimeError('Countdown regression timeout')
 except Exception:
  done=True;unreal.unregister_slate_post_tick_callback(watcher);(out/'runtime.json').write_text(json.dumps({'passed':False,'reason':traceback.format_exc(),'bombs':observed},indent=2))
watcher=unreal.register_slate_post_tick_callback(watch)
regression_namespace={'__name__':'sticky_regression'}
exec(compile((root/'Scripts/verify_sticky_bomb.py').read_text(encoding='utf-8'),'verify_sticky_bomb.py','exec'),regression_namespace)
