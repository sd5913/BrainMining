import unreal,json
from pathlib import Path
out=Path(unreal.Paths.project_dir())/'Saved/Validation/VesselDetail';out.mkdir(parents=True,exist_ok=True)
ML=unreal.MaterialEditingLibrary;m=unreal.load_asset('/Game/BloodCirculation/Materials/Materials/BloodVessel/M_Vein')
r={'textures':[],'nodes':[],'api':{}}
for name in ['get_material_property_input_node','get_material_property_input_node_output_name','get_inputs_for_material_expression','get_material_expression_input_names','get_material_expression_input_types']:
 r['api'][name]=getattr(ML,name).__doc__ if hasattr(ML,name) else None
for n in ML.get_material_expressions(m):
 row={'name':n.get_name(),'type':n.get_class().get_name()}
 if isinstance(n,unreal.MaterialExpressionTextureSample):
  t=n.get_editor_property('texture');row['texture']=t.get_path_name() if t else None
  row['sample']=str(n.get_editor_property('sampler_source'));row['mip_mode']=str(n.get_editor_property('mip_value_mode'))
  if t:r['textures'].append({'path':t.get_path_name(),**{p:str(t.get_editor_property(p)) for p in ['filter','mip_gen_settings','lod_group','lod_bias','max_texture_size','compression_settings','srgb']}})
 r['nodes'].append(row)
for name,p in [('color',unreal.MaterialProperty.MP_BASE_COLOR),('normal',unreal.MaterialProperty.MP_NORMAL),('roughness',unreal.MaterialProperty.MP_ROUGHNESS),('specular',unreal.MaterialProperty.MP_SPECULAR)]:
 n=ML.get_material_property_input_node(m,p);r[name]=n.get_name() if n else None
r['texture_filter_enum']=[n for n in dir(unreal.TextureFilter) if n.startswith('TF_')]
(out/'inspection.json').write_text(json.dumps(r,indent=2),encoding='utf8')
unreal.SystemLibrary.quit_editor()
