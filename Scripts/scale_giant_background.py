"""V03: background furniture 5x V02; preserve tabletop gameplay and entrance."""
import unreal
A=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);L=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert L.load_level('/Game/Passages/Echo/Maps/Lvl_EchoMazeToCards_P1')
TAG=unreal.Name('GiantBackgroundV03')
actors=A.get_all_level_actors()
if any(TAG in a.tags for a in actors):raise RuntimeError('V03 already applied. Adjust existing instances.')
# Furniture is enlarged much more than its distance from the camera, preserving angular scale.
for a in actors:
 label=a.get_actor_label();p=a.get_actor_location();s=a.get_actor_scale3d()
 if not label.startswith('Giant /'):continue
 if any(w in label for w in ['Bookcase','Book']):
  side=-1 if p.y<3600 else 1
  old=unreal.Vector(13468,3600+side*1758,0);new=unreal.Vector(17800,3600+side*4950,0)
  a.set_actor_location(new+(p-old)*5,False,False);a.set_actor_scale3d(s*5)
 elif any(w in label for w in ['Sideboard','Drawer','Candle']):
  old=unreal.Vector(11440,1302,0);new=unreal.Vector(10540,-4500,0)
  a.set_actor_location(new+(p-old)*5,False,False);a.set_actor_scale3d(s*5)
 elif any(w in label for w in ['Decorative pot','Plant leaves']):
  old=unreal.Vector(12100,5778,0);new=unreal.Vector(15500,10000,0)
  a.set_actor_location(new+(p-old)*5,False,False);a.set_actor_scale3d(s*5)
 elif any(w in label for w in ['Chair','chair']):
  if p.x>10540:
   # Opponent sits in this chair: same 10x overall scale as the enlarged cat.
   old=unreal.Vector(11842,3600,0);new=unreal.Vector(12500,3600,0)
   a.set_actor_location(new+(p-old)*(10/6),False,False);a.set_actor_scale3d(s*(10/6))
  else:
   # Move the old player chair aside; its massive back must not hide the tabletop beacon.
   old=unreal.Vector(9040,3600,0);new=unreal.Vector(8200,-5000,0)
   a.set_actor_location(new+(p-old)*5,False,False);a.set_actor_scale3d(s*5)
 elif any(w in label for w in ['Floor','floor','Ceiling','Side plaster','Wall dado','Wall panel','Dado upper','Window','window','carpet','Rug','Back plaster','Pendant']):
  # Room envelope expands around the fixed breach in the passage.
  flat=any(w in label for w in ['Floor','floor','carpet','Rug'])
  a.set_actor_location(unreal.Vector(6400+(p.x-6400)*2.6,3600+(p.y-3600)*4.4,p.z*(1 if flat else 5)),False,False)
  a.set_actor_scale3d(unreal.Vector(s.x*2.6,s.y*4.4,s.z*(1 if flat else 5)))
 elif label in ['Giant / Room title','Giant / Room subtitle','Giant / Ember sign backing']:
  a.set_actor_location(unreal.Vector(25000,3600,p.z*3.8),False,False);a.set_actor_scale3d(s*3.8)
 if isinstance(a,unreal.CardRoomStage):
  a.set_editor_property('opponent_scale',5.0);a.set_editor_property('opponent_base_local',unreal.Vector(980,0,115));a.set_editor_property('seat_pitch',10.0);a.set_editor_property('opponent_exit_local',unreal.Vector(7000,0,900))
 if label=='Giant / Warm pendant light':
  a.set_actor_location(unreal.Vector(6400+(p.x-6400)*2.6,3600+(p.y-3600)*4.4,p.z*5),False,False);a.light_component.set_intensity(2750000);a.light_component.set_editor_property('attenuation_radius',25500);a.light_component.set_editor_property('cast_shadows',False)
 if isinstance(a,unreal.MiningGiantRoomEntrance):
  box=a.get_component_by_class(unreal.BoxComponent);box.set_relative_location(unreal.Vector(9800,0,6810),False,False);box.set_box_extent(unreal.Vector(9800,12500,7000),False)
# Extend the entry wall around the existing diggable patch without covering it.
mat=unreal.load_asset('/Game/Rooms/CardDuel/Materials/M_Plaster')
def cube(label,p,size):
 a=A.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(*p));a.tags=[TAG];a.set_actor_label('Hall / '+label)
 a.static_mesh_component.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Cube'));a.static_mesh_component.set_material(0,mat);a.set_actor_scale3d(unreal.Vector(*(v/100 for v in size)))
for side in [-1,1]:cube('Entry surround',(6400,3600+side*6350,7000),(150,12300,14000))
cube('Entry upper wall',(6400,3600,7190),(150,400,13620))
# Warm architectural fill lights illuminate the enlarged furniture; stage light stays focused.
for x,y,z in [(16000,-1800,6500),(16000,9000,6500),(11000,-6200,4400),(22000,3600,10000)]:
 a=A.spawn_actor_from_class(unreal.PointLight,unreal.Vector(x,y,z));a.tags=[TAG];a.set_actor_label('Hall / Amber architectural fill')
 c=a.light_component;c.set_editor_property('mobility',unreal.ComponentMobility.MOVABLE);c.set_intensity(1400000);c.set_editor_property('attenuation_radius',16000);c.set_light_color(unreal.LinearColor(1,.62,.3,1));c.set_editor_property('cast_shadows',False)
assert L.save_current_level()
unreal.log('GIANT_BACKGROUND_V03_READY')
