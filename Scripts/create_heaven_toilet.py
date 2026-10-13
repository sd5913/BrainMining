"""Create the isolated, editable Heaven Toilet level. Existing maps are never overwritten.

Run in a dedicated UE editor process with -run=pythonscript -script=<this file>.
The OBJ source is generated deterministically; all level dressing is authored in the map.
"""
import math
from pathlib import Path
import unreal

ROOT = '/Game/CollectionPrototype/HeavenToilet'
MAP = ROOT + '/Maps/Lvl_HeavenToilet'
RADIUS = 2200.0
CENTER = 3460.0
assetlib = unreal.EditorAssetLibrary
assets = unreal.AssetToolsHelpers.get_asset_tools()
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
lib = unreal.MaterialEditingLibrary

if assetlib.does_asset_exist(MAP):
    raise RuntimeError('Heaven Toilet map already exists; preserving authored edits.')

def material(name, color, metallic=0, roughness=.65, glow=.08):
    path = ROOT + '/Materials/' + name
    if assetlib.does_asset_exist(path):
        return unreal.load_asset(path)
    m = assets.create_asset(name, ROOT + '/Materials', unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property('two_sided', True)
    c = lib.create_material_expression(m, unreal.MaterialExpressionConstant3Vector)
    c.set_editor_property('constant', unreal.LinearColor(*color, 1))
    lib.connect_material_property(c, '', unreal.MaterialProperty.MP_BASE_COLOR)
    for value, prop in [(metallic, unreal.MaterialProperty.MP_METALLIC), (roughness, unreal.MaterialProperty.MP_ROUGHNESS)]:
        n = lib.create_material_expression(m, unreal.MaterialExpressionConstant)
        n.set_editor_property('r', value)
        lib.connect_material_property(n, '', prop)
    e = lib.create_material_expression(m, unreal.MaterialExpressionConstant3Vector)
    e.set_editor_property('constant', unreal.LinearColor(*(v*glow for v in color), 1))
    lib.connect_material_property(e, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    lib.recompile_material(m)
    assetlib.save_loaded_asset(m)
    return m

pearl = material('M_HeavenPearl', (.48, .64, .70), roughness=.8, glow=.22)
floor_mat = material('M_HeavenFloor', (.24, .36, .40), metallic=.18, roughness=.43)
porcelain = material('M_HeavenPorcelain', (.83, .86, .77), roughness=.28)
gold = material('M_HeavenBrass', (.62, .38, .12), metallic=.65, roughness=.32)
pipe_mat = material('M_HeavenPipe', (.08, .25, .26), metallic=.65, roughness=.38)
glow = material('M_HeavenLight', (.65, .84, 1), glow=3)
warm_glow = material('M_HeavenWarmLight', (1, .70, .30), glow=4)
stone = material('M_HeavenBedrock', (.09, .13, .14), roughness=.95)
earth = material('M_HeavenEarth', (.28, .14, .065), roughness=.95)
earth_light = material('M_HeavenEarthLight', (.36, .21, .105), roughness=.95)

# Both faces and collision describe a hollow dome, never a scaled solid sphere.
# Azimuth/elevation boundaries explicitly include the walkable portal edges.
source = Path(unreal.Paths.project_dir()) / 'SourceAssets/HeavenToilet'
source.mkdir(parents=True, exist_ok=True)
half_portal = math.asin(310/RADIUS)
portal_elevation = math.asin(430/RADIUS)
azimuths = sorted(set([2*math.pi*i/96 for i in range(97)] + [math.pi-half_portal, math.pi+half_portal]))
elevations = sorted(set([math.pi*.5*i/24 for i in range(24)] + [portal_elevation]))
verts, faces = [], []
for e in elevations:
    for a in azimuths:
        # FBX OBJ import mirrors Y; encode that convention explicitly.
        verts.append((RADIUS*math.cos(e)*math.cos(a), -RADIUS*math.cos(e)*math.sin(a), RADIUS*math.sin(e)))
stride = len(azimuths)
for j in range(len(elevations)-1):
    for i in range(stride-1):
        a = (azimuths[i]+azimuths[i+1])*.5
        e = (elevations[j]+elevations[j+1])*.5
        if abs(a-math.pi) < half_portal and e < portal_elevation:
            continue
        v = j*stride+i+1
        faces.extend([(v, v+stride, v+stride+1), (v, v+stride+1, v+1)])
verts.append((0, 0, RADIUS))
top = len(verts)
for i in range(stride-1):
    v = (len(elevations)-1)*stride+i+1
    faces.append((v, top, v+1))
obj = source/'SM_HeavenHemisphere.obj'
obj.write_text('o HeavenHemisphere\ns off\n' + ''.join('v %.6f %.6f %.6f\n'%v for v in verts) + ''.join('f %d %d %d\n'%f for f in faces), encoding='ascii')
mesh_path = ROOT + '/Meshes/SM_HeavenHemisphere'
if not assetlib.does_asset_exist(mesh_path):
    opt = unreal.FbxImportUI()
    opt.automated_import_should_detect_type = False
    opt.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
    opt.import_materials = False
    opt.import_textures = False
    opt.static_mesh_import_data.combine_meshes = True
    opt.static_mesh_import_data.auto_generate_collision = False
    opt.static_mesh_import_data.normal_import_method = unreal.FBXNormalImportMethod.FBXNIM_COMPUTE_NORMALS
    task = unreal.AssetImportTask()
    task.filename = str(obj)
    task.destination_path = ROOT + '/Meshes'
    task.destination_name = 'SM_HeavenHemisphere'
    task.automated = True
    task.save = True
    task.options = opt
    task.factory = unreal.FbxFactory()
    assets.import_asset_tasks([task])
dome_mesh = unreal.load_asset(mesh_path)
assert dome_mesh, 'Dome import failed'
bounds = dome_mesh.get_bounding_box()
assert abs(bounds.max.z-RADIUS)<2 and abs(bounds.min.z)<2, str(bounds)
body = dome_mesh.get_editor_property('body_setup')
body.set_editor_property('collision_trace_flag', unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
body.set_editor_property('double_sided_geometry', True)
dome_mesh.set_material(0, pearl)
assetlib.save_loaded_asset(dome_mesh)

assert levels.new_level(MAP)

def mesh_actor(label, mesh, pos, scale=(1,1,1), mat=None, collision=True, folder='Heaven/Room'):
    a = actors.spawn_actor_from_class(unreal.StaticMeshActor, pos if isinstance(pos, unreal.Vector) else unreal.Vector(*pos))
    a.set_actor_label(label)
    a.set_folder_path(folder)
    c = a.static_mesh_component
    c.set_static_mesh(mesh if isinstance(mesh, unreal.StaticMesh) else unreal.load_asset('/Engine/BasicShapes/'+mesh))
    if mat:
        c.set_material(0, mat)
    c.set_collision_profile_name('BlockAll' if collision else 'NoCollision')
    a.set_actor_scale3d(unreal.Vector(*scale))
    return a

def cube(label, pos, size, mat, collision=True, folder='Heaven/Entrance'):
    return mesh_actor(label, 'Cube', pos, tuple(v/100 for v in size), mat, collision, folder)

def segment(label, p, q, radius, mat, folder='Heaven/Pipe', collision=False):
    p, q = unreal.Vector(*p), unreal.Vector(*q)
    d = q-p
    a = mesh_actor(label, 'Cylinder', (p+q)*.5, (radius/50, radius/50, d.length()/100), mat, collision, folder)
    a.set_actor_rotation(unreal.MathLibrary.make_rot_from_z(d), False)
    return a

def ring(label, center, radius, tube, mat, count=96):
    for i in range(count):
        a, b = 2*math.pi*i/count, 2*math.pi*(i+1)/count
        segment(label+' %02d'%i, (center[0]+radius*math.cos(a),center[1]+radius*math.sin(a),center[2]),
                (center[0]+radius*math.cos(b),center[1]+radius*math.sin(b),center[2]),tube,mat,'Heaven/Trim')

def lamp(label, pos, color, intensity, radius, folder='Heaven/Lighting'):
    a = actors.spawn_actor_from_class(unreal.PointLight, unreal.Vector(*pos))
    a.set_actor_label(label)
    a.set_folder_path(folder)
    c = a.light_component
    c.set_editor_property('mobility', unreal.ComponentMobility.MOVABLE)
    c.set_editor_property('intensity_units', unreal.LightUnits.CANDELAS)
    c.set_intensity(intensity)
    c.set_editor_property('attenuation_radius', radius)
    c.set_light_color(unreal.LinearColor(*color, 1))
    c.set_editor_property('cast_shadows', label in ['Heaven central key', 'Entrance warm key'])
    return a

mesh_actor('Heaven hemisphere - diameter 44m height 22m', dome_mesh, (CENTER,0,0))
mesh_actor('Heaven circular floor', 'Cylinder', (CENTER,0,-35), (45.2,45.2,.7), floor_mat)
cube('Entrance continuous floor', (190,0,-35), (2540,1580,70), stone)
for side in [-1,1]:
    cube('Entrance bedrock side', (210,side*805,450), (2600,90,970), stone)
cube('Entrance roof', (210,0,920), (2600,1700,80), stone)
cube('Entrance back wall', (-1100,0,450), (80,1700,970), stone)

# Three real excavatable strata. No fake entrance texture or pre-opened tunnel.
for i, (bottom, top) in enumerate([(0,260),(260,510),(510,880)]):
    rock = actors.spawn_actor_from_class(unreal.DiggableRock, unreal.Vector(920,0,(bottom+top)*.5))
    rock.set_actor_label('Heaven diggable soil stratum %02d'%i)
    rock.set_folder_path('Heaven/Soil')
    rock.set_editor_property('rock_size', unreal.Vector(920,1520,top-bottom))
    rock.set_editor_property('rock_material', earth_light if i==1 else earth)
    rock.reset_rock()

# Pipe remains unbroken from the exposed spout, through soil and into the cistern.
points = [(-300,125,130),(-300,125,220),(1550,125,220),(1550,125,26),
          (3540,125,26),(3540,0,26),(3540,0,88),(3498,0,88)]
for i,(p,q) in enumerate(zip(points,points[1:])):
    segment('Heaven continuous water pipe %02d'%i,p,q,10,pipe_mat)
for i,p in enumerate(points[1:-1]):
    mesh_actor('Heaven pipe elbow %02d'%i,'Sphere',p,(.26,.26,.26),pipe_mat,False,'Heaven/Pipe')
for x in range(-240,1510,150):
    segment('Heaven pipe coupling', (x-6,125,220),(x+6,125,220),15,gold)
for x in range(1720,3500,240):
    segment('Heaven floor pipe coupling',(x-6,125,26),(x+6,125,26),14,gold)
for z in [35,65,95]:
    mesh_actor('Heaven water droplet','Sphere',(-300,125,z),(.06,.06,.11),glow,False,'Heaven/Pipe')
mesh_actor('Heaven spout catch basin','Cylinder',(-300,125,3),(1,1,.06),pipe_mat,False,'Heaven/Pipe')

# The original small collectible is deliberately kept at human scale in the large hall.
mesh_actor('Heaven central porcelain dais','Cylinder',(CENTER,0,5),(4.4,4.4,.1),porcelain)
ring('Heaven central brass inlay',(CENTER,0,11),214,2.5,gold,64)
ring('Heaven floor halo',(CENTER,0,2),330,3,warm_glow,72)
ring('Heaven suspended halo',(CENTER,0,480),145,5,warm_glow,64)
ring('Heaven lower cove',(CENTER,0,22),2178,6,glow)
for z in [880,1540]:
    ring('Heaven dome latitude',(CENTER,0,z),math.sqrt(RADIUS**2-z*z)-12,4,gold)
# Thin curved ribs make the hemisphere's volume legible from inside.
for i in range(12):
    a=2*math.pi*i/12
    for j in range(18):
        e1=math.radians(14+74*j/18)
        e2=math.radians(14+74*(j+1)/18)
        r=RADIUS-10
        p=(CENTER+r*math.cos(e1)*math.cos(a),r*math.cos(e1)*math.sin(a),r*math.sin(e1))
        q=(CENTER+r*math.cos(e2)*math.cos(a),r*math.cos(e2)*math.sin(a),r*math.sin(e2))
        segment('Heaven curved meridian',p,q,3,gold,'Heaven/Trim')

lamp('Entrance warm key',(-100,-220,490),(1,.71,.43),650,1700)
lamp('Entrance cool fill',(280,330,420),(.36,.66,1),250,1200)
lamp('Heaven central key',(CENTER-170,-180,560),(1,.89,.68),1700,1900)
lamp('Heaven central cool fill',(CENTER+300,250,380),(.40,.73,1),700,1800)
for i in range(8):
    a=2*math.pi*i/8
    lamp('Heaven dome wash %02d'%i,(CENTER+1400*math.cos(a),1400*math.sin(a),820),(.50,.72,1),1400,3400)
lamp('Heaven zenith',(CENTER,0,1800),(.75,.86,1),4000,3600)

field=actors.spawn_actor_from_class(unreal.MiningCollectionField,unreal.Vector())
field.set_actor_label('Heaven Toilet - single authored collectible')
field.set_folder_path('Heaven/Gameplay')
field.set_editor_property('use_authored_placements',True)
placement=unreal.MiningCollectionPlacement()
placement.set_editor_property('item_id',0)
placement.set_editor_property('transform',unreal.Transform(location=unreal.Vector(CENTER,0,64)))
placement.set_editor_property('area_tag','HeavenToilet')
field.set_editor_property('authored_placements',[placement])

start=actors.spawn_actor_from_class(unreal.PlayerStart,unreal.Vector(-720,-100,100),unreal.Rotator())
start.set_actor_label('Heaven start - follow exposed pipe')
start.set_folder_path('Heaven/Gameplay')
pp=actors.spawn_actor_from_class(unreal.PostProcessVolume,unreal.Vector())
pp.set_actor_label('Heaven fixed exposure')
pp.set_folder_path('Heaven/Lighting')
pp.set_editor_property('unbound',True)
settings=pp.get_editor_property('settings')
for name,value in [('auto_exposure_min_brightness',4),('auto_exposure_max_brightness',4),('auto_exposure_bias',0),('bloom_intensity',.25)]:
    settings.set_editor_property('override_'+name,True)
    settings.set_editor_property(name,value)
pp.set_editor_property('settings',settings)
ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
ed.get_editor_world().get_world_settings().set_editor_property('default_game_mode',unreal.MiningCollectionGameMode)
ed.set_level_viewport_camera_info(unreal.Vector(1700,-730,450),unreal.Rotator(pitch=10,yaw=23,roll=0))
assert levels.save_current_level()
unreal.log('HEAVEN_TOILET_CREATED '+MAP)

# Apply the current 60% / shallow-water revision after constructing the base map.
import runpy
runpy.run_path(str(Path(unreal.Paths.project_dir())/'Scripts/update_heaven_toilet_water.py'))
