"""One-time upgrade of the existing passage map. Reuses existing card-room mesh/material assets."""
import unreal, math, shutil
from pathlib import Path
MAP='/Game/Passages/Echo/Maps/Lvl_EchoMazeToCards_P1'
ROOM='/Game/Rooms/CardDuel/Maps/Lvl_CardRoom_P1'
TAG='GiantCardRoomV01'
SCALE=6.0
ORIGIN=unreal.Vector(10540,3600,0)
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
root=Path(unreal.Paths.project_dir())
source=root/'Content/Passages/Echo/Maps/Lvl_EchoMazeToCards_P1.umap'
backup=root/'Saved/GiantRoomV01Backup/Content/Passages/Echo/Maps/Lvl_EchoMazeToCards_P1.umap'
backup.parent.mkdir(parents=True,exist_ok=True)
if not backup.exists():shutil.copy2(source,backup)
if not levels.load_level(ROOM):raise RuntimeError('Cannot read original room')
records=[]
for a in actors.get_all_level_actors():
    label=a.get_actor_label()
    if label.startswith('Entry wall'):continue
    p=a.get_actor_location();rot=a.get_actor_rotation();scale=a.get_actor_scale3d()
    data={'label':label,'p':(p.x,p.y,p.z),'r':(rot.pitch,rot.yaw,rot.roll),'s':(scale.x,scale.y,scale.z)}
    if isinstance(a,unreal.StaticMeshActor):
        c=a.static_mesh_component
        if not c.static_mesh:continue
        data.update(kind='mesh',mesh=c.static_mesh.get_path_name(),mats=[m.get_path_name() if m else None for m in c.get_materials()],collision=c.get_collision_enabled())
    elif isinstance(a,unreal.PointLight):
        c=a.light_component;color=c.get_editor_property('light_color')
        data.update(kind='light',intensity=c.get_editor_property('intensity'),radius=c.get_editor_property('attenuation_radius'),color=(color.r,color.g,color.b,color.a),shadow=c.get_editor_property('cast_shadows'))
    elif isinstance(a,unreal.TextRenderActor):
        c=a.get_component_by_class(unreal.TextRenderComponent);color=c.get_editor_property('text_render_color')
        data.update(kind='text',text=str(c.get_editor_property('text')),size=c.get_editor_property('world_size'),color=(color.r,color.g,color.b,color.a))
    else:continue
    records.append(data)
if not records:raise RuntimeError('Room source is empty')
if not levels.load_level(MAP):raise RuntimeError('Cannot open passage')
current=actors.get_all_level_actors()
if any(unreal.Name(TAG) in a.tags for a in current):raise RuntimeError('Already upgraded; edit existing giant room rather than duplicate it')
echo=next(a for a in current if isinstance(a,unreal.EchoPassageStage))
echo.set_editor_property('room_in_same_level',True)
echo.set_editor_property('destination_level',None)
echo.set_actor_transform(echo.get_actor_transform(),False,True)
def mark(a,label):
    a.set_actor_label(label);a.tags=[unreal.Name(TAG)];return a
for d in records:
    pos=ORIGIN+unreal.Vector(*(v*SCALE for v in d['p']))
    rot=unreal.Rotator(pitch=d['r'][0],yaw=d['r'][1],roll=d['r'][2])
    if d['kind']=='mesh':
        a=actors.spawn_actor_from_class(unreal.StaticMeshActor,pos,rot);c=a.static_mesh_component
        c.set_static_mesh(unreal.load_asset(d['mesh']))
        for i,path in enumerate(d['mats']):
            if path:c.set_material(i,unreal.load_asset(path))
        c.set_collision_enabled(d['collision']);a.set_actor_scale3d(unreal.Vector(*(v*SCALE for v in d['s'])))
    elif d['kind']=='light':
        a=actors.spawn_actor_from_class(unreal.PointLight,pos,rot);c=a.light_component
        c.set_editor_property('mobility',unreal.ComponentMobility.MOVABLE)
        c.set_intensity(d['intensity']*SCALE*SCALE);c.set_editor_property('attenuation_radius',d['radius']*SCALE)
        c.set_editor_property('source_radius',24*SCALE);c.set_editor_property('light_color',unreal.Color(*d['color']))
        c.set_editor_property('cast_shadows',d['shadow'])
    else:
        a=actors.spawn_actor_from_class(unreal.TextRenderActor,pos,rot);c=a.get_component_by_class(unreal.TextRenderComponent)
        c.set_text(d['text']);c.set_world_size(d['size']*SCALE);c.set_text_render_color(unreal.Color(*d['color']))
        c.set_horizontal_alignment(unreal.HorizTextAligment.EHTA_CENTER)
    mark(a,'Giant / '+d['label'])
def cube(label,pos,size,material,collision=True):
    a=mark(actors.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(*pos)),label)
    a.static_mesh_component.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Cube'))
    a.static_mesh_component.set_material(0,unreal.load_asset(material));a.set_actor_scale3d(unreal.Vector(*(v/100 for v in size)))
    if not collision:
        a.static_mesh_component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION);a.set_actor_enable_collision(False)
    return a
plaster='/Game/Rooms/CardDuel/Materials/M_Plaster'
# Preserve the giant wall except for a 4 m wide, 3.8 m tall diggable patch.
for side in [-1,1]:cube('Giant / Entrance side wall',(6400,3600+side*1510,1410),(150,2620,2820),plaster)
cube('Giant / Entrance upper wall',(6400,3600,1600),(150,400,2440),plaster)
wall=mark(actors.spawn_actor_from_class(unreal.MiningGiantRoomEntrance,unreal.Vector(6400,3600,190)),'Giant / Diggable plaster and warm pinhole')
wall.set_editor_property('rock_material',unreal.load_asset(plaster));wall.reset_rock()
# Retain the same opponent shape and effects, at a smaller scale than the giant furniture.
stage=mark(actors.spawn_actor_from_class(unreal.CardRoomStage,unreal.Vector(10540,3600,460)),'Giant / Tabletop card encounter')
stage.set_actor_scale3d(unreal.Vector(2,2,2));stage.set_editor_property('tabletop_entry',True);stage.set_editor_property('room_id','GiantCardDuel_P1')
# A real room light shines through the actual mesh aperture.
lamp=mark(actors.spawn_actor_from_class(unreal.SpotLight,unreal.Vector(6840,3600,155),unreal.Rotator(pitch=0,yaw=180,roll=0)),'Giant / Light through the pinhole')
c=lamp.light_component;c.set_editor_property('mobility',unreal.ComponentMobility.MOVABLE);c.set_intensity(18000)
c.set_editor_property('attenuation_radius',1200);c.set_editor_property('inner_cone_angle',16);c.set_editor_property('outer_cone_angle',26)
c.set_light_color(unreal.LinearColor(1,.65,.28,1));c.set_editor_property('cast_shadows',True)
# Very localized rim light keeps the plaster at the hole readable without revealing the maze.
lamp=mark(actors.spawn_actor_from_class(unreal.PointLight,unreal.Vector(6200,3600,175)),'Giant / Pinhole reflected warm rim')
c=lamp.light_component;c.set_editor_property('mobility',unreal.ComponentMobility.MOVABLE);c.set_intensity(5);c.set_editor_property('attenuation_radius',390);c.set_light_color(unreal.LinearColor(1,.63,.25,1));c.set_editor_property('cast_shadows',False)
# Table landing target is the scaled encounter approach position, physically on the tabletop.
landing=unreal.Vector(10000,3600,695)
for i in range(16):
    angle=i*2*math.pi/16
    a=cube('Giant / Table landing ring',(landing.x+145*math.cos(angle),landing.y+145*math.sin(angle),landing.z),(52,9,3),'/Game/Rooms/CardDuel/Materials/M_Lamp',False)
    a.set_actor_rotation(unreal.Rotator(pitch=0,yaw=math.degrees(angle)+90,roll=0),False)
for y in [-380,380]:
    source=unreal.Vector(landing.x,landing.y+y,2300)
    rotation=unreal.MathLibrary.find_look_at_rotation(source,landing)
    lamp=mark(actors.spawn_actor_from_class(unreal.SpotLight,source,rotation),'Giant / Table stage spotlight')
    c=lamp.light_component;c.set_editor_property('mobility',unreal.ComponentMobility.MOVABLE);c.set_intensity(190000)
    c.set_editor_property('attenuation_radius',2600);c.set_editor_property('inner_cone_angle',11);c.set_editor_property('outer_cone_angle',22)
    c.set_light_color(unreal.LinearColor(1,.81,.43,1));c.set_editor_property('cast_shadows',True)
# Low-poly translucent beam adds a legible vertical cue without changing existing room models.
asset_tools=unreal.AssetToolsHelpers.get_asset_tools();ml=unreal.MaterialEditingLibrary
path='/Game/Rooms/CardDuel/Materials/M_GiantStageBeam'
beam=unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else asset_tools.create_asset('M_GiantStageBeam','/Game/Rooms/CardDuel/Materials',unreal.Material,unreal.MaterialFactoryNew())
ml.delete_all_material_expressions(beam);beam.set_editor_property('blend_mode',unreal.BlendMode.BLEND_TRANSLUCENT)
beam.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT);beam.set_editor_property('two_sided',True)
color=ml.create_material_expression(beam,unreal.MaterialExpressionConstant3Vector);color.set_editor_property('constant',unreal.LinearColor(1,.68,.28,1))
opacity=ml.create_material_expression(beam,unreal.MaterialExpressionConstant);opacity.set_editor_property('r',.075)
ml.connect_material_property(color,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR);ml.connect_material_property(opacity,'',unreal.MaterialProperty.MP_OPACITY)
ml.recompile_material(beam);unreal.EditorAssetLibrary.save_loaded_asset(beam)
a=mark(actors.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(landing.x,landing.y,1490)),'Giant / Table stage light beam')
a.static_mesh_component.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Cone'));a.static_mesh_component.set_material(0,beam)
a.static_mesh_component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION);a.static_mesh_component.set_cast_shadow(False)
a.set_actor_scale3d(unreal.Vector(6.4,6.4,15.9));a.set_actor_enable_collision(False)
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
world.get_world_settings().set_editor_property('default_game_mode',unreal.MiningEncounterGameMode)
if not levels.save_current_level():raise RuntimeError('Map save failed')
unreal.log('GIANT_ROOM_SAME_LEVEL_READY: '+MAP+'; copied '+str(len(records))+' actors')
