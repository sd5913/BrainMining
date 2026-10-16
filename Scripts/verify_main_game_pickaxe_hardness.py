"""Check authored hardness, purchases, multipliers and reset in a MainGame PIE copy."""
import unreal,json,time,traceback,sys,hashlib
from pathlib import Path
ROOT=Path(unreal.Paths.project_dir()).resolve();OUT=ROOT/'Saved/Validation/PickaxeHardness';OUT.mkdir(exist_ok=True);sys.path.insert(0,str(ROOT/'Scripts'));import campaign_minerals_api as minerals
ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem);levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);helper=unreal.MiningGUIEditorLibrary;cfg=json.loads((ROOT/'Config/MainGameRooms.json').read_text());author=json.loads((OUT/'authoring.json').read_text());report={'passed':False,'checks':[],'levels':[]};state={'phase':'setup','start':time.monotonic(),'next':0,'busy':False,'level':0}
perf=unreal.get_default_object(unreal.load_class(None,'/Script/UnrealEd.EditorPerformanceSettings'));old=perf.get_editor_property('bThrottleCPUWhenNotForeground');perf.set_editor_property('bThrottleCPUWhenNotForeground',False)
def check(ok,msg):assert ok,msg;report['checks'].append(msg)
def finish(error=None):
 report.update(passed=error is None,error=error,elapsed=time.monotonic()-state['start']);(OUT/'runtime.json').write_text(json.dumps(report,ensure_ascii=False,indent=2));perf.set_editor_property('bThrottleCPUWhenNotForeground',old);unreal.unregister_slate_post_tick_callback(callback);levels.editor_request_end_play()
def tick(dt):
 if state['busy'] or time.monotonic()<state['next']:return
 state['busy']=True
 try:
  now=time.monotonic();assert now-state['start']<100,'Hardness PIE timed out';world=ed.get_game_world()
  if not world:return
  if state['phase']=='setup':
   halls=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.MiningExhibitHall);pc=unreal.GameplayStatics.get_player_controller(world,0)
   if not halls or not halls[0].get_editor_property('bReady') or not pc or not pc.mining_gui:return
   hall=halls[0];pawn=unreal.GameplayStatics.get_player_pawn(world,0);mine=pawn.get_component_by_class(unreal.MiningComponent);progress=pawn.get_component_by_class(unreal.MiningProgressionComponent);wallet=progress.get_wallet();wallet.add_coins(1000000);state.update(world=world,hall=hall,pc=pc,pawn=pawn,mine=mine,progress=progress,wallet=wallet,phase='level');hall.set_actor_tick_enabled(False)
   rocks=list(unreal.GameplayStatics.get_all_actors_of_class(world,unreal.DiggableRock));regions=list(unreal.GameplayStatics.get_all_actors_of_class(world,unreal.MiningExternalRegion))
   for i in range(4):
    r=next(r for r in regions if r.actor_has_tag('BM_Campaign_MainGame%02d'%(i+1)));check(r.rock_hardness==i and r.guide_hardness==i+1,'Region '+str(i+1)+' configuration matches design');check(r.generated_rock.hardness_level==i and all(g.hardness_level==i+1 for g in r.generated_guide_layers),'Region '+str(i+1)+' generated rocks inherit configured hardness')
   for item in author['rocks']:
    a=next(a for a in rocks if a.get_actor_label()==item['label']);check(a.hardness_level==item['hardness'],'Saved/runtime hardness: '+item['label'])
   layout=minerals._layout(hall.treasure);digest=hashlib.sha256(json.dumps(layout,sort_keys=True,allow_nan=False).encode()).hexdigest();check(len(layout)==165 and digest==author['mineral_layout_sha256'],'All mineral placement and fragment hosts survive hardness metadata edits')
   component_settings=unreal.get_default_object(unreal.load_class(None,'/Script/BrainMining.MiningPickaxeSettings'));state['base']=component_settings.get_editor_property('Levels')[0];check(component_settings.get_editor_property('RangeUpgradeMultiplier')==1.25 and component_settings.get_editor_property('IntervalUpgradeMultiplier')==.75,'Central upgrade multipliers match design')
   state['soil']=next(a for a in rocks if a.get_actor_label()=='LINK_Museum_Mine04_DiggableSoil');return
  if state['phase']=='level':
   L=state['level'];m=state['mine'];p=m.pickaxe_balance;b=state['base'];check(p.destruction_level==L and p.use_hardness,'Purchased level maps to destruction level '+str(L));check(abs(p.width-b.width*(1.25 if L>=2 else 1))<.01 and abs(p.interval-b.interval*(.75 if L>=3 else 1))<.001,'Range and timing cumulative effects at level '+str(L));check(p.depth==b.depth and len(p.stages)==1,'Shared per-hit depth at level '+str(L))
   for H,expected in [(0,[1,1.5,2,2.5,3][L]),(4,[0,.25,.5,.75,1][L])]:
    rock=state['soil'];rock.set_editor_property('hardness_level',H);check(abs(m.get_pickaxe_depth_multiplier(rock)-expected)<.001,'Runtime hardness multiplier power '+str(L)+' hardness '+str(H))
   state['soil'].set_editor_property('hardness_level',4);report['levels'].append({'level':L,'width':p.width,'depth':p.depth,'interval':p.interval})
   if L<4:
    before=state['wallet'].balance;price=state['progress'].get_next_price(0);check(state['progress'].purchase(0,L+1),'Purchase next pickaxe level '+str(L+1));check(state['wallet'].balance==before-price,'Purchase charges exact price once');state['level']+=1;state['next']=now+.1
   else:state.update(phase='reset',next=now+.1)
  elif state['phase']=='reset':
   before=state['wallet'].balance;state['mine'].reset_field();state['progress'].apply_upgrades();check(state['progress'].get_level(0)==4 and state['mine'].pickaxe_balance.destruction_level==4,'Reset preserves destruction level');check(state['wallet'].balance==before,'Reset preserves coins');check(state['soil'].hardness_level==4,'Reset preserves rock hardness');check(state['wallet'].mineral_double_chance==0,'Pickaxe level four adds no obsolete coin doubling');check(abs(state['mine'].radius-160)<.01 and abs(state['mine'].interval-.3)<.001,'Pickaxe growth leaves drill base radius and timing unchanged');finish()
 except Exception:finish(traceback.format_exc())
 finally:state['busy']=False
assert not ed.get_game_world();assert levels.load_level('/Game/MainGame/Maps/Lvl_MainGame');unreal.EditorPythonScripting.set_keep_python_script_alive(True);callback=unreal.register_slate_post_tick_callback(tick);helper.start_floating_preview()
