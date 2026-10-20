"""Move four selected deposits and rebuild only their supported crystal clues."""
import unreal,json,sys,math,shutil,datetime,traceback
from pathlib import Path
root=Path(unreal.Paths.project_dir());sys.path.insert(0,str(root/'Scripts'))
import campaign_minerals_api as api
L=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);A=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
E=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem);DL=unreal.MiningDesignerLibrary
out=root/'Saved/Validation/Feedback11';r={'passed':False}
try:
 assert L.load_level('/Game/MainGame/Maps/Lvl_MainGame')
 aa=A.get_all_level_actors();hall=next(a for a in aa if isinstance(a,unreal.MiningExhibitHall));field=api.get_field(hall)
 baseline=json.loads((out/'minerals-inspection.json').read_text(encoding='utf8'))
 original=baseline['layout'];placements=list(field.get_editor_property('authored_placements'));tag='BM_Campaign_MainGame01'
 area=next(a for a in hall.areas if str(a.tag)==tag);route=list(area.map_route)
 first_iron=min((i for i,p in enumerate(original) if p['area']==tag and p['id']=='4'),key=lambda i:original[i]['progress'])
 early_copper=sorted((i for i,p in enumerate(original) if p['area']==tag and p['id']=='2'),key=lambda i:original[i]['progress'])
 # The first copper is the initial introduction alongside the iron. The next
 # two are the pair beyond the second dirt barrier.
 copper=early_copper[1:3]
 def transform(t):
  result=unreal.Transform(location=unreal.Vector(*t[0]),scale=unreal.Vector(*t[2]))
  result.rotation=unreal.Quat(*t[1]);return result
 initial=early_copper[0];p=placements[initial];b=original[initial]
 p.set_editor_property('transform',transform(b['core']));p.set_editor_property('surface',transform(b['surface']))
 p.set_editor_property('fragments',[transform(t) for t in b['fragments']]);p.set_editor_property('fragment_supports',[unreal.Vector(*v) for v in b['supports']])
 p.set_editor_property('hosts',[next(a for a in aa if a.get_actor_label()==name) for name in baseline['hosts'][initial]])
 placements[initial]=p
 collection=next(a for a in aa if isinstance(a,unreal.MiningCollectionField))
 ant=next(p for p in collection.get_editor_property('authored_placements') if p.item_id==1)
 nearby=[i for i,p in enumerate(original) if p['area']==tag and (unreal.Vector(*p['surface'][0])-ant.clue_transform.translation).length()<650]
 assert nearby
 def hit(p,q):
  result=unreal.SystemLibrary.line_trace_single(E.get_editor_world(),p,q,unreal.TraceTypeQuery.ECC_VISIBILITY,True,[],unreal.DrawDebugTrace.NONE,True)
  assert result,'No surface hit'
  h=result.to_tuple();assert h[0] and isinstance(h[9],unreal.DiggableRock),'Surface must be excavatable'
  return h
 def placement_at(old,h):
  p=unreal.MiningTreasurePlacement()
  for key in ['treasure_id','value','area_tag','route_progress','guide_layer']:p.set_editor_property(key,old.get_editor_property(key))
  p.set_editor_property('surface',unreal.Transform(location=h[5],rotation=unreal.MathLibrary.conv_vector_to_rotator(h[7])))
  p.set_editor_property('transform',unreal.Transform(location=h[5]-h[7]*120))
  p.set_editor_property('hosts',[h[9]])
  return p
 changed={}
 sites=[p for p in DL.find_campaign_mineral_sites(hall) if str(p.area_tag)==tag and .11<p.route_progress<.16 and abs(p.surface.rotation.rotator().pitch)<25]
 candidates=[p for p in sites if all((p.surface.translation-q.surface.translation).length()>380 for i,q in enumerate(placements) if i!=first_iron)]
 assert candidates,'No separated later iron site'
 p=min(candidates,key=lambda p:abs(p.route_progress-.135))
 p.set_editor_property('treasure_id',placements[first_iron].treasure_id);p.set_editor_property('value',placements[first_iron].value)
 changed[first_iron]=p
 # Find real floor beside the left wall at the old deposit's route location.
 for index in copper:
  old=placements[index]
  original_surface=unreal.Vector(*original[index]['surface'][0])
  j=min(range(len(route)),key=lambda j:abs(route[j].x-original_surface.x))
  center=route[j];forward=(route[min(j+1,len(route)-1)]-route[max(0,j-1)]);forward.z=0;forward=forward.normal()
  left=unreal.Vector(forward.y,-forward.x,0)
  options=[]
  for along in [-120,-60,0,60,120]:
   for side in [0,55,110,165,220]:
    probe=center+forward*along+left*side+unreal.Vector(0,0,250)
    result=unreal.SystemLibrary.line_trace_single(E.get_editor_world(),probe,probe-unreal.Vector(0,0,1400),unreal.TraceTypeQuery.ECC_VISIBILITY,True,[],unreal.DrawDebugTrace.NONE,True)
    if not result:continue
    h=result.to_tuple()
    if h[0] and h[3]>10 and h[7].z>.65 and isinstance(h[9],unreal.DiggableRock) and h[5].z<original_surface.z-70:options.append(h)
  assert options,'No reachable lower floor for copper '+str(index)
  h=min(options,key=lambda h:(h[5]-(center+left*110)).length())
  changed[index]=placement_at(old,h)
 # Relative to the player's view towards the ant computer, mine the left wall.
 approach=ant.approach_transform.translation+unreal.Vector(0,0,100)
 forward=ant.transform.translation-approach;forward.z=0;forward=forward.normal()
 left=unreal.Vector(forward.y,-forward.x,0)
 for n,index in enumerate(nearby):
  options=[]
  for back in [150,300,450,600,750,900]:
   for height in [-40,0,60]:
    origin=approach-forward*(back+n*220)+unreal.Vector(0,0,height)
    result=unreal.SystemLibrary.line_trace_single(E.get_editor_world(),origin,origin+left*1600,unreal.TraceTypeQuery.ECC_VISIBILITY,True,[],unreal.DrawDebugTrace.NONE,True)
    if not result:continue
    h=result.to_tuple();delta=h[5]-ant.clue_transform.translation
    if h[0] and h[3]>25 and isinstance(h[9],unreal.DiggableRock) and 500<delta.length()<1800 and unreal.MathLibrary.dot_vector_vector(delta,left)>300:options.append(h)
  assert options,'No separated ant left-wall surface'
  target=ant.clue_transform.translation+left*800
  h=min(options,key=lambda h:(h[5]-target).length())
  r['ant_probe']=dict(left=api._xyz(left),hit=api._xyz(h[5]),normal=api._xyz(h[7]),distance=(h[5]-ant.clue_transform.translation).length())
  changed[index]=placement_at(placements[index],h)
 stage=api._stage(hall,A,field);indices=list(changed)
 stage.set_editor_property('authored_placements',[changed[i] for i in indices])
 assert DL.prepare_campaign_minerals(stage),'Mineral burial/clue preparation failed'
 validation=api.validate(stage,hall);assert validation['ok'],validation['errors']
 updated=list(stage.get_editor_property('authored_placements'))
 r['moves']=[]
 for index,p in zip(indices,updated):
  placements[index]=p
  r['moves'].append(dict(index=index,id=str(p.treasure_id),before=original[index],after=dict(core=api._xyz(p.transform.translation),surface=api._xyz(p.surface.translation),progress=p.route_progress)))
 field.set_editor_property('authored_placements',placements);A.destroy_actor(stage)
 final=api._layout(field)
 assert len(final)==len(original)
 def equivalent(a,b):
  if isinstance(a,dict):return a.keys()==b.keys() and all(equivalent(a[k],b[k]) for k in a)
  if isinstance(a,list):return len(a)==len(b) and all(equivalent(x,y) for x,y in zip(a,b))
  if isinstance(a,(float,int)) and isinstance(b,(float,int)):return abs(a-b)<.001
  return a==b
 assert all(equivalent(final[i],original[i]) for i in range(len(final)) if i not in changed),'Unrelated deposits changed: '+str([i for i in range(len(final)) if i not in changed and not equivalent(final[i],original[i])])
 assert all((final[i]['id'],final[i]['value'])==(original[i]['id'],original[i]['value']) for i in range(len(final)))
 r['validation']=validation;r['unchanged_deposits']=len(final)-len(changed)
 backup=root/'Saved/Backups'/('Feedback11-Minerals-'+datetime.datetime.now().strftime('%Y%m%d-%H%M%S'));backup.mkdir(parents=True)
 shutil.copy2(root/'Content/MainGame/Maps/Lvl_MainGame.umap',backup/'Lvl_MainGame.umap')
 assert L.save_current_level();r['passed']=True
except Exception:r['error']=traceback.format_exc();unreal.log_error(r['error'])
(out/'minerals.json').write_text(json.dumps(r,ensure_ascii=False,indent=2),encoding='utf8')
unreal.SystemLibrary.quit_editor()
