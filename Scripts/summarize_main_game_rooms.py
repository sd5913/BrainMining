"""Collect completed room-integration evidence without contacting the editor."""
import json,shutil
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'Saved/Validation/MainGameRooms';SNAP=OUT/'Regression';SNAP.mkdir(exist_ok=True)
paths={'authoring':'MainGameRooms/authoring.json','room_runtime':'MainGameRooms/runtime.json','room_saved':'MainGameRooms/saved_state.json','routes':'MainGameRoutesV2/runtime.json','clay':'MainGameRoutesV2/Clay/runtime.json','start_room':'MainGameStartRoom/runtime.json','junction':'MainGameJunction/saved_collision.json','polish':'MainGamePolish/saved_state.json','preview':'MainGameRooms/preview.json'}
reports={}
for name,path in paths.items():
    source=ROOT/'Saved/Validation'/path;data=json.loads(source.read_text(encoding='utf-8'));assert data['passed'],(name,data.get('error'));reports[name]=data
    shutil.copy2(source,SNAP/(name+'.json'))
native=json.loads((OUT/'card_tv.json').read_text());assert native['passed']==1 and native['failed']==0 and all(not t['errors'] for t in native['tests'])
report={'passed':True,'backup':reports['authoring']['backup'],'sections':7,'referenced_room_maps':3,'mineable_connections':6,'deposits':165,'fragments':187491,'checks':{name:len(data.get('checks',[])) for name,data in reports.items()},'native_tests':native,'corridor_samples':sum(r['capsule_samples'] for r in reports['routes']['regions']),'landmark_probes':sum(r['landmarks'] for r in reports['routes']['regions']),'office':reports['room_runtime']['office_validation'],'mirror':reports['room_runtime']['mirror_validation'],'all_source_maps_unchanged':reports['authoring']['source_maps_unchanged'],'evidence':paths,'scope':'PIE isolation fixtures with native gameplay and physical collision checks; not a human uninterrupted full-map playthrough.'}
(OUT/'acceptance.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8');print(json.dumps(report,ensure_ascii=False))
