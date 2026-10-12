"""Read authored anchors and bounds without saving any source map."""
import json
from pathlib import Path
import unreal

levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
result = {}
for path in ['/Game/ExternalMining/Maps/Lvl_ExternalMining_Demo', '/Game/PopupClose/Maps/Lvl_PopupClose', '/Game/WenSi/Maps/Lvl_WenSi_ScreenBridge']:
    assert levels.load_level(path)
    rows = []
    for a in actors.get_all_level_actors():
        p = a.get_actor_location()
        row = dict(label=a.get_actor_label(), cls=a.get_class().get_name(), pos=[p.x,p.y,p.z])
        if isinstance(a, unreal.MiningExternalRegion):
            row['cutters'] = [str(c.local_transform) for c in a.cutters]
            row['size'] = str(a.rock_size)
        if isinstance(a, unreal.GeneratedMiningRock):
            b,e = a.get_actor_bounds(False)
            row['bounds'] = [[b.x,b.y,b.z],[e.x,e.y,e.z]]
        if isinstance(a, unreal.PlayerStart):
            row['rotation'] = str(a.get_actor_rotation())
        rows.append(row)
    result[path] = rows
Path(unreal.Paths.project_saved_dir(), 'CampaignSourceAudit.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
unreal.SystemLibrary.quit_editor()
