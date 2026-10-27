"""Attach replacement visuals to the same tagged animated actors/colliders."""
import unreal,json,pathlib
assert not unreal.EditorLevelLibrary.get_pie_worlds(False)
assert unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True,True)
sub=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
actors=[a for a in sub.get_all_level_actors() if '/Game/WenSi/Maps/Lvl_WenSi_MirrorMuseum1.' in a.get_path_name()]
puzzle=next(a for a in actors if isinstance(a,unreal.WenSiMirrorPuzzle));origin=puzzle.get_actor_location();level=puzzle.get_outer()
assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).set_current_level_by_name('Lvl_WenSi_MirrorMuseum1')
tag='MuseumMedievalBalance';r=[]
for a in actors:
 if tag in [str(t) for t in a.tags]:sub.destroy_actor(a)
def hide(c):
 c.set_editor_property('render_in_main_pass',False);c.set_editor_property('render_in_depth_pass',False);c.set_cast_shadow(False);c.set_editor_property('visible_in_ray_tracing',False)
for a in actors:
 label=a.get_actor_label();tags=[str(t) for t in a.tags]
 if label.startswith(('BalanceColumn_','BalancePivot_','BalanceSuspension_','BalanceSpreader_','MM_Balance_Imported')) or 'WenSiBalanceCounterweight' in tags:
  a.modify()
  for c in a.get_components_by_class(unreal.StaticMeshComponent):hide(c)
def art(part,location,parent=None):
 mesh=unreal.load_asset('/Game/WenSi/Museum/Refresh/Meshes/SM_'+part);assert mesh
 a=sub.spawn_actor_from_class(unreal.StaticMeshActor,location,unreal.Rotator());assert a.get_outer()==level
 a.set_actor_label('MM_'+part);a.set_folder_path('MysteryMuseum/Refresh');a.tags=[tag]
 c=a.static_mesh_component;c.set_mobility(unreal.ComponentMobility.MOVABLE);c.set_static_mesh(mesh);c.set_collision_profile_name('NoCollision')
 if parent:assert a.attach_to_actor(parent,'',unreal.AttachmentRule.KEEP_WORLD,unreal.AttachmentRule.KEEP_WORLD,unreal.AttachmentRule.KEEP_WORLD,False)
 r.append({'part':part,'path':a.get_path_name(),'parent':parent.get_path_name() if parent else None});return a
art('MedievalScaleStand',origin+unreal.Vector(1590,-430,0))
for role,part in [('WenSiBalanceBeam','MedievalScaleBeam'),('WenSiBalanceLeftTray','MedievalScaleLeftPan'),('WenSiBalanceRightTray','MedievalScaleRightPan'),('WenSiBalanceCounterweight','MuseumMiningWeight')]:
 parent=next(a for a in actors if role in [str(t) for t in a.tags])
 p=parent.get_actor_location()
 if role=='WenSiBalanceCounterweight':p+=unreal.Vector(0,0,-17)
 art(part,p,parent)
for a in actors:
 if isinstance(a,unreal.WenSiMirrorExhibit) and a.get_editor_property('kind') in (1,7):
  t=a.get_actor_transform();a.modify();a.refresh_art_model();a.set_actor_transform(t,False,True)
assert unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True,True)
assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).set_current_level_by_name('Lvl_MainGame')
(pathlib.Path(unreal.Paths.project_saved_dir())/'Validation/MuseumTweaks/scale_applied.json').write_text(json.dumps(r,indent=2))
print('MUSEUM_MEDIEVAL_BALANCE_APPLIED')
