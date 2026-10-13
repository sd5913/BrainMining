"""Migrate the saved campaign without rebuilding or replacing its mining terrain."""
import hashlib
import json
import re
import shutil
from datetime import datetime
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir())
recipe = json.loads((root / 'Config/CampaignLayout.json').read_text(encoding='utf-8'))
backup = root / 'Saved/CampaignBackups' / ('crystals-museum-' + datetime.now().strftime('%Y%m%d-%H%M%S'))
backup.mkdir(parents=True)
shutil.copy2(root / 'Content/Campaign/Maps/Lvl_FullJourney.umap', backup / 'Previous.umap')
shutil.copy2(root / 'Content/Showcase/Treasure/Meshes/SM_CrystalClue.uasset', backup / 'SM_CrystalClue.uasset')
sources = {s['source'] for s in recipe['stages']}
sources.add('/Game/WenSi/Maps/Lvl_WenSi_ScreenBridge')
hashes = {s: hashlib.sha256((root / 'Content' / (s[6:] + '.umap')).read_bytes()).hexdigest() for s in sources}
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
assert levels.load_level(recipe['map'])
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
hall = next(a for a in actors.get_all_level_actors() if isinstance(a, unreal.MiningExhibitHall))
areas = list(hall.areas)
old = areas[5].source_level
if not old.get_loaded_level().get_path_name().startswith(recipe['stages'][5]['source'] + '.'):
    origin = areas[5].origin
    delta = unreal.Vector(950, 0, 0)
    # Move only the old exit adapter/corridor and ending; preserve all three mines.
    persistent = [a for a in actors.get_all_level_actors() if a.get_level() == hall.get_level()]
    for a in persistent:
        if a.actor_has_tag(areas[6].tag) or (a.actor_has_tag(areas[5].tag) and a.get_actor_location().x > origin.x + 1400):
            a.set_actor_location(a.get_actor_location() + delta, False, False)
    for a in persistent:
        if a.get_actor_label().startswith('Campaign_MuseumDoorWall'):
            loc = a.get_actor_location()
            loc.y = origin.y + (435 if loc.y > origin.y else -435)
            a.set_actor_location(loc, False, False)
            a.set_actor_scale3d(unreal.Vector(.2, 4.7, 5))
    assert unreal.EditorLevelUtils.remove_level_from_world(old.get_loaded_level())
    ending = areas[6].source_level
    assert unreal.EditorLevelUtils.remove_level_from_world(ending.get_loaded_level())
    for index in (5, 6):
        if index == 6:
            areas[index].set_editor_property('origin', areas[index].origin + delta)
            entry = areas[index].entry
            entry.translation += delta
            areas[index].set_editor_property('entry', entry)
            areas[index].set_editor_property('exit', areas[index].exit + delta)
            for prop in ('mining_bounds', 'activation_bounds'):
                bounds = areas[index].get_editor_property(prop)
                areas[index].set_editor_property(prop, unreal.Box(min=bounds.min + delta, max=bounds.max + delta))
        before = set(actors.get_all_level_actors())
        stream = unreal.EditorLevelUtils.add_level_to_world_with_transform(world, recipe['stages'][index]['source'], unreal.LevelStreamingAlwaysLoaded, unreal.Transform(location=areas[index].origin))
        assert stream
        areas[index].set_editor_property('source_level', stream)
        if index == 5:
            imported = set(actors.get_all_level_actors()) - before
            areas[index].set_editor_property('suppressed_source_actors', [a.get_name() for a in imported if re.fullmatch(r'(BackWall|EndWall)_?\d*', a.get_actor_label())])
            assert len(areas[index].suppressed_source_actors) == 2
    areas[5].set_editor_property('exit', areas[5].exit + delta)
    for prop in ('mining_bounds', 'activation_bounds'):
        bounds = areas[5].get_editor_property(prop)
        areas[5].set_editor_property(prop, unreal.Box(min=bounds.min, max=bounds.max + delta))
    # Bridge the 35 cm difference between the old entrance and the new source floor.
    a = actors.spawn_actor_from_class(unreal.StaticMeshActor, origin + unreal.Vector(-920, 0, -15))
    a.set_actor_label('Campaign_MuseumEntranceFloorAdapter')
    a.set_editor_property('tags', [areas[5].tag])
    a.static_mesh_component.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Cube'))
    a.static_mesh_component.set_material(0, unreal.load_asset('/Game/Showcase/Materials/M_ContinuousFloor'))
    a.set_actor_scale3d(unreal.Vector(.7, 4, .3))
    hall.set_editor_property('areas', areas)

assert unreal.MiningDesignerLibrary.build_crystal_clue_mesh()
field = next(a for a in actors.get_all_level_actors() if isinstance(a, unreal.MiningTreasureField) and a.get_editor_property('use_authored_placements'))
field.set_editor_property('crystal_clue_mesh', unreal.load_asset('/Game/Showcase/Treasure/Meshes/SM_CrystalClue'))
assert unreal.MiningDesignerLibrary.prepare_campaign_minerals(field)
rows = []
for p in field.get_editor_property('authored_placements'):
    rows.append(dict(id=str(p.treasure_id), value=p.value, depth=(p.transform.translation-p.surface.translation).length(), fragments=len(p.fragments)))
assert unreal.EditorLoadingAndSavingUtils.save_map(world, recipe['map'])
assert all(hashlib.sha256((root/'Content'/(s[6:]+'.umap')).read_bytes()).hexdigest() == h for s, h in hashes.items())
(root/'Saved/CampaignCrystalMuseumUpgrade.json').write_text(json.dumps(dict(backup=str(backup), sources=hashes, minerals=rows), indent=2), encoding='utf-8')
unreal.log('CAMPAIGN_CRYSTAL_MUSEUM_UPGRADE_SAVED')
unreal.SystemLibrary.quit_editor()
