"""PIE verification using actual V/left-click input and upgrade purchases."""
import unreal,time,json,traceback
from pathlib import Path
out=Path(unreal.Paths.project_saved_dir())/'Validation/DrillEnergy';out.mkdir(parents=True,exist_ok=True)
state={'phase':0,'next':time.monotonic()+3,'start':time.monotonic()};checks=[]
helper=unreal.MiningGUIEditorLibrary
def check(value,label):
    assert value,label
    checks.append(label)
def key(name,down):helper.send_preview_key(state['pc'],name,down)
def finish(ok,error=''):
    unreal.unregister_slate_post_tick_callback(energy_validation_handle)
    (out/'runtime.json').write_text(json.dumps({'passed':ok,'checks':checks,'error':error},ensure_ascii=False,indent=2),encoding='utf8')
    print('DRILL_ENERGY_RUNTIME',ok,len(checks),error)
    unreal.EditorLevelLibrary.editor_end_play()
def energy_tick(dt):
    try:
        now=time.monotonic()
        if state.get('gui'):
            alpha=state['gui'].drill_energy_bar.get_render_opacity()
            if state['phase']==.5 and 0<alpha<1:state['fade_in_sample']=True
            if state['phase']==4 and 0<alpha<1:state['fade_out_sample']=True
        if now-state['start']>90:raise RuntimeError('Energy preview timed out')
        if now<state['next']:return
        worlds=unreal.EditorLevelLibrary.get_pie_worlds(False)
        if not worlds:return
        world=worlds[0];pc=unreal.GameplayStatics.get_player_controller(world,0);pawn=unreal.GameplayStatics.get_player_pawn(world,0)
        if not pawn:return
        phase=state['phase']
        if phase==0:
            m=pawn.get_component_by_class(unreal.MiningComponent);gui=pc.get_editor_property('MiningGUI')
            check(m and gui and gui.drill_energy_bar,'Live HUD has the energy bar')
            check(m.get_editor_property('bUseDrillEnergy'),'Character uses energy mode')
            pawn.get_component_by_class(unreal.CharacterMovementComponent).set_movement_mode(unreal.MovementMode.MOVE_NONE)
            state.update(pc=pc,m=m,gui=gui,pawn=pawn,world=world)
            check(gui.drill_energy_bar.get_render_opacity()==0 and gui.drill_energy_bar.get_visibility()==unreal.SlateVisibility.HIDDEN,'Idle energy bar starts hidden')
            key('V',True);key('V',False)
            state.update(phase=.5,next=now+.4)
        elif phase==.5:
            m=state['m']
            check(m.get_editor_property('bDrillMode'),'V selects the drill')
            check(not m.is_mining_active(),'Selection does not start drilling')
            check(state.get('fade_in_sample') and state['gui'].drill_energy_bar.get_render_opacity()>.99,'Equipping fades the bar in through intermediate opacity')
            state.update(phase=1,next=now+11)
        elif phase==1:
            m=state['m'];check(m.get_editor_property('bDrillMode') and m.drill_energy==m.max_drill_energy,'Equipped idle drill stays full beyond ten seconds')
            check(m.get_drill_use_remaining()==0 and m.get_drill_cooldown_remaining()==0,'Both old countdowns are removed')
            key('LeftMouseButton',True);state.update(phase=2,next=now+3,before=m.drill_energy)
        elif phase==2:
            m=state['m'];check(m.drill_energy<state['before']-10,'Actual left-click drilling consumes energy')
            helper.capture_preview(pc,str(out/'drill_energy.png'))
            key('LeftMouseButton',False);state.update(phase=3,next=now+1.2,before=m.drill_energy)
        elif phase==3:
            m=state['m'];check(m.get_editor_property('bDrillMode') and m.drill_energy>state['before']+3,'Release recharges while still holding the drill')
            m.drill_consumption_rate=1000.;key('LeftMouseButton',True)
            state.update(phase=4,next=now+.7)
        elif phase==4:
            m=state['m'];key('LeftMouseButton',False)
            check(not m.get_editor_property('bDrillMode'),'Exhaustion stows the drill')
            check(state.get('fade_out_sample') and state['gui'].drill_energy_bar.get_render_opacity()==0,'Stowing fades the bar out through intermediate opacity')
            m.drill_consumption_rate=10.
            state.update(phase=5,next=now+1)
        elif phase==5:
            m=state['m'];key('V',True);key('V',False)
            check(state['gui'].drill_energy_bar.get_visibility()==unreal.SlateVisibility.HIDDEN and m.drill_energy>0,'Energy recharges while its bar is hidden')
            state.update(phase=5.5,next=now+.4)
        elif phase==5.5:
            m=state['m']
            check(m.get_editor_property('bDrillMode'),'Recovered energy can equip immediately')
            progression=pawn.get_component_by_class(unreal.MiningProgressionComponent)
            check(progression and progression.initialize_progression(),'Progression initializes')
            wallet=progression.get_wallet();wallet.add_coins(4000000)
            expected=[(20000,15.,100.,10.),(100000,15.,150.,10.),(500000,15.,150.,7.5),(2000000,15.,150.,7.5)]
            for level,(price,recovery,capacity,drain) in enumerate(expected,1):
                check(progression.get_next_price(3)==price,'Upgrade '+str(level)+' price matches plan')
                check(progression.purchase(3,level),'Upgrade '+str(level)+' purchases through gameplay')
                check(abs(m.drill_recovery_rate-recovery)<.01 and abs(m.max_drill_energy-capacity)<.01 and abs(m.drill_consumption_rate-drain)<.01,'Upgrade '+str(level)+' resource values match plan')
            check(m.get_editor_property('bInfiniteDrillEnergy'),'Level four has infinite energy')
            key('LeftMouseButton',True);state.update(phase=6,next=now+1)
        elif phase==6:
            key('LeftMouseButton',False)
            check(state['m'].drill_energy==state['m'].max_drill_energy,'Infinite energy stays full during drilling')
            pc.set_upgrade_menu_held(True);state.update(phase=7,next=now+.5)
        elif phase==7:
            check(state['gui'].drill_energy_bar.get_visibility()==unreal.SlateVisibility.HIDDEN,'Energy bar hides behind menus')
            check(pc.show_mouse_cursor,'Upgrade menu exposes game cursor')
            helper.capture_preview(pc,str(out/'upgrade_cursor.png'))
            pc.set_upgrade_menu_held(False);finish(True)
    except Exception:
        finish(False,traceback.format_exc())
unreal.EditorLoadingAndSavingUtils.load_map('/Game/MiningPrototype/Maps/Lvl_MiningLab')
helper.start_floating_preview()
energy_validation_handle=unreal.register_slate_post_tick_callback(energy_tick)
