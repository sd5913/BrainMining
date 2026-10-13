"""Append a new external excavation region and a room reserve to saved MainGame."""
import unreal,json,math,time,traceback,shutil
from pathlib import Path
ROOT=Path(unreal.Paths.project_dir());ART=ROOT/'Art/MainGame/TunnelCavernV2'
MAP='/Game/MainGame/Maps/Lvl_MainGame';OUT=ROOT/'Saved/Validation/cavern_creation.json'
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
state={'passed':False,'phase':'starting','started':time.monotonic()}
def checkpoint(**items):
    state.update(items);OUT.write_text(json.dumps(state,ensure_ascii=False,indent=2),encoding='utf-8');unreal.log('CAVERN '+json.dumps(items,ensure_ascii=False))
# Reuse the exact verified FBX import settings and closed-source-mesh repairs.
helper=(ROOT/'Scripts/create_main_game.py').read_text().split('\ntry:\n')[0]
ns={'__name__':'import_helpers'};exec(compile(helper,'create_main_game_import_helpers','exec'),ns)
ns['ART']=ART
def raw(p):return unreal.Vector(p[0]*100,-p[1]*100,p[2]*100)
def trace(world,a,b,ignore=None):
    return unreal.SystemLibrary.line_trace_single(world,a,b,unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,True,ignore or [],unreal.DrawDebugTrace.NONE,True)
def hit_actor(hit):return hit.to_tuple()[9] if hit and hit.to_tuple()[0] else None
def cutter(mesh,offset,region):
    result=unreal.MiningExternalCutter();result.set_editor_property('mesh',mesh)
    result.set_editor_property('local_transform',unreal.Transform(location=offset-region.get_actor_location()));return result
def make_region(label,tag,center,size):
    region=actors.spawn_actor_from_class(unreal.MiningExternalRegion,center);region.set_actor_label(label)
    region.set_editor_property('tags',[tag]);region.set_editor_property('rock_size',size)
    region.set_editor_property('shell_thickness',150.);region.set_editor_property('chunk_length',1600.)
    region.set_editor_property('chunk_triangle_budget',100000);region.set_editor_property('show_cutter_preview',False)
    region.set_editor_property('rock_material',rock_mat);region.set_editor_property('shell_material',rock_mat);region.set_editor_property('guide_material',guide_mat)
    region.set_folder_path('MainGame/Regions');return region
try:
    assert json.loads((ART/'progress.json').read_text())['passed']
    assert levels.load_level(MAP)
    world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    regions=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.MiningExternalRegion)
    assert not any('BM_MainGame_CavernV2' in [str(t) for t in r.tags] for r in regions),'Cavern V2 already exists; do not duplicate'
    previous=next(r for r in regions if 'BM_MainGame_V1' in [str(t) for t in r.tags])
    rotation=previous.get_actor_rotation();assert max(abs(rotation.pitch),abs(rotation.yaw),abs(rotation.roll))<.001
    old_cutter=previous.cutters[0];old_origin=previous.get_actor_location()+old_cutter.local_transform.translation
    rotation=old_cutter.local_transform.rotation.rotator();assert max(abs(rotation.pitch),abs(rotation.yaw),abs(rotation.roll))<.001
    assert old_cutter.local_transform.scale3d==unreal.Vector(1,1,1)
    backup=ROOT/'Saved/Backups/MainGame_BeforeCavernV2';backup.mkdir(parents=True,exist_ok=True)
    backup_map=backup/'Lvl_MainGame.umap'
    if not backup_map.exists():shutil.copy2(ROOT/'Content/MainGame/Maps/Lvl_MainGame.umap',backup_map)
    old_guide_paths=[g.get_path_name() for g in previous.generated_guide_layers]
    new_origin=old_origin+unreal.Vector(14800,0,-5680)
    checkpoint(phase='importing',previous_end_cm=[old_origin.x+6500,old_origin.y,old_origin.z+1000],reserved_length_m=18.)
    mesh=ns['import_fbx']('Tunnel_Cutter.fbx','/Game/MainGame/Models/CavernV2/Tunnel',1)[0]
    plugs=ns['import_fbx']('Tunnel_Blockers.fbx','/Game/MainGame/Models/CavernV2/Blockers',16)
    connector_mesh=ns['import_fbx']('FutureRoom_Connector.fbx','/Game/MainGame/Models/CavernV2/Connector',1)[0]
    rock_mat=unreal.load_asset('/Game/Campaign/Materials/M_CampaignLimestone');guide_mat=unreal.load_asset('/Game/Campaign/Materials/M_CampaignGuideRock')
    bounds=mesh.get_bounding_box()
    center=new_origin+unreal.Vector(0,(bounds.min.y+bounds.max.y)*.5,(bounds.min.z+bounds.max.z)*.5)
    size=unreal.Vector(12500,math.ceil((bounds.max.y-bounds.min.y+800)/100)*100,math.ceil((bounds.max.z-bounds.min.z+600)/100)*100)
    new=make_region('EDIT_MainGame_02_溶洞通路矿区','BM_MainGame_CavernV2',center,size)
    new.set_editor_property('cutters',[cutter(mesh,new_origin,new),cutter(connector_mesh,new_origin,new)])
    layers=[]
    for plug in plugs:
        layer=unreal.MiningExternalGuideLayer();layer.set_editor_property('mesh',plug);layers.append(layer)
    new.set_editor_property('guide_layers',layers);new.set_editor_property('guide_follow_cutter_transform',True);new.set_editor_property('guide_cutter_index',0)
    # The two rock bounds meet at x=8550. The common connector also cuts each shell.
    reserve=make_region('EDIT_MainGame_连接预留区_18m','BM_MainGame_FutureRoom',old_origin+unreal.Vector(7400,0,900),unreal.Vector(2300,2000,1800))
    reserve.set_editor_property('cutters',[cutter(connector_mesh,new_origin,reserve),cutter(old_cutter.mesh,old_origin,reserve),cutter(mesh,new_origin,reserve)])
    checkpoint(phase='generating_cavern')
    assert new.generate_region(),str(new.build_status)
    assert new.generate_guide_layers(),str(new.guide_build_status)
    checkpoint(phase='generating_reserve',new_chunks=len(new.generated_rock.chunks))
    assert reserve.generate_region(),str(reserve.build_status)
    # Remove casing seams on the old side without touching its 16 guide actors.
    previous.set_editor_property('cutters',list(previous.cutters)+[cutter(connector_mesh,new_origin,previous)])
    assert previous.generate_region(),str(previous.build_status)
    assert [g.get_path_name() for g in previous.generated_guide_layers]==old_guide_paths
    checkpoint(phase='checking_connections')
    all_guides=[g for r in [previous,new] for g in r.generated_guide_layers]
    floors=[]
    for i in range(61):
        x=6110+(8750-6110)*i/60
        # Trace from comfortably inside the headroom to the ground, across both seams.
        sample=old_origin+unreal.Vector(x,0,900-(x-6500)*.10)
        hit=trace(world,sample+unreal.Vector(0,0,200),sample-unreal.Vector(0,0,1600),all_guides)
        assert hit_actor(hit) in [r.generated_rock for r in [previous,new,reserve]]+[r.generated_shell for r in [previous,new,reserve]],(x,'Missing solid connector floor',str(hit_actor(hit)))
        location=hit.to_tuple()[5];floors.append([location.x,location.y,location.z])
    assert max(abs(b[2]-a[2]) for a,b in zip(floors[:-1],floors[1:]))<65.,'Large floor step at connector'
    for x in [6250,8550]:
        z=900-(x-6500)*.10
        for y in [-60,0,60]:
            hit=trace(world,old_origin+unreal.Vector(x-150,y,z),old_origin+unreal.Vector(x+150,y,z),all_guides)
            assert not hit_actor(hit),(x,y,'Shell/rock across connector')
    specs=json.loads((ART/'pillars.json').read_text());column_checks=[]
    for spec in specs:
        center=raw(spec['center_m'])+new_origin
        hits=[trace(world,center+unreal.Vector(sign*220,0,0),center,all_guides) for sign in [-1,1]]
        assert all(hit_actor(h)==new.generated_rock for h in hits),(spec['name'],'Column collision missing')
        column_checks.append(dict(name=spec['name'],world_center_cm=[center.x,center.y,center.z],collision=True))
    anchor=actors.spawn_actor_from_class(unreal.TargetPoint,old_origin+unreal.Vector(7400,0,900))
    anchor.set_actor_label('RESERVE_未来房间中心_18m连接空腔_勿放阻塞层');anchor.set_editor_property('tags',['BM_MainGame_FutureRoomAnchor']);anchor.set_folder_path('MainGame/FutureRooms')
    lights=[('Reserve',old_origin+unreal.Vector(7400,0,950))]
    for i,s in enumerate(json.loads((ART/'plan_summary.json').read_text())['cavern_centers_arc_m']):
        path=json.loads((ART/'centerlines.json').read_text())[0]
        import bisect
        index=bisect.bisect_left(path['arc'],s);position=raw(path['points'][index])+new_origin+unreal.Vector(0,200,120)
        lights.append((f'Cavern_{i+1}',position))
    for name,pos in lights:
        light=actors.spawn_actor_from_class(unreal.PointLight,pos);light.set_actor_label('MainGame_V2_'+name+'_Light');light.set_folder_path('MainGame/Lighting')
        component=light.get_component_by_class(unreal.PointLightComponent);component.set_editor_property('mobility',unreal.ComponentMobility.MOVABLE)
        component.set_editor_property('intensity',850.);component.set_editor_property('attenuation_radius',1800.);component.set_editor_property('cast_shadows',False)
    assert levels.save_current_level()
    assert unreal.EditorAssetLibrary.save_directory('/Game/MainGame/Models/CavernV2',only_if_is_dirty=True,recursive=True)
    checkpoint(passed=True,phase='complete',map=MAP,saved=True,new_origin_cm=[new_origin.x,new_origin.y,new_origin.z],
        new_region=new.get_path_name(),reserve_region=reserve.get_path_name(),old_region=previous.get_path_name(),
        new_rock_chunks=len(new.generated_rock.chunks),reserve_rock_chunks=len(reserve.generated_rock.chunks),
        new_guides=len(new.generated_guide_layers),new_guide_chunks=sum(len(g.chunks) for g in new.generated_guide_layers),
        previous_guides_preserved=True,column_checks=column_checks,connector_floors_cm=floors,seams_open=True,
        elapsed_seconds=time.monotonic()-state['started'])
except Exception:checkpoint(phase='failed',error=traceback.format_exc());raise
