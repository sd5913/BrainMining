"""Run the real-input campaign excavation suite against the updated placement manifest."""
from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir())
code=(root/'Scripts/verify_main_game_collections.py').read_text(encoding='utf-8')
code=code.replace("OUT=ROOT/'Saved/Validation/MainGameCollections'","OUT=ROOT/'Saved/Validation/CollectionFeedback'")
code=code.replace("auth=json.loads((OUT/'authoring.json').read_text());assert auth['passed']", "auth=json.loads((ROOT/'Saved/Validation/MainGameCollections/authoring.json').read_text());feedback=json.loads((OUT/'authoring.json').read_text());assert feedback['passed'];auth['placements']=feedback['placements']")
code=code.replace("test_map=cfg['source_map'] if promotion.get('passed') else cfg['preview_map']","test_map=cfg['source_map']")
code=code.replace('range(420,1460,65)','range(-300,1460,65)').replace('range(420,1460,90)','range(-300,1460,90)')
# The deeper cola shaft must also be excavated continuously before pickup.
code=code.replace("s['hosts'][item.item_id].mine_sphere(front,210);s['hosts'][item.item_id].mine_sphere(p,170)","[(s['hosts'][item.item_id].mine_sphere(front+(p-front)*(i/7),170)) for i in range(8)]")
code=code.replace("s['views']=list(auth['placements']);", "check('Three dead-end exploration rewards',len(feedback['branches'])==3);s['views']=list(auth['placements']);")
# Record the immediate outline used by ordinary collectible focus in the dark room.
code=code.replace("if ph==13:\n            key('E');", "if ph==13:\n            item=s['items'][s['direct_ids'][s['index']]]\n            if item.item_id in [6,7]:check('Standard white focus outline '+str(item.item_id),s['field'].target==item and item.get_component_by_class(unreal.DynamicMeshComponent).get_editor_property('render_custom_depth'))\n            key('E');")
# Verify ants vanish after collecting the computer, not merely on changing area.
code=code.replace("if ph==11:\n            area", "if ph==11:\n            check('Collected computer leaves no ants',len(tagged('CollectionSoilAnt'))==0)\n            area")
code=code.replace("item=s['dig_item'];shot('dug_%02d.png'%item.item_id)","item=s['dig_item'];check('Excavated pigeon flies toward player',item.item_id!=3 or s['field'].flock_bursts>0);shot('dug_%02d.png'%item.item_id)")
exec(compile(code,str(root/'Scripts/verify_main_game_collections.py'),'exec'))
