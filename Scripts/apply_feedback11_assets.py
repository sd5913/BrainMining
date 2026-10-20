"""Targeted edits only; keep the authored maps and existing local changes."""
import unreal,json,math,shutil,datetime
from pathlib import Path
root=Path(unreal.Paths.project_dir());out=root/'Saved/Validation/Feedback11'
backup=root/'Saved/Backups'/('Feedback11-'+datetime.datetime.now().strftime('%Y%m%d-%H%M%S'))
backup.mkdir(parents=True)
EA=unreal.EditorAssetLibrary;ML=unreal.MaterialEditingLibrary;DL=unreal.MiningDesignerLibrary
L=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);A=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
r={}
def protect(path,ext='uasset'):
 f=root/'Content'/(path.split('.')[0][6:]+'.'+ext);b=backup/f.relative_to(root)
 if f.exists() and not b.exists():b.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(f,b)
def save(a):
 protect(a.get_path_name());assert EA.save_loaded_asset(a,False)
def box(p,size):
 m=unreal.DynamicMesh()
 unreal.GeometryScript_Primitives.append_box(m,unreal.GeometryScriptPrimitiveOptions(),unreal.Transform(location=p),dimension_x=size.x,dimension_y=size.y,dimension_z=size.z,origin=unreal.GeometryScriptPrimitiveOriginMode.CENTER)
 return m
# The authored line is currently split across two subtitle cards.
s=unreal.load_asset('/Game/Subtitles/DA_OfficeGameplay2');protect(s.get_path_name())
lines=list(s.lines);lines[0].set_editor_property('text','感觉有点太辛苦了');lines[0].set_editor_property('audio',None)
s.set_editor_property('lines',[lines[0]]);save(s);r['subtitle']=[str(x.text) for x in s.lines]
# Surrounding cells use instanced quads. Restore their original nebula shading;
# the camera-crossing fade belongs only to the central, traversed space sheets.
m=unreal.load_asset('/Game/EndingEmergence/Materials/M_EE_CellNebula');protect(m.get_path_name())
for n in ML.get_material_expressions(m):
 if isinstance(n,unreal.MaterialExpressionCustom):
  code=n.get_editor_property('code')
  code=code.replace('float nearPlaneFade=smoothstep(180,1800,abs(dot(P-Cam,normalize(N))));','').replace('*nearPlaneFade','')
  n.set_editor_property('code',code)
m.set_editor_property('two_sided',True);ML.set_material_usage(m,unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES)
ML.recompile_material(m);save(m)
# Lower the top of the museum plug. Its exposed face now lies underfoot;
# preserve the 440 cm ceiling aperture and the solid digging depth below it.
path='/Game/MainGame/Maps/Lvl_MainGame';protect(path,'umap');assert L.load_level(path)
soil=next(a for a in A.get_all_level_actors() if a.actor_has_tag('BM_MuseumDownwardSoil'))
center=unreal.MathLibrary.inverse_transform_location(soil.get_actor_transform(),unreal.Vector(47200,11750,15060))
assert DL.set_campaign_rock_source(soil,box(center,unreal.Vector(440,440,520)))
r['soil_top']=15320;r['soil_bottom']=14800
assert L.save_current_level()
# Close the rear boundary ring of the ellipsoid with the same material and UV
# convention. The forward neck and its excavatable clot remain untouched.
path='/Game/EndingEmergence/Maps/Lvl_EndingEmergence_Demo';protect(path,'umap');assert L.load_level(path)
aa=A.get_all_level_actors();shell=next(a for a in aa if 'EllipsoidalCellInterior' in a.get_actor_label() and a.actor_has_tag('EndingEditableCell'))
meshpath='/Game/EndingEmergence/Meshes/SM_EE_CellRearCap';protect(meshpath)
dm=unreal.DynamicMesh()
unreal.GeometryScript_Primitives.append_sphere_box(dm,unreal.GeometryScriptPrimitiveOptions(),unreal.Transform(location=unreal.Vector(-402,0,425),scale=unreal.Vector(8,42,38)),radius=1,steps_x=12,steps_y=12,steps_z=12)
dm.recompute_normals(unreal.GeometryScriptCalculateNormalsOptions())
cap=unreal.load_asset(meshpath)
if not cap:
 opt=unreal.GeometryScriptCreateNewStaticMeshAssetOptions();opt.enable_collision=True;opt.collision_mode=unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE
 cap,result=unreal.GeometryScript_NewAssetUtils.create_new_static_mesh_asset_from_mesh(dm,meshpath,opt);assert cap
else:unreal.GeometryScript_AssetUtils.copy_mesh_to_static_mesh(dm,cap,unreal.GeometryScriptCopyMeshToAssetOptions(),unreal.GeometryScriptMeshWriteLOD())
cap.set_material(0,shell.static_mesh_component.get_material(0));save(cap)
a=next((a for a in aa if a.actor_has_tag('EndingCellRearSeal')),None)
if not a:a=A.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector())
c=a.static_mesh_component;c.set_mobility(unreal.ComponentMobility.MOVABLE);c.set_static_mesh(cap);c.set_collision_profile_name('BlockAll');c.set_cast_shadow(False)
a.set_actor_transform(shell.get_actor_transform(),False,True)
d=next(a for a in aa if isinstance(a,unreal.EndingEmergence));parent=next(c for c in d.get_components_by_class(unreal.SceneComponent) if c.get_name().startswith('01_PlayableCellsBloodBrain'))
a.attach_to_component(parent,'',unreal.AttachmentRule.KEEP_WORLD,unreal.AttachmentRule.KEEP_WORLD,unreal.AttachmentRule.KEEP_WORLD,False)
a.tags=['EndingEditableScenery','EndingEditableCell','EndingCellRearSeal'];c.component_tags=['EndingCellVisual']
a.set_actor_label('EDIT_细胞房_Demo2_后墙封口');a.set_folder_path('Ending_Edit/01_Cell/Demo2')
assert L.save_current_level()
r['passed']=True;r['backup']=str(backup);(out/'assets.json').write_text(json.dumps(r,ensure_ascii=False,indent=2),encoding='utf8')
unreal.SystemLibrary.quit_editor()
