import unreal

ROOT = '/Game/Showcase/Treasure'
lib = unreal.MaterialEditingLibrary
assets = unreal.AssetToolsHelpers.get_asset_tools()
expression_pools = {}
unreal.EditorAssetLibrary.make_directory(ROOT + '/Meshes')
unreal.EditorAssetLibrary.make_directory(ROOT + '/Materials')
if not unreal.MiningDesignerLibrary.build_treasure_meshes():
    raise RuntimeError('Treasure mesh baking failed')

def material(name):
    path = ROOT + '/Materials/' + name
    result = unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else assets.create_asset(name, ROOT + '/Materials', unreal.Material, unreal.MaterialFactoryNew())
    expression_pools[result.get_path_name()] = list(lib.get_material_expressions(result))
    return result

def node(mat, cls):
    pool = expression_pools[mat.get_path_name()]
    if pool:
        existing = pool.pop(0)
        if not isinstance(existing, cls):
            raise RuntimeError('Generated material graph changed; refusing to overwrite ' + existing.get_path_name())
        return existing
    return lib.create_material_expression(mat, cls)

def scalar(mat, name, value):
    result = node(mat, unreal.MaterialExpressionScalarParameter)
    result.set_editor_property('parameter_name', name)
    result.set_editor_property('default_value', value)
    return result

def vector(mat, name, color):
    result = node(mat, unreal.MaterialExpressionVectorParameter)
    result.set_editor_property('parameter_name', name)
    result.set_editor_property('default_value', unreal.LinearColor(*color))
    return result

def custom(mat, code, inputs, output=unreal.CustomMaterialOutputType.CMOT_FLOAT3):
    result = node(mat, unreal.MaterialExpressionCustom)
    result.set_editor_property('output_type', output)
    entries = []
    for name in inputs:
        entry = unreal.CustomInput()
        entry.set_editor_property('input_name', name)
        entries.append(entry)
    result.set_editor_property('inputs', entries)
    result.set_editor_property('code', code)
    for name, source in inputs.items():
        expression, pin = source if isinstance(source, tuple) else (source, '')
        if not lib.connect_material_expressions(expression, pin, result, name):
            raise RuntimeError('Cannot connect ' + name)
    return result

def output(expression, prop):
    if not lib.connect_material_property(expression, '', prop):
        raise RuntimeError('Cannot connect material output')

gem = material('M_TreasureMineral')
world = node(gem, unreal.MaterialExpressionWorldPosition)
origin = node(gem, unreal.MaterialExpressionObjectPositionWS)
local = node(gem, unreal.MaterialExpressionSubtract)
lib.connect_material_expressions(world, 'XYZ', local, 'A')
lib.connect_material_expressions(origin, '', local, 'B')
vertex = node(gem, unreal.MaterialExpressionVertexColor)
tint = vector(gem, 'Tint', (.08, .6, .3, 1))
style = scalar(gem, 'Style', 5)
highlight = scalar(gem, 'Highlight', 0)
base = custom(gem, '''
float3 color = Tint.rgb * lerp(.18, .65, pow(V.r, 3));
if (Style > .5 && Style < 1.5 || Style > 2.5 && Style < 3.5)
    color = lerp(color, float3(.025,.032,.038), Inclusion);
if (Style > 4.5 && Style < 5.5) {
    float bands = .5+.5*sin(P.z*.38 + sin(P.x*.13)*1.8 + sin(P.y*.17));
    color *= lerp(.4,1.4,smoothstep(.12,.85,bands));
}
if (Style > 5.5 && Style < 6.5) {
    float3 cell = floor(P*1.05);
    float grain = frac(sin(dot(cell,float3(12.9898,78.233,43.12)))*43758.5453);
    color = lerp(color, float3(.65,.42,.12), step(.988,grain)*.8);
}
return color*(1+Highlight*.18);
''', {'P': local, 'V': vertex, 'Inclusion': (vertex, 'A'), 'Tint': tint, 'Style': style, 'Highlight': highlight})
output(base, unreal.MaterialProperty.MP_BASE_COLOR)
output(scalar(gem, 'Roughness', .32), unreal.MaterialProperty.MP_ROUGHNESS)
output(scalar(gem, 'Metallic', .08), unreal.MaterialProperty.MP_METALLIC)
fresnel = node(gem, unreal.MaterialExpressionFresnel)
emission = custom(gem, 'return Base * Emission * (.6 + .55 * Rim) * (1 + Highlight*.3);', {'Base': base, 'Emission': scalar(gem, 'Emission', .45), 'Rim': fresnel, 'Highlight': highlight})
output(emission, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
lib.layout_material_expressions(gem)
lib.recompile_material(gem)
unreal.EditorAssetLibrary.save_loaded_asset(gem)

rock = material('M_TreasureRock')
pos = node(rock, unreal.MaterialExpressionWorldPosition)
rock_tint = vector(rock, 'Tint', (.062, .055, .047, 1))
noise = custom(rock, '''
float coarse = sin(P.x*.075+sin(P.z*.041)*2)*sin(P.y*.09+P.z*.063);
float medium = sin(P.x*.31+P.y*.24)*sin(P.z*.29-P.y*.18);
float fine = frac(sin(dot(floor(P*1.7),float3(12.98,71.17,43.12)))*43758.5453);
return saturate(.52+coarse*.23+medium*.12+(fine-.5)*.13);
''', {'P': (pos, 'XYZ')}, unreal.CustomMaterialOutputType.CMOT_FLOAT1)
mask = custom(rock, '''
float radius = max(Radius,1);
float distanceToGem = length(P-Center.rgb);
float halo = 1-smoothstep(radius*.18,radius,distanceToGem);
return halo*halo*lerp(.7,1,Noise)*Amount*Strength;
''', {'P': (pos, 'XYZ'), 'Center': vector(rock, 'HintCenter', (0,0,0,1)), 'Radius': scalar(rock, 'HintRadius',120), 'Noise':noise, 'Amount':scalar(rock,'HintAmount',0), 'Strength':scalar(rock,'HintStrength',.8)}, unreal.CustomMaterialOutputType.CMOT_FLOAT1)
hint_color = vector(rock, 'HintColor', (.1,1,.3,1))
rock_color = custom(rock, 'return Tint.rgb * lerp(.45,1.45,Noise) + Hint.rgb * Mask*.025;', {'Tint':rock_tint, 'Noise':noise, 'Hint':hint_color, 'Mask':mask})
output(rock_color, unreal.MaterialProperty.MP_BASE_COLOR)
output(scalar(rock,'Roughness',.92), unreal.MaterialProperty.MP_ROUGHNESS)
rock_emission = custom(rock, 'return Tint.rgb*.18 + Hint.rgb*Mask*1.8;', {'Tint':rock_tint, 'Hint':hint_color, 'Mask':mask})
output(rock_emission, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
lib.layout_material_expressions(rock)
lib.recompile_material(rock)
unreal.EditorAssetLibrary.save_loaded_asset(rock)

levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
if not levels.load_level('/Game/Showcase/Maps/Lvl_ContinuousShowcase'):
    raise RuntimeError('Cannot load continuous showcase')
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
field = next(actor for actor in actors if isinstance(actor, unreal.MiningTreasureField))
definitions = list(field.get_editor_property('definitions'))
for definition in definitions:
    number = int(str(definition.get_editor_property('id')))
    if not 1 <= number <= 9:
        continue
    definition.set_editor_property('model', unreal.load_asset(ROOT + f'/Meshes/SM_Treasure_{number:02}'))
    definition.set_editor_property('scale', unreal.Vector(.9,.9,.9))
    color = definition.get_editor_property('color')
    strongest = max(color.r,color.g,color.b,.001)
    hint = unreal.LinearColor(color.r/strongest,color.g/strongest,color.b/strongest,1) if number != 1 else unreal.LinearColor(.38,.3,.2,1)
    definition.set_editor_property('hint_color', hint)
    definition.set_editor_property('buried_hint', True)
    definition.set_editor_property('hint_radius', 120)
    definition.set_editor_property('hint_strength', .8)
    definition.set_editor_property('emissive_strength', .12 if number < 5 else .7 if number == 9 else .45)
    definition.set_editor_property('roughness', .65 if number == 1 else .38 if number < 5 else .28)
    definition.set_editor_property('metallic', .65 if 1 < number < 5 else .08)
    definition.set_editor_property('surface_style', number-1)
field.set_editor_property('definitions', definitions)
field.set_editor_property('treasure_material', gem)
field.set_editor_property('rock_material', rock)
field.set_editor_property('guide_material', rock)
for actor in actors:
    if actor.get_actor_label() == 'Treasure_LocalLight':
        actor.get_component_by_class(unreal.PointLightComponent).set_editor_property('intensity', 1000)
if not levels.save_current_level():
    raise RuntimeError('Cannot save upgraded showcase')
unreal.log('TREASURE_ART_UPGRADE_SAVED')
