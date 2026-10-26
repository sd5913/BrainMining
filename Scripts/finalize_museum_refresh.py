import unreal,json,pathlib,shutil
root=pathlib.Path(unreal.Paths.project_dir());out=root/'Saved/Validation/MuseumAssetRefresh'
assert not unreal.EditorLevelLibrary.get_pie_worlds(False)
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world.get_name()=='Lvl_MainGame'
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
for a in actors:
 if isinstance(a,unreal.WenSiMirrorExhibit) and a.get_editor_property('kind') in (1,3,4,7,9):
  a.modify();a.refresh_art_model()
assert unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True,True)
# Record the exact dependency closure before archiving unrelated demo content.
ar=unreal.AssetRegistryHelpers.get_asset_registry();options=unreal.AssetRegistryDependencyOptions(include_soft_package_references=True,include_hard_package_references=True,include_searchable_names=False,include_soft_management_references=False,include_hard_management_references=False)
seen=set();pending=['/Game/WenSi/Museum/Refresh/VFX/NS_MuseumSmoke']
while pending:
 p=pending.pop()
 if p in seen:continue
 seen.add(p)
 for dep in ar.get_dependencies(p,options):
  path=str(dep)
  if path.startswith('/Game/NiagaraExamples/') and path not in seen:pending.append(path)
(out/'smoke_dependencies.json').write_text(json.dumps(sorted(seen),indent=2))
smoke=unreal.load_asset('/Game/WenSi/Museum/Refresh/VFX/NS_MuseumSmoke')
unreal.get_editor_subsystem(unreal.AssetEditorSubsystem).open_editor_for_assets([smoke])
print('MUSEUM_REFRESH_FINALIZED',len(seen))
