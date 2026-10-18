"""Run the real campaign drill input/render regression and close cleanly."""
import unreal,time
from pathlib import Path
root=Path(unreal.Paths.project_dir()).resolve()
scope={'__name__':'__main__'}
exec(compile((root/'Scripts/verify_main_game_drill.py').read_text(encoding='utf-8'),'verify_main_game_drill.py','exec'),scope)
cleanup={}
def close(dt):
    if scope['s']['phase']!='done' or scope['ed'].get_game_world():return
    if 'at' not in cleanup:cleanup['at']=time.monotonic()+3
    if time.monotonic()<cleanup['at']:return
    unreal.unregister_slate_post_tick_callback(cleanup['handle'])
    unreal.SystemLibrary.quit_editor()
cleanup['handle']=unreal.register_slate_post_tick_callback(close)
