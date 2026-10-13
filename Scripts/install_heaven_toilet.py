"""Install the proximity prop only in the independent heaven preview."""
import unreal,json,pathlib
def install():
 api=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world();assert world.get_path_name().startswith('/Game/HeavenPreview/Maps/Lvl_HeavenPreview.')
 actors=api.get_all_level_actors();old=next((a for a in actors if a.get_actor_label()=='神圣马桶 · Fab原模型'),None)
 existing=next((a for a in actors if isinstance(a,unreal.HeavenToilet)),None)
 if not existing:
  assert old,'Original preview toilet required'
  transform=old.get_actor_transform();toilet=api.spawn_actor_from_class(unreal.HeavenToilet,transform.translation,transform.rotation.rotator());toilet.set_actor_transform(transform,False,True);toilet.set_actor_label('神圣马桶 · 靠近开盖喷水');toilet.set_folder_path('HeavenPreview/Set');api.destroy_actor(old)
 else:toilet=existing
 porcelain=unreal.load_asset('/Game/Fab/Toilet/Toilet')
 for component in toilet.get_components_by_class(unreal.StaticMeshComponent):component.set_material(0,porcelain)
 toilet.get_editor_property('water_outlet').set_relative_location(unreal.Vector(0,60,260),False,True)
 toilet.get_editor_property('opening_splash').set_asset(unreal.load_asset('/Game/HeavenPreview/VFX/NS_HeavenToiletOpeningSplash'))
 mesh=toilet.get_editor_property('body').static_mesh;mesh.get_editor_property('body_setup').set_editor_property('collision_trace_flag',unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE);unreal.EditorAssetLibrary.save_loaded_asset(mesh)
 for actor in actors:
  if isinstance(actor,unreal.NiagaraActor) and actor.get_actor_label()=='马桶喷水 · Niagara':api.destroy_actor(actor)
 assert toilet.get_component_by_class(unreal.NiagaraComponent)
 assert unreal.EditorLoadingAndSavingUtils.save_map(world,'/Game/HeavenPreview/Maps/Lvl_HeavenPreview')
 # Generated test water is no longer part of the preview or shipped content.
 for path in ['/Game/HeavenPreview/VFX/NS_HeavenFountain','/Game/HeavenPreview/Materials/M_WaterDroplet']:
  refs=unreal.EditorAssetLibrary.find_package_referencers_for_asset(path,True)
  if unreal.EditorAssetLibrary.does_asset_exist(path) and not refs:unreal.EditorAssetLibrary.delete_asset(path)
 print('HEAVEN_INTERACTIVE_INSTALLED',toilet.get_path_name(),str(toilet.get_actor_transform()))
install()
