"""Reference three authored room maps and refit the four existing excavation areas.

Run once with MainGame open, PIE stopped, and the room integration C++ compiled.
Never saves a source level. Existing mined solids and gem host identities survive.
"""
import unreal,json,sys,math,hashlib,traceback
from pathlib import Path
ROOT=Path(unreal.Paths.project_dir()).resolve();sys.path.insert(0,str(ROOT/'Scripts'))
import campaign_minerals_api as minerals
OUT=ROOT/'Saved/Validation/MainGameRooms';CFG=ROOT/'Config/MainGameRooms.json';cfg=json.loads(CFG.read_text())
baseline=json.loads((OUT/'before.json').read_text());MAP='/Game/MainGame/Maps/Lvl_MainGame';I=unreal.Transform()
ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem);levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
report={'passed':False,'backup':baseline['backup'],'phase':'start'}
def log(**kw):
    report.update(kw);(OUT/'authoring.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8');unreal.log('MAIN_GAME_ROOMS '+json.dumps(kw,ensure_ascii=False))
def V(p):return unreal.Vector(*p)
def xyz(p):return list(p.to_tuple())
def shifted(t,d):
    t=t.copy();t.translation=t.translation+d;return t
def box_shift(b,d):return unreal.Box(min=b.min+d,max=b.max+d)
def tagged(a,label,tag):
    assert a.get_level()==hall.get_level(),'Connection actor was spawned into a source level'
    a.set_actor_label(label);a.set_folder_path('MainGame/LinkedRooms/'+tag.removeprefix('BM_Campaign_'));a.set_editor_property('tags',[tag,'BM_MainGame_RoomConnection']);return a
def block(label,lo,hi,tag,material):
    a=tagged(actors.spawn_actor_from_class(unreal.StaticMeshActor,(V(lo)+V(hi))*.5),label,tag)
    a.static_mesh_component.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Cube'));a.static_mesh_component.set_material(0,material);a.static_mesh_component.set_collision_profile_name('BlockAll');a.set_actor_scale3d((V(hi)-V(lo))/100.);return a
def cube(label,p,size,tag,material,yaw=0):
    a=block(label,xyz(p-size*.5),xyz(p+size*.5),tag,material);a.set_actor_rotation(unreal.Rotator(yaw=yaw),False);return a
def material_shift(path,d):
    m=unreal.load_asset(path);values=list(m.get_editor_property('vector_parameter_values'))
    for value in values:
        if str(value.parameter_info.name).startswith(('MudStart','MudEnd')):
            c=value.parameter_value;value.set_editor_property('parameter_value',unreal.LinearColor(c.r+d.x,c.g+d.y,c.b+d.z,c.a))
    m.set_editor_property('vector_parameter_values',values,notify_mode=unreal.PropertyAccessChangeNotifyMode.NEVER)
    unreal.MaterialEditingLibrary.update_material_instance(m);assert unreal.EditorAssetLibrary.save_loaded_asset(m,False)
def door_cut(region,mouth,floor,tag):
    shell=region.generated_shell;c=shell.get_component_by_class(unreal.DynamicMeshComponent);m=c.get_dynamic_mesh();tool=unreal.DynamicMesh()
    p=unreal.MathLibrary.inverse_transform_location(shell.get_actor_transform(),mouth+unreal.Vector(0,0,180))
    unreal.GeometryScript_Primitives.append_box(tool,unreal.GeometryScriptPrimitiveOptions(),unreal.Transform(location=p),dimension_x=800,dimension_y=500,dimension_z=361,origin=unreal.GeometryScriptPrimitiveOriginMode.CENTER)
    unreal.GeometryScript_MeshBooleans.apply_mesh_boolean(m,I,tool,I,unreal.GeometryScriptBooleanOperation.SUBTRACT,unreal.GeometryScriptMeshBooleanOptions(fill_holes=True,simplify_output=False))
    assert unreal.GeometryScript_MeshQueries.get_is_closed_mesh(m),'Doorway opened bedrock solid';c.update_collision(False)
    # Collar closes the parts of the original broad cave mouth outside this
    # chosen doorway; there is no route into the void around the adapter.
    p,e=shell.get_actor_bounds(False);lo=p-e-unreal.Vector(5,5,5);hi=p+e+unreal.Vector(5,5,5)
    x=mouth.x+(100 if mouth.x<region.get_actor_location().x else -100);y=mouth.y
    mat=region.shell_material
    for side,a,b in [('South',(x-90,lo.y,lo.z),(x+90,y-250,hi.z)),('North',(x-90,y+250,lo.z),(x+90,hi.y,hi.z)),('Below',(x-90,y-250,lo.z),(x+90,y+250,floor)),('Above',(x-90,y-250,floor+360),(x+90,y+250,hi.z))]:
        block('LINK_'+tag+'_'+str(round(mouth.x))+'_Collar_'+side,a,b,tag,mat)
try:
    assert not cfg['installed'];assert not ed.get_game_world();assert ed.get_editor_world().get_path_name()==MAP+'.Lvl_MainGame'
    assert levels.set_current_level_by_name('Lvl_MainGame') and levels.save_current_level()
    existing=list(actors.get_all_level_actors());hall=next(a for a in existing if isinstance(a,unreal.MiningExhibitHall));old_areas=list(hall.areas);assert len(old_areas)==4
    field=minerals.get_field(hall);before_placements=list(field.get_editor_property('authored_placements'));assert len(before_placements)==165
    before_values=[(str(p.treasure_id),p.value,str(p.area_tag),bool(p.guide_layer),len(p.fragments)) for p in before_placements]
    deltas=[V(d) for d in cfg['mine_deltas']];tags=['BM_Campaign_MainGame%02d'%(i+1) for i in range(4)]
    regions=[next(a for a in existing if isinstance(a,unreal.MiningExternalRegion) and a.actor_has_tag(tag) and a.actor_has_tag('BM_MainGame_RoutesV2')) for tag in tags]
    original_solids={r.get_path_name():(r.generated_rock.get_path_name(),r.generated_rock.triangle_count,[g.get_path_name() for g in r.generated_guide_layers]) for r in regions}
    # Prepare a translated layout before mutating the live terrain or its hosts.
    placements=[]
    for index,p in enumerate(before_placements):
        d=deltas[tags.index(str(p.area_tag))];p=p.copy()
        p.set_editor_property('transform',shifted(p.transform,d));p.set_editor_property('surface',shifted(p.surface,d))
        p.set_editor_property('fragments',[shifted(t,d) for t in p.fragments]);p.set_editor_property('fragment_supports',[s+d for s in p.fragment_supports]);placements.append(p)
        if index%30==0:log(phase='prepare_minerals',translated=index)
    connectors=[r for r in existing if isinstance(r,unreal.MiningExternalRegion) and r not in regions]
    doomed={a for r in connectors for a in [r,r.generated_rock,r.generated_shell]+list(r.generated_guide_layers) if a}
    # Capture transforms before moving parents, then explicitly restore each
    # child to its translated original pose (never add the offset twice).
    poses=[]
    for a in existing:
        if a in doomed or a.get_level()!=hall.get_level():continue
        idx=next((i for i,t in enumerate(tags) if a.actor_has_tag(t)),None)
        if isinstance(a,unreal.PlayerStart) or a in [hall,field]:idx=0
        if idx is not None:poses.append((a,a.get_actor_transform(),idx))
    for a,t,idx in poses:a.set_actor_transform(shifted(t,deltas[idx]),False,True)
    remove=[a for a in existing if a in doomed or (a.get_level()==hall.get_level() and a.get_actor_label().startswith('RESERVE_'))]
    for a in sorted(remove,key=lambda a:isinstance(a,unreal.MiningExternalRegion)):
        # Region destruction can also destroy its generated actors.
        if a in actors.get_all_level_actors():actors.destroy_actor(a)
    assert not set(remove).intersection(actors.get_all_level_actors()),'Reserved corridor actors remain'
    for i,r in enumerate(regions):
        # Cutters from the old neighbouring positions were only seam helpers.
        r.set_editor_property('cutters',[r.cutters[0]],notify_mode=unreal.PropertyAccessChangeNotifyMode.NEVER)
        material_shift('/Game/MainGame/Materials/DirtTransition/MI_DirtWall_%02d'%(i+1),deltas[i])
    material_shift('/Game/MainGame/Materials/DirtTransition/MI_DirtWall_StartRoom',deltas[0])
    field.set_editor_property('authored_placements',placements,notify_mode=unreal.PropertyAccessChangeNotifyMode.NEVER)
    stream=old_areas[0].source_level;stream.set_editor_property('level_transform',shifted(stream.get_editor_property('level_transform'),deltas[0]))
    areas=[]
    for i,old in enumerate(old_areas):
        old=old.copy();d=deltas[i]
        for k in ['origin','exit']:old.set_editor_property(k,old.get_editor_property(k)+d)
        old.set_editor_property('entry',shifted(old.entry,d));old.set_editor_property('map_route',[p+d for p in old.map_route]);old.set_editor_property('mining_bounds',box_shift(old.mining_bounds,d));old.set_editor_property('activation_bounds',old.mining_bounds)
        if i:
            origin=V(cfg['mine_origins'][i]);entry=origin+unreal.Vector(-6600,0,6096);old.set_editor_property('entry',unreal.Transform(location=entry))
        if i<3:
            room=cfg['rooms'][i];west=cfg['connections'][2*i]['b'];old.set_editor_property('exit',V(west)+unreal.Vector(-150,0,96));old.set_editor_property('exit_extent',unreal.Vector(160,350,300))
            old.set_editor_property('map_route',list(old.map_route)+[old.exit])
        else:old.set_editor_property('exit',V(cfg['mine_origins'][i])+unreal.Vector(6500,0,800))
        areas.append(old)
        if i==3:continue
        spec=cfg['rooms'][i];room=unreal.MiningExperienceArea(title=spec['title'],instructions=spec['hint'],tag=spec['tag'],kind=getattr(unreal.MiningExperienceKind,spec['kind']),origin=V(spec['origin']),entry=unreal.Transform(location=V(spec['entry'])),exit=V(spec['exit']),exit_extent=unreal.Vector(170,420,450),mining_bounds=unreal.Box(min=V(spec['bounds'][0]),max=V(spec['bounds'][1])),activation_bounds=unreal.Box(min=V(spec['bounds'][0]),max=V(spec['bounds'][1])))
        before=set(actors.get_all_level_actors());stream=unreal.EditorLevelUtils.add_level_to_world_with_transform(ed.get_editor_world(),spec['source'],unreal.LevelStreamingAlwaysLoaded,unreal.Transform(location=V(spec['origin'])));assert stream
        imported=[a for a in actors.get_all_level_actors() if a not in before];assert len(imported)>=len(baseline['sources'][spec['source']])*.98
        assert levels.set_current_level_by_name('Lvl_MainGame')
        room.set_editor_property('source_level',stream)
        suppressed=[a.get_name() for a in imported if isinstance(a,unreal.ExponentialHeightFog) or (spec['id']=='Museum' and a.get_actor_label().startswith(('BackWall_','EndWall_')))]
        room.set_editor_property('suppressed_source_actors',suppressed);areas.append(room);log(phase='linked_'+spec['id'],source_actors=len(imported))
    hall.set_editor_property('areas',areas,notify_mode=unreal.PropertyAccessChangeNotifyMode.NEVER);hall.set_editor_property('lobby_entry',areas[0].entry)
    hall.set_editor_property('tags',list(hall.tags)+['BM_MainGame_RoomsLinked']);hall.set_editor_property('linear_campaign',True);hall.set_editor_property('continuous_campaign',True)
    core=unreal.load_asset('/Game/MainGame/Materials/DirtTransition/MI_ClayBlockage');door=unreal.load_asset('/Game/Campaign/Materials/M_CampaignDoor');links=[]
    for spec in cfg['connections']:
        a,b=V(spec['a']),V(spec['b']);length=b.x-a.x;h=cfg['adapter_height'];tag=spec['tag'];mat=regions[spec['mine']].rock_material;prefix='LINK_'+spec['id']
        cube(prefix+'_Floor',(a+b)*.5+unreal.Vector(0,0,-35),unreal.Vector(length+40,max(spec['width_a'],spec['width_b'])+140,70),tag,mat)
        cube(prefix+'_Roof',(a+b)*.5+unreal.Vector(0,0,h+35),unreal.Vector(length+40,max(spec['width_a'],spec['width_b'])+140,70),tag,mat)
        for side in [-1,1]:
            p=a+unreal.Vector(0,side*(spec['width_a']/2+35),h/2);q=b+unreal.Vector(0,side*(spec['width_b']/2+35),h/2);axis=q-p
            cube(prefix+('_South' if side<0 else '_North'),(p+q)*.5,unreal.Vector(axis.length()+90,70,h),tag,mat,math.degrees(math.atan2(axis.y,axis.x)))
        fraction=(spec['plug_x']-a.x)/length;width=spec['width_a']+(spec['width_b']-spec['width_a'])*fraction
        plug=tagged(actors.spawn_actor_from_class(unreal.DiggableRock,unreal.Vector(spec['plug_x'],a.y,a.z+h/2)),prefix+'_DiggableSoil',tag)
        plug.set_editor_property('rock_size',unreal.Vector(cfg['plug_depth'],width+80,h+40),notify_mode=unreal.PropertyAccessChangeNotifyMode.NEVER);plug.set_editor_property('rock_material',core,notify_mode=unreal.PropertyAccessChangeNotifyMode.NEVER);plug.reset_rock()
        origin=V(cfg['mine_origins'][spec['mine']]);mouth=origin+unreal.Vector(-6500 if spec['side']=='entry' else 6500,0,6000 if spec['side']=='entry' else 500)
        door_cut(regions[spec['mine']],mouth,a.z,tag)
        links.append(dict(**spec,plug=plug.get_path_name(),height=h))
    # Replacement museum end walls preserve the original alcove and room shell.
    museum=cfg['rooms'][2];o=V(museum['origin']);mat=unreal.load_asset('/Game/WenSi/Museum/Materials/MI_Limestone')
    for x in [-910,2460]:
        for side in [-1,1]:cube('LINK_Museum_EndWall_'+str(x)+'_'+str(side),o+unreal.Vector(x,side*435,210),unreal.Vector(20,470,420),museum['tag'],mat)
        cube('LINK_Museum_Header_'+str(x),o+unreal.Vector(x,0,390),unreal.Vector(20,400,60),museum['tag'],mat)
    areas=list(hall.areas)
    for index,spec in zip([1,3,5],cfg['rooms']):
        # Digging through the television is enough to expose the card-room exit.
        if spec['id']=='Journey':continue
        p=V(spec['exit'])+unreal.Vector(-55,0,84);gate=cube('LINK_'+spec['id']+'_CompletionGate',p,unreal.Vector(30,500,360),spec['tag'],door)
        areas[index].set_editor_property('progress_gate',gate)
    hall.set_editor_property('areas',areas,notify_mode=unreal.PropertyAccessChangeNotifyMode.NEVER)
    # Keep authoring/verification points in sync with the translated solids.
    bake_path=ROOT/'Saved/Validation/MainGameRoutesV2/Clay/bake.json';bake=json.loads(bake_path.read_text())
    for sample in bake['samples']:
        tag=sample['kind'];i=0 if tag=='junction' else ['BM_MainGame_V1','BM_MainGame_CavernV2','BM_MainGame_StalagmiteV3','BM_MainGame_NeuralV4'].index(tag);sample['position']=xyz(V(sample['position'])+deltas[i])
    bake_path.write_text(json.dumps(bake,ensure_ascii=False,indent=2),encoding='utf-8')
    start_path=ROOT/'Saved/Validation/MainGameStartRoom/integration.json';start=json.loads(start_path.read_text())
    for key in ['source_offset','bedroom_spawn','original_mine_entry','shaft_base','shaft_landing','upper_passage_end','junction_blockage']:start[key]=xyz(V(start[key])+deltas[0])
    start.update(junction_world_offset=cfg['mine_deltas'][0],linked_room_sequence=True);start_path.write_text(json.dumps(start,ensure_ascii=False,indent=2),encoding='utf-8')
    for r in regions:
        previous=original_solids[r.get_path_name()];assert (r.generated_rock.get_path_name(),r.generated_rock.triangle_count,[g.get_path_name() for g in r.generated_guide_layers])==previous
    after_values=[(str(p.treasure_id),p.value,str(p.area_tag),bool(p.guide_layer),len(p.fragments)) for p in field.get_editor_property('authored_placements')];assert before_values==after_values
    valid=minerals.validate(field,hall);assert valid['ok'],valid.get('errors')
    assert levels.set_current_level_by_name('Lvl_MainGame') and levels.save_current_level()
    for source,digest in baseline['source_hashes'].items():assert hashlib.sha256((ROOT/'Content'/(source[6:]+'.umap')).read_bytes()).hexdigest()==digest,source
    cfg.update(installed=True,generated_connections=links);CFG.write_text(json.dumps(cfg,ensure_ascii=False,indent=2),encoding='utf-8')
    log(passed=True,saved=True,phase='complete',areas=7,source_rooms=3,connections=6,deposits=valid['deposits'],fragments=valid['fragments'],mineral_layout_sha256=valid['layout_sha256'],mineral_rewards_and_hosts_preserved=True,source_maps_unchanged=True)
except Exception:log(passed=False,error=traceback.format_exc());raise
