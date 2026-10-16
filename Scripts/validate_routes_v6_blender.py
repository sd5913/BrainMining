"""Independent clearance checks on the deformed source cavern and all caps."""
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
code=(ROOT/'Scripts/validate_routes_v5_blender.py').read_text()
code=code.replace("ART=ROOT/'Art/MainGame/RoutesV5'","ART=ROOT/'Art/MainGame/RoutesV6'")
code=code.replace("for stage in ['TunnelV1','TunnelCavernV2','TunnelStalagmiteV3']:","for stage in ['TunnelCavernV2']:")
code=code.replace("stage+'_RoutesV5.blend'","stage+'_RoutesV6.blend'")
code=code.replace('Saved/Validation/MainGameRoutesV5/source-clearance.json','Saved/Validation/MainGameRoutesV6/source-clearance.json')
(ROOT/'Saved/Validation/MainGameRoutesV6').mkdir(parents=True,exist_ok=True)
exec(compile(code,'v6_source_clearance','exec'),{'__file__':__file__,'__name__':'__main__'})
