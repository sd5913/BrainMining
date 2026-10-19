import runpy,unreal
from pathlib import Path
scripts=Path(unreal.Paths.project_dir())/'Scripts'
runpy.run_path(str(scripts/'import_card_robot.py'),init_globals={'CARD_ROBOT_CHAIN':True})
runpy.run_path(str(scripts/'install_card_robot.py'))
