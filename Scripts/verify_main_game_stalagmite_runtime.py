"""NullRHI PIE: traverse second-to-third connector and verify mines/reset/stalagmite airspace."""
import unreal
from pathlib import Path
ROOT=Path(unreal.Paths.project_dir())
code=(ROOT/'Scripts/verify_main_game_cavern_runtime.py').read_text()
code=code.replace("now-state['time']<12", "now-state['time']<40")
code=code.replace("                report.update(walk_distance_cm=", "                velocity=pawn.get_velocity();report.update(walk_wall_seconds=now-state['time'],end_velocity_cm_s=[velocity.x,velocity.y,velocity.z])\n                report.update(walk_distance_cm=")
code=code.replace("old=find('BM_MainGame_V1');new=find('BM_MainGame_CavernV2');reserve=find('BM_MainGame_FutureRoom')",
    "old=find('BM_MainGame_CavernV2');new=find('BM_MainGame_StalagmiteV3');reserve=find('BM_MainGame_FutureRoom_02')")
for a,b in {'TunnelCavernV2':'TunnelStalagmiteV3','cavern_creation':'stalagmite_creation','cavern_runtime':'stalagmite_runtime',
    'CAVERN_RUNTIME':'STALAGMITE_RUNTIME',"ART/'pillars.json'":"ART/'stalagmites.json'",
    "ROOT/'Art/MainGame/TunnelV1/placement_plan.json'":"ROOT/'Art/MainGame/TunnelCavernV2/placement_plan.json'",
    'cavern_stays_after_deep_descent':'stalagmite_area_stays_after_deep_descent','cavern_position_cm':'stalagmite_area_position_cm'}.items():code=code.replace(a,b)
exec(compile(code,str(__file__),'exec'),{'__file__':str(__file__),'__name__':'__main__'})
