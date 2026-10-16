"""Migrate only Lvl_MainGame: three mines, four minerals, museum terminus.

Run in a separate editor with -ExecutePythonScript; saves only after validation.
Existing mine sites and size tiers are reused, then burial/clues are rebuilt.
"""
import unreal
import json
import shutil
import hashlib
import sys
import traceback
from collections import Counter
from datetime import datetime
from pathlib import Path

ROOT = Path(unreal.Paths.project_dir()).resolve()
sys.path.insert(0, str(ROOT / 'Scripts'))
import campaign_minerals_api as api
from main_game_material_plan import mineral_ids
OUT = ROOT / 'Saved/Validation/MaterialProgression'
OUT.mkdir(parents=True, exist_ok=True)
report = {'passed': False, 'saved': False}
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
ed = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
try:
    assert not ed.get_game_world()
    assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
    assert levels.load_level('/Game/MainGame/Maps/Lvl_MainGame')
    assert levels.set_current_level_by_name('Lvl_MainGame')
    backup = ROOT / 'Saved/Backups' / ('MainGame_MaterialProgression_' + datetime.now().strftime('%Y%m%d_%H%M%S'))
    backup.mkdir(parents=True)
    shutil.copy2(ROOT / 'Content/MainGame/Maps/Lvl_MainGame.umap', backup / 'Lvl_MainGame.umap')
    report['backup'] = str(backup)
    cfg = json.loads((ROOT / 'Config/MainGameRooms.json').read_text(encoding='utf-8'))
    sources = [r['source'] for r in cfg['rooms']] + ['/Game/Campaign/Maps/Lvl_FullJourney', '/Game/StartRoom/Maps/Lvl_StartRoom']
    source_hashes = {s: hashlib.sha256((ROOT / 'Content' / (s[6:] + '.umap')).read_bytes()).hexdigest() for s in sources}
    world, hall, actors = api._context()
    field = api.get_field(hall)
    assert field
    all_actors = list(actors.get_all_level_actors())
    persistent = hall.get_level()
    recipe = json.loads((ROOT / 'Config/MainGameMinerals.json').read_text(encoding='utf-8'))
    old_definitions = {str(d.id): d for d in field.definitions}
    retained = []
    distributions = []
    for area in recipe['areas']:
        group = sorted([p for p in field.get_editor_property('authored_placements') if str(p.area_tag) == area['tag']], key=lambda p: (p.route_progress, *p.transform.translation.to_tuple()))
        assert group, area['tag']
        ids = mineral_ids(area, len(group), recipe['seed'])
        for placement, mineral in zip(group, ids):
            old = old_definitions[str(placement.treasure_id)]
            tier = list(old.values_by_size).index(placement.value)
            placement.set_editor_property('treasure_id', str(mineral))
            placement.set_editor_property('value', old_definitions[str(mineral)].values_by_size[tier])
            # Begin burial search at the original exposed surface, rather than
            # repeatedly pushing previously baked gems deeper on each migration.
            transform = placement.transform
            transform.translation = placement.surface.translation
            placement.set_editor_property('transform', transform)
            placement.set_editor_property('fragments', [])
            placement.set_editor_property('fragment_supports', [])
            retained.append(placement)
        distributions.append({'area': area['tag'], 'count': len(group), 'minerals': dict(Counter(ids))})
    stage = api._stage(hall, actors, field)
    definitions = []
    for mineral in (2, 4, 6, 9):
        definition = old_definitions[str(mineral)]
        definition.set_editor_property('regions', [a['tag'] for a in recipe['areas'] if mineral in a['ids']])
        definitions.append(definition)
    stage.set_editor_property('definitions', definitions)
    stage.set_editor_property('authored_placements', retained)
    assert unreal.MiningDesignerLibrary.prepare_campaign_minerals(stage), 'Mineral burial failed; original map has not been saved'
    validation = api.validate(stage, hall)
    assert validation['ok'], validation.get('errors')
    api._commit(stage, field, actors)
    field.set_actor_label('MAIN_GAME_矿石背包配置')
    # Only actors owned by the persistent level and the removed mine are deleted.
    # Streamed museum/office/journey source actors are never changed.
    remove = set()
    for actor in all_actors:
        if actor.get_level() != persistent:
            continue
        if actor.actor_has_tag('BM_Campaign_MainGame04') or actor.actor_has_tag('BM_MainGame_NeuralV4') or 'Museum_Mine04' in actor.get_actor_label():
            remove.add(actor)
            if isinstance(actor, unreal.MiningExternalRegion):
                for child in [actor.generated_rock, actor.generated_shell, *actor.generated_guide_layers]:
                    if child: remove.add(child)
    areas = [a for a in hall.areas if str(a.tag) != 'BM_Campaign_MainGame04']
    for area in areas:
        if str(area.tag) in {a['tag'] for a in recipe['areas']}:
            area.set_editor_property('instructions', '沿碎晶线索挖出矿石，瞄准后按 E 收入背包；按 Tab 消耗材料升级矿镐。')
    assert len(areas) == 6 and str(areas[-1].tag) == 'BM_Campaign_MainGameMuseum'
    museum = areas[-1]
    if museum.progress_gate:
        remove.add(museum.progress_gate)
        museum.set_editor_property('progress_gate', None)
    museum.set_editor_property('instructions', '完成镜像博物馆的谜题，即完成本次旅程。')
    museum.set_editor_property('map_route', [])
    areas[-1] = museum
    hall.set_editor_property('areas', areas, notify_mode=unreal.PropertyAccessChangeNotifyMode.NEVER)
    report['removed'] = sorted(a.get_actor_label() for a in remove)
    for actor in remove:
        if unreal.SystemLibrary.is_valid(actor): assert actors.destroy_actor(actor)
    # Seal the former east exit using a non-diggable wall matching the museum.
    label = 'MAIN_GAME_Museum_TerminusWall'
    wall = next((a for a in actors.get_all_level_actors() if a.get_actor_label() == label), None)
    spec = next(r for r in cfg['rooms'] if r['id'] == 'Museum')
    origin = unreal.Vector(*spec['origin'])
    if not wall: wall = actors.spawn_actor_from_class(unreal.StaticMeshActor, origin + unreal.Vector(2460, 0, 210))
    wall.set_actor_label(label)
    wall.set_editor_property('tags', ['BM_Campaign_MainGameMuseum', 'BM_MainGame_MaterialProgression'])
    wall.static_mesh_component.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Cube'))
    wall.static_mesh_component.set_material(0, unreal.load_asset('/Game/WenSi/Museum/Materials/MI_Limestone'))
    wall.set_actor_scale3d(unreal.Vector(.4, 4.4, 4.2))
    wall.static_mesh_component.set_collision_profile_name('BlockAll')
    wall.set_actor_enable_collision(True)
    assert len([a for a in actors.get_all_level_actors() if isinstance(a, unreal.MiningExternalRegion) and a.actor_has_tag('BM_MainGame_RoutesV2')]) == 3
    assert not any(a.actor_has_tag('BM_Campaign_MainGame04') for a in actors.get_all_level_actors())
    assert api.validate(field, hall)['ok']
    assert levels.save_current_level()
    for source, digest in source_hashes.items():
        assert hashlib.sha256((ROOT / 'Content' / (source[6:] + '.umap')).read_bytes()).hexdigest() == digest, source
    for key in ('mine_deltas', 'mine_origins', 'mine_entry_positions'):
        cfg[key] = cfg[key][:3]
    for key in ('connections', 'generated_connections'):
        cfg[key] = [c for c in cfg[key] if c['mine'] < 3]
    cfg['rooms'][-1]['hint'] = '完成镜像博物馆的谜题，即完成本次旅程。'
    cfg['terminal_room'] = 'Museum'
    (ROOT / 'Config/MainGameRooms.json').write_text(json.dumps(cfg, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    path = ROOT / 'Config/MainGameRoutes.json'
    routes = json.loads(path.read_text())
    for key in ('stages', 'styles', 'guide_slots', 'active_guide_indices'): routes[key] = routes[key][:3]
    routes['terminal_room'] = 'Museum'
    path.write_text(json.dumps(routes, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    path = ROOT / 'Config/MainGameTuning.json'
    tuning = json.loads(path.read_text()); tuning['expected_deposits'] = len(retained)
    tuning['mineral_authoring_script'] = 'Scripts/apply_main_game_material_progression.py'
    path.write_text(json.dumps(tuning, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    report.update(passed=True, saved=True, distributions=distributions, minerals=validation, source_maps_unchanged=True)
except Exception:
    report['error'] = traceback.format_exc()
    unreal.log_error(report['error'])
finally:
    (OUT / 'authoring.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    unreal.SystemLibrary.execute_console_command(ed.get_editor_world(), 'QUIT_EDITOR')
