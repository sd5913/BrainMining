"""Bind imported stone/statue art to native gameplay actors in the *1 copies.

The actor identity, original pose, words and all logical proxy components remain
in place. Native ArtVisual survives Configure/BeginPlay/reset and follows moves.
"""
import unreal,pathlib,json

def install(name):
 assert name in ['Lvl_WenSi_MirrorMuseum1','Lvl_WenSi_ScreenBridge1']
 assert not unreal.EditorLevelLibrary.get_pie_worlds(False)
 assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()+unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()
 unreal.EditorLoadingAndSavingUtils.load_map('/Game/WenSi/Maps/'+name)
 records=[]
 for actor in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
  mirror=isinstance(actor,unreal.WenSiMirrorExhibit)
  if not mirror and not isinstance(actor,unreal.WenSiObject):continue
  kind=actor.get_editor_property('kind' if mirror else 'object_kind')
  stone=kind in ([0,6] if mirror else [0,5]);statue=kind==2;bust=mirror and kind==12
  if not (stone or statue or bust):continue
  path='/Game/Fab/Stone/stone1' if stone else '/Game/Museum/Meshes/SM_Hebe' if statue else '/Game/Museum/Meshes/SM_Philosopher'
  model=unreal.load_asset(path);assert isinstance(model,unreal.StaticMesh),path
  before=actor.get_actor_transform();word=actor.get_editor_property('word' if mirror else 'rule')
  actor.set_editor_property('art_model',model);actor.set_editor_property('art_rotation',unreal.Rotator(pitch=0,yaw=90,roll=0));actor.set_editor_property('uniform_art_scale',not stone)
  actor.refresh_art_model()
  assert actor.get_actor_location()==before.translation and actor.get_editor_property('word' if mirror else 'rule')==word
  art=actor.get_editor_property('art_visual');assert art.static_mesh==model and art.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION
  records.append({'actor':actor.get_name(),'kind':kind,'model':path,'original_location_and_word_preserved':True})
 assert unreal.EditorLoadingAndSavingUtils.save_current_level()
 out=pathlib.Path(unreal.Paths.project_saved_dir())/'MuseumArt';(out/(name+'_models.json')).write_text(json.dumps(records,ensure_ascii=False,indent=2),encoding='utf8')
 print('GAMEPLAY_ART_INSTALLED',name,len(records))

for map_name in ['Lvl_WenSi_ScreenBridge1','Lvl_WenSi_MirrorMuseum1']:install(map_name)
