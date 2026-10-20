import unreal,json,shutil,sys
from pathlib import Path
root=Path(unreal.Paths.project_dir());sys.path.insert(0,str(root/'Scripts'))
from scene_polish_materials import mat,node,prop,finish
out=root/'Saved/Validation/Playtest16';out.mkdir(parents=True,exist_ok=True)
L=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);A=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
EA=unreal.EditorAssetLibrary;DL=unreal.MiningDesignerLibrary;I=unreal.Transform()
report={}
def backup(path,ext='umap'):
 f=root/'Content'/(path[6:]+'.'+ext);dest=out/'Before'/f.relative_to(root)
 if f.exists() and not dest.exists():dest.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(f,dest)
def load(path):backup(path);assert L.load_level(path);return list(A.get_all_level_actors())
def sphere(center,radius):
 dm=unreal.DynamicMesh()
 unreal.GeometryScript_Primitives.append_sphere_box(dm,unreal.GeometryScriptPrimitiveOptions(),unreal.Transform(location=center),radius=radius,steps_x=8,steps_y=8,steps_z=8)
 return dm
def subtract(a,frame,b,bframe=I):
 unreal.GeometryScript_MeshBooleans.apply_mesh_boolean(a,frame,b,bframe,unreal.GeometryScriptBooleanOperation.SUBTRACT,unreal.GeometryScriptMeshBooleanOptions(fill_holes=True,simplify_output=False))
def actor(label,cls,pos):
 a=next((a for a in A.get_all_level_actors() if a.get_actor_label()==label),None) or A.spawn_actor_from_class(cls,pos)
 a.set_actor_label(label);return a
# Reuse the authored pear with a concave bite and pale fruit flesh.
path='/Game/StartRoom/assets/SM_BittenPear';pear=unreal.load_asset(path) if EA.does_asset_exist(path) else None
if not pear:
 original=unreal.load_asset('/Game/Countryside_Interior/Meshes/SM_Food_SM_Pear')
 materials=[s.material_interface for s in original.get_editor_property('static_materials')]
 flesh=mat('/Game/StartRoom/assets/M_PearFlesh')
 prop(node(flesh,unreal.MaterialExpressionConstant3Vector,constant=unreal.LinearColor(.83,.70,.39)),unreal.MaterialProperty.MP_BASE_COLOR)
 prop(node(flesh,unreal.MaterialExpressionConstant,r=.85),unreal.MaterialProperty.MP_ROUGHNESS);finish(flesh)
 dm=unreal.DynamicMesh();unreal.GeometryScript_AssetUtils.copy_mesh_from_static_mesh(original,dm,unreal.GeometryScriptCopyMeshFromAssetOptions(),unreal.GeometryScriptMeshReadLOD())
 for center,radius in [(unreal.Vector(2.6,-4.0,7.2),3.4),(unreal.Vector(.5,-4.3,8.8),2.2)]:
  bite=sphere(center,radius)
  unreal.GeometryScript_Materials.clear_material_i_ds(bite,len(materials))
  subtract(dm,I,bite)
 dm.recompute_normals(unreal.GeometryScriptCalculateNormalsOptions())
 opt=unreal.GeometryScriptCreateNewStaticMeshAssetOptions();opt.enable_collision=True;opt.collision_mode=unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE
 pear,result=unreal.GeometryScript_NewAssetUtils.create_new_static_mesh_asset_from_mesh(dm,path,opt);assert pear
 for index,m in enumerate(materials+[flesh]):pear.set_material(index,m)
 assert EA.save_loaded_asset(pear,False)
aa=load('/Game/StartRoom/Maps/Lvl_StartRoom')
plant=next(a for a in aa if a.get_actor_label()=='Circle')
plant.tags=list(plant.tags)+([] if plant.actor_has_tag('StartRoomView_Plant') else ['StartRoomView_Plant'])
plant.set_actor_enable_collision(True)
for c in plant.get_components_by_class(unreal.PrimitiveComponent):
 c.set_collision_profile_name('BlockAll');c.set_collision_enabled(unreal.CollisionEnabled.QUERY_ONLY)
# The cabinet is on the north wall; its left compartment is visible beside the open door.
wardrobe=next(a for a in aa if a.get_actor_label()=='ancient_Chinese_wardrobe')
p=wardrobe.get_actor_location()+unreal.Vector(33,-3,-18)
a=actor('STARTROOM_BittenPear_View',unreal.StaticMeshActor,p);a.tags=['StartRoomView_Pear']
c=a.static_mesh_component;c.set_static_mesh(pear);c.set_mobility(unreal.ComponentMobility.MOVABLE)
c.set_collision_profile_name('BlockAll');a.set_actor_scale3d(unreal.Vector(1.45,1.45,1.45));a.set_actor_rotation(unreal.Rotator(yaw=-45),False)
center,extent=a.get_actor_bounds(False);location=a.get_actor_location();location.z+=75.8572-(center.z-extent.z);a.set_actor_location(location,False,True)
fill=actor('STARTROOM_CabinetSoftFill',unreal.PointLight,location+unreal.Vector(-10,-20,25))
fill.set_actor_location(location+unreal.Vector(-10,-20,25),False,True)
c=fill.point_light_component;c.set_mobility(unreal.ComponentMobility.MOVABLE);c.set_intensity(.08);c.set_attenuation_radius(60);c.set_cast_shadows(False);c.set_light_color(unreal.LinearColor(1,.85,.63))
report['pear_position']=a.get_actor_location().to_tuple();report['plant']=plant.get_path_name()
assert L.save_current_level()
# Card reward follows the same editable anchor used by its interaction.
aa=load('/Game/Passages/Echo/Maps/Lvl_EchoMazeToCards_P2')
stage=next(a for a in aa if isinstance(a,unreal.CardRoomStage))
p=stage.reward_local;p.z=255;stage.reward_local=p
report['reward_local']=p.to_tuple();assert L.save_current_level()
aa=load('/Game/MainGame/Maps/Lvl_MainGame')
# Mushroom ambience is authored by apply_mushroom_niagara.py.
# Open only the overlap between the new heaven boundary and the main passage.
points=json.loads((root/'Art/MainGame/RoutesV5/TunnelStalagmiteV3/centerlines.json').read_text())[0]['points']
origin=unreal.Vector(39750,11750,13900)
points=[origin+unreal.Vector(p[0]*100,-p[1]*100,p[2]*100) for p in points]
points=[p for p in points[::8] if p.x<39600]
report['cleared_boundary']=[]
for a in aa:
 if a.get_actor_label() not in ['COL_Heaven_OuterUnmineableBoundary','COL_Heaven_OuterDiggableBuffer']:continue
 source=DL.get_campaign_rock_source(a)
 for p in points:subtract(source,a.get_actor_transform(),sphere(p,850))
 if isinstance(a,unreal.GeneratedMiningRock):assert DL.set_campaign_rock_source(a,source)
 else:
  c=a.get_component_by_class(unreal.DynamicMeshComponent);c.get_dynamic_mesh().reset()
  unreal.GeometryScript_MeshEdits.append_mesh(c.get_dynamic_mesh(),source,I);c.update_collision(False)
 report['cleared_boundary'].append(a.get_actor_label())
assert L.save_current_level()
aa=load('/Game/PopupClose/Maps/Lvl_PopupClose1')
report['office_walls']=[]
for a in aa:
 p,e=a.get_actor_bounds(False)
 if abs(p.x+713)<e.x+60 and isinstance(a,unreal.StaticMeshActor):
  report['office_walls'].append(dict(label=a.get_actor_label(),name=a.get_name(),pos=a.get_actor_location().to_tuple(),center=p.to_tuple(),extent=e.to_tuple(),mesh=str(a.static_mesh_component.static_mesh)))
cap=next(a for a in aa if a.get_actor_label()=='POLISH_Office_PartitionFront')
cap.set_actor_location(unreal.Vector(-712.825475,-418.4408,184.93),False,True);cap.set_actor_scale3d(unreal.Vector(.30,.10,3.9386))
assert L.save_current_level()
report['passed']=True
(out/'scene.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
unreal.SystemLibrary.quit_editor()
