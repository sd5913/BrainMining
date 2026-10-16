"""Update existing V5 wall materials and clod meshes without replacing mineral hosts."""
import unreal,json,sys,bisect,traceback,hashlib
from pathlib import Path
ROOT=Path(unreal.Paths.project_dir()).resolve();sys.path.insert(0,str(ROOT/'Scripts'))
import campaign_minerals_api as api
ART=ROOT/'Art/MainGame/RoutesV5';OUT=ROOT/'Saved/Validation/MainGameRoutesV5/OrganicWalls';OUT.mkdir(parents=True,exist_ok=True)
lib=unreal.MaterialEditingLibrary;levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
NEVER=unreal.PropertyAccessChangeNotifyMode.NEVER
STAGES=['TunnelV1','TunnelCavernV2','TunnelStalagmiteV3'];ORIGINS=[(-238,0,59),(-77,0,4),(397.5,-117.5,139)]
NAMES=['M_Earth','M_BlueViolet','M_DarkRed'];COLORS=[(.23,.14,.085),(.115,.09,.29),(.18,.024,.028)]
report={'passed':False,'materials':[],'clod_groups':[]}

def point(path,s,origin):
    i=max(1,min(len(path['arc'])-1,bisect.bisect_left(path['arc'],s)))
    f=max(0,min(1,(s-path['arc'][i-1])/(path['arc'][i]-path['arc'][i-1])))
    p=[(a+(b-a)*f+o)*100 for a,b,o in zip(path['points'][i-1],path['points'][i],origin)]
    p[1]*=-1
    return p+[100*(path['radii'][i-1]+(path['radii'][i]-path['radii'][i-1])*f)]

def vec(v):return 'float4('+','.join('%.6f'%x for x in v)+')'

def shader(stage,index):
    # Reuse exactly the existing soil/moss color function at the transition.
    code=(ROOT/'Art/MainGame/Materials/DryRockMoss.hlsl').read_text()
    start=code.index('float coarse=');end=code.index('float proximity=')
    code=code[:start]+'''float coarse=dirt.organic(dirt.turn(P*.009));
float grain=dirt.noise(dirt.turn(P*.22)+float3(3,8,1));
float middle=dirt.organic(dirt.turn(P*.047)+float3(9,2,6));
// Preserve the regional pigment; only soft natural grain changes its value.
float3 stone=Tint.rgb*(.69+coarse*.40+middle*.14+grain*.09);
float stoneHeight=.85*middle+.18*grain;
'''+code[end:]
    start=code.index('float2 chip=');end=code.index('// Soil coloration')
    code=code[:start]+'''// Broken, soft deposits instead of concentric rings or cellular motifs.
float patch=dirt.organic(dirt.turn(P*.022)+float3(7,13,3));
float mask=smoothstep(.28,.78,proximity+(patch-.5)*.85)*.84;
'''+code[end:]
    paths=json.loads((ART/stage/'centerlines.json').read_text());specs=json.loads((ART/stage/'placement_plan.json').read_text())['objects']
    volumes=[]
    for j,spec in enumerate(specs):
        if (index==1 and j in [2,6]) or (index==2 and j in [1,3,5]):continue
        path=paths[spec['path_index']]
        a=point(path,spec['start_arc_m'],ORIGINS[index]);b=point(path,spec['end_arc_m'],ORIGINS[index])
        volumes.append('proximity=max(proximity,dirt.influence(P,%s,%s,390));'%(vec(a),vec(b)))
    # Include room entrance/exit plugs as well as the internal guide layers.
    cfg=json.loads((ROOT/'Config/MainGameRooms.json').read_text())
    for connection in cfg['generated_connections']:
        if connection['mine']!=index or (index==2 and connection['side']=='exit') or (index==1 and connection['side']=='entry'):continue
        x=connection['plug_x'];samples=connection['floor_samples']
        k=max(1,min(len(samples)-1,bisect.bisect_left([p[0] for p in samples],x)))
        f=(x-samples[k-1][0])/max(.01,samples[k][0]-samples[k-1][0])
        z=samples[k-1][1]+f*(samples[k][1]-samples[k-1][1])+connection['height']*.5
        a=[x-70,connection['a'][1],z,connection['height']*.7];b=[x+70,*a[1:]]
        volumes.append('proximity=max(proximity,dirt.influence(P,%s,%s,390));'%(vec(a),vec(b)))
    code=code.replace('// __VOLUME_EVALUATION__','\n'.join(volumes))
    return code

try:
    assert not ed.get_game_world()
    manifest=json.loads((ART/'ClayRelief/manifest.json').read_text())
    assert json.loads((ART/'ClayRelief/geometry_validation.json').read_text())['passed']
    clay=unreal.load_asset('/Game/MainGame/Materials/DirtTransition/MI_ClayBlockage')
    clay_file=ROOT/'Content/MainGame/Materials/DirtTransition/MI_ClayBlockage.uasset'
    soil_hash=hashlib.sha256(clay_file.read_bytes()).hexdigest()
    for i,stage in enumerate(STAGES):
        mat=unreal.load_asset('/Game/MainGame/Materials/RoutesV5/'+NAMES[i])
        lib.delete_all_material_expressions(mat)
        mat.set_editor_property('tangent_space_normal',False)
        custom=lib.create_material_expression(mat,unreal.MaterialExpressionCustom)
        custom.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT4)
        params={k:lib.get_material_instance_scalar_parameter_value(clay,k) for k in ['MossScale','MossAmount','MudDepth']}
        params.update(MudCore=0.)
        params.update({k:lib.get_material_instance_vector_parameter_value(clay,k) for k in ['MudDark','MudLight','MossDark','MossLight']})
        params['Tint']=unreal.LinearColor(*COLORS[i],1)
        inputs=[]
        for name in ['P',*params]:
            entry=unreal.CustomInput();entry.set_editor_property('input_name',name);inputs.append(entry)
        custom.set_editor_property('inputs',inputs,notify_mode=NEVER)
        outputs=[]
        for name,typ in [('Height',unreal.CustomMaterialOutputType.CMOT_FLOAT1),('Emission',unreal.CustomMaterialOutputType.CMOT_FLOAT3),('SurfaceSpecular',unreal.CustomMaterialOutputType.CMOT_FLOAT1)]:
            output=unreal.CustomOutput();output.set_editor_property('output_name',name);output.set_editor_property('output_type',typ);outputs.append(output)
        custom.set_editor_property('additional_outputs',outputs)
        world=lib.create_material_expression(mat,unreal.MaterialExpressionWorldPosition)
        assert lib.connect_material_expressions(world,'',custom,'P')
        for name,value in params.items():
            vector=isinstance(value,unreal.LinearColor)
            node=lib.create_material_expression(mat,unreal.MaterialExpressionVectorParameter if vector else unreal.MaterialExpressionScalarParameter)
            node.set_editor_property('parameter_name',name,notify_mode=NEVER);node.set_editor_property('default_value',value,notify_mode=NEVER)
            assert lib.connect_material_expressions(node,'RGBA' if vector else '',custom,name)
        custom.set_editor_property('code',shader(stage,i),notify_mode=NEVER)
        assert lib.connect_material_property(custom,'',unreal.MaterialProperty.MP_BASE_COLOR)
        for prop,value in [(unreal.MaterialProperty.MP_ROUGHNESS,1.),(unreal.MaterialProperty.MP_SPECULAR,0.),(unreal.MaterialProperty.MP_METALLIC,0.)]:
            node=lib.create_material_expression(mat,unreal.MaterialExpressionConstant);node.set_editor_property('r',value);assert lib.connect_material_property(node,'',prop)
        # Small derivative bump, in world space, keeps faceted mesh silhouettes.
        normal=lib.create_material_expression(mat,unreal.MaterialExpressionCustom)
        normal.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT3)
        ni=[]
        for name in ['P','N','H']:
            entry=unreal.CustomInput();entry.set_editor_property('input_name',name);ni.append(entry)
        normal.set_editor_property('inputs',ni)
        normal.set_editor_property('code','float3 dx=ddx(P),dy=ddy(P); float3 a=cross(dy,N),b=cross(N,dx); float det=dot(dx,a); return normalize(N-sign(det)*(ddx(H)*a+ddy(H)*b)/max(abs(det),.00001));')
        vertex_normal=lib.create_material_expression(mat,unreal.MaterialExpressionVertexNormalWS)
        assert lib.connect_material_expressions(world,'',normal,'P')
        assert lib.connect_material_expressions(vertex_normal,'',normal,'N')
        assert lib.connect_material_expressions(custom,'Height',normal,'H')
        assert lib.connect_material_property(normal,'',unreal.MaterialProperty.MP_NORMAL)
        lib.layout_material_expressions(mat)
        errors=lib.recompile_material(mat);assert not errors,list(errors)
        assert unreal.EditorAssetLibrary.save_loaded_asset(mat,False)
        report['materials'].append(dict(name=NAMES[i],base_color=COLORS[i],roughness=1,specular=0))
    assert levels.load_level('/Game/MainGame/Maps/Lvl_MainGame')
    world,hall,actors=api._context();all_actors=actors.get_all_level_actors()
    for i,item in enumerate(manifest['regions']):
        for group in item['clod_meshes']:
            actor=next(a for a in all_actors if a.get_actor_label()==f'MAIN_GAME_ClayWallClods_{i+1}_{group["guide"]}')
            assert unreal.MiningDesignerLibrary.import_campaign_rock(actor,str(ART/'ClayRelief'/group['file']))
            assert actor.rock_material==clay
            report['clod_groups'].append(dict(actor=actor.get_actor_label(),clods=group['clods'],triangles=actor.triangle_count))
    valid=api.validate(api.get_field(hall),hall);assert valid['ok'],valid
    assert hashlib.sha256(clay_file.read_bytes()).hexdigest()==soil_hash
    assert levels.save_current_level()
    # Reopen the saved level and trace/mine/reset real protrusions, including
    # mineral host validation. Strip only its standalone process-exit command.
    verification=(ROOT/'Scripts/verify_routes_v5_clay_relief.py').read_text()
    verification=verification.replace("unreal.SystemLibrary.execute_console_command(ed.get_editor_world(),'QUIT_EDITOR')",'pass')
    exec(compile(verification,'saved_clay_verification','exec'),{'__name__':'__main__'})
    verified=json.loads((ROOT/'Saved/Validation/MainGameRoutesV5/ClayRelief/saved.json').read_text())
    assert verified['passed'],verified.get('error')
    report.update(passed=True,minerals=valid,density_bands=manifest['density_bands'],clay_material_unchanged=True)
except Exception:
    report['error']=traceback.format_exc();unreal.log_error(report['error'])
finally:
    (OUT/'authoring.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    unreal.SystemLibrary.execute_console_command(ed.get_editor_world(),'QUIT_EDITOR')
