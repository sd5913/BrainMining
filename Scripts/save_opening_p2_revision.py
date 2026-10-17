import unreal,json,hashlib
from pathlib import Path
L=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
E=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
assert not E.get_game_world(),'Stop PIE before saving the editable scene'
root=Path(unreal.Paths.project_dir());out=root/'Saved/Validation/IntroP2Office'
assert unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True,True),'Save failed'
poison=root/'Content/Passages/Echo/ArtP2/VFX/NS_P2_Poison.uasset'
remaining=unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
d=dict(saved=True,current_level=L.get_current_level().get_outer().get_path_name(),dirty_maps=[p.get_path_name() for p in remaining],poison_unchanged=hashlib.sha256(poison.read_bytes()).hexdigest()==(out/'poison_sha256_before.txt').read_text().strip())
(out/'saved_final.json').write_text(json.dumps(d,indent=2),encoding='utf8')
assert not remaining,d
assert d['poison_unchanged'],d
unreal.log('OPENING_P2_REVISION_SAVED '+json.dumps(d))
