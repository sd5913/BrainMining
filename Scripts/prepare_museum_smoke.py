import unreal,pathlib,json
source='/Game/NiagaraExamples/FX_Smoke/NS_Smoke_Plume'
dest='/Game/WenSi/Museum/Refresh/VFX/NS_MuseumSmoke'
if not unreal.EditorAssetLibrary.does_asset_exist(dest):assert unreal.EditorAssetLibrary.duplicate_asset(source,dest)
smoke=unreal.load_asset(dest);assert smoke
# The saved component supplies a hard package reference for cooking, while
# the native exhibit owns its activation, word response and collision proxies.
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
for a in actors:
 if isinstance(a,unreal.WenSiMirrorExhibit) and a.get_editor_property('kind')==4:
  comp=next((c for c in a.get_components_by_class(unreal.NiagaraComponent) if c.get_name()=='MuseumSmoke'),None)
  if comp:comp.set_asset(smoke)
assert unreal.EditorAssetLibrary.save_loaded_asset(smoke)
unreal.get_editor_subsystem(unreal.AssetEditorSubsystem).open_editor_for_assets([smoke])
assert unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True,True)
print('MUSEUM_SMOKE_READY',dest)
