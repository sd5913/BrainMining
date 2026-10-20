import unreal,json,sys
from pathlib import Path
root=Path(unreal.Paths.project_dir());sys.path.insert(0,str(root/'Scripts'))
import campaign_minerals_api as api
L=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);A=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
assert L.load_level('/Game/MainGame/Maps/Lvl_MainGame')
aa=A.get_all_level_actors();hall=next(a for a in aa if isinstance(a,unreal.MiningExhibitHall));field=api.get_field(hall)
def xyz(p):return [p.x,p.y,p.z]
r={'layout':api._layout(field),'areas':[],'collections':[]}
for a in hall.areas:r['areas'].append(dict(tag=str(a.tag),route=[xyz(p) for p in a.map_route]))
for a in aa:
 if isinstance(a,unreal.MiningCollectionField):
  for p in a.get_editor_property('authored_placements'):
   if p.item_id==1:r['collections'].append(dict(id=p.item_id,pos=xyz(p.transform.translation),clue=xyz(p.clue_transform.translation),approach=xyz(p.approach_transform.translation),host=p.host_rock.get_actor_label() if p.host_rock else ''))
sites=list(unreal.MiningDesignerLibrary.find_campaign_mineral_sites(hall))
r['sites']=[dict(index=i,area=str(p.area_tag),progress=p.route_progress,surface=api._transform(p.surface),core=xyz(p.transform.translation)) for i,p in enumerate(sites) if str(p.area_tag)=='BM_Campaign_MainGame01']
r['hosts']=[[h.get_actor_label() for h in p.hosts if h] for p in field.get_editor_property('authored_placements')]
(root/'Saved/Validation/Feedback11/minerals-inspection.json').write_text(json.dumps(r,ensure_ascii=False,indent=2),encoding='utf8')
unreal.SystemLibrary.quit_editor()
