"""Repair only the newly imported guide assets by retaining all FBX triangles."""
import runpy
from pathlib import Path
import unreal
helpers=runpy.run_path(str(Path(unreal.Paths.project_dir())/'Scripts/external_guide_helpers.py'))
helpers['models'](reimport=True)
runpy.run_path(str(Path(unreal.Paths.project_dir())/'Scripts/repair_external_guide_import.py'))
