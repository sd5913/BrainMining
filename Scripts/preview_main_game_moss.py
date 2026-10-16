"""Capture the same guide face and clay relief before/after a material edit."""
import unreal,json,time,traceback,sys
from pathlib import Path
ROOT=Path(unreal.Paths.project_dir()).resolve();sys.path.insert(0,str(ROOT/'Scripts'))
fixture=(ROOT/'Scripts/verify_main_game_dirt_transition.py').read_text(encoding='utf-8').split('\ndef tick(dt):')[0]
exec(compile(fixture,'moss_preview_fixture','exec'),globals())
BASE=ROOT/'Saved/Validation/MainGameMoss';version='after' if json.loads((BASE/'authoring.json').read_text()).get('passed') else 'before'
OUT=BASE/version;OUT.mkdir(parents=True,exist_ok=True)
report={'passed':False,'checks':[],'shots':[]};state.update(index=0)
bake=json.loads((ROOT/'Saved/Validation/MainGameRoutesV2/Clay/bake.json').read_text())
def tick(dt):
    if state['busy'] or state['phase']=='done':return
    state['busy']=True
    try:
        now=time.monotonic()
        assert now-state['start']<150,'Moss preview timed out'
        if now<state['next']:return
        world=editor.get_game_world()
        if not world:return
        phase=state['phase']
        if phase=='setup':
            pawn=unreal.GameplayStatics.get_player_pawn(world,0);pc=unreal.GameplayStatics.get_player_controller(world,0)
            halls=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.MiningExhibitHall)
            if not pawn or not pc.mining_gui or not halls or not halls[0].get_editor_property('bReady'):return
            state.update(world=world,pawn=pawn,pc=pc,hall=halls[0],movement=pawn.get_component_by_class(unreal.MiningCharacterMovementComponent),regions=list(unreal.GameplayStatics.get_all_actors_of_class(world,unreal.MiningExternalRegion)),phase='place')
        elif phase=='place':
            i=state['index'];n=i//2;stage,tag=STAGES[n];r=next(r for r in state['regions'] if r.actor_has_tag(tag))
            state['hall'].enter_area(n)
            if i%2==0:
                paths=json.loads((ROUTES/stage/'centerlines.json').read_text());rec=json.loads((ROUTES/stage/'placement_plan.json').read_text())['objects'][0]
                origin=r.get_actor_location()+r.cutters[0].local_transform.translation
                eye=origin+point(paths[rec['path_index']],rec['start_arc_m']-2.6);target=origin+point(paths[rec['path_index']],rec['start_arc_m']+1)
                name=stage+'_plug'
            else:
                rec=next(s for s in bake['samples'] if s['kind']==tag and abs(s['normal'][2])<.4 and s.get('previous_depth_cm',s['depth_cm'])>10)
                target=unreal.Vector(*rec['position']);normal=unreal.Vector(*rec['normal']);side=unreal.Vector(-normal.y,normal.x,0)
                eye=target+normal*185+side*65;name=stage+'_relief'
            move(eye-unreal.Vector(0,0,64));state['movement'].set_movement_mode(unreal.MovementMode.MOVE_FLYING);look(target)
            state.update(name=name,phase='photo',next=now+.9)
        elif phase=='photo':
            shot(state['name']);report['shots'].append(str(OUT/(state['name']+'.png')))
            state['index']+=1
            if state['index']==8:finish()
            else:state.update(phase='place',next=now+.1)
    except Exception:finish(traceback.format_exc())
    finally:state['busy']=False
assert not editor.get_game_world();assert levels.load_level('/Game/MainGame/Maps/Lvl_MainGame')
unreal.EditorPythonScripting.set_keep_python_script_alive(True);callback=unreal.register_slate_post_tick_callback(tick);helper.start_floating_preview()
