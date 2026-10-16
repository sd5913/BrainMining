"""Require explicit saved-map, PIE, geometry-test and build acceptance evidence."""
import json
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'Saved/Validation/PickaxeHardness'
author=json.loads((OUT/'authoring.json').read_text(encoding='utf-8'))
runtime=json.loads((OUT/'runtime.json').read_text(encoding='utf-8'))
native=json.loads((OUT/'native.json').read_text(encoding='utf-8'))
connections=json.loads((ROOT/'Saved/Validation/MainGameRoomEntrances/saved_state.json').read_text(encoding='utf-8'))
for data in (author,runtime,connections):
    assert data['passed'],data.get('error')
assert native['failed']==0 and native['passed']==15,native
assert all(t['state']=='Success' for t in native['tests'])
build=(ROOT/'Saved/Validation/pickaxe_hardness_test_fixture_build.log').read_text(errors='replace')
assert 'Result: Succeeded' in build
assert author['mineral_layout_sha256']=='d032ed60a4feee507dcf3867cbf3a65283a77b84ae9d73b30c959f79340dca4f'
assert (ROOT/'Content/MainGame/Maps/Lvl_MainGame.umap').stat().st_size>125000000
report={
    'passed':True,
    'build':'Succeeded',
    'native_tests':native['passed'],
    'runtime_checks':len(runtime['checks']),
    'saved_rock_actors':len(author['rocks']),
    'regions':author['regions'],
    'levels':runtime['levels'],
    'normal_hardness':[0,1,2,3],
    'soil_hardness':[1,2,3,4],
    'depth_factors':{'-4':0,'-3':.25,'-2':.5,'-1':.75,'0':1,'1':1.5,'2':2,'3':2.5,'4':3},
    'minerals':{'deposits':author['deposits'],'fragments':author['fragments'],'sha256':author['mineral_layout_sha256']},
    'map_recovery':{
        'reason':'Existing MainGame was an invalid 257-byte file before this task could load it.',
        'recovered_from':'C:/Users/11959/.codex/visualizations/2026/10/14/01a137f9-6903-7742-ad84-76b4b7697994/DarkSeams/BeforeFix/Lvl_MainGame.umap',
        'revalidated':'Six natural interfaces, boundaries, seven sections, four original terrains, mineral placement hashes, five original source-map hashes.',
        'saved_backup':'Saved/Backups/PickaxeHardness_20261015/Lvl_MainGame_hardness_saved.umap'
    }
}
(OUT/'acceptance.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps({'passed':True,'native_tests':native['passed'],'runtime_checks':len(runtime['checks']),'saved_rock_actors':len(author['rocks'])}))
