"""Render the production material HUD in an unsaved lightweight PIE fixture."""
import json
import time
import traceback
from pathlib import Path
import unreal

OUT=Path(unreal.Paths.project_dir()).resolve()/'Saved/Validation/MaterialProgression'
ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
helper=unreal.MiningGUIEditorLibrary
state={'phase':'wait','start':time.monotonic(),'next':0,'busy':False}
report={'passed':False,'fixture':'unsaved lightweight scene; production HUD and controller','checks':[]}
def check(ok,label):
    assert ok,label
    report['checks'].append(label)
def shot(name):
    check(helper.capture_preview(state['pc'],str(OUT/(name+'.png'))),'Captured '+name)
def press():
    helper.send_preview_key(state['pc'],'Tab',True)
    helper.send_preview_key(state['pc'],'Tab',False)
def finish(error=None):
    report.update(passed=error is None,error=error,elapsed=time.monotonic()-state['start'])
    (OUT/'hud.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    levels.editor_request_end_play()
    state.update(phase='done',next=time.monotonic()+2)
def tick(dt):
    if state['busy'] or time.monotonic()<state['next']:return
    state['busy']=True
    try:
        now=time.monotonic()
        if state['phase']=='done':
            if not ed.get_game_world():
                unreal.unregister_slate_post_tick_callback(callback)
                unreal.SystemLibrary.execute_console_command(ed.get_editor_world(),'QUIT_EDITOR')
            return
        assert now-state['start']<120,'HUD fixture timed out'
        world=ed.get_game_world()
        if not world:return
        if state['phase']=='wait':
            pc=unreal.GameplayStatics.get_player_controller(world,0)
            pawn=unreal.GameplayStatics.get_player_pawn(world,0)
            if not pc or not pawn or not pc.mining_gui:return
            if not state.get('hall_created'):
                progress=pawn.get_component_by_class(unreal.MiningProgressionComponent)
                progress.get_wallet().set_editor_property('use_mineral_inventory',True)
                hall=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.MiningExhibitHall)[0]
                area=unreal.MiningExperienceArea()
                area.set_editor_property('title','HUD 预览')
                area.set_editor_property('kind',unreal.MiningExperienceKind.MINING)
                area.set_editor_property('entry',unreal.Transform(location=unreal.Vector(0,0,200)))
                hall.set_editor_property('areas',[area])
                state['hall_created']=True
                return
            if not pc.mining_gui.get_editor_property('bMineralProgression'):return
            pawn.get_component_by_class(unreal.CharacterMovementComponent).set_movement_mode(unreal.MovementMode.MOVE_FLYING)
            progress=pawn.get_component_by_class(unreal.MiningProgressionComponent)
            state.update(pc=pc,gui=pc.mining_gui,progress=progress,wallet=progress.get_wallet(),phase='empty',next=now+2)
        elif state['phase']=='empty':
            shot('hud_01_empty')
            for mineral,count in [('2',28),('4',20),('6',6),('9',2)]:state['wallet'].add_mineral(mineral,count)
            state.update(phase='ready',next=now+.5)
        elif state['phase']=='ready':
            shot('hud_02_ready')
            # Route through Slate/game viewport just like a physical keyboard event.
            helper.send_preview_focused_key(state['pc'],'Tab',True)
            state.update(phase='focused',next=now+.3)
        elif state['phase']=='focused':
            helper.send_preview_focused_key(state['pc'],'Tab',False)
            check(state['progress'].get_level(0)==1,'Focused Slate Tab input buys exactly one level')
            press();press()
            state.update(phase='four_materials',next=now+.8)
        elif state['phase']=='four_materials':
            check(state['progress'].get_level(0)==3,'Level three displays four material requirements')
            shot('hud_03_four_materials');press()
            state.update(phase='max',next=now+.8)
        elif state['phase']=='max':
            check(state['progress'].get_level(0)==4,'Rendered HUD reaches maximum level')
            shot('hud_04_max');finish()
    except Exception:finish(traceback.format_exc())
    finally:state['busy']=False

world=unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
world.get_world_settings().set_editor_property('default_game_mode',unreal.MiningExhibitGameMode)
collection=actors.spawn_actor_from_class(unreal.MiningCollectionField,unreal.Vector())
collection.set_editor_property('area_managed',True)
collection.set_editor_property('use_authored_placements',True)
collection.set_editor_property('show_exploration_hud',False)
hall=actors.spawn_actor_from_class(unreal.MiningExhibitHall,unreal.Vector())
hall.set_editor_property('linear_campaign',True)
# Leave areas empty until the PIE callback has selected the material economy.
unreal.get_default_object(unreal.load_class(None,'/Script/UnrealEd.EditorPerformanceSettings')).set_editor_property('bThrottleCPUWhenNotForeground',False)
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
callback=unreal.register_slate_post_tick_callback(tick)
helper.start_floating_preview()
