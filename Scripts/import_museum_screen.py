"""Replace the blockout screen with Poly Haven's scanned antique panels."""
import unreal,json,pathlib,shutil
root=pathlib.Path(unreal.Paths.project_dir());out=root/'Saved/Validation/MuseumOutline';out.mkdir(parents=True,exist_ok=True)
assert not unreal.EditorLevelLibrary.get_pie_worlds(False)
assert unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True,True)
src=root/'Content/WenSi/Maps/Lvl_WenSi_MirrorMuseum1.umap'
if not (out/src.name).exists():shutil.copy2(src,out/src.name)
assets=unreal.AssetToolsHelpers.get_asset_tools();lib=unreal.MaterialEditingLibrary;folder='/Game/WenSi/Museum/Refresh'
unreal.SystemLibrary.execute_console_command(None,'Interchange.FeatureFlags.Import.FBX 0')
downloads=json.loads((root/'SourceAssets/MuseumRefresh/chinese_screen_panels/downloads.json').read_text())
def imp(kind,name,sub):
 path=folder+'/'+sub+'/'+name
 if unreal.EditorAssetLibrary.does_asset_exist(path):return unreal.load_asset(path)
 source=next(d['path'] for d in downloads if d['kind']==kind)
 t=unreal.AssetImportTask();t.filename=str(root/source);t.destination_path=folder+'/'+sub;t.destination_name=name;t.automated=True;t.save=True
 if kind=='model':
  opt=unreal.FbxImportUI();opt.automated_import_should_detect_type=False;opt.mesh_type_to_import=unreal.FBXImportType.FBXIT_STATIC_MESH;opt.original_import_type=unreal.FBXImportType.FBXIT_STATIC_MESH
  opt.import_mesh=True;opt.import_materials=False;opt.import_textures=False;opt.import_as_skeletal=False;opt.import_animations=False
  opt.static_mesh_import_data.combine_meshes=True;opt.static_mesh_import_data.auto_generate_collision=False
  opt.static_mesh_import_data.normal_import_method=unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS_AND_TANGENTS;t.options=opt
 assets.import_asset_tasks([t]);obj=unreal.load_asset(path);assert obj,path
 if kind!='model':
  obj.set_editor_property('srgb',kind=='base');obj.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_NORMALMAP if kind=='normal' else unreal.TextureCompressionSettings.TC_MASKS if kind=='orm' else unreal.TextureCompressionSettings.TC_DEFAULT)
  assert unreal.EditorAssetLibrary.save_loaded_asset(obj)
 return obj
mesh=imp('model','SM_AntiqueScreen','Meshes')
path=folder+'/Materials/MI_AntiqueScreen'
m=unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else assets.create_asset('MI_AntiqueScreen',folder+'/Materials',unreal.MaterialInstanceConstant,unreal.MaterialInstanceConstantFactoryNew())
lib.set_material_instance_parent(m,unreal.load_asset('/Game/Museum/Materials/M_PBRMaster'))
for kind,param in [('base','BC'),('normal','Normal'),('orm','ORM')]:lib.set_material_instance_texture_parameter_value(m,param,imp(kind,'T_AntiqueScreen_'+param,'Textures'))
for switch in ['Flat Color','Solid Metalness','Mask Based Roughness','Solid Emissive','Solid Roughness','Mask Based Color','Toggle | World Space']:lib.set_material_instance_static_switch_parameter_value(m,switch,False)
lib.set_material_instance_vector_parameter_value(m,'Color',unreal.LinearColor(1,1,1,1));lib.set_material_instance_scalar_parameter_value(m,'Roughness',1);lib.set_material_instance_scalar_parameter_value(m,'EmissiveLevel',0)
lib.update_material_instance(m);assert unreal.EditorAssetLibrary.save_loaded_asset(m)
for i in range(len(mesh.static_materials)):mesh.set_material(i,m)
assert unreal.EditorAssetLibrary.save_loaded_asset(mesh)
# Existing translucent parent, with depth writing enabled for passable exhibits.
ghostpath='/Game/WenSi/Museum/Materials/M_MuseumPassable'
if not unreal.EditorAssetLibrary.does_asset_exist(ghostpath):assert unreal.EditorAssetLibrary.duplicate_asset('/Engine/EngineDebugMaterials/M_SimpleTranslucent',ghostpath)
ghost=unreal.load_asset(ghostpath);ghost.set_editor_property('allow_translucent_custom_depth_writes',True);ghost.set_editor_property('opacity_mask_clip_value',.05)
lib.recompile_material(ghost);assert unreal.EditorAssetLibrary.save_loaded_asset(ghost)
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
screens=[a for a in actors if isinstance(a,unreal.WenSiMirrorExhibit) and a.get_editor_property('kind')==5 and '/Game/WenSi/Maps/Lvl_WenSi_MirrorMuseum1.' in a.get_path_name()]
assert len(screens)==1
for a in screens:
 a.modify();a.set_editor_property('art_model',mesh);a.set_editor_property('art_rotation',unreal.Rotator(pitch=0,yaw=90,roll=0));a.set_editor_property('uniform_art_scale',False);a.refresh_art_model()
# Hard references on an unused proxy slot; visible authored materials stay intact.
unused=next(c for c in screens[0].get_components_by_class(unreal.StaticMeshComponent) if c.get_name()=='Part11')
unused.set_material(0,ghost)
unused.set_material(1,unreal.load_asset('/Game/CollectionPrototype/Materials/M_CollectionSilhouette'))
assert unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True,True)
(out/'imported.json').write_text(json.dumps({'screen':mesh.get_path_name(),'bounds':str(mesh.get_bounds()),'actor':screens[0].get_path_name(),'ghost':ghost.get_path_name()},indent=2))
print('ANTIQUE_SCREEN_IMPORTED',str(mesh.get_bounds()))

