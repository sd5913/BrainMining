import unreal
MAP='/Game/CollectionPrototype/Maps/Lvl_Collection_V03'
MAT='/Game/CollectionPrototype/Materials'
lib=unreal.MaterialEditingLibrary
assets=unreal.AssetToolsHelpers.get_asset_tools()
path=MAT+'/M_CollectionStainedEarth'
m=unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else assets.create_asset('M_CollectionStainedEarth',MAT,unreal.Material,unreal.MaterialFactoryNew())
lib.delete_all_material_expressions(m)
def node(cls):return lib.create_material_expression(m,cls)
custom=node(unreal.MaterialExpressionCustom)
custom.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT3)
inputs=[]
for name in ['P','TrailY']:
    i=unreal.CustomInput();i.set_editor_property('input_name',name);inputs.append(i)
custom.set_editor_property('inputs',inputs)
custom.set_editor_property('code','''
float3 earth=float3(0.27,0.145,0.06);
float2 trail=float2(P.y-TrailY,P.z-155.0);
float corridor=1.0-smoothstep(100.0,145.0,length(trail*float2(1.0,.95)));
float3 color=earth;
float layer=floor(P.x/210.0);
[unroll] for(int i=0;i<16;++i) {
float seed=float(i)+layer*23.0;
float3 r=frac(sin(float3(seed*17.13+4.1,seed*37.21+9.4,seed*93.71+2.7))*43758.5453);
float2 center=(r.xy-.5)*float2(205.0,175.0);
float2 d=trail-center;
float radius=4.0+r.z*r.z*25.0;
float angle=atan2(d.y,d.x);
float border=radius*(1.0+.19*sin(angle*5.0+r.x*17.0)+.09*sin(angle*11.0+r.y*21.0));
float patch=1.0-smoothstep(border-.7,border+.7,length(d*float2(1.0,.85+r.x*.5)));
float runLength=radius+15.0+r.y*40.0;
float drip=(1.0-smoothstep(1.0,2.6,abs(d.x+sin(d.y*.07)*1.2)))*step(d.y,0.0)*smoothstep(-runLength,-runLength+8.0,d.y)*step(.55,r.z);
float3 pigment=r.x>.54?float3(.045,.24,.14):float3(.25,.034,.17);
color=lerp(color,pigment,max(patch,drip)*corridor*(.62+r.z*.25));
}
return color;
''')
pos=node(unreal.MaterialExpressionWorldPosition)
trail=node(unreal.MaterialExpressionScalarParameter);trail.set_editor_property('parameter_name','TrailY')
lib.connect_material_expressions(pos,'XYZ',custom,'P');lib.connect_material_expressions(trail,'',custom,'TrailY')
lib.connect_material_property(custom,'',unreal.MaterialProperty.MP_BASE_COLOR)
rough=node(unreal.MaterialExpressionConstant);rough.set_editor_property('r',.88)
lib.connect_material_property(rough,'',unreal.MaterialProperty.MP_ROUGHNESS)
glow=node(unreal.MaterialExpressionMultiply);amount=node(unreal.MaterialExpressionConstant);amount.set_editor_property('r',.12)
lib.connect_material_expressions(custom,'',glow,'A');lib.connect_material_expressions(amount,'',glow,'B')
lib.connect_material_property(glow,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
lib.recompile_material(m);unreal.EditorAssetLibrary.save_loaded_asset(m)


levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
if not unreal.EditorAssetLibrary.does_asset_exist(MAP):
    assert unreal.EditorAssetLibrary.duplicate_asset('/Game/CollectionPrototype/Maps/Lvl_Collection_V02',MAP)
    assert levels.load_level(MAP)
    actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
    field=next(a for a in actors if isinstance(a,unreal.MiningCollectionField))
    field.set_editor_property('excavation_depth',750);field.set_editor_property('display_version','V0.3');field.set_actor_label('V03 demonstration collection rules')
    assert levels.save_current_level()
unreal.log('COLLECTION_V03_CREATED '+MAP)
