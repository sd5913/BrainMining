"""V09: side spiral of books/cartons, leaving the entry's central reveal clear.
Run after V06 -> V07 -> V08. Safe to rerun; owns only route scenery.
"""
import unreal, math, json
from pathlib import Path
A=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
L=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert L.load_level('/Game/Passages/Echo/Maps/Lvl_EchoMazeToCards_P1')
actors=list(A.get_all_level_actors())
entrance=next(a for a in actors if isinstance(a,unreal.MiningGiantRoomEntrance))
assert unreal.Name('StudyV08LayoutApplied') in entrance.tags, 'Apply V08 first.'
TAG=unreal.Name('StudySideRouteV09')
old_prefixes=['Notebook','Ramp pages','Landing','File box','Box lid','Box label','Route paper','Entry notebook','Clutter route']
for a in actors:
    tags={str(t) for t in a.tags}
    old_route='StudyAccessV07' in tags and any(a.get_actor_label().startswith('Study V07 / '+s) for s in old_prefixes)
    if old_route or 'StudyPresentationV08' in tags or str(TAG) in tags:A.destroy_actor(a)
base='/Game/Rooms/CardDuel/Materials/'
names=['BookRed','BookBlue','BookGreen','Gold','Trim','Cream','GiantDeskPaper','GiantDeskPencil','GiantDeskInk','StudyCardboard','StudyPackingTape']
mats={k:unreal.load_asset(base+'M_'+k) for k in names}
assert all(mats.values())
def mark(a,label):
    a.tags=[TAG];a.set_actor_label('Study V09 / '+label);a.set_folder_path('GiantRoom/StudySideRouteV09');return a
def box(label,p,size,mat,rot=None,collision=True):
    a=mark(A.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(*p),rot or unreal.Rotator()),label)
    c=a.static_mesh_component;c.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Cube'));c.set_material(0,mats[mat])
    a.set_actor_scale3d(unreal.Vector(*(v/100 for v in size)))
    if not collision:a.set_actor_enable_collision(False);c.set_cast_shadow(False)
    return a
# The central archive pile supports the spiral from its inner edge, not from
# columns through the lower walking lanes. All dimensions share the existing 40x scale.
for i in range(6):
    z=12+i*498
    box('Archive carton %d'%i,(9400,-100,z+249),(1100,1100,498),'StudyCardboard')
    box('Carton lid %d'%i,(9400,-100,z+486),(1120,1120,24),'StudyCardboard')
    box('Carton tape %d'%i,(9400,462,z+249),(130,4,490),'StudyPackingTape',collision=False)
    box('Carton label %d'%i,(9280,465,z+249),(240,4,110),'Cream',collision=False)
corners=[(8600,700),(8600,-900),(10200,-900),(10200,700)]
route=[(7400,2200,12),(8000,700,12),(8600,700,12)]
ramps=[]
def bridge(label,pv,qv,kind,width=440):
    p=unreal.Vector(*pv);q=unreal.Vector(*qv);d=q-p;length=d.length();run=math.hypot(d.x,d.y)
    u=d.normal();side=unreal.Vector(-d.y/run,d.x/run,0)
    n=unreal.Vector(-d.x*d.z/(run*length),-d.y*d.z/(run*length),run/length)
    rot=unreal.MathLibrary.make_rot_from_x(d);mid=(p+q)*.5
    def at(t,s=0,h=0):
        v=p+u*t+side*s+n*h;return(v.x,v.y,v.z)
    material={'book':'BookRed','ruler':'GiantDeskPencil','cardboard':'StudyCardboard'}[kind]
    a=box(label,at(length*.5,0,-10),(length,width,20),material,rot)
    if kind=='book':
        box(label+' pages',at(length*.5,0,-30),(length-12,width-24,20),'GiantDeskPaper',rot)
        box(label+' binding',at(length*.5,-width*.5+8,-12),(length,16,28),'Gold',rot)
    elif kind=='ruler':
        for j in range(1,int(length/40)):
            major=j%5==0
            box(label+' tick',at(j*40,-width*.5+(55 if major else 30),2),(7,110 if major else 60,3),'GiantDeskInk',rot,False)
    else:
        box(label+' tape',at(length*.5,0,2),(length-10,100,3),'StudyPackingTape',rot,False)
        for t in [.33,.66]:box(label+' fold',at(length*t,0,2),(5,width-10,2),'Trim',rot,False)
    ramps.append({'actor':a.get_path_name(),'start':pv,'end':qv,'width':width,'slope_degrees':math.degrees(math.atan2(d.z,run))})
for i in range(7):
    x,y=corners[i%4];z=12+i*498
    mat=['BookBlue','BookGreen','BookRed'][i%3]
    box('Corner book pages %d'%i,(x,y,z-29),(424,424,42),'GiantDeskPaper')
    box('Corner book cover %d'%i,(x,y,z-4),(440,440,8),mat)
    if i:
        # A broad hardback extends inward onto the carton pile beneath each landing.
        cp=unreal.Vector(9400,-100,z-55);ep=unreal.Vector(x,y,z-55);d=ep-cp
        box('Supporting hardback %d'%i,((cp.x+ep.x)/2,(cp.y+ep.y)/2,z-70),(d.length()+420,430,30),mat,unreal.MathLibrary.make_rot_from_x(d))
    if i==6:break
    nx,ny=corners[(i+1)%4];dx=(nx-x)/1600;dy=(ny-y)/1600
    p=(x+dx*220,y+dy*220,z);q=(nx-dx*220,ny-dy*220,z+498)
    bridge('Spiral flight %d'%(i+1),p,q,['book','ruler','cardboard'][i%3])
    route.extend([p,q,(nx,ny,z+498)])
# A long wooden ruler rests on the final box-top book and the desk's front-left corner.
bridge('Desk ruler bridge',(10200,-900,3000),(11300,750,3000),'ruler')
route.extend([(11300,750,3000),(11400,1600,3000),(11400,3600,3000),(12060,3600,3000)])
# Floor markers remain flat and low so the view from the hole stays open.
for x,y,yaw in [(7300,2250,-60),(7900,950,-55),(8300,700,0)]:
    box('Paper route tab',(x,y,14),(150,85,4),'Gold',unreal.Rotator(yaw=yaw),False)
light=mark(A.spawn_actor_from_class(unreal.SpotLight,unreal.Vector(8900,-100,5200),unreal.Rotator(pitch=-90)),'Side route warm pool')
c=light.light_component;c.set_editor_property('mobility',unreal.ComponentMobility.MOVABLE);c.set_light_color(unreal.LinearColor(1,.64,.30,1));c.set_intensity(95000);c.set_editor_property('attenuation_radius',5600);c.set_editor_property('inner_cone_angle',20);c.set_editor_property('outer_cone_angle',36);c.set_editor_property('cast_shadows',False)
entrance.tags=list(entrance.tags) if unreal.Name('StudyV09LayoutApplied') in entrance.tags else list(entrance.tags)+[unreal.Name('StudyV09LayoutApplied')]
assert all(r['slope_degrees']<30 for r in ramps)
# Main entrance-to-stage sight corridor has no route scenery above floor markings.
for a in A.get_all_level_actors():
    if TAG in a.tags and isinstance(a,unreal.StaticMeshActor):
        center,extent=a.get_actor_bounds(False)
        assert center.y+extent.y<2300 or center.z+extent.z<20, a.get_actor_label()
out=Path(unreal.Paths.project_saved_dir())/'Automation/StudyV09';out.mkdir(parents=True,exist_ok=True)
(out/'layout.json').write_text(json.dumps({'version':'V09','route':route,'ramps':ramps,'clear_view_y':[2300,4900],'stair_core_bounds':[[8380,-1120,0],[10420,920,3000]],'note':'Six quarter-turn flights around cartons, then a ruler bridge; original movement and furniture preserved.'},indent=2),encoding='utf-8')
assert L.save_current_level()
unreal.log('STUDY_SIDE_ROUTE_V09_READY')
