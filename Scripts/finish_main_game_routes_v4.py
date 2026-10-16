"""Rebake clay and mineral hosts after the Lowpoly surface changes."""
import unreal,json
from pathlib import Path
ROOT=Path(unreal.Paths.project_dir()).resolve()
code=(ROOT/'Scripts/finish_main_game_routes_v3.py').read_text(encoding='utf-8').replace('MainGameRoutesV3','MainGameRoutesV4').replace('Art/MainGame/RoutesV3','Art/MainGame/RoutesV4')
code=code.replace("STAGES=['TunnelV1','TunnelCavernV2','TunnelStalagmiteV3']", "STAGES=['TunnelV1','TunnelCavernV2','TunnelStalagmiteV3','TunnelNeuralV4']")
code=code.replace("['BM_MainGame_V1','BM_MainGame_CavernV2','BM_MainGame_StalagmiteV3']", "['BM_MainGame_V1','BM_MainGame_CavernV2','BM_MainGame_StalagmiteV3','BM_MainGame_NeuralV4']")
code=code.replace('BM_MainGame_RegionalRoutesV3','BM_MainGame_LowpolyRoutesV4')
a=code.index("    previous=json.loads(");b=code.index('    assert levels.save_current_level()',a)
code=code[:a]+"    previous=json.loads((ROOT/'Saved/Validation/MainGameRoutesV3/Clay/bake.json').read_text(encoding='utf-8'))\n    joint_records=[s for s in previous['samples'] if s['kind']=='junction']\n    all_records += joint_records\n"+code[b:]
exec(compile(code,'lowpoly_finishing','exec'),{'__name__':'__main__'})
