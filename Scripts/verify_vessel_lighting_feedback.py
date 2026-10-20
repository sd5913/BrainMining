"""Exercise actual membrane exit, then compare local vessel lighting on/off."""
import unreal,time,json,traceback
from pathlib import Path
out=Path(unreal.Paths.project_dir())/'Saved/Validation/TraversalFeedback/Blood';out.mkdir(parents=True,exist_ok=True)
E=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem);L=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);H=unreal.MiningGUIEditorLibrary
s={'phase':'start','start':time.monotonic(),'next':0,'busy':False};r={'passed':False,'checks':[]}
settings=unreal.get_default_object(unreal.load_class(None,'/Script/UnrealEd.EditorPerformanceSettings'))
throttle=settings.get_editor_property('bThrottleCPUWhenNotForeground');settings.set_editor_property('bThrottleCPUWhenNotForeground',False)
def check(ok,name):assert ok,name;r['checks'].append(name)
def capture(name):check(H.capture_preview(s['pc'],str(out/(name+'.png'))),'Capture '+name)
def finish(error=None):
    r.update(passed=not error,error=error);(out/'runtime.json').write_text(json.dumps(r,indent=2),encoding='utf-8')
    if E.get_game_world():L.editor_request_end_play()
    settings.set_editor_property('bThrottleCPUWhenNotForeground',throttle)
    unreal.unregister_slate_post_tick_callback(handle);unreal.EditorPythonScripting.set_keep_python_script_alive(False)
def tick(dt):
    if s['busy'] or time.monotonic()<s['next']:return
    s['busy']=True
    try:
        now=time.monotonic();p=s['phase'];assert now-s['start']<120,'Timeout at '+p
        if p=='start':
            assert L.load_level('/Game/EndingEmergence/Maps/Lvl_EndingEmergence_Demo');H.start_floating_preview();s.update(phase='ready',next=now+8);return
        w=E.get_game_world()
        if not w:return
        if p=='ready':
            directors=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.EndingEmergence)
            if not directors or H.read_preview_object(directors[0],'ToolLight') is None:return
            d=directors[0];pc=unreal.GameplayStatics.get_player_controller(w,0);pawn=pc.get_controlled_pawn();s.update(d=d,pc=pc,pawn=pawn)
            lights=[c for c in d.get_components_by_class(unreal.PointLightComponent) if c.intensity==18000]
            check(len(lights)>=7,'Broad local lamps cover full vessel path');s['lights']=lights
            cells=[c for c in d.get_components_by_class(unreal.StaticMeshComponent) if c.get_name().startswith('BloodCell')]
            check(len(cells)==130,'All moving red blood cells use imported mesh')
            for c in cells:
                check(c.static_mesh.get_name()=='SM_RedBloodCell','Imported red cell '+c.get_name())
                m=c.get_material(0);check(isinstance(m,unreal.MaterialInstanceDynamic),'Lit red-cell material '+c.get_name())
                for name in ['EnissiveColor1','EnissiveColor2']:
                    v=m.get_vector_parameter_value(name);assert max(v.r,v.g,v.b)==0,'Cell still glows: '+name
            r['checks'].append('All red-cell emissive colours are zero; subsurface surface remains lit')
            # Lighting fixture starts the production blood-camera path directly;
            # it does not depend on the unrelated membrane walking lesson.
            check(H.write_preview_state(d,'GateIndex','1'),'Activate vessel lighting fixture')
            pc.set_view_target_with_blend(H.read_preview_object(d,'Camera'),0)
            pawn.get_component_by_class(unreal.CharacterMovementComponent).disable_movement();pawn.set_actor_hidden_in_game(True)
            s.update(phase='blood',next=now+.5);return
        if p=='blood':
            check(s['d'].gate_index==1,'Production blood-camera path is active')
            s.update(phase='pose',next=now+6);return
        if p=='pose':
            d=s['d'];d.set_actor_tick_enabled(False);s['camera']=H.read_preview_object(d,'Camera')
            capture('vessel_lit')
            for light in s['lights']:light.set_visibility(False)
            s.update(phase='dark',next=now+.6);return
        if p=='dark':
            capture('vessel_lamps_disabled')
            for light in s['lights']:light.set_visibility(True)
            s.update(phase='restored',next=now+.6);return
        if p=='restored':capture('vessel_lamps_restored');finish();return
    except Exception:finish(traceback.format_exc())
    finally:s['busy']=False
handle=unreal.register_slate_post_tick_callback(tick);unreal.EditorPythonScripting.set_keep_python_script_alive(True)
