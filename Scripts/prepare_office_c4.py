"""Back up the office code and create a separate C4 for batched finale visuals."""
import unreal,json,shutil,hashlib
from pathlib import Path
from datetime import datetime
root=Path(unreal.Paths.project_dir()).resolve();out=root/'Saved/Validation/OfficeC4';out.mkdir(parents=True,exist_ok=True)
assert not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
backup=root/'Saved/Backups'/('OfficeC4_'+datetime.now().strftime('%Y%m%d-%H%M%S'));backup.mkdir(parents=True)
for rel in ['Source/BrainMining/PopupClose/PopupOfficeExit.cpp','Source/BrainMining/Tests/PopupOfficeExitTests.cpp']:
    shutil.copy2(root/rel,backup/Path(rel).name)
path='/Game/PopupClose/FX/SM_OfficeFinaleC4'
assert not unreal.EditorAssetLibrary.does_asset_exist(path),'C4 finale asset already exists; inspect it instead of overwriting'
mesh=unreal.EditorAssetLibrary.duplicate_asset('/Game/Bomb/SM_StickyBomb',path);assert mesh
assert unreal.EditorAssetLibrary.save_loaded_asset(mesh,False)
protected=['Content/Bomb/SM_StickyBomb.uasset','Content/MainGame/Maps/Lvl_MainGame.umap','Content/MainGame/Blueprints/BP_MainGameCharacter.uasset','Content/PopupClose/Maps/Lvl_PopupClose1.umap']
hashes={p:hashlib.sha256((root/p).read_bytes()).hexdigest() for p in protected if (root/p).exists()}
report=dict(backup=str(backup),protected_hashes=hashes,asset=path,source_triangles=mesh.get_num_triangles(0),dirty_maps=[p.get_path_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()],dirty_content=[p.get_path_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()])
(out/'preparation.json').write_text(json.dumps(report,indent=2),encoding='utf-8');unreal.log(json.dumps(report))
