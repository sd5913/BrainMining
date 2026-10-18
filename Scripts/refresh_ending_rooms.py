"""Rebuild the existing ending map after room geometry changes, with a backup."""
from datetime import datetime
from pathlib import Path
import shutil
import unreal

project = Path(unreal.Paths.project_dir()).resolve()
map_path = '/Game/EndingEmergence/Maps/Lvl_EndingEmergence_Demo'
source = project / 'Content/EndingEmergence/Maps/Lvl_EndingEmergence_Demo.umap'
backup = project / 'Saved/Backups' / ('EndingRooms-' + datetime.now().strftime('%Y%m%d-%H%M%S') + '.umap')
backup.parent.mkdir(parents=True, exist_ok=True)
shutil.copy2(source, backup)

levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert levels.load_level(map_path)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
demo = next(a for a in actors.get_all_level_actors() if isinstance(a, unreal.EndingEmergence))
demo.build_demo()
gate = demo.get_editor_property('gates')[0]
assert abs(gate.get_editor_property('rock_size').y - 1300) < 1
assert levels.save_current_level()
unreal.log('ENDING_ROOMS_READY: half-size sealed exit, clean cell interior, blue goal; backup=' + str(backup))
