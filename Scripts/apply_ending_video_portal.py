"""Build the ending-only video aperture and update the scale overlap."""
from pathlib import Path
import unreal

project = Path(unreal.Paths.project_dir())
exec(compile((project/'Scripts/apply_ending_scale_zoom.py').read_text(encoding='utf-8-sig'), 'scale_zoom', 'exec'))
lib = unreal.MaterialEditingLibrary
folder = '/Game/EndingEmergence/Materials'
path = folder+'/M_EE_VideoPortal'
mat = unreal.load_asset(path) or unreal.AssetToolsHelpers.get_asset_tools().create_asset('M_EE_VideoPortal',folder,unreal.Material,unreal.MaterialFactoryNew())
lib.delete_all_material_expressions(mat)
mat.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT)
mat.set_editor_property('two_sided',True)
mat.set_editor_property('blend_mode',unreal.BlendMode.BLEND_OPAQUE)
bindings={
 'UV':(lib.create_material_expression(mat,unreal.MaterialExpressionScreenPosition),'ViewportUV')}
tex=lib.create_material_expression(mat,unreal.MaterialExpressionTextureObjectParameter)
tex.set_editor_property('parameter_name','VideoFrame')
tex.set_editor_property('texture',unreal.load_asset('/Game/EndingEmergence/PaperFinale/T_EndingFinaleFirstFrame') or unreal.load_asset('/Game/EndingEmergence/PaperFinale/T_EE_DeveloperCredits'))
bindings['VideoFrame']=(tex,'')
still=lib.create_material_expression(mat,unreal.MaterialExpressionTextureObjectParameter)
still.set_editor_property('parameter_name','FirstFrame')
still.set_editor_property('texture',unreal.load_asset('/Game/EndingEmergence/PaperFinale/T_EndingFinaleFirstFrame'))
bindings['FirstFrame']=(still,'')
for name,value in [('VideoAspect',1.8),('ViewportAspect',16/9),('VideoBlend',0.)]:
 node=lib.create_material_expression(mat,unreal.MaterialExpressionScalarParameter)
 node.set_editor_property('parameter_name',name);node.set_editor_property('default_value',value);bindings[name]=(node,'')
custom=lib.create_material_expression(mat,unreal.MaterialExpressionCustom)
custom.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT3)
inputs=[]
for key,(node,pin) in bindings.items():
 item=unreal.CustomInput();item.set_editor_property('input_name',key);inputs.append(item)
custom.set_editor_property('inputs',inputs)
custom.set_editor_property('code',r'''
// Screen projection continues unchanged after the physical rim leaves view.
// Fit, never stretch, videos whose aspect differs from the viewport.
float2 q=UV;
q=(q-.5)*float2(max(1,ViewportAspect/VideoAspect),max(1,VideoAspect/ViewportAspect))+.5;
float valid=step(0,q.x)*step(q.x,1)*step(0,q.y)*step(q.y,1);
float3 still=Texture2DSample(FirstFrame,FirstFrameSampler,saturate(q)).rgb;
float3 movie=Texture2DSample(VideoFrame,VideoFrameSampler,saturate(q)).rgb;
float3 colour=lerp(still,movie,saturate(VideoBlend))*valid;
return colour;
''')
for key,(node,pin) in bindings.items():assert lib.connect_material_expressions(node,pin,custom,key)
assert lib.connect_material_property(custom,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
lib.recompile_material(mat)
assert unreal.EditorAssetLibrary.save_loaded_asset(mat)
unreal.log('ENDING_VIDEO_PORTAL_READY')
