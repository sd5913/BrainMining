import unreal,pathlib
root=pathlib.Path(unreal.Paths.project_dir());src=root/'Art/Office/Popups';folder='/Game/PopupClose/PopupArt';tools=unreal.AssetToolsHelpers.get_asset_tools();lib=unreal.MaterialEditingLibrary;assets=unreal.EditorAssetLibrary
for name in ['Window','SystemMessage','SystemError','Robot','Message','Loading']:
 path=folder+'/T_Popup'+name;tex=unreal.load_asset(path)
 if not tex:
  t=unreal.AssetImportTask();t.filename=str(src/('T_Popup'+name+'.png'));t.destination_path=folder;t.destination_name='T_Popup'+name;t.automated=True;t.save=True;tools.import_asset_tasks([t]);tex=unreal.load_asset(path)
 tex.set_editor_property('srgb',True);tex.set_editor_property('lod_group',unreal.TextureGroup.TEXTUREGROUP_UI);tex.set_editor_property('never_stream',True);tex.set_editor_property('max_texture_size',2048);assets.save_loaded_asset(tex,False)
 m=unreal.load_asset(folder+'/M_Popup'+name)
 physical_tex=tex
 if name in ['Window','SystemMessage','SystemError']:
  physical_tex=unreal.load_asset(folder+'/T_PhysicalPopup'+name)
  if not physical_tex:
   t=unreal.AssetImportTask();t.filename=str(src/('T_PhysicalPopup'+name+'.png'));t.destination_path=folder;t.destination_name='T_PhysicalPopup'+name;t.automated=True;t.save=True;tools.import_asset_tasks([t]);physical_tex=unreal.load_asset(folder+'/T_PhysicalPopup'+name)
   physical_tex.set_editor_property('srgb',True);physical_tex.set_editor_property('max_texture_size',2048);assets.save_loaded_asset(physical_tex,False)
 if not m:
  m=tools.create_asset('M_Popup'+name,folder,unreal.Material,unreal.MaterialFactoryNew());m.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT);m.set_editor_property('two_sided',True)
  world=lib.create_material_expression(m,unreal.MaterialExpressionWorldPosition)
  local=lib.create_material_expression(m,unreal.MaterialExpressionTransformPosition);local.set_editor_property('transform_source_type',unreal.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_WORLD);local.set_editor_property('transform_type',unreal.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_LOCAL);lib.connect_material_expressions(world,'',local,'Input')
  uv=lib.create_material_expression(m,unreal.MaterialExpressionCustom);uv.set_editor_property('code','return float2(0.5 + P.y * 0.01, 0.5 - P.z * 0.01);');uv.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT2);pin=unreal.CustomInput();pin.set_editor_property('input_name','P');uv.set_editor_property('inputs',[pin]);lib.connect_material_expressions(local,'',uv,'P')
  sample=lib.create_material_expression(m,unreal.MaterialExpressionTextureSampleParameter2D);sample.set_editor_property('parameter_name','PopupArtwork');sample.set_editor_property('texture',tex);sample.set_editor_property('sampler_type',unreal.MaterialSamplerType.SAMPLERTYPE_COLOR);lib.connect_material_expressions(uv,'',sample,'Coordinates')
  mult=lib.create_material_expression(m,unreal.MaterialExpressionMultiply);mult.set_editor_property('const_b',.04);lib.connect_material_expressions(sample,'RGB',mult,'A');lib.connect_material_property(mult,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR);lib.recompile_material(m);assets.save_loaded_asset(m,False)
 # Physical popup slabs receive the room's lighting like paper; a tiny emission keeps the pixels readable.
 m.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
 m.set_editor_property('used_with_instanced_static_meshes',True)
 sample=next(n for n in unreal.ObjectIterator(unreal.MaterialExpressionTextureSampleParameter2D) if n.get_outer()==m)
 sample.set_editor_property('texture',physical_tex)
 lib.connect_material_property(sample,'RGB',unreal.MaterialProperty.MP_BASE_COLOR)
 for n in unreal.ObjectIterator(unreal.MaterialExpressionMultiply):
  if n.get_outer()==m:n.set_editor_property('const_b',.04)
 if not lib.get_material_property_input_node(m,unreal.MaterialProperty.MP_ROUGHNESS):
  rough=lib.create_material_expression(m,unreal.MaterialExpressionConstant);rough.set_editor_property('r',.88);lib.connect_material_property(rough,'',unreal.MaterialProperty.MP_ROUGHNESS)
 lib.recompile_material(m);assets.save_loaded_asset(m,False)
warning=unreal.load_asset(folder+'/T_PendingWarning')
if not warning:
 t=unreal.AssetImportTask();t.filename=str(src/'T_PendingWarning.png');t.destination_path=folder;t.destination_name='T_PendingWarning';t.automated=True;t.save=True;tools.import_asset_tasks([t]);warning=unreal.load_asset(folder+'/T_PendingWarning')
warning.set_editor_property('lod_group',unreal.TextureGroup.TEXTUREGROUP_UI)
warning.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_EDITOR_ICON)
warning.set_editor_property('mip_gen_settings',unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
warning.set_editor_property('never_stream',True);assets.save_loaded_asset(warning,False)
print('IMPORTED_POPUP_SKINS 6 UI textures + 6 physical materials + warning')
