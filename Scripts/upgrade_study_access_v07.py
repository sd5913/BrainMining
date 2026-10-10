"""V07 overlay for V06: walkable book route, raised cat seat, warm stage contrast.
Run rebuild_coherent_giant_study.py first only when rebuilding the whole room.
All new props retain the same human-reference scale (40). Re-running is idempotent.
"""
import unreal, math, json
from pathlib import Path
A=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
L=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert L.load_level('/Game/Passages/Echo/Maps/Lvl_EchoMazeToCards_P1')
assert not any(unreal.Name('StudyV08LayoutApplied') in a.tags for a in A.get_all_level_actors()), 'V08 layout is active: rebuild V06 before applying V07, then apply V08.'
TAG=unreal.Name('StudyAccessV07')
for a in list(A.get_all_level_actors()):
    if TAG in a.tags:A.destroy_actor(a)
actors=list(A.get_all_level_actors())
stage=next(a for a in actors if isinstance(a,unreal.CardRoomStage))
base='/Game/Rooms/CardDuel/Materials/'
mats={k:unreal.load_asset(base+'M_'+k) for k in ['Oak','OakLight','Trim','Cream','Gold','Green','BookRed','BookBlue','BookGreen','Dark','GiantDeskPaper','GiantDeskGlow']}
def mark(a,label):
    a.set_actor_label('Study V07 / '+label);a.tags=[TAG];a.set_folder_path('GiantRoom/StudyAccessV07');return a
def box(label,p,size,mat,rot=None,collision=True):
    a=mark(A.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(*p),rot or unreal.Rotator()),label)
    c=a.static_mesh_component;c.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Cube'));c.set_material(0,mats[mat]);a.set_actor_scale3d(unreal.Vector(*(v/100 for v in size)))
    if not collision:a.set_actor_enable_collision(False);c.set_cast_shadow(False)
    return a
def light(label,p,target,intensity,radius,inner=18,outer=35,shadow=False):
    a=mark(A.spawn_actor_from_class(unreal.SpotLight,unreal.Vector(*p),unreal.MathLibrary.find_look_at_rotation(unreal.Vector(*p),unreal.Vector(*target))),label)
    c=a.light_component;c.set_editor_property('mobility',unreal.ComponentMobility.MOVABLE);c.set_light_color(unreal.LinearColor(1,.64,.30,1));c.set_intensity(intensity);c.set_editor_property('attenuation_radius',radius);c.set_editor_property('cast_shadows',shadow);c.set_editor_property('inner_cone_angle',inner);c.set_editor_property('outer_cone_angle',outer)
    return a
# Six tilted notebooks connect supported turn platforms. Cover tops are exactly
# coincident with the platform surfaces; no jump/flight or increased step height.
route=[(9900,1500,12)]
ramps=[]
for i in range(6):
    x=9900+600*i;sign=1 if i%2==0 else -1
    sy=1800 if sign==1 else 2800;ey=sy+sign*1000
    z0=12+i*498;z1=12+(i+1)*498
    p=unreal.Vector(x,sy,z0);q=unreal.Vector(x,ey,z1);d=q-p
    length=d.length();normal=unreal.Vector(0,-sign*498/length,1000/length)
    rot=unreal.MathLibrary.make_rot_from_x(d)
    mid=(p+q)*.5-normal*10
    ramp=box('Notebook ramp %d'%(i+1),(mid.x,mid.y,mid.z),(length,440,20),['BookRed','BookGreen','BookBlue'][i%3],rot)
    # Recessed cream page block plus visible binding make the incline read as a book.
    pages=(p+q)*.5-normal*27
    box('Ramp pages %d'%(i+1),(pages.x,pages.y,pages.z),(length-18,410,14),'GiantDeskPaper',rot)
    binding=(p+q)*.5+unreal.Vector(218,0,0)-normal*11
    box('Notebook binding %d'%(i+1),(binding.x,binding.y,binding.z),(length,16,24),'Gold',rot)
    # Stacked file boxes below the end platform; lids and front label panels.
    cy=ey+sign*220;cx=x+300
    for j in range(i+1):
        bottom=12+j*498;height=498 if j<i else 418
        box('File box %d.%d'%(i+1,j+1),(cx,cy,bottom+height/2),(1020,430,height),'OakLight')
        box('Box lid %d.%d'%(i+1,j+1),(cx,cy,bottom+height-12),(1040,440,24),'Trim')
        box('Box label %d.%d'%(i+1,j+1),(cx-514,cy,bottom+height*.55),(4,170,85),'Cream',collision=False)
    # A 26x11x2cm reference book lies on the final box as the landing.
    box('Landing pages %d'%(i+1),(cx,cy,z1-40),(1024,424,64),'GiantDeskPaper')
    box('Landing lower cover %d'%(i+1),(cx,cy,z1-76),(1040,440,8),'BookGreen')
    box('Landing top %d'%(i+1),(cx,cy,z1-4),(1040,440,8),'BookGreen')
    route.extend([(x,sy,z0),(x,ey,z1),(x,cy,z1),(x+600,cy,z1),(x+600,ey,z1)])
    ramps.append({'actor':ramp.get_path_name(),'start':[p.x,p.y,p.z],'end':[q.x,q.y,q.z],'width':440,'slope_degrees':math.degrees(math.atan2(498,1000))})
    # Small paper tabs on each platform edge visually mark the next flight.
    for xx in [x+120,x+300,x+480]:box('Route paper tab',(xx,cy,z1+1),(70,90,2),'Gold',collision=False)
route.extend([(13600,1580,3000),(14000,2300,3000),(14760,3600,3000)])
# Entry ramp joins the first notebook smoothly to the ordinary floor.
box('Entry notebook',(9900,1650,7),(440,300,10),'BookRed')
light('Clutter route warm pool',(10700,2300,3600),(10900,2300,1200),95000,4600,25,42)
# Move the original chair only along X (absolute placement, no accumulated drift).
for a in actors:
    if str(unreal.Name('CoherentStudyV06')) not in [str(t) for t in a.tags]:continue
    label=a.get_actor_label();p=a.get_actor_location()
    if label=='Study / Chair seat':p.x=18700
    elif label=='Study / Chair leg':p.x=17900 if p.x<18300 else 19500
    elif label=='Study / Chair back':p.x=19580
    elif label=='Study / Chair back cushion':p.x=19480
    else:continue
    a.set_actor_location(p,False,False)
# Eight normal large hardback volumes, 4-5cm each, lift the seated plush above the desk edge.
for i in range(8):
    z=1800+i*200;cx=18700+(25 if i%2 else -25)
    box('Seat book pages %d'%i,(cx,3600,z+100),(1160,1560,176),'GiantDeskPaper')
    for dz in [6,194]:box('Seat book cover %d'%i,(cx,3600,z+dz),(1200,1600,12),['BookRed','BookBlue','BookGreen'][i%3])
    box('Seat book spine %d'%i,(cx-594,3600,z+100),(12,1600,200),['BookRed','BookBlue','BookGreen'][i%3])
stage.set_editor_property('opponent_scale',10.0)
stage.set_editor_property('opponent_base_local',unreal.Vector(1700,0,(3550-2773)/2))
stage.set_editor_property('seat_pitch',20.0)
stage.set_actor_transform(stage.get_actor_transform(),False,True)
# Dim broad room fills; dedicated warm stage key/rim preserve the readable silhouette.
levels={'Study / Warm pendant':220000,'Study / Warm shelves fill':105000,'Study / Sideboard warm fill':60000,'Study / Player and cat soft fill':85000,'Study / Table stage spotlight':300000,'Study / Desk lamp warm pool':110000}
for a in actors:
    if a.get_actor_label() in levels:a.light_component.set_intensity(levels[a.get_actor_label()])
# The former large emissive windows overwhelmed the light hierarchy. Use warm opaque
# cream panes here; retain emissive pinhole, bulbs and gameplay landing markers.
for a in actors:
    if a.get_actor_label()=='Study / Warm window':a.static_mesh_component.set_material(0,mats['Cream'])
light('Stage overhead key',(15600,3200,7700),(16000,3600,3000),1500000,8200,22,34,True)
light('Cat warm key',(16200,1700,7000),(18700,3600,4750),850000,6400,22,36,True)
light('Cat amber rim',(20100,5100,7200),(18700,3600,4900),550000,4800,18,32)
assert all(r['slope_degrees']<30 and r['width']>=400 for r in ramps)
assert abs(route[-3][2]-3000)<1
assert abs(3550-150-3400)<1
assert L.save_current_level()
out=Path(unreal.Paths.project_saved_dir())/'Automation/StudyV07';out.mkdir(parents=True,exist_ok=True)
(out/'layout.json').write_text(json.dumps({'route':route,'ramps':ramps,'cat_body_bottom':3400,'cat_root':[18700,3600,3550],'seat_pitch':20,'note':'Six 26.47 degree continuous inclines. No flight-energy or character changes.'},indent=2),encoding='utf-8')
unreal.log('STUDY_ACCESS_V07_READY')
