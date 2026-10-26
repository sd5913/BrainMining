import unreal,json,pathlib
ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem);w=ed.get_game_world() or ed.get_editor_world()
actors=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.WenSiMirrorExhibit) if ed.get_game_world() else unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
out=[]
for a in actors:
 if isinstance(a,unreal.WenSiMirrorExhibit) and a.get_editor_property('kind') in (5,8):
  out.append({'label':a.get_actor_label(),'word':str(a.get_editor_property('word')),'loc':str(a.get_actor_location()),'scale':str(a.get_actor_scale3d()),'rotation':str(a.get_editor_property('art_rotation')),'components':[{'name':c.get_name(),'transform':str(c.get_world_transform()),'bounds':str(c.get_local_bounds()),'visible':c.is_visible(),'render':c.get_editor_property('render_in_main_pass'),'materials':[str(c.get_material(i)) for i in range(c.get_num_materials())]} for c in a.get_components_by_class(unreal.StaticMeshComponent) if c.get_name()=='ArtVisual' or c.is_visible()]})
(pathlib.Path(unreal.Paths.project_saved_dir())/'Validation/MuseumAssetRefresh/Followup/art_inspection.json').write_text(json.dumps(out,indent=2))
