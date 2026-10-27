"""Exterior pigeon clues/GM approach and the established mineable clay-clod dressing."""
import bisect, json, math, random, shutil, traceback
from datetime import datetime
from pathlib import Path
import unreal

root=Path(unreal.Paths.project_dir()).resolve();out=root/'Saved/Validation/PigeonSurface';out.mkdir(parents=True,exist_ok=True)
ea=unreal.EditorAssetLibrary;ml=unreal.MaterialEditingLibrary
r={'passed':False};backup=root/'Saved/Backups'/('PigeonSurface-'+datetime.now().strftime('%Y%m%d-%H%M%S'))
try:
 levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);assert levels.load_level('/Game/MainGame/Maps/Lvl_MainGame')
 actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
 field=next(a for a in actors if isinstance(a,unreal.MiningCollectionField) and a.authored_placements)
 placements=list(field.authored_placements);bird=next(p for p in placements if p.item_id==3);host=bird.host_rock
 assert host.actor_has_tag('BM_Collection23_PigeonSoil')
 backup.mkdir(parents=True);shutil.copy2(root/'Content/MainGame/Maps/Lvl_MainGame.umap',backup/'Lvl_MainGame.umap')
 # The former approach was beyond the soil plug, inside the hidden dead end.
 # Use Branch_03's approach from the main route, before blocker 07.
 routes=root/'Art/MainGame/RoutesV6/TunnelCavernV2'
 plan=json.loads((routes/'placement_plan.json').read_text())['objects'][6]
 assert plan['path_name']=='Branch_03'
 path=json.loads((routes/'centerlines.json').read_text())[plan['path_index']]
 distance=plan['start_arc_m']-3.0
 index=bisect.bisect_left(path['arc'],distance)
 alpha=(distance-path['arc'][index-1])/(path['arc'][index]-path['arc'][index-1])
 q=[a+(b-a)*alpha for a,b in zip(path['points'][index-1],path['points'][index])]
 region=next(a for a in actors if isinstance(a,unreal.MiningExternalRegion) and a.actor_has_tag(bird.area_tag))
 route_origin=region.get_actor_location()+region.cutters[0].local_transform.translation
 base=route_origin+unreal.Vector(q[0]*100,-q[1]*100,q[2]*100)
 world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
 hit=unreal.SystemLibrary.line_trace_single(world,base,base-unreal.Vector(0,0,800),unreal.TraceTypeQuery.ECC_VISIBILITY,True,[a for a in actors if a.actor_has_tag('BM_PigeonSoilClods')],unreal.DrawDebugTrace.NONE,True)
 floor=hit.to_tuple();assert floor[0] and not floor[1] and floor[7].z>.55,floor
 approach=unreal.Vector(base.x,base.y,floor[5].z+96)
 eye=approach+unreal.Vector(0,0,64);direction=(bird.transform.translation-eye).normal()
 mesh=unreal.MiningDesignerLibrary.get_campaign_rock_source(host);frame=host.get_actor_transform();hits=[]
 for i in range(mesh.get_triangle_count()):
  valid,*vertices=unreal.GeometryScript_MeshQueries.get_triangle_positions(mesh,i)
  if not valid:continue
  a,b,c=[frame.transform_location(v) for v in vertices]
  e=b-a;f=c-a;h=direction.cross(f);det=e.dot(h)
  if abs(det)<1e-7:continue
  delta=eye-a;u=delta.dot(h)/det
  if not 0<=u<=1:continue
  cross=delta.cross(e);v=direction.dot(cross)/det
  if v<0 or u+v>1:continue
  d=f.dot(cross)/det
  if d>=0:hits.append(d)
 assert hits,'No exterior soil face along the route approach'
 front=eye+direction*min(hits)
 r.update(old_approach=list(bird.approach_transform.translation.to_tuple()),old_clue=list(bird.clue_transform.translation.to_tuple()),approach=list(approach.to_tuple()),eye=list(eye.to_tuple()),clue=list(front.to_tuple()),inward=list(direction.to_tuple()))
 bird.clue_transform=unreal.Transform(location=front,rotation=unreal.MathLibrary.make_rot_from_x(direction))
 bird.approach_transform=unreal.Transform(location=approach,rotation=unreal.MathLibrary.find_look_at_rotation(eye,front))
 field.set_editor_property('authored_placements',placements,notify_mode=unreal.PropertyAccessChangeNotifyMode.NEVER)
 source='/Game/MainGame/Materials/RoutesV5/M_BlueViolet'
 destination='/Game/MainGame/Materials/DirtTransition/M_PigeonSoilTransition'
 if ea.does_asset_exist(destination):
  shutil.copy2(root/'Content/MainGame/Materials/DirtTransition/M_PigeonSoilTransition.uasset',backup/'M_PigeonSoilTransition.uasset')
 original=unreal.load_asset(source)
 clay=unreal.load_asset('/Game/MainGame/Materials/DirtTransition/MI_ClayBlockage')
 assert original and clay
 origin=bird.clue_transform.translation;axis=unreal.MathLibrary.transform_direction(bird.clue_transform,unreal.Vector(1,0,0));center=bird.transform.translation
 # These are the adjoining editable terrain and its exterior diggable backing.
 # Match by current material and local bounds rather than unstable actor indices.
 targets=[host]
 for a in actors:
  if a==host or not isinstance(a,unreal.DiggableRock) or not a.rock_material:continue
  if a.rock_material.get_path_name().split('.')[0] not in [source,destination]:continue
  pos,extent=a.get_actor_bounds(False);delta=origin-pos
  if all(abs(getattr(delta,k))<getattr(extent,k)+200 for k in ['x','y','z']):targets.append(a)
 assert len(targets)==3,[(a.get_name(),a.get_actor_label()) for a in targets]
 r['actors']=[]
 for a in targets:
  r['actors'].append({'name':a.get_name(),'old_material':a.rock_material.get_path_name()})
  material=clay if a==host else original
  a.set_editor_property('rock_material',material,notify_mode=unreal.PropertyAccessChangeNotifyMode.NEVER)
  for component in a.get_components_by_class(unreal.DynamicMeshComponent):
   for index in range(component.get_num_materials()):component.set_material(index,material)
 # Broad, sparse clods retain the established faceting without a dense scatter
 # of tiny spots. Keep clear wall between pieces and leave the approach open.
 # This plug used to be bomb-only, so that pass explicitly skipped its clods.
 actor_api=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
 clods=next((a for a in actors if a.actor_has_tag('BM_PigeonSoilClods')),None)
 if not clods:clods=actor_api.spawn_actor_from_class(unreal.GeneratedMiningRock,center)
 clods.set_actor_label('MAIN_GAME_PigeonSoil_WallClods')
 clods.set_editor_property('tags',[bird.area_tag,'BM_PigeonSoilClods','BM_RoutesV5ClayRelief'])
 clods.set_editor_property('rock_material',clay,notify_mode=unreal.PropertyAccessChangeNotifyMode.NEVER)
 clods.set_editor_property('hardness_level',0,notify_mode=unreal.PropertyAccessChangeNotifyMode.NEVER)
 rng=random.Random(20261026);combined=unreal.DynamicMesh();accepted=[];samples=[]
 def point(s):
  index=max(1,min(len(path['arc'])-1,bisect.bisect_left(path['arc'],s)))
  t=max(0,min(1,(s-path['arc'][index-1])/(path['arc'][index]-path['arc'][index-1])))
  v=[a+(b-a)*t for a,b in zip(path['points'][index-1],path['points'][index])]
  return route_origin+unreal.Vector(v[0]*100,-v[1]*100,v[2]*100)
 for arc,sign in [(plan['start_arc_m'],-1),(plan['end_arc_m'],1)]:
  side_start=len(samples)
  clusters=[(rng.uniform(0,math.tau),rng.uniform(.55,.9)) for _ in range(2)]
  for attempt in range(90):
   if len(samples)-side_start>=10:break
   offset=.08+rng.expovariate(1.3)
   if offset>1.8:continue
   s=arc+sign*offset;p=point(s);t=(point(s+.1)-point(s-.1)).normal()
   u=t.cross(unreal.Vector(0,0,1)).normal();v=t.cross(u).normal()
   angle=rng.gauss(*rng.choice(clusters)) if rng.random()<.72 else rng.uniform(0,math.tau)
   radial=u*math.cos(angle)+v*math.sin(angle)
   trace=unreal.SystemLibrary.line_trace_single(world,p,p+radial*650,unreal.TraceTypeQuery.ECC_VISIBILITY,True,[host,clods],unreal.DrawDebugTrace.NONE,True)
   if not trace:continue
   result=trace.to_tuple()
   if not result[0] or result[1] or result[9] not in targets[1:]:continue
   anchor,normal=result[5],result[7]
   taper=1-.15*min(1,offset/1.8);width=rng.uniform(22,34)*taper;depth=rng.uniform(16,25)*taper
   if normal.z>.55:depth=min(depth,8)
   if any(anchor.distance(q)<width+w+45 for q,w in accepted):continue
   piece=unreal.DynamicMesh()
   transform=unreal.Transform(location=anchor-center+normal*depth*.08,rotation=unreal.MathLibrary.make_rot_from_z(normal),scale=unreal.Vector(width,width*rng.uniform(.58,1.12),depth))
   unreal.GeometryScript_Primitives.append_sphere_lat_long(piece,unreal.GeometryScriptPrimitiveOptions(),transform,radius=1,steps_phi=4,steps_theta=rng.choice([6,7,8]))
   unreal.GeometryScript_Normals.set_per_face_normals(piece)
   assert unreal.GeometryScript_MeshQueries.get_is_closed_mesh(piece)
   unreal.GeometryScript_MeshEdits.append_mesh(combined,piece,unreal.Transform())
   accepted.append((anchor,width));samples.append({'position':list(anchor.to_tuple()),'normal':list(normal.to_tuple()),'width':width,'depth':depth,'side':sign,'offset_m':offset})
 assert 12<=len(samples)<=20,len(samples)
 assert all(sum(s['side']==sign for s in samples)>=6 for sign in [-1,1])
 assert unreal.MiningDesignerLibrary.set_campaign_rock_source(clods,combined)
 clods.reset_rock()
 assert levels.save_current_level()
 if ea.does_asset_exist(destination):assert ea.delete_asset(destination)
 # Remove only the withdrawn, task-created edge materials after restoring
 # every actor to its original material and saving those references.
 for name in ['MI_PigeonSoilEdge','MI_PigeonClodEdge','M_PigeonEdge']:
  path_to_remove='/Game/MainGame/Materials/DirtTransition/'+name
  if ea.does_asset_exist(path_to_remove):assert ea.delete_asset(path_to_remove)
 r.update(passed=True,backup=str(backup),material=clay.get_path_name(),clod_actor=clods.get_name(),clods=samples,center=list(center.to_tuple()),axis=list(axis.to_tuple()))
except:r['error']=traceback.format_exc();unreal.log_error(r['error'])
finally:
 (out/'authoring.json').write_text(json.dumps(r,ensure_ascii=False,indent=2),encoding='utf8');unreal.SystemLibrary.quit_editor()
