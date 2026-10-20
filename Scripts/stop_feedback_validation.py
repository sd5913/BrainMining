"""Retire only callbacks owned by this task's interrupted validation scripts."""
import gc,types,unreal
names={'verify_traversal_feedback.py','verify_opening_subtitle_feedback.py','verify_feather_fade.py','verify_vessel_lighting_feedback.py','verify_ending_video_stability.py'}
for fn in gc.get_objects():
    if isinstance(fn,types.FunctionType) and fn.__name__=='tick' and fn.__code__.co_filename.replace('\\','/').rsplit('/',1)[-1] in names:
        ctx=fn.__globals__;handle=ctx.get('handle')
        if handle:
            try:unreal.unregister_slate_post_tick_callback(handle)
            except Exception:pass
            unreal.log('FEEDBACK_CALLBACK_RETIRED '+fn.__code__.co_filename)
unreal.EditorPythonScripting.set_keep_python_script_alive(False)
