import runpy
from pathlib import Path
import unreal
runpy.run_path(str(Path(unreal.Paths.project_dir())/'Scripts/fill50_guides.py'))['verify'](reload=True)
