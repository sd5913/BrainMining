import unreal

destination = '/Game/Showcase/Blueprints/BP_SharedMiningCharacter'
if unreal.EditorAssetLibrary.does_asset_exist(destination):
    raise RuntimeError('Shared character already exists; edit its components in the Blueprint viewport.')

source = unreal.load_asset('/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter')
blueprint = unreal.AssetToolsHelpers.get_asset_tools().duplicate_asset(
    'BP_SharedMiningCharacter', '/Game/Showcase/Blueprints', source)
assert blueprint
unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
library = unreal.SubobjectDataBlueprintFunctionLibrary
components = {}
for handle in subsystem.k2_gather_subobject_data_for_blueprint(blueprint):
    component = library.get_object_for_blueprint(library.get_data(handle), blueprint)
    if component:
        components[component.get_name()] = component

components['PickaxeMount'].set_editor_property('relative_location', unreal.Vector(70, 25, -50))
components['PickaxeModel'].set_editor_property('relative_location', unreal.Vector(-9.885566, -0.907176, 1.205274))
components['PickaxeModel'].set_editor_property('relative_rotation', unreal.Rotator(pitch=-6.922544, yaw=5.243221, roll=-4.019591))
for side, lateral in [('Right', 28), ('Left', -28)]:
    components['Drill' + side + 'Mount'].set_editor_property('relative_location', unreal.Vector(80, lateral, -26))
    model = components['Drill' + side + 'Model']
    model.set_editor_property('relative_location', unreal.Vector())
    model.set_editor_property('relative_rotation', unreal.Rotator())

for name in ['PickaxeModel', 'DrillRightModel', 'DrillLeftModel']:
    components[name].set_editor_property('first_person_primitive_type', unreal.FirstPersonPrimitiveType.FIRST_PERSON)

for name, intensity, radius in [('MiningLight', 1.2, 2200), ('MiningFillLight', 0.15, 1400)]:
    light = components[name]
    light.set_editor_property('use_inverse_squared_falloff', False)
    light.set_editor_property('intensity_units', unreal.LightUnits.UNITLESS)
    light.set_editor_property('light_falloff_exponent', 2)
    light.set_editor_property('intensity', intensity)
    light.set_editor_property('attenuation_radius', radius)
    light.set_editor_property('visible', True)
    light.set_editor_property('cast_shadows', False)
components['MiningLight'].set_editor_property('relative_location', unreal.Vector(-20, 0, 0))
components['MiningLight'].set_editor_property('inner_cone_angle', 45)
components['MiningLight'].set_editor_property('outer_cone_angle', 80)

unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
assert unreal.EditorAssetLibrary.save_loaded_asset(blueprint, False)
unreal.log('SHARED_MINING_CHARACTER_CREATED ' + destination)
