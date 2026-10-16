"""Verify unchanged authored assets and leave MainGame ready for the user."""
import unreal,json,hashlib
from pathlib import Path
root=Path(unreal.Paths.project_dir()).resolve();out=root/'Saved/Validation/MainGameMiningFX'
assert not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level('/Game/MainGame/Maps/Lvl_MainGame')
before=json.loads((out/'prebuild.json').read_text(encoding='utf-8'))
for p,digest in before['protected_hashes'].items():assert hashlib.sha256((root/p).read_bytes()).hexdigest()==digest,p
(out/'saved.json').write_text(json.dumps({'passed':True,'level':'/Game/MainGame/Maps/Lvl_MainGame','protected_assets_unchanged':True}),encoding='utf-8');unreal.log('MINING_FX_FINAL_SAVED_CHECK_PASSED')
