import unreal,pathlib,json
assert not unreal.EditorLevelLibrary.get_pie_worlds(False)
assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).set_current_level_by_name('Lvl_MainGame')
assert unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True,True)
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
museum=[a for a in actors if '/Game/WenSi/Maps/Lvl_WenSi_MirrorMuseum1.' in a.get_path_name()]
ar=unreal.AssetRegistryHelpers.get_asset_registry();opts=unreal.AssetRegistryDependencyOptions(include_soft_package_references=True,include_hard_package_references=True)
deps=[str(d) for d in ar.get_dependencies('/Game/WenSi/Maps/Lvl_WenSi_MirrorMuseum1',opts)]
assert '/Game/WenSi/Museum/Refresh/Meshes/SM_BronzeShark' in deps
assert '/Game/WenSi/Museum/Refresh/Meshes/SM_FoldingScreen' in deps
assert '/Game/Museum/Materials/MI_Stucco01' in deps
out=pathlib.Path(unreal.Paths.project_saved_dir())/'Validation/MuseumAssetRefresh/Followup'
(out/'final_resources.json').write_text(json.dumps({'source_sublevel':'/Game/WenSi/Maps/Lvl_WenSi_MirrorMuseum1','museum_actor_count':len(museum),'dependencies':deps,'all_saved':True},indent=2))
print('FOLLOWUP_FINALIZED',len(museum))
