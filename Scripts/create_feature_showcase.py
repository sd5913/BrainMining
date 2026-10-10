import json
import os
import unreal


MAP = "/Game/Showcase/Maps/Lvl_FeatureShowcase"
ROOT = "/Game/Showcase"
ORIGIN = unreal.Vector(-60000, 0, 0)
EXHIBITS = [
    ("MiningLab", "挖掘 / 飞行 / 雾室", "/Game/MiningPrototype/Maps/Lvl_MiningLab", "MINING", "左键挖掘，空格跳跃；本图保留原设置。挖通岩体观察雾气，R 重置。", "MINING + MIST", (0.2, 0.9, 0.75)),
    ("AutoRooms", "房间推进 / 分块矿道", "/Game/MiningPrototype/Maps/Lvl_MiningDescent_AutoRooms", "MINING", "挖向下一主房间；按住空格飞行，松开下落。M 地图，R 重置。", "ROOM PROGRESSION", (0.2, 0.9, 0.75)),
    ("HollowBranches", "空洞 / 隐藏入口 / 支路", "/Game/MiningPrototype/Maps/Lvl_MiningHollowBranches_Demo", "MINING", "探索开放与隐藏洞室，挖开堵块和入口；保留原飞行与地图工具。", "CAVES + BRANCHES", (0.2, 0.9, 0.75)),
    ("HollowChain", "多矿区串联", "/Game/MiningPrototype/Maps/Lvl_MiningHollowChain_Demo", "MINING", "体验多房间、多矿区与侧洞室；旧线性模式保留冻结和回收。", "HOLLOW CHAIN", (0.4, 0.65, 1.0)),
    ("ManualMine", "二维钢笔矿道样例", "/Game/MiningPrototype/Maps/Lvl_MiningManual_Demo", "MINING", "体验手绘通道、凹形空洞与支线。修改造型需退出 Play 使用矿道钢笔编辑器。", "PEN TOOL SAMPLE", (0.4, 0.65, 1.0)),
    ("SpatialMine", "三维隧道 / 连续岩体", "/Game/MiningPrototype/Maps/Lvl_MiningSpatial_Demo", "MINING", "空格飞行，左键挖掘；空间之间可自行挖通，外围基岩不可挖。M 三维地图。", "3D SPATIAL MINE", (0.4, 0.65, 1.0)),
    ("Collection", "挖掘收集 / 三维图鉴", "/Game/CollectionPrototype/Maps/Lvl_Collection_V03", "COLLECTION", "沿四种线索挖掘，E 收集，Tab 图鉴；拖动旋转、滚轮缩放，R 新一轮。", "COLLECTION GALLERY", (1.0, 0.72, 0.25)),
    ("Echo", "盲人敲地 / 回声迷宫", "/Game/Passages/Echo/Maps/Lvl_EchoMazeToCards_P1", "ECHO", "向前走触发坍塌；落地后单击左键敲地，R 重来。出口进入卡牌展区。", "ECHO MAZE", (1.0, 0.72, 0.25)),
    ("Cards", "卡牌房 / 四轮演出", "/Game/Rooms/CardDuel/Maps/Lvl_CardRoom_P1", "CARDS", "靠近牌桌 E 入座，点击选牌，空格出牌，Q 离席。奖励仅为本地演示。", "CARD ENCOUNTER", (1.0, 0.72, 0.25)),
    ("Popup", "电脑弹窗 / 窗口喷发", "/Game/PopupClose/Maps/Lvl_PopupClose", "POPUP", "瞄准电脑按 E，点击弹窗右上角关闭，等待窗口喷发；F5 重开。", "POPUP OVERLOAD", (1.0, 0.4, 0.57)),
    ("WenSi", "文思 / 词语交换解谜", "/Game/WenSi/Maps/Lvl_WenSi", "WEN_SI", "E 思考，G 重组并拖动红词交换；Q/R 放下左右思考，改变物性打开出口。", "WORD RECOMBINATION", (1.0, 0.4, 0.57)),
]


def main():
    library = unreal.EditorAssetLibrary
    if library.does_asset_exist(MAP):
        raise RuntimeError("Showcase already exists; refusing to overwrite an edited level")
    for exhibit in EXHIBITS:
        if not library.does_asset_exist(exhibit[2]):
            raise RuntimeError("Missing source map: " + exhibit[2])
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    library.make_directory(ROOT + "/Maps")
    library.make_directory(ROOT + "/Materials")
    library.make_directory(ROOT + "/Subtitles")
    if not levels.new_level(MAP):
        raise RuntimeError("Could not create showcase level")
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    world.get_world_settings().set_editor_property("default_game_mode", unreal.MiningShowcaseGameMode.static_class())
    director = actors.spawn_actor_from_class(unreal.MiningShowcaseDirector, ORIGIN)
    director.set_actor_label("SHOWCASE_Director_11_Exhibits")
    director.set_folder_path("Showcase/Control")
    cube = unreal.load_asset("/Engine/BasicShapes/Cube")

    def material(name, color, emissive=False):
        path = ROOT + "/Materials/" + name
        asset = unreal.load_asset(path) if library.does_asset_exist(path) else tools.create_asset(name, ROOT + "/Materials", unreal.Material, unreal.MaterialFactoryNew())
        unreal.MaterialEditingLibrary.delete_all_material_expressions(asset)
        asset.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
        node = unreal.MaterialEditingLibrary.create_material_expression(asset, unreal.MaterialExpressionConstant3Vector)
        node.set_editor_property("constant", unreal.LinearColor(*color, 1))
        unreal.MaterialEditingLibrary.connect_material_property(node, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
        unreal.MaterialEditingLibrary.recompile_material(asset)
        library.save_loaded_asset(asset)
        return asset

    floor_material = material("M_ShowcaseFloor", (0.025, 0.045, 0.07))
    dark_material = material("M_ShowcasePlinth", (0.055, 0.085, 0.115))

    def box(label, position, size, surface):
        actor = actors.spawn_actor_from_class(unreal.StaticMeshActor, ORIGIN + unreal.Vector(*position))
        actor.set_actor_label(label)
        actor.set_folder_path("Showcase/Architecture")
        component = actor.static_mesh_component
        component.set_static_mesh(cube)
        component.set_material(0, surface)
        actor.set_actor_scale3d(unreal.Vector(*[value / 100 for value in size]))
        return actor

    def text(label, value, position, size, color):
        actor = actors.spawn_actor_from_class(unreal.TextRenderActor, ORIGIN + unreal.Vector(*position), unreal.Rotator(pitch=0, yaw=180, roll=0))
        actor.set_actor_label(label)
        actor.set_folder_path("Showcase/Signs")
        component = actor.get_component_by_class(unreal.TextRenderComponent)
        component.set_text(value)
        component.set_world_size(size)
        component.set_horizontal_alignment(unreal.HorizTextAligment.EHTA_CENTER)
        component.set_text_render_color(unreal.Color(*[int(channel * 255) for channel in color], 255))

    box("Gallery_Floor", (800, 0, -25), (6600, 4000, 50), floor_material)
    box("Gallery_Ceiling", (800, 0, 1000), (6600, 4000, 40), dark_material)
    for side in [-1, 1]:
        box("Gallery_SideWall", (800, side * 2000, 490), (6600, 30, 1000), dark_material)
    box("Gallery_BackWall", (4100, 0, 490), (40, 4000, 1000), dark_material)
    text("Gallery_Title", "BRAIN MINING / FEATURE GALLERY", (-900, 0, 700), 80, (0.3, 0.95, 0.82))
    text("Gallery_Controls", "WASD / E ENTER    F2 DIRECTORY    F5 RESTART    F8 LOBBY", (-895, 0, 560), 30, (0.75, 0.85, 0.9))
    entries = []
    for index, (identity, title, path, kind, instructions, english, color) in enumerate(EXHIBITS):
        entry = unreal.MiningShowcaseExhibit()
        entry.set_editor_property("id", identity)
        entry.set_editor_property("title", title)
        entry.set_editor_property("instructions", instructions)
        entry.set_editor_property("level", unreal.load_asset(path))
        entry.set_editor_property("kind", getattr(unreal.MiningShowcaseKind, kind))
        entry.set_editor_property("color", unreal.LinearColor(*color, 1))
        entry.set_editor_property("subtitle_id", "Showcase_" + identity)
        entries.append(entry)
        asset_name = "DA_Showcase_" + identity
        segment = tools.create_asset(asset_name, ROOT + "/Subtitles", unreal.SubtitleSegment, unreal.SubtitleSegmentFactory())
        if not segment:
            segment = unreal.load_asset(ROOT + "/Subtitles/" + asset_name)
        line = unreal.SubtitleLine()
        line.set_editor_property("text", title + "。" + instructions)
        segment.set_editor_property("segment_id", "Showcase_" + identity)
        segment.set_editor_property("lines", [line])
        library.save_loaded_asset(segment)
        position_x = (index // 3) * 850
        position_y = ((index % 3) - 1) * 1000
        accent = material("M_ShowcaseAccent_" + str(index), color, True)
        box("Exhibit_%02d_Pad" % (index + 1), (position_x, position_y, 3), (550, 650, 6), accent)
        box("Exhibit_%02d_Plinth" % (index + 1), (position_x + 260, position_y, 105), (55, 570, 210), dark_material)
        box("Exhibit_%02d_Header" % (index + 1), (position_x + 255, position_y, 220), (60, 570, 12), accent)
        text("Exhibit_%02d_Label" % (index + 1), "%02d / %s" % (index + 1, english), (position_x + 225, position_y, 170), 23, color)
        text("Exhibit_%02d_Enter" % (index + 1), "[ E ] ENTER", (position_x + 224, position_y, 92), 28, (0.9, 0.95, 1))
    director.set_editor_property("exhibits", entries)
    for position_x in [-900, 650, 2300, 3600]:
        light = actors.spawn_actor_from_class(unreal.PointLight, ORIGIN + unreal.Vector(position_x, 0, 820))
        light.set_actor_label("Gallery_LocalLight")
        light.set_folder_path("Showcase/Lighting")
        component = light.get_component_by_class(unreal.PointLightComponent)
        component.set_mobility(unreal.ComponentMobility.MOVABLE)
        component.set_editor_property("intensity", 18000)
        component.set_editor_property("attenuation_radius", 2500)
        component.set_editor_property("cast_shadows", False)
    start = actors.spawn_actor_from_class(unreal.PlayerStart, ORIGIN + unreal.Vector(-1300, 0, 110))
    start.set_actor_label("Showcase_LobbyStart")
    start.set_folder_path("Showcase/Control")
    unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).set_level_viewport_camera_info(ORIGIN + unreal.Vector(-2700, -1600, 1800), unreal.Rotator(pitch=-22, yaw=22, roll=0))
    if not levels.save_current_level():
        raise RuntimeError("Could not save showcase")
    report = {"map": MAP, "exhibits": [{"id": entry[0], "map": entry[2]} for entry in EXHIBITS], "sources_modified": False}
    report_dir = os.path.join(unreal.Paths.project_saved_dir(), "Validation")
    os.makedirs(report_dir, exist_ok=True)
    with open(os.path.join(report_dir, "showcase_creation.json"), "w", encoding="utf-8") as output:
        json.dump(report, output, ensure_ascii=False, indent=2)
    unreal.log("SHOWCASE_CREATED: " + MAP)


main()
