"""Three saved-region clay/wall transitions, using the real PIE camera and strikes."""
from pathlib import Path
import unreal
ROOT=Path(unreal.Paths.project_dir()).resolve()
source=(ROOT/'Scripts/verify_routes_v5_debris_reuse.py').read_text(encoding='utf-8')
scope={};exec(compile(source[:source.index("exec(compile(code,'verify_debris_reuse'")],'reuse_helpers','exec'),scope)
code=scope['code'].replace('MainGameRoutesV5/DebrisFixed','MainGameRoutesV5/OrganicWalls')
code=code.replace("move(h[5]+h[6]*420-unreal.Vector(0,0,64))", "move((s['probe_start'] if cases[s['index']][0]==2 else h[5]+h[6]*760+h[6].cross(unreal.Vector(0,0,1))*90)-unreal.Vector(0,0,64))")
exec(compile(code,'organic_walls','exec'),dict(__name__='__main__',STRICT=False,cases=[(0,.8,False,'guide'),(1,.8,False,'guide'),(2,.8,False,'guide')]))
