"""Import separated pipe assemblies and fit them to the existing water-pipe route."""
import unreal,pathlib,json,math

def install():
 api=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world();assert '/HeavenToilet/Maps/Lvl_HeavenToilet.' in world.get_path_name()
 actors=api.get_all_level_actors();root=pathlib.Path(unreal.Paths.project_dir()).resolve();folder='/Game/HeavenPreview/Meshes/Pipes'
 merged=next(a for a in actors if 'modular_pipes_junct' in a.get_actor_label());original=merged.static_mesh_component.static_mesh
 materials=[original.get_material(i) for i in range(len(original.get_editor_property('static_materials')))]
 models={};tasks=[]
 unreal.SystemLibrary.execute_console_command(None,'Interchange.FeatureFlags.Import.FBX 0')
 for part in [2,9,13]:
  name=f'SM_HeavenPipePart_{part:03d}';path=folder+'/'+name
  if not unreal.EditorAssetLibrary.does_asset_exist(path):
   task=unreal.AssetImportTask();task.filename=str(root/'Saved/HeavenPreview/Pipes'/(name+'.fbx'));task.destination_path=folder;task.destination_name=name;task.automated=True;task.save=True
   opt=unreal.FbxImportUI();opt.set_editor_property('automated_import_should_detect_type',False);opt.mesh_type_to_import=unreal.FBXImportType.FBXIT_STATIC_MESH;opt.import_materials=False;opt.import_textures=False;opt.import_animations=False;opt.static_mesh_import_data.combine_meshes=True;opt.static_mesh_import_data.auto_generate_collision=False;opt.static_mesh_import_data.normal_import_method=unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS;task.options=opt;tasks.append(task)
 if tasks:unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
 for part in [2,9,13]:
  mesh=unreal.load_asset(folder+f'/SM_HeavenPipePart_{part:03d}');assert mesh
  for i,mat in enumerate(materials):mesh.set_material(i,mat)
  unreal.EditorAssetLibrary.save_loaded_asset(mesh);models[part]=mesh
 # Keep the supplied combined asset as a hidden source, rather than leaving its demo arrangement in the room.
 merged.set_actor_hidden_in_game(True);merged.static_mesh_component.set_visibility(False);merged.static_mesh_component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION);merged.set_folder_path('Heaven/PipeSource')
 for a in actors:
  if 'HeavenPipeReplacement' in [str(t) for t in a.tags]:api.destroy_actor(a)
 segments=sorted([a for a in actors if a.get_actor_label().startswith('Heaven continuous water pipe')],key=lambda a:a.get_actor_label())
 assert len(segments)==7
 records=[]
 elbows=sorted([a for a in actors if a.get_actor_label().startswith('Heaven pipe elbow')],key=lambda a:a.get_actor_label())
 curve=models[13];curvebox=curve.get_bounding_box();tube=(curvebox.max.y-curvebox.min.y)
 intersection=unreal.Vector(curvebox.min.x+tube*.5,0,curvebox.min.z+tube*.5)
 reachx=curvebox.max.x-intersection.x;reachz=curvebox.max.z-intersection.z
 trims=[[0.,0.] for _ in segments];curves=[]
 for i,old in enumerate(elbows):
  if i>=len(segments)-1:break
  before=segments[i];after=segments[i+1];corner=old.get_actor_location()
  incoming=(before.get_actor_location()-corner).normal();outgoing=(after.get_actor_location()-corner).normal()
  radial=min(before.get_actor_scale3d().x*100/tube,before.get_actor_scale3d().z*100*.4/reachz,after.get_actor_scale3d().z*100*.4/reachx)
  trims[i][1]=reachz*radial;trims[i+1][0]=reachx*radial
  curves.append((i,corner,unreal.MathLibrary.make_rot_from_xz(outgoing,incoming),radial))
 def make(label,mesh,point,rot,scale):
  a=api.spawn_actor_from_class(unreal.StaticMeshActor,point,rot);a.set_actor_label(label);a.set_folder_path('Heaven/PipesDetailed');a.tags=['HeavenPipeReplacement'];c=a.static_mesh_component;c.set_static_mesh(mesh);c.set_collision_profile_name('NoCollision');a.set_actor_scale3d(scale);return a
 for index,old in enumerate(segments):
  oldscale=old.get_actor_scale3d();original_length=oldscale.z*100;diameter=oldscale.x*100;direction=unreal.MathLibrary.greater_greater_vector_rotator(unreal.Vector(0,0,1),old.get_actor_rotation());center=old.get_actor_location()
  length=original_length-trims[index][0]-trims[index][1];center+=direction*((trims[index][0]-trims[index][1])*.5)
  part=9 if length>85 else 2;model=models[part];box=model.get_bounding_box();size=box.max-box.min
  # Longitudinal fit stays modest by tiling long spans; cross section uses uniform scale.
  radial=diameter/max(size.y,size.z);natural=size.x*radial;count=max(1,round(length/natural));span=length/count
  rot=unreal.MathLibrary.make_rot_from_x(direction)
  for j in range(count):
   scale=unreal.Vector(span/size.x,radial,radial);p=center+direction*(-length*.5+span*(j+.5));p-=unreal.MathLibrary.greater_greater_vector_rotator((box.min+box.max)*.5*scale,rot)
   make(f'写实管道 · 路段{index:02d}_{j:02d}',model,p,rot,scale)
  old.set_actor_hidden_in_game(True);old.static_mesh_component.set_visibility(False)
  records.append({'route_segment':old.get_actor_label(),'center':str(center),'length_cm':length,'parts':count,'original_centerline_preserved':True})
 # Detailed flanged curves replace the old spherical elbow covers at their original vertices.
 for i,corner,rot,radial in curves:
  location=corner-unreal.MathLibrary.greater_greater_vector_rotator(intersection*radial,rot)
  make(f'写实弯管 · 转角{i:02d}',curve,location,rot,unreal.Vector(radial,radial,radial))
  old=elbows[i];old.set_actor_hidden_in_game(True);old.static_mesh_component.set_visibility(False)
 for a in actors:
  if a.get_actor_label().startswith(('Heaven pipe coupling','Heaven floor pipe coupling','Heaven water droplet')):
   a.set_actor_hidden_in_game(True)
   for c in a.get_components_by_class(unreal.StaticMeshComponent):c.set_visibility(False)
 field=next(a for a in actors if isinstance(a,unreal.MiningCollectionField));toilet=next(a for a in actors if isinstance(a,unreal.HeavenToilet));placements=field.get_editor_property('authored_placements');entry=placements[0];entry.presentation_actor=toilet;placements[0]=entry;field.set_editor_property('authored_placements',placements)
 water=next(a for a in actors if isinstance(a,unreal.HeavenToiletWater));water.set_editor_property('outlet_spray_system',unreal.load_asset('/Game/HeavenPreview/VFX/NS_HeavenEntranceFlow'));water.rebuild_water()
 assert unreal.EditorLoadingAndSavingUtils.save_current_level()
 (root/'Saved/HeavenPreview/Pipes/installed.json').write_text(json.dumps(records,ensure_ascii=False,indent=2),encoding='utf8');print('DETAILED_PIPE_ROUTE_INSTALLED',len(records),len(elbows))

install()
