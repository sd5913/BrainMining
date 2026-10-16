"""Save and back up the current localized excavation material before refinement."""
import unreal,json,shutil,hashlib
from pathlib import Path
from datetime import datetime
ROOT=Path(unreal.Paths.project_dir()).resolve();OUT=ROOT/'Saved/Validation/MainGameMoss';OUT.mkdir(parents=True,exist_ok=True)
ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem);levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert not ed.get_game_world();assert levels.save_current_level()
folder='/Game/MainGame/Materials/DirtTransition'
assert unreal.EditorAssetLibrary.save_directory(folder,False,True)
backup=ROOT/'Saved/Backups'/('MainGame_Moss_'+datetime.now().strftime('%Y%m%d-%H%M%S'));backup.mkdir(parents=True)
shutil.copytree(ROOT/'Content/MainGame/Materials/DirtTransition',backup/'DirtTransition')
for name in ['Art/MainGame/Materials/DryRockMoss.hlsl','Config/MainGameTuning.json']:shutil.copy2(ROOT/name,backup/Path(name).name)
report=dict(backup=str(backup),map_sha256=hashlib.sha256((ROOT/'Content/MainGame/Maps/Lvl_MainGame.umap').read_bytes()).hexdigest(),original_world=ed.get_editor_world().get_path_name())
(OUT/'authoring.json').write_text(json.dumps(report,indent=2),encoding='utf-8');unreal.log('MOSS_BACKUP '+json.dumps(report))
