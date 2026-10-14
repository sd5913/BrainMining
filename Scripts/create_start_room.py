"""Author the standalone miner bedroom. Refuses to overwrite an existing level.

Run in UE Python after compiling MiningStartRoom. Shared source assets are read-only.
"""
import json
import math
import traceback
from pathlib import Path
import unreal

ROOT = '/Game/StartRoom'
MAP = ROOT + '/Maps/Lvl_StartRoom'
project = Path(unreal.Paths.project_dir()).resolve()
out = project / 'Saved/Validation/StartRoom'
out.mkdir(parents=True, exist_ok=True)
assetlib = unreal.EditorAssetLibrary
assets = unreal.AssetToolsHelpers.get_asset_tools()
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
report = {'passed': False, 'map': MAP}

def load(path):
    obj = unreal.load_asset(path)
    assert obj, path
    return obj

def shape(label, pos, size, mat, mesh='Cube', rotation=None, collision=True, folder='Room'):
    a = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(*pos))
    a.set_actor_label(label)
    a.set_folder_path('StartRoom/' + folder)
    c = a.static_mesh_component
    c.set_static_mesh(load('/Engine/BasicShapes/' + mesh))
    c.set_material(0, mat)
    c.set_collision_profile_name('BlockAll' if collision else 'NoCollision')
    a.set_actor_scale3d(unreal.Vector(*(v / 100 for v in size)))
    if rotation: a.set_actor_rotation(rotation, False)
    return a

def bar(label, start, end, width, mat, folder='Details', collision=False):
    a, b = unreal.Vector(*start), unreal.Vector(*end)
    rot = unreal.MathLibrary.find_look_at_rotation(a, b)
    return shape(label, ((a.x+b.x)/2,(a.y+b.y)/2,(a.z+b.z)/2), ((b-a).length(),width,width), mat,
                 rotation=rot, collision=collision, folder=folder)

def text(label, words, pos, yaw=180, height=15, color=(.85,.76,.58)):
    a = actors.spawn_actor_from_class(unreal.TextRenderActor, unreal.Vector(*pos), unreal.Rotator(yaw=yaw))
    a.set_actor_label(label)
    a.set_folder_path('StartRoom/Signs')
    c = a.text_render
    c.set_text(words)
    c.set_world_size(height)
    c.set_text_render_color(unreal.Color(*(int(v*255) for v in color),255))
    c.set_horizontal_alignment(unreal.HorizTextAligment.EHTA_CENTER)
    return a

def light(label, pos, intensity, radius, rgb):
    a = actors.spawn_actor_from_class(unreal.PointLight, unreal.Vector(*pos))
    a.set_actor_label(label)
    a.set_folder_path('StartRoom/Lighting')
    c = a.point_light_component
    c.set_editor_property('mobility', unreal.ComponentMobility.MOVABLE)
    c.set_editor_property('intensity_units', unreal.LightUnits.UNITLESS)
    c.set_editor_property('use_inverse_squared_falloff', False)
    c.set_editor_property('intensity', intensity)
    c.set_editor_property('attenuation_radius', radius)
    c.set_editor_property('light_falloff_exponent', 2.)
    c.set_editor_property('cast_shadows', True)
    c.set_light_color(unreal.LinearColor(*rgb,1))
    return a

def blueprint(name, parent):
    path = ROOT + '/Blueprints/' + name
    assert not assetlib.does_asset_exist(path), path
    f = unreal.BlueprintFactory()
    f.set_editor_property('parent_class', parent)
    bp = assets.create_asset(name, ROOT+'/Blueprints', unreal.Blueprint, f)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    assert assetlib.save_loaded_asset(bp)
    return bp, unreal.load_class(None, path+'.'+name+'_C')

try:
    assert not assetlib.does_asset_exist(MAP), 'StartRoom already exists; preserve authored edits.'
    assert not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world(), 'Stop PIE before authoring.'
    assert unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
    assetlib.make_directory(ROOT)
    # Reuse the established material palette and imported pickaxe; never edit their source packages.
    mats = {name: load('/Game/Rooms/CardDuel/Materials/'+path) for name,path in {
        'wood':'M_Oak','wood_light':'M_OakLight','metal':'M_Dark','brass':'M_Gold',
        'linen':'M_Cream','cloth':'M_GiantDeskCloth','red':'M_BookRed','paper':'M_GiantDeskPaper',
        'light':'M_Lamp','green':'M_BookGreen','ink':'M_GiantDeskInk',
        'rug':'MI_StudyV10_RugRust','leather':'MI_StudyV10_Leather','teal':'MI_StudyV10_BagTeal'}.items()}
    stone = load('/Game/Campaign/Materials/M_CampaignLimestone')
    source_character = '/Game/Showcase/Blueprints/BP_SharedMiningCharacter'
    character_path = ROOT+'/Blueprints/BP_StartRoomCharacter'
    assert not assetlib.does_asset_exist(character_path)
    cbp = assetlib.duplicate_asset(source_character, character_path)
    char_cls = unreal.load_class(None, character_path+'.BP_StartRoomCharacter_C')
    char = unreal.get_default_object(char_cls)
    flight = char.get_component_by_class(unreal.MiningCharacterMovementComponent)
    settings = flight.get_editor_property('flight_settings')
    settings.set_editor_property('enabled', True)
    settings.set_editor_property('enable_heat', True)
    flight.set_editor_property('flight_settings', settings)
    char.get_component_by_class(unreal.MiningComponent).set_editor_property('fall_reset_distance', 0.)
    unreal.BlueprintEditorLibrary.compile_blueprint(cbp)
    assert assetlib.save_loaded_asset(cbp)
    mbp, mode_cls = blueprint('BP_StartRoomGameMode', unreal.MiningStartRoomGameMode)
    unreal.get_default_object(mode_cls).set_editor_property('default_pawn_class', char_cls)
    unreal.BlueprintEditorLibrary.compile_blueprint(mbp)
    assert assetlib.save_loaded_asset(mbp)
    pbp, pickup_cls = blueprint('BP_StarterEquipment', unreal.MiningStarterPickup)
    dbp, director_cls = blueprint('BP_StartRoom', unreal.MiningStartRoom)
    assert levels.new_level(MAP)
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    world.get_world_settings().set_editor_property('default_game_mode', mode_cls)
    world.get_world_settings().set_editor_property('kill_z', -3000.)

    # 9 x 7.2 m bedroom; the east wall has exactly one 2.4 x 2.5 m opening.
    shape('Bedroom_SolidFloor', (0,0,-20), (960,760,40), stone)
    for i in range(12):
        shape('Floor_Plank_%02d'%i, (-435+i*79,0,2), (77,718,4), mats['wood_light' if i%3==0 else 'wood'])
    for label,pos,size in [
        ('West_ClosedWall',(-470,0,165),(40,760,330)),
        ('North_ClosedWall',(0,370,165),(940,40,330)),
        ('South_ClosedWall',(0,-370,165),(940,40,330)),
        ('East_WallLeft',(470,-245,165),(40,250,330)),
        ('East_WallRight',(470,245,165),(40,250,330)),
        ('East_OverDoor',(470,0,292),(40,240,76)),
        ('Ceiling',(0,0,350),(980,780,40))]: shape(label,pos,size,stone)
    for x in [-440,-220,20,250,440]:
        shape('Ceiling_TimberBeam',(x,0,320),(18,720,22),mats['wood'],folder='Timber')
        for y in [-348,348]:
            shape('Wall_TimberPost',(x,y,155),(18,18,310),mats['wood'],folder='Timber')
            for z in [30,150,290]: shape('Timber_IronBand',(x,y,z),(20,20,7),mats['metal'],folder='Timber')
    for y in [-349,349]: shape('Wall_Skirting',(0,y,18),(920,8,28),mats['wood'],folder='Timber')
    for y in [-126,126]: shape('Exit_TimberJamb',(452,y,135),(34,20,270),mats['wood'],folder='Exit')
    shape('Exit_TimberLintel',(452,0,264),(34,270,22),mats['wood'],folder='Exit')
    rock = actors.spawn_actor_from_class(unreal.DiggableRock, unreal.Vector(525,0,125))
    rock.set_actor_label('EXIT_DiggableRockLayer')
    rock.set_folder_path('StartRoom/Exit')
    rock.set_editor_property('rock_size',unreal.Vector(110,240,250))
    rock.set_editor_property('rock_material', stone)
    rock.set_editor_property('tags',['BM_StartRoom_ExitBlocker'])
    rock.reset_rock()
    for label,pos,size in [('TunnelFloor',(830,0,-20),(720,320,40)),('TunnelNorth',(830,170,150),(720,40,300)),
                           ('TunnelSouth',(830,-170,150),(720,40,300)),('TunnelRoof',(830,0,315),(720,380,30)),
                           ('FutureConnectionCap',(1190,0,150),(40,380,300))]:
        shape(label,pos,size,stone,folder='Exit/Tunnel')
    for x in [680,940,1140]:
        for y in [-140,140]: shape('Tunnel_Support',(x,y,135),(15,15,270),mats['wood'],folder='Exit/Tunnel')
        shape('Tunnel_Crossbeam',(x,0,270),(15,295,18),mats['wood'],folder='Exit/Tunnel')
    text('Exit_Label','MINE ACCESS', (429,0,281),height=16)

    # Bed: timber frame, linen mattress, pillow, tucked green blanket and metal bands.
    shape('Bed_Rug',(-260,-215,6),(280,170,3),mats['rug'],collision=False,folder='Bed')
    shape('Bed_Frame',(-260,-225,34),(218,128,16),mats['wood'],folder='Bed')
    shape('Bed_Mattress',(-260,-225,52),(207,118,24),mats['linen'],folder='Bed')
    for x in [-359,-161]:
        for y in [-280,-170]:
            shape('Bed_Post',(x,y,32),(10,10,64),mats['wood'],folder='Bed')
            shape('Bed_PostCap',(x,y,67),(12,12,7),mats['brass'],collision=False,folder='Bed')
    shape('Bed_Headboard',(-364,-225,76),(10,128,90),mats['wood'],folder='Bed')
    for y in [-274,-245,-216,-187]: shape('Headboard_Slat',(-357,y,82),(3,15,65),mats['wood_light'],folder='Bed')
    shape('Pillow',(-326,-225,73),(44,83,16),mats['linen'],'Sphere',collision=False,folder='Bed')
    shape('Blanket',(-217,-225,67),(126,119,7),mats['cloth'],collision=False,folder='Bed')
    shape('Blanket_DrapedEdge',(-217,-286,49),(126,5,39),mats['cloth'],collision=False,folder='Bed')
    for x in [-269,-260]: shape('Blanket_Stripe',(x,-225,71),(3,119,1),mats['linen'],collision=False,folder='Bed')
    shape('Bedside_Cabinet',(-378,-91,34),(58,55,68),mats['wood'],folder='Bed')
    shape('Bedside_DrawerFace',(-347,-91,43),(3,48,29),mats['wood_light'],folder='Bed')
    shape('Drawer_Pull',(-344,-91,43),(4,12,3),mats['brass'],collision=False,folder='Bed')

    # Dresser, hanging work clothes, hard hat, boots and lockers give the room a miner's identity.
    shape('Dresser',(-412,182,55),(70,140,110),mats['wood'],folder='Dressing')
    for y in [142,217]:
        shape('Dresser_Door',(-374,y,57),(4,66,96),mats['wood_light'],folder='Dressing')
        shape('Dresser_Handle',(-370,y,58),(5,4,16),mats['metal'],collision=False,folder='Dressing')
    shape('Hardhat_Brim',(-410,160,114),(53,43,5),mats['brass'],'Cylinder',collision=False,folder='Dressing')
    shape('Hardhat_Crown',(-410,160,126),(43,35,24),mats['brass'],'Sphere',collision=False,folder='Dressing')
    shape('Hardhat_Lamp',(-385,160,125),(7,14,12),mats['metal'],collision=False,folder='Dressing')
    shape('Hardhat_Lens',(-380,160,125),(2,10,8),mats['light'],collision=False,folder='Dressing')
    for y in [165,196]:
        shape('WorkBoot_Foot',(-315,y,13),(54,23,25),mats['leather'],'Sphere',collision=False,folder='Dressing')
        shape('WorkBoot_Shaft',(-330,y,31),(25,24,38),mats['leather'],'Cylinder',collision=False,folder='Dressing')
        shape('WorkBoot_Collar',(-330,y,50),(27,26,7),mats['metal'],'Cylinder',collision=False,folder='Dressing')
    shape('Coat_HookBoard',(-110,344,219),(128,8,17),mats['wood'],folder='Dressing')
    shape('WorkCoat_Body',(-110,329,166),(65,15,91),mats['teal'],collision=False,folder='Dressing')
    for x in [-151,-69]: shape('WorkCoat_Sleeve',(x,329,171),(23,15,74),mats['teal'],rotation=unreal.Rotator(pitch=18 if x==-151 else -18),collision=False,folder='Dressing')
    for z in [170,143]: shape('Coat_ReflectiveStrip',(-110,318,z),(63,2,5),mats['linen'],collision=False,folder='Dressing')
    for x in [-130,-90]: shape('Coat_Pocket',(x,318,130),(18,3,18),mats['metal'],collision=False,folder='Dressing')
    # Crates, ore specimens, a route sketch and an enamel mug reuse existing assets/palette.
    for x,y,z in [(298,268,35),(372,272,35),(340,273,102)]:
        shape('Supply_Crate',(x,y,z),(65,62,64),mats['wood_light'],folder='Supplies')
        for dz in [-20,20]: shape('Crate_Band',(x,y,z+dz),(68,65,5),mats['wood'],folder='Supplies')
        shape('Crate_Diagonal',(x,y-33,z),(75,4,8),mats['wood'],rotation=unreal.Rotator(pitch=40),collision=False,folder='Supplies')
    shape('Ore_Shelf',(120,-337,159),(180,29,8),mats['wood'],folder='Supplies')
    for i in range(4):
        mesh = load('/Game/Showcase/Treasure/Meshes/SM_Treasure_%02d'%(i+1))
        a=actors.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(55+i*39,-335,167))
        a.set_actor_label('Ore_Specimen_%d'%i);a.set_folder_path('StartRoom/Supplies')
        c=a.static_mesh_component;c.set_static_mesh(mesh);c.set_collision_profile_name('NoCollision')
        b=mesh.get_bounding_box();extent=max(b.max.x-b.min.x,b.max.y-b.min.y,b.max.z-b.min.z)
        a.set_actor_scale3d(unreal.Vector(22/extent,22/extent,22/extent))
    shape('MiningMap_Frame',(45,-343,233),(155,10,104),mats['wood'],folder='Supplies')
    shape('MiningMap_Paper',(45,-336,233),(139,2,88),mats['paper'],collision=False,folder='Supplies')
    for start,end in [((-12,-333,207),(13,-333,225)),((13,-333,225),(60,-333,225)),((60,-333,225),(99,-333,259)),
                      ((13,-333,225),(13,-333,264)),((60,-333,225),(60,-333,201))]:bar('Map_TunnelRoute',start,end,2,mats['ink'])
    text('ShiftPlate','SHIFT 07',(-442,23,234),height=19,yaw=0)
    cup=actors.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(-379,-91,72))
    cup.set_actor_label('Bedside_CoffeeCup');cup.set_folder_path('StartRoom/Bed')
    cup.static_mesh_component.set_static_mesh(load('/Game/PopupClose/OfficeArt/Meshes/SM_CoffeeCup'))
    cup.static_mesh_component.set_collision_profile_name('NoCollision')
    b=cup.static_mesh_component.static_mesh.get_bounding_box();s=14/max(b.max.x-b.min.x,b.max.y-b.min.y,b.max.z-b.min.z)
    cup.set_actor_scale3d(unreal.Vector(s,s,s))
    cup.set_actor_location(unreal.Vector(-379,-91,70-b.min.z*s),False,True)

    # Equipment is on the floor next to the sole exit. Visuals attach to the reusable pickup root.
    pick=actors.spawn_actor_from_class(pickup_cls,unreal.Vector(326,-87,22))
    pick.set_actor_label('PICKUP_Pickaxe_E');pick.set_folder_path('StartRoom/Equipment')
    pick.interaction_box.set_box_extent(unreal.Vector(65,38,22),False)
    model=actors.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(326,-87,11))
    model.set_actor_label('Pickaxe_ExistingModel');model.set_folder_path('StartRoom/Equipment')
    model.static_mesh_component.set_static_mesh(load('/Game/Pickaxe/SM_Pickaxe'))
    model.static_mesh_component.set_editor_property('mobility',unreal.ComponentMobility.MOVABLE)
    model.static_mesh_component.set_collision_profile_name('NoCollision')
    model.set_actor_rotation(unreal.Rotator(pitch=0,yaw=25,roll=90),False)
    model.set_actor_scale3d(unreal.Vector(.85,.85,.85))
    origin,extent=model.get_actor_bounds(False)
    model.set_actor_location(model.get_actor_location()+unreal.Vector(0,0,7-(origin.z-extent.z)),False,True)
    assert model.attach_to_actor(pick,'',unreal.AttachmentRule.KEEP_WORLD,unreal.AttachmentRule.KEEP_WORLD,unreal.AttachmentRule.KEEP_WORLD,False)
    jet=actors.spawn_actor_from_class(pickup_cls,unreal.Vector(333,87,26))
    jet.set_actor_label('PICKUP_Jetpack_E');jet.set_folder_path('StartRoom/Equipment')
    jet.set_editor_property('equipment',unreal.MiningStarterEquipment.JETPACK)
    jet.interaction_box.set_box_extent(unreal.Vector(47,43,28),False)
    # No independent jetpack mesh exists in the project: editable assembly of existing primitive meshes.
    for label,pos,size,mat,mesh in [
        ('Jetpack_Backplate',(333,87,13),(75,69,10),mats['metal'],'Cube'),
        ('Jetpack_Harness',(339,87,23),(35,39,16),mats['leather'],'Cube'),
        ('Jetpack_LeftTank',(329,61,28),(25,25,64),mats['brass'],'Cylinder'),
        ('Jetpack_RightTank',(329,113,28),(25,25,64),mats['brass'],'Cylinder'),
        ('Jetpack_LeftNozzle',(366,61,23),(16,16,16),mats['metal'],'Cylinder'),
        ('Jetpack_RightNozzle',(366,113,23),(16,16,16),mats['metal'],'Cylinder'),
        ('Jetpack_Control',(324,87,34),(18,24,9),mats['teal'],'Cube'),
        ('Jetpack_Lamp',(325,87,40),(5,12,2),mats['light'],'Cube')]:
        rot=unreal.Rotator(pitch=90) if mesh=='Cylinder' else None
        part=shape(label,pos,size,mat,mesh,rot,False,'Equipment')
        part.static_mesh_component.set_editor_property('mobility',unreal.ComponentMobility.MOVABLE)
        assert part.attach_to_actor(jet,'',unreal.AttachmentRule.KEEP_WORLD,unreal.AttachmentRule.KEEP_WORLD,unreal.AttachmentRule.KEEP_WORLD,False)
    director=actors.spawn_actor_from_class(director_cls,unreal.Vector())
    director.set_actor_label('STARTROOM_EquipmentUnlockDirector');director.set_folder_path('StartRoom/Gameplay')
    start=actors.spawn_actor_from_class(unreal.PlayerStart,unreal.Vector(-405,55,102),unreal.Rotator(yaw=-30))
    start.set_actor_label('PLAYER_START_MinerBedroom');start.set_folder_path('StartRoom/Gameplay')

    # Warm practical fixtures, with cool spill visible beyond the sealed doorway.
    for label,pos in [('Bedside',(-378,-91,107)),('Ceiling',(-45,0,286)),('Exit',(401,0,236))]:
        shape(label+'_LampBase',(pos[0],pos[1],pos[2]-11),(22,22,5),mats['metal'],'Cylinder',collision=False,folder='Lighting')
        shape(label+'_LampGlass',pos,(15,15,20),mats['light'],'Cylinder',collision=False,folder='Lighting')
        shape(label+'_LampCap',(pos[0],pos[1],pos[2]+12),(24,24,6),mats['metal'],'Cylinder',collision=False,folder='Lighting')
        light(label+'_WarmPractical',pos,6. if label=='Ceiling' else 3.5,680 if label=='Ceiling' else 400,(1,.69,.38))
    light('Room_SoftFill',(-180,140,205),1.5,700,(.48,.60,.72))
    light('Tunnel_ColdSpill',(810,0,210),3.,600,(.35,.61,.78))
    pp=actors.spawn_actor_from_class(unreal.PostProcessVolume,unreal.Vector())
    pp.set_actor_label('StartRoom_Exposure');pp.set_folder_path('StartRoom/Lighting');pp.set_editor_property('unbound',True)
    p=pp.get_editor_property('settings')
    for name,value in [('override_auto_exposure_min_brightness',True),('override_auto_exposure_max_brightness',True),
                       ('auto_exposure_min_brightness',0.),('auto_exposure_max_brightness',0.),
                       ('override_auto_exposure_bias',True),('auto_exposure_bias',0.),
                       ('override_motion_blur_amount',True),('motion_blur_amount',0.)]:p.set_editor_property(name,value)
    pp.set_editor_property('settings',p)
    unreal.EditorLevelLibrary.set_level_viewport_camera_info(unreal.Vector(-420,290,215),unreal.Rotator(pitch=-13,yaw=-39,roll=0))
    assert levels.save_current_level()
    assert assetlib.save_directory(ROOT,only_if_is_dirty=True,recursive=True)
    report.update(passed=True,actors=len(actors.get_all_level_actors()),pickaxe_model='/Game/Pickaxe/SM_Pickaxe',
                  jetpack_visual='Editable assembly of existing engine meshes and project materials',
                  reused=['SharedMiningCharacter','MiningComponent','MiningCharacterMovementComponent','MiningInteractionHUDWidget','DiggableRock'],
                  bedroom_cm=[900,720,330],exit_cm=[240,250],blocker_depth_cm=110)
except Exception:
    report['error']=traceback.format_exc()
    raise
finally:
    (out/'creation.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    unreal.log('STARTROOM_CREATION '+json.dumps(report,ensure_ascii=False))
