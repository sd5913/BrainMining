"""Reanchor only the pigeon placement to the soil face visible from its approach."""
import json, shutil, traceback
from pathlib import Path
from datetime import datetime
import unreal

root=Path(unreal.Paths.project_dir())
out=root/'Saved/Validation/FeedbackOct26';out.mkdir(parents=True,exist_ok=True)
r={'passed':False}
def vec(v):return [v.x,v.y,v.z]
try:
 levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
 assert levels.load_level('/Game/MainGame/Maps/Lvl_MainGame')
 actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
 field=next(a for a in actors if isinstance(a,unreal.MiningCollectionField) and a.authored_placements)
 placements=list(field.authored_placements);bird=next(p for p in placements if p.item_id==3)
 host=bird.host_rock;assert host and host.actor_has_tag('BM_Collection23_PigeonSoil')
 origin=bird.approach_transform.translation
 direction=(bird.transform.translation-origin).normal()
 mesh=unreal.MiningDesignerLibrary.get_campaign_rock_source(host);frame=host.get_actor_transform()
 hits=[]
 for i in range(mesh.get_triangle_count()):
  valid,*vertices=unreal.GeometryScript_MeshQueries.get_triangle_positions(mesh,i)
  if not valid:continue
  a,b,c=[frame.transform_location(v) for v in vertices]
  e=b-a;f=c-a;h=direction.cross(f);det=e.dot(h)
  if abs(det)<1e-7:continue
  q=origin-a;u=q.dot(h)/det
  if not 0<=u<=1:continue
  cross=q.cross(e);v=direction.dot(cross)/det
  if v<0 or u+v>1:continue
  distance=f.dot(cross)/det
  if distance>=0:hits.append(distance)
 hits=sorted(set(round(h,3) for h in hits));assert len(hits)>=2,hits
 near,far=hits[:2];assert far-near>80,(near,far)
 front=origin+direction*near;body=origin+direction*((near+far)*.5)
 facing=unreal.MathLibrary.make_rot_from_x(direction)
 r.update(old_body=vec(bird.transform.translation),old_clue=vec(bird.clue_transform.translation),
          body=vec(body),clue=vec(front),approach=vec(origin),inward=vec(direction),soil_thickness=far-near,host=host.get_path_name())
 backup=root/'Saved/Backups'/('FeedbackOct26-'+datetime.now().strftime('%Y%m%d-%H%M%S'));backup.mkdir(parents=True)
 shutil.copy2(root/'Content/MainGame/Maps/Lvl_MainGame.umap',backup/'Lvl_MainGame.umap')
 bird.transform=unreal.Transform(location=body,rotation=facing,scale=bird.transform.scale3d)
 bird.clue_transform=unreal.Transform(location=front,rotation=facing)
 field.set_editor_property('authored_placements',placements,notify_mode=unreal.PropertyAccessChangeNotifyMode.NEVER)
 assert levels.save_current_level()
 r['passed']=True
except:
 r['error']=traceback.format_exc();unreal.log_error(r['error'])
finally:
 (out/'authoring.json').write_text(json.dumps(r,ensure_ascii=False,indent=2),encoding='utf8')
 unreal.SystemLibrary.quit_editor()
