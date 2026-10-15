"""MainGame-only hidden body and localized, excavatable clay transitions."""
import unreal,json,hashlib,shutil,traceback,bisect,sys
from pathlib import Path
from datetime import datetime
ROOT=Path(unreal.Paths.project_dir()).resolve()
sys.path.insert(0,str(ROOT/'Scripts'))
from main_game_route_sources import route_root
ROUTES=route_root(ROOT)
OUT=ROOT/'Saved/Validation/MainGameDirt';OUT.mkdir(parents=True,exist_ok=True)
FOLDER='/Game/MainGame/Materials/DirtTransition'
lib=unreal.MaterialEditingLibrary
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
editor=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
report={'passed':False}
STAGES=[('TunnelV1','BM_MainGame_V1'),('TunnelCavernV2','BM_MainGame_CavernV2'),('TunnelStalagmiteV3','BM_MainGame_StalagmiteV3'),('TunnelNeuralV4','BM_MainGame_NeuralV4')]
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def signature(r):
    return {'rock':r.generated_rock.get_path_name(),'triangles':r.generated_rock.triangle_count,'chunks':len(r.generated_rock.chunks),
        'shell':r.generated_shell.get_path_name(),'guides':[(g.get_path_name(),g.triangle_count,len(g.chunks),g.successful_cuts) for g in r.generated_guide_layers]}
def assign(rock,mat):
    rock.set_editor_property('rock_material',mat,notify_mode=unreal.PropertyAccessChangeNotifyMode.NEVER)
    for c in rock.get_components_by_class(unreal.DynamicMeshComponent):c.set_material(0,mat)
def point(path,s):
    i=max(1,min(len(path['arc'])-1,bisect.bisect_left(path['arc'],s)))
    t=(s-path['arc'][i-1])/(path['arc'][i]-path['arc'][i-1])
    p=[a+(b-a)*t for a,b in zip(path['points'][i-1],path['points'][i])]
    radius=path['radii'][i-1]+(path['radii'][i]-path['radii'][i-1])*t
    return unreal.Vector(p[0]*100,-p[1]*100,p[2]*100),radius*100
def duplicate(src,dst):
    obj=unreal.load_asset(dst) if unreal.EditorAssetLibrary.does_asset_exist(dst) else unreal.EditorAssetLibrary.duplicate_asset(src,dst)
    assert obj,dst
    return obj
def vec(v,r):return unreal.LinearColor(v.x,v.y,v.z,r)
def instance(src,name,parent,volumes,core=0):
    m=duplicate(src,FOLDER+'/'+name)
    lib.set_material_instance_parent(m,parent)
    scalars={str(v.get_editor_property('parameter_info').get_editor_property('name')):v for v in m.get_editor_property('scalar_parameter_values')}
    for k,v in [('MudCount',len(volumes)),('MudCore',core),('MudReach',240),('MudCellSize',45),('MudDepth',2.5)]:
        scalars[k]=unreal.ScalarParameterValue(parameter_info=unreal.MaterialParameterInfo(name=k),parameter_value=float(v))
    vectors={str(v.get_editor_property('parameter_info').get_editor_property('name')):v for v in m.get_editor_property('vector_parameter_values')}
    for i,(a,b) in enumerate(volumes):
        for k,v in [('MudStart'+str(i),a),('MudEnd'+str(i),b)]:
            vectors[k]=unreal.VectorParameterValue(parameter_info=unreal.MaterialParameterInfo(name=k),parameter_value=v)
    for k,v in [('MudDark',unreal.LinearColor(.028,.009,.003,1)),('MudLight',unreal.LinearColor(.12,.04,.012,1))]:
        vectors[k]=unreal.VectorParameterValue(parameter_info=unreal.MaterialParameterInfo(name=k),parameter_value=v)
    if core:
        scalars['Specular']=unreal.ScalarParameterValue(parameter_info=unreal.MaterialParameterInfo(name='Specular'),parameter_value=.06)
    m.set_editor_property('scalar_parameter_values',list(scalars.values()),notify_mode=unreal.PropertyAccessChangeNotifyMode.NEVER)
    m.set_editor_property('vector_parameter_values',list(vectors.values()),notify_mode=unreal.PropertyAccessChangeNotifyMode.NEVER)
    lib.update_material_instance(m)
    assert lib.get_material_instance_scalar_parameter_value(m,'MudCount')==len(volumes)
    assert unreal.EditorAssetLibrary.save_loaded_asset(m,False)
    return m
try:
    assert not editor.get_game_world()
    if editor.get_editor_world().get_path_name()=='/Game/MainGame/Maps/Lvl_MainGame.Lvl_MainGame':
        assert levels.set_current_level_by_name('Lvl_MainGame') and levels.save_current_level()
        assert levels.load_level('/Engine/Maps/Entry')
    bp=unreal.load_asset('/Game/MainGame/Blueprints/BP_MainGameCharacter')
    assert unreal.EditorAssetLibrary.save_loaded_asset(bp,False)
    backup=ROOT/'Saved/Backups'/('MainGame_DirtTransition_'+datetime.now().strftime('%Y%m%d-%H%M%S'));backup.mkdir(parents=True)
    for rel in ['Content/MainGame/Maps/Lvl_MainGame.umap','Content/MainGame/Blueprints/BP_MainGameCharacter.uasset']:
        shutil.copy2(ROOT/rel,backup/Path(rel).name)
    protected=['Content/StartRoom/Maps/Lvl_StartRoom.umap','Content/Campaign/Maps/Lvl_FullJourney.umap',
        'Content/Showcase/Blueprints/BP_SharedMiningCharacter.uasset','Content/MainGame/Materials/M_MainGameStageRock.uasset',
        'Content/Campaign/Materials/M_CampaignGuideRock.uasset']
    hashes={p:sha(ROOT/p) for p in protected}
    # Hide only the two skeletal bodies. Do not propagate to camera/tool children.
    subs=unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem);data=unreal.SubobjectDataBlueprintFunctionLibrary
    bodies=[]
    for h in subs.k2_gather_subobject_data_for_blueprint(bp):
        c=data.get_object_for_blueprint(data.get_data(h),bp)
        if isinstance(c,unreal.SkeletalMeshComponent):
            c.set_editor_property('hidden_in_game',True)
            c.set_editor_property('cast_shadow',False)
            c.set_editor_property('cast_hidden_shadow',False)
            c.set_editor_property('visibility_based_anim_tick_option',unreal.VisibilityBasedAnimTickOption.ALWAYS_TICK_POSE_AND_REFRESH_BONES)
            bodies.append(c.get_name())
    assert len(bodies)==2
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    assert all(c.get_editor_property('hidden_in_game') for c in unreal.get_default_object(bp.generated_class()).get_components_by_class(unreal.SkeletalMeshComponent))
    assert unreal.EditorAssetLibrary.save_loaded_asset(bp,False)
    # Extend the existing PBR rock graph and its world-space normal function.
    master=duplicate('/Game/MainGame/Materials/M_MainGameStageRock',FOLDER+'/M_DirtTransition')
    custom=next(n for n in lib.get_material_expressions(master) if isinstance(n,unreal.MaterialExpressionCustom))
    inputs=list(custom.get_editor_property('inputs'))
    original={str(i.get_editor_property('input_name')) for i in inputs}
    params={'MudCount':0.,'MudCore':0.,'MudReach':240.,'MudCellSize':45.,'MudDepth':2.5,'MudSpecular':.06,
        'MudDark':unreal.LinearColor(.075,.028,.010,1),'MudLight':unreal.LinearColor(.27,.115,.038,1)}
    for i in range(32):
        params['MudStart'+str(i)]=unreal.LinearColor(0,0,0,1)
        params['MudEnd'+str(i)]=unreal.LinearColor(1,0,0,1)
    nodes={str(n.get_editor_property('parameter_name')):n for n in lib.get_material_expressions(master) if isinstance(n,(unreal.MaterialExpressionScalarParameter,unreal.MaterialExpressionVectorParameter))}
    for name,val in params.items():
        if name not in original:
            entry=unreal.CustomInput();entry.set_editor_property('input_name',name);inputs.append(entry)
        node=nodes.get(name) or lib.create_material_expression(master,unreal.MaterialExpressionVectorParameter if isinstance(val,unreal.LinearColor) else unreal.MaterialExpressionScalarParameter)
        node.set_editor_property('parameter_name',name,notify_mode=unreal.PropertyAccessChangeNotifyMode.NEVER)
        node.set_editor_property('default_value',val,notify_mode=unreal.PropertyAccessChangeNotifyMode.NEVER)
        node.set_editor_property('group','Dirt Volumes' if name.startswith(('MudStart','MudEnd')) else 'Dirt Surface',notify_mode=unreal.PropertyAccessChangeNotifyMode.NEVER)
        nodes[name]=node
    custom.set_editor_property('inputs',inputs,notify_mode=unreal.PropertyAccessChangeNotifyMode.NEVER)
    for name,val in params.items():assert lib.connect_material_expressions(nodes[name],'RGBA' if isinstance(val,unreal.LinearColor) else '',custom,name)
    if 'ClodColor' not in original:
        v=unreal.CustomInput();v.set_editor_property('input_name','ClodColor');inputs.append(v)
        custom.set_editor_property('inputs',inputs,notify_mode=unreal.PropertyAccessChangeNotifyMode.NEVER)
    vertex=next((n for n in lib.get_material_expressions(master) if isinstance(n,unreal.MaterialExpressionVertexColor)),None) or lib.create_material_expression(master,unreal.MaterialExpressionVertexColor)
    assert lib.connect_material_expressions(vertex,'',custom,'ClodColor')
    if 'BaseSpecular' not in original:
        v=unreal.CustomInput();v.set_editor_property('input_name','BaseSpecular');inputs.append(v)
        custom.set_editor_property('inputs',inputs,notify_mode=unreal.PropertyAccessChangeNotifyMode.NEVER)
    assert lib.connect_material_expressions(nodes['Specular'],'',custom,'BaseSpecular')
    outputs=list(custom.get_editor_property('additional_outputs'))
    if 'SurfaceSpecular' not in [str(v.get_editor_property('output_name')) for v in outputs]:
        v=unreal.CustomOutput();v.set_editor_property('output_name','SurfaceSpecular');v.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT1);outputs.append(v)
        custom.set_editor_property('additional_outputs',outputs)
    assert lib.connect_material_property(custom,'SurfaceSpecular',unreal.MaterialProperty.MP_SPECULAR)
    overlay=(ROOT/'Art/MainGame/Materials/DirtTransition.hlsl').read_text(encoding='utf-8')
    evaluation='\n'.join('if(MudCount>%.1f) proximity=max(proximity,dirt.influence(P,MudStart%d,MudEnd%d,MudReach));'%(i+.5,i,i) for i in range(32))
    overlay=overlay.replace('// __VOLUME_EVALUATION__',evaluation)
    original_code=(ROOT/'Art/MainGame/Materials/StageRockPattern.hlsl').read_text(encoding='utf-8')
    code=original_code.replace('return float4(max(color,0.001),clamp(rough,.10,.98));',overlay+'\nreturn float4(max(color,0.001),clamp(rough,.10,.98));')
    custom.set_editor_property('code',code,notify_mode=unreal.PropertyAccessChangeNotifyMode.NEVER)
    lib.layout_material_expressions(master)
    errors=lib.recompile_material(master);assert not errors,list(errors)
    assert unreal.EditorAssetLibrary.save_loaded_asset(master,False)
    unreal.log('DIRT_MASTER_COMPILED')
    assert levels.load_level('/Game/MainGame/Maps/Lvl_MainGame')
    world=editor.get_editor_world()
    recipe=json.loads((ROOT/'Config/MainGameRockThemes.json').read_text(encoding='utf-8'))
    regions=list(unreal.GameplayStatics.get_all_actors_of_class(world,unreal.MiningExternalRegion))
    before={r.get_path_name():signature(r) for r in regions}
    original_mats={r.get_path_name():r.rock_material.get_path_name() for r in regions}
    baseline=recipe['folder']+'/'+recipe['themes'][0]['name']
    mud=instance(baseline,'MI_ClayBlockage',master,[],core=1)
    changed=[]
    for idx,(stage,tag) in enumerate(STAGES):
        r=next(r for r in regions if r.actor_has_tag(tag))
        paths=json.loads((ROUTES/stage/'centerlines.json').read_text(encoding='utf-8'))
        plan=json.loads((ROUTES/stage/'placement_plan.json').read_text(encoding='utf-8'))['objects']
        assert len(plan)==len(r.generated_guide_layers)==16
        volumes=[]
        origin=r.get_actor_location()+r.cutters[0].local_transform.translation
        for record in plan:
            path=paths[record['path_index']];start=record['start_arc_m'];end=record['end_arc_m'];middle=(start+end)*.5
            for sa,sb in [(start,middle),(middle,end)]:
                a,ra=point(path,sa);b,rb=point(path,sb)
                volumes.append((vec(a+origin,ra),vec(b+origin,rb)))
        mat=instance(recipe['folder']+'/'+recipe['themes'][idx]['name'],'MI_DirtWall_%02d'%(idx+1),master,volumes)
        r.set_editor_property('rock_material',mat,notify_mode=unreal.PropertyAccessChangeNotifyMode.NEVER)
        r.set_editor_property('guide_material',mud,notify_mode=unreal.PropertyAccessChangeNotifyMode.NEVER)
        assign(r.generated_rock,mat)
        for g in r.generated_guide_layers:assign(g,mud)
        changed.append(dict(tag=tag,stage=stage,wall=mat.get_path_name(),guides=16,volumes=len(volumes)))
    joint=instance(baseline,'MI_DirtWall_StartRoom',master,[(unreal.LinearColor(-6440,0,6170,230),unreal.LinearColor(-6260,0,6170,230))])
    plug=next(a for a in actors.get_all_level_actors() if a.get_actor_label()=='STARTROOM_Junction_DiggableBlockage')
    assign(plug,mud)
    junction=[]
    for a in actors.get_all_level_actors():
        if (a.get_actor_label().startswith('STARTROOM_Junction_Bedrock_') or a.get_actor_label() in ['STARTROOM_UpperPassage_Floor','STARTROOM_UpperPassage_Roof','STARTROOM_UpperPassage_North','STARTROOM_UpperPassage_South']) and isinstance(a,unreal.StaticMeshActor):
            a.static_mesh_component.set_material(0,joint);junction.append(a.get_actor_label())
    assert all(signature(r)==before[r.get_path_name()] for r in regions),'Terrain geometry changed.'
    assert len(next(a for a in actors.get_all_level_actors() if isinstance(a,unreal.MiningTreasureField)).get_editor_property('authored_placements'))==125
    assert levels.save_current_level()
    assert all(sha(ROOT/p)==digest for p,digest in hashes.items()),'A shared source asset changed.'
    report.update(passed=True,saved=True,hidden_bodies=bodies,regions=changed,guide_count=64,junction_surfaces=junction,
        protected_hashes=hashes,geometry_preserved=True,minerals_preserved=125,backup=str(backup),original_materials=original_mats)
except Exception:
    report['error']=traceback.format_exc();raise
finally:
    (OUT/'authoring.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    unreal.log('MAIN_GAME_DIRT '+json.dumps(report,ensure_ascii=False))
