"""V05: familiar desktop objects in the seated player's view, inspired by miniature-stage composition.
Preserves the original player, entrance, card stage, cat chair, warm room and V04 shell.
Run with UE Python. Only actors tagged MiniDeskV05 are regenerated on repeat.
"""
import math
import unreal
A=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
L=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert L.load_level('/Game/Passages/Echo/Maps/Lvl_EchoMazeToCards_P1')
TAG=unreal.Name('MiniDeskV05')
ADJUSTED=unreal.Name('MiniDeskV05Adjusted')
original=list(A.get_all_level_actors())
for a in original:
    if TAG in a.tags:A.destroy_actor(a)
M='/Game/Rooms/CardDuel/Materials/'
mats={k:unreal.load_asset(M+'M_'+k) for k in ['Oak','OakLight','Trim','Cream','Gold','Green','BookRed','BookBlue','BookGreen','Dark','GiantDeskPaper','GiantDeskCloth','GiantDeskPencil','GiantDeskInk','GiantDeskEraser','GiantDeskGlow']}
for k,m in mats.items():assert m, k
def shape(label,p,size,mat='Oak',mesh='Cube',rotation=None,collision=True):
    a=A.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(*p),rotation or unreal.Rotator())
    a.set_actor_label('Mini desk / '+label);a.tags=[TAG];a.set_folder_path('GiantRoom/MiniDeskV05')
    c=a.static_mesh_component;c.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/'+mesh));c.set_material(0,mats[mat])
    a.set_actor_scale3d(unreal.Vector(*(v/100 for v in size)))
    if not collision:a.set_actor_enable_collision(False);c.set_cast_shadow(False)
    return a
def bar(label,p,q,width,mat='Trim',collision=True):
    p,q=unreal.Vector(*p),unreal.Vector(*q);d=q-p
    rot=unreal.MathLibrary.make_rot_from_z(d)
    return shape(label,((p.x+q.x)/2,(p.y+q.y)/2,(p.z+q.z)/2),(width,width,d.length()),mat,'Cylinder',rot,collision)
def text(label,value,p,size=100,yaw=180,color=(244,214,158)):
    a=A.spawn_actor_from_class(unreal.TextRenderActor,unreal.Vector(*p),unreal.Rotator(yaw=yaw))
    a.tags=[TAG];a.set_actor_label('Mini desk / '+label);a.set_folder_path('GiantRoom/MiniDeskV05')
    c=a.get_component_by_class(unreal.TextRenderComponent);c.set_text(value);c.set_world_size(size)
    c.set_text_render_color(unreal.Color(r=color[0],g=color[1],b=color[2],a=255))
    c.set_horizontal_alignment(unreal.HorizTextAligment.EHTA_CENTER);a.set_actor_enable_collision(False)
    return a
# Enlarge the desktop laterally, keeping the exact gameplay height and central card lane.
for a in A.get_all_level_actors():
    if TAG in a.tags or ADJUSTED in a.tags:continue
    label=a.get_actor_label();p=a.get_actor_location();s=a.get_actor_scale3d();change=False
    if label=='Giant / Card table top':s.y=51;change=True
    elif label=='Giant / Card table felt':s.y=49.8;change=True
    elif label=='Giant / Table brass trim':
        if abs(p.y-3600)>100:p.y=3600+(-1 if p.y<3600 else 1)*2472
        else:s.y=49.56
        change=True
    elif label.startswith('Desk / Giant textbook'):
        p=unreal.Vector(12200,2050,698)+(p-unreal.Vector(10340,3020,698))*2.6;s=s*2.6;change=True
    elif label.startswith('Desk / Back desk atlas'):
        p=unreal.Vector(10920,1970,698)+(p-unreal.Vector(11200,3030,698))*2;s=s*2;change=True
    elif any(w in label for w in ['Desk / Open notebook','Desk / Notebook']):
        p=unreal.Vector(10360,5180,698)+(p-unreal.Vector(10390,4100,698))*1.6;s=s*1.6;change=True
    elif label.startswith('Desk /') and any(w in label for w in ['pencil','Pencil','eraser']):
        A.destroy_actor(a);continue
    if change:
        a.set_actor_location(p,False,False);a.set_actor_scale3d(s);a.tags=list(a.tags)+[ADJUSTED]
# Two rear desk wings keep the central opponent chair unobstructed.
for y in [1950,5250]:
    shape('Rear desktop wing',(12580,y,618),(2040,1800,138),'OakLight')
    shape('Wing edge dark wood',(13600,y,610),(24,1800,110),'Trim')
    for x in [9900,13200]:
        shape('Outer desk leg',(x,y,300),(150,150,600),'Oak')
    shape('Desk apron',(11500,1050 if y<3600 else 6150,450),(4000,90,330),'Oak')
# Visible grain and deep end-grain fascia make the desk read as one piece of furniture.
for y in [1230,1420,2780,4460,5900]:
    shape('Long wood grain',(10540,y,688.5),(1830,5,1.5),'Oak',collision=False)
for z in [590,618,646]:
    shape('Front edge grain',(9518,3600,z),(3,5020,3),'Trim',collision=False)
# A readable standing book and layered spines directly behind the left book stack.
shape('Standing atlas pages',(13400,2050,1900),(240,1370,2400),'GiantDeskPaper')
for x in [13255,13545]:shape('Standing atlas cover',(x,2050,1900),(32,1460,2460),'BookGreen')
shape('Standing atlas spine',(13400,2788,1900),(315,36,2460),'BookGreen')
for z in [920,1030,2710,2820]:shape('Atlas gold tooling',(13235,2050,z),(8,1250,12),'Gold',collision=False)
text('Atlas cover title','FIELD\nNOTES',(13230,2050,2000),210)
text('Atlas cover edition','BOTANICAL ATLAS',(13230,2050,1500),72)
for i,title in enumerate(['NATURE','LITTLE WORLDS','FIELD GUIDE','STORIES']):
    text('Stack book title '+str(i),title,(11202+i%2*91,2050,760+i*247),87)
# A large hollow faceted cocoa mug: recognizable handle and rim, not an anonymous cylinder.
cx,cy,base=12300,5110,696
radius,height=520,1650
for i in range(16):
    ang=2*math.pi*i/16
    px,py=cx+radius*math.cos(ang),cy+radius*math.sin(ang)
    shape('Ceramic mug wall',(px,py,base+height/2),(80,210,height),'Cream',rotation=unreal.Rotator(yaw=math.degrees(ang)))
    shape('Mug foot ring',(px,py,base+55),(98,211,110),'BookBlue',rotation=unreal.Rotator(yaw=math.degrees(ang)))
    shape('Mug blue rim',(px,py,base+height-20),(98,211,90),'BookBlue',rotation=unreal.Rotator(yaw=math.degrees(ang)))
shape('Cocoa surface',(cx,cy,base+height-110),(980,980,22),'Trim','Cylinder',collision=False)
# Handle lies in YZ and faces the player; its opening is several player-heights high.
handle=[]
for i in range(13):
    t=-math.pi*.5+math.pi*i/12
    handle.append((cx,cy+460+510*math.cos(t),base+830+610*math.sin(t)))
for p,q in zip(handle,handle[1:]):bar('Mug arched handle',p,q,135,'Cream')
text('Cocoa label','COCOA',(cx-565,cy,base+790),165,180,(80,103,107))
# Simple paw emblem on the cup face, made with project geometry.
shape('Cup paw pad',(cx-563,cy,base+420),(12,225,170),'BookBlue','Sphere',collision=False)
for dy,dz in [(-145,570),(-50,630),(55,630),(150,570)]:
    shape('Cup paw toe',(cx-566,cy+dy,base+dz),(14,90,110),'BookBlue','Sphere',collision=False)
# Giant pencil and ruler show everyday units in the middle/foreground.
bar('Giant yellow pencil',(10400,4390,830),(12050,4390,830),260,'GiantDeskPencil')
shape('Pencil wood tip',(12160,4390,830),(260,260,260),'GiantDeskPaper','Cone',unreal.Rotator(pitch=90))
shape('Pencil graphite tip',(12245,4390,830),(95,95,150),'Dark','Cone',unreal.Rotator(pitch=90))
bar('Pencil metal ferrule',(10290,4390,830),(10400,4390,830),274,'Gold')
bar('Pencil pink eraser',(10030,4390,830),(10290,4390,830),266,'GiantDeskEraser')
shape('Measuring ruler',(11030,3970,702),(1540,170,24),'GiantDeskPencil')
for i in range(31):
    x=10290+i*48
    shape('Ruler scale tick',(x,3915+(0 if i%5==0 else 14),715),(4,64 if i%5==0 else 35,2),'GiantDeskInk',collision=False)
# A large binder clip at the near edge: another familiar tiny object at player size.
shape('Binder clip wedge',(10880,3040,795),(240,310,205),'Dark')
for y in [2900,3180]:
    for p,q in [((10800,y,890),(10800,y,1200)),((10800,y,1200),(10965,y,1200)),((10965,y,1200),(10965,y,890))]:
        bar('Binder clip wire handle',p,q,20,'Gold')
# Articulated desk lamp crosses above the desktop, providing an identifiable local light source.
shape('Lamp heavy foot',(13250,5740,760),(570,570,130),'Green','Cylinder')
bar('Lamp lower arm',(13250,5740,820),(13400,4500,3500),100,'Green')
bar('Lamp upper arm',(13400,4500,3500),(12000,2350,2850),110,'Green')
for p in [(13250,5740,940),(13400,4500,3500),(12000,2350,2850)]:
    shape('Lamp hinge',p,(230,230,230),'Gold','Sphere')
shape('Desk lamp shade',(12000,2350,2840),(1150,1150,600),'Green','Cone')
shape('Desk lamp glowing underside',(12000,2350,2550),(1070,1070,22),'GiantDeskGlow','Cylinder',collision=False)
source=unreal.Vector(12000,2350,2500);target=unreal.Vector(10600,3600,700)
a=A.spawn_actor_from_class(unreal.SpotLight,source,unreal.MathLibrary.find_look_at_rotation(source,target))
a.tags=[TAG];a.set_actor_label('Mini desk / Warm working lamp');a.set_folder_path('GiantRoom/MiniDeskV05')
c=a.light_component;c.set_editor_property('mobility',unreal.ComponentMobility.MOVABLE)
c.set_intensity(150000);c.set_editor_property('attenuation_radius',4300)
c.set_editor_property('inner_cone_angle',30);c.set_editor_property('outer_cone_angle',48)
c.set_light_color(unreal.LinearColor(1,.68,.32,1));c.set_editor_property('cast_shadows',False)
assert L.save_current_level()
unreal.log('MINI_DESK_V05_READY')
