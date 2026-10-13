"""Compatibility adapter. See Docs/MineralAuthoring.md for the public API."""
import json
import sys
from pathlib import Path
import unreal


def populate(world, hall):
    root = Path(unreal.Paths.project_dir()).resolve()
    if str(root/'Scripts') not in sys.path:
        sys.path.insert(0, str(root/'Scripts'))
    import campaign_minerals_api as minerals
    if world != unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world():
        raise ValueError('populate requires the current editor world')
    field, report = minerals.rebuild(hall=hall)
    output = root/'Saved/CampaignMineralsManifest.json'
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    unreal.log('CAMPAIGN_MINERALS_READY '+str(report['deposits']))
    return field


if __name__ == '__main__':
    raise RuntimeError('Use bake_campaign_minerals.py -MineralsMode=rebuild -MineralsSave; this adapter is import-only.')
