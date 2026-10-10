import json
import math
import os
import unreal

MAP = '/Game/Showcase/Maps/Lvl_ContinuousShowcase'
library = unreal.EditorAssetLibrary
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
tools = unreal.AssetToolsHelpers.get_asset_tools()
if library.does_asset_exist(MAP):
    raise RuntimeError('Continuous map already exists; refusing to overwrite authored work.')
if not levels.new_level(MAP):
    raise RuntimeError('Could not create continuous map')
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
world.get_world_settings().set_editor_property('default_game_mode', unreal.MiningShowcaseGameMode.static_class())

def material(name, color):
    path = '/Game/Showcase/Materials/' + name
    if library.does_asset_exist(path):
        return unreal.load_asset(path)
    asset = tools.create_asset(name, '/Game/Showcase/Materials', unreal.Material, unreal.MaterialFactoryNew())
    asset.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
    node = unreal.MaterialEditingLibrary.create_material_expression(asset, unreal.MaterialExpressionConstant3Vector)
    node.set_editor_property('constant', unreal.LinearColor(*color, 1))
    unreal.MaterialEditingLibrary.connect_material_property(node, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    unreal.MaterialEditingLibrary.recompile_material(asset)
    library.save_loaded_asset(asset)
    return asset

floor_mat = material('M_ContinuousFloor', (.055, .095, .13))
edge_mat = material('M_ContinuousEdge', (.1, .62, .57))
wall_mat = material('M_ContinuousWall', (.08, .12, .16))
cube = unreal.load_asset('/Engine/BasicShapes/Cube')

def box(name, position, size, mat=floor_mat, pitch=0, collision=True):
    actor = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(*position), unreal.Rotator(pitch=pitch, yaw=0, roll=0))
    actor.set_actor_label(name)
    actor.set_folder_path('Continuous/Walkways')
    actor.static_mesh_component.set_static_mesh(cube)
    actor.static_mesh_component.set_material(0, mat)
    actor.set_actor_scale3d(unreal.Vector(*(value / 100 for value in size)))
    if not collision:
        actor.static_mesh_component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    return actor

def text(name, value, position, yaw=180, size=40):
    actor = actors.spawn_actor_from_class(unreal.TextRenderActor, unreal.Vector(*position), unreal.Rotator(pitch=0, yaw=yaw, roll=0))
    actor.set_actor_label(name)
    actor.set_folder_path('Continuous/Wayfinding')
    component = actor.get_component_by_class(unreal.TextRenderComponent)
    component.set_text(value)
    component.set_world_size(size)
    component.set_horizontal_alignment(unreal.HorizTextAligment.EHTA_CENTER)
    component.set_text_render_color(unreal.Color(r=130, g=240, b=222, a=255))

def walk(name, start, finish, width=650, rails=True):
    horizontal = start[1] == finish[1]
    length = abs(finish[0] - start[0]) if horizontal else abs(finish[1] - start[1])
    center = ((start[0] + finish[0]) / 2, (start[1] + finish[1]) / 2, -20)
    size = (length, width, 40) if horizontal else (width, length, 40)
    box(name, center, size)
    for side in (-1, 1):
        edge = (center[0], center[1] + side * (width / 2 - 8), 2) if horizontal else (center[0] + side * (width / 2 - 8), center[1], 2)
        box(name + '_stripe', edge, (length, 8, 4) if horizontal else (8, length, 4), edge_mat, collision=False)
        if rails:
            box(name + '_rail', (edge[0], edge[1], 55), (length, 12, 110) if horizontal else (12, length, 110), wall_mat)

def doorway(name, origin, local_x, total_width, opening=380, height=450):
    side_width = (total_width - opening) / 2
    for side in (-1, 1):
        box(name, (origin[0] + local_x, origin[1] + side * (opening / 2 + side_width / 2), height / 2), (24, side_width, height), wall_mat)
    box(name + '_header', (origin[0] + local_x, origin[1], height - 50), (24, opening, 100), wall_mat)

def imported(path, offset, excluded):
    result = unreal.MiningDesignerLibrary.import_showcase_room(path, unreal.Vector(*offset), excluded)
    if not result:
        raise RuntimeError('Import failed: ' + path)
    return result

collection = imported('/Game/CollectionPrototype/Maps/Lvl_Collection_V03', (0, 0, 0), ['V02 entrance boundary'])
field = next(actor for actor in collection if isinstance(actor, unreal.MiningCollectionField))
field.set_editor_property('area_managed', True)
doorway('Collection_Entry', (0, 0, 0), -1290, 3100, 650, 1200)

cards_origin = (0, 9300, 0)
cards = imported('/Game/Rooms/CardDuel/Maps/Lvl_CardRoom_P1', cards_origin, ['Entry wall'])
doorway('Cards_Entry', cards_origin, -690, 940, 380, 470)

wensi_origin = (0, 6500, 0)
wensi = imported('/Game/WenSi/Maps/Lvl_WenSi', wensi_origin, ['BackWall', 'ExitEndWall'])
puzzle = next(actor for actor in wensi if isinstance(actor, unreal.WenSiPuzzle))
puzzle.set_editor_property('area_managed', True)
for actor in wensi:
    if type(actor) == unreal.Actor:
        for component in actor.get_components_by_class(unreal.StaticMeshComponent):
            component.set_material(0, floor_mat if actor.get_actor_label() == 'Floor' else unreal.load_asset('/Game/Rooms/CardDuel/Materials/M_Cream'))
doorway('WenSi_Entry', wensi_origin, -550, 840, 380, 500)
doorway('WenSi_Exit', wensi_origin, 700, 840, 380, 500)

popup = actors.spawn_actor_from_class(unreal.PopupCloseStage, unreal.Vector(0, 3500, 0))
popup.set_editor_property('area_managed', True)
popup.set_actor_label('05_Popup_Continuous')
popup.set_folder_path('Continuous/Gameplay')
echo = actors.spawn_actor_from_class(unreal.EchoPassageStage, unreal.Vector(5000, -3500, 0))
echo.set_editor_property('use_maze', True)
echo.set_editor_property('walk_through_connections', True)
echo.set_actor_label('03_Echo_Continuous')
echo.set_folder_path('Continuous/Gameplay')

rock = actors.spawn_actor_from_class(unreal.DiggableRock, unreal.Vector(650, -4000, 300))
rock.set_actor_label('01_Mining_FogRock')
rock.set_folder_path('Continuous/Gameplay')
rock.set_editor_property('rock_size', unreal.Vector(1000, 1000, 600))
rock.set_editor_property('rock_material', unreal.load_asset('/Game/CollectionPrototype/Materials/M_CollectionEarth'))
chamber = rock.get_component_by_class(unreal.FogChamberComponent)
chamber.set_editor_property('enabled', True)
chamber.set_editor_property('room_size', unreal.Vector(400, 480, 380))
chamber.set_editor_property('room_offset', unreal.Vector(0, 0, -90))
chamber.set_editor_property('fog_material', unreal.load_asset('/Game/MiningPrototype/Materials/M_ChamberMist'))
rock.reset_rock()
box('Mining_Court', (400, -4000, -25), (3800, 2000, 50))
for side in (-1, 1):
    box('Mining_Border', (400, -4000 + side * 1000, 70), (3800, 20, 140), wall_mat)

walk('Main_Promenade', (-2200, -4000), (-2200, 9800), 850, False)
box('Promenade_WestRail', (-2620, 2900, 65), (15, 13800, 130), wall_mat)
for name, ordinate, destination in [('Mining', -4000, -1400), ('Collection', 0, -1180), ('Popup', 3500, -400), ('WenSi', 6500, -450), ('Cards', 9300, -600)]:
    walk(name + '_Approach', (-2200, ordinate), (destination, ordinate), 600, False)
    text(name + '_Wayfinding', name.upper() + '  >>>', (-1630, ordinate + 340, 230), 180, 36)
for lower, upper in [(-3650, -2600), (-1700, -350), (350, 1450), (2150, 3150), (3850, 6150), (6850, 7250), (7950, 8950), (9650, 9800)]:
    box('Promenade_EastRail', (-1780, (lower + upper) / 2, 65), (15, upper - lower, 130), wall_mat)
walk('WenSi_ExitWalk', (650, 6500), (1300, 6500), 600, False)
walk('WenSi_ReturnNorth', (1300, 6500), (1300, 7600), 600, False)
walk('WenSi_ReturnWest', (1300, 7600), (-2200, 7600), 600, False)

slope = math.degrees(math.atan2(900, 2600))
box('Echo_EntryRamp', (2900, -3500, 430), (math.hypot(2600, 900), 390, 40), floor_mat, pitch=slope)
for side in (-1, 1):
    box('Echo_RampRail', (2900, -3500 + side * 200, 490), (math.hypot(2600, 900), 16, 140), edge_mat, pitch=slope)
box('Echo_UpperLanding', (4230, -3500, 880), (100, 400, 40))
text('Echo_RampSign', '03 / ECHO MAZE\nWALK UP THE RAMP', (1590, -3200, 230), 180, 32)
walk('Echo_ExitBridge', (11950, 100), (13000, 100), 620, False)
walk('Echo_ExitNorth', (13000, 100), (13000, 1800), 620, False)
walk('Echo_ReturnPromenade', (13000, 1800), (-2200, 1800), 620, False)
text('Echo_ExitSign', 'MAZE EXIT  /  PROMENADE >>>', (12600, 100, 240), 180, 30)
text('Arrival_Title', 'BRAIN MINING\nONE WORLD / SIX EXPERIENCES', (-1100, -2150, 350), 180, 55)
text('Arrival_Directions', 'SOUTH: MINING / ECHO\nNORTH: COLLECTION / POPUP / WENSI / CARDS', (-1100, -2150, 180), 180, 25)
walk('Welcome_Overlook', (-2200, -2150), (-1100, -2150), 900, False)

for name, center, radius, intensity in [('MiningLight', (0, -4000, 600), 2200, 5000), ('CollectionLight', (-500, 0, 650), 2300, 8000), ('WelcomeLight', (-1900, -2100, 800), 2200, 1000)]:
    light = actors.spawn_actor_from_class(unreal.PointLight, unreal.Vector(*center))
    light.set_actor_label(name)
    component = light.get_component_by_class(unreal.PointLightComponent)
    component.set_mobility(unreal.ComponentMobility.MOVABLE)
    component.set_editor_property('intensity', intensity)
    component.set_editor_property('attenuation_radius', radius)
    component.set_editor_property('cast_shadows', False)
exposure = actors.spawn_actor_from_class(unreal.PostProcessVolume, unreal.Vector())
exposure.set_actor_label('Continuous_Exposure')
exposure.set_editor_property('unbound', True)
settings = exposure.get_editor_property('settings')
settings.set_editor_property('override_auto_exposure_method', True)
settings.set_editor_property('auto_exposure_method', unreal.AutoExposureMethod.AEM_MANUAL)
settings.set_editor_property('override_auto_exposure_apply_physical_camera_exposure', True)
settings.set_editor_property('auto_exposure_apply_physical_camera_exposure', False)
settings.set_editor_property('override_auto_exposure_bias', True)
settings.set_editor_property('auto_exposure_bias', 0)
exposure.set_editor_property('settings', settings)

regions = [
    ('MiningLab', '01 / 挖掘与雾室', 'MINING', (400, -4000, 400), (1850, 1000, 900), '左键挖掘岩体，打通封闭雾室。F5 仅重置本区。\n右侧坡道通向回声迷宫；原路返回步道。'),
    ('Collection', '02 / 收集与三维图鉴', 'COLLECTION', (950, 0, 550), (2250, 1580, 900), '沿四条线索挖掘，E 收集，Tab 打开图鉴；空格飞行。\n离开后藏品和挖掘进度保留。'),
    ('Echo', '03 / 回声迷宫', 'ECHO', (8150, -2000, 550), (4000, 2950, 900), '从坡道步行进入，地板坍塌后左键敲击辨路。\n出口接回步道，不传送至卡牌房。R/F5 重回迷宫入口。'),
    ('Cards', '04 / 卡牌房', 'CARDS', (-60, 9300, 230), (750, 480, 700), '走进房间，靠近座位按 E 入座，点击选牌，空格确认。\nQ/F8 离座后从原门步行离开。'),
    ('Popup', '05 / 弹窗电脑', 'POPUP', (0, 3500, 160), (600, 380, 650), '走近电脑，瞄准屏幕按 E；点击关闭弹窗，观察实体喷发。\nF8 退出桌面，原路返回步道。'),
    ('WenSi', '06 / 文思解谜', 'WEN_SI', (100, 6500, 250), (750, 420, 650), 'E 思考两个物体，G 编辑并交换属性词，Q/R 取消思考。\n解开障碍走到出口，沿外侧通道回到步道。'),
]
director = actors.spawn_actor_from_class(unreal.MiningShowcaseDirector, unreal.Vector(-2200, -2100, 0))
director.set_actor_label('CONTINUOUS_SixRegions_NoStreaming')
director.set_editor_property('continuous_layout', True)
entries = []
for identity, title, kind, center, extent, instructions in regions:
    entry = unreal.MiningShowcaseExhibit()
    entry.set_editor_property('id', identity)
    entry.set_editor_property('title', title)
    entry.set_editor_property('kind', getattr(unreal.MiningShowcaseKind, kind))
    entry.set_editor_property('center', unreal.Vector(*center))
    entry.set_editor_property('extent', unreal.Vector(*extent))
    entry.set_editor_property('instructions', instructions)
    entries.append(entry)
director.set_editor_property('exhibits', entries)
actors.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(-2200, -2150, 100), unreal.Rotator(pitch=0, yaw=0, roll=0))
unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).set_level_viewport_camera_info(unreal.Vector(-8500, -9000, 13500), unreal.Rotator(pitch=-43, yaw=48, roll=0))
if not levels.save_current_level():
    raise RuntimeError('Could not save continuous map')
os.makedirs(os.path.join(unreal.Paths.project_saved_dir(), 'Validation'), exist_ok=True)
with open(os.path.join(unreal.Paths.project_saved_dir(), 'Validation', 'continuous_creation.json'), 'w', encoding='utf-8') as output:
    json.dump({'map': MAP, 'regions': [region[0] for region in regions], 'source_maps_modified': False, 'streamed_levels': 0}, output, indent=2)
unreal.log('CONTINUOUS_SHOWCASE_CREATED ' + MAP)
