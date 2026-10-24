import unreal,json,traceback
from pathlib import Path
root=Path(unreal.Paths.project_dir());out=root/'Saved/Validation/Collection23';r={'passed':False}
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem);api=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
try:
 assert levels.load_level('/Game/MainGame/Maps/Lvl_MainGame');aa=list(api.get_all_level_actors());field=next(a for a in aa if isinstance(a,unreal.MiningCollectionField) and a.authored_placements)
 region=next(a for a in aa if isinstance(a,unreal.MiningExternalRegion) and a.actor_has_tag('BM_Campaign_MainGame03'));origin=region.get_actor_location()+region.cutters[0].local_transform.translation
 path=json.loads((root/'Art/MainGame/RoutesV6/TunnelStalagmiteV3/centerlines.json').read_text())[1];raw=lambda q:origin+unreal.Vector(q[0]*100,-q[1]*100,q[2]*100)
 candidates=[]
 for k in range(len(path['points'])-3,1,-1):
  start=raw(path['points'][k]);heading=(raw(path['points'][-1])-raw(path['points'][-8])).normal()
  for d in [heading,unreal.Vector(0,1,0),unreal.Vector(0,-1,0)]:
   hit=unreal.SystemLibrary.line_trace_single(ed.get_editor_world(),start,start+d*1300,unreal.TraceTypeQuery.ECC_VISIBILITY,True,[],unreal.DrawDebugTrace.NONE,True)
   h=hit.to_tuple() if hit else None
   if h and h[0] and not h[1] and isinstance(h[9],unreal.DiggableRock) and h[9].actor_has_tag('BM_Campaign_MainGame03') and abs(h[7].z)<.65 and h[5].distance(start)>80:candidates.append((h,start));break
  if candidates:break
 assert candidates,'No Taiji wall at terminal elevation'
 h,start=candidates[0];front=h[5];n=h[7];ps=list(field.authored_placements);p=next(p for p in ps if p.item_id==20);rot=unreal.MathLibrary.make_rot_from_x(n*-1)
 p.host_rock=h[9];p.transform=unreal.Transform(location=front-n*125,rotation=rot);p.clue_transform=unreal.Transform(location=front,rotation=rot);p.approach_transform=unreal.Transform(location=start,rotation=unreal.MathLibrary.find_look_at_rotation(start,front));field.set_editor_property('authored_placements',ps)
 r['taiji']=dict(position=list(p.transform.translation.to_tuple()),clue=list(front.to_tuple()),normal=list(n.to_tuple()),approach=list(start.to_tuple()),host=p.host_rock.get_name())
 for Id,text in [(16,'火候让食材涌现出新的香气，真香！'),(17,'小小的鱼缸，也是一整个相互依存的世界。'),(18,'一枚，再一枚……财富也能在积累中涌现。'),(19,'几种滋味碰在一起，灵感也有了新的层次。'),(20,'此消彼长，变化在彼此之间生发。'),(21,'车门一开，人潮就涌出来了！'),(22,'一条评论引来另一条，转眼就变成满屏讨论。')]:
  folder='/Game/Subtitles/Collectibles';name=f'DA_CollectiblePickup_{Id:02d}';asset=unreal.load_asset(folder+'/'+name) if unreal.EditorAssetLibrary.does_asset_exist(folder+'/'+name) else None
  if not asset:
   f=unreal.DataAssetFactory();f.set_editor_property('data_asset_class',unreal.SubtitleSegment);asset=unreal.AssetToolsHelpers.get_asset_tools().create_asset(name,folder,unreal.SubtitleSegment,f)
  asset.set_editor_property('segment_id',f'收集物拾取_{Id:02d}');line=unreal.SubtitleLine();line.set_editor_property('text',text);asset.set_editor_property('lines',[line]);assert unreal.EditorAssetLibrary.save_loaded_asset(asset)
 assert levels.save_current_level();r['passed']=True
except Exception:r['error']=traceback.format_exc();unreal.log_error(r['error'])
finally:(out/'refinement.json').write_text(json.dumps(r,ensure_ascii=False,indent=2),encoding='utf8');unreal.SystemLibrary.quit_editor()
