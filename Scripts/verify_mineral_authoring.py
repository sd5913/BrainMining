"""Integration regression in a separate editor. Main map is never saved.

Checks existing 131 deposits, invalid recipes, generation failure preservation,
repeatable rebuilding, clue-only preservation, and save/reload in a scratch map.
"""
import copy
import hashlib
import json
import sys
import traceback
import uuid
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir()).resolve()
sys.path.insert(0, str(root/'Scripts'))
import campaign_minerals_api as minerals

result = {'ok': False}
source_file = root/'Content/Campaign/Maps/Lvl_FullJourney.umap'
original_hash = hashlib.sha256(source_file.read_bytes()).hexdigest()
try:
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    assert levels.load_level('/Game/Campaign/Maps/Lvl_FullJourney')
    field = minerals.get_field()
    baseline = minerals.validate(field)
    assert baseline['ok'], baseline['errors']
    result['existing_deposits'] = baseline['deposits']
    recipe = json.loads((root/'Config/CampaignMinerals.json').read_text(encoding='utf-8'))
    full = '-MineralAuthoringFull' in unreal.SystemLibrary.get_command_line().split()
    if not full:
        recipe['bands'] = [copy.deepcopy(recipe['bands'][0])]
        recipe['bands'][0].update(ids=[1], weights=[1], budget=2, value_weights=[1, 0, 0, 0])
    result['full_recipe'] = full
    bad = copy.deepcopy(recipe)
    bad['values'][0][0] = 0
    try:
        minerals.rebuild(recipe=bad)
        raise AssertionError('Invalid recipe was accepted')
    except ValueError:
        pass
    assert minerals.validate(field)['layout_sha256'] == baseline['layout_sha256']
    # Fail after staging has been allocated: original actor/content must survive.
    original_stage = minerals._stage
    def unusable_stage(hall, actors, original):
        stage = original_stage(hall, actors, original)
        stage.set_editor_property('definitions', [])
        return stage
    minerals._stage = unusable_stage
    try:
        minerals.rebuild(recipe=recipe)
        raise AssertionError('Invalid staging field was accepted')
    except minerals.MineralBakeError:
        pass
    finally:
        minerals._stage = original_stage
    assert minerals.get_field() == field
    assert minerals.validate(field)['layout_sha256'] == baseline['layout_sha256']
    result['failure_preserves_original'] = True
    field, first = minerals.rebuild(recipe=recipe)
    result['rebuilt_deposits'] = first['deposits']
    result['rebuilt_fragments'] = first['fragments']
    _, second = minerals.rebuild(recipe=recipe)
    assert first['layout_sha256'] == second['layout_sha256'], 'Rebuild drift'
    result['repeatable_rebuild'] = True
    _, clues = minerals.rebuild_clues(preserve_surface=True)
    _, clues_again = minerals.rebuild_clues(preserve_surface=True)
    assert clues['layout_sha256'] == clues_again['layout_sha256'], 'Clue-only rebuild drift'
    result['repeatable_clues_preserve_surface'] = True
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    scratch = '/Game/Developers/MineralAuthoringTest/Lvl_BakedMineralTest_' + uuid.uuid4().hex
    scratch_file = root/'Content'/(scratch[6:]+'.umap')
    assert unreal.EditorLoadingAndSavingUtils.save_map(world, scratch)
    assert levels.load_level('/Engine/Maps/Entry')
    assert levels.load_level(scratch)
    reloaded = minerals.validate()
    assert reloaded['ok'], reloaded['errors']
    assert reloaded['layout_sha256'] == clues_again['layout_sha256']
    result['saved_reload_identical'] = True
    result['sample'] = reloaded
    assert levels.load_level('/Engine/Maps/Entry')
    assert unreal.EditorAssetLibrary.delete_asset(scratch)
    # UE's deletion can leave an untracked .umap on disk with source control enabled.
    # Only remove the uniquely named file created by this test.
    scratch_file.unlink(missing_ok=True)
    result['ok'] = True
except Exception:
    result['traceback'] = traceback.format_exc()
    unreal.log_error(result['traceback'])
finally:
    result['source_map_unchanged'] = hashlib.sha256(source_file.read_bytes()).hexdigest() == original_hash
    result['ok'] &= result['source_map_unchanged']
    output = root/'Saved/MineralAuthoring/integration.json'
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding='utf-8')
    unreal.log('MINERAL_AUTHORING_INTEGRATION '+str(result['ok']))
    unreal.SystemLibrary.quit_editor()
