import json
import os
import unreal

MAP = '/Game/Showcase/Maps/Lvl_MainExperiences'
assets = unreal.EditorAssetLibrary
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
if assets.does_asset_exist(MAP):
    raise RuntimeError('Main experiences already exists; refusing to overwrite.')
if not levels.new_level(MAP):
    raise RuntimeError('Could not create main experiences level')
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
world.get_world_settings().set_editor_property('default_game_mode', unreal.MiningExhibitGameMode.static_class())
specs = [
    ('矿道与矿区 · Shen-Yilang', '/Game/ExternalMining/Maps/Lvl_ExternalMining_Demo', (-250000, 0, 0), 'BM_Mining', '左键挖掘，空格飞行，V 钻头；矿道与矿区使用 Shen-Yilang 版本。'),
    ('藏品、矿物宝物与金币', '/Game/CollectionPrototype/Maps/Lvl_Collection_V03', (-250000, -100000, 0), 'BM_Collection', '沿环境线索找藏品；矿物暴露后按 E 收集金币。I 图鉴，Tab 升级。'),
    ('回声 → 巨人卡牌 → 电视逃生', '/Game/Passages/Echo/Maps/Lvl_EchoMazeToCards_P1', (0, 0, 0), 'BM_Journey', '跟随回声，进入巨人卡牌房间，再完成洪水与电视逃生。'),
    ('四设备办公室', '/Game/PopupClose/Maps/Lvl_PopupClose', (0, -100000, 0), 'BM_Office', '靠近设备按 E；处理电脑弹窗与打印机工作。'),
    ('文思博物馆', '/Game/WenSi/Maps/Lvl_WenSi_ScreenBridge', (250000, -100000, 0), 'BM_Museum', '使用触摸屏、空间拖拽和电路桥，完成新版博物馆谜题。'),
    ('细胞宇宙', '/Game/CellUniverse/Maps/Lvl_CellUniverse_Demo', (250000, 0, 0), 'BM_Ending', '挖掘并穿越细胞宇宙的阶段关卡。'),
]
groups = []
entries = []
manifest = []
for title, source, offset, tag, hint in specs:
    copied = list(unreal.MiningDesignerLibrary.copy_experience_actors(source, unreal.Vector(*offset), tag))
    if not copied:
        raise RuntimeError('Empty import: ' + source)
    starts = [actor for actor in copied if isinstance(actor, unreal.PlayerStart)]
    offices = [actor for actor in copied if isinstance(actor, unreal.PopupCloseStage)]
    if not starts and not offices:
        raise RuntimeError('No authored player start: ' + source)
    entry = starts[0].get_actor_transform() if starts else offices[0].get_player_spawn_transform()
    if len(groups) in (3, 4):
        location = entry.translation
        entry.translation = unreal.Vector(location.x, location.y, 110)
    entries.append(entry)
    for start in starts:
        actors.destroy_actor(start)
        copied.remove(start)
    groups.append(copied)
    area = unreal.MiningExperienceArea()
    for name, value in [('title', title), ('instructions', hint), ('tag', tag), ('origin', unreal.Vector(*offset)), ('entry', entry)]:
        area.set_editor_property(name, value)
    location = entry.translation
    manifest.append({'title': title, 'source': source, 'actors': len(copied), 'entry': [location.x, location.y, location.z]})
    unreal.log('MAIN_EXPERIENCE_IMPORTED ' + json.dumps(manifest[-1], ensure_ascii=False))
    if len(groups) == 1:
        areas = []
    areas.append(area)

def single(group, actor_class):
    found = [actor for actor in group if isinstance(actor, actor_class)]
    if len(found) != 1:
        raise RuntimeError('Expected one ' + str(actor_class) + ': ' + str(len(found)))
    return found[0]

collection = single(groups[1], unreal.MiningCollectionField)
collection.set_editor_property('area_managed', True)
catalogue_key = unreal.Key()
catalogue_key.set_editor_property('key_name', 'I')
collection.set_editor_property('catalogue_key', catalogue_key)
collection.set_editor_property('shared_abilities', True)
collection.set_editor_property('show_exploration_hud', False)
for actor in groups[1]:
    if actor.get_actor_label() == 'V02 entrance boundary':
        actors.destroy_actor(actor)
office = single(groups[3], unreal.PopupCloseStage)
office.set_editor_property('area_managed', True)
museum = single(groups[4], unreal.WenSiPuzzle)
museum.set_editor_property('area_managed', True)
museum.set_editor_property('area_player_start', entries[4])
ending = single(groups[5], unreal.MiningCellUniverse)
ending.set_editor_property('area_managed', True)
echo = single(groups[2], unreal.EchoPassageStage)
echo.set_editor_property('room_in_same_level', True)
echo.set_editor_property('destination_level', None)
escape_candidates = [actor for actor in groups[2] if isinstance(actor, unreal.MiningRoomEscape)]
escape = escape_candidates[0] if escape_candidates else actors.spawn_actor_from_class(unreal.MiningRoomEscape, unreal.Vector())
escape.set_editor_property('area_managed', True)
escape.set_editor_property('card_stage', single(groups[2], unreal.CardRoomStage))
escape.set_editor_property('entrance', single(groups[2], unreal.MiningGiantRoomEntrance))
escape.set_editor_property('tags', ['BM_Journey'])
escape.set_actor_label('Journey_LatestEscape_Television')

treasure = single(unreal.MiningDesignerLibrary.copy_experience_actors('/Game/Showcase/Maps/Lvl_ContinuousShowcase', unreal.Vector(), 'BM_Collection', unreal.MiningTreasureField), unreal.MiningTreasureField)
treasure.set_actor_location(unreal.Vector(-253300, -100000, 0), False, False)
treasure.set_editor_property('use_shared_wallet', True)

cube = unreal.load_asset('/Engine/BasicShapes/Cube')
floor_mat = unreal.load_asset('/Game/Showcase/Materials/M_ContinuousFloor')
edge_mat = unreal.load_asset('/Game/Showcase/Materials/M_ContinuousEdge')
def box(name, location, size, tag, material=floor_mat):
    actor = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(*location))
    actor.set_actor_label(name)
    actor.set_editor_property('tags', [tag])
    actor.static_mesh_component.set_static_mesh(cube)
    actor.static_mesh_component.set_material(0, material)
    actor.set_actor_scale3d(unreal.Vector(*(value / 100 for value in size)))
    return actor

def label(value, location, tag, size=55):
    actor = actors.spawn_actor_from_class(unreal.TextRenderActor, unreal.Vector(*location), unreal.Rotator(yaw=180))
    actor.set_editor_property('tags', [tag])
    actor.text_render.set_text(value)
    actor.text_render.set_world_size(size)
    actor.text_render.set_horizontal_alignment(unreal.HorizTextAligment.EHTA_CENTER)
    actor.text_render.set_text_render_color(unreal.Color(140, 245, 225, 255))

box('Collection_Treasure_ConnectedCourt', (-252750, -100000, -25), (5000, 3200, 50), 'BM_Collection')
label('MINERAL TREASURES / E TO COLLECT', (-252000, -100000, 430), 'BM_Collection', 40)
label('COLLECTION CLUES >>>', (-251350, -100000, 650), 'BM_Collection', 40)
for position in [(-253300, -100000, 800), (900, -150000, 850)]:
    lamp = actors.spawn_actor_from_class(unreal.PointLight, unreal.Vector(*position))
    lamp.set_editor_property('tags', ['BM_Collection' if position[0] < 0 else 'BM_Hub'])
    lamp.point_light_component.set_editor_property('intensity', 14000)
    lamp.point_light_component.set_editor_property('attenuation_radius', 3600)
    lamp.point_light_component.set_editor_property('cast_shadows', False)

lobby = unreal.Transform(location=unreal.Vector(0, -150000, 100))
actors.spawn_actor_from_class(unreal.PlayerStart, lobby.translation)
box('Experience_Lobby', (750, -150000, -25), (2800, 2800, 50), 'BM_Hub')
label('BRAIN MINING / SIX EXPERIENCES', (1800, -150000, 560), 'BM_Hub', 70)
names = ['01 MINING', '02 COLLECTION + TREASURE', '03 ECHO / CARDS / ESCAPE', '04 OFFICE', '05 MUSEUM', '06 CELL UNIVERSE']
for index, name in enumerate(names):
    position = (550 + (index // 3) * 650, -150000 + ((index % 3) - 1) * 650, 10)
    box('Portal_' + str(index), position, (450, 480, 20), 'BM_Hub', edge_mat)
    label(name + '\nE TO ENTER', (position[0] + 120, position[1], 220), 'BM_Hub', 29)
hall = actors.spawn_actor_from_class(unreal.MiningExhibitHall, lobby.translation)
hall.set_actor_label('MAIN_SixAreas_OnePersistentLevel')
hall.set_editor_property('areas', areas)
hall.set_editor_property('lobby_entry', lobby)
if not museum.get_editor_property('circuit_puzzle'):
    raise RuntimeError('Museum must contain latest circuit puzzle')
if len(ending.get_editor_property('gates')) != 11:
    raise RuntimeError('Ending must contain all eleven gates')
if any(gate is None for gate in ending.get_editor_property('gates')):
    raise RuntimeError('Ending gate references must resolve after copying')
if not levels.save_current_level():
    raise RuntimeError('Main experience map save failed')
report = os.path.join(unreal.Paths.project_saved_dir(), 'MainExperiencesManifest.json')
with open(report, 'w', encoding='utf-8') as output:
    json.dump({'map': MAP, 'areas': manifest}, output, ensure_ascii=False, indent=2)
unreal.log('MAIN_EXPERIENCES_SAVED ' + MAP)
