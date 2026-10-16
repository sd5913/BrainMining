"""Audit angular mine geometry using the established route collision checks."""
import unreal
from pathlib import Path
ROOT=Path(unreal.Paths.project_dir()).resolve()
code=(ROOT/'Scripts/verify_main_game_routes_v3.py').read_text(encoding='utf-8')
code=code.replace('MainGameRoutesV3','MainGameRoutesV4').replace('Art/MainGame/RoutesV3','Art/MainGame/RoutesV4')
code=code.replace('rise=unreal.Vector(0,0,10000 if i>=2 else 0)','rise=unreal.Vector()')
code=code.replace("V(before['exit'])+unreal.Vector(0,0,10000 if i>=1 else 0)","V(before['exit'])")
exec(compile(code,'angular_route_audit','exec'),{'__name__':'__main__','unreal':unreal})
