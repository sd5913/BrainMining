"""Dress the streamed museum source without changing puzzle collision envelopes."""
import unreal,json,pathlib
assert not unreal.EditorLevelLibrary.get_pie_worlds(False)
sub=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
actors=[a for a in sub.get_all_level_actors() if '/Game/WenSi/Maps/Lvl_WenSi_MirrorMuseum1.' in a.get_path_name()]
assert len(actors)>700
puzzle=next(a for a in actors if isinstance(a,unreal.WenSiMirrorPuzzle));origin=puzzle.get_actor_location()
level=puzzle.get_outer();world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
stream=unreal.GameplayStatics.get_streaming_level(world,'Lvl_WenSi_MirrorMuseum1');assert stream
assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).set_current_level_by_name('Lvl_WenSi_MirrorMuseum1')
folder='/Game/WenSi/Museum/Refresh/'
tag='MuseumRefreshFollowup';report={'plinths':[],'walls':[],'apparatus':[],'balance':[]}
for a in actors:
 if tag in [str(t) for t in a.tags]:sub.destroy_actor(a)
wall=unreal.load_asset('/Game/Museum/Materials/MI_Stucco01')
enamel=unreal.load_asset(folder+'Materials/MI_ApparatusEnamel');iron=unreal.load_asset(folder+'Materials/MI_BalanceIron');brass=unreal.load_asset(folder+'Materials/MI_BalanceBrass')
def hide(c):
 c.set_editor_property('render_in_main_pass',False);c.set_editor_property('render_in_depth_pass',False);c.set_cast_shadow(False);c.set_editor_property('visible_in_ray_tracing',False)
def art(name,mesh,location,scale=(1,1,1),rotation=None,parent=None):
 mesh=unreal.load_asset(mesh) if isinstance(mesh,str) else mesh
 a=sub.spawn_actor_from_class(unreal.StaticMeshActor,location,rotation or unreal.Rotator());assert a.get_outer()==level
 a.set_actor_label('MM_'+name);a.set_folder_path('MysteryMuseum/Refresh');a.tags=[tag]
 c=a.static_mesh_component;c.set_mobility(unreal.ComponentMobility.MOVABLE);c.set_static_mesh(mesh);c.set_collision_profile_name('NoCollision');a.set_actor_scale3d(unreal.Vector(*scale))
 if parent:assert a.attach_to_actor(parent,'',unreal.AttachmentRule.KEEP_WORLD,unreal.AttachmentRule.KEEP_WORLD,unreal.AttachmentRule.KEEP_WORLD,False)
 return a
def fit(name,mesh,center,size,rotation=None,parent=None):
 mesh=unreal.load_asset(mesh) if isinstance(mesh,str) else mesh;b=mesh.get_bounds();rotation=rotation or unreal.Rotator()
 # Rotations used here are right angles; scale in local axes before rotating.
 dim=unreal.MathLibrary.greater_greater_vector_rotator(b.box_extent*2,rotation)
 ratios=unreal.Vector(size[0]/abs(dim.x),size[1]/abs(dim.y),size[2]/abs(dim.z))
 local=unreal.MathLibrary.less_less_vector_rotator(ratios,rotation);scale=unreal.Vector(abs(local.x),abs(local.y),abs(local.z))
 offset=unreal.MathLibrary.greater_greater_vector_rotator(b.origin*scale,rotation)
 return art(name,mesh,center-offset,scale.to_tuple(),rotation,parent)
plinths=('BalloonStand_','SmokeStand_','SmallExhibitPlinth_','MetalPlinth_','HopperPedestal_','RewardPlinth_')
for a in actors:
 label=a.get_actor_label();cs=a.get_components_by_class(unreal.StaticMeshComponent)
 if label.startswith('StatuePlinth_'):
  sub.destroy_actor(a);report['removed_empty_plinth']=label;continue
 if label.startswith(plinths):
  c=cs[0];b=c.static_mesh.get_bounds();scale=a.get_actor_scale3d();size=b.box_extent*2*scale
  center=a.get_actor_location()+b.origin*scale
  hide(c)
  fit(label+'MuseumStand','/Game/Museum/Meshes/SM_PillarStand02',center,size.to_tuple())
  report['plinths'].append(label)
 if label.startswith(('RewardFront_','RewardHeader_','RewardSide_')):
  for c in cs:c.set_material(0,wall)
  report['walls'].append(label)
 if label.startswith(('HopperLeg_','HopperSlope_','CatchTray_','ContactInsulator_','BallContactPedestal','HopperApertureRim','PlugFace_')):
  for c in cs:c.set_material(0,enamel)
  report['apparatus'].append(label)
 if label.startswith('BalanceBase_'):
  for c in cs:c.set_material(0,unreal.load_asset('/Game/Museum/Materials/MI_WallTrims01'))
 if label.startswith(('BalanceColumn_','BalanceSuspension_','BalanceSpreader_')):
  for c in cs:c.set_material(0,iron)
 if label.startswith('BalanceButtonRing_'):
  for c in cs:c.set_material(0,brass)
 tags=[str(t) for t in a.tags]
 if 'WenSiBalanceBeam' in tags:
  for c in cs:hide(c)
  child=art('Balance_ImportedBeam',folder+'Meshes/SM_BalanceBeam',a.get_actor_location(),(90/42,2.5,1.3),parent=a)
  report['balance'].append({'actor':child.get_name(),'parent':a.get_name(),'role':'beam'})
 if any(t in tags for t in ('WenSiBalanceLeftTray','WenSiBalanceRightTray')):
  for c in cs:hide(c)
  child=fit('Balance_ImportedPan_'+a.get_name(),folder+'Meshes/SM_BalancePan',a.get_actor_location(),(112,112,10),parent=a)
  report['balance'].append({'actor':child.get_name(),'parent':a.get_name(),'role':'pan'})
 if label.startswith('BalancePivot_'):
  for c in cs:hide(c)
  child=fit('Balance_ImportedHinge',folder+'Meshes/SM_BalancePivot',a.get_actor_location(),(14,24,22))
  report['balance'].append({'actor':child.get_name(),'role':'hinge'})
for a in actors:
 if not isinstance(a,unreal.WenSiMirrorExhibit):continue
 k=a.get_editor_property('kind')
 if k in (5,8):
  a.modify();a.set_editor_property('art_model',unreal.load_asset(folder+'Meshes/'+('SM_FoldingScreen' if k==5 else 'SM_ConductiveBlock')))
  a.set_editor_property('art_rotation',unreal.Rotator(pitch=0,yaw=90 if k==5 else 0,roll=0));a.set_editor_property('uniform_art_scale',False);a.refresh_art_model()
shark=unreal.load_asset(folder+'Meshes/SM_BronzeShark');b=shark.get_bounds();s=170/(b.box_extent.y*2);rot=unreal.Rotator(pitch=0,yaw=90,roll=0)
center=origin+unreal.Vector(-230,360,b.box_extent.z*s+1)
off=unreal.MathLibrary.greater_greater_vector_rotator(b.origin*s,rot)
shark_actor=art('BronzeShark_PhaseOne',shark,center-off,(s,s,s),rot)
# Use the statue's exact mesh for collision, avoiding an invisible box above its base.
body=shark.get_editor_property('body_setup');body.set_editor_property('collision_trace_flag',unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
assert unreal.EditorAssetLibrary.save_loaded_asset(shark)
shark_actor.static_mesh_component.set_collision_profile_name('BlockAll')
report['shark']=shark_actor.get_path_name()
assert unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True,True)
assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).set_current_level_by_name('Lvl_MainGame')
(pathlib.Path(unreal.Paths.project_saved_dir())/'Validation/MuseumAssetRefresh/Followup/applied.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
print('FOLLOWUP_APPLIED',report)
