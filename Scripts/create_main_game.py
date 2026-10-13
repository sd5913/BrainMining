"""Create a standalone MainGame excavation map with the external-region tool."""
import unreal,json,math,time,hashlib,traceback
from pathlib import Path
ROOT=Path(unreal.Paths.project_dir());ART=ROOT/'Art/MainGame/TunnelV1'
MAP='/Game/MainGame/Maps/Lvl_MainGame'
OUT=ROOT/'Saved/Validation/main_game_creation.json'
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
state={'passed':False,'phase':'starting','owner':'MainGameCreation20261013','started':time.monotonic()}
previous=json.loads(OUT.read_text(encoding='utf-8')) if OUT.exists() else {}
def checkpoint(**items):
    state.update(items);OUT.write_text(json.dumps(state,ensure_ascii=False,indent=2),encoding='utf-8');unreal.log('MAIN_GAME '+json.dumps(items,ensure_ascii=False))
def meshes_at(path):
    return sorted([a for p in unreal.EditorAssetLibrary.list_assets(path,recursive=False) if isinstance((a:=unreal.load_asset(p)),unreal.StaticMesh)],key=lambda a:a.get_name())
def import_fbx(file,dest,count):
    meshes=meshes_at(dest)
    if not meshes:
        options=unreal.FbxImportUI()
        for name,value in {'import_mesh':True,'import_as_skeletal':False,'import_animations':False,'import_materials':False,'import_textures':False,
                           'automated_import_should_detect_type':False,'mesh_type_to_import':unreal.FBXImportType.FBXIT_STATIC_MESH}.items():options.set_editor_property(name,value)
        data=options.static_mesh_import_data
        for name,value in {'combine_meshes':False,'convert_scene':True,'convert_scene_unit':True,'transform_vertex_to_absolute':True,
                           'bake_pivot_in_vertex':False,'auto_generate_collision':False,'remove_degenerates':False,'build_nanite':False}.items():data.set_editor_property(name,value)
        task=unreal.AssetImportTask()
        for name,value in {'filename':str(ART/file),'destination_path':dest,'automated':True,'save':True,'replace_existing':False,'options':options,'factory':unreal.FbxFactory()}.items():task.set_editor_property(name,value)
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task]);meshes=meshes_at(dest)
    assert len(meshes)==count,[(m.get_name()) for m in meshes]
    # Keep solids closed while repairing micron-size rounding artifacts from FBX float coordinates.
    for mesh in meshes:
        geo=unreal.DynamicMesh()
        unreal.GeometryScript_AssetUtils.copy_mesh_from_static_mesh_v2(mesh,geo,unreal.GeometryScriptCopyMeshFromAssetOptions(apply_build_settings=False),
            unreal.GeometryScriptMeshReadLOD(lod_type=unreal.GeometryScriptLODType.SOURCE_MODEL,lod_index=0))
        before=geo.get_triangle_count()
        unreal.GeometryScript_MeshRepair.weld_mesh_edges(geo,unreal.GeometryScriptWeldEdgesOptions(tolerance=.001,only_unique_pairs=True))
        unreal.GeometryScript_MeshRepair.repair_mesh_degenerate_geometry(geo,unreal.GeometryScriptDegenerateTriangleOptions(mode=unreal.GeometryScriptRepairMeshMode.REPAIR_OR_SKIP,min_triangle_area=1e-8,min_edge_length=.001))
        assert unreal.GeometryScript_MeshQueries.get_is_closed_mesh(geo),mesh.get_name()
        if geo.get_triangle_count()!=before:
            unreal.GeometryScript_AssetUtils.copy_mesh_to_static_mesh(geo,mesh,unreal.GeometryScriptCopyMeshToAssetOptions(),unreal.GeometryScriptMeshWriteLOD())
            assert unreal.EditorAssetLibrary.save_loaded_asset(mesh)
    return meshes
def cls(path):return unreal.load_class(None,path+'.'+path.rsplit('/',1)[-1]+'_C')
def duplicate(source,dest):
    obj=unreal.load_asset(dest) if unreal.EditorAssetLibrary.does_asset_exist(dest) else unreal.EditorAssetLibrary.duplicate_asset(source,dest)
    assert obj,dest;return obj
def raw(p):return unreal.Vector(p[0]*100,-p[1]*100,p[2]*100)
def trace(world,a,b,ignore=None):
    return unreal.SystemLibrary.line_trace_single(world,a,b,unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,True,ignore or [],unreal.DrawDebugTrace.NONE,True)
try:
    assert json.loads((ART/'progress.json').read_text())['passed']
    map_exists=unreal.EditorAssetLibrary.does_asset_exist(MAP)
    if map_exists:
        assert previous.get('owner')==state['owner'] and not previous.get('passed'),'Existing MainGame map will not be overwritten'
        assert (ROOT/'Content/MainGame/Maps/Lvl_MainGame.umap').stat().st_size<65536,'Existing map contains authored geometry'
    checkpoint(phase='importing')
    cutter_mesh=import_fbx('Tunnel_Cutter.fbx','/Game/MainGame/Models/Tunnel',1)[0]
    blocker_meshes=import_fbx('Tunnel_Blockers.fbx','/Game/MainGame/Models/Blockers',16)
    checkpoint(phase='configuring_character',cutter=cutter_mesh.get_path_name(),blocker_count=len(blocker_meshes))
    character_path='/Game/MainGame/Blueprints/BP_MainGameCharacter';mode_path='/Game/MainGame/Blueprints/BP_MainGameGameMode'
    character_bp=duplicate('/Game/ExternalMining/Blueprints/BP_ExternalMiningTestCharacter',character_path)
    character=unreal.get_default_object(cls(character_path));mining=character.get_component_by_class(unreal.MiningComponent)
    assert mining;mining.set_editor_property('fall_reset_distance',0.)
    unreal.BlueprintEditorLibrary.compile_blueprint(character_bp);assert unreal.EditorAssetLibrary.save_loaded_asset(character_bp)
    mode_bp=duplicate('/Game/ExternalMining/Blueprints/BP_ExternalMiningTestGameMode',mode_path)
    unreal.get_default_object(cls(mode_path)).set_editor_property('default_pawn_class',cls(character_path))
    unreal.BlueprintEditorLibrary.compile_blueprint(mode_bp);assert unreal.EditorAssetLibrary.save_loaded_asset(mode_bp)
    assert (levels.load_level(MAP) if map_exists else levels.new_level(MAP))
    world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    region=actors.spawn_actor_from_class(unreal.MiningExternalRegion,unreal.Vector())
    region.set_actor_label('EDIT_MainGame_外部通路矿区');region.set_editor_property('tags',['BM_MainGame_V1'])
    bounds=cutter_mesh.get_bounding_box();center=unreal.Vector(0,(bounds.min.y+bounds.max.y)*.5,(bounds.min.z+bounds.max.z)*.5)
    size=unreal.Vector(12500.,math.ceil((bounds.max.y-bounds.min.y+800)/100)*100,math.ceil((bounds.max.z-bounds.min.z+600)/100)*100)
    region.set_actor_location(center,False,True)
    region.set_editor_property('rock_size',size);region.set_editor_property('shell_thickness',150.)
    region.set_editor_property('chunk_length',1600.);region.set_editor_property('chunk_triangle_budget',100000)
    region.set_editor_property('show_cutter_preview',False)
    rock_material=unreal.load_asset('/Game/Campaign/Materials/M_CampaignLimestone')
    guide_material=unreal.load_asset('/Game/Campaign/Materials/M_CampaignGuideRock')
    assert rock_material and guide_material
    region.set_editor_property('rock_material',rock_material);region.set_editor_property('shell_material',rock_material)
    region.set_editor_property('guide_material',guide_material)
    cutter=unreal.MiningExternalCutter();cutter.set_editor_property('mesh',cutter_mesh)
    cutter.set_editor_property('local_transform',unreal.Transform(location=-center));region.set_editor_property('cutters',[cutter])
    layers=[]
    for mesh in blocker_meshes:
        layer=unreal.MiningExternalGuideLayer();layer.set_editor_property('mesh',mesh);layers.append(layer)
    region.set_editor_property('guide_layers',layers);region.set_editor_property('guide_follow_cutter_transform',True);region.set_editor_property('guide_cutter_index',0)
    checkpoint(phase='generating_rock',rock_size_cm=[size.x,size.y,size.z])
    assert region.generate_region(),str(region.build_status)
    checkpoint(phase='generating_guides',rock_chunks=len(region.generated_rock.chunks),rock_status=str(region.build_status))
    assert region.generate_guide_layers(),str(region.guide_build_status)
    paths=json.loads((ART/'centerlines.json').read_text());main=paths[0]
    def point(s):
        import bisect
        i=max(1,min(len(main['arc'])-1,bisect.bisect_left(main['arc'],s)));u=(s-main['arc'][i-1])/(main['arc'][i]-main['arc'][i-1])
        return raw([a+(b-a)*u for a,b in zip(main['points'][i-1],main['points'][i])])
    start_center=point(5.5);floor=trace(world,start_center,start_center-unreal.Vector(0,0,2000))
    assert floor and floor.to_tuple()[0],'No walkable floor at spawn'
    spawn=floor.to_tuple()[5]+unreal.Vector(0,0,100)
    player_start=actors.spawn_actor_from_class(unreal.PlayerStart,spawn);player_start.set_actor_label('PLAYER_START_MainGame_头灯挖掘')
    yaw=unreal.MathLibrary.find_look_at_rotation(point(5.5),point(7.)).yaw;player_start.set_actor_rotation(unreal.Rotator(yaw=yaw),False)
    # Subtle entrance/exit lamps supplement the player's built-in headlamp.
    for label,position in [('Entry',point(3.)+unreal.Vector(0,0,180)),('Exit',point(main['arc'][-1]-4)+unreal.Vector(0,0,160))]:
        light=actors.spawn_actor_from_class(unreal.PointLight,position);light.set_actor_label('MainGame_'+label+'_Light')
        component=light.get_component_by_class(unreal.PointLightComponent);component.set_editor_property('mobility',unreal.ComponentMobility.MOVABLE)
        component.set_editor_property('intensity',650.);component.set_editor_property('attenuation_radius',1400.);component.set_editor_property('cast_shadows',False)
    world.get_world_settings().set_editor_property('default_game_mode',cls(mode_path))
    world.get_world_settings().set_editor_property('kill_z',-100000.)
    # Both mouths must pass through the non-diggable casing; the guides stay clear of the mouths.
    entry_trace=trace(world,raw(main['points'][0])+unreal.Vector(-1000,0,200),point(3.))
    exit_trace=trace(world,point(main['arc'][-1]-3),raw(main['points'][-1])+unreal.Vector(1000,0,-200))
    assert not (entry_trace and entry_trace.to_tuple()[0]) and not (exit_trace and exit_trace.to_tuple()[0]),'Entry/exit is still blocked by rock or bedrock'
    checkpoint(phase='saving',guide_count=len(region.generated_guide_layers),guide_chunks=sum(len(g.chunks) for g in region.generated_guide_layers),
               guide_status=str(region.guide_build_status),spawn_cm=[spawn.x,spawn.y,spawn.z],mouths_open=True,headlamp_character=character_path)
    assert levels.save_current_level()
    assert unreal.EditorAssetLibrary.save_directory('/Game/MainGame',only_if_is_dirty=True,recursive=True)
    checkpoint(passed=True,phase='complete',map=MAP,saved=True,elapsed_seconds=time.monotonic()-state['started'])
except Exception:checkpoint(phase='failed',error=traceback.format_exc());raise
