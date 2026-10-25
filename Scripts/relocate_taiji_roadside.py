"""Move only seven branch collectibles; keep terrain/mineral identities intact."""
import unreal, json, traceback, shutil
from pathlib import Path
from datetime import datetime

root=Path(unreal.Paths.project_dir());out=root/'Saved/Validation/BranchClues';out.mkdir(parents=True,exist_ok=True)
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem);api=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
report={'passed':False,'placements':[]}
def xyz(p):return list(p.to_tuple())
def trace(a,b):
 h=unreal.SystemLibrary.line_trace_single(ed.get_editor_world(),a,b,unreal.TraceTypeQuery.ECC_VISIBILITY,True,[],unreal.DrawDebugTrace.NONE,True)
 return h.to_tuple() if h and h.to_tuple()[0] else None
try:
 assert levels.load_level('/Game/MainGame/Maps/Lvl_MainGame')
 actors=list(api.get_all_level_actors());field=next(a for a in actors if isinstance(a,unreal.MiningCollectionField) and a.authored_placements)
 mineral=next(a for a in actors if isinstance(a,unreal.MiningTreasureField) and a.get_editor_property('authored_placements'))
 minerals=list(mineral.get_editor_property('authored_placements'));obstacles=[]
 for p in minerals:
  obstacles.extend([p.transform.translation,p.surface.translation]);obstacles.extend(f.translation for f in p.fragments)
 ps=list(field.authored_placements);others=[p.transform.translation for p in ps if p.item_id!=20]
 for Id,Region,Branch in [(20,3,0)]:
  area=f'BM_Campaign_MainGame{Region:02}';region=next(a for a in actors if isinstance(a,unreal.MiningExternalRegion) and a.actor_has_tag(area))
  stage=['TunnelV1','TunnelCavernV2','TunnelStalagmiteV3'][Region-1];path=json.loads((root/'Art/MainGame/RoutesV6'/stage/'centerlines.json').read_text())[Branch]
  origin=region.get_actor_location()+region.cutters[0].local_transform.translation;raw=lambda q:origin+unreal.Vector(q[0]*100,-q[1]*100,q[2]*100)
  candidates=[];end=raw(path['points'][-1]);stats=dict(floors=0,hits=0,clearance=0,best_clearance=0,other=0)
  for k in range(len(path['points'])-4,max(2,int(len(path['points'])*.35)),-3):
   base=raw(path['points'][k]);floor=trace(base,base-unreal.Vector(0,0,1400))
   if not floor or floor[1]:continue
   stats['floors']+=1
   start=unreal.Vector(base.x,base.y,floor[5].z+145)
   for d in [unreal.Vector(0,1,0),unreal.Vector(0,-1,0),unreal.Vector(1,0,0),unreal.Vector(-1,0,0)]:
    h=trace(start,start+d*950)
    if not h or h[1] or h[9]!=region.generated_rock or abs(h[7].z)>.55 or h[5].distance(start)<80:continue
    stats['hits']+=1
    front=h[5];n=h[7];pos=front-n*(190 if Id==21 else 140)
    clear=min(min(front.distance(v),pos.distance(v)) for v in obstacles)
    stats['best_clearance']=max(stats['best_clearance'],clear)
    if clear<500:stats['clearance']+=1;continue
    if any(front.distance(v)<1200 for v in others):stats['other']+=1;continue
    # Keep close enough to the floor to inspect and reach without jumping.
    candidates.append((front.distance(end),-clear,h,start,floor[5],pos))
  report.setdefault('candidate_stats',{})[str(Id)]=stats
  assert candidates,f'No mineral-free branch site for {Id}: {stats}'
  _,_,h,start,ground,pos=min(candidates,key=lambda c:(c[0],c[1]));front=h[5];n=h[7]
  p=next(p for p in ps if p.item_id==Id);rot=unreal.MathLibrary.make_rot_from_x(n*-1)
  p.host_rock=h[9];p.transform=unreal.Transform(location=pos,rotation=rot,scale=unreal.Vector(1.8,1.8,1.8) if Id==21 else unreal.Vector(1,1,1))
  p.clue_transform=unreal.Transform(location=front,rotation=rot);approach=unreal.Vector(start.x,start.y,ground.z+100)
  p.approach_transform=unreal.Transform(location=approach,rotation=unreal.MathLibrary.find_look_at_rotation(approach,front));others.append(pos)
  report['placements'].append(dict(id=Id,region=Region,branch=Branch,position=xyz(pos),clue=xyz(front),normal=xyz(n),approach=xyz(approach),host=h[9].get_name(),min_mineral_distance=min(min(front.distance(v),pos.distance(v)) for v in obstacles),scale=1.8 if Id==21 else 1.))
 backup=root/'Saved/Backups'/('BranchClues_'+datetime.now().strftime('%Y%m%d-%H%M%S')+'.umap');backup.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(root/'Content/MainGame/Maps/Lvl_MainGame.umap',backup)
 field.set_editor_property('authored_placements',ps,notify_mode=unreal.PropertyAccessChangeNotifyMode.NEVER)
 assert levels.save_current_level()
 report['mineral_count_unchanged']=len(mineral.get_editor_property('authored_placements'))==len(minerals);assert report['mineral_count_unchanged']
 fixture=root/'Config/BranchCollectiblePlacements.json'; data=json.loads(fixture.read_text(encoding='utf8')); data=[next((q for q in report['placements'] if q['id']==p['id']),p) for p in data]; fixture.write_text(json.dumps(data,ensure_ascii=False,indent=2),encoding='utf8')
 report['passed']=True
except Exception:report['error']=traceback.format_exc();unreal.log_error(report['error'])
finally:(out/'taiji-relocation.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8');unreal.SystemLibrary.quit_editor()
