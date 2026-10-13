"""Isolated NullRHI PIE: actual connector walk, excavation/reset, free passage between arbors."""
import unreal
from pathlib import Path
ROOT=Path(unreal.Paths.project_dir())
code=(ROOT/'Scripts/verify_main_game_cavern_runtime.py').read_text()
code=code.replace("now-state['time']<12", "now-state['time']<40")
code=code.replace("                report.update(walk_distance_cm=", "                velocity=pawn.get_velocity();report.update(walk_wall_seconds=now-state['time'],end_velocity_cm_s=[velocity.x,velocity.y,velocity.z])\n                report.update(walk_distance_cm=")
code=code.replace("old=find('BM_MainGame_V1');new=find('BM_MainGame_CavernV2');reserve=find('BM_MainGame_FutureRoom')",
    "old=find('BM_MainGame_StalagmiteV3');new=find('BM_MainGame_NeuralV4');reserve=find('BM_MainGame_FutureRoom_03')")
for a,b in {'TunnelCavernV2':'TunnelNeuralV4','cavern_creation':'neural_creation','cavern_runtime':'neural_runtime',
    'CAVERN_RUNTIME':'NEURAL_RUNTIME',"ART/'pillars.json'":"ART/'neural_rocks.json'",
    "ROOT/'Art/MainGame/TunnelV1/placement_plan.json'":"ROOT/'Art/MainGame/TunnelStalagmiteV3/placement_plan.json'",
    'cavern_stays_after_deep_descent':'neural_area_stays_after_deep_descent','cavern_position_cm':'neural_area_position_cm'}.items():code=code.replace(a,b)
# Teleport to the middle of the empty neural gallery, not on a dendrite.
code=code.replace("destination=raw(spec['center_m'])+origin+unreal.Vector(0,250,0)","path=json.loads((ART/'centerlines.json').read_text())[0];import bisect\n                index=bisect.bisect_left(path['arc'],spec['arc_m']);destination=raw(path['points'][index])+origin")
exec(compile(code,str(__file__),'exec'),{'__file__':str(__file__),'__name__':'__main__'})
