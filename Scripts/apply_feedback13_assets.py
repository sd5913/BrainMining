"""Targeted scene edits for the October 20 feedback; never regenerate the maps."""
import unreal,json,math
from pathlib import Path
root=Path(unreal.Paths.project_dir());out=root/'Saved/Validation/Feedback13';out.mkdir(parents=True,exist_ok=True)
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);editor=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
r={}
# Credits are updated separately after the widget binding compile check.
seg=unreal.load_asset('/Game/Subtitles/DA_LeaveStartRoom');lines=list(seg.get_editor_property('lines'))
# Soil discovery is now played by the completed crossing objective.
lines=[x for x in lines if '软软的泥土' not in str(x.text)]
seg.set_editor_property('lines',lines);assert unreal.EditorAssetLibrary.save_loaded_asset(seg)
r['subtitle_lines']=[str(x.text) for x in seg.get_editor_property('lines')]
assert levels.load_level('/Game/MainGame/Maps/Lvl_MainGame');all=actors.get_all_level_actors();sealed=[]
for a in all:
 name=a.get_actor_label()
 if name in ['COL_Heaven_DiggableDomeShell','COL_Heaven_SeamCollar'] or name.startswith(('COL_Heaven_BranchFloor','COL_Heaven_BranchRoof','COL_Heaven_BranchWall')):
  a.tags=list(a.tags)+(['BM_UnmineableBoundary'] if not a.actor_has_tag('BM_UnmineableBoundary') else []);sealed.append(name)
r['protected_heaven_boundaries']=sealed;assert len(sealed)>=6
near=[]
for a in all:
 p=a.get_actor_location()
 if math.dist([p.x,p.y,p.z],[9950,6550,10000])<650:
  near.append(dict(name=a.get_actor_label(),cls=a.get_class().get_name(),pos=[p.x,p.y,p.z],tags=[str(t) for t in a.tags],components=[dict(name=c.get_name(),cls=c.get_class().get_name()) for c in a.get_components_by_class(unreal.ActorComponent)]))
r['floor_robot_nearby']=near
stage=next(a for a in all if isinstance(a,unreal.CardRoomStage));r['styled_effects']=dict(enabled=stage.get_editor_property('bStyledEffects'),systems=[x.get_path_name() if x else None for x in stage.get_editor_property('StyledSystems')])
entrance=next(a for a in all if isinstance(a,unreal.MiningGiantRoomEntrance));stream=unreal.GameplayStatics.get_streaming_level(editor.get_editor_world(),entrance.get_level().get_outer().get_path_name().split('.')[0]);assert stream;unreal.EditorLevelUtils.make_level_current(stream)
source=next(a for a in all if a.get_actor_label()=='Study V09 / Desk ruler bridge2');tick=next(a for a in all if 'Desk ruler bridge tick' in a.get_actor_label())
# A clearly readable wooden ruler leads left from the entrance to the book route.
existing=[a for a in all if a.actor_has_tag('CardRoomLeftRulerGuide')]
if not existing:
 angle=-62;rot=unreal.Rotator(yaw=angle);center=unreal.Vector(7180,3000,10036.1);axis=unreal.Vector(math.cos(math.radians(angle)),math.sin(math.radians(angle)),0);side=unreal.Vector(-axis.y,axis.x,0)
 def prop(name,pos,scale,mat):
  a=actors.spawn_actor_from_class(unreal.StaticMeshActor,pos,rot);c=a.static_mesh_component;c.set_mobility(unreal.ComponentMobility.MOVABLE);c.set_static_mesh(source.static_mesh_component.static_mesh);c.set_material(0,mat);c.set_collision_profile_name('NoCollision');a.set_actor_scale3d(scale);a.set_actor_label(name);a.tags=list(entrance.tags)+['CardRoomLeftRulerGuide'];a.set_folder_path('Study_Feedback/EntranceGuide');return a
 body=prop('入口地面 / 尺子指向左侧书本阶梯',center,unreal.Vector(9,1.2,.10),source.static_mesh_component.get_material(0))
 for i in range(1,18):
  p=center+axis*(-450+i*50)+side*36+unreal.Vector(0,0,5.8)
  prop('入口尺子 / 刻度%02d'%i,p,unreal.Vector(.035,.48 if i%5==0 else .26,.014),tick.static_mesh_component.get_material(0))
 r['ruler_created']=True
else:r['ruler_created']=False
assert unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True,True)
assert levels.load_level('/Game/EndingEmergence/Maps/Lvl_EndingEmergence_Demo');all=actors.get_all_level_actors();d=next(a for a in all if isinstance(a,unreal.EndingEmergence));set0=next(c for c in d.get_components_by_class(unreal.SceneComponent) if c.get_name().startswith('01_PlayableCellsBloodBrain'))
path='/Game/EndingEmergence/Meshes/SM_EE_SealedCellNeck';mesh=unreal.load_asset(path)
if not mesh:
 buf=unreal.GeometryScriptSimpleMeshBuffers();vs=[];uv=[];tris=[];sides=64
 def ease(x):x=max(0,min(1,x));return x*x*(3-2*x)
 x=1290;sx=x+71;n=ease((sx-1050)/550);cross=math.sqrt(1-((sx-800)/1200)**2);ry=650*cross*(1-.5*n)*1.21096;rz=570*cross*(1-.5*n)*1.158437;z=(425-212.5*n)*1.158437
 anchors=[(1290,1,z,ry*1.025,rz*1.025),(1395,0,212.5,180,180),(1460,0,212.5,190,190),(1605,0,212.5,244,214)]
 rings=[]
 for j in range(len(anchors)-1):
  a,b=anchors[j],anchors[j+1]
  for k in range(12):
   t=k/12;s=ease(t);rings.append([a[0]+(b[0]-a[0])*t]+[a[q]+(b[q]-a[q])*s for q in range(1,5)])
 rings.append(anchors[-1])
 for j,(x,y,z,ry,rz) in enumerate(rings):
  for i in range(sides+1):
   a=i*2*math.pi/sides;vs.append(unreal.Vector(x,y+ry*math.cos(a),z+rz*math.sin(a)));uv.append(unreal.Vector2D(i/sides,j/12))
 for j in range(len(rings)-1):
  for i in range(sides):
   a=j*(sides+1)+i;b=a+1;c=a+sides+1;dd=c+1;tris.extend([unreal.IntVector(a,b,c),unreal.IntVector(b,dd,c)])
 buf.vertices=vs;buf.uv0=uv;buf.triangles=tris;dm=unreal.DynamicMesh();dm.append_buffers_to_mesh(buf);dm.recompute_normals(unreal.GeometryScriptCalculateNormalsOptions())
 opt=unreal.GeometryScriptCreateNewStaticMeshAssetOptions();opt.enable_recompute_normals=True;opt.enable_recompute_tangents=True;opt.enable_collision=True;opt.collision_mode=unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE
 mesh,result=unreal.GeometryScript_NewAssetUtils.create_new_static_mesh_asset_from_mesh(dm,path,opt);assert mesh,str(result)
 mesh.set_material(0,unreal.load_asset('/Game/BloodCirculation/Materials/MaterialInstance/Cell/MI_RedBloodCell1'));assert unreal.EditorAssetLibrary.save_loaded_asset(mesh)
if not any(a.actor_has_tag('EndingSealedCellNeck') for a in all):
 a=actors.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector());c=a.static_mesh_component;c.set_mobility(unreal.ComponentMobility.MOVABLE);c.set_static_mesh(mesh);c.set_collision_profile_name('NoCollision');c.set_editor_property('cast_shadow',False);c.set_editor_property('visible_in_ray_tracing',False)
 a.attach_to_component(set0,'',unreal.AttachmentRule.KEEP_WORLD,unreal.AttachmentRule.KEEP_WORLD,unreal.AttachmentRule.KEEP_WORLD,False);a.tags=['EndingEditableScenery','EndingEditableCell','EndingSealedCellNeck'];c.component_tags=['EndingCellVisual'];a.set_actor_label('EDIT_细胞房_Demo2_出口封口膜');a.set_folder_path('Ending_Edit/01_Cell/Demo2')
assert levels.save_current_level();r['cell_exit_seal']=path
(out/'asset_changes.json').write_text(json.dumps(r,ensure_ascii=False,indent=2),encoding='utf8');unreal.SystemLibrary.quit_editor()
