"""Dress only the two user-made *1 maps with the imported Museum collection.

Original actor transforms, colliders and gameplay actors stay intact. Obsolete
blockout decoration is hidden at component level, retaining its collision.
New architectural dressing is non-colliding and organized in MysteryMuseum.
"""
import unreal,math,json,pathlib

MAPS=['Lvl_WenSi_MirrorMuseum1','Lvl_WenSi_ScreenBridge1']
TAG='MysteryMuseumArt'

def dress(name):
 assert name in MAPS
 assert not unreal.EditorLevelLibrary.get_pie_worlds(False)
 assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()+unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()
 world=unreal.EditorLoadingAndSavingUtils.load_map('/Game/WenSi/Maps/'+name);assert world
 subsystem=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
 original=[a for a in subsystem.get_all_level_actors() if TAG not in [str(t) for t in a.tags]]
 def collision_snapshot(actors):
  def transform(a):
   p=a.get_actor_location();s=a.get_actor_scale3d();r=a.get_actor_rotation()
   return [p.x,p.y,p.z,r.pitch,r.yaw,r.roll,s.x,s.y,s.z]
  return {a.get_name():{'class':a.get_class().get_name(),'transform':transform(a),'collision':[(c.get_name(),str(c.get_collision_enabled()),str(c.get_collision_profile_name())) for c in a.get_components_by_class(unreal.PrimitiveComponent)]} for a in actors}
 before=collision_snapshot(original)
 for a in subsystem.get_all_level_actors():
  if TAG in [str(t) for t in a.tags]:subsystem.destroy_actor(a)
 mirror='Mirror' in name
 wall=unreal.load_asset('/Game/Museum/Materials/MI_Stucco01');wood=unreal.load_asset('/Game/Museum/Materials/MI_WallTrims01');floor=unreal.load_asset('/Game/Museum/Materials/MI_StageFloor01');ceiling=unreal.load_asset('/Game/Museum/Materials/MI_ModularCeiling')
 count={'meshes':0,'lights':0,'hidden_blockout':0}
 for a in original:
  label=a.get_actor_label();parts=a.get_components_by_class(unreal.StaticMeshComponent)
  old_art=(mirror and label.startswith(('FloorTile_','Column_','Frame_','Art_','AbstractArt_','BrassTrim_','Cornice_','BenchSeat_','BenchLeg_','DecorPlinth_','DecorSculpture_','CeilingLamp_','Wainscot_'))) or (not mirror and ('WenSiMuseumDecor' in [str(t) for t in a.tags] or label.startswith(('Circuit_Tile_','Circuit_Frame_','Circuit_Art_','Circuit_Accent_','Circuit_Trim_','Circuit_Cornice_','Circuit_Wainscot_'))))
  if old_art and parts:
   for c in parts:c.set_visibility(False,True)
   count['hidden_blockout']+=1
  if a.get_class().get_name() not in ['Actor','StaticMeshActor']:continue
  for c in parts:
   if label.startswith(('Wall_','BackWall','EndWall','LeftWall','RightWall','Partition','Passage','CorridorWall','Circuit_Wall','Circuit_EntranceShoulder','PortalHeader')):c.set_material(0,wall)
   elif label.startswith(('Floor_','NearFloor','FarFloor','CorridorFloor','Circuit_Floor')):c.set_material(0,floor)
   elif label.startswith(('Ceiling_','LowCeiling','LowApproachCeiling','LowExitCeiling','LiftPocketCeiling','Circuit_Ceiling')) or label=='Ceiling':c.set_material(0,ceiling)
  # Replace flat ambient wash with explicit gallery pools of light below.
  for c in a.get_components_by_class(unreal.LightComponent):c.set_intensity(0)
  for c in a.get_components_by_class(unreal.PostProcessComponent):c.set_editor_property('enabled',False)
  if isinstance(a,unreal.PostProcessVolume):a.set_editor_property('enabled',False)

 def mesh(asset,center,scale=(1,1,1),rotation=(0,0,0),label=None):
  m=unreal.load_asset('/Game/Museum/Meshes/'+asset);assert m,asset
  rot=unreal.Rotator(pitch=rotation[0],yaw=rotation[1],roll=rotation[2]);s=unreal.Vector(*scale);b=m.get_bounds()
  # Position by bounds center, accounting for the pack's off-origin pivots.
  offset=unreal.MathLibrary.greater_greater_vector_rotator(unreal.Vector(b.origin.x*s.x,b.origin.y*s.y,b.origin.z*s.z),rot)
  a=subsystem.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(*center)-offset,rot)
  a.set_actor_label('MM_'+(label or asset)+'_'+str(count['meshes']));a.set_folder_path('MysteryMuseum/Architecture' if any(x in asset for x in ['Floor','Ceiling','Column','Trim','window']) else 'MysteryMuseum/Exhibitions');a.tags=[TAG]
  c=a.static_mesh_component;c.set_static_mesh(m);c.set_collision_profile_name('NoCollision');a.set_actor_scale3d(s)
  count['meshes']+=1;return a

 def light(center,target,intensity=1600,color=(1,.79,.53),angle=46,radius=850):
  a=subsystem.spawn_actor_from_class(unreal.SpotLight,unreal.Vector(*center));a.set_actor_label('MM_ExhibitLight_'+str(count['lights']));a.set_folder_path('MysteryMuseum/Lighting');a.tags=[TAG]
  a.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(unreal.Vector(*center),unreal.Vector(*target)),False)
  c=a.get_component_by_class(unreal.SpotLightComponent);c.set_mobility(unreal.ComponentMobility.MOVABLE);c.set_intensity_units(unreal.LightUnits.LUMENS);c.set_intensity(intensity*.6);c.set_light_color(unreal.LinearColor(*color));c.set_attenuation_radius(radius);c.set_outer_cone_angle(angle);c.set_inner_cone_angle(angle*.65)
  c.set_cast_shadows(True);count['lights']+=1

 def tiles(x0,x1,y0,y1,z=1):
  nx=math.ceil((x1-x0)/300);ny=math.ceil((y1-y0)/300);sx=(x1-x0)/nx;sy=(y1-y0)/ny
  for ix in range(nx):
   for iy in range(ny):mesh('SM_FloorTile01',(x0+(ix+.5)*sx,y0+(iy+.5)*sy,z-5),(sx/400,sy/400,1))

 def gallery(x0,x1,width,height):
  tiles(x0,x1,-width/2,width/2)
  span=x1-x0;n=math.ceil(span/300);step=span/n
  # Pack-native wall moulding below the stucco and articulated coffer ceiling.
  for y in [-width/2+10,width/2-10]:
   for i in range(n):
    mesh('SM_WallTrim03',(x0+(i+.5)*step,y,60),(1,step/400,1),(0,90 if y<0 else -90,0))
   column_n=max(3,round(span/340));positions=[x0+75+i*(span-150)/(column_n-1) for i in range(column_n)]
   for x in positions:mesh('SM_Column01',(x,y+(25 if y<0 else -25),(height-10)/2),(.65,.65,(height-10)/465.36))
   for i in range(column_n-1):
    x=(positions[i]+positions[i+1])/2
    if i%3==2 and height>350:
     mesh('SM_window_02',(x,y+(18 if y<0 else -18),height*.64),(.95,1,1),(0,0 if y<0 else 180,0))
     light((x,y+(85 if y<0 else -85),height-45),(x,0,100),650,(.40,.57,.76),60)
    else:
     picture=mesh('SM_Picture01' if i%2==0 else 'SM_Picture02',(x,y+(24 if y<0 else -24),height*.60),(1.4,1.4,-1.4),(0,180 if y<0 else 0,90))
     picture.static_mesh_component.set_material(1,unreal.load_asset('/Game/Museum/Materials/MI_Painting'+str([5,14,23,17][i%4]).zfill(2)))
     light((x,y+(110 if y<0 else -110),height-45),(x,y,height*.58),950)
  nx=math.ceil(span/300);ny=math.ceil(width/300)
  for ix in range(nx):
   for iy in range(ny):mesh('SM_ModularCeiling01',(x0+(ix+.5)*span/nx,-width/2+(iy+.5)*width/ny,height-10),(span/nx/300,width/ny/300,1))
  for x in [x0+span*.25,x0+span*.75]:
   light((x,0,height-35),(x,0,0),2400,(.93,.90,.84),58,1000)
  fill=subsystem.spawn_actor_from_class(unreal.PointLight,unreal.Vector((x0+x1)/2,0,height-65));fill.set_actor_label('MM_SoftGalleryFill');fill.set_folder_path('MysteryMuseum/Lighting');fill.tags=[TAG]
  component=fill.get_component_by_class(unreal.PointLightComponent);component.set_mobility(unreal.ComponentMobility.MOVABLE);component.set_intensity_units(unreal.LightUnits.LUMENS);component.set_intensity(300 if height>350 else 160);component.set_attenuation_radius(1500);component.set_light_color(unreal.LinearColor(.93,.90,.84));component.set_cast_shadows(False);count['lights']+=1

 if mirror:
  gallery(-900,440,1300,420);gallery(1010,2450,1300,420);tiles(455,995,-118,118)
  for x,y in [(-710,-550),(350,530),(2050,500)]:
   mesh('SM_PillarStand01',(x,y,50),(.40,.40,.83))
   mesh('SM_Philosopher' if x<0 else 'SM_Hebe',(x,y,168),(.85,.85,1.05),(0,-90 if y>0 else 90,0))
   light((x-70,y-(110 if y>0 else -110),365),(x,y,150),1100)
  for x,y in [(-570,490),(1670,510)]:mesh('SM_Bench',(x,y,45),(.75,.65,.73),(0,90,0))
 else:
  gallery(-935,235,1200,490);gallery(665,1495,780,295);tiles(270,625,-118,118)
  for x,y in [(-740,-470),(-420,-470),(-80,-470)]:
   mesh('SM_PillarStand02',(x,y,48),(.32,.32,.80));mesh('SM_Philosopher' if x==-420 else 'SM_Hebe',(x,y,160),(.70,.70,.92),(0,90,0));light((x-30,y+110,380),(x,y,145),1100)
  mesh('SM_Bench',(-650,465,45),(.75,.65,.73),(0,90,0))
  # A distant quiet display at the back of the second room, clear of circuitry.
  mesh('SM_Philosopher',(1420,300,120),(.60,.60,.70),(0,180,0))
  light((1400,200,265),(1420,300,120),550)
 # Keep gate and rule-transfer route readable instead of making gameplay dark.
 for x,z in [(250,225),(600,225),(970,255)]:light((x-80,0,z),(x+50,0,80),650,(.86,.90,1),55,500)
 fog=subsystem.spawn_actor_from_class(unreal.ExponentialHeightFog,unreal.Vector(0,0,-80));fog.set_actor_label('MM_LowAtmosphere');fog.set_folder_path('MysteryMuseum/Lighting');fog.tags=[TAG]
 fc=fog.get_component_by_class(unreal.ExponentialHeightFogComponent);fc.set_editor_property('fog_density',.008);fc.set_editor_property('fog_height_falloff',.7);fc.set_editor_property('fog_max_opacity',.18);fc.set_editor_property('start_distance',350);fc.set_editor_property('fog_inscattering_luminance',unreal.LinearColor(.12,.17,.25));fc.set_editor_property('enable_volumetric_fog',True)
 post=subsystem.spawn_actor_from_class(unreal.PostProcessVolume,unreal.Vector());post.set_actor_label('MM_Exposure');post.set_folder_path('MysteryMuseum/Lighting');post.tags=[TAG];post.set_editor_property('unbound',True)
 settings=post.get_editor_property('settings')
 for prop,val in [('override_auto_exposure_method',True),('auto_exposure_method',unreal.AutoExposureMethod.AEM_MANUAL),('override_auto_exposure_bias',True),('auto_exposure_bias',-1.3),('override_auto_exposure_apply_physical_camera_exposure',True),('auto_exposure_apply_physical_camera_exposure',False),('override_vignette_intensity',True),('vignette_intensity',.20)]:settings.set_editor_property(prop,val)
 post.set_editor_property('settings',settings)
 after=collision_snapshot(original);assert before==after,'Original transforms/collision changed'
 out=pathlib.Path(unreal.Paths.project_saved_dir())/'MuseumArt'
 (out/(name+'_dressing.json')).write_text(json.dumps({'map':name,'counts':count,'original_collision_and_transforms_preserved':True,'original_actor_count':len(original)},indent=2),encoding='utf8')
 assert unreal.EditorLoadingAndSavingUtils.save_map(world,'/Game/WenSi/Maps/'+name)
 print('MUSEUM_DRESSED',name,count)

for map_name in MAPS:dress(map_name)
