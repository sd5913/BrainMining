"""Editor-only public API for fixed, baked mineral layouts. Import has no side effects.

See Docs/MineralAuthoring.md. No map loading, saving, art rebuilding or editor exit
is performed by this module. The command-line adapter owns those operations.
"""
import hashlib
import json
import math
from pathlib import Path

import unreal
from campaign_mineral_plan import make_plan


class MineralBakeError(RuntimeError):
    def __init__(self, message, report=None):
        super().__init__(message)
        self.report = report or {'ok': False, 'errors': [message]}


def _context(hall=None):
    editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    if editor.get_game_world():
        raise MineralBakeError('Stop PIE before authoring minerals.')
    world = editor.get_editor_world()
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    halls = [a for a in actors.get_all_level_actors() if isinstance(a, unreal.MiningExhibitHall)]
    if hall is None:
        if len(halls) != 1:
            raise MineralBakeError('Expected exactly one MiningExhibitHall; pass hall explicitly.')
        hall = halls[0]
    if hall not in halls:
        raise MineralBakeError('Hall must belong to the current editor world.')
    return world, hall, actors


def get_field(hall=None):
    _, hall, actors = _context(hall)
    fields = [a for a in actors.get_all_level_actors()
              if isinstance(a, unreal.MiningTreasureField) and a.get_level() == hall.get_level()
              and a.get_editor_property('use_authored_placements')]
    if len(fields) > 1:
        raise MineralBakeError('Multiple authored mineral fields in the hall level; pass field explicitly.')
    return fields[0] if fields else None


def _field_context(hall, field):
    world, hall, actors = _context(hall)
    field = field if field is not None else get_field(hall)
    if field is not None and (not isinstance(field, unreal.MiningTreasureField)
                             or field not in actors.get_all_level_actors()
                             or field.get_level() != hall.get_level()):
        raise MineralBakeError('Field must be in the current hall level.')
    return world, hall, actors, field


def _xyz(v):
    return [v.x, v.y, v.z]


def _transform(t):
    q = t.rotation
    return [_xyz(t.translation), [q.x, q.y, q.z, q.w], _xyz(t.scale3d)]


def _layout(field):
    # Host object paths are deliberately excluded: Save As changes the package prefix.
    return [dict(id=str(p.treasure_id), area=str(p.area_tag), value=p.value,
                 progress=p.route_progress, guide=p.guide_layer, core=_transform(p.transform),
                 surface=_transform(p.surface), fragments=[_transform(t) for t in p.fragments],
                 supports=[_xyz(q) for q in p.fragment_supports])
            for p in field.get_editor_property('authored_placements')]


def validate(field=None, hall=None):
    """Read-only geometry, burial, surface-clue and interior-density validation."""
    _, hall, _, field = _field_context(hall, field)
    if field is None:
        return {'ok': False, 'errors': ['No authored mineral field.'], 'placements': []}
    report = json.loads(unreal.MiningDesignerLibrary.validate_campaign_minerals(field))
    layout = _layout(field)
    report.update(schema_version=1, field=field.get_path_name(), deposits=len(layout),
                  fragments=sum(len(p['fragments']) for p in layout),
                  layout_sha256=hashlib.sha256(json.dumps(layout, sort_keys=True, allow_nan=False).encode()).hexdigest())
    return report


def _stage(hall, actors, original):
    stage = actors.spawn_actor_from_class(unreal.MiningTreasureField, hall.get_actor_location())
    if not stage:
        raise MineralBakeError('Could not create a staging mineral field.')
    if stage.get_level() != hall.get_level():
        actors.destroy_actor(stage)
        raise MineralBakeError('Set the hall level (normally Persistent Level) as Current Level first.')
    stage.set_editor_property('use_authored_placements', True)
    stage.set_editor_property('use_shared_wallet', True)
    if original:
        for key in ('definitions', 'treasure_material', 'crystal_clue_mesh', 'random_seed'):
            stage.set_editor_property(key, original.get_editor_property(key))
    if not stage.get_editor_property('crystal_clue_mesh'):
        stage.set_editor_property('crystal_clue_mesh', unreal.load_asset('/Game/Showcase/Treasure/Meshes/SM_CrystalClue'))
    if not stage.get_editor_property('crystal_clue_mesh') or not stage.treasure_material:
        actors.destroy_actor(stage)
        raise MineralBakeError('Missing mineral art assets; restore them before baking.')
    return stage


def _commit(stage, original, actors):
    # Validate the staging actor first. Keep the original actor identity/references.
    if original:
        keys = ('definitions', 'random_seed', 'crystal_clue_mesh', 'use_shared_wallet', 'use_authored_placements', 'authored_placements')
        previous = {key: original.get_editor_property(key) for key in keys}
        try:
            for key in keys:
                original.set_editor_property(key, stage.get_editor_property(key))
        except Exception:
            for key in keys:
                original.set_editor_property(key, previous[key])
            raise
        actors.destroy_actor(stage)
        return original
    stage.set_actor_label('CAMPAIGN_Minerals_EditConfigToRebalance')
    return stage


def rebuild(hall=None, recipe=None, field=None):
    """Re-scan current terrain and replace gems+clues only after validation.

    Returns (field, report). Recipe is a dict or JSON filename. No save/quit.
    A fixed recipe and unchanged geometry produce the same layout.
    """
    world, hall, actors, original = _field_context(hall, field)
    root = Path(unreal.Paths.project_dir()).resolve()
    if recipe is None:
        recipe = root / 'Config/CampaignMinerals.json'
    if not isinstance(recipe, dict):
        recipe_path = Path(recipe)
        if not recipe_path.is_absolute():
            recipe_path = root / recipe_path
        recipe = json.loads(recipe_path.read_text(encoding='utf-8'))
    plan = make_plan(recipe)  # Preflight before modifying any actor.
    area_tags = {str(a.tag) for a in hall.areas if a.kind == unreal.MiningExperienceKind.MINING and len(a.map_route) >= 2}
    for phase in plan:
        if 'BM_Campaign_' + phase['band']['area'] not in area_tags:
            raise MineralBakeError('Recipe area missing a mining Area/MapRoute: ' + phase['band']['area'])
    candidates = list(unreal.MiningDesignerLibrary.find_campaign_mineral_sites(hall))
    selected, summary = [], []
    for phase in plan:
        band = phase['band']; lo, hi = band['range']; tag = 'BM_Campaign_' + band['area']
        sites = [p for p in candidates if str(p.area_tag) == tag and lo <= p.route_progress <= hi]
        for index, entry in enumerate(phase['entries']):
            progress = lo + (hi-lo)*(index+.5)/len(phase['entries'])
            available = [p for p in sites if all(math.dist(_xyz(p.transform.translation), _xyz(q.transform.translation)) >= band['spacing'] for q in selected)]
            if not available:
                raise MineralBakeError('Insufficient buried sites in %s/%s; check MapRoute, rock tags/collision, wall thickness, spacing or budget.' % (band['area'], band['phase']))
            p = min(available, key=lambda s: (abs(s.route_progress-progress)+(0 if s.guide_layer == (index % 5 < 2) else .065), *_xyz(s.transform.translation)))
            sites.remove(p)
            p.set_editor_property('treasure_id', str(entry['id'])); p.set_editor_property('value', entry['value'])
            selected.append(p)
        summary.append(dict(area=band['area'], phase=band['phase'], count=len(phase['entries']), total=phase['total'], budget=band['budget']))
    stage = _stage(hall, actors, original)
    try:
        definitions = list(stage.definitions)
        for d in definitions:
            mineral_id = int(str(d.id))
            values = recipe['values'][mineral_id-1]
            d.set_editor_property('values_by_size', values)
            d.set_editor_property('size_multipliers', [.7+.65*(v-values[0])/(values[-1]-values[0]) for v in values])
            d.set_editor_property('regions', sorted({'BM_Campaign_'+b['area'] for b in recipe['bands'] if mineral_id in b['ids']}))
        stage.set_editor_property('definitions', definitions)
        stage.set_editor_property('random_seed', recipe['seed'])
        stage.set_editor_property('authored_placements', selected)
        if not unreal.MiningDesignerLibrary.prepare_campaign_minerals(stage):
            raise MineralBakeError('Burial/clue generation failed; original field is unchanged. See Unreal log.')
        report = validate(stage, hall)
        if not report['ok']:
            raise MineralBakeError('Generated layout failed validation; original field is unchanged.', report)
        report.update(mode='rebuild', recipe=recipe, summary=summary)
        result = _commit(stage, original, actors)
        report['field'] = result.get_path_name()
        return result, report
    except Exception:
        actors.destroy_actor(stage)
        raise


def rebuild_clues(hall=None, field=None, preserve_surface=True):
    """Keep gem positions/values, rebuild clues. Use full rebuild after terrain changes.

    preserve_surface=True retains the exact accepted surface clues; False projects
    fresh clues onto current walls. Failed staging never replaces the original.
    """
    _, hall, actors, original = _field_context(hall, field)
    if original is None:
        raise MineralBakeError('No authored mineral field; run rebuild first.')
    before = _layout(original)
    stage = _stage(hall, actors, original)
    try:
        stage.set_editor_property('authored_placements', original.get_editor_property('authored_placements'))
        if not unreal.MiningDesignerLibrary.refine_campaign_crystal_clues(stage, preserve_surface):
            raise MineralBakeError('Clue generation failed; original field is unchanged. Check terrain thickness.')
        after = _layout(stage)
        for a, b in zip(before, after):
            if any(a[k] != b[k] for k in ('core', 'surface', 'id', 'value', 'area')):
                raise MineralBakeError('Clue-only bake unexpectedly changed a gem.')
            if preserve_surface:
                def outer(row):
                    return [(t, s) for t, s in zip(row['fragments'], row['supports']) if math.dist(t[0], s) > 1]
                if outer(a) != outer(b):
                    raise MineralBakeError('Preserved surface clues changed.')
        report = validate(stage, hall)
        if not report['ok']:
            raise MineralBakeError('Clue layout failed validation; original field is unchanged.', report)
        report.update(mode='clues', preserve_surface=preserve_surface)
        result = _commit(stage, original, actors)
        report['field'] = result.get_path_name()
        return result, report
    except Exception:
        actors.destroy_actor(stage)
        raise
