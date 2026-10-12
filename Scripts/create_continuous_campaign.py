"""Continuous campaign: cropped half-scale source mines, real doorways and buried clues."""
import bisect, hashlib, json, math, re, runpy, shutil
from datetime import datetime
from pathlib import Path
import unreal

root=Path(unreal.Paths.project_dir())
recipe=json.loads((root/'Config/CampaignLayout.json').read_text(encoding='utf-8'))
cmd=unreal.SystemLibrary.get_command_line()
override=re.search(r'-CampaignOutput=(/Game/[\w/]+)',cmd)
destination=override.group(1) if override else recipe['map']
assert destination.startswith('/Game/Campaign/Maps/')
lib=unreal.EditorAssetLibrary
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
if lib.does_asset_exist(destination):
    assert '-CampaignRebuild' in cmd.split(), 'Use -CampaignRebuild or a fresh -CampaignOutput.'
    backup=root/'Saved/CampaignBackups'/('continuous-'+datetime.now().strftime('%Y%m%d-%H%M%S'))
    backup.mkdir(parents=True)
    shutil.copy2(root/'Content'/(destination[6:]+'.umap'),backup/'Previous.umap')
    shutil.copy2(root/'Config/CampaignLayout.json',backup/'CampaignLayout.json')
    if (root/'Saved/FullJourneyManifest.json').exists():shutil.copy2(root/'Saved/FullJourneyManifest.json',backup/'PreviousManifest.json')
runpy.run_path(str(root/'Scripts/campaign_materials.py'))['ensure_blocker_materials']()
stain=runpy.run_path(str(root/'Scripts/campaign_materials.py'))['ensure_campaign_stain']()
assert unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
world.get_world_settings().set_editor_property('default_game_mode',unreal.MiningExhibitGameMode.static_class())
cube=unreal.load_asset('/Engine/BasicShapes/Cube')
floor=unreal.load_asset('/Game/Showcase/Materials/M_ContinuousFloor')
edge=unreal.load_asset('/Game/Showcase/Materials/M_ContinuousEdge')
door_material=runpy.run_path(str(root/'Scripts/campaign_materials.py'))['ensure_campaign_door']()
earth=unreal.load_asset('/Game/CollectionPrototype/Materials/M_CollectionEarth')
white_rock=runpy.run_path(str(root/'Scripts/campaign_materials.py'))['ensure_campaign_limestone']()
soil_actors=[]
points=json.loads((root/recipe['mine_centerlines']).read_text())[0]['points']
arcs=[0.0]
for a,b in zip(points,points[1:]):arcs.append(arcs[-1]+math.dist(a,b))
blocks=[b for b in json.loads((root/recipe['mine_blockers']).read_text())['objects'] if b['path_index']==0]
scale=recipe.get('mine_scale',.5)
areas=[unreal.MiningExperienceArea() for _ in recipe['stages']]
groups={}; mines={}; reports=[{} for _ in areas]; placements=[]
def V(x=0,y=0,z=0):return unreal.Vector(x,y,z)
def xyz(p):return [p.x,p.y,p.z]
def tag(a,name,i):
    a.set_actor_label(name);a.set_editor_property('tags',['BM_Campaign_'+recipe['stages'][i]['id']]);a.set_folder_path('Campaign/'+recipe['stages'][i]['id']);return a
def box(name,p,size,i,mat=floor,rotation=None):
    a=tag(actors.spawn_actor_from_class(unreal.StaticMeshActor,p,rotation or unreal.Rotator()),name,i)
    a.static_mesh_component.set_static_mesh(cube);a.static_mesh_component.set_material(0,mat);a.set_actor_scale3d(size/100);return a
def text(value,p,i,yaw=180):
    a=tag(actors.spawn_actor_from_class(unreal.TextRenderActor,p,unreal.Rotator(yaw=yaw)),value,i)
    a.text_render.set_text(value);a.text_render.set_world_size(30);a.text_render.set_text_render_color(unreal.Color(110,230,210,255));return a
def light(p,i):
    a=tag(actors.spawn_actor_from_class(unreal.PointLight,p),'Campaign_PathLight',i)
    c=a.point_light_component;c.set_editor_property('use_inverse_squared_falloff',False);c.set_editor_property('intensity_units',unreal.LightUnits.UNITLESS)
    c.set_editor_property('intensity',.55);c.set_editor_property('attenuation_radius',1000);c.set_editor_property('cast_shadows',False)
def soil(name,p,size,i,rotation=None,material=None,roughness=None):
    a=tag(actors.spawn_actor_from_class(unreal.GeneratedMiningRock,p,rotation or unreal.Rotator()),name,i)
    a.set_editor_property('rock_size',size)
    a.set_editor_property('rock_material',material or white_rock)
    assert unreal.MiningDesignerLibrary.build_campaign_soil(a,size,roughness or V(12,22,18))
    soil_actors.append(a)
    return a

def corridor(a,b,i,width=400,height=360):
    # Horizontal connection with a continuous floor. Door locations determine its height.
    assert abs(a.y-b.y)<1 and abs(a.z-b.z)<1
    mid=(a+b)/2; length=abs(b.x-a.x)
    soil('Campaign_TunnelGround',mid-V(0,0,500),V(length,width+1400,1000),i,roughness=V(0,18,3))
    soil('Campaign_TunnelRoof',mid+V(0,0,height+350),V(length,width+1400,700),i,roughness=V(0,22,55))
    for side in [-1,1]:soil('Campaign_TunnelSoilLeft' if side<0 else 'Campaign_TunnelSoilRight',mid+V(0,side*(width/2+350),height/2),V(length,700,height),i,roughness=V(0,70,18))
    light(mid+V(0,0,230),i)
def getone(group,cls):return next(a for a in group if isinstance(a,cls))
def copy(i,offset):
    spec=recipe['stages'][i];g=list(unreal.MiningDesignerLibrary.copy_experience_actors(spec['source'],offset,'BM_Campaign_'+spec['id']))
    assert g
    for a in g:
        if isinstance(a,unreal.PlayerStart):actors.destroy_actor(a)
    g=[a for a in g if not isinstance(a,unreal.PlayerStart)]
    groups[i]=g
    area=areas[i]
    for k,val in [('title',spec['title']),('instructions',spec['hint']),('tag','BM_Campaign_'+spec['id']),('kind',getattr(unreal.MiningExperienceKind,spec['kind'])),('origin',offset)]:area.set_editor_property(k,val)
    reports[i]=dict(id=spec['id'],source=spec['source'],source_sha256=hashlib.sha256((root/'Content'/(spec['source'][6:]+'.umap')).read_bytes()).hexdigest())
    return g
def make_mine(i,target,anchor_end=False):
    spec=recipe['stages'][i];g=copy(i,V());region=getone(g,unreal.MiningExternalRegion)
    rt=region.get_actor_transform();ct=region.cutters[0].local_transform
    def original(d):
        j=max(1,min(len(points)-1,bisect.bisect_left(arcs,d)));t=(d-arcs[j-1])/(arcs[j]-arcs[j-1]);p=[a+(b-a)*t for a,b in zip(points[j-1],points[j])]
        return unreal.MathLibrary.transform_location(rt,unreal.MathLibrary.transform_location(ct,V(p[0]*100,-p[1]*100,p[2]*100)))
    start,end=spec['start_arc_m'],spec['end_arc_m'];delta=original(end)-original(start)
    yaw=-math.degrees(math.atan2(delta.y,delta.x)); rotation=unreal.Rotator(yaw=yaw)
    frame=unreal.Transform(rotation=rotation,scale=V(scale,scale,scale))
    anchor=original(end if anchor_end else start)
    offset=target-unreal.MathLibrary.transform_location(frame,anchor)
    frame.translation=offset
    def at(d):return unreal.MathLibrary.transform_location(frame,original(d))
    snapshots=[(a,a.get_actor_transform()) for a in g]
    for a,t in snapshots:
        new=unreal.Transform(location=unreal.MathLibrary.transform_location(frame,t.translation),rotation=unreal.Rotator(yaw=t.rotation.rotator().yaw+yaw),scale=t.scale3d*scale)
        a.set_actor_transform(new,False,True)
    for a in g:
        if isinstance(a,unreal.DirectionalLight):actors.destroy_actor(a)
    if region.generated_shell:actors.destroy_actor(region.generated_shell)
    n=math.ceil((end-start)*scale/1.5);route=[at(start+(end-start)*j/n) for j in range(n+1)]
    low=V(min(p.x for p in route)-460,min(p.y for p in route)-1050,min(p.z for p in route)-480)
    high=V(max(p.x for p in route)+460,max(p.y for p in route)+1050,max(p.z for p in route)+650)
    entry_edge=V(low.x,route[0].y,route[0].z-120);exit_edge=V(high.x,route[-1].y,route[-1].z-120)
    openings=[]
    def opening(p,size,rot=None):openings.append(unreal.Transform(location=p,rotation=rot or unreal.Rotator(),scale=size))
    # Real open mouths through the cropped source rock; keep a solid, level floor.
    for p,e in [(route[0],entry_edge),(route[-1],exit_edge)]:
        opening(V((p.x+e.x)/2,p.y,p.z+60),V(abs(p.x-e.x)+360,400,360))
        # Replace the whole short junction envelope; its outer faces meet the original rock exactly.
        opening(V((p.x+e.x)/2,p.y,e.z+30),V(abs(p.x-e.x),1800,2060))
    caches=[]
    for cache in spec['collectibles']:
        k=cache['gap_after'];d=(blocks[k-1]['end_arc_m']+blocks[k]['start_arc_m'])/2
        p=at(d)
        front=unreal.MiningDesignerLibrary.find_campaign_wall(region.generated_rock,p+V(0,0,90),p+V(0,2000,90))-V(0,5,0)
        rot=unreal.Rotator(yaw=90)
        transform=unreal.Transform(location=front,rotation=rot)
        center=unreal.MathLibrary.transform_location(transform,V(550,0,0))
        opening(center,V(1120,560,460),rot)
        # The bank is flush with the surrounding rock, with an approach at tunnel floor level.
        bank_center=unreal.MathLibrary.transform_location(transform,V(560,0,0))
        rock=soil('Campaign_BuriedBank_%d'%cache['item_id'],bank_center,V(1120,600,510),i,rot,stain if cache['item_id']==2 else white_rock,V(7,9,9))
        # Use the cave's own floor here; a separate foundation cuts across the curved source floor.
        placement=unreal.MiningCollectionPlacement();placement.set_editor_property('item_id',cache['item_id']);placement.set_editor_property('area_tag','BM_Campaign_'+spec['id'])
        placement.set_editor_property('host_rock',rock);placement.set_editor_property('clue_transform',transform)
        placement.set_editor_property('transform',unreal.Transform(location=unreal.MathLibrary.transform_location(transform,V(780,0,-50)),rotation=rot))
        placements.append(placement);caches.append(dict(item_id=cache['item_id'],front=xyz(front),position=xyz(placement.transform.translation)))
        light(p+V(0,0,160),i)
    for a in g:
        if isinstance(a,unreal.GeneratedMiningRock):assert unreal.MiningDesignerLibrary.bake_campaign_rock(a,low,high,openings),a.get_actor_label()
    # Diggable soil encloses the cropped footprint; no static blue support blocks.
    center=(low+high)/2;size=high-low
    for z in [low.z-1500,high.z+1500]:soil('Campaign_OuterSoil',V(center.x,center.y,z),V(size.x,size.y+6000,3000),i,roughness=V())
    for y in [low.y-1500,high.y+1500]:soil('Campaign_OuterSoil',V(center.x,y,center.z),V(size.x,3000,size.z),i,roughness=V())
    for e,x in [(entry_edge,low.x-100),(exit_edge,high.x+100)]:
        if i==0 and x<low.x:continue # The closed starting wall already fills this entire end face.
        for y0,y1 in [(low.y,e.y-200),(e.y+200,high.y)]:
            if y1>y0:soil('Campaign_EntranceSoil',V(x,(y0+y1)/2,center.z),V(200,y1-y0,size.z),i,roughness=V())
        for z0,z1 in [(low.z,e.z),(e.z+360,high.z)]:
            if z1>z0:soil('Campaign_EntranceSoil',V(x,e.y,(z0+z1)/2),V(200,400,z1-z0),i,roughness=V())
    if i==0:
        soil('Campaign_StartClosedSoil',V(low.x-1500,center.y,center.z),V(3000,size.y+6000,size.z+6000),i,roughness=V())
    for p,e in [(route[0],entry_edge),(route[-1],exit_edge)]:corridor(e,V(p.x,p.y,p.z-120),i)
    for j in range(0,len(route),5):
        p=route[j];a=tag(actors.spawn_actor_from_class(unreal.StaticMeshActor,p),'Campaign_DescentGuide',i)
        a.static_mesh_component.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Sphere'));a.static_mesh_component.set_material(0,edge);a.static_mesh_component.set_collision_profile_name('NoCollision');a.set_actor_enable_collision(False);a.set_actor_scale3d(V(.10,.10,.10))
        if j%10==0:light(p+V(0,0,90),i)
    areas[i].set_editor_property('origin',region.get_actor_location());areas[i].set_editor_property('entry',unreal.Transform(location=entry_edge+V(160,0,96)))
    areas[i].set_editor_property('mining_bounds',unreal.Box(min=low-V(5,5,5),max=high+V(5,5,5)))
    areas[i].set_editor_property('map_route',[entry_edge+V(0,0,120)]+route+[exit_edge+V(0,0,120)])
    reports[i].update(scale=scale,playable_length_cm=(end-start)*scale*100,entry=xyz(entry_edge),exit=xyz(exit_edge),collectibles=caches)
    mines[i]=dict(entry=entry_edge,exit=exit_edge,route=route,bounds=(low,high));return mines[i]

# Anchor the existing echo/card/TV room at its original world coordinates.
mine2=make_mine(2,V(-1600,0,1020),True)
office_origin=mine2['entry']-V(1800,0,0)
office=copy(1,office_origin)
getone(office,unreal.PopupCloseStage).set_editor_property('area_managed',True)
getone(office,unreal.PopupCloseStage).set_editor_property('start_on_area_entry',True)
getone(office,unreal.PopupCloseOffice).set_editor_property('walk_through_connections',True)
areas[1].set_editor_property('entry',unreal.Transform(location=office_origin+V(-1020,0,96)))
mine1=make_mine(0,office_origin+V(-2250,0,120),True)
journey=copy(3,V())
echo=getone(journey,unreal.EchoPassageStage);echo.set_editor_property('walk_through_connections',True);echo.set_editor_property('room_in_same_level',True);echo.set_editor_property('destination_level',None)
escapes=[a for a in journey if isinstance(a,unreal.MiningRoomEscape)]
escape=escapes[0] if escapes else tag(actors.spawn_actor_from_class(unreal.MiningRoomEscape,V()),'Campaign_JourneyEscape',3)
escape.set_editor_property('area_managed',True);escape.set_editor_property('walk_through_connections',True)
escape.set_editor_property('card_stage',getone(journey,unreal.CardRoomStage));escape.set_editor_property('entrance',getone(journey,unreal.MiningGiantRoomEntrance))
areas[3].set_editor_property('entry',unreal.Transform(location=V(-650,0,996)))
mine3=make_mine(4,V(33800,11750,10020))
museum_origin=mine3['exit']+V(1545,0,0)
museum=copy(5,museum_origin)
puzzle=getone(museum,unreal.WenSiPuzzle);puzzle.set_editor_property('area_managed',True)
areas[5].set_editor_property('entry',unreal.Transform(location=museum_origin+V(-800,0,96)));puzzle.set_editor_property('area_player_start',areas[5].entry)
for a in museum:
    if re.fullmatch(r'(BackWall|EndWall)\d*',a.get_actor_label()):actors.destroy_actor(a)
for x in [-945,1510]:
    for y in [-405,405]:box('Campaign_MuseumDoorWall',museum_origin+V(x,y,250),V(20,410,500),5)
    box('Campaign_MuseumDoorHeader',museum_origin+V(x,0,430),V(20,400,140),5)
ending_origin=museum_origin+V(2300,0,0)
ending=copy(6,ending_origin);cell=getone(ending,unreal.MiningCellUniverse);cell.set_editor_property('area_managed',True)
for c in cell.get_components_by_class(unreal.StaticMeshComponent):
    if c.get_name().startswith('BackWall'):c.set_collision_profile_name('NoCollision');c.set_visibility(False)
areas[6].set_editor_property('entry',unreal.Transform(location=ending_origin+V(150,0,96)))

# Join real floors to real door openings, not teleport destinations.
corridor(mine1['exit'],office_origin-V(1200,0,0),0)
corridor(office_origin+V(1200,0,0),mine2['entry'],1)
corridor(mine2['exit'],V(-750,0,900),2)
corridor(V(32750,11750,9900),mine3['entry'],3,500)
corridor(mine3['exit'],museum_origin-V(945,0,0),4)
corridor(museum_origin+V(1510,0,0),ending_origin-V(60,0,0),5)
# Resolve overlaps against the cropped source geometry and each previously fitted soil section.
assert unreal.MiningDesignerLibrary.fit_campaign_soil(soil_actors,[a for i in [0,2,4] for a in groups[i] if isinstance(a,unreal.GeneratedMiningRock)])
thresholds=[office_origin+V(-1350,0,100),office_origin+V(1450,0,100),V(-900,0,1000),V(33100,11750,10000),museum_origin+V(-1095,0,100),museum_origin+V(1760,0,100)]
for i,p in enumerate(thresholds):
    areas[i].set_editor_property('exit',p);areas[i].set_editor_property('exit_extent',V(200,380,450))
    if i in [1,3,5]:
        door=p-V(170,0,-80)
        gate=box('Campaign_ProgressDoor',door,V(30,400,360),i,door_material)
        areas[i].set_editor_property('progress_gate',gate)
        text('COMPLETE ROOM TO OPEN',door+V(-25,-165,240),i)
    if i in [0,2,4]:
        r=list(areas[i].map_route);r.append(p);areas[i].set_editor_property('map_route',r)
    reports[i]['transition']=xyz(p)
for i,a in enumerate(areas):
    if i not in mines:
        mn=a.origin-V(1500,1200,500);mx=a.origin+V(1900,1200,1500)
        if i==3:mn=V(-1000,-600,-300);mx=V(33100,15000,14000)
        if i==6:mn=a.origin-V(200,30000,6000);mx=a.origin+V(200000,90000,90000)
        a.set_editor_property('mining_bounds',unreal.Box(min=mn,max=mx))
        a.set_editor_property('activation_bounds',unreal.Box(min=mn,max=mx))
    reports[i]['entry_spawn']=xyz(a.entry.translation)
placements.sort(key=lambda p:p.item_id)
collection=actors.spawn_actor_from_class(unreal.MiningCollectionField,V(0,-100000,0));collection.set_actor_label('CAMPAIGN_BuriedClues_2_1_1')
for k,val in [('area_managed',True),('shared_abilities',True),('show_exploration_hud',False),('use_authored_placements',True),('buried_with_clues',True),('authored_placements',placements)]:collection.set_editor_property(k,val)
key=unreal.Key();key.set_editor_property('key_name','I');collection.set_editor_property('catalogue_key',key)
hall=actors.spawn_actor_from_class(unreal.MiningExhibitHall,areas[0].entry.translation);hall.set_actor_label('CAMPAIGN_ContinuousFlow_EditAreasHere')
hall.set_editor_property('linear_campaign',True);hall.set_editor_property('continuous_campaign',True);hall.set_editor_property('areas',areas);hall.set_editor_property('lobby_entry',areas[0].entry)
actors.spawn_actor_from_class(unreal.PlayerStart,areas[0].entry.translation)
runpy.run_path(str(root/'Scripts/add_campaign_minerals.py'))['populate'](world,hall)
assert unreal.EditorLoadingAndSavingUtils.save_map(world,destination)
(root/'Saved/FullJourneyManifest.json').write_text(json.dumps(dict(map=destination,continuous=True,stages=reports,recipe=recipe),ensure_ascii=False,indent=2),encoding='utf-8')
unreal.log('CONTINUOUS_CAMPAIGN_SAVED '+destination)
unreal.SystemLibrary.quit_editor()
