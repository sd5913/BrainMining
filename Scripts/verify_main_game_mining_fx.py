"""Regression: both mining tools, both layer types, all four campaign regions."""
import unreal,runpy
from pathlib import Path
root=Path(unreal.Paths.project_dir()).resolve()
cases=[(i,.8,drill,layer) for i in range(4) for layer in ['normal','guide'] for drill in [False,True]]
runpy.run_path(str(root/'Scripts/inspect_main_game_mining_fx_runtime.py'),init_globals={'STRICT':True,'cases':cases},run_name='__main__')
