"""Refresh only cell interior and city dressing; starlight assets stay untouched."""
import runpy
from pathlib import Path
for script in ['apply_ending_city.py','apply_ending_cell.py']:
    runpy.run_path(str(Path(__file__).with_name(script)),run_name='__main__')
