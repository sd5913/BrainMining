"""Attach the existing StartRoom source level and a jetpack-only shaft to MainGame.

Run in the full editor, with MainGame open and PIE stopped. Source maps are read-only.
The source room's end cap and global exposure are suppressed by the existing campaign
source-level overrides. All room pickups, furniture, lights and diggable exit survive.
"""
import hashlib,json,shutil,time,traceback
from datetime import datetime
from pathlib import Path
import unreal

ROOT=Path(unreal.Paths.project_dir()).resolve()
MAP='/Game/MainGame/Maps/Lvl_MainGame'
SOURCE='/Game/StartRoom/Maps/Lvl_StartRoom'
OUT=ROOT/'Saved/Validation/MainGameStartRoom/integration.json'
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
editor=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
result={'passed':False,'started':time.monotonic()}
def V(x=0,y=0,z=0):return unreal.Vector(x,y,z)
def xyz(v):return list(v.to_tuple())
def digest(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def tag(a,label,folder='Shaft'):
    a.set_actor_label(label)
    a.set_folder_path('MainGame/StartRoomConnection/'+folder)
    a.set_editor_property('tags',['BM_Campaign_MainGame01','BM_MainGame_StartRoomConnection'])
    return a
def box(label,center,size,material,collision=True):
    a=tag(actors.spawn_actor_from_class(unreal.StaticMeshActor,center),label)
    a.static_mesh_component.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Cube'))
    a.static_mesh_component.set_material(0,material)
    a.static_mesh_component.set_collision_profile_name('BlockAll' if collision else 'NoCollision')
    a.set_actor_scale3d(size/100.)
    return a
def slab(label,lo,hi,material):return box(label,(lo+hi)*.5,hi-lo,material)
def lamp(label,p,color):
    a=tag(actors.spawn_actor_from_class(unreal.PointLight,p),label,'Lighting')
    c=a.point_light_component
    c.set_editor_property('mobility',unreal.ComponentMobility.MOVABLE)
    c.set_editor_property('use_inverse_squared_falloff',False)
    c.set_editor_property('intensity_units',unreal.LightUnits.UNITLESS)
    c.set_editor_property('intensity',2.)
    c.set_editor_property('attenuation_radius',750.)
    c.set_editor_property('cast_shadows',False)
    c.set_light_color(color)
    return a
def sign(label,words,p,yaw=180):
    a=tag(actors.spawn_actor_from_class(unreal.TextRenderActor,p,unreal.Rotator(yaw=yaw)),label,'Signs')
    a.text_render.set_text(words)
    a.text_render.set_world_size(18.)
    a.text_render.set_horizontal_alignment(unreal.HorizTextAligment.EHTA_CENTER)
    a.text_render.set_text_render_color(unreal.Color(145,225,240,255))
    return a
try:
    assert not editor.get_game_world(),'Stop PIE before integration.'
    world=editor.get_editor_world()
    assert world.get_path_name()==MAP+'.Lvl_MainGame','Open MainGame first.'
    assert levels.save_current_level()
    hall=next(a for a in actors.get_all_level_actors() if isinstance(a,unreal.MiningExhibitHall))
    assert len(hall.areas)==4 and not hall.areas[0].source_level,'StartRoom already attached or unexpected area setup.'
    assert not any(a.actor_has_tag('BM_MainGame_StartRoomConnection') for a in actors.get_all_level_actors())
    source_file=ROOT/'Content/StartRoom/Maps/Lvl_StartRoom.umap'
    full_file=ROOT/'Content/Campaign/Maps/Lvl_FullJourney.umap'
    source_hash=digest(source_file);full_hash=digest(full_file)
    backup=ROOT/'Saved/Backups'/('MainGame_StartRoomAttach_'+datetime.now().strftime('%Y%m%d-%H%M%S'))
    backup.mkdir(parents=True)
    shutil.copy2(ROOT/'Content/MainGame/Maps/Lvl_MainGame.umap',backup/'Lvl_MainGame.umap')
    field=next(a for a in actors.get_all_level_actors() if isinstance(a,unreal.MiningTreasureField))
    before_count=len(field.get_editor_property('authored_placements'))
    original_entry=hall.areas[0].entry
    # A 16 m rise is below the unupgraded jetpack's five-second heat budget.
    # Upper landing height is selected from the measured existing mine mouth.
    upper=6000.
    rise=1600.
    lower=upper-rise
    offset=V(-10400,0,lower)
    shaft_x=-8880.
    left=shaft_x-200.;right=shaft_x+200.
    half_width=150.;thickness=70.;headroom=330.
    source_end=offset.x+1190.
    mouth_x=-6170.
    existing=set(actors.get_all_level_actors())
    stream=unreal.EditorLevelUtils.add_level_to_world_with_transform(world,SOURCE,unreal.LevelStreamingAlwaysLoaded,unreal.Transform(location=offset))
    assert stream,'Failed to add StartRoom source level.'
    imported=[a for a in actors.get_all_level_actors() if a not in existing]
    assert len([a for a in imported if isinstance(a,unreal.MiningStarterPickup)])==2
    suppress=[a.get_name() for a in imported if a.get_actor_label() in ['FutureConnectionCap','StartRoom_Exposure']]
    assert len(suppress)==2,'Expected source end cap and exposure volume.'
    # Keep the host persistent level current; the source level must never be saved here.
    assert levels.set_current_level_by_name('Lvl_MainGame')
    stone=unreal.load_asset('/Game/Campaign/Materials/M_CampaignLimestone')
    timber=unreal.load_asset('/Game/Rooms/CardDuel/Materials/M_Oak')
    metal=unreal.load_asset('/Game/Rooms/CardDuel/Materials/M_Dark')
    assert stone and timber and metal
    # Lower horizontal throat, continuous with the existing room tunnel.
    slab('STARTROOM_LowerApproach_Floor',V(source_end,-220,lower-70),V(right,220,lower),stone)
    slab('STARTROOM_LowerApproach_Roof',V(source_end,-220,lower+300),V(left,220,lower+370),stone)
    # Shaft side walls are uninterrupted: no stairs, ladders or collision footholds.
    slab('STARTROOM_Shaft_North',V(source_end,half_width,lower),V(right+thickness,half_width+thickness,upper+headroom+70),stone)
    slab('STARTROOM_Shaft_South',V(source_end,-half_width-thickness,lower),V(right+thickness,-half_width,upper+headroom+70),stone)
    slab('STARTROOM_Shaft_WestAboveDoor',V(left-thickness,-half_width,lower+300),V(left,half_width,upper+headroom+70),stone)
    slab('STARTROOM_Shaft_EastBelowLanding',V(right,-half_width,lower),V(right+thickness,half_width,upper),stone)
    slab('STARTROOM_Shaft_TopRoof',V(left-thickness,-220,upper+headroom),V(right+thickness,220,upper+headroom+70),stone)
    # Upper passage opens eastward into the original first mining region.
    slab('STARTROOM_UpperPassage_Floor',V(right,-220,upper-70),V(mouth_x,220,upper),stone)
    slab('STARTROOM_UpperPassage_Roof',V(right,-220,upper+headroom),V(mouth_x,220,upper+headroom+70),stone)
    slab('STARTROOM_UpperPassage_North',V(right,150,upper),V(mouth_x,220,upper+headroom),stone)
    slab('STARTROOM_UpperPassage_South',V(right,-220,upper),V(mouth_x,-150,upper+headroom),stone)
    # Non-colliding timber collars make the enclosed shaft and its landing readable.
    for z in [lower+330,lower+820,upper+300]:
        for side in [-1,1]:box('STARTROOM_Shaft_TimberBand',V(shaft_x,side*147,z),V(400,12,18),timber,False)
    for x in [-8500,-7600,-6750]:
        for side in [-1,1]:box('STARTROOM_UpperPassage_Frame',V(x,side*145,upper+155),V(16,14,310),timber,False)
        box('STARTROOM_UpperPassage_Lintel',V(x,0,upper+315),V(20,300,18),timber,False)
    lamp('STARTROOM_Shaft_BaseLight',V(shaft_x,0,lower+250),unreal.LinearColor(.3,.65,1.,1))
    lamp('STARTROOM_Shaft_TopLight',V(shaft_x,0,upper+220),unreal.LinearColor(.4,.8,1.,1))
    lamp('STARTROOM_MineConnection_Light',V(-7100,0,upper+240),unreal.LinearColor(1.,.7,.4,1))
    sign('STARTROOM_JetpackInstruction','HOLD SPACE\nJETPACK ASCENT',V(right-4,0,lower+185))
    sign('STARTROOM_RegionOneSign','REGION 01  >',V(-6790,144,upper+190),yaw=-90)
    areas=list(hall.areas)
    entry=unreal.Transform(location=offset+V(-405,55,102),rotation=unreal.Rotator(yaw=-30))
    areas[0].set_editor_property('entry',entry)
    areas[0].set_editor_property('source_level',stream)
    areas[0].set_editor_property('suppressed_source_actors',suppress)
    areas[0].set_editor_property('instructions','先瞄准装备按 E 拾取镐子和喷气背包，挖开卧室出口；到竖井按住空格上升，进入区域一。沿碎晶挖矿，按住 Tab 升级。')
    b=areas[0].mining_bounds
    areas[0].set_editor_property('mining_bounds',unreal.Box(min=V(min(b.min.x,offset.x-500),min(b.min.y,-450),min(b.min.z,lower-100)),max=b.max))
    hall.set_editor_property('areas',areas)
    hall.set_editor_property('lobby_entry',entry)
    starts=[a for a in existing if isinstance(a,unreal.PlayerStart)]
    assert len(starts)==1
    starts[0].set_actor_transform(entry,False,True)
    starts[0].set_actor_label('PLAYER_START_MainGame_StartRoom')
    assert len(field.get_editor_property('authored_placements'))==before_count==125
    assert levels.save_current_level()
    assert digest(source_file)==source_hash and digest(full_file)==full_hash,'Source maps changed.'
    result.update(passed=True,source_level=SOURCE,source_actors=len(imported),suppressed=suppress,source_offset=xyz(offset),
        bedroom_spawn=xyz(entry.translation),original_mine_entry=xyz(original_entry.translation),shaft_base=xyz(V(shaft_x,0,lower)),
        shaft_landing=xyz(V(shaft_x,0,upper)),upper_passage_end=xyz(V(mouth_x,0,upper)),rise_cm=rise,
        source_sha256=source_hash,fulljourney_sha256=full_hash,minerals_preserved=before_count,backup=str(backup),saved=True)
except Exception:
    result['error']=traceback.format_exc()
    raise
finally:
    result['elapsed_seconds']=time.monotonic()-result.pop('started')
    OUT.parent.mkdir(parents=True,exist_ok=True)
    OUT.write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
    unreal.log('MAIN_GAME_START_ROOM '+json.dumps(result,ensure_ascii=False))
