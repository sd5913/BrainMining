import unreal

MAP = '/Game/Showcase/Maps/Lvl_ContinuousShowcase'
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
levels.load_level(MAP)
existing = actors.get_all_level_actors()
director = next(actor for actor in existing if isinstance(actor, unreal.MiningShowcaseDirector))
if any(isinstance(actor, unreal.MiningTreasureField) for actor in existing):
    raise RuntimeError('Treasure annex already exists; refusing to overwrite authored settings.')
cube = unreal.load_asset('/Engine/BasicShapes/Cube')
floor_mat = unreal.load_asset('/Game/Showcase/Materials/M_ContinuousFloor')
edge_mat = unreal.load_asset('/Game/Showcase/Materials/M_ContinuousEdge')
wall_mat = unreal.load_asset('/Game/Showcase/Materials/M_ContinuousWall')

def box(name, location, size, material):
    actor = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(*location))
    actor.set_actor_label(name)
    actor.set_folder_path('Continuous/Treasure')
    component = actor.static_mesh_component
    component.set_static_mesh(cube)
    component.set_material(0, material)
    actor.set_actor_scale3d(unreal.Vector(*(value / 100 for value in size)))
    return actor

def sign(name, content, location, yaw=90, size=30):
    actor = actors.spawn_actor_from_class(unreal.TextRenderActor, unreal.Vector(*location), unreal.Rotator(yaw=yaw))
    actor.set_actor_label(name)
    actor.set_folder_path('Continuous/Treasure')
    component = actor.text_render
    component.set_text(content)
    component.set_world_size(size)
    component.set_text_render_color(unreal.Color(245, 204, 90, 255))

south_rails = [actor for actor in existing if actor.get_actor_label().startswith('Mining_Border') and abs(actor.get_actor_location().y + 5000) < 10]
if len(south_rails) != 1:
    raise RuntimeError('Expected exactly one mining south rail; no map changes saved.')
actors.destroy_actor(south_rails[0])
box('Mining_SouthRail_Left', (-1250, -5000, 70), (500, 20, 140), wall_mat)
box('Mining_SouthRail_Right', (950, -5000, 70), (2700, 20, 140), wall_mat)
box('Treasure_ConnectingWalk', (-700, -5200, -25), (600, 600, 50), floor_mat)
box('Treasure_WayfindingStripe', (-700, -5200, 2), (16, 600, 3), edge_mat)
box('Treasure_Court', (400, -6300, -25), (3800, 2200, 50), floor_mat)
box('Treasure_BackRail', (400, -7400, 70), (3800, 20, 140), wall_mat)
for side in (-1, 1):
    box('Treasure_SideRail', (400 + side * 1900, -6300, 70), (20, 2200, 140), wall_mat)
field = actors.spawn_actor_from_class(unreal.MiningTreasureField, unreal.Vector(400, -6300, 0))
field.set_actor_label('07_Treasure_Config_EditDefinitions')
field.set_folder_path('Continuous/Treasure')
sign('Treasure_Entry', '07 / TREASURE MINING\nEXPOSE >50%  /  E TO COLLECT', (-1000, -5400, 300), size=32)
sign('Treasure_Normal', 'NORMAL ROCK / WEIGHTED TREASURES', (-1100, -5740, 310), size=25)
sign('Treasure_Guide', 'GUIDE ROCK / WEIGHTED TREASURES', (-1100, -6590, 310), size=25)
sign('Treasure_MiningSign', 'TREASURE ANNEX  >>>\nWALK SOUTH', (-1100, -4720, 250), yaw=180, size=25)
for ordinate in (-5700, -6800):
    light = actors.spawn_actor_from_class(unreal.PointLight, unreal.Vector(400, ordinate, 700))
    light.set_actor_label('Treasure_LocalLight')
    light.set_folder_path('Continuous/Treasure')
    component = light.get_component_by_class(unreal.PointLightComponent)
    component.set_mobility(unreal.ComponentMobility.MOVABLE)
    component.set_editor_property('intensity', 11000)
    component.set_editor_property('attenuation_radius', 2300)
    component.set_editor_property('cast_shadows', False)
entries = list(director.get_editor_property('exhibits'))
entry = unreal.MiningShowcaseExhibit()
entry.set_editor_property('id', 'TreasureShowcase')
entry.set_editor_property('title', '07 / 宝物挖掘与金币')
entry.set_editor_property('kind', unreal.MiningShowcaseKind.TREASURE)
entry.set_editor_property('center', unreal.Vector(400, -6300, 350))
entry.set_editor_property('extent', unreal.Vector(1900, 1290, 900))
entry.set_editor_property('instructions', '普通岩层与紫色引导岩层各埋藏一排随机宝物。\n左键挖掘，暴露超过 50% 后瞄准按 E 领取金币。\n沿北侧支路回到挖掘区；金币跨区保留。')
entries.append(entry)
director.set_editor_property('exhibits', entries)
director.set_actor_label('CONTINUOUS_SevenRegions_NoStreaming')
for actor in existing:
    if actor.get_actor_label() == 'Arrival_Title':
        actor.text_render.set_text('BRAIN MINING\nONE WORLD / SEVEN EXPERIENCES')
if not levels.save_current_level():
    raise RuntimeError('Failed to save treasure annex')
unreal.log('TREASURE_SHOWCASE_SAVED')
