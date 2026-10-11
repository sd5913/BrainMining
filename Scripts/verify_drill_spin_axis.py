"""Check the shaft remains in place while real V/LMB input spins offset drill models."""
import json,time,traceback,re
from pathlib import Path
import unreal
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
root=Path(unreal.Paths.project_dir());out=root/'Saved/Validation/DrillSpinAxis';out.mkdir(parents=True,exist_ok=True)
editor=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
h=unreal.MiningGUIEditorLibrary
perf=unreal.get_default_object(unreal.load_class(None,'/Script/UnrealEd.EditorPerformanceSettings'))
old=perf.get_editor_property('bThrottleCPUWhenNotForeground');perf.set_editor_property('bThrottleCPUWhenNotForeground',False)
report={'passed':False,'checks':[],'max_shaft_drift_cm':0.,'samples':[]};s={'phase':'setup','started':time.monotonic()};busy=False
def check(ok,label):
    assert ok,label
    report['checks'].append(label)
def numbers(value):return [float(x) for x in re.findall(r'(?:x|y|z|pitch|yaw|roll): ([+-]?[0-9.]+)',value)]
before=json.loads((root/'Saved/Drill/spin-issue-before.json').read_text())
for path,entry in before.items():
    if not path.startswith('/Game/'):continue
    bp=unreal.load_asset(path);cdo=unreal.get_default_object(bp.generated_class())
    for name,expected in entry.items():
        c=cdo.get_editor_property(name)
        for key,prop in [('position','relative_location'),('rotation','relative_rotation'),('scale','relative_scale3d')]:
            actual=numbers(str(c.get_editor_property(prop)));target=numbers(expected[key])
            check(all(abs(a-b)<.0001 for a,b in zip(actual,target)),path.split('/')[-1]+' '+name+' '+key+' unchanged')
    e=cdo.get_component_by_class(unreal.MiningToolEffectsComponent)
    check(e.drill_spin_axis==unreal.Vector(1,0,0) and e.drill_spin_pivot==unreal.Vector(),'Editable model-local axis and pivot available')
for name in ['SM_Drill','CF_DrillSpin']:
    check((root/'Content/Drill'/(name+'.uasset')).read_bytes()==(root/'Saved/Drill/SpinAxisBackup'/(name+'.uasset')).read_bytes(),name+' asset kept byte-for-byte')
def key(n,v):h.send_preview_key(s['pc'],n,v)
def shaft_point(model,local):
    return unreal.MathLibrary.transform_location(model.get_attach_parent().get_relative_transform(),
        unreal.MathLibrary.transform_location(model.get_relative_transform(),local))
def distance(a,b):return ((a.x-b.x)**2+(a.y-b.y)**2+(a.z-b.z)**2)**.5
def capture_targets():
    s['targets']={}
    for model in [s['e'].drill_mesh,s['e'].left_drill_mesh]:
        for along in [0.,15.]:
            p=unreal.Vector(along,0,0)
            s['targets'][(model.get_name(),along)]=unreal.MathLibrary.transform_location(model.get_relative_transform(),p)
    s['angles']=[]
def measure():
    errors=[]
    for model in [s['e'].drill_mesh,s['e'].left_drill_mesh]:
        for along in [0.,15.]:
            errors.append(distance(shaft_point(model,unreal.Vector(along,0,0)),s['targets'][(model.get_name(),along)]))
    drift=max(errors);report['max_shaft_drift_cm']=max(report['max_shaft_drift_cm'],drift)
    assert drift<.3,'Shaft orbits the mount: '+str(drift)
    s['angles'].append(s['e'].drill_spin_degrees)
def finish(ok,reason=''):
    unreal.unregister_slate_post_tick_callback(cb)
    if editor.get_game_world() and 'pc' in s:key('LeftMouseButton',False)
    perf.set_editor_property('bThrottleCPUWhenNotForeground',old)
    report.update(passed=ok,reason=reason,elapsed=time.monotonic()-s['started'])
    (out/'runtime.json').write_text(json.dumps(report,indent=2))
    unreal.log('DRILL_SPIN_AXIS_RESULT '+json.dumps(report))
    if editor.get_game_world():levels.editor_request_end_play()
def tick(dt):
    if time.monotonic()-s['started']>100:raise RuntimeError('Spin validation timeout')
    if s['phase']!='setup' and editor.get_game_world() is None:raise RuntimeError('PIE session was ended during validation')
    if s['phase']=='setup':
        world=editor.get_game_world()
        if not world:return
        if 'ready' not in s:s['ready']=time.monotonic();return
        if time.monotonic()-s['ready']<4:return
        pc=unreal.GameplayStatics.get_player_controller(world,0);pawn=unreal.GameplayStatics.get_player_character(world,0)
        e=pawn.get_component_by_class(unreal.MiningToolEffectsComponent)
        s.update(world=world,pc=pc,pawn=pawn,e=e)
        check('ExternalMiningTestCharacter' in pawn.get_class().get_name(),'Test uses actual ExternalMining character')
        pawn.get_component_by_class(unreal.CharacterMovementComponent).set_movement_mode(unreal.MovementMode.MOVE_NONE)
        key('V',True);s.update(phase='equip',next=unreal.GameplayStatics.get_time_seconds(world)+.35);return
    now=unreal.GameplayStatics.get_time_seconds(s['world'])
    if s['phase']=='equip' and now>=s['next']:
        key('V',False);capture_targets();key('LeftMouseButton',True);s.update(phase='spin',next=now+1.2)
    elif s['phase']=='spin':
        measure()
        if now<s['next']:return
        report['samples'].append({'phase':'spin','angles':s['angles'],'running':s['e'].get_editor_property('bDrillRunning'),
            'drill_mode':s['pawn'].get_component_by_class(unreal.MiningComponent).get_editor_property('bDrillMode')})
        check(s['e'].get_editor_property('bDrillRunning') and len(set(round(a) for a in s['angles']))>8,'Authored drill curve produces actual continuous spin')
        check(report['max_shaft_drift_cm']<.3,'Both offset shafts stay fixed apart from intended tiny vibration')
        check(h.capture_preview(s['pc'],str(out/'correct-shaft.png')),'Captured corrected drill pose')
        model=s['e'].drill_mesh
        model.set_editor_property('relative_rotation',unreal.Rotator(pitch=-23,yaw=16,roll=14))
        model.set_editor_property('relative_location',unreal.Vector(-17,8,-19))
        capture_targets();s.update(phase='tilted',next=now+1.2)
    elif s['phase']=='tilted':
        measure()
        if now<s['next']:return
        check(len(set(round(a) for a in s['angles']))>8,'Edited model continues spinning')
        check(report['max_shaft_drift_cm']<.3,'Changing Model position and orientation in PIE preserves its own shaft axis')
        key('LeftMouseButton',False);s.update(phase='settle-stop')
    elif s['phase']=='settle-stop' and not s['e'].get_editor_property('bDrillRunning'):
        s.update(phase='stopped',next=now+.12,stopped_angle=s['e'].drill_spin_degrees)
    elif s['phase']=='stopped' and now>=s['next']:
        check(not s['e'].get_editor_property('bDrillRunning') and abs(s['e'].drill_spin_degrees-s['stopped_angle'])<.01,'Release stops the animation normally')
        check(s['pc'].mining_gui.jetpack_heat_bar is not None,'Jetpack UI remains mounted')
        finish(True)
def guard(dt):
    global busy
    if busy:return
    busy=True
    try:tick(dt)
    except Exception:finish(False,traceback.format_exc())
    finally:busy=False
cb=unreal.register_slate_post_tick_callback(guard)
h.start_floating_preview()
