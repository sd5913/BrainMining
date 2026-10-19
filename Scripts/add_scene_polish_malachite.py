"""Add one authored malachite deposit to the first red-route dead end."""
import unreal,json,sys,traceback,shutil
from pathlib import Path
from datetime import datetime
ROOT=Path(unreal.Paths.project_dir()).resolve();sys.path.insert(0,str(ROOT/'Scripts'))
import campaign_minerals_api as minerals
OUT=ROOT/'Saved/Validation/ScenePolish';report={'passed':False}
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);api=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
TAG='BM_ScenePolish_Branch01Malachite'
try:
 assert levels.load_level('/Game/MainGame/Maps/Lvl_MainGame')
 hall=next(a for a in api.get_all_level_actors() if isinstance(a,unreal.MiningExhibitHall));field=minerals.get_field(hall)
 if not field.actor_has_tag(TAG):
  region=next(a for a in api.get_all_level_actors() if isinstance(a,unreal.MiningExternalRegion) and a.actor_has_tag('BM_Campaign_MainGame03'))
  origin=region.get_actor_location()+region.cutters[0].local_transform.translation
  branch=next(b for b in json.loads((ROOT/'Art/MainGame/RoutesV6/TunnelStalagmiteV3/centerlines.json').read_text()) if b['name']=='Branch_01')
  def world(p):return origin+unreal.Vector(p[0]*100,-p[1]*100,p[2]*100)
  start=world(branch['points'][-10]);end=world(branch['points'][-1]);direction=(end-start).normal()
  hit=None
  for offset in [unreal.Vector(),unreal.Vector(0,0,-100),unreal.Vector(0,0,100)]:
   h=unreal.SystemLibrary.line_trace_single(ed.get_editor_world(),start+offset,end+direction*900+offset,unreal.TraceTypeQuery.ECC_VISIBILITY,True,[],unreal.DrawDebugTrace.NONE,True).to_tuple()
   if h[0] and isinstance(h[9],unreal.DiggableRock):hit=h;break
  assert hit,'Dead-end wall must be excavatable'
  value=next(d for d in field.definitions if str(d.id)=='6').values_by_size[0]
  candidates=[]
  for direction in [unreal.Vector(0,0,-1),unreal.Vector(1,0,0),unreal.Vector(-1,0,0),unreal.Vector(0,1,0),unreal.Vector(0,-1,0),unreal.Vector(0,0,1)]:
   h=unreal.SystemLibrary.line_trace_single(ed.get_editor_world(),start,start+direction*1000,unreal.TraceTypeQuery.ECC_VISIBILITY,True,[],unreal.DrawDebugTrace.NONE,True).to_tuple()
   if h[0] and isinstance(h[9],unreal.DiggableRock):
    candidates.append(unreal.MiningTreasurePlacement(treasure_id='6',value=value,area_tag='BM_Campaign_MainGame03',route_progress=.18,guide_layer=False,surface=unreal.Transform(location=h[5],rotation=unreal.MathLibrary.make_rot_from_x(h[7])),transform=unreal.Transform(location=h[5]),hosts=[h[9]]))
  candidates.sort(key=lambda p:p.surface.translation.distance(end))
  stage=minerals._stage(hall,api,field)
  try:
   prepared=False
   for p in candidates:
    stage.set_editor_property('authored_placements',[p])
    if unreal.MiningDesignerLibrary.prepare_campaign_minerals(stage):prepared=True;break
   assert prepared,'No fully buried, reachable deposit in the dead-end wall'
   validated=minerals.validate(stage,hall);assert validated['ok'],validated.get('errors')
   new=list(stage.get_editor_property('authored_placements'));assert len(new)==1
   existing=list(field.get_editor_property('authored_placements'))
   assert all(x.transform.translation.distance(new[0].transform.translation)>100 for x in existing),'New deposit overlaps an existing reward'
   backup=ROOT/'Saved/Backups'/('BeforeMalachite_'+datetime.now().strftime('%Y%m%d_%H%M%S')+'.umap');backup.parent.mkdir(exist_ok=True,parents=True);shutil.copy2(ROOT/'Content/MainGame/Maps/Lvl_MainGame.umap',backup)
   field.set_editor_property('authored_placements',existing+new);field.tags=list(field.tags)+[TAG]
   report.update(position=list(new[0].transform.translation.to_tuple()),surface=list(new[0].surface.translation.to_tuple()),branch=branch['name'],before=len(existing),after=len(existing)+1,fragments=len(new[0].fragments),validation=validated)
  finally:api.destroy_actor(stage)
  assert levels.save_current_level()
  tuning=ROOT/'Config/MainGameTuning.json';cfg=json.loads(tuning.read_text());cfg['expected_deposits']=len(field.get_editor_property('authored_placements'));tuning.write_text(json.dumps(cfg,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
 report['passed']=True
except Exception:report['error']=traceback.format_exc();unreal.log_error(report['error'])
(OUT/'malachite.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
levels.load_level('/Engine/Maps/Entry');unreal.SystemLibrary.quit_editor()
