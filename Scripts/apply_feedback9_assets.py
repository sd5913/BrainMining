"""Targeted feedback: actual forward copper deposit, deep coins, two-sided bomb signs, subtitle copy."""
import unreal,json,shutil,sys,traceback,re
from pathlib import Path
root=Path(unreal.Paths.project_dir());out=root/'Saved/Validation/Feedback9';out.mkdir(parents=True,exist_ok=True);sys.path.insert(0,str(root/'Scripts'))
import campaign_minerals_api as minerals
L=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);A=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);E=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem);EA=unreal.EditorAssetLibrary
r={'passed':False,'subtitles':[],'bombs':[]}
def backup(path,ext='uasset'):
 rel=Path('Content')/(path.removeprefix('/Game/')+'.'+ext);dest=out/'BeforeAssets'/rel
 if not dest.exists():dest.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(root/rel,dest)
def triangles(a):
 dm=unreal.MiningDesignerLibrary.get_campaign_rock_source(a);t=a.get_actor_transform();result=[]
 for i in range(dm.get_triangle_count()):
  valid,*vs=unreal.GeometryScript_MeshQueries.get_triangle_positions(dm,i)
  if valid:result.append(tuple(t.transform_location(v) for v in vs))
 return result
def ray(ts,o,d):
 hits=[]
 for a,b,c in ts:
  e=b-a;f=c-a;h=d.cross(f);det=e.dot(h)
  if abs(det)<1e-7:continue
  q=o-a;u=q.dot(h)/det
  if u<0 or u>1:continue
  cross=q.cross(e);v=d.dot(cross)/det
  if v<0 or u+v>1:continue
  t=f.dot(cross)/det
  if t>=0:hits.append(t)
 return min(hits) if hits else None
try:
 for i in range(23):
  path=f'/Game/Subtitles/Collectibles/DA_CollectiblePickup_{i:02d}';a=unreal.load_asset(path);ls=list(a.get_editor_property('lines'));changed=False
  for line in ls:
   before=str(line.text);after=re.sub(r'[,，]',' ',before).replace('。','').replace('.','');after=re.sub(' +',' ',after).strip()
   if before!=after:changed=True;line.set_editor_property('text',after)
  if changed:backup(path);a.set_editor_property('lines',ls);assert EA.save_loaded_asset(a,False)
  r['subtitles'].append(dict(id=i,lines=[str(x.text) for x in ls]))
 path='/Game/Subtitles/DA_PickaxeUpgrade1';a=unreal.load_asset(path);assert a;backup(path);ls=list(a.get_editor_property('lines'));ls[0].set_editor_property('text','矿镐升级可以让挖掘更轻松');a.set_editor_property('lines',ls);assert EA.save_loaded_asset(a,False)
 assert L.load_level('/Game/MainGame/Maps/Lvl_MainGame');assert L.set_current_level_by_name('Lvl_MainGame');backup('/Game/MainGame/Maps/Lvl_MainGame','umap')
 aa=list(A.get_all_level_actors());hall=next(a for a in aa if isinstance(a,unreal.MiningExhibitHall));field=minerals.get_field(hall);ps=list(field.get_editor_property('authored_placements'));old=ps[9];assert str(old.treasure_id)=='2'
 candidates=[]
 for z in [10360,10400,10320]:
  for x in [-27200,-27150,-27100]:
   start=unreal.Vector(x,800,z)
   for end in [unreal.Vector(x,1900,z),unreal.Vector(x,-500,z),unreal.Vector(x+1100,800,z)]:
    h=unreal.SystemLibrary.line_trace_single(E.get_editor_world(),start,end,unreal.TraceTypeQuery.ECC_VISIBILITY,True,[],unreal.DrawDebugTrace.NONE,True).to_tuple()
    if not h[0] or not isinstance(h[9],unreal.DiggableRock) or abs(h[7].z)>.7:continue
    clearance=min(h[5].distance(p.surface.translation) for i,p in enumerate(ps) if i!=9)
    if clearance<210:continue
    candidates.append((h[5].distance(old.surface.translation),h))
 assert candidates,'No valid lower sidewall position';_,hit=min(candidates,key=lambda q:q[0])
 p=unreal.MiningTreasurePlacement()
 for key in ['treasure_id','value','area_tag','route_progress','guide_layer']:p.set_editor_property(key,old.get_editor_property(key))
 p.surface=unreal.Transform(location=hit[5],rotation=unreal.MathLibrary.make_rot_from_x(hit[7]));p.transform=unreal.Transform(location=hit[5]-hit[7]*120);p.hosts=[hit[9]]
 stage=minerals._stage(hall,A,field);stage.set_editor_property('authored_placements',[p]);assert unreal.MiningDesignerLibrary.prepare_campaign_minerals(stage)
 validation=minerals.validate(stage,hall);assert validation['ok'],validation['errors'];ps[9]=list(stage.get_editor_property('authored_placements'))[0];field.set_editor_property('authored_placements',ps);A.destroy_actor(stage)
 r['copper']=dict(index=9,before=old.surface.translation.to_tuple(),after=ps[9].surface.translation.to_tuple(),validation=validation)
 collection=next(a for a in aa if isinstance(a,unreal.MiningCollectionField) and a.actor_has_tag('BM_MainGame_Collections'));placements=list(collection.get_editor_property('authored_placements'))
 coin=next(p for p in placements if p.item_id==18);direction=unreal.MathLibrary.get_forward_vector(coin.clue_transform.rotation.rotator());coin.transform=unreal.Transform(location=coin.clue_transform.translation+direction*400,rotation=coin.transform.rotation.rotator(),scale=coin.transform.scale3d)
 collection.set_editor_property('authored_placements',placements);r['coin_position']=coin.transform.translation.to_tuple()
 specs=json.loads((root/'Config/BranchCollectiblePlacements.json').read_text(encoding='utf8'));next(s for s in specs if s['id']==18)['position']=list(coin.transform.translation.to_tuple());(root/'Config/BranchCollectiblePlacements.json').write_text(json.dumps(specs,ensure_ascii=False,indent=2),encoding='utf8')
 # Restore the missing rear stencil, using only the target wall's source surface.
 for sign in aa:
  if not sign.actor_has_tag('BM_Polish32_BombSign') or not sign.actor_has_tag('BM_Campaign_MainGame03') or not sign.get_actor_label().endswith('_Front'):continue
  host=sign.get_attach_parent_actor();assert host;ts=triangles(host);d=-sign.get_actor_forward_vector();origin=sign.get_actor_location()-d*2000;dist=ray(ts,origin,d);assert dist is not None
  surface=origin+d*dist;rotation=unreal.MathLibrary.make_rot_from_xy(d,unreal.Vector(0,0,-1));right=unreal.MathLibrary.get_right_vector(rotation);up=unreal.MathLibrary.get_up_vector(rotation)
  chosen=None
  for size in [110,100,90,80,70]:
   distances=[ray(ts,surface-d*500+right*(y*size)+up*(z*size),d) for y in [-1,-.5,0,.5,1] for z in [-1,-.5,0,.5,1]]
   if all(v is not None and 250<v<750 for v in distances):chosen=(size,min(distances),max(distances));break
  assert chosen,host.get_actor_label();size,near,far=chosen
  center=surface-d*500+d*((near+far)*.5)
  back=A.spawn_actor_from_class(unreal.DecalActor,center,rotation);back.set_actor_label(sign.get_actor_label().replace('_Front','_Back'));back.decal.set_decal_material(sign.decal.get_decal_material());back.decal.set_editor_property('decal_size',unreal.Vector((far-near)*.5+12,size,size));back.decal.set_fade_screen_size(.0001)
  back.tags=list(sign.tags);back.attach_to_actor(host,'',unreal.AttachmentRule.KEEP_WORLD,unreal.AttachmentRule.KEEP_WORLD,unreal.AttachmentRule.KEEP_WORLD,False)
  r['bombs'].append(dict(name=back.get_actor_label(),position=center.to_tuple(),direction=d.to_tuple(),size=size,depth=(far-near)*.5+12))
 assert L.save_current_level()
 r['drill']=[]
 for path in ['/Game/Showcase/Blueprints/BP_SharedMiningCharacter','/Game/MainGame/Blueprints/BP_MainGameCharacter']:
  bp=unreal.load_asset(path);c=unreal.get_default_object(unreal.EditorAssetLibrary.load_blueprint_class(path)).get_component_by_class(unreal.MiningComponent)
  if c.get_editor_property('max_drill_energy')!=36:
   backup(path);c.set_editor_property('max_drill_energy',36);c.set_editor_property('drill_energy',36);assert EA.save_loaded_asset(bp,False)
  r['drill'].append(dict(path=path,capacity=c.get_editor_property('max_drill_energy')))
 r['passed']=True
except: r['error']=traceback.format_exc();unreal.log_error(r['error'])
(out/'assets.json').write_text(json.dumps(r,ensure_ascii=False,indent=2),encoding='utf8');unreal.SystemLibrary.quit_editor()
