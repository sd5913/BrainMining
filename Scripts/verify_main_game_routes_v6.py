"""Run the established gameplay/collision audit against the current V6 route data."""
import unreal
from pathlib import Path
ROOT=Path(unreal.Paths.project_dir()).resolve()
code=(ROOT/'Scripts/verify_main_game_routes_v5.py').read_text()
code=code.replace("Saved/Validation/MainGameRoutesV5","Saved/Validation/MainGameRoutesV6").replace("Art/MainGame/RoutesV5","Art/MainGame/RoutesV6")
code=code.replace('==139','==101').replace('139 mineral deposits','101 mineral deposits')
exec(compile(code,'v6_gameplay','exec'),{'__name__':'__main__'})
