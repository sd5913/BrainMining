"""Saved route profiles, interfaces, source transforms, hardness and mineral hosts."""
import unreal,json,sys,hashlib,traceback
from pathlib import Path
ROOT=Path(unreal.Paths.project_dir()).resolve();sys.path.insert(0,str(ROOT/'Scripts'));import campaign_minerals_api as api
OUT=ROOT/'Saved/Validation/MainGameRoutesV3';before=json.loads((OUT/'before.json').read_text(encoding='utf-8'));cfg=json.loads((ROOT/'Config/MainGameRooms.json').read_text(encoding='utf-8'));sources=json.loads((ROOT/'Saved/Validation/MainGameRooms/before.json').read_text(encoding='utf-8'))['sources'];report={'passed':False,'checks':[],'regions':[]}
def V(p):return unreal.Vector(*p)
def check(ok,msg):assert ok,msg;report['checks'].append(msg)
try:
 ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem);assert not ed.get_game_world();assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level('/Game/MainGame/Maps/Lvl_MainGame');actors=list(unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors());hall=next(a for a in actors if isinstance(a,unreal.MiningExhibitHall));check(len(hall.areas)==7,'Seven mine/room sections preserved');check(hall.areas[0].entry.translation.distance(V(before['areas'][0]['entry']))<.01,'Bedroom and jetpack access remain at the original position')
 for i,stage in enumerate(['TunnelV1','TunnelCavernV2','TunnelStalagmiteV3','TunnelNeuralV4']):
  r=next(a for a in actors if isinstance(a,unreal.MiningExternalRegion) and a.actor_has_tag('BM_Campaign_MainGame%02d'%(i+1)));a=hall.areas[i*2];d=unreal.Vector(0,0,10000 if i>=2 else 0);expected_exit=V(before['areas'][i*2]['exit'])+unreal.Vector(0,0,10000 if i>=1 else 0)
  check(a.entry.translation.distance(V(before['areas'][i*2]['entry'])+d)<.01,'Saved entry '+str(i+1));check(a.exit.distance(expected_exit)<.01,'Saved outlet '+str(i+1));check(r.rock_hardness==i and r.generated_rock.hardness_level==i and r.guide_hardness==i+1 and all(g.hardness_level==i+1 for g in r.generated_guide_layers),'Hardness preserved in region '+str(i+1));check(len(r.generated_guide_layers)==8,'Eight blockage layers in region '+str(i+1))
  plan=json.loads((ROOT/'Art/MainGame/RoutesV3'/stage/'plan_summary.json').read_text());report['regions'].append(dict(stage=stage,style=plan['style'],triangles=r.generated_rock.triangle_count,entry=list(a.entry.translation.to_tuple()),exit=list(a.exit.to_tuple()),plan=plan))
  if i==0:check(plan['spiral_turns']==0 and plan['narrow_main_fraction']>.6,'Region one is narrow branching without a repeated spiral')
  if i==1:check(plan['main_drop_m']==-45 and plan['ascending_vertical_fraction']>.8,'Region two primarily ascends and ends 45 metres above its mouth')
  if i==2:check(plan['spiral_turns']>2 and plan['spiral_main_fraction']>.65,'Region three primarily follows a continuous descending spiral')
  if i==3:check(r.generated_rock.triangle_count==before['regions'][i]['triangles'],'Fourth-region mixed geometry and neural scenery retained')
 for i,spec in zip([1,3,5],cfg['rooms']):
  a=hall.areas[i];check(a.origin.distance(V(spec['origin']))<.01 and a.entry.translation.distance(V(spec['entry']))<.01 and a.exit.distance(V(spec['exit']))<.01,'Room entry/origin/outlet follows connection plan '+spec['id']);check(a.source_level.get_editor_property('level_transform').translation.distance(V(spec['origin']))<.01,'Room remains a transformed source-level reference '+spec['id'])
  source=a.source_level.get_loaded_level();loaded={x.get_name():x for x in actors if x.get_level()==source};count=0
  for item in sources[spec['source']]:
   if item['name'] not in loaded:continue
   actor=loaded[item['name']];check(actor.get_actor_location().distance(V(item['position'])+V(spec['origin']))<1,'Source actor translation '+spec['id']+'/'+item['name']);count+=1
  check(count>3,'Source actors loaded '+spec['id'])
 valid=api.validate(api.get_field(hall),hall);check(valid['ok'] and valid['deposits']==165,'All relocated minerals retain valid hosts, burial and clues');report['minerals']=valid
 for src,digest in before['source_hashes'].items():check(hashlib.sha256((ROOT/'Content'/(src[6:]+'.umap')).read_bytes()).hexdigest()==digest,'Original source map unchanged '+src)
 check(not any(a.get_level()==hall.get_level() and a.get_components_by_class(unreal.LightComponent) for a in actors),'No unexplained fixed corridor lights added');report['passed']=True
except Exception:report['error']=traceback.format_exc();raise
finally:(OUT/'saved_state.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8');unreal.log('REGIONAL_ROUTES_SAVED '+str(report['passed']))
