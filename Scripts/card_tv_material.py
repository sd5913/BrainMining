"""Rebuild only the card-room TV interior, preserving its asset and references."""
import unreal

def build_tv_material():
    lib=unreal.MaterialEditingLibrary
    path='/Game/Rooms/CardDuel/Materials/M_EscapeTVInterior'
    m=unreal.load_asset(path)
    if not m:
        m=unreal.AssetToolsHelpers.get_asset_tools().create_asset('M_EscapeTVInterior','/Game/Rooms/CardDuel/Materials',unreal.Material,unreal.MaterialFactoryNew())
    lib.delete_all_material_expressions(m)
    m.set_editor_property('blend_mode',unreal.BlendMode.BLEND_OPAQUE)
    m.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    def vector(name,value):
        n=lib.create_material_expression(m,unreal.MaterialExpressionVectorParameter)
        n.set_editor_property('parameter_name',name);n.set_editor_property('default_value',unreal.LinearColor(*value,1));return n
    def scalar(value,target):
        n=lib.create_material_expression(m,unreal.MaterialExpressionConstant);n.set_editor_property('r',value)
        assert lib.connect_material_property(n,'',target)
    base=vector('BaseColor',(.045,.065,.075))
    assert lib.connect_material_property(base,'RGB',unreal.MaterialProperty.MP_BASE_COLOR)
    n=lib.create_material_expression(m,unreal.MaterialExpressionCustom)
    n.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    inputs=[]
    for name in ['P','O','A','U']:
        i=unreal.CustomInput();i.set_editor_property('input_name',name);inputs.append(i)
    n.set_editor_property('inputs',inputs)
    # Coarse, static traces; neither time nor noise drives the entire excavated surface.
    n.set_editor_property('code',r"""
float3 d=P-O;
float along=dot(d,A), height=dot(d,U), side=dot(d,normalize(cross(U,A)));
float lower=1-smoothstep(-120,60,height);
float panel=1-smoothstep(3,8,abs(frac(along/800)*800-400));
float rail=1-smoothstep(10,22,abs(height+235));
float2 q=float2(along,side);
float traceMask=(1-smoothstep(3,8,abs(frac(q.y/500)*500-250)))*.025;
float3 shell=lerp(float3(.055,.09,.11),float3(.13,.105,.065),lower);
return shell+panel*float3(.025,.035,.04)+rail*float3(.08,.21,.24)+traceMask;
""")
    position=lib.create_material_expression(m,unreal.MaterialExpressionWorldPosition)
    assert lib.connect_material_expressions(position,'XYZ',n,'P')
    for key,name,value in [('O','TunnelOrigin',(23520,11750,6700)),('A','TunnelAxis',(.882,0,.471)),('U','TunnelUp',(-.471,0,.882))]:
        assert lib.connect_material_expressions(vector(name,value),'RGB',n,key)
    assert lib.connect_material_property(n,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    scalar(.82,unreal.MaterialProperty.MP_ROUGHNESS)
    scalar(.05,unreal.MaterialProperty.MP_METALLIC)
    lib.recompile_material(m)
    assert unreal.EditorAssetLibrary.save_loaded_asset(m)
    unreal.log('CARD_TV_COMFORT_MATERIAL_READY')
    return m

if __name__=='__main__':
    build_tv_material()
