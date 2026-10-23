"""Authored bomb stencils, pigeon destination, stone boundary, smooth gem halo and task audio."""
import unreal,json,shutil,sys,math
from pathlib import Path
root=Path(unreal.Paths.project_dir());out=root/'Saved/Validation/Polish32';sys.path.insert(0,str(root/'Scripts'))
from scene_polish_materials import node,wire,prop,custom,finish
EA=unreal.EditorAssetLibrary;ML=unreal.MaterialEditingLibrary
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);api=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
r={'signs':[]}
def backup(path,ext='uasset'):
 rel=Path('Content')/(path.removeprefix('/Game/')+'.'+ext);src=root/rel;dst=out/'Before'/rel
 if src.exists() and not dst.exists():dst.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(src,dst)
def setp(o,k,v):o.set_editor_property(k,v,notify_mode=unreal.PropertyAccessChangeNotifyMode.NEVER)
def vec(v):return list(v.to_tuple())
# Import the user's original recording directly; UE's SoundFactory supports MP3.
audio='/Game/Audio/UI/S_TaskComplete';backup(audio)
task=unreal.AssetImportTask();task.filename='C:/Users/abao/Downloads/sound-when-something-is-done-on-the-computer-informative.mp3';task.destination_path='/Game/Audio/UI';task.destination_name='S_TaskComplete';task.automated=True;task.replace_existing=True;task.save=True
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task]);assert EA.does_asset_exist(audio)
sound=unreal.load_asset(audio);sound.set_editor_property('loading_behavior',unreal.SoundWaveLoadingBehavior.FORCE_INLINE);sound.set_editor_property('sound_class_object',unreal.load_asset('/Game/Audio/Mix/SC_Effects'));EA.save_loaded_asset(sound,False)
subtitle='/Game/Subtitles/Collectibles/DA_CollectiblePickup_00';backup(subtitle)
asset=unreal.load_asset(subtitle);lines=list(asset.lines);lines[0].set_editor_property('text',str(lines[0].text).replace('不太优雅的想法','有点味道的想法'));asset.set_editor_property('lines',lines);assert EA.save_loaded_asset(asset,False)
# Matte charcoal stone: world-space irregular mineral strata and pitted normals.
path='/Game/MainGame/Materials/M_BedrockDark';backup(path);m=unreal.load_asset(path);ML.delete_all_material_expressions(m);m.set_editor_property('tangent_space_normal',False)
p=node(m,unreal.MaterialExpressionWorldPosition);n=node(m,unreal.MaterialExpressionVertexNormalWS)
color=custom(m,'''
float3 q=P*.018;
float strata=sin(q.z*1.8+sin(q.x*.7)+sin(q.y*.9)*1.7);
float grains=sin(dot(q,float3(3.1,2.7,1.3))+sin(q.z*4.1))*sin(dot(q,float3(-2.3,1.9,3.7)));
float seams=1-smoothstep(.035,.13,abs(strata+.20*grains));
float flecks=.5+.5*sin(dot(P,float3(.43,.31,.57))+sin(P.x*.69));
return lerp(float3(.025,.031,.040),float3(.067,.075,.086),saturate(.48+.26*grains+.12*flecks))*(1-.6*seams);
''',{'P':(p,'XYZ')});prop(color,unreal.MaterialProperty.MP_BASE_COLOR)
normal=custom(m,'float3 g=.20*cos(P*.16+sin(P.yzx*.097))+.08*cos(P*.79);g-=N*dot(g,N);return normalize(N-g);',{'P':(p,'XYZ'),'N':(n,'')});prop(normal,unreal.MaterialProperty.MP_NORMAL)
prop(node(m,unreal.MaterialExpressionConstant,r=.95),unreal.MaterialProperty.MP_ROUGHNESS);prop(node(m,unreal.MaterialExpressionConstant,r=.13),unreal.MaterialProperty.MP_SPECULAR);finish(m)
# Keep the worn stencil readable in unlit corners without a solid backing plate.
path='/Game/MainGame/Materials/M_BombWallIcon';backup(path);m=unreal.load_asset(path)
for expression in list(unreal.ObjectIterator(unreal.MaterialExpressionConstant3Vector)):
 if expression.get_outer()==m and expression.get_editor_property('desc')=='BM_StencilLowLight':ML.delete_material_expression(m,expression)
emission=node(m,unreal.MaterialExpressionConstant3Vector,constant=unreal.LinearColor(.12,.13,.14),desc='BM_StencilLowLight');prop(emission,unreal.MaterialProperty.MP_EMISSIVE_COLOR);finish(m)
# The old two multiplied sine waves are visibly periodic/checkered, not a missing texture.
path='/Game/MainGame/Materials/M_BuriedMineralGlow';backup(path);m=unreal.load_asset(path)
for o in unreal.ObjectIterator(unreal.MaterialExpressionCustom):
 if o.get_outer()==m and 'float grain=' in o.get_editor_property('code'):
  o.set_editor_property('code','float d=length(P-Center)/max(Radius,1.0);float h=1.0-smoothstep(.08,1.0,d);return saturate(Amount)*(.62*h*h*h*h+.025*h*h);')
finish(m)
backup('/Game/MainGame/Maps/Lvl_MainGame','umap');assert levels.load_level('/Game/MainGame/Maps/Lvl_MainGame');levels.set_current_level_by_name('Lvl_MainGame')
aa=list(api.get_all_level_actors())
for a in aa:
 if a.actor_has_tag('BM_Polish32_BombSign'):api.destroy_actor(a)
material=unreal.load_asset('/Game/MainGame/Materials/M_BombWallIcon')
walls={}
for a in aa:
 if not a.actor_has_tag('BM_BombOnly'):continue
 dm=unreal.MiningDesignerLibrary.get_campaign_rock_source(a);tris=[];faces=[]
 for i in range(dm.get_triangle_count()):
  valid,v1,v2,v3=unreal.GeometryScript_MeshQueries.get_triangle_positions(dm,i)
  if not valid:continue
  cross=(v2-v1).cross(v3-v1);area=cross.length()
  if area<.01:continue
  normal=cross/area;center=(v1+v2+v3)/3
  tris.append((area,normal,center));faces.append((v1,v2,v3))
 assert tris,a.get_actor_label()
 axis=max(tris,key=lambda t:t[0])[1]
 if axis.x>0:axis=-axis
 planes=[]
 for side in [1,-1]:
  n=axis*side;surface=[t for t in tris if t[1].dot(n)>.97]
  assert surface
  total=sum(t[0] for t in surface);center=sum((t[2]*t[0] for t in surface),unreal.Vector())/total
  # Mesh triangle winding may be reversed. Choose the extreme plane in this direction.
  allproj=[t[2].dot(n) for t in tris];center+=n*(max(allproj)-center.dot(n))
  world=unreal.MathLibrary.transform_location(a.get_actor_transform(),center)
  normal=unreal.MathLibrary.transform_direction(a.get_actor_transform(),n)
  # Snap the projector to the actual center-face intersection, not an extreme
  # vertex plane: bevelled plugs can put that plane hundreds of cm in front.
  direction=unreal.Vector(-normal.x,-normal.y,-normal.z);origin=world+normal*400;hits=[]
  for vertices in faces:
   v0,v1,v2=[unreal.MathLibrary.transform_location(a.get_actor_transform(),v) for v in vertices]
   e1=v1-v0;e2=v2-v0;h=direction.cross(e2);det=e1.dot(h)
   if abs(det)<.00001:continue
   inv=1/det;delta=origin-v0;u=inv*delta.dot(h)
   if u<0 or u>1:continue
   cross=delta.cross(e1);v=inv*direction.dot(cross)
   if v<0 or u+v>1:continue
   distance=inv*e2.dot(cross)
   if distance>=0:hits.append(distance)
  assert hits,a.get_actor_label()
  projector=origin+direction*min(hits)+normal*3
  decal=api.spawn_actor_from_class(unreal.DecalActor,projector,unreal.MathLibrary.make_rot_from_xy(unreal.Vector(-normal.x,-normal.y,-normal.z),unreal.Vector(0,0,-1)))
  decal.set_actor_label('MAIN_GAME_BombStencil_'+a.get_name()+('_Front' if side==1 else '_Back'))
  decal.decal.set_decal_material(material);setp(decal.decal,'decal_size',unreal.Vector(60,135,135));decal.decal.set_fade_screen_size(.001)
  setp(decal,'tags',['BM_Polish32_BombSign']+[str(t) for t in a.tags if str(t).startswith('BM_Campaign_')])
  decal.attach_to_actor(a,'',unreal.AttachmentRule.KEEP_WORLD,unreal.AttachmentRule.KEEP_WORLD,unreal.AttachmentRule.KEEP_WORLD,False)
  planes.append((world,normal));r['signs'].append(dict(wall=a.get_actor_label(),position=vec(world),normal=vec(normal)))
 walls[a.get_actor_label()]=(a,planes)
field=next(a for a in aa if isinstance(a,unreal.MiningCollectionField) and a.actor_has_tag('BM_MainGame_Collections'))
placements=list(field.authored_placements);bird=next(p for p in placements if p.item_id==3)
gate,planes=walls['MAIN_GAME_BombOnly_2_7'];front,n=planes[0];back=planes[1][0]
facing=unreal.MathLibrary.make_rot_from_x(unreal.Vector(-n.x,-n.y,-n.z))
body=back-n*90;clue=front+n*7+unreal.Vector(0,0,35)
setp(bird,'transform',unreal.Transform(location=body,rotation=facing));setp(bird,'clue_transform',unreal.Transform(location=clue,rotation=facing));setp(bird,'approach_transform',unreal.Transform(location=front+n*300,rotation=facing));setp(bird,'host_rock',gate)
setp(field,'authored_placements',placements);r['pigeon']=dict(body=vec(body),clue=vec(clue),approach=vec(front+n*300),host=gate.get_path_name())
assert levels.save_current_level();(out/'scene-changes.json').write_text(json.dumps(r,ensure_ascii=False,indent=2),encoding='utf8')
unreal.SystemLibrary.quit_editor()
