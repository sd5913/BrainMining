"""Idempotent V02: dress the saved giant room; do not recreate the passage."""
import unreal, math
A=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
L=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
MAP='/Game/Passages/Echo/Maps/Lvl_EchoMazeToCards_P1'
assert L.load_level(MAP)
TAG=unreal.Name('GiantDeskV02')
for a in A.get_all_level_actors():
 if TAG in a.tags:A.destroy_actor(a)
base='/Game/Rooms/CardDuel/Materials/'
def mat(name,color,glow=False):
 path=base+name
 m=unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else unreal.AssetToolsHelpers.get_asset_tools().create_asset(name,base[:-1],unreal.Material,unreal.MaterialFactoryNew())
 ml=unreal.MaterialEditingLibrary;ml.delete_all_material_expressions(m)
 if glow:m.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT)
 c=ml.create_material_expression(m,unreal.MaterialExpressionConstant3Vector);c.set_editor_property('constant',unreal.LinearColor(*color,1))
 ml.connect_material_property(c,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR if glow else unreal.MaterialProperty.MP_BASE_COLOR)
 ml.recompile_material(m);unreal.EditorAssetLibrary.save_loaded_asset(m);return m
paper=mat('M_GiantDeskPaper',(.72,.57,.36));red=mat('M_GiantDeskCloth',(.24,.04,.025));ink=mat('M_GiantDeskInk',(.012,.022,.026));gold=mat('M_GiantDeskPencil',(.8,.34,.03));rose=mat('M_GiantDeskEraser',(.5,.14,.1));warm=mat('M_GiantDeskGlow',(40,15,3),True)
def shape(label,p,size,material,mesh='Cube',rot=unreal.Rotator(),collision=True):
 a=A.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(*p),rot);a.set_actor_label('Desk / '+label);a.tags=[TAG]
 a.static_mesh_component.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/'+mesh));a.static_mesh_component.set_material(0,material)
 a.set_actor_scale3d(unreal.Vector(*(v/100 for v in size)))
 if not collision:a.set_actor_enable_collision(False);a.static_mesh_component.set_cast_shadow(False)
 return a
def book(label,x,y,z,w,h,t,cover):
 shape(label+' pages',(x,y,z+t*.5),(w-14,h-14,t),paper)
 for dz in [0,t]:shape(label+' cover',(x,y,z+dz),(w,h,10),cover)
 shape(label+' spine',(x-w*.5,y,z+t*.5),(16,h,t+8),cover)
 for dz in range(12,int(t),12):shape(label+' page edge',(x,y-h*.5-1,z+dz),(w-20,2,1.5),ink,collision=False)
# Nearby objects make the player's size legible even after landing on the desk.
for i in range(4):book('Giant textbook',10340+i%2*35,3020,698+i*95,750-i*30,440,82,red if i%2 else gold)
book('Open notebook',10390,4100,698,720,430,28,red)
for i in range(11):shape('Notebook ruling',(10390,3930+i*29,730),(650,2,1),ink,collision=False)
shape('Notebook center fold',(10390,4100,731),(5,420,2),ink,collision=False)
for x,y,z in [(9800,4120,733),(10950,4190,733)]:
 shape('Oversized pencil',(x,y,z),(58,58,780),gold,'Cylinder',unreal.Rotator(pitch=90,yaw=0,roll=0))
 shape('Pencil graphite',(x+425,y,z),(58,58,85),ink,'Cone',unreal.Rotator(pitch=90,yaw=0,roll=0))
 shape('Pink eraser',(x-415,y,z),(62,62,75),rose,'Cylinder',unreal.Rotator(pitch=90,yaw=0,roll=0))
book('Back desk atlas',11200,3030,698,470,390,115,red)
for a in A.get_all_level_actors():
 label=a.get_actor_label()
 if isinstance(a,unreal.CardRoomStage):
  a.set_editor_property('opponent_scale',3.0);a.set_editor_property('seat_pitch',12.0)
 if label.startswith('Giant /') and isinstance(a,unreal.PointLight):
  c=a.light_component;c.set_light_color(unreal.LinearColor(1,.58,.24,1))
  if 'pendant' in label:c.set_intensity(110000)
  if 'fill' in label:c.set_intensity(42000);c.set_light_color(unreal.LinearColor(1,.72,.44,1))
 if label=='Giant / Table stage spotlight':
  a.light_component.set_light_color(unreal.LinearColor(1,.68,.3,1));a.light_component.set_intensity(240000)
 if isinstance(a,unreal.StaticMeshActor):
  if label in ['Giant / Night window','Giant / Pendant glow']:a.static_mesh_component.set_material(0,warm)
  if label=='Giant / Card table felt':a.static_mesh_component.set_material(0,unreal.load_asset(base+'M_OakLight'))
  if label=='Giant / Table stage light beam':a.set_actor_enable_collision(False)
# Additive beam stays luminous under the local -6 exposure instead of darkening the room.
m=unreal.load_asset(base+'M_GiantStageBeam');ml=unreal.MaterialEditingLibrary;ml.delete_all_material_expressions(m)
m.set_editor_property('blend_mode',unreal.BlendMode.BLEND_ADDITIVE)
c=ml.create_material_expression(m,unreal.MaterialExpressionConstant3Vector);c.set_editor_property('constant',unreal.LinearColor(180,90,20,1))
o=ml.create_material_expression(m,unreal.MaterialExpressionConstant);o.set_editor_property('r',.045)
ml.connect_material_property(c,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR);ml.connect_material_property(o,'',unreal.MaterialProperty.MP_OPACITY)
ml.recompile_material(m);unreal.EditorAssetLibrary.save_loaded_asset(m)
assert L.save_current_level()
unreal.log('GIANT_DESK_V02_READY')
