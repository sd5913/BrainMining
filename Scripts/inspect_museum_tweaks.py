import unreal,pathlib,json,shutil
out=pathlib.Path(unreal.Paths.project_saved_dir())/'Validation/MuseumTweaks';out.mkdir(parents=True,exist_ok=True)
assert not unreal.EditorLevelLibrary.get_pie_worlds(False)
assert unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True,True)
backup=out/'Lvl_WenSi_MirrorMuseum1_before.umap'
if not backup.exists():shutil.copy2(pathlib.Path(unreal.Paths.project_content_dir())/'WenSi/Maps/Lvl_WenSi_MirrorMuseum1.umap',backup)
actors=[a for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors() if '/Game/WenSi/Maps/Lvl_WenSi_MirrorMuseum1.' in a.get_path_name()]
r={'exhibits':[],'stand':[],'portals':[]}
for a in actors:
 label=a.get_actor_label()
 if isinstance(a,unreal.WenSiMirrorExhibit) and a.get_editor_property('kind') in (6,12):
  r['exhibits'].append({'kind':a.get_editor_property('kind'),'location':a.get_actor_location().to_tuple(),'actor_rotation':a.get_actor_rotation().to_tuple(),'art_rotation':a.get_editor_property('art_rotation').to_tuple()})
 if label.startswith(('Hopper','CatchTray','ContactInsulator','BallContactPedestal')):
  r['stand'].append({'label':label,'path':a.get_path_name(),'meshes':[{'name':c.get_name(),'material':str(c.get_material(0))} for c in a.get_components_by_class(unreal.StaticMeshComponent)]})
 if label.startswith(('Portal','Corridor')):r['portals'].append({'label':label,'location':a.get_actor_location().to_tuple()})
p=unreal.load_asset('/Game/Museum/Materials/M_PBRMaster');lib=unreal.MaterialEditingLibrary
r['parameters']={n:[str(v) for v in getattr(lib,'get_'+n+'_parameter_names')(p)] for n in ('scalar','vector','static_switch')}
(out/'before.json').write_text(json.dumps(r,ensure_ascii=False,indent=2),encoding='utf8');print('MUSEUM_TWEAKS_INSPECTED')
