"""Fit HeavenPreview art to the existing gameplay hall; never recreate its level."""
import unreal,math,pathlib,datetime,shutil,json

DEST='/Game/CollectionPrototype/HeavenToilet/Maps/Lvl_HeavenToilet'
TAG='HeavenPreviewMigration'

def migrate():
 api=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
 w=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
 assert w.get_path_name().split('.')[0]==DEST and not unreal.EditorLevelLibrary.get_pie_worlds(False)
 root=pathlib.Path(unreal.Paths.project_dir());out=root/'Saved/HeavenPreview'
 backup=out/('BeforeMigration_'+datetime.datetime.now().strftime('%Y%m%d_%H%M%S'));backup.mkdir(parents=True)
 source=root/('Content/'+DEST.removeprefix('/Game/')+'.umap');shutil.copy2(source,backup/source.name)
 if unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()+unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages():assert unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True,True)
 actors=api.get_all_level_actors();field=next(a for a in actors if isinstance(a,unreal.MiningCollectionField));water=next(a for a in actors if isinstance(a,unreal.HeavenToiletWater))
 center=water.get_actor_location();radius=water.get_editor_property('radius');placements=field.get_editor_property('authored_placements');assert len(placements)==1 and placements[0].item_id==0
 # The three existing steps top out at 90cm; preserve their authored geometry.
 steps=[a for a in actors if a.get_actor_label().startswith('Heaven dais step')];assert len(steps)==3
 top=max(a.get_actor_bounds(False)[0].z+a.get_actor_bounds(False)[1].z for a in steps)
 def named(label,cls,pos):
  a=next((a for a in actors if a.get_actor_label()==label),None)
  if not a:a=api.spawn_actor_from_class(cls,pos)
  a.set_actor_location(pos,False,True);a.set_actor_label(label);a.set_editor_property('tags',[TAG]);a.set_folder_path('Heaven/PreviewArt');return a
 def mesh(label,asset,pos,scale,mat):
  a=named(label,unreal.StaticMeshActor,pos);c=a.static_mesh_component;c.set_static_mesh(asset);c.set_material(0,mat);c.set_collision_profile_name('NoCollision');c.set_cast_shadow(False);a.set_actor_scale3d(scale);return a
 sky=unreal.load_asset('/Game/HeavenPreview/Materials/M_HeavenHallSky') or unreal.load_asset('/Game/HeavenPreview/Materials/M_HeavenSky');porcelain=unreal.load_asset('/Game/Fab/Toilet/Toilet');body=unreal.load_asset('/Game/HeavenPreview/Meshes/SM_HeavenToiletBody')
 dome=next(a for a in actors if a.get_actor_label().startswith('Heaven hemisphere'))
 dome.static_mesh_component.set_material(0,sky)
 # Remove the visible architectural grid while retaining its hollow collider.
 hidden=[]
 for a in actors:
  if any(a.get_actor_label().startswith(prefix) for prefix in ['Heaven dome latitude','Heaven curved meridian','Heaven lower cove']):
   a.set_actor_hidden_in_game(True)
   for c in a.get_components_by_class(unreal.StaticMeshComponent):c.set_visibility(False);c.set_cast_shadow(False)
   hidden.append(a.get_name())
 # Shift the cloud shader's world-space anchor, leaving the preview source unchanged.
 folder='/Game/HeavenPreview/Materials';assets=unreal.AssetToolsHelpers.get_asset_tools();lib=unreal.MaterialEditingLibrary
 def cloud_material(source,name):
  path=folder+'/'+name;m=unreal.load_asset(path)
  if not m:
   m=assets.duplicate_asset(name,folder,unreal.load_asset(folder+'/'+source));assert m
   for node in unreal.ObjectIterator(unreal.MaterialExpressionCustom):
    if node.get_outer()==m:
     code=node.get_editor_property('code').replace('P.xy',f'(P.xy-float2({center.x},{center.y}))').replace('smoothstep(350,625,r)','smoothstep(1000,1410,r)');node.set_editor_property('code',code)
   lib.recompile_material(m);unreal.EditorAssetLibrary.save_loaded_asset(m)
  return m
 cloud=cloud_material('M_CloudDeck','M_HeavenHallCloudDeck');puff=cloud_material('M_CloudPuff','M_HeavenHallCloudPuff')
 plane=unreal.load_asset('/Engine/BasicShapes/Plane');sphere=unreal.load_asset('/Engine/BasicShapes/Sphere')
 mesh('天堂云海 · 水面下方',plane,center+unreal.Vector(0,0,46),unreal.Vector(radius*2/100,radius*2/100,1),cloud)
 for i in range(32):
  angle=math.tau*i/32
  # Keep the excavated doorway and pipe visible/usable at the western portal.
  if abs(angle-math.pi)<.22:continue
  r=radius-60+35*math.sin(i*3.7);s=2.2+.45*math.sin(i*2.3)
  mesh(f'天堂边缘云_{i:02d}',sphere,center+unreal.Vector(math.cos(angle)*r,math.sin(angle)*r,30),unreal.Vector(s*1.5,s*1.2,s*.5),puff)
 scale=.22659039959308247;bounds=body.get_bounding_box();pos=center+unreal.Vector(0,0,top-bounds.min.z*scale)
 toilet=named('神圣马桶 · 靠近开盖喷水',unreal.HeavenToilet,pos);toilet.set_actor_rotation(unreal.Rotator(pitch=0,yaw=90,roll=0),False);toilet.set_actor_scale3d(unreal.Vector(scale,scale,scale))
 for c in toilet.get_components_by_class(unreal.StaticMeshComponent):c.set_material(0,porcelain)
 toilet.get_editor_property('water_outlet').set_relative_location(unreal.Vector(0,60,260),False,True)
 # New binding is applied after the editor DLL has rebuilt; the placed model is already reviewable.
 bound=False
 try:
  entry=placements[0];entry.set_editor_property('presentation_actor',toilet);placements[0]=entry;field.set_editor_property('authored_placements',placements);bound=True
 except Exception:pass
 for a in actors:
  if isinstance(a,unreal.PointLight) and a.get_actor_label().startswith('Heaven'):
   c=a.get_component_by_class(unreal.PointLightComponent);c.set_light_color(unreal.LinearColor(1,.96,.87,1))
 pp=next(a for a in actors if isinstance(a,unreal.PostProcessVolume));settings=pp.get_editor_property('settings')
 for k,v in [('override_auto_exposure_min_brightness',True),('auto_exposure_min_brightness',0.),('override_auto_exposure_max_brightness',True),('auto_exposure_max_brightness',0.),('override_auto_exposure_bias',True),('auto_exposure_bias',-1.5)]:settings.set_editor_property(k,v)
 pp.set_editor_property('settings',settings)
 unreal.EditorLevelLibrary.set_level_viewport_camera_info(center+unreal.Vector(-530,-170,200),unreal.Rotator(pitch=-6,yaw=18,roll=0))
 assert unreal.EditorLoadingAndSavingUtils.save_current_level()
 report={'map':DEST,'center':str(center),'toilet_position':str(pos),'platform_top':top,'radius':radius,'bound_to_original_collection':bound,'hidden_grid_count':len(hidden),'backup':str(backup)}
 (out/'migration.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8');print('HEAVEN_ART_PLACED',report)

migrate()
