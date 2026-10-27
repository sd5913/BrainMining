"""Unify the gold apparatus/covers and aim exhibit artwork without moving colliders."""
import unreal,pathlib,json,math
assert not unreal.EditorLevelLibrary.get_pie_worlds(False)
assert unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True,True)
lib=unreal.MaterialEditingLibrary;assets=unreal.EditorAssetLibrary
path='/Game/WenSi/Museum/Materials/MI_MuseumRoughGold'
gold=unreal.load_asset(path) if assets.does_asset_exist(path) else assets.duplicate_asset('/Game/WenSi/Museum/Materials/MI_MuseumMetal032',path)
assert gold
# Keep the existing fine metal normal map, with a warm gold color and broad highlights.
for key,value in [('Flat Color',True),('Solid Metalness',True),('Solid Roughness',True),('Mask Based Roughness',False),('Mask Based Color',False)]:
 lib.set_material_instance_static_switch_parameter_value(gold,key,value)
lib.set_material_instance_vector_parameter_value(gold,'Color',unreal.LinearColor(.72,.43,.105,1))
for key,value in [('Metalness',1),('Roughness',.48),('Tiling',2),('EmissiveLevel',0)]:
 lib.set_material_instance_scalar_parameter_value(gold,key,value)
lib.update_material_instance(gold);assert assets.save_loaded_asset(gold)
assert abs(lib.get_material_instance_scalar_parameter_value(gold,'Roughness')-.48)<.001
actors=[a for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors() if '/Game/WenSi/Maps/Lvl_WenSi_MirrorMuseum1.' in a.get_path_name()]
puzzle=next(a for a in actors if isinstance(a,unreal.WenSiMirrorPuzzle));origin=puzzle.get_actor_location()
r={'material':gold.get_path_name(),'stand':[],'orientation':[]}
for a in actors:
 if a.get_actor_label().startswith(('HopperLeg_','HopperSlope_','CatchTray_','HopperApertureRim','BallContactPedestal','BallContactSeat')):
  a.modify()
  for c in a.get_components_by_class(unreal.StaticMeshComponent):c.set_material(0,gold)
  r['stand'].append(a.get_actor_label())
 if not isinstance(a,unreal.WenSiMirrorExhibit):continue
 k=a.get_editor_property('kind')
 if k not in (1,7,6,12):continue
 t=a.get_actor_transform();a.modify()
 if k in (6,12):
  # FBX head axes: horse -X, cat +Y. Aim the artwork, keeping collision axes fixed.
  target=origin+unreal.Vector(1000,0,0) if k==6 else origin+unreal.Vector(1600,0,0)
  d=target-a.get_actor_location();head_yaw=math.degrees(math.atan2(d.y,d.x))
  yaw=head_yaw-(180 if k==6 else 90)-a.get_actor_rotation().yaw
  a.set_editor_property('art_rotation',unreal.Rotator(pitch=0,yaw=yaw,roll=0))
  r['orientation'].append({'kind':k,'yaw':yaw,'target':target.to_tuple()})
 a.refresh_art_model();a.set_actor_transform(t,False,True)
assert unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True,True)
out=pathlib.Path(unreal.Paths.project_saved_dir())/'Validation/MuseumTweaks'
(out/'applied.json').write_text(json.dumps(r,ensure_ascii=False,indent=2),encoding='utf8')
print('MUSEUM_TWEAKS_APPLIED',len(r['stand']))
