"""Echo visual v0.2: black surfaces, white impact ripples, delayed contour reveal."""
import unreal
ROOT='/Game/Passages/Echo/Materials'
assets=unreal.AssetToolsHelpers.get_asset_tools();lib=unreal.MaterialEditingLibrary
path=ROOT+'/M_EchoContour'
m=unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else assets.create_asset('M_EchoContour',ROOT,unreal.Material,unreal.MaterialFactoryNew())
lib.delete_all_material_expressions(m)
m.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT)
custom=lib.create_material_expression(m,unreal.MaterialExpressionCustom)
inputs={};wp=lib.create_material_expression(m,unreal.MaterialExpressionWorldPosition);inputs['Position']=wp
for name,value in [('Now',0),('Speed',1000),('Fade',2.6),('Strength',1),('Preview',1),('SurfaceMode',0)]+[(f'Start{i}',-1000) for i in range(4)]+[(f'Radius{i}',1550) for i in range(4)]:
    e=lib.create_material_expression(m,unreal.MaterialExpressionScalarParameter);e.set_editor_property('parameter_name',name);e.set_editor_property('default_value',value);inputs[name]=e
for i in range(4):
    name=f'Origin{i}';e=lib.create_material_expression(m,unreal.MaterialExpressionVectorParameter);e.set_editor_property('parameter_name',name);inputs[name]=e
ins=[]
for name in inputs:
    inp=unreal.CustomInput();inp.set_editor_property('input_name',name);ins.append(inp)
custom.set_editor_property('inputs',ins);custom.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT3)
custom.set_editor_property('code',"""float3 origins[4]={Origin0,Origin1,Origin2,Origin3};
float starts[4]={Start0,Start1,Start2,Start3};
float radii[4]={Radius0,Radius1,Radius2,Radius3};
float contour=Preview;
float ripple=0;
for(int i=0;i<4;i++) {
 float d=length(Position-origins[i]);
 float t=Now-starts[i];
 float age=t-d/max(Speed,1.0);
 float rangeFade=pow(saturate(1-d/max(radii[i],1.0)),1.35);
 if(t>=0 && d<radii[i]) {
  // Two narrow rings expand from the actual impact. No diffuse surface illumination.
  float front=1-smoothstep(4.0,12.0,abs(d-t*Speed));
  float trailing=1-smoothstep(3.0,8.0,abs(d-max(0.0,t-.15)*Speed));
  ripple=max(ripple,(front+trailing*.45*step(.15,t))*rangeFade);
  // Contours appear behind the leading circle and fade with distance AND time.
  if(age>=0 && age<Fade) {
   float arrival=smoothstep(0.0,.06,age);
   float residual=pow(saturate(1-age/max(Fade,.01)),2)*rangeFade;
   contour=max(contour,arrival*residual);
  }
 }
}
float value=lerp(contour*2.8,ripple*3.5,saturate(SurfaceMode))*Strength;
return float3(value,value,value);""")
for name,e in inputs.items():lib.connect_material_expressions(e,'',custom,name)
lib.connect_material_property(custom,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
lib.recompile_material(m);unreal.EditorAssetLibrary.save_loaded_asset(m)
unreal.log('ECHO_PASSAGE_WHITE_RIPPLE_MATERIAL_SAVED')
