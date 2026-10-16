"""Build PBR flood water, textured TV fill and readable screen type; no map edits."""
import unreal
lib=unreal.MaterialEditingLibrary
base='/Game/Rooms/CardDuel/Materials/'
def make(name,model):
    path=base+name
    # Recreate owned generated assets: UE retains custom water outputs when only expressions are cleared.
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        assert unreal.EditorAssetLibrary.delete_asset(path)
    m=unreal.AssetToolsHelpers.get_asset_tools().create_asset(name,base.rstrip('/'),unreal.Material,unreal.MaterialFactoryNew())
    m.set_editor_property('blend_mode',unreal.BlendMode.BLEND_OPAQUE)
    m.set_editor_property('shading_model',model)
    return m

def scalar(m,value):
    n=lib.create_material_expression(m,unreal.MaterialExpressionConstant);n.set_editor_property('r',value);return n

def color(m,value):
    n=lib.create_material_expression(m,unreal.MaterialExpressionConstant3Vector)
    n.set_editor_property('constant',unreal.LinearColor(*value,1));return n

def prop(m,n,target):
    assert lib.connect_material_property(n,'',target)

def custom(m,code):
    n=lib.create_material_expression(m,unreal.MaterialExpressionCustom)
    n.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    inputs=[]
    for key in ['P','T']:
        i=unreal.CustomInput();i.set_editor_property('input_name',key);inputs.append(i)
    n.set_editor_property('inputs',inputs);n.set_editor_property('code',code)
    for cls,key in [(unreal.MaterialExpressionWorldPosition,'P'),(unreal.MaterialExpressionTime,'T')]:
        source=lib.create_material_expression(m,cls)
        ok=lib.connect_material_expressions(source,'XYZ' if key=='P' else '',n,key)
        unreal.log('ESCAPE_CONNECT '+key+' '+str(ok))
        assert ok,key
    return n

def save(m):
    lib.recompile_material(m);assert unreal.EditorAssetLibrary.save_loaded_asset(m)

m=make('M_EscapeWater',unreal.MaterialShadingModel.MSM_SINGLE_LAYER_WATER)
m.set_editor_property('tangent_space_normal',False)
prop(m,color(m,(.008,.018,.017)),unreal.MaterialProperty.MP_BASE_COLOR)
prop(m,scalar(m,.08),unreal.MaterialProperty.MP_ROUGHNESS)
prop(m,scalar(m,.5),unreal.MaterialProperty.MP_SPECULAR)
prop(m,scalar(m,.04),unreal.MaterialProperty.MP_OPACITY)
prop(m,custom(m,"float2 q=P.xy; float a=dot(q,float2(.007,.004))+T*.65; float b=dot(q,float2(-.011,.009))-T*.8; return normalize(float3(.028*cos(a)-.019*cos(b),.016*cos(a)+.016*cos(b),1));"),unreal.MaterialProperty.MP_NORMAL)
w=lib.create_material_expression(m,unreal.MaterialExpressionSingleLayerWaterMaterialOutput)
for key,n in [('ScatteringCoefficients',color(m,(.00008,.00013,.00012))),('AbsorptionCoefficients',color(m,(.0009,.00035,.00022))),('PhaseG',scalar(m,.2)),('ColorScaleBehindWater',color(m,(1,1,1)))]:
    assert lib.connect_material_expressions(n,'',w,key),key
save(m)
# A suspended jet needs a translucent surface; Single Layer Water assumes a deep water body.
m=make('M_EscapeJet',unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
m.set_editor_property('blend_mode',unreal.BlendMode.BLEND_TRANSLUCENT)
m.set_editor_property('translucency_lighting_mode',unreal.TranslucencyLightingMode.TLM_SURFACE_PER_PIXEL_LIGHTING)
prop(m,color(m,(.08,.17,.2)),unreal.MaterialProperty.MP_BASE_COLOR)
prop(m,scalar(m,.08),unreal.MaterialProperty.MP_ROUGHNESS)
prop(m,scalar(m,.65),unreal.MaterialProperty.MP_SPECULAR)
prop(m,scalar(m,.28),unreal.MaterialProperty.MP_OPACITY)
prop(m,scalar(m,1.035),unreal.MaterialProperty.MP_REFRACTION)
save(m)
import sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parent))
from card_tv_material import build_tv_material
build_tv_material()
# Retain the engine font atlas decoding, otherwise glyphs become solid rectangles.
name='M_EscapeScreenText';path=base+name
if unreal.EditorAssetLibrary.does_asset_exist(path):
    m=unreal.load_asset(path)
else:
    m=unreal.AssetToolsHelpers.get_asset_tools().duplicate_asset(name,base.rstrip('/'),unreal.load_asset('/Engine/EngineMaterials/DefaultTextMaterialOpaque'))
m.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT)
m.set_editor_property('two_sided',True)
prop(m,color(m,(12,18,12)),unreal.MaterialProperty.MP_EMISSIVE_COLOR)
save(m)
unreal.log('CARD_ESCAPE_MATERIALS_READY')
