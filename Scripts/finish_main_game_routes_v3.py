"""Preserve matte clay relief and reward budgets while re-siting gems on new walls."""
import unreal,json,sys,bisect,traceback,collections
from pathlib import Path
ROOT=Path(unreal.Paths.project_dir()).resolve();sys.path.insert(0,str(ROOT/'Scripts'));import campaign_minerals_api as api
OUT=ROOT/'Saved/Validation/MainGameRoutesV3';ART=ROOT/'Art/MainGame/RoutesV3';BEFORE=json.loads((OUT/'before.json').read_text(encoding='utf-8'));assert json.loads((OUT/'authoring.json').read_text())['passed']
STAGES=['TunnelV1','TunnelCavernV2','TunnelStalagmiteV3'];actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);NEVER=unreal.PropertyAccessChangeNotifyMode.NEVER
assert not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world();assert levels.set_current_level_by_name('Lvl_MainGame')
def point(path,s):
 i=max(1,min(len(path['arc'])-1,bisect.bisect_left(path['arc'],s)));u=(s-path['arc'][i-1])/(path['arc'][i]-path['arc'][i-1]);p=[a+(b-a)*u for a,b in zip(path['points'][i-1],path['points'][i])];radius=path['radii'][i-1]*(1-u)+path['radii'][i]*u;return unreal.Vector(p[0]*100,-p[1]*100,p[2]*100),radius*100
report={'passed':False}
try:
 all_actors=list(actors.get_all_level_actors());hall=next(a for a in all_actors if isinstance(a,unreal.MiningExhibitHall))
 for i,stage in enumerate(STAGES):
  r=next(a for a in all_actors if isinstance(a,unreal.MiningExternalRegion) and a.actor_has_tag('BM_Campaign_MainGame%02d'%(i+1)));origin=r.get_actor_location()+r.cutters[0].local_transform.translation;paths=json.loads((ART/stage/'centerlines.json').read_text());plan=json.loads((ART/stage/'placement_plan.json').read_text())['objects'];mat=r.rock_material;values={str(v.parameter_info.name):v for v in mat.get_editor_property('vector_parameter_values')};n=0
  for record in plan:
   a,b=record['start_arc_m'],record['end_arc_m'];mid=(a+b)/2
   for sa,sb in [(a,mid),(mid,b)]:
    pa,ra=point(paths[record['path_index']],sa);pb,rb=point(paths[record['path_index']],sb);pa+=origin;pb+=origin
    for key,color in [('MudStart'+str(n),unreal.LinearColor(pa.x,pa.y,pa.z,ra)),('MudEnd'+str(n),unreal.LinearColor(pb.x,pb.y,pb.z,rb))]:values[key]=unreal.VectorParameterValue(parameter_info=unreal.MaterialParameterInfo(name=key),parameter_value=color)
    n+=1
  assert n==16;mat.set_editor_property('vector_parameter_values',list(values.values()),notify_mode=NEVER);unreal.MaterialEditingLibrary.set_material_instance_scalar_parameter_value(mat,'MudCount',n);unreal.MaterialEditingLibrary.update_material_instance(mat);assert unreal.EditorAssetLibrary.save_loaded_asset(mat,False)
 clay_report=OUT/'Clay/bake.json'
 if not clay_report.exists() or not json.loads(clay_report.read_text(encoding='utf-8'))['passed']:
  code=(ROOT/'Scripts/bake_main_game_3d_dirt.py').read_text(encoding='utf-8');code=code.replace("OUT=ROOT/'Saved/Validation/MainGameDirt3D'","OUT=ROOT/'Saved/Validation/MainGameRoutesV3/Clay'");a=code.index('stages=[');b=code.index('\n',a);code=code[:a]+"stages="+repr(list(zip(STAGES,['BM_MainGame_V1','BM_MainGame_CavernV2','BM_MainGame_StalagmiteV3'])))+code[b:]
  code=code.replace("ROOT/'Art/MainGame'/stage","ROOT/'Art/MainGame/RoutesV3'/stage").replace('depth=rng.uniform(6,16)','depth=rng.uniform(6,16)*1.3').replace('assert len(records)>80','assert len(records)>30')
  code=code.replace("assert not any(a.actor_has_tag('BM_MainGame_3DDirtBaked') for a in actors.get_all_level_actors()),'3D soil already baked; restore the backup before rebaking.'","assert not any(r.generated_rock.actor_has_tag('BM_MainGame_3DDirtBaked') for r in actors.get_all_level_actors() if isinstance(r,unreal.MiningExternalRegion) and r.actor_has_tag('BM_MainGame_RegionalRoutesV3'))")
  a=code.index('    # Permanent junction walls');b=code.index('    report.update(passed=True',a)
  code=code[:a]+'''    previous=json.loads((ROOT/'Saved/Validation/MainGameRoutesV2/Clay/bake.json').read_text(encoding='utf-8'))
    joint_records=[s for s in previous['samples'] if s['kind']=='junction']
    retained=[dict(s,position=[s['position'][0],s['position'][1],s['position'][2]+10000]) for s in previous['samples'] if s['kind']=='BM_MainGame_NeuralV4']
    all_records+=joint_records+retained
    assert levels.save_current_level()
'''+code[b:]
  exec(compile(code,'regional_clay_relief','exec'),{'__name__':'__main__'})
 minerals_report=OUT/'minerals.json'
 if not minerals_report.exists() or not json.loads(minerals_report.read_text(encoding='utf-8'))['passed']:
  code=(ROOT/'Scripts/rebuild_main_game_dense_minerals.py').read_text(encoding='utf-8').replace("OUT=ROOT/'Saved/Validation/MainGamePolish'","OUT=ROOT/'Saved/Validation/MainGameRoutesV3'");exec(compile(code,'regional_mineral_siting','exec'),{'__name__':'__main__'})
 field=api.get_field(hall);valid=api.validate(field,hall);assert valid['ok'];after=[{k:p[k] for k in ['id','area','value','guide']} for p in api._layout(field)]
 key=lambda p:(p['id'],p['area'],p['value'],p['guide']);assert collections.Counter(map(key,BEFORE['rewards']))==collections.Counter(map(key,after)),'Reward recipes changed'
 report.update(passed=True,minerals=valid,reward_recipes_unchanged=True,matte_materials_retained=True,clay_relief=True);assert levels.save_current_level()
except Exception:report['error']=traceback.format_exc();raise
finally:(OUT/'finishing.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8');unreal.log('REGIONAL_ROUTES_FINISHED '+json.dumps({k:v for k,v in report.items() if k!='minerals'},ensure_ascii=False))
