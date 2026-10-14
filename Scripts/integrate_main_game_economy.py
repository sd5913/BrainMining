"""Reuse FullJourney economy/UI and bake its mineral system into the four MainGame mines."""
import unreal,json,sys,copy,shutil,hashlib,traceback,time,bisect
from pathlib import Path
ROOT=Path(unreal.Paths.project_dir()).resolve();OUT=ROOT/'Saved/Validation/main_game_system_integration.json'
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world();assert world.get_path_name()=='/Game/MainGame/Maps/Lvl_MainGame.Lvl_MainGame'
state={'passed':False,'phase':'starting'};started=time.monotonic()
def checkpoint(**kw):state.update(kw);OUT.write_text(json.dumps(state,ensure_ascii=False,indent=2),encoding='utf-8');unreal.log('MAIN_GAME_SYSTEMS '+json.dumps(kw,ensure_ascii=False))
def append_tag(actor,tag):
    tags=list(actor.tags)
    if tag not in [str(t) for t in tags]:actor.set_editor_property('tags',tags+[tag],notify_mode=unreal.PropertyAccessChangeNotifyMode.NEVER)
def geometry(r):
    return dict(rock=r.generated_rock.get_path_name(),triangles=r.generated_rock.triangle_count,
        material=r.rock_material.get_path_name(),shell=r.generated_shell.get_path_name(),
        guides=[dict(path=g.get_path_name(),triangles=g.triangle_count,material=g.rock_material.get_path_name()) for g in r.generated_guide_layers])
def source_objects():
    source=unreal.load_asset('/Game/Campaign/Maps/Lvl_FullJourney');assert source
    objects=list(unreal.GameplayStatics.get_all_actors_of_class(source,unreal.Actor))
    return source,next(a for a in objects if isinstance(a,unreal.MiningExhibitHall)),next(a for a in objects if isinstance(a,unreal.MiningTreasureField))
def owned_actor(kind,label,location):
    existing=[a for a in actors.get_all_level_actors() if isinstance(a,kind) and a.actor_has_tag('BM_MainGame_SystemIntegration')]
    assert len(existing)<=1,(kind,'Duplicate integration actor')
    obj=existing[0] if existing else actors.spawn_actor_from_class(kind,location)
    obj.set_actor_label(label);obj.set_folder_path('MainGame/Systems');append_tag(obj,'BM_MainGame_SystemIntegration');return obj
def merge_bounds(a,b):
    return unreal.Box(min=unreal.Vector(min(a.min.x,b.min.x),min(a.min.y,b.min.y),min(a.min.z,b.min.z)),
                      max=unreal.Vector(max(a.max.x,b.max.x),max(a.max.y,b.max.y),max(a.max.z,b.max.z)))
try:
    assert levels.save_current_level()
    source_file=ROOT/'Content/Campaign/Maps/Lvl_FullJourney.umap';source_hash=hashlib.sha256(source_file.read_bytes()).hexdigest()
    regions=list(unreal.GameplayStatics.get_all_actors_of_class(world,unreal.MiningExternalRegion));assert len(regions)==7
    before={r.get_path_name():geometry(r) for r in regions}
    checkpoint(phase='reading_fulljourney');source,source_hall,source_field=source_objects()
    assert source_field.get_editor_property('use_authored_placements')
    source_mode=unreal.get_default_object(source.get_world_settings().default_game_mode)
    source_player=unreal.get_default_object(source_mode.default_pawn_class)
    source_progression=source_player.get_component_by_class(unreal.MiningProgressionComponent);assert source_progression
    # Work only on MainGame's existing character/mode assets.
    character_bp=unreal.load_asset('/Game/MainGame/Blueprints/BP_MainGameCharacter')
    player=unreal.get_default_object(character_bp.generated_class());progression=player.get_component_by_class(unreal.MiningProgressionComponent)
    assert progression;progression.set_editor_property('tracks',source_progression.tracks)
    player.get_component_by_class(unreal.MiningComponent).set_editor_property('fall_reset_distance',0.)
    unreal.BlueprintEditorLibrary.compile_blueprint(character_bp);assert unreal.EditorAssetLibrary.save_loaded_asset(character_bp,only_if_is_dirty=False)
    mode_bp=unreal.load_asset('/Game/MainGame/Blueprints/BP_MainGameGameMode')
    if not isinstance(unreal.get_default_object(mode_bp.generated_class()),unreal.MiningExhibitGameMode):
        unreal.BlueprintEditorLibrary.reparent_blueprint(mode_bp,source.get_world_settings().default_game_mode)
    mode=unreal.get_default_object(mode_bp.generated_class())
    mode.set_editor_property('default_pawn_class',character_bp.generated_class());mode.set_editor_property('player_controller_class',source_mode.player_controller_class)
    mode.set_editor_property('hud_class',source_mode.hud_class)
    unreal.BlueprintEditorLibrary.compile_blueprint(mode_bp);assert unreal.EditorAssetLibrary.save_loaded_asset(mode_bp,only_if_is_dirty=False)
    world.get_world_settings().set_editor_property('default_game_mode',mode_bp.generated_class())
    starts=list(unreal.GameplayStatics.get_all_actors_of_class(world,unreal.PlayerStart));assert len(starts)==1;entry=starts[0].get_actor_transform()
    hall=owned_actor(unreal.MiningExhibitHall,'MAIN_GAME_金币升级与区域管理',entry.translation)
    hall.set_editor_property('linear_campaign',True);hall.set_editor_property('continuous_campaign',True);hall.set_editor_property('lobby_entry',entry)
    collection=owned_actor(unreal.MiningCollectionField,'MAIN_GAME_共享交互接口',unreal.Vector(0,-100000,0))
    for k,v in dict(area_managed=True,shared_abilities=True,show_exploration_hud=False,use_authored_placements=True,authored_placements=[]).items():collection.set_editor_property(k,v)
    key=unreal.Key();key.set_editor_property('key_name','I');collection.set_editor_property('catalogue_key',key)
    field=owned_actor(unreal.MiningTreasureField,'MAIN_GAME_结晶与金币配置',entry.translation)
    if not field.get_editor_property('authored_placements'):
        for k in ['definitions','treasure_material','crystal_clue_mesh','random_seed']:field.set_editor_property(k,source_field.get_editor_property(k))
    field.set_editor_property('use_shared_wallet',True);field.set_editor_property('use_authored_placements',True)
    spec=[('TunnelV1','BM_MainGame_V1','MainGame01','硅质岩区'),('TunnelCavernV2','BM_MainGame_CavernV2','MainGame02','溶洞岩区'),
          ('TunnelStalagmiteV3','BM_MainGame_StalagmiteV3','MainGame03','石笋岩区'),('TunnelNeuralV4','BM_MainGame_NeuralV4','MainGame04','神经岩区')]
    areas=[]
    for i,(folder,old_tag,name,title) in enumerate(spec):
        region=next(r for r in regions if old_tag in [str(t) for t in r.tags]);tag='BM_Campaign_'+name
        for obj in [region,region.generated_rock,region.generated_shell]+list(region.generated_guide_layers):append_tag(obj,tag)
        origin=region.get_actor_location()+region.cutters[0].local_transform.translation
        path=json.loads((ROOT/'Art/MainGame'/folder/'centerlines.json').read_text())[0]
        route=[origin+unreal.Vector(p[0]*100,-p[1]*100,p[2]*100) for p in path['points']]
        # The MainGame path itself, rather than FullJourney's edited cavity, is the bake route.
        area=unreal.MiningExperienceArea();area.set_editor_property('tag',tag);area.set_editor_property('kind',unreal.MiningExperienceKind.MINING)
        area.set_editor_property('title',f'{i+1}. {title}');area.set_editor_property('instructions','沿碎晶线索挖出矿物，瞄准后按 E 收取金币；按住 Tab 升级能力。')
        area.set_editor_property('origin',region.get_actor_location());area.set_editor_property('map_route',route)
        if i==0:spawn=entry
        else:
            p=route[8];hit=unreal.SystemLibrary.line_trace_single(world,p,p-unreal.Vector(0,0,3000),unreal.TraceTypeQuery.ECC_VISIBILITY,True,list(region.generated_guide_layers),unreal.DrawDebugTrace.NONE,True)
            assert hit and hit.to_tuple()[0],(name,'Missing entry floor')
            spawn=unreal.Transform(location=hit.to_tuple()[5]+unreal.Vector(0,0,100),rotation=unreal.MathLibrary.find_look_at_rotation(route[8],route[12]))
        area.set_editor_property('entry',spawn)
        end=route[-1]+(unreal.Vector(900,0,-90) if i<3 else unreal.Vector(0,0,0))
        area.set_editor_property('exit',end);area.set_editor_property('exit_extent',unreal.Vector(220,1400,1500))
        size=region.rock_size*.5+unreal.Vector(250,250,250);center=region.get_actor_location()
        bounds=unreal.Box(min=center-size,max=center+size);area.set_editor_property('mining_bounds',bounds);area.set_editor_property('activation_bounds',bounds)
        areas.append(area)
    # Connector terrain belongs to the preceding stage for scoped excavation reset.
    for i,tag in enumerate(['BM_MainGame_FutureRoom','BM_MainGame_FutureRoom_02','BM_MainGame_FutureRoom_03']):
        r=next(r for r in regions if tag in [str(t) for t in r.tags])
        for obj in [r,r.generated_rock,r.generated_shell]:append_tag(obj,str(areas[i].tag))
        areas[i].set_editor_property('mining_bounds',merge_bounds(areas[i].mining_bounds,unreal.Box(min=r.get_actor_location()-r.rock_size*.5,max=r.get_actor_location()+r.rock_size*.5)))
    hall.set_editor_property('areas',areas)
    # Retain FullJourney's values, weights and total economic budgets, spreading the end reserve into the fourth region.
    economy=copy.deepcopy(json.loads((ROOT/'Config/CampaignMinerals.json').read_text()));economy['seed']=20261014
    for band in economy['bands']:
        if band['area']=='Mine01':band['area']='MainGame01'
        elif band['area']=='Mine02':band['area']='MainGame02'
        elif '末端储备' in band['phase']:band.update(area='MainGame04',phase='终段',range=[.03,.94])
        else:band['area']='MainGame03'
    # Each region is now a full-length tunnel: use the full route for its two economic phases.
    for name in ['MainGame01','MainGame02','MainGame03']:
        bands=[b for b in economy['bands'] if b['area']==name]
        if len(bands)==2:bands[0]['range']=[.03,.45];bands[1]['range']=[.45,.94]
    recipe_path=ROOT/'Config/MainGameMinerals.json';recipe_path.write_text(json.dumps(economy,ensure_ascii=False,indent=2),encoding='utf-8')
    sys.path.insert(0,str(ROOT/'Scripts'));import campaign_minerals_api as minerals
    checkpoint(phase='baking_minerals');field,bake=minerals.rebuild(hall=hall,field=field,recipe=economy)
    assert bake['ok'],bake['errors']
    assert all(geometry(r)==before[r.get_path_name()] for r in regions),'Integration changed mining geometry/materials'
    assert levels.save_current_level()
    assert hashlib.sha256(source_file.read_bytes()).hexdigest()==source_hash,'FullJourney source map changed'
    checkpoint(passed=True,phase='complete',saved=True,deposits=bake['deposits'],fragments=bake['fragments'],layout_sha256=bake['layout_sha256'],
        summary=bake['summary'],hall=hall.get_path_name(),field=field.get_path_name(),controller=source_mode.player_controller_class.get_path_name(),
        upgrade_prices=[[s.price for s in t.steps] for t in progression.tracks],geometry_and_materials_preserved=True,fulljourney_unchanged=True,elapsed_seconds=time.monotonic()-started)
except Exception:checkpoint(phase='failed',error=traceback.format_exc());raise
