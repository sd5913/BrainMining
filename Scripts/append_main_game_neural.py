"""Append the fourth region after the stalagmite region; use the existing external cutter tool."""
import unreal
from pathlib import Path
ROOT=Path(unreal.Paths.project_dir())
code=(ROOT/'Scripts/append_main_game_cavern.py').read_text()
code=code.replace("'chunk_length',1600.", "'chunk_length',1800.")
code=code.replace("reserve.set_editor_property('cutters',[cutter(connector_mesh,new_origin,reserve),cutter(old_cutter.mesh,old_origin,reserve),cutter(mesh,new_origin,reserve)])", "reserve.set_editor_property('cutters',[cutter(connector_mesh,new_origin,reserve)])")
for a,b in {
    'TunnelCavernV2':'TunnelNeuralV4','CavernV2':'NeuralV4','cavern_creation':'neural_creation','CAVERN ':'NEURAL ',
    'EDIT_MainGame_02_溶洞通路矿区':'EDIT_MainGame_04_神经岩石通路矿区',
    'EDIT_MainGame_连接预留区_18m':'EDIT_MainGame_连接预留区_03_18m',
    'BM_MainGame_FutureRoom':'BM_MainGame_FutureRoom_03',
    'RESERVE_未来房间中心_18m连接空腔_勿放阻塞层':'RESERVE_未来房间中心_03_18m连接空腔_勿放阻塞层',
    "'BM_MainGame_V1'":"'BM_MainGame_StalagmiteV3'",
    "ART/'pillars.json'":"ART/'neural_rocks.json'",
    "center=raw(spec['center_m'])+new_origin":"center=raw(spec['probe_m'])+new_origin",
    'column_checks':'neural_rock_checks','Column collision missing':'Neural rock collision missing','MainGame_V2_':'MainGame_V4_',
    "phase='generating_cavern'":"phase='generating_neural_region'",
}.items():code=code.replace(a,b)
exec(compile(code,str(__file__),'exec'),{'__file__':str(__file__),'__name__':'__main__'})
