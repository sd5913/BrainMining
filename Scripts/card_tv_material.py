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
    base=vector('BaseColor',(.003,.006,.012))
    assert lib.connect_material_property(base,'RGB',unreal.MaterialProperty.MP_BASE_COLOR)
    n=lib.create_material_expression(m,unreal.MaterialExpressionCustom)
    n.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    inputs=[]
    for name in ['P','T']:
        i=unreal.CustomInput();i.set_editor_property('input_name',name);inputs.append(i)
    n.set_editor_property('inputs',inputs)
    # Restore the original dark circuit board and cyan pulsing tracks.
    n.set_editor_property('code',r"""
float2 q=float2(P.x*.012+P.y*.007,P.z*.014+P.y*.009);
float2 tile=floor(q/4), u=frac(q/4);
float id=frac(sin(dot(tile,float2(17.13,91.71)))*4375.23);
float lane=.2+floor(id*4)*.14;
float trace=(1-smoothstep(.009,.017,abs(u.y-lane)))*step(.12,u.x)*step(u.x,.82);
trace=max(trace,(1-smoothstep(.009,.017,abs(u.x-.82)))*step(lane,u.y)*step(u.y,.92));
float pad=1-smoothstep(.023,.036,length(u-float2(.12,lane)));
float seam=1-smoothstep(.006,.012,min(min(u.x,1-u.x),min(u.y,1-u.y)));
float pulse=pow(saturate(.5+.5*sin(q.x*1.7+q.y*.5-T*1.6)),12);
return float3(.003,.008,.015)+seam*float3(.005,.014,.026)+(trace+pad)*float3(.025,.18,.32)*(.65+pulse*24.0);
""")
    position=lib.create_material_expression(m,unreal.MaterialExpressionWorldPosition)
    assert lib.connect_material_expressions(position,'XYZ',n,'P')
    time=lib.create_material_expression(m,unreal.MaterialExpressionTime)
    assert lib.connect_material_expressions(time,'',n,'T')
    assert lib.connect_material_property(n,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    scalar(.38,unreal.MaterialProperty.MP_ROUGHNESS)
    scalar(.3,unreal.MaterialProperty.MP_METALLIC)
    lib.recompile_material(m)
    assert unreal.EditorAssetLibrary.save_loaded_asset(m)
    unreal.log('CARD_TV_COMFORT_MATERIAL_READY')
    return m

if __name__=='__main__':
    build_tv_material()
