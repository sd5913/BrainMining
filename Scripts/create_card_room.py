"""Create only /Game/Rooms/CardDuel. Run once with UE Python; refuses to replace an existing map."""
import unreal
ROOT = "/Game/Rooms/CardDuel"
MAP = ROOT + "/Maps/Lvl_CardRoom_P1"
assets = unreal.AssetToolsHelpers.get_asset_tools()
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
if unreal.EditorAssetLibrary.does_asset_exist(MAP):
    raise RuntimeError("Card room already exists. Refusing to overwrite the playable level.")
for folder in ("Maps", "Materials", "Audio"):
    unreal.EditorAssetLibrary.make_directory(ROOT + "/" + folder)

def material(name, color, glow=False, parameter=False):
    path=ROOT+"/Materials/"+name
    if unreal.EditorAssetLibrary.does_asset_exist(path): return unreal.load_asset(path)
    mat=assets.create_asset(name,ROOT+"/Materials",unreal.Material,unreal.MaterialFactoryNew())
    expr=unreal.MaterialEditingLibrary.create_material_expression(mat,unreal.MaterialExpressionVectorParameter if parameter else unreal.MaterialExpressionConstant3Vector)
    if parameter:
        expr.set_editor_property("parameter_name","Color")
        expr.set_editor_property("default_value",unreal.LinearColor(*color,1))
    else: expr.set_editor_property("constant",unreal.LinearColor(*color,1))
    unreal.MaterialEditingLibrary.connect_material_property(expr,"",unreal.MaterialProperty.MP_BASE_COLOR)
    if glow:
        unreal.MaterialEditingLibrary.connect_material_property(expr,"",unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    rough=unreal.MaterialEditingLibrary.create_material_expression(mat,unreal.MaterialExpressionConstant)
    rough.set_editor_property("r",.76)
    unreal.MaterialEditingLibrary.connect_material_property(rough,"",unreal.MaterialProperty.MP_ROUGHNESS)
    unreal.MaterialEditingLibrary.recompile_material(mat)
    unreal.EditorAssetLibrary.save_loaded_asset(mat)
    return mat

material("M_CardRoomSurface",(.7,.4,.22),parameter=True)
material("M_CardRoomGlow",(.2,.8,1),True,True)
palette={
 "Oak":(.22,.105,.045), "OakLight":(.42,.24,.105), "Trim":(.085,.045,.023),
 "Plaster":(.36,.23,.135), "Green":(.025,.095,.078), "Carpet":(.11,.17,.14),
 "Gold":(.65,.38,.11), "Cream":(.76,.63,.40), "Brick":(.14,.061,.034),
 "BookRed":(.32,.08,.045), "BookBlue":(.055,.15,.21), "BookGreen":(.10,.19,.085),
 "Dark":(.014,.02,.025), "Window":(.10,.23,.30)}
mats={name:material("M_"+name,color) for name,color in palette.items()}
mats["Lamp"]=material("M_Lamp",(1,.56,.19),True)
if not levels.new_level(MAP): raise RuntimeError("Could not create separate card room level")

def shape(name,pos,size,mat="Oak",mesh="Cube",rotation=None,collision=True):
    obj=actors.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(*pos),rotation or unreal.Rotator())
    obj.set_actor_label(name)
    comp=obj.static_mesh_component
    comp.set_static_mesh(unreal.load_asset("/Engine/BasicShapes/"+mesh))
    comp.set_material(0,mats[mat])
    obj.set_actor_scale3d(unreal.Vector(*(v/100 for v in size)))
    if not collision: comp.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    return obj

def label(name,text,pos,size=25,color=(218,175,104),yaw=180):
    obj=actors.spawn_actor_from_class(unreal.TextRenderActor,unreal.Vector(*pos),unreal.Rotator(pitch=0,yaw=yaw,roll=0))
    obj.set_actor_label(name)
    comp=obj.get_component_by_class(unreal.TextRenderComponent)
    comp.set_text(text); comp.set_world_size(size); comp.set_text_render_color(unreal.Color(r=color[0],g=color[1],b=color[2],a=255)); comp.set_horizontal_alignment(unreal.HorizTextAligment.EHTA_CENTER)
    return obj

shape("Floor foundation",(-60,0,-15),(1260,940,30),"Trim")
for i in range(18):
    shape("Oak floor plank %02d"%i,(-655+i*70,0,1),(67,940,3),"OakLight" if i%4==0 else "Oak")
shape("Back plaster wall",(555,0,235),(25,940,470),"Plaster")
for y in (-470,470):
    shape("Side plaster wall",(-60,y,235),(1260,25,470),"Plaster")
    shape("Wall dado",(-60,y*.967,90),(1240,20,180),"Green")
    shape("Dado upper moulding",(-60,y*.947,183),(1240,12,10),"Gold")
    for x in range(-610,561,130): shape("Wall panel stile",(x,y*.944,89),(7,10,176),"Trim")
shape("Entry wall",(-690,0,235),(25,940,470),"Plaster")
shape("Ceiling",(-60,0,482),(1280,960,22),"Oak")
for x in (-520,-100,320): shape("Ceiling beam",(x,0,452),(28,940,32),"Trim")
shape("Room carpet border",(-20,0,5),(770,560,4),"Gold",collision=False)
shape("Room carpet",(-20,0,8),(748,538,3),"Carpet",collision=False)
for y in (-245,245): shape("Rug stitch",(-20,y,10),(706,3,1),"Cream",collision=False)
# Empty tabletop is deliberate until the player starts matching.
shape("Card table top",(0,0,103),(340,252,18),"OakLight")
shape("Card table felt",(0,0,113),(310,218,3),"Green",collision=False)
for y in (-112,112): shape("Table brass trim",(0,y,115),(316,2,2),"Gold",collision=False)
for x in (-159,159): shape("Table brass trim",(x,0,115),(2,226,2),"Gold",collision=False)
for x in (-134,134):
    for y in (-92,92): shape("Table leg",(x,y,50),(18,18,100),"Trim")
for x,direction in ((-250,-1),(217,1)):
    shape("Velvet chair seat",(x,0,54),(82,88,15),"Green")
    shape("Chair back",(x+direction*36,0,116),(14,89,126),"Trim")
    shape("Chair back cushion",(x+direction*26,0,121),(9,73,96),"Green")
    for dx in (-29,29):
        for y in (-32,32): shape("Chair foot",(x+dx,y,25),(9,9,50),"Trim")
# Tall bookcases frame the opponent.
for side in (-1,1):
    center=side*293
    shape("Bookcase back",(521,center,181),(22,225,342),"Trim")
    for y in (center-115,center+115): shape("Bookcase upright",(488,y,181),(83,13,355),"OakLight")
    for z in (29,119,209,299,355): shape("Bookcase shelf",(488,center,z),(83,241,12),"OakLight")
    for row,z in enumerate((37,127,217)):
        for b in range(10):
            h=43+(b*17+row*11)%29
            shape("Book",(482,center-98+b*21,z+h/2),(44,13+(b%3)*2,h),["BookRed","BookBlue","BookGreen","Cream"][(b+row)%4])
shape("Ember sign backing",(528,0,320),(16,300,101),"Green")
label("Room title","EMBER PARLOUR",(515,0,335),27)
label("Room subtitle","ONE TABLE. FOUR TURNS.",(514,0,300),12)
# Warm wall sconces, windows, sideboard, candle details.
for y in (-432,432):
    shape("Window frame",(-170,y,290),(210,12,186),"Trim")
    shape("Night window",(-170,y*.978,290),(188,6,164),"Window",collision=False)
    shape("Window mullion",(-170,y*.964,290),(6,6,164),"Gold",collision=False)
    shape("Window crossbar",(-170,y*.964,290),(188,6,6),"Gold",collision=False)
shape("Sideboard",(150,-383,72),(255,112,140),"OakLight")
shape("Sideboard top",(150,-383,148),(270,128,12),"Trim")
for x in (55,150,245):
    shape("Sideboard drawer",(x,-323,94),(74,3,69),"Green")
    shape("Drawer knob",(x,-318,99),(7,7,7),"Gold","Sphere",collision=False)
for x in (66,88,110):
    shape("Candlestick",(x,-369,167),(8,8,34),"Gold","Cylinder",collision=False)
    shape("Candle",(x,-369,196),(7,7,32),"Cream","Cylinder",collision=False)
    shape("Candle flame",(x,-369,215),(4,4,8),"Lamp","Sphere",collision=False)
shape("Decorative pot",(260,363,65),(72,72,112),"BookRed","Cylinder")
for i in range(7): shape("Plant leaves",(260+(i%3-1)*21,363+(i//3-1)*22,145+i%2*23),(45,45,100),"BookGreen","Sphere",collision=False)
for x,y in ((-190,0),(245,0),(130,-355),(190,365)):
    shape("Pendant cord",(x,y,415),(3,3,65),"Dark","Cylinder",collision=False)
    shape("Pendant shade",(x,y,382),(56,56,32),"Green","Cone",collision=False)
    shape("Pendant glow",(x,y,364),(31,31,8),"Lamp","Sphere",collision=False)
    lamp=actors.spawn_actor_from_class(unreal.PointLight,unreal.Vector(x,y,345))
    lamp.set_actor_label("Warm pendant light")
    lamp.light_component.set_editor_property("mobility",unreal.ComponentMobility.MOVABLE)
    lamp.light_component.set_editor_property("intensity",3500)
    lamp.light_component.set_editor_property("attenuation_radius",850)
    lamp.light_component.set_editor_property("light_color",unreal.Color(r=255,g=195,b=122,a=255))
    lamp.light_component.set_editor_property("source_radius",24)
fill=actors.spawn_actor_from_class(unreal.PointLight,unreal.Vector(-310,0,245))
fill.set_actor_label("Soft table fill")
fill.light_component.set_editor_property("mobility",unreal.ComponentMobility.MOVABLE)
fill.light_component.set_editor_property("intensity",1700)
fill.light_component.set_editor_property("attenuation_radius",650)
fill.light_component.set_editor_property("light_color",unreal.Color(r=255,g=231,b=204,a=255))
fill.light_component.set_editor_property("cast_shadows",False)
# Fixed exposure makes the standalone room predictable on different machines.
post=actors.spawn_actor_from_class(unreal.PostProcessVolume,unreal.Vector())
post.set_editor_property("unbound",True)
settings=post.get_editor_property("settings")
settings.set_editor_property("override_auto_exposure_method",True)
settings.set_editor_property("auto_exposure_method",unreal.AutoExposureMethod.AEM_MANUAL)
settings.set_editor_property("override_auto_exposure_bias",True)
settings.set_editor_property("auto_exposure_bias",-6.0)
settings.set_editor_property("override_auto_exposure_apply_physical_camera_exposure",True)
settings.set_editor_property("auto_exposure_apply_physical_camera_exposure",False)
post.set_editor_property("settings",settings)
# Use the room-only mode. Never modify the project's startup/default map.
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
world.get_world_settings().set_editor_property("default_game_mode",unreal.load_class(None,"/Script/BrainMining.MiningEncounterGameMode"))
stage=actors.spawn_actor_from_class(unreal.load_class(None,"/Script/CardRoom.CardRoomStage"),unreal.Vector())
stage.set_actor_label("CardRoom_Encounter")
start=actors.spawn_actor_from_class(unreal.PlayerStart,unreal.Vector(-465,-145,96),unreal.Rotator(pitch=0,yaw=18,roll=0))
start.set_actor_label("CardRoom_Entry")
unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).set_level_viewport_camera_info(unreal.Vector(-465,-210,250),unreal.Rotator(pitch=-12,yaw=23,roll=0))
if not levels.save_current_level(): raise RuntimeError("Could not save card room map")
# Import generated, original prototype audio if present.
import os
sound_dir=os.path.join(unreal.Paths.project_saved_dir(),"CardRoomSourceAudio")
for name in ("S_Laser","S_Poison","S_Impact","S_Finale","S_Arrival","S_Draw","S_Reward"):
    filename=os.path.join(sound_dir,name+".wav")
    if os.path.exists(filename):
        task=unreal.AssetImportTask(); task.set_editor_property("filename",filename); task.set_editor_property("destination_path",ROOT+"/Audio"); task.set_editor_property("automated",True); task.set_editor_property("save",True)
        assets.import_asset_tasks([task])
unreal.log("CARDROOM_V01_CREATED: "+MAP)