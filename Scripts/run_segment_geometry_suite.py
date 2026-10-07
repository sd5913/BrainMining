import runpy
from pathlib import Path

scripts = Path(__file__).parent
runpy.run_path(str(scripts / "verify_direct_faces.py"))
runpy.run_path(str(scripts / "verify_segmented_geometry.py"))
