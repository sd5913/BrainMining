"""MainGame-only volume ownership, tall parkour chambers and rewarding dead ends."""
import unreal,json,sys,traceback,hashlib,shutil,time,math
from pathlib import Path
from datetime import datetime
ROOT=Path(unreal.Paths.project_dir()).resolve();sys.path.insert(0,str(ROOT/'Scripts'))
import campaign_minerals_api as api
from main_game_routes_v6_shape import world as warp
from rebuild_main_game_v6_minerals import rebuild
OUT=ROOT/'Saved/Validation/MainGameRoutesV6';OUT.mkdir(parents=True,exist_ok=True);ART=ROOT/'Art/MainGame/RoutesV6'
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem);actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
lib=unreal.MiningDesignerLibrary;NEVER=unreal.PropertyAccessChangeNotifyMode.NEVER
report={'passed':False,'steps':[]};started=time.monotonic()
def log(step,**values):
    report['steps'].append(dict(step=step,elapsed=time.monotonic()-started,**values));(OUT/'authoring.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8');unreal.log('V6 '+step)
def setp(o,k,v):o.set_editor_property(k,v,notify_mode=NEVER)
def deform(actor):
    source=lib.get_campaign_rock_source(actor);assert source,actor.get_actor_label()
    points=unreal.GeometryScript_List.convert_vector_list_to_array(unreal.GeometryScript_MeshQueries.get_all_vertex_positions(source,False)[1]);transform=actor.get_actor_transform();moved=0
    world_points=[unreal.MathLibrary.transform_location(transform,p) for p in points]
    bottom=min(p.z for p in world_points);top=max(p.z for p in world_points)
    envelope=(max(p.x for p in world_points)-min(p.x for p in world_points))>12000
    for i,p in enumerate(points):
        q=unreal.MathLibrary.transform_location(transform,p);w=unreal.Vector(*warp(q.to_tuple()))
        # The coarse outer box has no vertices above individual chambers.
        # Enlarge both sides of its shell so the taller cave stays fully enclosed.
        if envelope and q.z>top-120:w.z=q.z+2200
        if envelope and q.z<bottom+120:w.z=q.z-2200
        if q.distance(w)<.0001:continue
        local=unreal.MathLibrary.inverse_transform_location(transform,w)
        _,valid=unreal.GeometryScript_MeshEdits.set_vertex_position(source,i,local,True);assert valid;moved+=1
    if not moved:return 0
    assert unreal.GeometryScript_MeshQueries.get_is_closed_mesh(source)
    if isinstance(actor,unreal.GeneratedMiningRock):
        if not lib.set_campaign_rock_source(actor,source):
            verts=unreal.GeometryScript_List.convert_vector_list_to_array(unreal.GeometryScript_MeshQueries.get_all_vertex_positions(source,False)[1])
            triangles=unreal.GeometryScript_List.convert_triangle_list_to_array(unreal.GeometryScript_MeshQueries.get_all_triangle_indices(source,False)[1])
            for name,positions in [('failed-deformed',verts),('failed-original',points)]:
                lines=[]
                for v in positions:
                    p=unreal.MathLibrary.transform_location(transform,v);lines.append('v %.8f %.8f %.8f'%(p.x/100,-p.y/100,p.z/100))
                for t in triangles:lines.append('f %d %d %d'%(t.x+1,t.z+1,t.y+1))
                (OUT/(name+'.obj')).write_text('\n'.join(lines))
            raise AssertionError(actor.get_actor_label())
    else:
        component=actor.get_component_by_class(unreal.DynamicMeshComponent);assert component
        component.get_dynamic_mesh().reset();unreal.GeometryScript_MeshEdits.append_mesh(component.get_dynamic_mesh(),source,unreal.Transform())
        unreal.GeometryScript_Normals.recompute_normals(component.get_dynamic_mesh(),unreal.GeometryScriptCalculateNormalsOptions());component.update_collision(False)
    return moved

try:
    assert not ed.get_game_world();assert levels.load_level('/Game/MainGame/Maps/Lvl_MainGame');assert levels.set_current_level_by_name('Lvl_MainGame')
    world,hall,actors=api._context();assert not hall.actor_has_tag('BM_RoutesV6'),'Already installed; use verification or the recorded backup'
    backup=ROOT/'Saved/Backups'/('MainGame_RoutesV6_'+datetime.now().strftime('%Y%m%d_%H%M%S'));backup.mkdir(parents=True)
    shutil.copy2(ROOT/'Content/MainGame/Maps/Lvl_MainGame.umap',backup/'Lvl_MainGame.umap')
    for path in ['MainGameRoutes.json','MainGameMinerals.json','MainGameTuning.json']:shutil.copy2(ROOT/'Config'/path,backup/path)
    report['backup']=str(backup)
    cfg=json.loads((ROOT/'Config/MainGameRooms.json').read_text())
    hashes={r['source']:hashlib.sha256((ROOT/'Content'/(r['source'][6:]+'.umap')).read_bytes()).hexdigest() for r in cfg['rooms']}
    all_actors=list(actors.get_all_level_actors());regions=[next(a for a in all_actors if isinstance(a,unreal.MiningExternalRegion) and a.actor_has_tag(f'BM_Campaign_MainGame{i+1:02}')) for i in range(3)]
    # Preserve actor identities and their canonical sources, including every
    # guide and soil clod. Identical spatial deformation keeps their joins aligned.
    region=regions[1]
    meshes=[a for a in all_actors if isinstance(a,unreal.GeneratedMiningRock) and a.actor_has_tag('BM_Campaign_MainGame02') and a!=region.generated_rock]
    for a in meshes:log('deform '+a.get_actor_label(),vertices=deform(a))
    for a in all_actors:
        if a.actor_has_tag('BM_Campaign_MainGame02') and (isinstance(a,unreal.Light) or isinstance(a,unreal.NiagaraActor)):
            a.set_actor_location(unreal.Vector(*warp(a.get_actor_location().to_tuple())),False,True)
    areas=list(hall.areas);route=list(areas[2].map_route);setp(areas[2],'map_route',[unreal.Vector(*warp(p.to_tuple())) for p in route]);setp(hall,'areas',areas)
    helper=(ROOT/'Scripts/create_main_game.py').read_text(encoding='utf-8').split('\ntry:\n')[0]
    ns={'__name__':'import_helpers'};exec(compile(helper,'import_helpers','exec'),ns);ns['ART']=ART/'TunnelCavernV2'
    cutter=ns['import_fbx']('Tunnel_Cutter.fbx','/Game/MainGame/Models/RoutesV6/TunnelCavernV2/Tunnel',1)[0]
    cutters=list(region.cutters);cutters[0].set_editor_property('mesh',cutter);setp(region,'cutters',cutters)
    # Rebuild the host from the stretched cavity instead of warping the coarse
    # exterior box: nonlinear vertex warping would fold its large triangles.
    bounds=cutter.get_bounding_box();offset=cutters[0].local_transform.translation
    size=region.rock_size
    size.y=math.ceil(max(abs(bounds.min.y+offset.y),abs(bounds.max.y+offset.y))*2/100+8)*100
    size.z=math.ceil(max(abs(bounds.min.z+offset.z),abs(bounds.max.z+offset.z))*2/100+8)*100
    setp(region,'rock_size',size)
    success=False
    for chunk in [1800.,1700.,1900.,1600.,2000.,1400.]:
        setp(region,'chunk_length',chunk)
        if region.generate_region():success=True;break
    assert success,str(region.build_status)
    for a in [region.generated_rock,region.generated_shell]:setp(a,'tags',list(a.tags)+['BM_Campaign_MainGame02','BM_RoutesV5'])
    region.apply_hardness_settings()
    old_helper=(ROOT/'Scripts/apply_main_game_routes_v5.py').read_text(encoding='utf-8').split('\ntry:\n')[0]
    shell_ns={'__name__':'shell_helpers'};exec(compile(old_helper,'shell_helpers','exec'),shell_ns)
    import main_game_room_entrances_geometry as entrances
    origin=region.get_actor_location()+offset
    for spec in cfg['generated_connections']:
        if spec['mine']!=1:continue
        mouth=origin.x+(-6500 if spec['side']=='entry' else 6500)
        floor=min(entrances.floor_at(spec,mouth-400),entrances.floor_at(spec,mouth+400))
        shell_ns['cut_box'](region.generated_shell,unreal.Vector(mouth,spec['a'][1],floor+500),unreal.Vector(800,1100,1500))
    areas=list(hall.areas);center=region.get_actor_location()
    setp(areas[2],'mining_bounds',unreal.Box(min=center-size/2,max=center+size/2))
    setp(areas[2],'activation_bounds',unreal.Box(min=center-size/2,max=center+size/2));setp(hall,'areas',areas)
    all_actors=list(actors.get_all_level_actors())
    log('taller cave regenerated',size=list(size.to_tuple()))
    models=ns['import_fbx']('Platforms.fbx','/Game/MainGame/Models/RoutesV6/Platforms',10)
    features=json.loads((ART/'TunnelCavernV2/features.json').read_text())
    for spec in features['platforms']:
        model=next(m for m in models if m.get_name().endswith(spec['name']))
        body=model.get_editor_property('body_setup');body.set_editor_property('collision_trace_flag',unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE);assert unreal.EditorAssetLibrary.save_loaded_asset(model,False)
        platform=next(a for a in all_actors if a.actor_has_tag('BM_ParkourPlatform') and a.get_actor_label().endswith(spec['name']))
        platform.static_mesh_component.set_static_mesh(model)
    log('ten mushroom platforms installed')
    partition=ART/'LayerOwnership';partition.mkdir(parents=True,exist_ok=True)
    operations=[];exported={}
    def export(a):
        name=a.get_actor_label();filename=name+'.obj'
        if name not in exported:
            source=lib.get_campaign_rock_source(a);transform=a.get_actor_transform()
            points=unreal.GeometryScript_List.convert_vector_list_to_array(unreal.GeometryScript_MeshQueries.get_all_vertex_positions(source,False)[1])
            triangles=unreal.GeometryScript_List.convert_triangle_list_to_array(unreal.GeometryScript_MeshQueries.get_all_triangle_indices(source,False)[1])
            lines=[]
            for p in points:
                q=unreal.MathLibrary.transform_location(transform,p);lines.append('v %.9f %.9f %.9f'%(q.x/100,-q.y/100,q.z/100))
            for t in triangles:lines.append('f %d %d %d'%(t.x+1,t.z+1,t.y+1))
            (partition/filename).write_text('\n'.join(lines),encoding='utf-8');exported[name]=a
        return filename
    for i,r in enumerate(regions):
        guides=list(r.generated_guide_layers)
        clods=[a for a in all_actors if a.get_actor_label().startswith(f'MAIN_GAME_ClayWallClods_{i+1}_')]
        for clod in clods:operations.append(dict(target=export(clod),owners=[export(guides[int(clod.get_actor_label().rsplit('_',1)[1])])]))
        operations.append(dict(target=export(r.generated_rock),owners=[export(a) for a in [*guides,*clods]]))
    for connection in cfg['generated_connections']:
        rock=unreal.load_object(None,connection['rock']);plug=unreal.load_object(None,connection['plug'])
        if isinstance(rock,unreal.GeneratedMiningRock) and isinstance(plug,unreal.GeneratedMiningRock):operations.append(dict(target=export(rock),owners=[export(plug)]))
    (partition/'operations.json').write_text(json.dumps(operations,ensure_ascii=False,indent=2),encoding='utf-8')
    assert (partition/'result.json').exists(),'Source export complete; build layer ownership meshes before installing'
    result=json.loads((partition/'result.json').read_text());assert result['passed']
    for name,digest in result['hashes'].items():assert hashlib.sha256((partition/name).read_bytes()).hexdigest()==digest,('Stale partition source',name)
    for operation in operations:
        actor=exported[Path(operation['target']).stem]
        assert lib.import_campaign_rock(actor,str(partition/'Results'/operation['target'])),actor.get_actor_label()
        log('imported exclusive volume '+actor.get_actor_label())
    log('exclusive layer volumes imported')
    log('connector overlaps removed')
    from repair_main_game_v6_shaft import apply as repair_shaft
    report['shaft_slabs']=repair_shaft();log('shaft coplanar slabs trimmed')
    minerals=rebuild(hall,actors);report['minerals']=minerals;assert minerals['total']==101
    setp(hall,'tags',list(hall.tags)+['BM_RoutesV6'])
    assert levels.save_current_level()
    for source,digest in hashes.items():assert hashlib.sha256((ROOT/'Content'/(source[6:]+'.umap')).read_bytes()).hexdigest()==digest
    routes=json.loads((ROOT/'Config/MainGameRoutes.json').read_text());routes.update(revision=6,source_root='Art/MainGame/RoutesV6',styles=['narrow-spires','tall-mushroom-jetpack','tight-descending-coils']);(ROOT/'Config/MainGameRoutes.json').write_text(json.dumps(routes,indent=2))
    tuning=json.loads((ROOT/'Config/MainGameTuning.json').read_text());tuning['expected_deposits']=101;(ROOT/'Config/MainGameTuning.json').write_text(json.dumps(tuning,ensure_ascii=False,indent=2),encoding='utf-8')
    recipe=json.loads((ROOT/'Config/MainGameMinerals.json').read_text())
    for i,area in enumerate(recipe['areas']):area.update(main_route_deposits=[26,30,33][i],dead_end_rewards=4,dead_end_mineral=[4,6,9][i])
    recipe['note']='常规通路 89 个矿点，12 条死路各配置 1 个本区最高级矿石，共 101 个。保留原区域权重与每颗矿石入包 1 个。';(ROOT/'Config/MainGameMinerals.json').write_text(json.dumps(recipe,ensure_ascii=False,indent=2),encoding='utf-8')
    report.update(passed=True,source_maps_unchanged=True);log('saved')
except Exception:
    report['error']=traceback.format_exc();unreal.log_error(report['error'])
finally:
    (OUT/'authoring.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8');unreal.SystemLibrary.execute_console_command(ed.get_editor_world(),'QUIT_EDITOR')
