"""Restore only absent imported material dependencies using the original Blender palette."""
import unreal

def ensure_blocker_materials():
    directory = '/Game/ExternalMining/GuideLayers/Fill50V3'
    palette = [(.31,.155,.075),(.39,.205,.095),(.47,.26,.13),(.55,.32,.17),(.61,.39,.23)]
    lib = unreal.MaterialEditingLibrary
    for i, rgb in enumerate(palette, 1):
        name = 'Tunnel_Blocker_Rock_%02d' % i
        path = directory + '/' + name
        if unreal.EditorAssetLibrary.does_asset_exist(path):
            continue
        material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, directory, unreal.Material, unreal.MaterialFactoryNew())
        color = lib.create_material_expression(material, unreal.MaterialExpressionConstant3Vector)
        color.set_editor_property('constant', unreal.LinearColor(*rgb,1))
        lib.connect_material_property(color, '', unreal.MaterialProperty.MP_BASE_COLOR)
        rough = lib.create_material_expression(material, unreal.MaterialExpressionConstant)
        rough.set_editor_property('r', 1.0)
        lib.connect_material_property(rough, '', unreal.MaterialProperty.MP_ROUGHNESS)
        lib.recompile_material(material)
        assert unreal.EditorAssetLibrary.save_loaded_asset(material)

def ensure_campaign_limestone(guide=False):
    """The campaign's existing rock reference now uses dark, warm mineral strata."""
    name='M_CampaignGuideRock' if guide else 'M_CampaignLimestone'
    path='/Game/Campaign/Materials/'+name
    m=unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else unreal.AssetToolsHelpers.get_asset_tools().create_asset(name,'/Game/Campaign/Materials',unreal.Material,unreal.MaterialFactoryNew())
    lib=unreal.MaterialEditingLibrary;lib.delete_all_material_expressions(m)
    p=lib.create_material_expression(m,unreal.MaterialExpressionWorldPosition)
    c=lib.create_material_expression(m,unreal.MaterialExpressionCustom)
    i=unreal.CustomInput();i.set_editor_property('input_name','P');c.set_editor_property('inputs',[i]);c.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    c.set_editor_property('code',"float n=.5+.22*sin(P.x*.009+sin(P.y*.008)*2)+.16*sin(P.z*.018+P.y*.006); float bands=.5+.5*sin(P.z*.033+sin(P.x*.006)*2.4); return lerp(float3(.055,.038,.027),float3(.17,.112,.070),saturate(n))*(.86+.14*bands);")
    if guide:c.set_editor_property('code',c.get_editor_property('code').replace('float3(.055,.038,.027)','float3(.055,.027,.011)').replace('float3(.17,.112,.070)','float3(.15,.075,.022)'))
    lib.connect_material_expressions(p,'XYZ',c,'P');lib.connect_material_property(c,'',unreal.MaterialProperty.MP_BASE_COLOR)
    r=lib.create_material_expression(m,unreal.MaterialExpressionConstant);r.set_editor_property('r',.93);lib.connect_material_property(r,'',unreal.MaterialProperty.MP_ROUGHNESS)
    lib.recompile_material(m);assert unreal.EditorAssetLibrary.save_loaded_asset(m)
    return m


def ensure_miner_headlamp():
    path='/Game/Campaign/Materials/M_MinerHeadlamp'
    m=unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else unreal.AssetToolsHelpers.get_asset_tools().create_asset('M_MinerHeadlamp','/Game/Campaign/Materials',unreal.Material,unreal.MaterialFactoryNew())
    lib=unreal.MaterialEditingLibrary;lib.delete_all_material_expressions(m)
    m.set_editor_property('material_domain',unreal.MaterialDomain.MD_LIGHT_FUNCTION)
    uv=lib.create_material_expression(m,unreal.MaterialExpressionTextureCoordinate)
    c=lib.create_material_expression(m,unreal.MaterialExpressionCustom)
    i=unreal.CustomInput();i.set_editor_property('input_name','UV');c.set_editor_property('inputs',[i]);c.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT1)
    c.set_editor_property('code',"float2 p=UV-.5; float r=length(p); float a=atan2(p.y,p.x); float ripple=.94+.06*cos(r*62+sin(a*3)*.65); return ripple*lerp(1,.25,smoothstep(.10,.58,r));")
    lib.connect_material_expressions(uv,'',c,'UV');lib.connect_material_property(c,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    lib.recompile_material(m);assert unreal.EditorAssetLibrary.save_loaded_asset(m)
    return m


def ensure_campaign_door():
    path='/Game/Campaign/Materials/M_CampaignDoor'
    if unreal.EditorAssetLibrary.does_asset_exist(path):return unreal.load_asset(path)
    m=unreal.AssetToolsHelpers.get_asset_tools().create_asset('M_CampaignDoor','/Game/Campaign/Materials',unreal.Material,unreal.MaterialFactoryNew())
    lib=unreal.MaterialEditingLibrary
    color=lib.create_material_expression(m,unreal.MaterialExpressionConstant3Vector)
    color.set_editor_property('constant',unreal.LinearColor(.18,.14,.09,1))
    lib.connect_material_property(color,'',unreal.MaterialProperty.MP_BASE_COLOR)
    lib.recompile_material(m);assert unreal.EditorAssetLibrary.save_loaded_asset(m)
    return m

def ensure_campaign_stain():
    """Same dirt-stain cue in a movable local frame, so elevation/rotation do not erase it."""
    path='/Game/Campaign/Materials/M_CampaignStainedEarth'
    m=unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else unreal.AssetToolsHelpers.get_asset_tools().create_asset('M_CampaignStainedEarth','/Game/Campaign/Materials',unreal.Material,unreal.MaterialFactoryNew())
    lib=unreal.MaterialEditingLibrary
    lib.delete_all_material_expressions(m)
    custom=lib.create_material_expression(m,unreal.MaterialExpressionCustom)
    custom.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    ins=[]
    for name in ['P','Origin','Forward','Right']:
        i=unreal.CustomInput();i.set_editor_property('input_name',name);ins.append(i)
    custom.set_editor_property('inputs',ins)
    custom.set_editor_property('code','''
float3 q=P-Origin;
float2 trail=float2(dot(q,Right),q.z);
float corridor=1-smoothstep(100,145,length(trail*float2(1,.95)));
float3 color=float3(.11,.074,.045);
float layer=floor((dot(q,Forward)+10)/150);
[unroll] for(int i=0;i<18;++i) {
float seed=i+layer*23;
float3 r=frac(sin(float3(seed*17.13+4.1,seed*37.21+9.4,seed*93.71+2.7))*43758.5453);
float2 d=trail-(r.xy-.5)*float2(205,175);
float radius=6+r.z*r.z*27;
float border=radius*(1+.19*sin(atan2(d.y,d.x)*5+r.x*17));
float patch=1-smoothstep(border-.7,border+.7,length(d*float2(1,.85+r.x*.5)));
float drip=(1-smoothstep(1,2.6,abs(d.x+sin(d.y*.07))))*step(d.y,0)*smoothstep(-radius-45,-radius-37,d.y)*step(.55,r.z);
color=lerp(color,r.x>.54?float3(.045,.24,.14):float3(.3,.025,.19),max(patch,drip)*corridor*(.7+r.z*.25));
}
return color;
''')
    p=lib.create_material_expression(m,unreal.MaterialExpressionWorldPosition)
    lib.connect_material_expressions(p,'XYZ',custom,'P')
    for name,default in [('Origin',unreal.LinearColor(0,0,0,1)),('Forward',unreal.LinearColor(1,0,0,1)),('Right',unreal.LinearColor(0,1,0,1))]:
        n=lib.create_material_expression(m,unreal.MaterialExpressionVectorParameter);n.set_editor_property('parameter_name',name);n.set_editor_property('default_value',default);lib.connect_material_expressions(n,'RGB',custom,name)
    lib.connect_material_property(custom,'',unreal.MaterialProperty.MP_BASE_COLOR)
    rough=lib.create_material_expression(m,unreal.MaterialExpressionConstant);rough.set_editor_property('r',.88);lib.connect_material_property(rough,'',unreal.MaterialProperty.MP_ROUGHNESS)
    lib.recompile_material(m);assert unreal.EditorAssetLibrary.save_loaded_asset(m)
    return m
