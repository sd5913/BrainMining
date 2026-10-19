"""Validate only the three relocated deposits against the saved terrain, without saving."""
import unreal,json
from pathlib import Path
root=Path(unreal.Paths.project_dir())
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
api=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
assert levels.load_level('/Game/MainGame/Maps/Lvl_MainGame')
field=next(a for a in api.get_all_level_actors() if isinstance(a,unreal.MiningTreasureField))
placements=list(field.get_editor_property('authored_placements'))
field.set_editor_property('authored_placements',[placements[i] for i in [4,6,64]])
report=json.loads(unreal.MiningDesignerLibrary.validate_campaign_minerals(field))
report['source_indices']=[4,6,64]
field.set_editor_property('authored_placements',placements)
(root/'Saved/Validation/PackagePlaytest/minerals.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
unreal.SystemLibrary.quit_editor()
