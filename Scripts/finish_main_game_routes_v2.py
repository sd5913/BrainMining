"""Update dirt influence volumes and preserve thick mineable clay on new routes."""
import unreal,json,bisect,traceback
from pathlib import Path
ROOT=Path(unreal.Paths.project_dir()).resolve();ART=ROOT/'Art/MainGame/RoutesV2';OUT=ROOT/'Saved/Validation/MainGameRoutesV2'
assert json.loads((OUT/'authoring.json').read_text())['passed']
STAGES=['TunnelV1','TunnelCavernV2','TunnelStalagmiteV3','TunnelNeuralV4']
TAGS=['BM_MainGame_V1','BM_MainGame_CavernV2','BM_MainGame_StalagmiteV3','BM_MainGame_NeuralV4']
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);lib=unreal.MaterialEditingLibrary
def point(path,s):
    i=max(1,min(len(path['arc'])-1,bisect.bisect_left(path['arc'],s)));u=(s-path['arc'][i-1])/(path['arc'][i]-path['arc'][i-1])
    p=[a+(b-a)*u for a,b in zip(path['points'][i-1],path['points'][i])];r=path['radii'][i-1]+(path['radii'][i]-path['radii'][i-1])*u
    return unreal.Vector(p[0]*100,-p[1]*100,p[2]*100),r*100
for stage,tag in zip(STAGES,TAGS):
    r=next(a for a in actors.get_all_level_actors() if isinstance(a,unreal.MiningExternalRegion) and a.actor_has_tag(tag));origin=r.get_actor_location()+r.cutters[0].local_transform.translation
    paths=json.loads((ART/stage/'centerlines.json').read_text());plan=json.loads((ART/stage/'placement_plan.json').read_text())['objects']
    mat=r.rock_material;values={str(v.parameter_info.name):v for v in mat.get_editor_property('vector_parameter_values')};volumes=[]
    for record in plan:
        path=paths[record['path_index']];a=record['start_arc_m'];b=record['end_arc_m'];mid=(a+b)/2
        for sa,sb in [(a,mid),(mid,b)]:
            pa,ra=point(path,sa);pb,rb=point(path,sb);pa+=origin;pb+=origin
            volumes.append((unreal.LinearColor(pa.x,pa.y,pa.z,ra),unreal.LinearColor(pb.x,pb.y,pb.z,rb)))
    assert len(volumes)==32
    for i,(a,b) in enumerate(volumes):
        for name,value in [('MudStart'+str(i),a),('MudEnd'+str(i),b)]:values[name]=unreal.VectorParameterValue(parameter_info=unreal.MaterialParameterInfo(name=name),parameter_value=value)
    mat.set_editor_property('vector_parameter_values',list(values.values()),notify_mode=unreal.PropertyAccessChangeNotifyMode.NEVER);lib.update_material_instance(mat)
    assert unreal.EditorAssetLibrary.save_loaded_asset(mat,False)
    # Existing cave fill lights follow the new spiral instead of remaining
    # buried in the old route. Dedicated new lights illuminate the wider halls.
    index=STAGES.index(stage)+1
    for n,fraction in enumerate([.43,.57],1):
        label=f'MainGame_V{index}_Cavern_{n}_Light'
        light=next((a for a in actors.get_all_level_actors() if a.get_actor_label()==label),None)
        if light:
            p,_=point(paths[0],paths[0]['arc'][-1]*fraction)
            light.set_actor_location(origin+p+unreal.Vector(0,0,130),False,True)
# Reuse the established closed-boolean bake, keeping the existing entrance clods.
code=(ROOT/'Scripts/bake_main_game_3d_dirt.py').read_text()
code=code.replace("OUT=ROOT/'Saved/Validation/MainGameDirt3D'","OUT=ROOT/'Saved/Validation/MainGameRoutesV2/Clay'")
code=code.replace("ROOT/'Art/MainGame'/stage","ROOT/'Art/MainGame/RoutesV2'/stage")
code=code.replace("assert not any(a.actor_has_tag('BM_MainGame_3DDirtBaked') for a in actors.get_all_level_actors()),'3D soil already baked; restore the backup before rebaking.'","assert not any(r.generated_rock.actor_has_tag('BM_MainGame_3DDirtBaked') for r in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.MiningExternalRegion) if r.actor_has_tag('BM_MainGame_RoutesV2')),'New route soil already baked.'")
code=code.replace('depth=rng.uniform(6,16)','depth=rng.uniform(6,16)*1.3')
start=code.index('    # Permanent junction walls');end=code.index('    report.update(passed=True',start)
code=code[:start]+'''    joint_records=[r for r in json.loads((ROOT/'Saved/Validation/MainGameDirt3D/bake.json').read_text())['samples'] if r['kind']=='junction']
    all_records+=joint_records
    assert levels.save_current_level()
'''+code[end:]
exec(compile(code,'routes_v2_clay_bake','exec'),{'__name__':'__main__'})
for r in unreal.GameplayStatics.get_all_actors_of_class(unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world(),unreal.MiningExternalRegion):
    if r.actor_has_tag('BM_MainGame_RoutesV2'):
        r.generated_rock.set_editor_property('tags',list(r.generated_rock.tags)+['BM_MainGame_ClayRelief130'])
assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
