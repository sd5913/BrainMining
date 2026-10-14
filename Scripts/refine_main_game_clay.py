"""Match all clay shading to the blockage core and increase relief by 30%."""
import unreal,json,shutil,math,traceback,time
from pathlib import Path
from datetime import datetime
ROOT=Path(unreal.Paths.project_dir()).resolve();OUT=ROOT/'Saved/Validation/MainGameClayRefinement';OUT.mkdir(parents=True,exist_ok=True)
MAP='/Game/MainGame/Maps/Lvl_MainGame';FOLDER='/Game/MainGame/Materials/DirtTransition'
editor=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem);levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
queries=unreal.GeometryScript_MeshQueries;edits=unreal.GeometryScript_MeshEdits;lib=unreal.MaterialEditingLibrary
I=unreal.Transform();result={'passed':False,'stages':[]};start=time.monotonic()
def V(v):return unreal.Vector(*v)
def local(a,p):return unreal.MathLibrary.inverse_transform_location(a.get_actor_transform(),p)
def copy_mesh(mesh):
    out=unreal.DynamicMesh();edits.append_mesh(out,mesh,I);return out
def positions(mesh):return unreal.GeometryScript_List.convert_vector_list_to_array(queries.get_all_vertex_positions(mesh,False)[1])
def deform(mesh,records,actor,bounds=None,marked=True):
    points=positions(mesh)
    vertex_colors=None
    if marked:
        colors=unreal.GeometryScript_VertexColors.get_mesh_per_vertex_colors(mesh,True)
        assert colors[2]
        vertex_colors=unreal.GeometryScript_List.convert_color_list_to_array(colors[1])
    anchors=[]
    for r in records:
        p=local(actor,V(r['position']));n=local(actor,V(r['position'])+V(r['normal']))-p
        n=n/max(n.length(),.001);anchors.append((p,n,r))
    moved=clamped=0
    for i,p in enumerate(points):
        if vertex_colors and vertex_colors[i].r-vertex_colors[i].g<.6:continue
        a,n,r=min(anchors,key=lambda entry:p.distance(entry[0]))
        if r['normal'][2]>.55:continue
        delta=p-a;h=unreal.MathLibrary.dot_vector_vector(delta,n)
        if h<=.08 or h>r['depth_cm']*1.25:continue
        lateral=(delta-n*h).length()
        if lateral>r['width_radius_cm']*1.35:continue
        q=p+n*h*.30
        if bounds:
            before=q
            q=unreal.Vector(*[max(lo+.25,min(hi-.25,x)) for x,lo,hi in zip(q.to_tuple(),bounds.min.to_tuple(),bounds.max.to_tuple())])
            if q.distance(before)>.01:clamped+=1
        _,valid=edits.set_vertex_position(mesh,i,q,True);assert valid
        moved+=1
    unreal.GeometryScript_Normals.recompute_normals(mesh,unreal.GeometryScriptCalculateNormalsOptions())
    assert queries.get_is_closed_mesh(mesh),'Relief deformation opened the mesh'
    return moved,clamped
try:
    assert not editor.get_game_world()
    world=editor.get_editor_world();assert world.get_path_name()==MAP+'.Lvl_MainGame'
    all_actors=list(actors.get_all_level_actors())
    assert not any(a.actor_has_tag('BM_MainGame_ClayRelief130') for a in all_actors),'Already refined; restore backup before repeating.'
    assert levels.set_current_level_by_name('Lvl_MainGame') and levels.save_current_level()
    bake_path=ROOT/'Saved/Validation/MainGameDirt3D/bake.json';bake=json.loads(bake_path.read_text(encoding='utf-8'))
    backup=ROOT/'Saved/Backups'/('MainGame_ClayMatch_'+datetime.now().strftime('%Y%m%d-%H%M%S'));backup.mkdir(parents=True)
    shutil.copy2(ROOT/'Content/MainGame/Maps/Lvl_MainGame.umap',backup/'Lvl_MainGame.umap')
    shutil.copytree(ROOT/'Content/MainGame/Materials/DirtTransition',backup/'DirtTransition')
    shutil.copy2(bake_path,backup/'bake.json');result['backup']=str(backup)
    for stage in bake['stages']:
        rock=next(a.generated_rock for a in all_actors if isinstance(a,unreal.MiningExternalRegion) and a.actor_has_tag(stage['tag']))
        buckets={}
        for r in bake['samples']:
            if r['kind']==stage['tag']:buckets.setdefault(r['chunk'],[]).append(r)
        prepared=[];changed=limited=0
        for index,records in buckets.items():
            chunk=rock.chunks[index];mesh=copy_mesh(chunk.initial_mesh)
            islands=queries.get_num_connected_components(mesh)
            moved,clamped=deform(mesh,records,rock,chunk.bounds)
            assert queries.get_num_connected_components(mesh)==islands
            assert mesh.get_triangle_count()==chunk.initial_mesh.get_triangle_count()
            prepared.append((chunk.initial_mesh,mesh));changed+=moved;limited+=clamped
        assert changed>100
        for source,mesh in prepared:source.reset();edits.append_mesh(source,mesh,I)
        rock.set_editor_property('tags',list(rock.tags)+['BM_MainGame_ClayRelief130'])
        rock.reset_rock();assert rock.triangle_count==stage['after_triangles']
        result['stages'].append(dict(stage=stage['stage'],moved_vertices=changed,bounds_clamped_vertices=limited))
    joint=next(a for a in all_actors if a.actor_has_tag('BM_MainGame_3DDirtJunction'))
    mesh=copy_mesh(joint.get_combined_mesh())
    moved,_=deform(mesh,[r for r in bake['samples'] if r['kind']=='junction'],joint,marked=False)
    verts=positions(mesh);tris=unreal.GeometryScript_List.convert_triangle_list_to_array(queries.get_all_triangle_indices(mesh,False)[1])
    lines=[]
    for p in verts:
        p=unreal.MathLibrary.transform_location(joint.get_actor_transform(),p);lines.append('v %.9f %.9f %.9f'%(p.x/100,-p.y/100,p.z/100))
    for t in tris:lines.append('f %d %d %d'%(t.x+1,t.z+1,t.y+1))
    obj=OUT/'junction_clods_130.obj';obj.write_text('\n'.join(lines)+'\n',encoding='utf-8')
    initial_joint=joint.triangle_count
    assert unreal.MiningDesignerLibrary.import_campaign_rock(joint,str(obj))
    assert joint.triangle_count==initial_joint
    joint.set_editor_property('tags',list(joint.tags)+['BM_MainGame_ClayRelief130'])
    assert levels.save_current_level()
    # Compile only in an empty scene; do not repeatedly refresh the mining map.
    assert levels.load_level('/Engine/Maps/Entry')
    master=unreal.load_asset(FOLDER+'/M_DirtTransition')
    custom=next(n for n in lib.get_material_expressions(master) if isinstance(n,unreal.MaterialExpressionCustom))
    inputs=list(custom.get_editor_property('inputs'));names={str(v.get_editor_property('input_name')) for v in inputs}
    nodes={str(n.get_editor_property('parameter_name')):n for n in lib.get_material_expressions(master) if isinstance(n,unreal.MaterialExpressionScalarParameter)}
    for name in ['BaseSpecular','MudSpecular']:
        if name not in names:
            v=unreal.CustomInput();v.set_editor_property('input_name',name);inputs.append(v)
    custom.set_editor_property('inputs',inputs,notify_mode=unreal.PropertyAccessChangeNotifyMode.NEVER)
    spec=nodes['Specular']
    mud_spec=nodes.get('MudSpecular') or lib.create_material_expression(master,unreal.MaterialExpressionScalarParameter)
    mud_spec.set_editor_property('parameter_name','MudSpecular',notify_mode=unreal.PropertyAccessChangeNotifyMode.NEVER)
    mud_spec.set_editor_property('default_value',.06,notify_mode=unreal.PropertyAccessChangeNotifyMode.NEVER)
    mud_spec.set_editor_property('group','Dirt Surface',notify_mode=unreal.PropertyAccessChangeNotifyMode.NEVER)
    assert lib.connect_material_expressions(spec,'',custom,'BaseSpecular')
    assert lib.connect_material_expressions(mud_spec,'',custom,'MudSpecular')
    outputs=list(custom.get_editor_property('additional_outputs'))
    if 'SurfaceSpecular' not in [str(v.get_editor_property('output_name')) for v in outputs]:
        v=unreal.CustomOutput();v.set_editor_property('output_name','SurfaceSpecular');v.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT1);outputs.append(v)
        custom.set_editor_property('additional_outputs',outputs)
    overlay=(ROOT/'Art/MainGame/Materials/DirtTransition.hlsl').read_text(encoding='utf-8')
    overlay=overlay.replace('// __VOLUME_EVALUATION__','\n'.join('if(MudCount>%.1f) proximity=max(proximity,dirt.influence(P,MudStart%d,MudEnd%d,MudReach));'%(i+.5,i,i) for i in range(32)))
    code=(ROOT/'Art/MainGame/Materials/StageRockPattern.hlsl').read_text(encoding='utf-8').replace('return float4(max(color,0.001),clamp(rough,.10,.98));',overlay+'\nreturn float4(max(color,0.001),clamp(rough,.10,.98));')
    custom.set_editor_property('code',code,notify_mode=unreal.PropertyAccessChangeNotifyMode.NEVER)
    assert lib.connect_material_property(custom,'SurfaceSpecular',unreal.MaterialProperty.MP_SPECULAR)
    errors=lib.recompile_material(master);assert not errors,list(errors)
    assert unreal.EditorAssetLibrary.save_loaded_asset(master,False)
    core=unreal.load_asset(FOLDER+'/MI_ClayBlockage')
    scalar_names=['MudCellSize','MudDepth'];vector_names=['MudDark','MudLight']
    soil_scalars={k:lib.get_material_instance_scalar_parameter_value(core,k) for k in scalar_names}
    soil_scalars['MudSpecular']=.06
    soil_vectors={k:lib.get_material_instance_vector_parameter_value(core,k) for k in vector_names}
    for path in unreal.EditorAssetLibrary.list_assets(FOLDER):
        mat=unreal.load_asset(path)
        if not isinstance(mat,unreal.MaterialInstanceConstant):continue
        sv={str(v.get_editor_property('parameter_info').get_editor_property('name')):v for v in mat.get_editor_property('scalar_parameter_values')}
        vv={str(v.get_editor_property('parameter_info').get_editor_property('name')):v for v in mat.get_editor_property('vector_parameter_values')}
        for k,v in soil_scalars.items():sv[k]=unreal.ScalarParameterValue(parameter_info=unreal.MaterialParameterInfo(name=k),parameter_value=v)
        for k,v in soil_vectors.items():vv[k]=unreal.VectorParameterValue(parameter_info=unreal.MaterialParameterInfo(name=k),parameter_value=v)
        mat.set_editor_property('scalar_parameter_values',list(sv.values()),notify_mode=unreal.PropertyAccessChangeNotifyMode.NEVER)
        mat.set_editor_property('vector_parameter_values',list(vv.values()),notify_mode=unreal.PropertyAccessChangeNotifyMode.NEVER)
        lib.update_material_instance(mat);assert unreal.EditorAssetLibrary.save_loaded_asset(mat,False)
    for r in bake['samples']:
        r['previous_depth_cm']=r['depth_cm']
        if r['normal'][2]<=.55:r['depth_cm']*=1.3
    bake['clay_refinement_backup']=str(backup);bake['relief_scale']=1.3
    bake_path.write_text(json.dumps(bake,ensure_ascii=False,indent=2),encoding='utf-8')
    assert levels.load_level(MAP)
    result.update(passed=True,saved=True,relief_scale=1.3,topology_preserved=True,clay_material_unified=True,junction_moved_vertices=moved,elapsed_seconds=time.monotonic()-start)
except Exception:
    result['error']=traceback.format_exc();raise
finally:
    (OUT/'authoring.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
    unreal.log('CLAY_REFINEMENT '+json.dumps(result,ensure_ascii=False))
