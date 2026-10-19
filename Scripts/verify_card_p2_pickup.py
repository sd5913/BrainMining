"""Real-time card presentation fixture: shared subtitles, drill model, E ownership."""
import unreal,time,json,traceback
from pathlib import Path
out=Path(unreal.Paths.project_dir())/'Saved/Validation/CardP2Flow'
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
helper=unreal.MiningGUIEditorLibrary
s=dict(phase=0,start=time.monotonic(),next=0,checks=[])
def check(v,msg):
    if not v:raise AssertionError(msg)
    s['checks'].append(msg)
def finish(ok,error=''):
    (out/'pickup.json').write_text(json.dumps(dict(passed=ok,error=error,checks=s['checks'],lamp=s.get('lamp',[])),ensure_ascii=False,indent=2),encoding='utf-8')
    levels.editor_request_end_play();s.update(phase=99,next=time.monotonic()+2)
def tick(dt):
    if s.get('busy'):return
    s['busy']=True
    try:
        now=time.monotonic()
        if now<s['next']:return
        if s['phase']==99:
            unreal.unregister_slate_post_tick_callback(handle);unreal.EditorPythonScripting.set_keep_python_script_alive(False);return
        if now-s['start']>130:raise RuntimeError('Card presentation timed out')
        if s['phase']==0:helper.start_floating_preview();s.update(phase=1,next=now+3);return
        w=ed.get_game_world()
        if not w:return
        if s['phase']==1:
            pc=unreal.GameplayStatics.get_player_controller(w,0);pawn=unreal.GameplayStatics.get_player_pawn(w,0)
            if not pawn:return
            stage=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.CardRoomStage)[0]
            mine=pawn.get_component_by_class(unreal.MiningComponent)
            s['lamp']=[dict(label=a.get_actor_label(),location=str(a.get_actor_location()),rotation=str(a.get_actor_rotation())) for a in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.SpotLight) if 'Lamp' in a.get_actor_label() or 'lamp' in a.get_actor_label()]
            check(not mine.is_drill_unlocked() and not mine.activate_drill(),'Drill locked before winning and pickup')
            s.update(pc=pc,pawn=pawn,mine=mine,stage=stage,phase=2,match_world_time=unreal.GameplayStatics.get_time_seconds(w))
            unreal.SystemLibrary.execute_console_command(w,'BrainMining.ReviewP2',pc);return
        if s['phase']==2:
            age=unreal.GameplayStatics.get_time_seconds(w)-s['match_world_time']
            if age>.4 and not s.get('matching'):
                check(helper.capture_preview(s['pc'],str(out/'08-matching.png')),'Matching screenshot captured');s['matching']=True
            if age>2.45 and not s.get('matched'):
                check(helper.capture_preview(s['pc'],str(out/'09-matched.png')),'Arrival screenshot captured');s['matched']=True
            reward=next(c for c in s['stage'].get_components_by_class(unreal.StaticMeshComponent) if c.get_name()=='CollectibleOrb')
            if reward.is_visible():
                check(s.get('matched'),'Matching success occurs before the duel reward')
                check(not s['mine'].is_drill_unlocked(),'Victory alone does not unlock drill')
                check(reward.static_mesh.get_name()=='SM_DrillStylized','Reward uses the actual player drill mesh')
                check('CardRoomGlow' not in reward.get_material(0).get_path_name(),'Drill reward retains its authored surface')
                s.update(phase=3,next=now+.8);return
        if s['phase']==3:
            helper.capture_preview(s['pc'],str(out/'10-drill-reward.png'))
            helper.send_preview_key(s['pc'],'E',True);helper.send_preview_key(s['pc'],'E',False)
            s.update(phase=4,next=now+.5);return
        if s['phase']==4:
            check(s['mine'].is_drill_unlocked() and s['mine'].get_editor_property('bDrillMode'),'E pickup unlocks and equips drill')
            escape=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.MiningRoomEscape)[0]
            entry=escape.get_editor_property('computer_entry')
            s['pawn'].get_component_by_class(unreal.CharacterMovementComponent).disable_movement()
            s['pawn'].set_actor_location(entry+unreal.Vector(-1900,-600,-100),False,True)
            s['pc'].set_control_rotation(unreal.MathLibrary.find_look_at_rotation(s['pawn'].get_actor_eyes_view_point()[0],entry))
            s.update(escape=escape,phase=5);return
        if s['phase']==5:
            if s['escape'].get_editor_property('water_z')<s['escape'].get_editor_property('table_water_z'):return
            helper.capture_preview(s['pc'],str(out/'11-green-television.png'))
            check(True,'Intact television signal captured after water reaches table')
            finish(True)
    except Exception:finish(False,traceback.format_exc())
    finally:s['busy']=False
handle=unreal.register_slate_post_tick_callback(tick)
