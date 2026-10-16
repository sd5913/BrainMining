"""Update only serialized presentation/balance assets for the October playtest.

Run in UnrealEditor-Cmd with -run=pythonscript. No mineral redistribution or
terrain regeneration is performed. Originals are retained under Saved/Backups.
"""
import json
import shutil
from datetime import datetime
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
OUT = ROOT / 'Saved/Validation/PlaytestFeedback'
OUT.mkdir(parents=True, exist_ok=True)
BACKUP = ROOT / 'Saved/Backups' / ('PlaytestFeedback_' + datetime.now().strftime('%Y%m%d-%H%M%S'))
BACKUP.mkdir(parents=True)
assets = unreal.EditorAssetLibrary
report = {'backup': str(BACKUP), 'saved': [], 'materials': {}}

def backup(package, extension='uasset'):
    relative = Path('Content') / (package.removeprefix('/Game/') + '.' + extension)
    dest = BACKUP / relative
    dest.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(ROOT / relative, dest)

def save(asset):
    assert assets.save_loaded_asset(asset, False), asset.get_path_name()
    report['saved'].append(asset.get_path_name())

levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert levels.load_level('/Game/MainGame/Maps/Lvl_MainGame')
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
fields = [a for a in actors if isinstance(a, unreal.MiningTreasureField) and a.get_editor_property('use_authored_placements')]
assert len(fields) == 1, len(fields)
field = fields[0]
backup('/Game/MainGame/Maps/Lvl_MainGame', 'umap')
definitions = list(field.definitions)
iron = next(d for d in definitions if str(d.id) == '4')
report['iron_before'] = str(iron.color)
iron.set_editor_property('color', unreal.LinearColor(.60, .045, .028, 1))
iron.set_editor_property('hint_color', unreal.LinearColor(.82, .08, .035, 1))
iron.set_editor_property('roughness', .58)
iron.set_editor_property('metallic', .18)
iron.set_editor_property('emissive_strength', .08)
for definition in definitions:
    definition.set_editor_property('hint_radius', 180.)
field.set_editor_property('definitions', definitions)
report['iron_after'] = str(iron.color)
report['mineral_placements_preserved'] = len(field.get_editor_property('authored_placements'))
assert levels.save_current_level()
report['saved'].append('/Game/MainGame/Maps/Lvl_MainGame')

path = '/Game/MainGame/Blueprints/BP_MainGameCharacter'
backup(path)
bp = unreal.load_asset(path)
cdo = unreal.get_default_object(bp.generated_class())
move = cdo.get_component_by_class(unreal.MiningCharacterMovementComponent)
settings = move.get_editor_property('flight_settings')
report['flight_before'] = {'lift': settings.lift_acceleration, 'boost': settings.boost_acceleration_multiplier}
settings.set_editor_property('lift_acceleration', 1500.)
settings.set_editor_property('boost_acceleration_multiplier', 2.6)
move.set_editor_property('flight_settings', settings)
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
save(bp)
report['flight_after'] = {'lift': settings.lift_acceleration, 'boost': settings.boost_acceleration_multiplier}

source = unreal.load_asset('/Game/Bomb/SM_StickyBomb')
path = '/Game/PopupClose/FX/SM_OfficeFinaleC4'
crowd = unreal.load_asset(path)
backup(path)
mapping = {str(slot.material_slot_name): slot.material_interface for slot in source.static_materials}
slots = list(crowd.static_materials)
report['crowd_before'] = [(str(s.material_slot_name), s.material_interface.get_path_name() if s.material_interface else None) for s in slots]
for slot in slots:
    name = str(slot.material_slot_name)
    assert name in mapping and mapping[name], (name, list(mapping))
    slot.set_editor_property('material_interface', mapping[name])
crowd.set_editor_property('static_materials', slots)
save(crowd)
for material in set(mapping.values()):
    # Cook needs the instancing permutation; editor auto-repair does not run in a package.
    if not material.get_editor_property('used_with_instanced_static_meshes'):
        backup(material.get_path_name().split('.')[0])
        material.set_editor_property('used_with_instanced_static_meshes', True)
        unreal.MaterialEditingLibrary.recompile_material(material)
        save(material)
report['crowd_after'] = [(str(s.material_slot_name), s.material_interface.get_path_name()) for s in crowd.static_materials]
report['crowd_triangles'] = [crowd.get_num_triangles(i) for i in range(crowd.get_num_lods())]

for path in ['/Game/Passages/Echo/Materials/M_SharedPickaxeBlack',
             '/Game/Passages/Echo/Materials/M_SharedPickaxeLines',
             '/Game/Rooms/CardDuel/Materials/M_CardRoomFlameSprite',
             '/Game/Rooms/CardDuel/Materials/M_CardRoomSmokeSprite',
             '/Game/CollectionPrototype/Materials/M_CollectionSilhouette']:
    material = unreal.load_asset(path)
    assert material, path
    report['materials'][path] = material.get_class().get_name()

report['entry'] = []
for actor in actors:
    if actor.get_name() in ['GeneratedMiningRock_9', 'GeneratedMiningRock_10', 'DynamicMeshActor_4']:
        record = {'name': actor.get_name(), 'label': actor.get_actor_label(), 'tags': [str(t) for t in actor.tags], 'position': str(actor.get_actor_location())}
        if isinstance(actor, unreal.DiggableRock):
            for prop in ['hardness', 'required_destruction_level']:
                try: record[prop] = str(actor.get_editor_property(prop))
                except Exception: pass
        report['entry'].append(record)
(OUT / 'assets.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
unreal.log('PLAYTEST_FEEDBACK_ASSETS ' + json.dumps(report, ensure_ascii=False))
