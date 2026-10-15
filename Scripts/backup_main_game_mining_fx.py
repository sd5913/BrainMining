"""Preserve the reproduced state and source before restarting the editor."""
import unreal,json,hashlib,shutil
from pathlib import Path
from datetime import datetime
root=Path(unreal.Paths.project_dir()).resolve();out=root/'Saved/Validation/MainGameMiningFX'
assert not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()
backup=root/'Saved/Backups'/('MainGameMiningFX_'+datetime.now().strftime('%Y%m%d-%H%M%S'));backup.mkdir(parents=True)
file=root/'Source/BrainMining/MiningDrillDustBurst.cpp'
code=file.read_text(encoding='utf-8').replace('\t// Impact effects follow the player, even where the nearest area origin\n\t// belongs to a neighbouring room or another winding route.\n\tTags.AddUnique(TEXT("BM_PlayerEffect"));\n','')
(backup/file.name).write_text(code,encoding='utf-8')
shutil.copy2(out/'observations.json',out/'reproduced_before.json')
protected=['Content/MainGame/Maps/Lvl_MainGame.umap','Content/MainGame/Blueprints/BP_MainGameCharacter.uasset','Content/PopupClose/FX/SM_OfficeFinaleC4.uasset']
report=dict(passed=True,backup=str(backup),protected_hashes={p:hashlib.sha256((root/p).read_bytes()).hexdigest() for p in protected})
(out/'prebuild.json').write_text(json.dumps(report,indent=2),encoding='utf-8');unreal.log('MINING_FX_SAVED_FOR_REBUILD')
