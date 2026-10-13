"""PIE-only repeatable cut/frame profile; never changes saved terrain."""
import unreal,time,json,traceback
from pathlib import Path
root=Path(unreal.Paths.project_dir()); editor=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem); levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
s=dict(start=time.perf_counter(),last=time.perf_counter(),phase=0,cuts=[],frames=[],next=0)
settings=unreal.get_default_object(unreal.load_class(None,"/Script/UnrealEd.EditorPerformanceSettings")); old=settings.get_editor_property("bThrottleCPUWhenNotForeground");settings.set_editor_property("bThrottleCPUWhenNotForeground",False)
def finish():
    unreal.unregister_slate_post_tick_callback(handle);settings.set_editor_property("bThrottleCPUWhenNotForeground",old)
    name="MiningProfileAfter.json" if (root/"Saved/MiningProfileBefore.json").exists() and "error" not in json.loads((root/"Saved/MiningProfileBefore.json").read_text()) else "MiningProfileBefore.json"
    data={k:v for k,v in s.items() if k not in ('placement','pawn','field')}
    frames=sorted(s['frames']);data['summary']=dict(frames=len(frames),p95_ms=frames[int(len(frames)*.95)] if frames else 0,max_ms=max(frames,default=0),over_50ms=sum(v>50 for v in frames))
    (root/"Saved"/name).write_text(json.dumps(data,indent=2,default=str));levels.editor_request_end_play();unreal.log("MINING_PROFILE "+name)
def tick(dt):
    now=time.perf_counter(); gap=(now-s["last"])*1000;s["last"]=now
    try:
        w=editor.get_game_world()
        if not w or now-s["start"]<20:return
        if s["phase"]==0:
            f=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.MiningTreasureField)[0]
            p=f.get_editor_property("authored_placements")[0];s.update(phase=1,next=now+1,field=f)
            s["placement"]=p;s["pawn"]=unreal.GameplayStatics.get_player_character(w,0)
            s["pawn"].get_component_by_class(unreal.CharacterMovementComponent).set_movement_mode(unreal.MovementMode.MOVE_NONE)
            s["pawn"].set_actor_location(p.surface.translation+(unreal.MathLibrary.transform_location(p.surface,unreal.Vector(1,0,0))-p.surface.translation)*200,False,True)
            # Exclude the blocking PIE world-creation frame from steady-state mining statistics.
            s["last"]=now
            return
        s["frames"].append(gap)
        if hasattr(s["field"],"last_terrain_update_milliseconds"):
            s.setdefault("clue_updates",[]).append([s["field"].last_terrain_update_milliseconds,s["field"].last_updated_deposits])
        if now<s["next"]:return
        if len(s["cuts"])>=12:finish();return
        p=s["placement"]; origin=p.surface.translation; n=(unreal.MathLibrary.transform_location(p.surface,unreal.Vector(1,0,0))-p.surface.translation);i=len(s["cuts"])
        start=time.perf_counter(); rows=[]
        for r in p.hosts:
            ok=r.mine_sphere(origin-n*(i*35),100)
            if ok:rows.append(dict(total=r.last_total_milliseconds,boolean=r.last_boolean_milliseconds,collision=r.last_collision_milliseconds))
        s["cuts"].append(dict(wall_ms=(time.perf_counter()-start)*1000,rocks=rows));s["next"]=time.perf_counter()+.5
    except Exception:s["error"]=traceback.format_exc();finish()
handle=unreal.register_slate_post_tick_callback(tick)

