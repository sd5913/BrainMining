"""Bake closed, faceted clay clods into MainGame's editable terrain chunks.

Clods are unioned with the host rock, rather than spawned as floating decals.
The unmineable entrance receives a separately mineable, resettable clay mesh.
"""
import unreal,json,math,random,bisect,shutil,time,traceback
from pathlib import Path
from datetime import datetime
ROOT=Path(unreal.Paths.project_dir()).resolve();OUT=ROOT/'Saved/Validation/MainGameDirt3D';OUT.mkdir(parents=True,exist_ok=True)
editor=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem);levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
edits=unreal.GeometryScript_MeshEdits;queries=unreal.GeometryScript_MeshQueries;colors=unreal.GeometryScript_VertexColors
stages=[('TunnelV1','BM_MainGame_V1'),('TunnelCavernV2','BM_MainGame_CavernV2'),('TunnelStalagmiteV3','BM_MainGame_StalagmiteV3'),('TunnelNeuralV4','BM_MainGame_NeuralV4')]
report={'passed':False,'stages':[]};rng=random.Random(2026101403);started=time.monotonic()
I=unreal.Transform()
def V(x=0,y=0,z=0):return unreal.Vector(x,y,z)
def local(actor,p):return unreal.MathLibrary.inverse_transform_location(actor.get_actor_transform(),p)
def clone(mesh,outer):
    result=unreal.new_object(unreal.DynamicMesh,outer=outer)
    edits.append_mesh(result,mesh,I);return result
def sample(path,s):
    s=max(0,min(path['arc'][-1],s));i=max(1,min(len(path['arc'])-1,bisect.bisect_left(path['arc'],s)))
    t=(s-path['arc'][i-1])/(path['arc'][i]-path['arc'][i-1])
    p=[a+(b-a)*t for a,b in zip(path['points'][i-1],path['points'][i])]
    r=path['radii'][i-1]+(path['radii'][i]-path['radii'][i-1])*t
    return V(p[0]*100,-p[1]*100,p[2]*100),r*100
def trace(a,b):
    h=unreal.SystemLibrary.line_trace_single(world,a,b,unreal.TraceTypeQuery.ECC_VISIBILITY,True,ignore,unreal.DrawDebugTrace.NONE,True)
    return h.to_tuple() if h else None
def distance2(p,b):
    return sum(max(a-v,0,v-z)**2 for v,a,z in zip(p.to_tuple(),b.min.to_tuple(),b.max.to_tuple()))
def clod(center,normal,width,depth):
    m=unreal.DynamicMesh()
    transform=unreal.Transform(location=center+normal*depth*.08,rotation=unreal.MathLibrary.make_rot_from_z(normal),scale=V(width,width*rng.uniform(.64,1.15),depth))
    unreal.GeometryScript_Primitives.append_sphere_lat_long(m,unreal.GeometryScriptPrimitiveOptions(),transform,radius=1,steps_phi=4,steps_theta=rng.choice([6,7,8]))
    unreal.GeometryScript_Normals.set_per_face_normals(m)
    colors.set_mesh_constant_vertex_color(m,unreal.LinearColor(1,0,0,1),unreal.GeometryScriptColorFlags(),True)
    assert queries.get_is_closed_mesh(m)
    return m
def record_point(p,n,depth,width,kind):
    return dict(position=list(p.to_tuple()),normal=list(n.to_tuple()),depth_cm=depth,width_radius_cm=width,kind=kind)
try:
    assert not editor.get_game_world()
    world=editor.get_editor_world();assert world.get_path_name()=='/Game/MainGame/Maps/Lvl_MainGame.Lvl_MainGame'
    assert not any(a.actor_has_tag('BM_MainGame_3DDirtBaked') for a in actors.get_all_level_actors()),'3D soil already baked; restore the backup before rebaking.'
    assert levels.set_current_level_by_name('Lvl_MainGame') and levels.save_current_level()
    backup=ROOT/'Saved/Backups'/('MainGame_3DDirt_'+datetime.now().strftime('%Y%m%d-%H%M%S'));backup.mkdir(parents=True)
    shutil.copy2(ROOT/'Content/MainGame/Maps/Lvl_MainGame.umap',backup/'Lvl_MainGame.umap')
    report['backup']=str(backup)
    regions=list(unreal.GameplayStatics.get_all_actors_of_class(world,unreal.MiningExternalRegion))
    ignore=[g for r in regions for g in r.generated_guide_layers]
    ignore += [a for a in actors.get_all_level_actors() if a.actor_has_tag('BM_StartRoom_ExitBlocker') or a.get_actor_label()=='STARTROOM_Junction_DiggableBlockage']
    all_records=[]
    for stage,tag in stages:
        region=next(r for r in regions if r.actor_has_tag(tag));rock=region.generated_rock
        assert rock.get_editor_property('terrain_source') is None,'Clone shared terrain sources before editing.'
        paths=json.loads((ROOT/'Art/MainGame'/stage/'centerlines.json').read_text(encoding='utf-8'))
        plan=json.loads((ROOT/'Art/MainGame'/stage/'placement_plan.json').read_text(encoding='utf-8'))['objects']
        origin=region.get_actor_location()+region.cutters[0].local_transform.translation
        chunks=list(rock.chunks);buckets={};records=[];accepted=[]
        for layer in plan:
            path=paths[layer['path_index']]
            for end,sign in [(layer['start_arc_m'],-1),(layer['end_arc_m'],1)]:
                for offset,prob in [(15,.92),(85,.66),(175,.40)]:
                    s=end+sign*offset/100.;p,r=sample(path,s);a,_=sample(path,s-.1);b,_=sample(path,s+.1)
                    tangent=b-a;tangent=tangent/max(tangent.length(),.001)
                    right=V(-tangent.y,tangent.x,0);right=right/max(right.length(),.001)
                    for k in range(6):
                        if rng.random()>prob:continue
                        angle=(k+rng.uniform(-.22,.22))*math.tau/6
                        direction=right*math.cos(angle)+V(0,0,math.sin(angle))
                        hit=trace(origin+p,origin+p+direction*r*2.1)
                        if not hit or not hit[0] or hit[9]!=rock:continue
                        anchor,normal=hit[5],hit[7]
                        width=rng.uniform(13,34);depth=rng.uniform(6,16)
                        if normal.z>.55:depth=min(depth,7.)
                        if any(anchor.distance(q)<width+qw+8 for q,qw in accepted):continue
                        lp=local(rock,anchor)
                        index=min(range(len(chunks)),key=lambda i:distance2(lp,chunks[i].bounds))
                        if distance2(lp,chunks[index].bounds)>25:continue
                        piece=clod(lp,normal,width,depth)
                        pb=queries.get_mesh_bounding_box(piece);cb=chunks[index].bounds
                        if any(a<b+1 for a,b in zip(pb.min.to_tuple(),cb.min.to_tuple())) or any(a>b-1 for a,b in zip(pb.max.to_tuple(),cb.max.to_tuple())):continue
                        if index not in buckets:buckets[index]=unreal.DynamicMesh()
                        edits.append_mesh(buckets[index],piece,I)
                        rec=record_point(anchor,normal,depth,width,tag);rec['chunk']=index
                        records.append(rec);accepted.append((anchor,width))
        assert len(records)>80,(tag,len(records))
        prepared=[];before=rock.triangle_count
        for index,tool in buckets.items():
            mesh=clone(chunks[index].initial_mesh,rock)
            colors.set_mesh_constant_vertex_color(mesh,unreal.LinearColor(1,1,1,1),unreal.GeometryScriptColorFlags(),True)
            was_closed=queries.get_is_closed_mesh(mesh)
            count=mesh.get_triangle_count()
            unreal.GeometryScript_MeshBooleans.apply_mesh_boolean(mesh,I,tool,I,unreal.GeometryScriptBooleanOperation.UNION,unreal.GeometryScriptMeshBooleanOptions(fill_holes=True,simplify_output=False))
            assert mesh.get_triangle_count()>count,(tag,index,'Union removed existing terrain')
            assert not was_closed or queries.get_is_closed_mesh(mesh),(tag,index,'Open result')
            prepared.append((index,mesh))
        for index,mesh in prepared:
            # The chunk reference/bounds are read-only. Edit its actor-owned
            # DynamicMesh contents in place, strictly inside its existing bounds.
            original=chunks[index].initial_mesh
            original.reset();edits.append_mesh(original,mesh,I)
        rock.set_editor_property('tags',list(rock.tags)+['BM_MainGame_3DDirtBaked'])
        rock.reset_rock()
        assert rock.triangle_count>before
        report['stages'].append(dict(stage=stage,tag=tag,clods=len(records),changed_chunks=len(prepared),before_triangles=before,after_triangles=rock.triangle_count))
        all_records+=records
        unreal.log('3D_DIRT_STAGE '+json.dumps(report['stages'][-1]))
    # Permanent junction walls cannot lose their support. Their clay coating is
    # nevertheless a normal mineable GeneratedMiningRock and belongs to area R.
    joint=unreal.DynamicMesh();joint_records=[]
    for x in [-6650,-6530,-6410,-6290]:
        for side in [-1,1]:
            for z in [6050,6160,6270]:
                if rng.random()>.80:continue
                hit=trace(V(x,0,z),V(x,side*280,z+rng.uniform(-20,20)))
                if not hit or not hit[0] or not isinstance(hit[9],unreal.StaticMeshActor):continue
                width=rng.uniform(14,30);depth=rng.uniform(6,13)
                edits.append_mesh(joint,clod(hit[5]-V(-6350,0,6170),hit[7],width,depth),I)
                joint_records.append(record_point(hit[5],hit[7],depth,width,'junction'))
        for y in [-95,95]:
            hit=trace(V(x,y,6160),V(x,y,6450))
            if not hit or not hit[0]:continue
            width=rng.uniform(14,23);depth=rng.uniform(6,11)
            edits.append_mesh(joint,clod(hit[5]-V(-6350,0,6170),hit[7],width,depth),I)
            joint_records.append(record_point(hit[5],hit[7],depth,width,'junction'))
    assert len(joint_records)>15
    actor=actors.spawn_actor_from_class(unreal.GeneratedMiningRock,V(-6350,0,6170))
    actor.set_actor_label('STARTROOM_Junction_3D_ClayClods');actor.set_folder_path('MainGame/StartRoomConnection/MineJunction')
    actor.set_editor_property('tags',['BM_Campaign_MainGame01','BM_MainGame_3DDirtBaked','BM_MainGame_3DDirtJunction'])
    actor.set_editor_property('rock_material',unreal.load_asset('/Game/MainGame/Materials/DirtTransition/MI_ClayBlockage'),notify_mode=unreal.PropertyAccessChangeNotifyMode.NEVER)
    # Use the project's native, closed-solid importer to initialize the private
    # generated source without bypassing read-only UObject properties.
    verts=unreal.GeometryScript_List.convert_vector_list_to_array(queries.get_all_vertex_positions(joint,False)[1])
    tris=unreal.GeometryScript_List.convert_triangle_list_to_array(queries.get_all_triangle_indices(joint,False)[1])
    lines=[]
    for p in verts:
        p=p+V(-6350,0,6170);lines.append('v %.9f %.9f %.9f'%(p.x/100,-p.y/100,p.z/100))
    for t in tris:lines.append('f %d %d %d'%(t.x+1,t.z+1,t.y+1))
    obj=OUT/'junction_clods.obj';obj.write_text('\n'.join(lines)+'\n',encoding='utf-8')
    assert unreal.MiningDesignerLibrary.import_campaign_rock(actor,str(obj))
    actor.reset_rock();assert actor.triangle_count>0
    assert levels.save_current_level()
    all_records+=joint_records
    report.update(passed=True,saved=True,clods=len(all_records),junction_clods=len(joint_records),samples=all_records,elapsed_seconds=time.monotonic()-started)
except Exception:
    report['error']=traceback.format_exc();raise
finally:
    (OUT/'bake.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    unreal.log('3D_DIRT_BAKE '+json.dumps({k:v for k,v in report.items() if k!='samples'},ensure_ascii=False))
