"""Validate the live cinematic-camera blendable and compare the same frozen view."""
from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir())
code=(root/'Scripts/verify_vessel_lighting_feedback.py').read_text(encoding='utf8')
code=code.replace("'Saved/Validation/TraversalFeedback/Blood'","'Saved/Validation/VesselPaint/Preview'")
code=code.replace('c.intensity==18000','c.intensity==900')
start=code.index("            capture('vessel_lit')")
end=code.index("    except Exception:",start)
code=code[:start]+"""            camera=s['camera'].get_component_by_class(unreal.CameraComponent)
            paint=H.read_preview_object(d,'PainterlyMaterial');s.update(paint=paint,camera_component=camera)
            weight=d.get_editor_property('active_paint_weight');r['blood_paint_weight']=weight;s['paint_weight']=weight
            check(abs(weight-.7)<.01,'Blood uses the restored 70 percent oil-paint weight')
            blends=camera.get_editor_property('post_process_settings').weighted_blendables.array
            check(any(b.object==paint and abs(b.weight-weight)<.01 for b in blends),'Oil shader is attached to the actual blood-flow camera')
            camera.remove_blendable(paint)
            s.update(phase='unpainted',next=now+1);return
        if p=='unpainted':
            capture('01_without_oil')
            s['camera_component'].add_or_update_blendable(s['paint'],s['paint_weight'])
            s.update(phase='painted',next=now+3);return
        if p=='painted':
            capture('02_with_oil')
            d=s['d']
            check(H.write_preview_state(d,'VesselDistance','999999'),'Reach the actual vessel exit')
            d.set_actor_tick_enabled(True);s.update(phase='leaving',next=now+1.8);return
        if p=='leaving':
            d=s['d'];d.set_actor_tick_enabled(False)
            check(d.get_editor_property('active_paint_weight')==0,'Brain entry removes the oil-paint stage weight')
            blends=s['camera_component'].get_editor_property('post_process_settings').weighted_blendables.array
            check(not any(b.object==s['paint'] for b in blends),'Brain entry removes the camera blendable')
            finish();return
"""+code[end:]
code=code.replace("unreal.EditorPythonScripting.set_keep_python_script_alive(False)","unreal.EditorPythonScripting.set_keep_python_script_alive(False);unreal.SystemLibrary.quit_editor()")
exec(compile(code,'verify_vessel_paint','exec'))
