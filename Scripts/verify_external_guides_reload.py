"""Fresh-process verification of the saved guide actors, without regenerating them."""
import runpy
from pathlib import Path
import unreal
runpy.run_path(str(Path(unreal.Paths.project_dir())/'Scripts/verify_external_guides.py'),
              init_globals={'GENERATE_GUIDES':False,'GUIDE_REPORT_NAME':'external_guides_reload.json'})
