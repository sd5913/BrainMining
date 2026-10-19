"""Import the generated painting and specialize the user's CelPaint shader."""
from pathlib import Path
import json
import unreal

root=Path(unreal.Paths.project_dir())
folder='/Game/MouseMuseum'
assets=unreal.AssetToolsHelpers.get_asset_tools()
lib=unreal.MaterialEditingLibrary
task=unreal.AssetImportTask()
task.filename=str(root/'SourceAssets/MouseMuseum/T_MouseKiss.png')
task.destination_path=folder
task.destination_name='T_MouseKiss'
task.automated=True
task.replace_existing=True
task.save=True
assets.import_asset_tasks([task])
texture=unreal.load_asset(folder+'/T_MouseKiss')
assert texture
texture.set_editor_property('srgb',True)
unreal.EditorAssetLibrary.save_loaded_asset(texture)
path=folder+'/M_MouseKiss'
mat=unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else assets.create_asset('M_MouseKiss',folder,unreal.Material,unreal.MaterialFactoryNew())
lib.delete_all_material_expressions(mat)
mat.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT)
mat.set_editor_property('two_sided',True)
uv=lib.create_material_expression(mat,unreal.MaterialExpressionTextureCoordinate)
tex=lib.create_material_expression(mat,unreal.MaterialExpressionTextureSample)
tex.set_editor_property('texture',texture)
strength=lib.create_material_expression(mat,unreal.MaterialExpressionScalarParameter)
strength.set_editor_property('parameter_name','Highlight')
strength.set_editor_property('default_value',0.)
custom=lib.create_material_expression(mat,unreal.MaterialExpressionCustom)
custom.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT3)
bindings={'UV':(uv,''),'Image':(tex,'RGB'),'Highlight':(strength,'')}
inputs=[]
for name in bindings:
    i=unreal.CustomInput();i.set_editor_property('input_name',name);inputs.append(i)
custom.set_editor_property('inputs',inputs)
custom.set_editor_property('code','float r=length((UV-float2(.6374,.5656))*float2(126,168)); float ring=(1-smoothstep(.12,.4,abs(r-2.9))); return Image*.82+float3(1,1,1)*ring*Highlight*2;')
for name,(node,output) in bindings.items(): assert lib.connect_material_expressions(node,output,custom,name)
assert lib.connect_material_property(custom,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
lib.recompile_material(mat)
assert unreal.EditorAssetLibrary.save_loaded_asset(mat)

parent=unreal.load_asset('/Game/CelPaint/CelPaint_Inst')
assert parent, 'User CelPaint instance did not load'
base=unreal.load_asset('/Game/CelPaint/Main/CelPaint')
assert base.get_editor_property('material_domain')==unreal.MaterialDomain.MD_POST_PROCESS
path=folder+'/MI_EndingOilPaint'
oil=unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else assets.create_asset('MI_EndingOilPaint',folder,unreal.MaterialInstanceConstant,unreal.MaterialInstanceConstantFactoryNew())
lib.set_material_instance_parent(oil,parent)
names=[str(n) for n in lib.get_scalar_parameter_names(base)]
out=root/'Saved/Validation/MouseMuseum';out.mkdir(parents=True,exist_ok=True)
(out/'shader-parameters.json').write_text(json.dumps({'scalars':{n:lib.get_material_instance_scalar_parameter_value(parent,n) for n in names},'switches':[str(n) for n in lib.get_static_switch_parameter_names(base)],'vectors':[str(n) for n in lib.get_vector_parameter_names(base)]},indent=2),encoding='utf-8')
settings={'SampleRadius':4.,'SampleStep':2.}
for name,value in settings.items():
    assert name in names, name
    lib.set_material_instance_scalar_parameter_value(oil,name,value)
switches={'PaintAll':True,'PaintSky':True,'CelShadeAll':False,'CelShadeSky':False}
for name,value in switches.items():
    # UE 5.8's setter returns false even after applying the value; verify readback.
    lib.set_material_instance_static_switch_parameter_value(oil,name,value)
    assert lib.get_material_instance_static_switch_parameter_value(oil,name)==value, name
lib.update_material_instance(oil)
assert unreal.EditorAssetLibrary.save_loaded_asset(oil)
report={'texture':texture.get_path_name(),'paint_parent':parent.get_path_name(),'switches':{n:lib.get_material_instance_static_switch_parameter_value(oil,n) for n in switches},'scalar_parameters':{n:lib.get_material_instance_scalar_parameter_value(oil,n) for n in names},'dependencies':[str(x) for x in unreal.AssetRegistryHelpers.get_asset_registry().get_dependencies('/Game/CelPaint/Main/CelPaint',unreal.AssetRegistryDependencyOptions(include_soft_package_references=True,include_hard_package_references=True))]}
out=root/'Saved/Validation/MouseMuseum';out.mkdir(parents=True,exist_ok=True)
(out/'assets.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
unreal.log('MOUSE_MUSEUM_ASSETS_READY')
