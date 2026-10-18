"""Upgrade the existing V03 test map; preserve other maps and back up V03 once."""
import shutil
from pathlib import Path
import unreal

MAP = '/Game/CollectionPrototype/Maps/Lvl_Collection_V03'
root = Path(unreal.Paths.project_dir())
assert unreal.MiningDesignerLibrary.build_collection_number_mesh()
backup = root / 'Saved/Backups/CollectionBeforeExpansion/Lvl_Collection_V03.umap'
if not backup.exists():
    backup.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(root / 'Content/CollectionPrototype/Maps/Lvl_Collection_V03.umap', backup)

path = '/Game/CollectionPrototype/Materials/M_CollectionGlow'
if not unreal.EditorAssetLibrary.does_asset_exist(path):
    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        'M_CollectionGlow', '/Game/CollectionPrototype/Materials', unreal.Material, unreal.MaterialFactoryNew())
    lib = unreal.MaterialEditingLibrary
    tint = lib.create_material_expression(material, unreal.MaterialExpressionVectorParameter)
    tint.set_editor_property('parameter_name', 'Tint')
    tint.set_editor_property('default_value', unreal.LinearColor(.1, 1, .25, 1))
    strength = lib.create_material_expression(material, unreal.MaterialExpressionConstant)
    strength.set_editor_property('r', 5)
    mul = lib.create_material_expression(material, unreal.MaterialExpressionMultiply)
    lib.connect_material_expressions(tint, '', mul, 'A')
    lib.connect_material_expressions(strength, '', mul, 'B')
    lib.connect_material_property(tint, '', unreal.MaterialProperty.MP_BASE_COLOR)
    lib.connect_material_property(mul, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    lib.recompile_material(material)
    assert unreal.EditorAssetLibrary.save_loaded_asset(material)

# Pool particles use ISMs in game; persist the usage permutations, not editor-only auto fixes.
for material_path in [path, '/Game/CollectionPrototype/Materials/M_CollectionTint']:
    material = unreal.load_asset(material_path)
    if not material.get_editor_property('used_with_instanced_static_meshes'):
        material.set_editor_property('used_with_instanced_static_meshes', True)
        unreal.MaterialEditingLibrary.recompile_material(material)
        assert unreal.EditorAssetLibrary.save_loaded_asset(material)

# Collection-specific glow and glass; shared campaign materials remain untouched.
lib = unreal.MaterialEditingLibrary
def material(name, unlit=True, blend=unreal.BlendMode.BLEND_OPAQUE):
    package='/Game/CollectionPrototype/Materials'
    result=unreal.load_asset(package+'/'+name) if unreal.EditorAssetLibrary.does_asset_exist(package+'/'+name) else unreal.AssetToolsHelpers.get_asset_tools().create_asset(name,package,unreal.Material,unreal.MaterialFactoryNew())
    lib.delete_all_material_expressions(result)
    result.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT if unlit else unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    result.set_editor_property('blend_mode',blend)
    result.set_editor_property('two_sided',True)
    result.set_editor_property('used_with_instanced_static_meshes',True)
    return result
def scalar(m,value):
    n=lib.create_material_expression(m,unreal.MaterialExpressionConstant);n.set_editor_property('r',value);return n
def tint(m,value):
    n=lib.create_material_expression(m,unreal.MaterialExpressionVectorParameter);n.set_editor_property('parameter_name','Tint');n.set_editor_property('default_value',unreal.LinearColor(*value,1));return n
def multiply(m,a,b):
    n=lib.create_material_expression(m,unreal.MaterialExpressionMultiply)
    assert lib.connect_material_expressions(a,'',n,'A') and lib.connect_material_expressions(b,'',n,'B')
    return n
def prop(m,n,p):
    assert lib.connect_material_property(n,'',p)
def save(m):
    lib.recompile_material(m);assert unreal.EditorAssetLibrary.save_loaded_asset(m)
for name,color,strength in [('M_CollectionFlowerGlow',(.08,.48,.25),1.7),('M_CollectionSparkCore',(.3,1,.08),22)]:
    m=material(name);prop(m,multiply(m,tint(m,color),scalar(m,strength)),unreal.MaterialProperty.MP_EMISSIVE_COLOR);save(m)
m=material('M_CollectionSoftGlow',blend=unreal.BlendMode.BLEND_ADDITIVE)
uv=lib.create_material_expression(m,unreal.MaterialExpressionTextureCoordinate)
mask=lib.create_material_expression(m,unreal.MaterialExpressionCustom)
mask.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT1)
entry=unreal.CustomInput();entry.set_editor_property('input_name','UV');mask.set_editor_property('inputs',[entry])
mask.set_editor_property('code','float2 p=(UV-.5)*2; float r=length(p); float glow=exp(-dot(p,p)*7)*saturate(1-r); float star=(exp(-abs(p.x)*55-abs(p.y)*5)+exp(-abs(p.y)*55-abs(p.x)*5))*.35; return saturate(glow+star)*saturate(1-r);')
assert lib.connect_material_expressions(uv,'',mask,'UV')
prop(m,multiply(m,tint(m,(.25,1,.07)),scalar(m,6)),unreal.MaterialProperty.MP_EMISSIVE_COLOR)
prop(m,mask,unreal.MaterialProperty.MP_OPACITY);save(m)
m=material('M_CollectionGlass',False,unreal.BlendMode.BLEND_TRANSLUCENT)
m.set_editor_property('translucency_lighting_mode',unreal.TranslucencyLightingMode.TLM_SURFACE_PER_PIXEL_LIGHTING)
prop(m,tint(m,(.62,.83,.77)),unreal.MaterialProperty.MP_BASE_COLOR)
prop(m,scalar(m,.1),unreal.MaterialProperty.MP_ROUGHNESS)
prop(m,scalar(m,.8),unreal.MaterialProperty.MP_SPECULAR)
fresnel=lib.create_material_expression(m,unreal.MaterialExpressionFresnel)
fresnel.set_editor_property('exponent',3)
fresnel.set_editor_property('base_reflect_fraction',.07)
prop(m,multiply(m,fresnel,scalar(m,.38)),unreal.MaterialProperty.MP_OPACITY);save(m)

levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert levels.load_level(MAP)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
field = next(a for a in actors if isinstance(a, unreal.MiningCollectionField))
field.set_editor_property('expanded_collection', True)
field.set_editor_property('display_version', 'V0.5 / 12 FINDS')
field.set_actor_label('V05 refined collection test - 12 types')
# Replace the positive-Y side wall's entry section with a walkable opening.
for actor in actors:
    if 'side bedrock' in actor.get_actor_label() and actor.get_actor_location().y > 1500:
        actor.set_actor_location(unreal.Vector(1825, 1560, 580), False, False)
        actor.set_actor_scale3d(unreal.Vector(27.5, 1.2, 12.4))
assert levels.save_current_level()
unreal.log('COLLECTION_EXPANSION_SAVED ' + MAP)
