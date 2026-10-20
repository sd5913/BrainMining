from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir());code=(root/'Scripts/verify_vessel_lighting_feedback.py').read_text(encoding='utf8')
code=code.replace('Saved/Validation/TraversalFeedback/Blood','Saved/Validation/Playtest16/Ending').replace('c.intensity==18000','c.intensity==900')
a=code.index("            capture('vessel_lit')");b=code.index('    except Exception:',a)
code=code[:a]+'''            camera=s['camera'].get_component_by_class(unreal.CameraComponent);s['camera_component']=camera
            check(abs(d.active_paint_weight-.7)<.01,'Vessel retains the requested oil painting effect')
            capture('01_vessel_painted')
            check(H.write_preview_state(d,'VesselDistance','999999'),'Reach actual vessel exit')
            d.set_actor_tick_enabled(True);s.update(phase='transition',next=now+.35);return
        if p=='transition':
            d=s['d'];r['mid_transition_paint']=d.active_paint_weight
            check(0<d.active_paint_weight<.7,'Oil shader fades through vessel exit')
            pp=s['camera_component'].post_process_settings
            check(pp.vignette_intensity==0,'Exit has no brown vignette')
            capture('02_exit_transition');s.update(phase='neurons',next=now+1.4);return
        if p=='neurons':
            d=s['d'];check(d.gate_index==2,'Continuous camera arrives in neuron stage');check(d.active_paint_weight==0,'Oil blend finishes cleanly')
            capture('03_neurons')
            check(H.prepare_ending_credits_preview(s['pc'],[1]*12),'Open production credits with test inventory')
            s.update(phase='credits',next=now+10);return
        if p=='credits':
            capture('04_credits')
            H.send_preview_key(s['pc'],'Escape',True);H.send_preview_key(s['pc'],'Escape',False)
            s.update(phase='confirm',next=now+.4);return
        if p=='confirm':
            gi=unreal.GameplayStatics.get_game_instance(w)
            session=next(o for o in unreal.ObjectIterator(unreal.MiningExperienceSubsystem) if o.get_outer()==gi)
            check(session.quit_confirmation,'Credits Escape requests confirmation instead of exiting')
            capture('05_quit_confirmation');session.cancel_quit_confirmation();session.toggle_pause_menu()
            check(not session.quit_confirmation and E.get_game_world() is not None,'Cancelling preserves the game')
            finish();return
'''+code[b:]
code=code.replace('unreal.EditorPythonScripting.set_keep_python_script_alive(False)','unreal.EditorPythonScripting.set_keep_python_script_alive(False);unreal.SystemLibrary.quit_editor()')
exec(compile(code,'playtest16_ending_runtime','exec'))
