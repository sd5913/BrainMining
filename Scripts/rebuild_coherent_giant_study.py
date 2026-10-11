"""V06 coherent giant study: all environment dimensions below are human-reference cm.
ONE scale factor (40) converts room/furniture/stationery to world cm.
Keep the original miniature pawn, passage, excavation entrance and card rules.
This replaces only room decor owned by the V01-V05 generators.
"""
import math, json
from pathlib import Path
import unreal
S=40.0
O=unreal.Vector(6400,3600,0)
TAG=unreal.Name('CoherentStudyV06')
A=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
L=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert L.load_level('/Game/Passages/Echo/Maps/Lvl_EchoMazeToCards_P1')
all_actors=list(A.get_all_level_actors())
stage=next(a for a in all_actors if isinstance(a,unreal.CardRoomStage))
entrance=next(a for a in all_actors if isinstance(a,unreal.MiningGiantRoomEntrance))
entrance.tags=[t for t in entrance.tags if str(t) not in ['StudyV08LayoutApplied','StudyV09LayoutApplied','StudyV10LayoutApplied','StudyV11LayoutApplied']]
owned={'GiantCardRoomV01','GiantDeskV02','GiantBackgroundV03','MiniDeskV05','CoherentStudyV06','StudyAccessV07','StudyPresentationV08','StudySideRouteV09','StudyLampDecorV10','StudyLampBeamV11'}
keep={'Giant / Light through the pinhole','Giant / Pinhole reflected warm rim'}
for a in all_actors:
    if a==stage or a==entrance or a.get_actor_label() in keep:continue
    if any(str(t) in owned for t in a.tags):A.destroy_actor(a)
base='/Game/Rooms/CardDuel/Materials/'
mats={k:unreal.load_asset(base+'M_'+k) for k in ['Oak','OakLight','Trim','Plaster','Carpet','Cream','Gold','Green','BookRed','BookBlue','BookGreen','Dark','GiantDeskPaper','GiantDeskPencil','GiantDeskInk','GiantDeskEraser','GiantDeskGlow','GiantStageBeam']}
assert all(mats.values())
def wp(p):return O+unreal.Vector(*(v*S for v in p))
def mark(a,label):
    a.tags=[TAG];a.set_actor_label('Study / '+label);a.set_folder_path('GiantRoom/CoherentStudyV06');return a
def shape(label,p,size,mat='Oak',mesh='Cube',rotation=None,collision=True):
    a=mark(A.spawn_actor_from_class(unreal.StaticMeshActor,wp(p),rotation or unreal.Rotator()),label)
    c=a.static_mesh_component;c.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/'+mesh));c.set_material(0,mats[mat])
    a.set_actor_scale3d(unreal.Vector(*(v*S/100 for v in size)))
    if not collision:a.set_actor_enable_collision(False);c.set_cast_shadow(False)
    return a
def bar(label,p,q,width,mat='Trim',collision=True):
    d=unreal.Vector(*q)-unreal.Vector(*p)
    return shape(label,tuple((p[i]+q[i])/2 for i in range(3)),(width,width,d.length()),mat,'Cylinder',unreal.MathLibrary.make_rot_from_z(d),collision)
def text(label,value,p,size=2,yaw=180,color=(235,210,156)):
    a=mark(A.spawn_actor_from_class(unreal.TextRenderActor,wp(p),unreal.Rotator(yaw=yaw)),label)
    c=a.get_component_by_class(unreal.TextRenderComponent);c.set_text(value);c.set_world_size(size*S)
    c.set_horizontal_alignment(unreal.HorizTextAligment.EHTA_CENTER)
    c.set_text_render_color(unreal.Color(r=color[0],g=color[1],b=color[2],a=255));a.set_actor_enable_collision(False)
    return a
def light(label,p,intensity,radius=240,color=(1,.65,.32),target=None,shadow=False):
    rot=unreal.MathLibrary.find_look_at_rotation(wp(p),wp(target)) if target else unreal.Rotator()
    a=mark(A.spawn_actor_from_class(unreal.SpotLight if target else unreal.PointLight,wp(p),rot),label)
    c=a.light_component;c.set_editor_property('mobility',unreal.ComponentMobility.MOVABLE)
    c.set_light_color(unreal.LinearColor(*color,1));c.set_intensity(intensity)
    c.set_editor_property('attenuation_radius',radius*S);c.set_editor_property('cast_shadows',shadow)
    if target:c.set_editor_property('inner_cone_angle',18);c.set_editor_property('outer_cone_angle',38)
    return a
# 5m x 5m x 3m study, in giant-reference units.
shape('Floor',(250,0,-2),(500,500,4),'Oak')
for i in range(25):
    shape('Floor plank',(10+i*20,0,.15),(19.7,500,.3),'OakLight')
shape('Back wall',(500,0,150),(5,500,300),'Plaster')
for side in [-1,1]:
    y=side*250
    shape('Side wall',(250,y,150),(500,5,300),'Plaster')
    shape('Wainscot',(250,y-side*3.3,48),(500,1.6,96),'Green')
    shape('Dado moulding',(250,y-side*4.5,98),(500,2.5,3),'Gold')
    for x in range(30,500,45):shape('Wall panel trim',(x,y-side*4.6,49),(1.4,1,94),'Trim')
    shape('Entry surround',(0,side*127.5,150),(3.75,245,300),'Plaster')
shape('Entry upper wall',(0,0,154.75),(3.75,10,290.5),'Plaster')
shape('Ceiling',(250,0,304),(505,505,8),'Oak')
for x in [80,250,420]:shape('Ceiling beam',(x,0,294),(12,500,12),'Trim')
shape('Rug border',(300,0,.6),(350,300,.5),'Gold',collision=False)
shape('Rug',(300,0,.9),(342,292,.4),'Carpet',collision=False)
# Desk: 180cm wide, 90cm deep, 75cm high, 3cm solid top; no implausible low U-shaped platform.
D=222.5;H=75.0
shape('Desk solid top',(D,0,H-1.5),(90,180,3),'OakLight')
for x in [D-38,D+38]:
    for y in [-82,82]:shape('Desk leg',(x,y,36),(6,6,72),'Oak')
for y in [-85,85]:shape('Desk side apron',(D,y,68),(80,3,8),'Oak')
shape('Desk rear apron',(D+40,0,68),(3,164,8),'Oak')
for z in [72.5,73.5,74.5]:
    shape('Desk end grain',(D-45.04,0,z),(.06,178,.06),'Trim',collision=False)
for y in [-70,-45,39,72]:
    shape('Desk fine wood grain',(D,y,H+.01),(87,.08,.02),'Oak',collision=False)
# Opponent chair: 50x50cm seat, 45cm seat height, 95cm back height.
C=D+75
shape('Chair seat',(C,0,41),(50,50,8),'Green')
for x in [C-20,C+20]:
    for y in [-20,20]:shape('Chair leg',(x,y,18.5),(4,4,37),'Trim')
shape('Chair back',(C+22,0,67.5),(4,50,55),'Trim')
shape('Chair back cushion',(C+19.5,0,69),(1,43,46),'Green')
# Bookcases: 110x35x210cm, physically against rear wall and inside the main viewing cone.
for side in [-1,1]:
    y=side*115;x=479
    shape('Bookcase back',(495.5,y,105),(2,110,210),'Trim')
    for yy in [y-54,y+54]:shape('Bookcase upright',(x,yy,105),(35,2,210),'OakLight')
    for z in [1.5,53,104.5,156,208.5]:shape('Bookcase shelf',(x,y,z),(35,110,3),'OakLight')
    for row,z in enumerate([3,54.5,106,157.5]):
        for i in range(16):
            h=24+(i*7+row*3)%15
            shape('Shelf book',(477,y-47+i*6.1,z+h/2),(23,3.5+(i%3)*.4,h),['BookRed','BookBlue','BookGreen','Cream'][(i+row)%4])
shape('Room sign',(496.5,0,170),(1.5,90,34),'Green')
text('Room title','EMBER PARLOUR',(495.65,0,176),6.5)
text('Room subtitle','ONE TABLE. FOUR TURNS.',(495.65,0,164),2.7)
# Sideboard: 150x40x90cm, drawers, ordinary 20cm candles.
shape('Sideboard',(175,-222,43),(148,38,86),'OakLight')
shape('Sideboard top',(175,-222,88),(150,40,4),'Trim')
for x in [125,175,225]:
    shape('Drawer',(x,-202.7,49),(46,1,60),'Green')
    shape('Drawer knob',(x,-201.5,52),(2,2,2),'Gold','Sphere',collision=False)
for x in [135,147,159]:
    shape('Candle holder',(x,-222,93),(5,5,6),'Gold','Cylinder')
    shape('Candle',(x,-222,106),(3,3,20),'Cream','Cylinder')
    shape('Candle flame',(x,-222,117),(1.4,1.4,3),'GiantDeskGlow','Sphere',collision=False)
# Plant about 1.3m overall; room windows and pendants also use the same unit system.
shape('Plant pot',(350,215,20),(32,32,40),'BookRed','Cylinder')
for i in range(7):
    shape('Plant foliage',(350+(i%3-1)*10,215+(i//3-1)*9,75+i%2*16),(23,23,70),'BookGreen','Sphere',collision=False)
for side in [-1,1]:
    y=side*246
    shape('Window frame',(210,y,170),(130,3,150),'Trim')
    shape('Warm window',(210,y-side*2,170),(122,1,142),'GiantDeskGlow',collision=False)
    shape('Window mullion',(210,y-side*3,170),(2,1,142),'Gold',collision=False)
    shape('Window crossbar',(210,y-side*3,170),(122,1,2),'Gold',collision=False)
for x,y in [(200,0),(390,0)]:
    shape('Pendant cord',(x,y,268),(1,1,64),'Dark','Cylinder',collision=False)
    shape('Pendant shade',(x,y,234),(42,42,24),'Green','Cone',collision=False)
    shape('Pendant bulb',(x,y,222),(30,30,4),'GiantDeskGlow','Cylinder',collision=False)
    light('Warm pendant',(x,y,215),1800000,310)
light('Warm shelves fill',(415,0,145),950000,230)
light('Sideboard warm fill',(150,-155,130),500000,165)
# Books: 30x22x3cm; four books form a 13.8cm stack, not furniture-sized blocks.
def book(label,x,y,z,w=22,h=30,t=3,cover='BookRed'):
    shape(label+' pages',(x,y,z+t/2),(w-.5,h-.5,t-.4),'GiantDeskPaper')
    for dz in [.1,t-.1]:shape(label+' cover',(x,y,z+dz),(w,h,.2),cover)
    shape(label+' spine',(x-w/2+.1,y,z+t/2),(.25,h,t),cover)
    for dz in [.65,1.15,1.65,2.15,2.65]:
        if dz<t-.2:shape(label+' page edge',(x,y-h/2+.05,z+dz),(w-.4,.03,.025),'Oak',collision=False)
    return z+t
z=H+.03
for i,title in enumerate(['NATURE','LITTLE WORLDS','FIELD GUIDE','STORIES']):
    x=254+i%2*.7;t=3.2 if i%2 else 3.7
    book('Desk book '+str(i),x,-28,z,22-i*.5,30,t,['BookRed','BookGreen','BookBlue','BookRed'][i])
    text('Book spine '+str(i),title,(x-(22-i*.5)/2-.03,-28,z+t*.42),1.35)
    z+=t+.04
# Standing atlas 22x30x3cm, beside the stack; modest bookend supports it.
shape('Standing atlas pages',(264,-57,90),(2.6,21.6,29.6),'GiantDeskPaper')
for x in [262.55,265.45]:shape('Standing atlas cover',(x,-57,90),(.3,22,30),'BookGreen')
shape('Standing atlas spine',(264,-68,90),(3,.3,30),'BookGreen')
text('Atlas cover','FIELD\nNOTES',(262.36,-57,94),3.2)
shape('Bookend base',(264,-72,H+.15),(8,7,.3),'Gold')
shape('Bookend upright',(264,-68.6,H+5),(8,.3,10),'Gold')
# Open A4 notebook, 18cm pencil, 30cm ruler, 32mm binder clip.
book('Open notebook',215,56,H+.05,42,30,1,'BookRed')
for i in range(12):shape('Notebook ruled line',(215,43+i*2.15,H+1.06),(38,.045,.02),'BookBlue',collision=False)
shape('Notebook fold',(215,56,H+1.1),(.08,29,.04),'Trim',collision=False)
bar('Pencil barrel',(239,35,H+.4),(254.3,35,H+.4),.75,'GiantDeskPencil')
shape('Pencil wood',(255.25,35,H+.4),(.75,.75,1.9),'GiantDeskPaper','Cone',unreal.Rotator(pitch=90))
shape('Pencil graphite',(256.08,35,H+.4),(.2,.2,.45),'Dark','Cone',unreal.Rotator(pitch=90))
bar('Pencil ferrule',(238.3,35,H+.4),(239,35,H+.4),.79,'Gold')
bar('Pencil eraser',(237,35,H+.4),(238.3,35,H+.4),.79,'GiantDeskEraser')
shape('Ruler',(236,12,H+.1),(30,3,.2),'GiantDeskPencil')
for i in range(31):
    shape('Ruler centimetre tick',(221+i,11.05,H+.212),(.04,1.15 if i%5==0 else .65,.015),'GiantDeskInk',collision=False)
shape('Binder clip',(237,-12.5,H+1.1),(2.2,3.2,2.2),'Dark')
for y in [-13.9,-11.1]:
    for p,q in [((236.2,y,H+2),(236.2,y,H+4.2)),((236.2,y,H+4.2),(237.8,y,H+4.2)),((237.8,y,H+4.2),(237.8,y,H+2))]:
        bar('Clip wire',p,q,.12,'Gold')
# Mug: 8cm outer diameter, 10cm tall; rim, drink and handle proportions match.
cx,cy=250,19
for i in range(20):
    ang=2*math.pi*i/20;p=(cx+3.75*math.cos(ang),cy+3.75*math.sin(ang))
    rot=unreal.Rotator(yaw=math.degrees(ang))
    shape('Mug wall',(*p,H+5),(.5,1.22,10),'Cream',rotation=rot)
    shape('Mug rim',(*p,H+9.85),(.58,1.24,.35),'BookBlue',rotation=rot)
shape('Mug coffee',(cx,cy,H+9.25),(7,7,.1),'Trim','Cylinder',collision=False)
points=[(cx,cy+3.5+2.4*math.cos(-math.pi/2+i*math.pi/12),H+5+3.2*math.sin(-math.pi/2+i*math.pi/12)) for i in range(13)]
for p,q in zip(points,points[1:]):bar('Mug handle',p,q,.65,'Cream')
text('Mug label','COCOA',(cx-4.05,cy,H+5.8),1.25,color=(65,95,100))
shape('Mug paw pad',(cx-4.02,cy,H+3.1),(.08,1.7,1.1),'BookBlue','Sphere',collision=False)
for dy,dz in [(-1,4.2),(-.35,4.6),(.35,4.6),(1,4.2)]:
    shape('Mug paw toe',(cx-4.03,cy+dy,H+dz),(.08,.6,.7),'BookBlue','Sphere',collision=False)
# Desk lamp: 45cm above desk, 15cm base, 20cm shade; restrained articulated arms.
shape('Desk lamp foot',(254,70,H+.7),(15,15,1.4),'Green','Cylinder')
bar('Desk lamp lower arm',(254,70,H+1.4),(260,62,H+45),1,'Green')
bar('Desk lamp upper arm',(260,62,H+45),(247,48,H+37),1,'Green')
for p in [(254,70,H+2),(260,62,H+45),(247,48,H+37)]:shape('Desk lamp hinge',p,(3,3,3),'Gold','Sphere')
shape('Desk lamp shade',(247,48,H+36),(20,20,10),'Green','Cone')
shape('Desk lamp diffuser',(247,48,H+31),(18.5,18.5,.25),'GiantDeskGlow','Cylinder',collision=False)
light('Desk lamp warm pool',(247,48,H+30),70000,85,target=(D,0,H))
# Gameplay alignment uses existing stage APIs; the room scale is not applied to the pawn or dealt cards.
stage.set_actor_location(wp((D,0,H))-unreal.Vector(0,0,227),False,False)
stage.set_actor_scale3d(unreal.Vector(2,2,2))
stage.set_editor_property('opponent_scale',10.0)
# 45cm seat top + 3.75cm root offset puts the existing plush body on the cushion.
root_z=45*S+150
stage.set_editor_property('opponent_base_local',unreal.Vector((wp((C,0,0)).x-stage.get_actor_location().x)/2,0,(root_z-stage.get_actor_location().z)/2))
stage.set_editor_property('seat_pitch',8.0)
stage.set_editor_property('opponent_exit_local',unreal.Vector((wp((497,0,0)).x-stage.get_actor_location().x)/2,0,1600))
stage.set_actor_transform(stage.get_actor_transform(),False,True)
bounds=entrance.get_component_by_class(unreal.BoxComponent)
bounds.set_relative_location(unreal.Vector(10000,0,5810),False,False);bounds.set_box_extent(unreal.Vector(10000,10000,6000),False)
# Keep the reachable landing at the stage's unchanged local approach offset.
landing=(D-13.5,0,H+.1)
for i in range(16):
    ang=2*math.pi*i/16
    shape('Landing light ring',(landing[0]+3.6*math.cos(ang),3.6*math.sin(ang),landing[2]),(1.3,.225,.075),'GiantDeskGlow',rotation=unreal.Rotator(yaw=math.degrees(ang)+90),collision=False)
for side in [-1,1]:light('Table stage spotlight',(landing[0],side*9.5,H+40),240000,65,target=landing,shadow=True)
shape('Landing light beam',(landing[0],0,H+20),(16,16,40),'GiantStageBeam','Cone',collision=False)
light('Player and cat soft fill',(D-30,0,H+26),95000,175,color=(1,.73,.47))
# Geometric assertions use world dimensions, independent of visual preference.
by_label={}
for a in A.get_all_level_actors():by_label.setdefault(a.get_actor_label(),[]).append(a)
def top(label):
    a=by_label['Study / '+label][0];p,e=a.get_actor_bounds(False);return p.z+e.z
assert abs(top('Desk solid top')-3000)<1
assert abs(top('Chair seat')-1800)<1
assert abs(top('Bookcase upright')-8400)<1
assert abs(stage.get_actor_location().z+227-3000)<1
assert abs(top('Mug wall')-3400)<1
assert abs(top('Standing atlas cover')-4200)<1
assert L.save_current_level()
report={'scale':S,'reference_cm':{'room':[500,500,300],'desk':[90,180,75],'chair_seat':[50,50,45],'bookcase':[35,110,210],'mug':[8,8,10],'pencil_length':19.3,'pencil_diameter':.75,'book':[22,30,3.5]},'desk_world_z':3000,'stage_world':str(stage.get_actor_location()),'geometry_assertions':'passed'}
out=Path(unreal.Paths.project_saved_dir())/'Automation/CoherentStudyV06';out.mkdir(parents=True,exist_ok=True);(out/'layout.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
unreal.log('COHERENT_STUDY_V06_READY')
