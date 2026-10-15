"""Install RoutesV2 into MainGame only, retaining fixed mouths and StartRoom.

Each region is saved separately and journaled for safe resumption. Native external
region generation retains the game's chunked excavation and R-reset behavior.
"""
import unreal,json,math,time,shutil,hashlib,traceback,bisect,gc
from pathlib import Path
from datetime import datetime
ROOT=Path(unreal.Paths.project_dir()).resolve();ART=ROOT/'Art/MainGame/RoutesV2';OUT=ROOT/'Saved/Validation/MainGameRoutesV2'
MAP='/Game/MainGame/Maps/Lvl_MainGame';STAGES=['TunnelV1','TunnelCavernV2','TunnelStalagmiteV3','TunnelNeuralV4']
TAGS=['BM_MainGame_V1','BM_MainGame_CavernV2','BM_MainGame_StalagmiteV3','BM_MainGame_NeuralV4']
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);editor=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
assert not editor.get_game_world()
assert editor.get_editor_world().get_path_name()==MAP+'.Lvl_MainGame'
assert levels.set_current_level_by_name('Lvl_MainGame') and levels.save_current_level()
state_path=OUT/'authoring.json';state=json.loads(state_path.read_text()) if state_path.exists() else {'passed':False,'completed':[]}
def save(**kw):
    state.update(kw);state_path.write_text(json.dumps(state,ensure_ascii=False,indent=2),encoding='utf-8');unreal.log('ROUTES_APPLY '+json.dumps(kw,ensure_ascii=False))
def V(p):return unreal.Vector(*p)
def raw(p):return unreal.Vector(p[0]*100,-p[1]*100,p[2]*100)
def tag(a,tags):a.set_editor_property('tags',list(dict.fromkeys([str(t) for t in a.tags]+tags)),notify_mode=unreal.PropertyAccessChangeNotifyMode.NEVER)
def union(a,b):return unreal.Box(min=V([min(x,y) for x,y in zip(a.min.to_tuple(),b.min.to_tuple())]),max=V([max(x,y) for x,y in zip(a.max.to_tuple(),b.max.to_tuple())]))
try:
    if 'backup' not in state:
        backup=ROOT/'Saved/Backups'/('MainGame_RoutesV2_'+datetime.now().strftime('%Y%m%d-%H%M%S'));backup.mkdir(parents=True)
        shutil.copy2(ROOT/'Content/MainGame/Maps/Lvl_MainGame.umap',backup/'Lvl_MainGame.umap')
        shutil.copytree(ROOT/'Content/MainGame/Materials/DirtTransition',backup/'DirtTransition')
        shutil.copy2(ROOT/'Config/MainGameMinerals.json',backup/'MainGameMinerals.json')
        protected=json.loads((ROOT/'Saved/Validation/MainGameDirt/authoring.json').read_text())['protected_hashes']
        save(backup=str(backup),protected_hashes=protected,phase='backed_up')
    for stage in STAGES:assert json.loads((ART/stage/'progress.json').read_text())['passed'],stage
    helper=(ROOT/'Scripts/create_main_game.py').read_text().split('\ntry:\n')[0];ns={'__name__':'import_helpers'};exec(compile(helper,'route_import_helpers','exec'),ns)
    world=editor.get_editor_world();regions=list(unreal.GameplayStatics.get_all_actors_of_class(world,unreal.MiningExternalRegion))
    hall=next(a for a in actors.get_all_level_actors() if isinstance(a,unreal.MiningExhibitHall));areas=list(hall.areas)
    before=json.loads((OUT/'before.json').read_text())
    results=state.get('regions',[])
    for index,(stage,rtag) in enumerate(zip(STAGES,TAGS)):
        if stage in state['completed']:continue
        r=next(r for r in regions if r.actor_has_tag(rtag));origin=r.get_actor_location()+r.cutters[0].local_transform.translation
        others=[(c.mesh,r.get_actor_location()+c.local_transform.translation,c.enabled) for c in list(r.cutters)[1:]]
        ns['ART']=ART/stage
        mesh=ns['import_fbx']('Tunnel_Cutter.fbx','/Game/MainGame/Models/RoutesV2/'+stage+'/Tunnel',1)[0]
        plugs=ns['import_fbx']('Tunnel_Blockers.fbx','/Game/MainGame/Models/RoutesV2/'+stage+'/Blockers',16)
        bounds=mesh.get_bounding_box();old_center=r.get_actor_location();old_size=r.rock_size
        # Retain the original x seams. Expand y/z to keep every new route inside bedrock.
        lo=origin+bounds.min;hi=origin+bounds.max
        miny=min(old_center.y-old_size.y/2,lo.y-450);maxy=max(old_center.y+old_size.y/2,hi.y+450)
        minz=min(old_center.z-old_size.z/2,lo.z-350);maxz=max(old_center.z+old_size.z/2,hi.z+350)
        center=V([old_center.x,(miny+maxy)/2,(minz+maxz)/2]);size=V([12500,math.ceil((maxy-miny)/100)*100,math.ceil((maxz-minz)/100)*100])
        r.set_actor_location(center,False,True);r.set_editor_property('rock_size',size,notify_mode=unreal.PropertyAccessChangeNotifyMode.NEVER)
        cutters=[]
        for m,o,enabled in [(mesh,origin,True)]+others:
            c=unreal.MiningExternalCutter(mesh=m,local_transform=unreal.Transform(location=o-center),enabled=enabled);cutters.append(c)
        r.set_editor_property('cutters',cutters,notify_mode=unreal.PropertyAccessChangeNotifyMode.NEVER)
        r.set_editor_property('guide_layers',[unreal.MiningExternalGuideLayer(mesh=m) for m in plugs],notify_mode=unreal.PropertyAccessChangeNotifyMode.NEVER)
        save(phase='generating_'+stage)
        success=False
        for chunk in [1800.,1700.,1900.,1600.]:
            r.set_editor_property('chunk_length',chunk,notify_mode=unreal.PropertyAccessChangeNotifyMode.NEVER)
            if r.generate_region():success=True;break
        assert success,str(r.build_status)
        assert r.generate_guide_layers(),str(r.guide_build_status)
        area_tag='BM_Campaign_MainGame%02d'%(index+1)
        for obj in [r,r.generated_rock,r.generated_shell]+list(r.generated_guide_layers):tag(obj,[area_tag])
        tag(r,['BM_MainGame_RoutesV2'])
        paths=json.loads((ART/stage/'centerlines.json').read_text());route=[origin+raw(p) for p in paths[0]['points']]
        area=next(a for a in areas if str(a.tag)==area_tag)
        assert route[0].distance(V(before['areas'][index]['start']))<.01 and route[-1].distance(V(before['areas'][index]['end']))<.01
        area.set_editor_property('map_route',route);area.set_editor_property('origin',center)
        bounds=unreal.Box(min=center-size*.5-unreal.Vector(250,250,250),max=center+size*.5+unreal.Vector(250,250,250))
        area.set_editor_property('mining_bounds',union(area.mining_bounds,bounds));area.set_editor_property('activation_bounds',union(area.activation_bounds,bounds))
        # The first connector also stores old neighboring cutters; replace references
        # while preserving the already saved connector terrain and its exact seams.
        for reserve in regions:
            if reserve in [r] or reserve in [x for x in regions if any(x.actor_has_tag(t) for t in TAGS)]:continue
            updated=[]
            for c in reserve.cutters:
                if c.mesh.get_path_name()==before['regions'][next(j for j,x in enumerate(before['regions']) if rtag in x['tags'])]['cutters'][0]['mesh']:
                    c.set_editor_property('mesh',mesh)
                updated.append(c)
            reserve.set_editor_property('cutters',updated,notify_mode=unreal.PropertyAccessChangeNotifyMode.NEVER)
        hall.set_editor_property('areas',areas)
        # Place soft fill lights inside each new chamber; reuse the same named actors.
        plan=json.loads((ART/stage/'plan_summary.json').read_text());main=paths[0]
        for n,s in enumerate(plan['cavern_centers_arc_m']):
            i=bisect.bisect_left(main['arc'],s);p=origin+raw(main['points'][i])+unreal.Vector(0,0,180)
            label=f'MainGame_RoutesV2_{stage}_Chamber_{n+1}'
            light=next((a for a in actors.get_all_level_actors() if a.get_actor_label()==label),None)
            if not light:light=actors.spawn_actor_from_class(unreal.PointLight,p)
            light.set_actor_label(label);light.set_actor_location(p,False,True);light.set_folder_path('MainGame/Lighting/RoutesV2')
            comp=light.get_component_by_class(unreal.PointLightComponent);comp.set_editor_property('mobility',unreal.ComponentMobility.MOVABLE)
            comp.set_editor_property('intensity',1200.);comp.set_editor_property('attenuation_radius',2300.);comp.set_editor_property('cast_shadows',False)
        results.append(dict(stage=stage,origin=list(origin.to_tuple()),size=list(size.to_tuple()),chunks=len(r.generated_rock.chunks),triangles=r.generated_rock.triangle_count,guides=16,chunk_length=r.chunk_length))
        state['completed'].append(stage);assert levels.save_current_level()
        save(phase='saved_'+stage,regions=results)
        gc.collect();unreal.SystemLibrary.collect_garbage()
    (ROOT/'Config/MainGameRoutes.json').write_text(json.dumps(dict(revision=2,source_root='Art/MainGame/RoutesV2',stages=STAGES),indent=2),encoding='utf-8')
    for p,digest in state['protected_hashes'].items():assert hashlib.sha256((ROOT/p).read_bytes()).hexdigest()==digest,p
    assert levels.save_current_level()
    save(passed=True,saved=True,phase='geometry_complete',fixed_mouths=True,protected_sources_unchanged=True)
except Exception:
    save(passed=False,error=traceback.format_exc());raise
