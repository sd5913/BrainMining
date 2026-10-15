"""Leave the editor in MainGame and verify the authored C4 survived the restart."""
import unreal,json,hashlib
from pathlib import Path
root=Path(unreal.Paths.project_dir()).resolve();out=root/'Saved/Validation/OfficeC4'
assert not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level('/Game/MainGame/Maps/Lvl_MainGame')
mesh=unreal.load_asset('/Game/PopupClose/FX/SM_OfficeFinaleC4');assert mesh.get_num_lods()==2 and mesh.get_num_triangles(1)==721
prep=json.loads((out/'preparation.json').read_text(encoding='utf-8'))
for p,digest in prep['protected_hashes'].items():assert hashlib.sha256((root/p).read_bytes()).hexdigest()==digest,p
(out/'saved.json').write_text(json.dumps({'passed':True,'level':'/Game/MainGame/Maps/Lvl_MainGame','lod_triangles':721,'protected_assets_unchanged':True}),encoding='utf-8');unreal.log('OFFICE_C4_FINAL_SAVED_CHECK_PASSED')
