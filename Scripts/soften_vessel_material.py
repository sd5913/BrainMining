"""Create an ending-only material from the supplied vein, preserving source assets."""
import unreal,json,traceback
from pathlib import Path
out=Path(unreal.Paths.project_dir())/'Saved/Validation/VesselDetail';out.mkdir(parents=True,exist_ok=True)
EA=unreal.EditorAssetLibrary;ML=unreal.MaterialEditingLibrary;r={'passed':False}
try:
 source='/Game/BloodCirculation/Materials/Materials/BloodVessel/M_Vein'
 target='/Game/EndingEmergence/Materials/M_EE_SoftVein'
 instance='/Game/EndingEmergence/Materials/MI_EE_SoftVein'
 assert not EA.does_asset_exist(target),'Derived material already exists; inspect before changing'
 m=EA.duplicate_asset(source,target);assert m
 for n in ML.get_material_expressions(m):
  if isinstance(n,unreal.MaterialExpressionTextureSample):
   n.set_editor_property('mip_value_mode',unreal.TextureMipValueMode.TMVM_MIP_BIAS)
   n.set_editor_property('const_mip_value',2)
 # Soften high-frequency normal contrast without blurring cells or the camera.
 original=ML.get_material_property_input_node(m,unreal.MaterialProperty.MP_NORMAL)
 output=ML.get_material_property_input_node_output_name(m,unreal.MaterialProperty.MP_NORMAL)
 flat=ML.create_material_expression(m,unreal.MaterialExpressionConstant3Vector);flat.set_editor_property('constant',unreal.LinearColor(0,0,1))
 blend=ML.create_material_expression(m,unreal.MaterialExpressionLinearInterpolate);blend.set_editor_property('const_alpha',.72)
 assert ML.connect_material_expressions(original,output,blend,'A')
 assert ML.connect_material_expressions(flat,'',blend,'B')
 normal=ML.create_material_expression(m,unreal.MaterialExpressionNormalize);assert ML.connect_material_expressions(blend,'',normal,ML.get_material_expression_input_names(normal)[0])
 assert ML.connect_material_property(normal,'',unreal.MaterialProperty.MP_NORMAL)
 original=ML.get_material_property_input_node(m,unreal.MaterialProperty.MP_ROUGHNESS)
 output=ML.get_material_property_input_node_output_name(m,unreal.MaterialProperty.MP_ROUGHNESS)
 rough=ML.create_material_expression(m,unreal.MaterialExpressionMax);rough.set_editor_property('const_b',.8)
 assert ML.connect_material_expressions(original,output,rough,'A');assert ML.connect_material_property(rough,'',unreal.MaterialProperty.MP_ROUGHNESS)
 original=ML.get_material_property_input_node(m,unreal.MaterialProperty.MP_SPECULAR)
 output=ML.get_material_property_input_node_output_name(m,unreal.MaterialProperty.MP_SPECULAR)
 spec=ML.create_material_expression(m,unreal.MaterialExpressionMultiply);spec.set_editor_property('const_b',.15)
 assert ML.connect_material_expressions(original,output,spec,'A');assert ML.connect_material_property(spec,'',unreal.MaterialProperty.MP_SPECULAR)
 ML.recompile_material(m);assert EA.save_loaded_asset(m,False)
 mi=unreal.AssetToolsHelpers.get_asset_tools().create_asset('MI_EE_SoftVein','/Game/EndingEmergence/Materials',unreal.MaterialInstanceConstant,unreal.MaterialInstanceConstantFactoryNew())
 assert mi;ML.set_material_instance_parent(mi,m)
 old=unreal.load_asset('/Game/BloodCirculation/Materials/MaterialInstance/BloodVessel/MI_Vein')
 for name in ML.get_scalar_parameter_names(old):ML.set_material_instance_scalar_parameter_value(mi,name,ML.get_material_instance_scalar_parameter_value(old,name))
 ML.update_material_instance(mi);assert EA.save_loaded_asset(mi,False)
 r.update(passed=True,material=target,instance=instance,mip_bias=2,normal_strength=.28,minimum_roughness=.8,specular_multiplier=.15)
except Exception:r['error']=traceback.format_exc();unreal.log_error(r['error'])
(out/'material.json').write_text(json.dumps(r,indent=2),encoding='utf8');unreal.SystemLibrary.quit_editor()
