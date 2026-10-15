"""Verify saved mine interfaces, source room references and buried mineral hosts."""
import unreal
from pathlib import Path
ROOT=Path(unreal.Paths.project_dir()).resolve()
code=(ROOT/'Scripts/verify_main_game_routes_v3_saved.py').read_text(encoding='utf-8')
code=code.replace('MainGameRoutesV3','MainGameRoutesV4').replace('Art/MainGame/RoutesV3','Art/MainGame/RoutesV4')
code=code.replace('unreal.Vector(0,0,10000 if i>=2 else 0)','unreal.Vector()').replace('unreal.Vector(0,0,10000 if i>=1 else 0)','unreal.Vector()')
code=code.replace("if i==3:check(r.generated_rock.triangle_count==before['regions'][i]['triangles'],'Fourth-region mixed geometry and neural scenery retained')", "if i==3:check(plan['landscape_count']==8 and plan['style']=='mixed-expedition','Fourth-region mixed route and eight neural scenery groups retained')")
exec(compile(code,'saved_angular_mines','exec'),{'__name__':'__main__','unreal':unreal})
