"""V10: physical desk-lamp stage lighting and floor dressing.
Prerequisite V09. Re-running rebuilds this overlay only. The floor plush is a
static copy of the opponent's authored mesh parts, never a second game stage.
"""
import unreal, math, json
from pathlib import Path
A=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
L=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert L.load_level('/Game/Passages/Echo/Maps/Lvl_EchoMazeToCards_P1')
actors=list(A.get_all_level_actors())
entrance=next(a for a in actors if isinstance(a,unreal.MiningGiantRoomEntrance))
stage=next(a for a in actors if isinstance(a,unreal.CardRoomStage))
assert unreal.Name('StudyV09LayoutApplied') in entrance.tags, 'Apply V09 first.'
TAG=unreal.Name('StudyLampDecorV10')
entrance.tags=[t for t in entrance.tags if str(t)!='StudyV11LayoutApplied']
# Retire the displaced lamp, oversized plain rug, and unmotivated player-stage lights.
retire={'Study / Rug','Study / Rug border','Study / Table stage spotlight','Study / Landing light beam','Study V07 / Stage overhead key'}
for a in actors:
    if TAG in a.tags or unreal.Name('StudyLampBeamV11') in a.tags or a.get_actor_label() in retire or a.get_actor_label().startswith('Study / Desk lamp '):A.destroy_actor(a)
base='/Game/Rooms/CardDuel/Materials/'
mats={k:unreal.load_asset(base+'M_'+k) for k in ['Green','Gold','Dark','Cream','BookRed','BookBlue','GiantDeskPaper','GiantDeskPencil','GiantDeskInk','GiantDeskGlow']}
# Persistent material instances use the same color parameter as the live opponent.
def tint(name,color):
    asset='MI_StudyV10_'+name;m=unreal.load_asset(base+asset)
    if not m:m=unreal.AssetToolsHelpers.get_asset_tools().create_asset(asset,base.rstrip('/'),unreal.MaterialInstanceConstant,unreal.MaterialInstanceConstantFactoryNew())
    unreal.MaterialEditingLibrary.set_material_instance_parent(m,unreal.load_asset(base+'M_CardRoomSurface'))
    unreal.MaterialEditingLibrary.set_material_instance_vector_parameter_value(m,'Color',unreal.LinearColor(*color,1))
    unreal.EditorAssetLibrary.save_loaded_asset(m);mats[name]=m
for name,color in {'RugWine':(.24,.055,.033),'RugRust':(.47,.15,.055),'RugCream':(.67,.43,.20),'BagTeal':(.045,.17,.18),'BagEdge':(.023,.055,.055),'Leather':(.35,.13,.055),'ToyFur':(.73,.43,.24),'ToyCream':(.92,.75,.51),'ToyInk':(.025,.016,.013),'ToyNose':(.42,.09,.09),'ToyBow':(.22,.035,.025)}.items():tint(name,color)
def mark(a,label):
    a.tags=[TAG];a.set_actor_label('Study V10 / '+label);a.set_folder_path('GiantRoom/StudyLampDecorV10');return a
def xyz(v):return(v.x,v.y,v.z)
def shape(label,p,size,mat,mesh='Cube',rot=None,collision=True,shadow=True):
    a=mark(A.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(*p),rot or unreal.Rotator()),label)
    c=a.static_mesh_component;c.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/'+mesh));c.set_material(0,mats[mat]);a.set_actor_scale3d(unreal.Vector(*(v/100 for v in size)))
    if not collision:a.set_actor_enable_collision(False)
    c.set_cast_shadow(shadow);return a

def bar(label,p,q,width,mat,collision=False):
    p=unreal.Vector(*p);q=unreal.Vector(*q);d=q-p
    return shape(label,xyz((p+q)*.5),(width,width,d.length()),mat,'Cylinder',unreal.MathLibrary.make_rot_from_z(d),collision)
def spotlight(label,p,target,intensity,radius,inner,outer,shadow=True):
    a=mark(A.spawn_actor_from_class(unreal.SpotLight,unreal.Vector(*p),unreal.MathLibrary.find_look_at_rotation(unreal.Vector(*p),unreal.Vector(*target))),label)
    c=a.light_component;c.set_editor_property('mobility',unreal.ComponentMobility.MOVABLE);c.set_light_color(unreal.LinearColor(1,.70,.40,1));c.set_intensity(intensity);c.set_editor_property('attenuation_radius',radius);c.set_editor_property('inner_cone_angle',inner);c.set_editor_property('outer_cone_angle',outer);c.set_editor_property('cast_shadows',shadow);c.set_editor_property('source_radius',28)
    return a
# Same 15cm foot, 20cm shade and ~45cm height as the old lamp, now beside the arena.
foot=(12900,4900,3028);elbow=(13100,4660,4770)
opening=unreal.Vector(12400,4380,4400);target=unreal.Vector(12450,3600,3004)
axis=(opening-target).normal();rot=unreal.MathLibrary.make_rot_from_z(axis)
neck=opening+axis*400
shape('Lamp foot',foot,(600,600,56),'Green','Cylinder')
shape('Lamp foot trim',(12900,4900,3057),(490,490,12),'Gold','Cylinder',collision=False)
bar('Lamp lower arm',(12900,4900,3060),elbow,40,'Green',True)
bar('Lamp upper arm',elbow,xyz(neck),40,'Green',True)
for p in [(12900,4900,3100),elbow,xyz(neck)]:shape('Lamp joint',p,(120,120,120),'Gold','Sphere')
shape('Lamp shade',xyz(opening+axis*200),(800,800,400),'Green','Cone',rot)
shape('Lamp rim',xyz(opening+axis*3),(808,808,20),'Gold','Cylinder',rot,False)
shape('Lamp diffuser',xyz(opening-axis*10),(750,750,12),'GiantDeskGlow','Cylinder',rot,False,False)
source=opening-axis*28
spotlight('Lamp stage source',xyz(source),xyz(target),650000,5500,26,48)
# Small practical glow illuminates the shade/hinge; the broad stage pool comes from its mouth.
a=mark(A.spawn_actor_from_class(unreal.PointLight,opening-axis*40),'Lamp bulb bounce');c=a.light_component;c.set_editor_property('mobility',unreal.ComponentMobility.MOVABLE);c.set_light_color(unreal.LinearColor(1,.62,.28,1));c.set_intensity(6500);c.set_editor_property('attenuation_radius',650);c.set_editor_property('cast_shadows',False)
# A restrained cord leads from the lamp toward the desk's back edge.
cord=[(12900,5150,3015),(13300,5300,3015),(13800,5450,3015),(14380,5500,3015),(14500,5500,2600),(14500,5500,500)]
for p,q in zip(cord,cord[1:]):bar('Lamp cord',p,q,12,'Dark')
# Thin woven runner frames the entrance; its top is a normal small step (12cm world).
shape('Rug backing',(8820,3600,18),(3920,3020,12),'RugWine',collision=True)
shape('Rug inset',(8820,3600,25),(3670,2770,2),'RugRust',collision=False)
shape('Rug field',(8820,3600,27),(3420,2520,2),'RugWine',collision=False)
for x in [7130,10510]:shape('Rug border seam',(x,3600,29),(22,2440,2),'RugCream',collision=False)
for y in [2360,4840]:shape('Rug border seam',(8820,y,29),(3360,22,2),'RugCream',collision=False)
for x in range(7380,10400,360):
    for y in [2200,5000]:shape('Rug woven diamond',(x,y,29),(120,120,2),'RugCream',rot=unreal.Rotator(yaw=45),collision=False)
for x in [7820,8820,9820]:
    shape('Rug centre diamond',(x,3600,29),(490,490,2),'RugRust',rot=unreal.Rotator(yaw=45),collision=False)
    shape('Rug centre inset',(x,3600,31),(360,360,2),'RugCream',rot=unreal.Rotator(yaw=45),collision=False)
    shape('Rug centre seed',(x,3600,33),(235,235,2),'RugWine',rot=unreal.Rotator(yaw=45),collision=False)
for x in [6820,10820]:
    for y in range(2200,5050,90):shape('Rug fringe',(x,y,17),(120,30,6),'RugCream',collision=False)
# Floor plush: copy the 18 actual authored parts, including V08 muzzle and smile.
# One ordinary static prop, not an actor with cards, rewards or battle state.
origin=unreal.Vector(9950,6550,117);plush_scale=14.0;plush_yaw=35.0
names={'VelvetBody','RoundHead','LeftEar','RightEar','LeftEye','RightEye','Nose','LeftMitten','RightMitten','BowTie','Mouth','LeftBrow','RightBrow','LeftMuzzle','RightMuzzle','SmileLeft','SmileRight','NoseStem'}
cloned=[]
for part in stage.get_components_by_class(unreal.StaticMeshComponent):
    name=part.get_name()
    if name not in names:continue
    loc=part.get_editor_property('relative_location');parent=part.get_attach_parent()
    if parent and parent.get_name()=='OpponentHeadAim':loc+=parent.get_editor_property('relative_location')
    angle=math.radians(plush_yaw);p=origin+unreal.Vector(loc.x*math.cos(angle)-loc.y*math.sin(angle),loc.x*math.sin(angle)+loc.y*math.cos(angle),loc.z)*plush_scale
    r=unreal.MathLibrary.compose_rotators(part.get_editor_property('relative_rotation'),unreal.Rotator(yaw=plush_yaw))
    a=mark(A.spawn_actor_from_class(unreal.StaticMeshActor,p,r),'Floor plush '+name);c=a.static_mesh_component;c.set_static_mesh(part.static_mesh)
    material='ToyCream' if 'Muzzle' in name else 'ToyNose' if name=='Nose' else 'ToyBow' if name=='BowTie' else 'ToyInk' if name in {'LeftEye','RightEye','Mouth','LeftBrow','RightBrow','SmileLeft','SmileRight','NoseStem'} else 'ToyFur'
    c.set_material(0,mats[material]);a.set_actor_scale3d(part.get_editor_property('relative_scale3d')*plush_scale)
    a.set_actor_enable_collision(name in {'VelvetBody','RoundHead','LeftMitten','RightMitten'});cloned.append(name)
assert set(cloned)==names, cloned
# A 45cm canvas schoolbag: curved body, front pocket, zipper, leather tabs and straps.
bo=(9680,9100,12);yaw=18.0;ang=math.radians(yaw)
def bp(p):return(bo[0]+p[0]*math.cos(ang)-p[1]*math.sin(ang),bo[1]+p[0]*math.sin(ang)+p[1]*math.cos(ang),bo[2]+p[2])
def bag(label,p,size,mat,mesh='Cube',rot=None):
    return shape('Backpack '+label,bp(p),size,mat,mesh,unreal.MathLibrary.compose_rotators(rot or unreal.Rotator(),unreal.Rotator(yaw=yaw)))
bag('body',(0,0,920),(860,1420,1840),'BagTeal','Sphere')
bag('flat lower',(0,0,530),(690,1230,1010),'BagTeal')
bag('base',(0,0,70),(720,1280,140),'Leather')
bag('top flap',(-170,0,1440),(680,1370,580),'BagTeal','Sphere')
bag('pocket piping',(-396,0,665),(170,1100,750),'BagEdge')
bag('front pocket',(-485,0,665),(42,1030,680),'BagTeal')
bag('zipper',(-510,0,975),(12,1000,24),'Gold')
bag('zipper pull',(-530,-400,920),(35,40,100),'Leather',rot=unreal.Rotator(roll=-12))
bag('label patch',(-511,0,700),(12,290,175),'Cream')
bag('label paw',(-523,0,680),(12,95,65),'Leather','Sphere')
for y,z in [(-60,742),(-20,760),(20,760),(60,742)]:bag('label paw toe',(-523,y,z),(12,28,34),'Leather','Sphere')
for y in [-400,400]:
    bag('buckle tab',(-390,y,1220),(70,110,310),'Leather')
    bag('buckle',(-435,y,1210),(35,150,110),'Gold')
# Top handle and two shoulder straps describe the familiar silhouette.
for y in [-180,180]:bar('Backpack handle',bp((0,y,1730)),bp((0,y,1980)),65,'Leather')
bar('Backpack handle',bp((0,-180,1980)),bp((0,180,1980)),65,'Leather')
for sy in [-1,1]:
    pts=[(280,sy*390,1500),(650,sy*490,1100),(760,sy*520,450),(580,sy*420,100),(300,sy*330,210)]
    for p,q in zip(pts,pts[1:]):bar('Backpack shoulder strap',bp(p),bp(q),100,'BagEdge')
# A closed notebook and pencil at the rug's outer edge give low, familiar scale cues.
shape('Floor notebook pages',(8750,5530,68),(840,570,96),'GiantDeskPaper')
for z in [16,120]:shape('Floor notebook cover',(8750,5530,z),(880,610,8),'BookBlue')
shape('Floor notebook spine',(8318,5530,68),(16,610,112),'BookBlue')
bar('Floor pencil',(7550,5300,35),(8100,5680,35),30,'GiantDeskPencil')
# Soft floor pool is sourced from the existing first ceiling pendant, not another floating lamp.
spotlight('Pendant floor spill',(11700,3600,8580),(8700,4800,20),650000,11500,18,39,True)
# Validate scenery ownership and keep raised floor objects outside the central reveal and walk path.
checks=[]
route=json.loads((Path(unreal.Paths.project_saved_dir())/'Automation/StudyV09/layout.json').read_text(encoding='utf-8'))['route']
for a in A.get_all_level_actors():
    if TAG not in a.tags or not isinstance(a,unreal.StaticMeshActor):continue
    label=a.get_actor_label()
    if not any(k in label for k in ['Floor plush','Backpack','Floor notebook','Floor pencil']):continue
    p,e=a.get_actor_bounds(False)
    assert p.y-e.y>4900, 'Central reveal blocked by '+label
    for q in route:
        assert not (abs(q[0]-p.x)<e.x+60 and abs(q[1]-p.y)<e.y+60 and q[2]<p.z+e.z+100), 'Walking lane blocked by '+label
    checks.append(label)
assert (source-opening).length()<40
assert not any(a.get_actor_label() in retire for a in A.get_all_level_actors())
marker=unreal.Name('StudyV10LayoutApplied')
if marker not in entrance.tags:entrance.tags=list(entrance.tags)+[marker]
assert L.save_current_level()
out=Path(unreal.Paths.project_saved_dir())/'Automation/StudyV10';out.mkdir(parents=True,exist_ok=True)
(out/'layout.json').write_text(json.dumps({'version':'V10','lamp_source':xyz(source),'lamp_opening':xyz(opening),'lamp_target':xyz(target),'floor_plush_origin':xyz(origin),'floor_plush_scale':plush_scale,'copied_plush_parts':cloned,'decor_clearance_checks':len(checks),'backpack_origin':bo,'rug_bounds':[[6860,2090,12],[10780,5110,34]],'route':'StudyV09/layout.json','note':'Static same-design floor plush. No battle or pawn changes.'},indent=2),encoding='utf-8')
unreal.log('STUDY_LAMP_DECOR_V10_READY')
