"""Update only the three encounter map GameMode overrides, preserving their actors/content."""
import unreal
from pathlib import Path
import shutil
maps=[
 '/Game/Passages/Echo/Maps/Lvl_EchoPassage_P1',
 '/Game/Passages/Echo/Maps/Lvl_EchoMazeToCards_P1',
 '/Game/Rooms/CardDuel/Maps/Lvl_CardRoom_P1']
root=Path(unreal.Paths.project_dir())
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
for name in maps:
    source=root/'Content'/(name.removeprefix('/Game/')+'.umap')
    backup=root/'Saved/EncounterSharedPlayerBackup'/source.relative_to(root)
    backup.parent.mkdir(parents=True,exist_ok=True)
    if not backup.exists():shutil.copy2(source,backup)
    if not levels.load_level(name):raise RuntimeError('Cannot load '+name)
    world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    world.get_world_settings().set_editor_property('default_game_mode',unreal.MiningEncounterGameMode)
    if not levels.save_current_level():raise RuntimeError('Cannot save '+name)
    unreal.log('ENCOUNTER_SHARED_PLAYER_MAP: '+name)
unreal.log('ENCOUNTER_SHARED_PLAYER_INTEGRATED')
