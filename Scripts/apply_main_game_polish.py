"""MainGame: half as many plugs, no corridor fill lights, matte earth materials.

Existing route rock meshes, scenery, entry/exit transforms and StartRoom assets
are retained. Native guide regeneration removes disabled plugs from R resets.
"""
import unreal,json,time,shutil,hashlib,traceback,bisect
from pathlib import Path
from datetime import datetime
ROOT=Path(unreal.Paths.project_dir()).resolve();OUT=ROOT/'Saved/Validation/MainGamePolish';OUT.mkdir(exist_ok=True)
MAP='/Game/MainGame/Maps/Lvl_MainGame';FOLDER='/Game/MainGame/Materials/DirtTransition'
settings=json.loads((ROOT/'Config/MainGameTuning.json').read_text());tone=settings['material'];keep=settings['active_blocker_indices']
ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem);levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);lib=unreal.MaterialEditingLibrary
path=OUT/'authoring.json';state=json.loads(path.read_text()) if path.exists() else dict(passed=False,completed_regions=[])
assert not ed.get_game_world();assert not state['passed'],'Already applied'
def log(**kw):state.update(kw);path.write_text(json.dumps(state,ensure_ascii=False,indent=2),encoding='utf-8');unreal.log('POLISH '+json.dumps(kw,ensure_ascii=False))
def C(v):return unreal.LinearColor(*v,1)
def params(m,scalars,vectors):
    s={str(p.parameter_info.name):p for p in m.get_editor_property('scalar_parameter_values')};v={str(p.parameter_info.name):p for p in m.get_editor_property('vector_parameter_values')}
    for k,x in scalars.items():s[k]=unreal.ScalarParameterValue(parameter_info=unreal.MaterialParameterInfo(name=k),parameter_value=float(x))
    for k,x in vectors.items():v[k]=unreal.VectorParameterValue(parameter_info=unreal.MaterialParameterInfo(name=k),parameter_value=x)
    m.set_editor_property('scalar_parameter_values',list(s.values()),notify_mode=unreal.PropertyAccessChangeNotifyMode.NEVER)
    m.set_editor_property('vector_parameter_values',list(v.values()),notify_mode=unreal.PropertyAccessChangeNotifyMode.NEVER)
    lib.update_material_instance(m);assert unreal.EditorAssetLibrary.save_loaded_asset(m,False)
def assign(rock,m):
    rock.set_editor_property('rock_material',m,notify_mode=unreal.PropertyAccessChangeNotifyMode.NEVER)
    for c in rock.get_components_by_class(unreal.DynamicMeshComponent):c.set_material(0,m)
def point(p,s):
    i=max(1,min(len(p['arc'])-1,bisect.bisect_left(p['arc'],s)));u=(s-p['arc'][i-1])/(p['arc'][i]-p['arc'][i-1]);q=[a+(b-a)*u for a,b in zip(p['points'][i-1],p['points'][i])]
    return unreal.Vector(q[0]*100,-q[1]*100,q[2]*100),(p['radii'][i-1]+(p['radii'][i]-p['radii'][i-1])*u)*100
try:
    if 'backup' not in state:
        assert ed.get_editor_world().get_path_name()==MAP+'.Lvl_MainGame'
        assert levels.set_current_level_by_name('Lvl_MainGame') and levels.save_current_level()
        backup=ROOT/'Saved/Backups'/('MainGame_Polish_'+datetime.now().strftime('%Y%m%d-%H%M%S'));backup.mkdir(parents=True)
        shutil.copy2(ROOT/'Content/MainGame/Maps/Lvl_MainGame.umap',backup/'Lvl_MainGame.umap');shutil.copytree(ROOT/'Content/MainGame/Materials/DirtTransition',backup/'DirtTransition')
        shutil.copy2(ROOT/'Config/MainGameMinerals.json',backup/'MainGameMinerals.json')
        protected=json.loads((ROOT/'Saved/Validation/MainGameRoutesV2/authoring.json').read_text())['protected_hashes']
        before={r.get_path_name():dict(rock=r.generated_rock.get_path_name(),triangles=r.generated_rock.triangle_count) for r in actors.get_all_level_actors() if isinstance(r,unreal.MiningExternalRegion)}
        log(backup=str(backup),protected_hashes=protected,geometry_before=before,phase='backup')
    if not state.get('material_saved'):
        assert levels.load_level('/Engine/Maps/Entry')
        m=unreal.load_asset(FOLDER+'/M_DirtTransition');custom=next(n for n in lib.get_material_expressions(m) if isinstance(n,unreal.MaterialExpressionCustom))
        scalar=dict(MossAmount=tone['moss_amount'],MossScale=tone['moss_scale'])
        vector=dict(MossDark=C(tone['moss_dark']),MossLight=C(tone['moss_light']))
        inputs=list(custom.get_editor_property('inputs'));names={str(v.get_editor_property('input_name')) for v in inputs}
        nodes={str(n.get_editor_property('parameter_name')):n for n in lib.get_material_expressions(m) if isinstance(n,(unreal.MaterialExpressionScalarParameter,unreal.MaterialExpressionVectorParameter))}
        for k,x in {**scalar,**vector}.items():
            if k not in names:
                entry=unreal.CustomInput();entry.set_editor_property('input_name',k);inputs.append(entry)
            node=nodes.get(k) or lib.create_material_expression(m,unreal.MaterialExpressionVectorParameter if k in vector else unreal.MaterialExpressionScalarParameter)
            node.set_editor_property('parameter_name',k,notify_mode=unreal.PropertyAccessChangeNotifyMode.NEVER);node.set_editor_property('default_value',x,notify_mode=unreal.PropertyAccessChangeNotifyMode.NEVER);node.set_editor_property('group','Moss',notify_mode=unreal.PropertyAccessChangeNotifyMode.NEVER);nodes[k]=node
        custom.set_editor_property('inputs',inputs,notify_mode=unreal.PropertyAccessChangeNotifyMode.NEVER)
        for k in {**scalar,**vector}:assert lib.connect_material_expressions(nodes[k],'RGBA' if k in vector else '',custom,k)
        code=(ROOT/'Art/MainGame/Materials/DryRockMoss.hlsl').read_text()
        evaluation='\n'.join('if(MudCount>%.1f) proximity=max(proximity,dirt.influence(P,MudStart%d,MudEnd%d,MudReach));'%(i+.5,i,i) for i in range(32))
        custom.set_editor_property('code',code.replace('// __VOLUME_EVALUATION__',evaluation),notify_mode=unreal.PropertyAccessChangeNotifyMode.NEVER)
        m.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_DEFAULT_LIT,notify_mode=unreal.PropertyAccessChangeNotifyMode.NEVER)
        zero=next((n for n in lib.get_material_expressions(m) if isinstance(n,unreal.MaterialExpressionConstant) and n.get_editor_property('desc')=='MainGame Matte Zero'),None) or lib.create_material_expression(m,unreal.MaterialExpressionConstant)
        one=next((n for n in lib.get_material_expressions(m) if isinstance(n,unreal.MaterialExpressionConstant) and n.get_editor_property('desc')=='MainGame Matte One'),None) or lib.create_material_expression(m,unreal.MaterialExpressionConstant)
        for node,value,desc in [(zero,0,'MainGame Matte Zero'),(one,1,'MainGame Matte One')]:node.set_editor_property('r',value,notify_mode=unreal.PropertyAccessChangeNotifyMode.NEVER);node.set_editor_property('desc',desc,notify_mode=unreal.PropertyAccessChangeNotifyMode.NEVER)
        for prop in [unreal.MaterialProperty.MP_SPECULAR,unreal.MaterialProperty.MP_METALLIC,unreal.MaterialProperty.MP_EMISSIVE_COLOR]:assert lib.connect_material_property(zero,'',prop)
        assert lib.connect_material_property(one,'',unreal.MaterialProperty.MP_ROUGHNESS)
        errors=lib.recompile_material(m);assert not errors,list(errors);assert unreal.EditorAssetLibrary.save_loaded_asset(m,False)
        scalar.update(Theme=4,Roughness=1,RoughnessVariation=0,Specular=0,Metallic=0,GlowStrength=0,MudSpecular=0,PatternScale=.013,DetailScale=.085,BumpDepth=.6,MudDepth=1)
        vector.update(Tint=C(tone['stone_dark']),MineralLight=C(tone['stone_light']),Accent=C(tone['stone_dark']),MudDark=C(tone['soil_dark']),MudLight=C(tone['soil_light']))
        for p in unreal.EditorAssetLibrary.list_assets(FOLDER):
            mi=unreal.load_asset(p)
            if isinstance(mi,unreal.MaterialInstanceConstant):params(mi,scalar,vector)
        log(material_saved=True,phase='material_complete')
    assert levels.load_level(MAP);world=ed.get_editor_world();assert levels.set_current_level_by_name('Lvl_MainGame')
    removed=list(state.get('removed_lights',[]))
    for a in list(actors.get_all_level_actors()):
        if a.get_level().get_path_name().startswith(MAP+'.') and a.get_components_by_class(unreal.LightComponent):
            removed.append(a.get_actor_label());assert actors.destroy_actor(a)
    log(removed_lights=removed)
    tags=['BM_MainGame_V1','BM_MainGame_CavernV2','BM_MainGame_StalagmiteV3','BM_MainGame_NeuralV4'];stages=['TunnelV1','TunnelCavernV2','TunnelStalagmiteV3','TunnelNeuralV4'];core=unreal.load_asset(FOLDER+'/MI_ClayBlockage')
    regions=[a for a in actors.get_all_level_actors() if isinstance(a,unreal.MiningExternalRegion)]
    for index,(tag,stage) in enumerate(zip(tags,stages)):
        r=next(r for r in regions if r.actor_has_tag(tag));mi=unreal.load_asset(FOLDER+'/MI_DirtWall_%02d'%(index+1))
        if stage not in state['completed_regions']:
            layers=list(r.guide_layers);assert len(layers)==16
            for i,layer in enumerate(layers):layer.set_editor_property('enabled',i in keep)
            r.set_editor_property('guide_layers',layers,notify_mode=unreal.PropertyAccessChangeNotifyMode.NEVER);r.set_editor_property('guide_material',core,notify_mode=unreal.PropertyAccessChangeNotifyMode.NEVER)
            assert r.generate_guide_layers(),str(r.guide_build_status);assert len(r.generated_guide_layers)==8
            for g in r.generated_guide_layers:g.set_editor_property('tags',list(g.tags)+['BM_Campaign_MainGame%02d'%(index+1)])
            paths=json.loads((ROOT/'Art/MainGame/RoutesV2'/stage/'centerlines.json').read_text());records=json.loads((ROOT/'Art/MainGame/RoutesV2'/stage/'placement_plan.json').read_text())['objects']
            origin=r.get_actor_location()+r.cutters[0].local_transform.translation;vectors={};n=0
            for i in keep:
                record=records[i];p=paths[record['path_index']];a,b=record['start_arc_m'],record['end_arc_m'];mid=(a+b)/2
                for sa,sb in [(a,mid),(mid,b)]:
                    pa,ra=point(p,sa);pb,rb=point(p,sb);pa+=origin;pb+=origin
                    vectors['MudStart'+str(n)]=unreal.LinearColor(pa.x,pa.y,pa.z,ra);vectors['MudEnd'+str(n)]=unreal.LinearColor(pb.x,pb.y,pb.z,rb);n+=1
            params(mi,{'MudCount':n},vectors)
            state['completed_regions'].append(stage);assert levels.save_current_level();log(phase='saved_'+stage)
    # Every persistent mine surface, including connectors, bedrock and the
    # entrance seam, uses the same matte stone/soil family.
    for r in regions:
        area=next((str(t) for t in r.tags if str(t).startswith('BM_Campaign_MainGame')),None);i=int(area[-2:]) if area else 1
        mi=unreal.load_asset(FOLDER+'/MI_DirtWall_%02d'%i);r.set_editor_property('rock_material',mi,notify_mode=unreal.PropertyAccessChangeNotifyMode.NEVER);r.set_editor_property('shell_material',mi,notify_mode=unreal.PropertyAccessChangeNotifyMode.NEVER)
        assign(r.generated_rock,mi)
        for c in r.generated_shell.get_components_by_class(unreal.DynamicMeshComponent):c.set_material(0,mi)
        for g in r.generated_guide_layers:assign(g,core)
        prior=state['geometry_before'][r.get_path_name()];assert r.generated_rock.get_path_name()==prior['rock'] and r.generated_rock.triangle_count==prior['triangles']
    assert levels.save_current_level()
    for p,digest in state['protected_hashes'].items():assert hashlib.sha256((ROOT/p).read_bytes()).hexdigest()==digest,p
    log(passed=True,saved=True,phase='complete',blockers_before=64,blockers_after=32,ordinary_geometry_preserved=True,error=None)
except Exception:log(passed=False,error=traceback.format_exc());raise
