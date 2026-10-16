"""Replace the narrow fixed pipe end with a wide, fully diggable soil mouth."""
import unreal,json,sys,shutil,hashlib,traceback,random
from datetime import datetime
from pathlib import Path
ROOT=Path(unreal.Paths.project_dir()).resolve();sys.path.insert(0,str(ROOT/'Scripts'))
import campaign_minerals_api as minerals
OUT=ROOT/'Saved/Validation/MainGameJunction';OUT.mkdir(parents=True,exist_ok=True)
MAP='/Game/MainGame/Maps/Lvl_MainGame';cfg=json.loads((ROOT/'Config/MainGameJunction.json').read_text())
ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem);levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
edits=unreal.GeometryScript_MeshEdits;queries=unreal.GeometryScript_MeshQueries;I=unreal.Transform();report={'passed':False}
def V(*p):return unreal.Vector(*p)
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def bounds(a,lo,hi):
    a.set_actor_location((V(*lo)+V(*hi))*.5,False,True);a.set_actor_scale3d((V(*hi)-V(*lo))/100.)
def tag(a,label):
    a.set_actor_label(label);a.set_folder_path('MainGame/StartRoomConnection/MineJunction')
    a.set_editor_property('tags',['BM_Campaign_MainGame01','BM_MainGame_StartRoomConnection','BM_MainGame_StartRoomJunction','BM_MainGame_WideJunction']);return a
try:
    assert not ed.get_game_world();assert ed.get_editor_world().get_path_name()==MAP+'.Lvl_MainGame'
    assert levels.set_current_level_by_name('Lvl_MainGame') and levels.save_current_level()
    all_actors=list(actors.get_all_level_actors());named={a.get_actor_label():a for a in all_actors}
    assert not any(a.actor_has_tag('BM_MainGame_WideJunction') for a in all_actors),'Wide junction already applied'
    backup=ROOT/'Saved/Backups'/('MainGame_WideJunction_'+datetime.now().strftime('%Y%m%d-%H%M%S'));backup.mkdir(parents=True)
    shutil.copy2(ROOT/'Content/MainGame/Maps/Lvl_MainGame.umap',backup/'Lvl_MainGame.umap')
    bake_path=ROOT/'Saved/Validation/MainGameRoutesV2/Clay/bake.json';manifest_path=ROOT/'Saved/Validation/MainGameStartRoom/integration.json'
    shutil.copy2(bake_path,backup/'bake.json');shutil.copy2(manifest_path,backup/'integration.json');report['backup']=str(backup)
    protected=json.loads((ROOT/'Saved/Validation/MainGamePolish/authoring.json').read_text())['protected_hashes']
    world,hall,_=minerals._context();field=minerals.get_field(hall);mineral_sha=minerals.validate(field,hall)['layout_sha256']
    region=next(a for a in all_actors if isinstance(a,unreal.MiningExternalRegion) and a.actor_has_tag('BM_MainGame_V1'))
    original_rock=(region.generated_rock.get_path_name(),region.generated_rock.triangle_count)
    shell=region.generated_shell;component=shell.get_component_by_class(unreal.DynamicMeshComponent)
    before=component.get_dynamic_mesh();cut=unreal.DynamicMesh();edits.append_mesh(cut,before,I)
    local=unreal.MathLibrary.inverse_transform_location(shell.get_actor_transform(),V(-6345,0,6240))
    tool=unreal.DynamicMesh();unreal.GeometryScript_Primitives.append_box(tool,unreal.GeometryScriptPrimitiveOptions(),unreal.Transform(location=local),dimension_x=390,dimension_y=640,dimension_z=481,origin=unreal.GeometryScriptPrimitiveOriginMode.CENTER)
    unreal.GeometryScript_MeshBooleans.apply_mesh_boolean(cut,I,tool,I,unreal.GeometryScriptBooleanOperation.SUBTRACT,unreal.GeometryScriptMeshBooleanOptions(fill_holes=True,simplify_output=False))
    assert queries.get_is_closed_mesh(cut) and cut.get_triangle_count()>0,'New bedrock doorway is not closed'
    report['shell_triangles_before']=before.get_triangle_count();report['shell_triangles_after']=cut.get_triangle_count()
    # Truncate the narrow pipe before the new mouth. Its replacement enclosure
    # lies outside the diggable soil, so mining sideways can actually widen it.
    slabs={
      'STARTROOM_UpperPassage_Floor':((-8680,-220,5930),(-6900,220,6000)),
      'STARTROOM_UpperPassage_Roof':((-8680,-220,6330),(-6900,220,6400)),
      'STARTROOM_UpperPassage_North':((-8680,150,6000),(-6900,220,6330)),
      'STARTROOM_UpperPassage_South':((-8680,-220,6000),(-6900,-150,6330)),
      'STARTROOM_Junction_Expanded_Floor':((-6900,-400,5930),(-6170,400,6000)),
      'STARTROOM_Junction_Expanded_Roof':((-6900,-400,6480),(-6170,400,6550)),
      'STARTROOM_Junction_Expanded_North':((-6900,320,6000),(-6170,400,6480)),
      'STARTROOM_Junction_Expanded_South':((-6900,-400,6000),(-6170,-320,6480)),
      'STARTROOM_Junction_Shoulder_North':((-6970,150,6000),(-6900,400,6550)),
      'STARTROOM_Junction_Shoulder_South':((-6970,-400,6000),(-6900,-150,6550)),
      'STARTROOM_Junction_Shoulder_Roof':((-6970,-150,6330),(-6900,150,6550)),
      'STARTROOM_Junction_Bedrock_North':((-6500,320,-100),(-6320,2000,7500)),
      'STARTROOM_Junction_Bedrock_South':((-6500,-3000,-100),(-6320,-320,7500)),
      'STARTROOM_Junction_Bedrock_Below':((-6500,-320,-100),(-6320,320,6000)),
      'STARTROOM_Junction_Bedrock_Above':((-6500,-320,6480),(-6320,320,7500)),
    }
    material=unreal.load_asset('/Game/MainGame/Materials/DirtTransition/MI_DirtWall_StartRoom')
    for label,(lo,hi) in slabs.items():
        a=named.get(label) or tag(actors.spawn_actor_from_class(unreal.StaticMeshActor,V(0,0,0)),label)
        a.static_mesh_component.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Cube'));a.static_mesh_component.set_material(0,material);a.static_mesh_component.set_collision_profile_name('BlockAll');bounds(a,lo,hi)
    for a in all_actors:
        if a.get_actor_label() in ['STARTROOM_UpperPassage_Frame','STARTROOM_UpperPassage_Lintel'] and a.get_actor_location().x>-6900:assert actors.destroy_actor(a)
    named['STARTROOM_RegionOneSign'].set_actor_location(V(-6790,314,6260),False,True)
    # Commit only the widened local shell doorway. Ordinary terrain and all
    # mineral host actor identities remain intact.
    before.reset();edits.append_mesh(before,cut,I);component.update_collision(False)
    plug=named['STARTROOM_Junction_DiggableBlockage'];plug.set_actor_location(V(*cfg['plug_position']),False,True)
    plug.set_editor_property('rock_size',V(*cfg['plug_size']),notify_mode=unreal.PropertyAccessChangeNotifyMode.NEVER);plug.reset_rock()
    plug.set_editor_property('tags',list(plug.tags)+['BM_MainGame_WideJunction'])
    # Re-seat entrance relief against the new walls rather than leaving the old
    # coating floating at the former 3m pipe surface.
    rng=random.Random(2026101505);mesh=unreal.DynamicMesh();samples=[];clay=named['STARTROOM_Junction_3D_ClayClods']
    def clod(anchor,normal,width,depth):
        piece=unreal.DynamicMesh();center=anchor-normal*1
        tr=unreal.Transform(location=unreal.MathLibrary.inverse_transform_location(clay.get_actor_transform(),center),rotation=unreal.MathLibrary.make_rot_from_z(normal),scale=V(width,width*rng.uniform(.7,1.1),depth))
        unreal.GeometryScript_Primitives.append_sphere_lat_long(piece,unreal.GeometryScriptPrimitiveOptions(),tr,radius=1,steps_phi=4,steps_theta=7)
        unreal.GeometryScript_Normals.set_per_face_normals(piece);edits.append_mesh(mesh,piece,I)
        samples.append(dict(position=list(anchor.to_tuple()),normal=list(normal.to_tuple()),depth_cm=depth,width_radius_cm=width,kind='junction'))
    for x in [-6800,-6660,-6520,-6380,-6250]:
        for side in [-1,1]:
            for z in [6070,6210,6380]:
                if rng.random()<.82:clod(V(x,side*320,z),V(0,-side,0),rng.uniform(18,30),rng.uniform(10,17))
        for y in [-150,150]:clod(V(x,y,6480),V(0,0,-1),rng.uniform(18,27),rng.uniform(10,14))
    assert queries.get_is_closed_mesh(mesh)
    vertices=unreal.GeometryScript_List.convert_vector_list_to_array(queries.get_all_vertex_positions(mesh,False)[1]);triangles=unreal.GeometryScript_List.convert_triangle_list_to_array(queries.get_all_triangle_indices(mesh,False)[1]);lines=[]
    for p in vertices:
        p=unreal.MathLibrary.transform_location(clay.get_actor_transform(),p);lines.append('v %.9f %.9f %.9f'%(p.x/100,-p.y/100,p.z/100))
    for t in triangles:lines.append('f %d %d %d'%(t.x+1,t.z+1,t.y+1))
    obj=OUT/'wide_junction_clods.obj';obj.write_text('\n'.join(lines)+'\n',encoding='utf-8');assert unreal.MiningDesignerLibrary.import_campaign_rock(clay,str(obj));clay.reset_rock()
    bake=json.loads(bake_path.read_text());bake['samples']=[s for s in bake['samples'] if s['kind']!='junction']+samples;bake.update(junction_clods=len(samples),clods=len(bake['samples']),junction_revision=cfg['revision']);bake_path.write_text(json.dumps(bake,ensure_ascii=False,indent=2),encoding='utf-8')
    manifest=json.loads(manifest_path.read_text());manifest.update(junction_blockage=cfg['plug_position'],junction_blockage_size=cfg['plug_size'],junction_half_width=cfg['half_width'],junction_floor_z=cfg['floor_z'],junction_roof_z=cfg['roof_z'],junction_wide_backup=str(backup));manifest_path.write_text(json.dumps(manifest,ensure_ascii=False,indent=2),encoding='utf-8')
    valid=minerals.validate(field,hall);assert valid['ok'] and valid['deposits']==165 and valid['layout_sha256']==mineral_sha
    assert (region.generated_rock.get_path_name(),region.generated_rock.triangle_count)==original_rock
    for p,d in protected.items():assert sha(ROOT/p)==d,p
    assert levels.save_current_level();assert levels.load_level(MAP)
    report.update(passed=True,saved_and_reloaded=True,width_cm=640,height_cm=480,plug_size_cm=cfg['plug_size'],junction_clods=len(samples),ordinary_terrain_preserved=True,mineral_layout_unchanged=True,protected_assets_unchanged=True)
except Exception:report['error']=traceback.format_exc();raise
finally:
    (OUT/'authoring.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8');unreal.log('WIDE_JUNCTION '+json.dumps(report,ensure_ascii=False))
