"""Apply targeted player-feedback authoring changes, preserving unrelated rooms and source meshes."""
import unreal,json,traceback,shutil,math
from pathlib import Path
from datetime import datetime
root=Path(unreal.Paths.project_dir());out=root/'Saved/Validation/PlayerFeedback26';out.mkdir(parents=True,exist_ok=True)
api=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);ml=unreal.MaterialEditingLibrary
r={};back=root/'Saved/Backups'/('PlayerFeedback26_'+datetime.now().strftime('%Y%m%d-%H%M%S'));back.mkdir(parents=True)
def backup(path):
 p=root/'Content'/path.removeprefix('/Game/');p=p.with_suffix('.umap' if 'Maps/' in path else '.uasset')
 if p.exists():dest=back/p.relative_to(root/'Content');dest.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(p,dest)
def v(x):return [x.x,x.y,x.z]
def segment(path,lines):
 backup(path);a=unreal.load_asset(path);assert a
 ls=[]
 for t in lines:l=unreal.SubtitleLine();l.set_editor_property('text',t);ls.append(l)
 a.set_editor_property('lines',ls);assert unreal.EditorAssetLibrary.save_loaded_asset(a)
def triangles(a):
 dm=unreal.MiningDesignerLibrary.get_campaign_rock_source(a);T=a.get_actor_transform();ts=[]
 for i in range(dm.get_triangle_count()):
  valid,*vs=unreal.GeometryScript_MeshQueries.get_triangle_positions(dm,i)
  if valid:ts.append(tuple(T.transform_location(x) for x in vs))
 return ts
def ray(ts,o,d):
 hits=[]
 for a,b,c in ts:
  e=b-a;f=c-a;h=d.cross(f);det=e.dot(h)
  if abs(det)<1e-7:continue
  q=o-a;u=q.dot(h)/det
  if u<0 or u>1:continue
  cross=q.cross(e);w=d.dot(cross)/det
  if w<0 or u+w>1:continue
  t=f.dot(cross)/det
  if t>=0:hits.append(t)
 return sorted(set(round(t,3) for t in hits))
try:
 segment('/Game/Subtitles/DA_LeaveStartRoom',['亮晶晶的地方说明一定有矿石 赶紧挖挖看'])
 for Id,text in {12:'收集到了25个铜矿！灵感涌现！',13:'收集到了15个铁矿！灵感涌现！',14:'收集到了10个孔雀石！灵感涌现！',15:'收集到了5个钻石！灵感涌现！',22:'一句话很轻，无数句话堆在一起，却能改变世界，也能压垮一个人'}.items():segment(f'/Game/Subtitles/Collectibles/DA_CollectiblePickup_{Id:02}',[text])
 path='/Game/CollectionPrototype/Materials/M_CollectionSilhouette';backup(path);m=unreal.load_asset(path)
 changed=0
 for n in unreal.ObjectIterator(unreal.MaterialExpressionCustom):
  if n.get_outer()!=m:continue
  code=n.get_editor_property('code')
  if 'float2 uv = GetDefaultSceneTextureUV' in code and 'BM_ViewportBorder' not in code:
   code=code.replace('float2 uv = GetDefaultSceneTextureUV', '''// BM_ViewportBorder: letterboxed render targets contain undefined pixels outside the view.
float2 viewPixel = GetViewportUV(Parameters) * View.ViewSizeAndInvSize.xy;
if(any(viewPixel < 3.0) || any(viewPixel > View.ViewSizeAndInvSize.xy - 3.0)) return Base.rgb;
float2 uv = GetDefaultSceneTextureUV''')
   n.set_editor_property('code',code);changed+=1
 ml.recompile_material(m);assert unreal.EditorAssetLibrary.save_loaded_asset(m);r['outline_border_nodes']=changed
 assert levels.load_level('/Game/MainGame/Maps/Lvl_MainGame');backup('/Game/MainGame/Maps/Lvl_MainGame')
 aa=list(api.get_all_level_actors());field=next(a for a in aa if isinstance(a,unreal.MiningCollectionField) and a.authored_placements);ps=list(field.authored_placements)
 specs=json.loads((root/'Config/BranchCollectiblePlacements.json').read_text(encoding='utf8'))
 for Id in [17,18]:
  p=next(p for p in ps if p.item_id==Id);direction=unreal.MathLibrary.transform_direction(p.clue_transform,unreal.Vector(1,0,0));old=p.transform.translation
  p.transform=unreal.Transform(location=p.clue_transform.translation+direction*230,rotation=p.transform.rotation.rotator(),scale=p.transform.scale3d)
  spec=next(s for s in specs if s['id']==Id);spec['position']=v(p.transform.translation)
 bird=next(p for p in ps if p.item_id==3);ts=triangles(bird.host_rock);d=unreal.MathLibrary.transform_direction(bird.clue_transform,unreal.Vector(1,0,0));o=bird.clue_transform.translation-d*250
 distances=ray(ts,o,d);r['pigeon_intersections']=distances
 assert len(distances)>=2,distances
 # Center the pigeon within the first solid interval immediately behind its feather clue.
 near,far=distances[:2];assert far-near>80,(near,far)
 bird.transform=unreal.Transform(location=o+d*((near+far)*.5),rotation=bird.transform.rotation.rotator(),scale=bird.transform.scale3d)
 r['pigeon_position']=v(bird.transform.translation);r['pigeon_soil_thickness']=far-near
 field.set_editor_property('authored_placements',ps,notify_mode=unreal.PropertyAccessChangeNotifyMode.NEVER)
 r['bombs']=[]
 for decal in aa:
  if not decal.actor_has_tag('BM_Polish32_BombSign') or not decal.actor_has_tag('BM_Campaign_MainGame03'):continue
  if decal.get_actor_label().endswith('_Back'):
   api.destroy_actor(decal);continue
  host=decal.get_attach_parent_actor()
  if not host:continue
  ts=triangles(host);d=decal.get_actor_forward_vector();right=decal.get_actor_right_vector();up=decal.get_actor_up_vector();old=decal.get_actor_location()
  # The source is now tessellated: the largest triangle may belong to a bevel.
  # Use the thin axis of this plug and its geometric centre, not an old corner anchor.
  lo=unreal.Vector(*(min(getattr(q,k) for t in ts for q in t) for k in ['x','y','z']))
  hi=unreal.Vector(*(max(getattr(q,k) for t in ts for q in t) for k in ['x','y','z']))
  spans=v(hi-lo);axis=spans.index(min(spans));components=[0,0,0];components[axis]=1 if v(d)[axis]>=0 else -1
  d=unreal.Vector(*components);rotation=unreal.MathLibrary.make_rot_from_xy(d,unreal.Vector(0,0,-1))
  right=unreal.MathLibrary.get_right_vector(rotation);up=unreal.MathLibrary.get_up_vector(rotation)
  center=(lo+hi)*.5;face=center-d*(spans[axis]*.5+30)
  candidates=[face,old,face+up*45,face-up*45,face+right*45,face-right*45]
  best=None
  for p in candidates:
   hs=ray(ts,p-d*120,d)
   if not hs:continue
   front=p-d*120+d*hs[0]-d*80
   for size in [125,110,96,82,68,52,40]:
    good=True
    for y,z in [(a,b) for a in [-.92,0,.92] for b in [-.92,0,.92]]:
     h=ray(ts,front+right*(y*size)+up*(z*size),d)
     if not h or h[0]>170:good=False;break
    if good:
     score=size*1000-front.distance(old)
     if best is None or score>best[0]:best=(score,front,size)
     break
  if not best:
   r['failed_projection']=dict(name=decal.get_actor_label(),host=host.get_actor_label(),d=v(d),old=v(old),triangles=[[v(x) for x in t] for t in ts],candidates=[v(p) for p in candidates]);raise AssertionError(decal.get_actor_label())
  decal.set_actor_rotation(rotation,False);decal.set_actor_location(best[1]+d*75,False,False);decal.decal.set_editor_property('decal_size',unreal.Vector(110,best[2],best[2]));decal.set_editor_property('tags',list(decal.tags)+['BM_SingleBombProjection'])
  r['bombs'].append(dict(name=decal.get_actor_label(),position=v(best[1]),half_size=best[2]))
 assert levels.save_current_level();(root/'Config/BranchCollectiblePlacements.json').write_text(json.dumps(specs,ensure_ascii=False,indent=2),encoding='utf8')
 backup('/Game/WenSi/Maps/Lvl_WenSi_MirrorMuseum');assert levels.load_level('/Game/WenSi/Maps/Lvl_WenSi_MirrorMuseum')
 unreal.SystemLibrary.execute_console_command(unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world(),'WenSi.UpgradeBalance')
 r['passed']=True
except: r['error']=traceback.format_exc();unreal.log_error(r['error'])
(out/'authoring.json').write_text(json.dumps(r,ensure_ascii=False,indent=2),encoding='utf8')
unreal.SystemLibrary.quit_editor()
