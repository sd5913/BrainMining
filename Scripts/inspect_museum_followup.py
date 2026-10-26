import unreal,json,pathlib,shutil
out=pathlib.Path(unreal.Paths.project_saved_dir())/'Validation/MuseumAssetRefresh/Followup'
out.mkdir(parents=True,exist_ok=True)
assert not unreal.EditorLevelLibrary.get_pie_worlds(False)
assert unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True,True)
src=pathlib.Path(unreal.Paths.project_content_dir())/'WenSi/Maps/Lvl_WenSi_MirrorMuseum1.umap'
if not (out/src.name).exists():shutil.copy2(src,out/src.name)
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
museum=[a for a in actors if '/Game/WenSi/Maps/Lvl_WenSi_MirrorMuseum1.' in a.get_path_name()]
report=[]
for a in museum:
 cs=a.get_components_by_class(unreal.StaticMeshComponent)
 if not cs:continue
 report.append({'name':a.get_name(),'label':a.get_actor_label(),'class':a.get_class().get_name(),'loc':a.get_actor_location().to_tuple(),'rot':a.get_actor_rotation().to_tuple(),'scale':a.get_actor_scale3d().to_tuple(),'tags':[str(t) for t in a.tags],'kind':a.get_editor_property('kind') if isinstance(a,unreal.WenSiMirrorExhibit) else None,'components':[{'name':c.get_name(),'mesh':c.static_mesh.get_path_name() if c.static_mesh else None,'visible':c.is_visible(),'render':c.get_editor_property('render_in_main_pass'),'collision':str(c.get_collision_enabled()),'bounds':str(c.get_local_bounds()),'materials':[c.get_material(i).get_path_name() if c.get_material(i) else None for i in range(c.get_num_materials())]} for c in cs]})
(out/'actors_before.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
print('FOLLOWUP_INSPECTED',len(museum),len(report))

