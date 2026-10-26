"""Apply imported models to the actual museum sublevel loaded by MainGame."""
import unreal,json,pathlib
assert not unreal.EditorLevelLibrary.get_pie_worlds(False)
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world.get_name()=='Lvl_MainGame',world.get_name()
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
targets=[a for a in actors if isinstance(a,unreal.WenSiMirrorExhibit) and '/Game/WenSi/Maps/Lvl_WenSi_MirrorMuseum1.' in a.get_path_name()]
assert len(targets)>=12
root='/Game/WenSi/Museum/Refresh/Meshes/'
models={0:'SM_Rock',3:'SM_Balloon_UE',6:'SM_PorcelainHorse',12:'SM_Cat'}
report=[]
for a in targets:
 k=a.get_editor_property('kind')
 if k not in models and k not in (1,7,9):continue
 # The miniature has an authored editor display scale; gameplay applies its
 # word scale during BeginPlay, independently of this saved presentation.
 if k==12:a.set_actor_scale3d(unreal.Vector(.28,.28,.28))
 transform=a.get_actor_transform()
 p=(a.get_actor_location().to_tuple(),a.get_actor_rotation().to_tuple(),a.get_actor_scale3d().to_tuple())
 a.modify()
 if k in models:
  a.set_editor_property('art_model',unreal.load_asset(root+models[k]))
  a.set_editor_property('art_rotation',unreal.Rotator(pitch=0,yaw=0 if k in (0,3) else 90,roll=0))
  a.set_editor_property('uniform_art_scale',k!=0)
 if k==6:
  a.set_editor_property('display_noun','雕塑');a.set_actor_label('雕塑（白瓷马）')
 if k==12:a.set_actor_label('摆件（猫雕塑）')
 a.refresh_art_model()
 a.set_actor_transform(transform,False,True)
 after=(a.get_actor_location().to_tuple(),a.get_actor_rotation().to_tuple(),a.get_actor_scale3d().to_tuple())
 assert after==p,('Exhibit transform changed',a.get_name(),p,after)
 report.append({'actor':a.get_path_name(),'kind':k,'model':str(a.get_editor_property('art_model')),'noun':a.get_editor_property('display_noun')})
packages=unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()+unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()
assert unreal.EditorLoadingAndSavingUtils.save_packages(packages,False)
pathlib.Path(unreal.Paths.project_saved_dir(),'Validation/MuseumAssetRefresh/applied.json').write_text(json.dumps({'world':world.get_path_name(),'updated':report,'saved':[p.get_name() for p in packages]},ensure_ascii=False,indent=2),encoding='utf8')
print('MUSEUM_ART_APPLIED',len(report))
