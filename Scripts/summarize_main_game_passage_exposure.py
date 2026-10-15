"""Collect the saved-scene visibility checks for the dark-corridor fix."""
import json,hashlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'Saved/Validation/MainGamePassageExposure'
runtime=json.loads((OUT/'runtime.json').read_text());images=json.loads((OUT/'visibility.json').read_text(encoding='utf-8-sig'));interior=json.loads((ROOT/'Saved/Validation/MainGameRoomEntrances/exposure_runtime.json').read_text());comparison=json.loads((ROOT/'Saved/Validation/MainGameRoomEntrances/image_comparison.json').read_text(encoding='utf-8-sig'))
for data in [runtime,images,interior,comparison]:assert data['passed'],data.get('error',data.get('failed'))
assert len(runtime['views'])==24 and len(images['views'])==24
native=json.loads((OUT/'card_tv.json').read_text());assert native['passed']==1 and native['failed']==0
layout=json.loads((OUT/'scene_layout.json').read_text());assert layout['passed']
map_path=ROOT/'Content/MainGame/Maps/Lvl_MainGame.umap';before=Path('C:/Users/11959/.codex/visualizations/2026/10/14/01a137f9-6903-7742-ad84-76b4b7697994/DarkSeams/BeforeFix/Lvl_MainGame.umap');same=hashlib.sha256(map_path.read_bytes()).digest()==hashlib.sha256(before.read_bytes()).digest()
report={'passed':True,'checks':len(runtime['checks'])+len(interior['checks']),'passage_views':24,'map_package_bytes_unchanged':same,'scene_layout':layout,'minimum_scene_mean_luma':min(v['mean_luma'] for v in images['views']),'maximum_scene_black_fraction':max(v['black_fraction'] for v in images['views']),'maximum_scene_white_clipping':max(v['clipped_fraction'] for v in images['views']),'room_interior_activity_luma_change_percent':max(abs(v['luma_ratio']-1)*100 for v in comparison['comparisons']),'native_card_tv':native,'runtime':'runtime.json','images':'visibility.json','backup':'Saved/Backups/MainGame_PassageExposure_20261015','scope':'Six seams facing mine/room before/after excavation, source interior profiles, game activation comparisons, saved rock/guide/mineral layout and native card/TV workflow. Package bytes may change on editor save.'}
(OUT/'acceptance.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8');print(json.dumps(report,ensure_ascii=False))
