"""Apply the October 21 playtest text/material/layout changes and inspect mineral sites."""
import unreal,json,sys,shutil,traceback
from pathlib import Path
root=Path(unreal.Paths.project_dir());sys.path.insert(0,str(root/'Scripts'))
out=root/'Saved/Validation/Playtest17';out.mkdir(parents=True,exist_ok=True)
EA=unreal.EditorAssetLibrary;ML=unreal.MaterialEditingLibrary
L=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);A=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
E=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
r={'passed':False,'subtitles':[],'widgets':[]}
def protect(path):
 f=root/'Content'/(path.split('.')[0][6:]+'.uasset');b=out/'Before'/f.relative_to(root)
 if f.exists() and not b.exists():b.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(f,b)
def save(a):assert EA.save_loaded_asset(a,False)
try:
 for path in EA.list_assets('/Game/Subtitles',recursive=True,include_folder=False):
  a=unreal.load_asset(path)
  if not isinstance(a,unreal.SubtitleSegment):continue
  lines=list(a.lines);changed=False
  for line in lines:
   text=str(line.text);new=text.replace('按C打开界面吧','按C打开藏品界面吧').replace('不断的往上跳','不断地往上跳')
   if str(a.segment_id)=='首次收藏提示':
    r.setdefault('collection_hint',[]).append(text)
    if 'C' in text:new='按C打开藏品界面吧'
   if 'SKIP' in text.upper() or ('四个键' in text and '3秒' in text):new='比如长按P键持续3秒，就可以跳过这一关'
   if text!=new:
    protect(path);line.set_editor_property('text',new);changed=True
    r['subtitles'].append({'asset':path,'before':text,'after':new})
  if changed:a.set_editor_property('lines',lines);save(a)
 path='/Game/MainGame/Materials/M_PlayerFootprint';protect(path);m=unreal.load_asset(path)
 for n in ML.get_material_expressions(m):
  if isinstance(n,unreal.MaterialExpressionCustom):
   code=n.get_editor_property('code')
   if code=='return Shape*saturate(Life);':n.set_editor_property('code','return .38*Shape*saturate(Life);')
   if code=='return float3(.7,.75,.78)*Shape*saturate(Life);':n.set_editor_property('code','return float3(.32,.34,.35)*Shape*saturate(Life);')
 ML.recompile_material(m);save(m)
 for path in ['/Game/CollectionPrototype/UI/WBP_CollectionInspect','/Game/EndingEmergence/UI/WBP_EndingCredits']:
  protect(path);bp=unreal.load_asset(path)
  if 'EndingCredits' in path:unreal.get_default_object(bp.generated_class()).set_editor_property('prompt_delay',4.5)
  for w in unreal.ObjectIterator(unreal.Widget):
   if not w.get_outer() or w.get_outer().get_outer()!=bp:continue
   if w.get_name().startswith(('ItemLabel','ItemButton','ItemArt')):
    r['widgets'].append({'name':w.get_name(),'slot':str(w.slot),'offsets':str(w.slot.get_offsets()) if isinstance(w.slot,unreal.CanvasPanelSlot) else ''})
   if w.get_name().startswith('ItemLabel') and isinstance(w.slot,unreal.CanvasPanelSlot):
    # Label is an independent canvas overlay: centre its actual desired height.
    slot=w.slot;off=slot.get_offsets();r['widgets'][-1]['old_alignment']=str(slot.get_alignment())
    slot.set_auto_size(True);slot.set_alignment(unreal.Vector2D(.5,.5))
    slot.set_position(unreal.Vector2D(111,49))
  unreal.BlueprintEditorLibrary.compile_blueprint(bp);save(bp)
 assert L.load_level('/Game/MainGame/Maps/Lvl_MainGame')
 aa=A.get_all_level_actors();hall=next(a for a in aa if isinstance(a,unreal.MiningExhibitHall))
 import campaign_minerals_api as api
 field=api.get_field(hall);p=list(field.get_editor_property('authored_placements'));area=next(a for a in hall.areas if str(a.tag)=='BM_Campaign_MainGame01')
 route=list(area.map_route);r['copper_probes']=[]
 for index in [6,7]:
  old=p[index];j=min(range(len(route)),key=lambda k:abs(route[k].x-old.surface.translation.x));center=route[j]
  forward=route[min(j+1,len(route)-1)]-route[max(0,j-1)];forward.z=0;forward=forward.normal();left=unreal.Vector(forward.y,-forward.x,0)
  probes=[]
  for z in [-210,-140,-70,0,70]:
   origin=center+unreal.Vector(0,0,z)
   hit=unreal.SystemLibrary.line_trace_single(E.get_editor_world(),origin,origin+left*1400,unreal.TraceTypeQuery.ECC_VISIBILITY,True,[],unreal.DrawDebugTrace.NONE,True)
   if hit:
    h=hit.to_tuple()
    if h[0] and isinstance(h[9],unreal.DiggableRock):probes.append({'z':z,'point':api._xyz(h[5]),'normal':api._xyz(h[7]),'distance':h[3]})
  r['copper_probes'].append({'index':index,'center':api._xyz(center),'left':api._xyz(left),'old':api._xyz(old.surface.translation),'hits':probes})
 r['opening']=[{'label':a.get_actor_label(),'eye':str(a.get_editor_property('seat_eye')),'facing':str(a.get_editor_property('facing'))} for a in aa if isinstance(a,unreal.MiningOpening)]
 r['passed']=True
except Exception:r['error']=traceback.format_exc();unreal.log_error(r['error'])
(out/'assets.json').write_text(json.dumps(r,ensure_ascii=False,indent=2),encoding='utf8')
unreal.SystemLibrary.quit_editor()
