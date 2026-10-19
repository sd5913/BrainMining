"""Update only the ending scale transition material; leave other effects intact."""
from pathlib import Path
import unreal
source = Path(unreal.Paths.project_dir()) / 'Scripts/apply_ending_camera_finish.py'
code = source.read_text(encoding='utf-8-sig')
exec(compile(code.split("post_finish('PulseTransit'", 1)[0], str(source), 'exec'))
unreal.log('ENDING_SCALE_ZOOM_READY')
