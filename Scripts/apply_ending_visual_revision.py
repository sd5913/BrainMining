"""Apply this revision to the existing single ending map, without rebuilding gameplay."""
import runpy
from pathlib import Path
for script in ['apply_ending_city.py','apply_ending_cell.py','apply_ending_starlight.py']:
    runpy.run_path(str(Path(__file__).with_name(script)),run_name='__main__')
