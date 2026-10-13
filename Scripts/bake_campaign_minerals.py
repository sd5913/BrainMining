"""Separate-editor CLI adapter. See Docs/MineralAuthoring.md. Import has no side effects."""
import json
import re
import shutil
import sys
import traceback
from datetime import datetime, timezone
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir()).resolve()
if str(root/'Scripts') not in sys.path:
    sys.path.insert(0, str(root/'Scripts'))
import campaign_minerals_api as minerals


def main():
    command = unreal.SystemLibrary.get_command_line()
    def option(name, default=None):
        found = re.search(r'(?:^|\s)-'+name+r'=(?:"([^"]+)"|([^\s]+))', command, re.IGNORECASE)
        return (found.group(1) or found.group(2)) if found else default
    def flag(name):
        return bool(re.search(r'(?:^|\s)-'+name+r'(?:\s|$)', command, re.IGNORECASE))
    def map_file(package):
        if not re.fullmatch(r'/Game/[A-Za-z0-9_/]+', package):
            raise ValueError('Map must be a /Game/... package path: '+package)
        return root/'Content'/(package[6:]+'.umap')
    output_dir = root/'Saved/MineralAuthoring'
    output_dir.mkdir(parents=True, exist_ok=True)
    report = {'ok': False, 'saved': False, 'errors': [], 'started_utc': datetime.now(timezone.utc).isoformat()}
    # A crashed run must not leave a previous success looking like this run's result.
    (output_dir/'latest.json').write_text(json.dumps(report), encoding='utf-8')
    try:
        mode = option('MineralsMode', 'validate').lower()
        if mode not in ('validate', 'rebuild', 'clues'):
            raise ValueError('MineralsMode must be validate, rebuild or clues')
        save = flag('MineralsSave')
        if mode == 'validate' and save:
            raise ValueError('validate is read-only; remove -MineralsSave')
        recipe_file = Path(option('MineralsRecipe', str(root/'Config/CampaignMinerals.json')))
        if not recipe_file.is_absolute():
            recipe_file = root/recipe_file
        layout = json.loads((root/'Config/CampaignLayout.json').read_text(encoding='utf-8'))
        source = option('MineralsMap', layout['map'])
        destination = option('MineralsOutput', source)
        report.update(mode=mode, source=source, output=destination)
        source_file, target_file = map_file(source), map_file(destination)
        if destination in {s['source'] for s in layout['stages']}:
            raise ValueError('Output must not overwrite a gameplay source map')
        if unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world():
            raise ValueError('Stop PIE before running mineral authoring')
        if unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages() or unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages():
            raise ValueError('Save current editor changes before using the CLI adapter; use the imported API to edit the current map')
        if not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level(source):
            raise ValueError('Cannot load '+source)
        if save:
            backup = root/'Saved/CampaignBackups'/('mineral-authoring-'+datetime.now().strftime('%Y%m%d-%H%M%S-%f'))
            backup.mkdir(parents=True)
            shutil.copy2(source_file, backup/'Source.umap')
            if destination != source and target_file.exists():
                shutil.copy2(target_file, backup/'PreviousOutput.umap')
            shutil.copy2(recipe_file, backup/'CampaignMinerals.json')
            report['backup'] = str(backup)
        if mode == 'rebuild':
            _, result = minerals.rebuild(recipe=recipe_file)
        elif mode == 'clues':
            _, result = minerals.rebuild_clues(preserve_surface=not flag('MineralsRefreshSurface'))
        else:
            result = minerals.validate()
        report.update(result, mode=mode, source=source, output=destination)
        if not report['ok']:
            raise minerals.MineralBakeError('Validation failed; no map saved.', report)
        if save:
            world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
            if not unreal.EditorLoadingAndSavingUtils.save_map(world, destination):
                raise ValueError('Validated, but map save failed (file lock or permissions); see backup')
            report['saved'] = True
        unreal.log('MINERAL_AUTHORING_OK '+json.dumps({k: report[k] for k in ('mode', 'deposits', 'fragments', 'layout_sha256', 'saved')}))
    except Exception as error:
        if isinstance(error, minerals.MineralBakeError):
            report.update(error.report)
        report['ok'] = False
        report.setdefault('errors', []).append(str(error))
        report['traceback'] = traceback.format_exc()
        unreal.log_error('MINERAL_AUTHORING_FAILED '+str(error))
    finally:
        (output_dir/'latest.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
        unreal.SystemLibrary.quit_editor()


if __name__ == '__main__':
    main()
