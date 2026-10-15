"""Shorten the attached StartRoom shaft and seal the original region-one mouth.

Run with MainGame open and PIE stopped. Only the host map is saved; the source
room is moved by its streaming transform and reloaded, never edited in place.
"""
import hashlib,json,shutil,time,traceback
from datetime import datetime
from pathlib import Path
import unreal

ROOT=Path(unreal.Paths.project_dir()).resolve()
OUT=ROOT/'Saved/Validation/MainGameStartRoom'
MAP='/Game/MainGame/Maps/Lvl_MainGame'
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
editor=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
report={'passed':False}
def V(x=0,y=0,z=0):return unreal.Vector(x,y,z)
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def bounds(a,lo,hi):
    a.set_actor_location((V(*lo)+V(*hi))*.5,False,True)
    a.set_actor_scale3d((V(*hi)-V(*lo))/100.)
def named(label):return next(a for a in actors.get_all_level_actors() if a.get_actor_label()==label)
def tagged(a,label):
    a.set_actor_label(label)
    a.set_folder_path('MainGame/StartRoomConnection/MineJunction')
    a.set_editor_property('tags',['BM_Campaign_MainGame01','BM_MainGame_StartRoomConnection','BM_MainGame_StartRoomJunction'])
    return a
try:
    assert not editor.get_game_world(),'Stop PIE first.'
    assert editor.get_editor_world().get_path_name()==MAP+'.Lvl_MainGame'
    assert levels.set_current_level_by_name('Lvl_MainGame')
    assert levels.save_current_level()
    backup=ROOT/'Saved/Backups'/('MainGame_StartRoomSeal_'+datetime.now().strftime('%Y%m%d-%H%M%S'))
    backup.mkdir(parents=True)
    shutil.copy2(ROOT/'Content/MainGame/Maps/Lvl_MainGame.umap',backup/'Lvl_MainGame.umap')
    shutil.copy2(OUT/'integration.json',backup/'integration.json')
    sources=[ROOT/'Content/StartRoom/Maps/Lvl_StartRoom.umap',ROOT/'Content/Campaign/Maps/Lvl_FullJourney.umap']
    hashes=[sha(p) for p in sources]
    hall=next(a for a in actors.get_all_level_actors() if isinstance(a,unreal.MiningExhibitHall))
    field=next(a for a in actors.get_all_level_actors() if isinstance(a,unreal.MiningTreasureField))
    assert len(field.get_editor_property('authored_placements'))==125
    stream=hall.areas[0].source_level
    assert stream
    old=stream.get_editor_property('level_transform')
    assert abs(old.translation.x+10400)<1 and old.translation.z in [4400.,5200.]
    # Keep the measured mine landing at z=6000, lift the bedroom eight metres.
    stream.set_editor_property('level_transform',unreal.Transform(location=V(-10400,0,5200)))
    slabs={
        'LowerApproach_Floor':((-9210,-220,5130),(-8680,220,5200)),
        'LowerApproach_Roof':((-9210,-220,5500),(-9080,220,5570)),
        'Shaft_North':((-9210,150,5200),(-8610,220,6400)),
        'Shaft_South':((-9210,-220,5200),(-8610,-150,6400)),
        'Shaft_WestAboveDoor':((-9150,-150,5500),(-9080,150,6400)),
        'Shaft_EastBelowLanding':((-8680,-150,5200),(-8610,150,6000)),
    }
    for label,(lo,hi) in slabs.items():bounds(named('STARTROOM_'+label),lo,hi)
    bands=sorted([a for a in actors.get_all_level_actors() if a.get_actor_label().startswith('STARTROOM_Shaft_TimberBand')],key=lambda a:(a.get_actor_location().z,a.get_actor_location().y))
    assert len(bands)==6
    for a,z in zip(bands,[5530,5530,5910,5910,6300,6300]):
        p=a.get_actor_location();p.z=z;a.set_actor_location(p,False,True)
    named('STARTROOM_Shaft_BaseLight').set_actor_location(V(-8880,0,5450),False,True)
    named('STARTROOM_JetpackInstruction').set_actor_location(V(-8684,0,5385),False,True)
    entry=unreal.Transform(location=V(-10805,55,5302),rotation=unreal.Rotator(yaw=-30))
    areas=list(hall.areas);areas[0].set_editor_property('entry',entry)
    hall.set_editor_property('areas',areas);hall.set_editor_property('lobby_entry',entry)
    named('PLAYER_START_MainGame_StartRoom').set_actor_transform(entry,False,True)
    region=next(a for a in actors.get_all_level_actors() if a.actor_has_tag('BM_MainGame_V1') and isinstance(a,unreal.MiningExternalRegion))
    stone=region.get_editor_property('shell_material')
    guide=region.get_editor_property('guide_material')
    assert stone and guide
    # A full cross-section collar overlaps the existing unmineable shell. Its
    # only opening matches the enclosed 3m-wide, 3.3m-high upper passage.
    collar={
        'North':((-6500,150,-100),(-6320,2000,7500)),
        'South':((-6500,-3000,-100),(-6320,-150,7500)),
        'Below':((-6500,-150,-100),(-6320,150,6000)),
        'Above':((-6500,-150,6330),(-6320,150,7500)),
    }
    all_labels={a.get_actor_label():a for a in actors.get_all_level_actors()}
    for side,(lo,hi) in collar.items():
        label='STARTROOM_Junction_Bedrock_'+side
        a=all_labels.get(label) or tagged(actors.spawn_actor_from_class(unreal.StaticMeshActor,V()),label)
        a.static_mesh_component.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Cube'))
        a.static_mesh_component.set_material(0,stone)
        a.static_mesh_component.set_collision_profile_name('BlockAll')
        bounds(a,lo,hi)
        assert not isinstance(a,unreal.DiggableRock)
    label='STARTROOM_Junction_DiggableBlockage'
    plug=all_labels.get(label) or tagged(actors.spawn_actor_from_class(unreal.DiggableRock,V(-6350,0,6170)),label)
    plug.set_actor_location(V(-6350,0,6170),False,True)
    plug.set_editor_property('rock_size',V(180,340,360))
    plug.set_editor_property('rock_material',guide)
    plug.reset_rock()
    assert plug.triangle_count>0
    assert levels.save_current_level()
    # Reload applies the new streaming transform to source actors without
    # serializing transformed source-room instances back into their source map.
    assert levels.load_level(MAP)
    assert [sha(p) for p in sources]==hashes,'A reference map changed.'
    room=next(a for a in actors.get_all_level_actors() if isinstance(a,unreal.MiningStartRoom))
    assert abs(room.get_actor_location().z-5200)<1
    manifest=json.loads((OUT/'integration.json').read_text(encoding='utf-8'))
    manifest.update(source_offset=[-10400,0,5200],bedroom_spawn=[-10805,55,5302],shaft_base=[-8880,0,5200],rise_cm=800,
        junction_blockage=[-6350,0,6170],junction_blockage_size=[180,340,360],junction_shell_parts=4,refinement_backup=str(backup))
    (OUT/'integration.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2),encoding='utf-8')
    report.update(passed=True,rise_cm=800,shell_parts=4,blockage_size_cm=[180,340,360],source_maps_unchanged=True,saved_and_reloaded=True,backup=str(backup))
except Exception:
    report['error']=traceback.format_exc()
    raise
finally:
    (OUT/'refinement.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    unreal.log('STARTROOM_REFINEMENT '+json.dumps(report,ensure_ascii=False))
