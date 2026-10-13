"""Append the third region using the verified external-region construction workflow."""
import unreal
from pathlib import Path
ROOT=Path(unreal.Paths.project_dir())
code=(ROOT/'Scripts/append_main_game_cavern.py').read_text()
code=code.replace("reserve.set_editor_property('cutters',[cutter(connector_mesh,new_origin,reserve),cutter(old_cutter.mesh,old_origin,reserve),cutter(mesh,new_origin,reserve)])", "reserve.set_editor_property('cutters',[cutter(connector_mesh,new_origin,reserve)])")
for a,b in {
    'TunnelCavernV2':'TunnelStalagmiteV3',
    'CavernV2':'StalagmiteV3',
    'cavern_creation':'stalagmite_creation',
    'CAVERN ':'STALAGMITE ',
    'EDIT_MainGame_02_溶洞通路矿区':'EDIT_MainGame_03_石笋通路矿区',
    'EDIT_MainGame_连接预留区_18m':'EDIT_MainGame_连接预留区_02_18m',
    'BM_MainGame_FutureRoom':'BM_MainGame_FutureRoom_02',
    'RESERVE_未来房间中心_18m连接空腔_勿放阻塞层':'RESERVE_未来房间中心_02_18m连接空腔_勿放阻塞层',
    "'BM_MainGame_V1'":"'BM_MainGame_CavernV2'",
    "ART/'pillars.json'":"ART/'stalagmites.json'",
    "center=raw(spec['center_m'])+new_origin":"center=raw(spec['probe_m'])+new_origin",
    'column_checks':'stalagmite_checks',
    'Column collision missing':'Stalagmite collision missing',
    'MainGame_V2_':'MainGame_V3_',
    "phase='generating_cavern'":"phase='generating_stalagmite_region'",
}.items():code=code.replace(a,b)
exec(compile(code,str(__file__),'exec'),{'__file__':str(__file__),'__name__':'__main__'})
