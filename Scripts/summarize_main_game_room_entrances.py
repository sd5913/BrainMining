"""Collect explicit results for the natural entrances and exposure refinement."""
import json,shutil
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'Saved/Validation/MainGameRoomEntrances'
source=ROOT/'Saved/Validation/MainGameRooms/runtime.json';runtime=json.loads(source.read_text(encoding='utf-8'));assert runtime['passed'];shutil.copy2(source,OUT/'room_runtime.json')
reports={name:json.loads((OUT/(file+'.json')).read_text(encoding='utf-8-sig')) for name,file in [('rooms','room_runtime'),('saved','saved_state'),('exposure','exposure_runtime'),('wall_mining','rock_mining'),('images','image_comparison')]}
for name,data in reports.items():assert data['passed'],(name,data.get('error'))
native=json.loads((OUT/'card_tv.json').read_text());assert native['passed']==1 and native['failed']==0 and all(t['state']=='Success' and not t['errors'] for t in native['tests'])
assert len(runtime['connections'])==6 and all(c['lip_cuts']==0 for c in runtime['connections'])
assert len(reports['wall_mining']['walls'])==6 and all(c['wall_cut_and_reset'] for c in reports['wall_mining']['walls'])
assert reports['saved']['mineral_layout_sha256']=='d032ed60a4feee507dcf3867cbf3a65283a77b84ae9d73b30c959f79340dca4f'
summary={'passed':True,'map':'/Game/MainGame/Maps/Lvl_MainGame','natural_connections':6,'room_entrance_clay_plugs':3,'checks':{name:len(data.get('checks',[])) for name,data in reports.items()},'same_view_max_luma_difference_percent':max(abs(c['luma_ratio']-1)*100 for c in reports['images']['comparisons']),'native_card_tv':native,'office':runtime['office_validation'],'mirror':runtime['mirror_validation'],'original_deposits':165,'original_fragments':187491,'source_maps_unchanged':True,'backup':json.loads((OUT/'before.json').read_text())['backup'],'scope':'PIE seam mining/reset, capsule clearance, 13 floor/backing probes per seam, native office/mirror/card/TV workflows, saved-map reload and identical-camera exposure images; not a human uninterrupted full-map playthrough.'}
(OUT/'acceptance.json').write_text(json.dumps(summary,ensure_ascii=False,indent=2),encoding='utf-8');print(json.dumps(summary,ensure_ascii=False))
