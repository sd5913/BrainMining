"""Exercise production pickup, subtitle asset lookup, queuing, and generated voice.

Uses an unsaved PIE fixture; returns to MainGame without saving either level.
"""
import json, time, traceback
from pathlib import Path
import unreal

root=Path(unreal.Paths.project_dir())
out=root/'Saved/Validation/CollectibleSubtitles'
expected=json.loads((out/'installed.json').read_text(encoding='utf8'))['segments']
ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
assert not ed.get_game_world()
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
original=ed.get_editor_world().get_path_name().split('.')[0]
report={'passed':False,'checks':[]}
state={'phase':'wait','start':time.monotonic(),'next':0,'index':0}

def check(ok,label):
    assert ok,label
    report['checks'].append(label)

def pose(index):
    item=state['items'][index]
    position=item.get_actor_location()
    movement=state['pawn'].get_component_by_class(unreal.CharacterMovementComponent)
    movement.stop_movement_immediately()
    movement.set_component_tick_enabled(False)
    state['pawn'].set_actor_location(position+unreal.Vector(-260,0,10),False,True)
    state['pc'].set_control_rotation(unreal.MathLibrary.find_look_at_rotation(state['pawn'].get_actor_eyes_view_point()[0],position))
    state.update(phase='pickup',next=time.monotonic()+.75,index=index)

def active():
    for component in unreal.ObjectIterator(unreal.AudioComponent):
        sound=component.get_editor_property('sound')
        if sound and sound.get_outer()==state['subtitles'] and component.is_playing():return sound
    return None

def finish(error=None):
    if error and 'pawn' in state:
        report['debug']={'pawn':str(state['pawn'].get_actor_location()),'eyes':str(state['pawn'].get_actor_eyes_view_point()),'rotation':str(state['pc'].get_control_rotation()),'target':str(state['field'].target),'item':str(state['items'][state['index']].get_actor_location()),'released':state['items'][state['index']].released}
        unreal.MiningGUIEditorLibrary.capture_preview(state['pc'],str(out/'failure.png'))
    report.update(passed=error is None,error=error)
    (out/'runtime.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
    if 'subtitles' in state:state['subtitles'].stop_all()
    levels.editor_request_end_play()
    state.update(phase='cleanup',next=time.monotonic()+1)

def tick(dt):
    now=time.monotonic()
    if now<state['next']:return
    if state['phase']=='cleanup':
        if ed.get_game_world():return
        unreal.unregister_slate_post_tick_callback(handle)
        levels.load_level(original)
        return
    try:
        assert now-state['start']<120,'Pickup subtitle fixture timed out'
        world=ed.get_game_world()
        if not world:return
        if state['phase']=='wait':
            pawn=unreal.GameplayStatics.get_player_pawn(world,0)
            pc=unreal.GameplayStatics.get_player_controller(world,0)
            fields=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.MiningCollectionField)
            if not pawn or not pc or not fields or len(fields[0].items)<12:return
            pawn.get_component_by_class(unreal.CharacterMovementComponent).set_movement_mode(unreal.MovementMode.MOVE_FLYING)
            gi=unreal.GameplayStatics.get_game_instance(world)
            subs=next(s for s in unreal.ObjectIterator(unreal.SubtitleSubsystem) if s.get_outer()==gi)
            state.update(pawn=pawn,pc=pc,field=fields[0],items=sorted(fields[0].items,key=lambda a:a.item_id),subtitles=subs)
            pose(0)
        elif state['phase']=='pickup':
            index=state['index']; field=state['field']; item=state['items'][index]
            if index<11:state['subtitles'].stop_all()
            field.collect_target()
            check(item.collected and field.inventory[index]==1,'Successful pickup enters shared inventory '+str(index))
            check(active() is not None,'Pickup starts procedural subtitle speech '+str(index))
            if index<11:
                segment=unreal.load_asset(expected[index]['asset'])
                check(str(segment.segment_id)==expected[index]['id'],'Pickup segment uses the matching item ID '+str(index))
                check(str(segment.lines[0].text)==expected[index]['text'],'Subtitle matches authored text '+str(index))
                check(segment.lines[0].audio is None,'Pickup has no audio asset '+str(index))
                check(any(w.get_owning_player()==state['pc'] and w.is_in_viewport() and w.is_visible() for w in unreal.ObjectIterator(unreal.SubtitleWidget)), 'Common subtitle widget is visible '+str(index))
            if index==2:
                unreal.MiningGUIEditorLibrary.capture_preview(state['pc'],str(out/'reagent_pickup.png'))
            if index==11:
                check(active()==state['previous_voice'],'Quick second pickup preserves the first subtitle')
                state.update(phase='queue',next=now+.1)
            else:
                if index<10:
                    state['subtitles'].stop_all(); field.collect_target()
                    check(field.inventory[index]==1 and active() is None,'Failed repeat pickup neither duplicates inventory nor subtitle '+str(index))
                if index==10:state['previous_voice']=active()
                pose(index+1)
        elif state['phase']=='queue':
            if active() is None or active()==state['previous_voice']:return
            check(active().get_outer()==state['subtitles'],'Queued ribbon subtitle plays after the previous pickup')
            check(state['field'].unique_count() == 12 if hasattr(state['field'],'unique_count') else all(v==1 for v in state['field'].inventory),'All twelve types collected exactly once')
            finish()
    except Exception:finish(traceback.format_exc())

world=unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
world.get_world_settings().set_editor_property('default_game_mode',unreal.MiningCollectionGameMode)
field=actors.spawn_actor_from_class(unreal.MiningCollectionField,unreal.Vector())
field.set_editor_property('use_authored_placements',True)
field.set_editor_property('show_exploration_hud',False)
placements=[]
for index in range(12):
    row=unreal.MiningCollectionPlacement()
    row.item_id=index
    row.transform=unreal.Transform(location=unreal.Vector(index*650,0,300))
    placements.append(row)
field.set_editor_property('authored_placements',placements)
handle=unreal.register_slate_post_tick_callback(tick)
unreal.MiningGUIEditorLibrary.start_floating_preview()
