"""Keep the six authored connection records aligned with their saved soil solids."""
import unreal,json
from pathlib import Path
ROOT=Path(unreal.Paths.project_dir()).resolve();assert not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world();cfg=json.loads((ROOT/'Config/MainGameRooms.json').read_text(encoding='utf-8'));actors=list(unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors());hall=next(a for a in actors if isinstance(a,unreal.MiningExhibitHall));report=[]
for s in cfg['generated_connections']:
 a=next(a for a in actors if a.get_actor_label()=='LINK_'+s['id']+'_DiggableSoil');previous=s['plug_x'];s['plug_x']=a.get_actor_location().x;s['plug']=a.get_path_name();report.append({'id':s['id'],'previous_x':previous,'actual_x':s['plug_x']})
(ROOT/'Config/MainGameRooms.json').write_text(json.dumps(cfg,ensure_ascii=False,indent=2),encoding='utf-8');(ROOT/'Saved/Validation/MainGameRoutesV3/manifest_sync.json').write_text(json.dumps(report,indent=2),encoding='utf-8');unreal.log('REGIONAL_SEAM_MANIFEST_SYNCHRONIZED')
