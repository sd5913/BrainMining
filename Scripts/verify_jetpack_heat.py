"""PIE check using Space input, gameplay time, real HUD and captured game frames."""
import json,time,traceback
from pathlib import Path
import unreal

editor=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
helper=unreal.MiningGUIEditorLibrary
perf=unreal.get_default_object(unreal.load_class(None,'/Script/UnrealEd.EditorPerformanceSettings'))
old_throttle=perf.get_editor_property('bThrottleCPUWhenNotForeground')
perf.set_editor_property('bThrottleCPUWhenNotForeground',False)
out=Path(unreal.Paths.project_saved_dir())/'Validation/JetpackHeat';out.mkdir(parents=True,exist_ok=True)
report={'passed':False,'checks':[],'samples':{}}
state={'phase':'setup','started':time.monotonic()}
def check(condition,label):
    assert condition,label
    report['checks'].append(label)
def key(name,pressed):helper.send_preview_key(state['pc'],name,pressed)
def sample(name):
    m=state['movement'];ui=state['ui']
    report['samples'][name]={'heat':m.jetpack_heat,'lock':m.jetpack_overheat_remaining,
        'held':m.flight_input_held,'thrust':m.is_flight_thrust_active(),'velocity_z':m.velocity.z,
        'height':state['pawn'].get_actor_location().z,'ui_heat':ui.displayed_heat,
        'ui_locked':ui.get_editor_property('bDisplayedOverheated')}
def shot(name):check(helper.capture_preview(state['pc'],str(out/(name+'.png'))),'Captured '+name)
def finish(passed,reason=''):
    unreal.unregister_slate_post_tick_callback(callback)
    if editor.get_game_world() and 'pc' in state:key('SpaceBar',False)
    perf.set_editor_property('bThrottleCPUWhenNotForeground',old_throttle)
    report.update(passed=passed,reason=reason,elapsed=time.monotonic()-state['started'])
    (out/'runtime.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    unreal.log('JETPACK_RUNTIME_RESULT '+json.dumps(report))
    if editor.get_game_world():levels.editor_request_end_play()
def tick(delta):
    if state.get('busy'):return
    state['busy']=True
    try:
        if time.monotonic()-state['started']>120:raise RuntimeError('Jetpack runtime timeout')
        if state['phase']=='setup':
            world=editor.get_game_world()
            if not world:return
            pc=unreal.GameplayStatics.get_player_controller(world,0)
            pawn=unreal.GameplayStatics.get_player_character(world,0)
            if not pc or not pawn or not pc.mining_gui:return
            if 'ready' not in state:state['ready']=time.monotonic();return
            if time.monotonic()-state['ready']<3:return
            m=pawn.get_component_by_class(unreal.MiningCharacterMovementComponent)
            gui=pc.mining_gui;ui=gui.jetpack_heat_bar
            state.update(world=world,pc=pc,pawn=pawn,movement=m,gui=gui,ui=ui)
            check(m.flight_settings.enabled,'Actual level character has existing flight enabled')
            check(ui and gui.is_in_viewport(),'Editable jetpack child is mounted in the real HUD')
            check(ui.displayed_heat==0 and not ui.get_editor_property('bDisplayedOverheated'),'Idle heat starts empty and white')
            check(not gui.blast_upgrade.coin_button.get_is_enabled(),'Locked upgrade remains disabled')
            check(not pc.show_mouse_cursor and not pc.is_look_input_ignored(),'Normal game keeps mouse look and hidden cursor')
            mat=ui.heat_bar.get_dynamic_material()
            check(mat is not None,'Heat bar uses the editable UI dynamic material')
            sample('idle');shot('idle-white')
            state.update(phase='heat',start=unreal.GameplayStatics.get_time_seconds(world),spawn_z=pawn.get_actor_location().z)
            key('SpaceBar',True);return
        now=unreal.GameplayStatics.get_time_seconds(state['world'])
        m=state['movement'];ui=state['ui']
        if state['phase']=='heat' and now-state['start']>=2.5:
            sample('partial')
            check(m.flight_input_held and m.is_flight_thrust_active(),'Space input activates existing hold-to-fly')
            check(.45<m.jetpack_heat<.65,'Heat accumulates over sustained flight')
            check(abs(ui.displayed_heat-m.jetpack_heat)<.03,'UI fill follows actual gameplay heat')
            check(state['pawn'].get_actor_location().z>state['spawn_z']+30,'Character actually rises')
            shot('partial-mask');state['phase']='wait-lock'
        elif state['phase']=='wait-lock' and m.is_jetpack_overheated():
            if not ui.get_editor_property('bDisplayedOverheated'):return
            sample('overheated')
            check(m.jetpack_heat>.97 and ui.displayed_heat>.97,'Full heat fills the bar before cooling starts')
            check(not m.is_flight_thrust_active(),'Overheat blocks thrust while Space remains held')
            check(m.velocity.z<=0,'Overheat removes upward drift')
            state.update(phase='locked',lock_start=now,lock_remaining=m.jetpack_overheat_remaining)
            shot('overheated-red');key('SpaceBar',False);key('SpaceBar',True)
        elif state['phase']=='locked' and now-state['lock_start']>=1.0:
            sample('locked-after-repress')
            check(m.is_jetpack_overheated() and not m.is_flight_thrust_active(),'Repressing Space cannot bypass lockout')
            check(ui.get_editor_property('bDisplayedOverheated') and .55<ui.displayed_heat<.75,'Red locked bar retreats to about two thirds')
            check(abs(ui.displayed_heat-m.jetpack_heat)<.03,'Cooldown UI follows actual remaining heat')
            check(abs(m.jetpack_overheat_remaining-(state['lock_remaining']-(now-state['lock_start'])))<.05,'Three-second lock uses gameplay time')
            shot('cooldown-retreat-two-thirds');state['phase']='late-lock'
        elif state['phase']=='late-lock' and now-state['lock_start']>=2.0:
            sample('locked-near-end')
            check(m.is_jetpack_overheated() and not m.is_flight_thrust_active(),'Flight stays blocked while the last red segments retreat')
            check(ui.get_editor_property('bDisplayedOverheated') and .2<ui.displayed_heat<.4,'Locked bar retreats to about one third')
            check(ui.displayed_heat<report['samples']['locked-after-repress']['ui_heat'],'Red fill shrinks monotonically during cooldown')
            shot('cooldown-retreat-one-third');state['phase']='recover'
        elif state['phase']=='recover' and not m.is_jetpack_overheated():
            if ui.get_editor_property('bDisplayedOverheated'):return
            sample('recovered')
            check(abs((now-state['lock_start'])-state['lock_remaining'])<.15,'Flight re-enables after the three-second lock')
            check(m.is_flight_thrust_active() and m.jetpack_heat<.05,'Held Space resumes with a fresh heat budget')
            state.update(phase='release',release_at=now+.6)
        elif state['phase']=='release' and now>=state['release_at']:
            key('SpaceBar',False);state.update(phase='cool',release_heat=m.jetpack_heat,released_at=now)
        elif state['phase']=='cool' and now-state['released_at']>.5:
            sample('released-cool')
            check(not m.flight_input_held and not m.is_flight_thrust_active(),'Release stops flight normally')
            check(m.jetpack_heat<state['release_heat'],'Released jetpack cools gradually')
            check(m.velocity.z<=0,'Release returns to falling or landing')
            key('Tab',True);state.update(phase='menu',menu_at=now)
        elif state['phase']=='menu' and now-state['menu_at']>.4:
            check(state['pc'].show_mouse_cursor and state['pc'].is_look_input_ignored(),'Existing Tab upgrade interaction still works')
            check(ui.displayed_heat==0,'Bar returns to empty white after cooling')
            key('Tab',False);shot('recovered-white');finish(True)
    except Exception:
        finish(False,traceback.format_exc())
    finally:
        state['busy']=False
callback=unreal.register_slate_post_tick_callback(tick)
helper.start_floating_preview()
