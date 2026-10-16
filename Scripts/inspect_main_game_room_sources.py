"""Back up MainGame, inspect source room anchors without saving source maps."""
import unreal,json,hashlib,shutil,traceback
from pathlib import Path
from datetime import datetime
ROOT=Path(unreal.Paths.project_dir()).resolve();OUT=ROOT/'Saved/Validation/MainGameRooms';OUT.mkdir(parents=True,exist_ok=True)
MAP='/Game/MainGame/Maps/Lvl_MainGame';SOURCES=['/Game/PopupClose/Maps/Lvl_PopupClose1','/Game/Passages/Echo/Maps/Lvl_EchoMazeToCards_P1','/Game/WenSi/Maps/Lvl_WenSi_MirrorMuseum1']
ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem);levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
def xyz(p):return list(p.to_tuple())
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def area(a):return dict(tag=str(a.tag),kind=str(a.kind),title=str(a.title),origin=xyz(a.origin),entry=xyz(a.entry.translation),entry_rotation=str(a.entry.rotation.rotator()),exit=xyz(a.exit),exit_extent=xyz(a.exit_extent),route=[xyz(p) for p in a.map_route],bounds=[xyz(a.mining_bounds.min),xyz(a.mining_bounds.max)],activation=[xyz(a.activation_bounds.min),xyz(a.activation_bounds.max)],source=a.source_level.get_path_name() if a.source_level else None)
def snapshot():
    rows=[]
    for a in actors.get_all_level_actors():
        p,e=a.get_actor_bounds(False);row=dict(name=a.get_name(),label=a.get_actor_label(),type=a.get_class().get_name(),position=xyz(a.get_actor_location()),yaw=a.get_actor_rotation().yaw,bounds=[xyz(p),xyz(e)],level=a.get_level().get_path_name(),tags=[str(t) for t in a.tags])
        if isinstance(a,unreal.MiningExhibitHall):row['areas']=[area(x) for x in a.areas]
        if isinstance(a,unreal.MiningExternalRegion):row.update(rock=a.generated_rock.get_path_name(),rock_triangles=a.generated_rock.triangle_count,shell=a.generated_shell.get_path_name(),guides=[g.get_path_name() for g in a.generated_guide_layers],cutters=[dict(mesh=c.mesh.get_path_name(),position=xyz(c.local_transform.translation)) for c in a.cutters])
        if isinstance(a,unreal.StaticMeshActor):row.update(mesh=a.static_mesh_component.static_mesh.get_path_name() if a.static_mesh_component.static_mesh else None,collision=str(a.static_mesh_component.get_collision_enabled()))
        if any(s in row['type'] for s in ['Echo','CardRoomStage','MiningGiant','MiningRoomEscape','PopupClose','WenSiMirrorPuzzle']):
            row['properties']={}
            for k in ['room_in_same_level','walk_through_connections','destination_level','destination_transform','room_id','entry','exit','computer_entry','computer_checkpoint_position','checkpoint','tunnel_end','water_start_z','water_end_z','table_water_z','build_study_layout','area_managed','start_on_area_entry','interior_lighting','eruption_before_bomb_seconds','opponent_exit_local']:
                try:row['properties'][k]=str(a.get_editor_property(k))
                except Exception:pass
        rows.append(row)
    return rows
assert not ed.get_game_world()
backup=ROOT/'Saved/Backups'/('MainGame_RoomIntegration_'+datetime.now().strftime('%Y%m%d-%H%M%S'));backup.mkdir(parents=True)
for source in SOURCES:
    target=backup/'SourceMaps'/Path(source).name;target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(ROOT/'Content'/(source[6:]+'.umap'),target.with_suffix('.umap'))
# Preserve any work already present in the user's currently open source room.
assert levels.save_current_level()
assert levels.load_level(MAP);assert levels.save_current_level()
shutil.copy2(ROOT/'Content/MainGame/Maps/Lvl_MainGame.umap',backup/'Lvl_MainGame.umap');shutil.copytree(ROOT/'Content/MainGame/Materials/DirtTransition',backup/'DirtTransition');shutil.copytree(ROOT/'Config',backup/'Config')
hashes={p:sha(ROOT/'Content'/(p[6:]+'.umap')) for p in SOURCES+['/Game/StartRoom/Maps/Lvl_StartRoom','/Game/Campaign/Maps/Lvl_FullJourney']}
report=dict(backup=str(backup),source_hashes=hashes,main_game=snapshot(),sources={})
try:
    for source in SOURCES:
        assert levels.load_level(source);report['sources'][source]=snapshot()
    assert all(sha(ROOT/'Content'/(p[6:]+'.umap'))==d for p,d in hashes.items())
finally:
    assert levels.load_level(MAP);(OUT/'before.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8');unreal.log('MAIN_GAME_ROOMS_INSPECT '+json.dumps(dict(backup=str(backup),sources={k:len(v) for k,v in report['sources'].items()})))
