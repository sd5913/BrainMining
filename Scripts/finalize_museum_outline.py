import unreal,json,pathlib
assert not unreal.EditorLevelLibrary.get_pie_worlds(False)
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
report=[]
for a in actors:
 if not isinstance(a,unreal.WenSiMirrorExhibit) or '/Game/WenSi/Maps/Lvl_WenSi_MirrorMuseum1.' not in a.get_path_name():continue
 transform=a.get_actor_transform();a.modify();a.refresh_art_model();a.set_actor_transform(transform,False,True)
 c=a.get_editor_property('art_visual')
 report.append({'kind':a.get_editor_property('kind'),'mesh':str(c.static_mesh),'custom_depth':c.get_editor_property('render_custom_depth'),'overlay':str(c.get_overlay_material()),'parts':[{'name':p.get_name(),'custom_depth':p.get_editor_property('render_custom_depth'),'rendered':p.get_editor_property('render_in_main_pass'),'overlay':str(p.get_overlay_material())} for p in a.get_components_by_class(unreal.StaticMeshComponent) if p.get_name().startswith('Part')]})
assert unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True,True)
ar=unreal.AssetRegistryHelpers.get_asset_registry();opts=unreal.AssetRegistryDependencyOptions(include_hard_package_references=True,include_soft_package_references=True)
deps=[str(v) for v in ar.get_dependencies('/Game/WenSi/Maps/Lvl_WenSi_MirrorMuseum1',opts)]
for p in ['/Game/WenSi/Museum/Refresh/Meshes/SM_AntiqueScreen','/Game/WenSi/Museum/Materials/M_MuseumPassable','/Game/CollectionPrototype/Materials/M_CollectionSilhouette']:assert p in deps,p
(pathlib.Path(unreal.Paths.project_saved_dir())/'Validation/MuseumOutline/final_resources.json').write_text(json.dumps({'actors':report,'dependencies':deps,'saved':True},indent=2))
print('MUSEUM_OUTLINE_SAVED',len(report))
