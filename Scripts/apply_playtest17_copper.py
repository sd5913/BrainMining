"""Place the two early copper clues on the low side wall, outside the walking line."""
import unreal,json,sys,shutil,traceback
from pathlib import Path
root=Path(unreal.Paths.project_dir());sys.path.insert(0,str(root/'Scripts'))
import campaign_minerals_api as api
out=root/'Saved/Validation/Playtest17';r={'passed':False}
L=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);A=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);E=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
try:
 assert L.load_level('/Game/MainGame/Maps/Lvl_MainGame')
 hall=next(a for a in A.get_all_level_actors() if isinstance(a,unreal.MiningExhibitHall));field=api.get_field(hall)
 placements=list(field.get_editor_property('authored_placements'));before=api._layout(field)
 probes=json.loads((out/'assets.json').read_text(encoding='utf8'))['copper_probes'];changes=[]
 for data in probes:
  index=data['index'];old=placements[index];origin=unreal.Vector(*data['center'])+unreal.Vector(0,0,-140 if index==6 else 0)
  hit=unreal.SystemLibrary.line_trace_single(E.get_editor_world(),origin,origin+unreal.Vector(*data['left'])*1400,unreal.TraceTypeQuery.ECC_VISIBILITY,True,[],unreal.DrawDebugTrace.NONE,True).to_tuple()
  assert hit[0] and isinstance(hit[9],unreal.DiggableRock) and abs(hit[7].z)<.4
  p=unreal.MiningTreasurePlacement()
  for key in ['treasure_id','value','area_tag','route_progress','guide_layer']:p.set_editor_property(key,old.get_editor_property(key))
  p.set_editor_property('surface',unreal.Transform(location=hit[5],rotation=unreal.MathLibrary.conv_vector_to_rotator(hit[7])))
  p.set_editor_property('transform',unreal.Transform(location=hit[5]-hit[7]*120));p.set_editor_property('hosts',[hit[9]])
  changes.append(p)
 stage=api._stage(hall,A,field);stage.set_editor_property('authored_placements',changes)
 assert unreal.MiningDesignerLibrary.prepare_campaign_minerals(stage)
 report=api.validate(stage,hall);assert report['ok'],report
 for index,p in zip([6,7],stage.get_editor_property('authored_placements')):placements[index]=p
 field.set_editor_property('authored_placements',placements);A.destroy_actor(stage)
 after=api._layout(field)
 assert all(before[i]==after[i] for i in range(len(before)) if i not in [6,7])
 src=root/'Content/MainGame/Maps/Lvl_MainGame.umap';backup=out/'Before/Lvl_MainGame-before-side-copper.umap'
 if not backup.exists():shutil.copy2(src,backup)
 assert L.save_current_level();r.update(passed=True,validation=report,moves=[{'index':i,'before':before[i],'after':after[i]} for i in [6,7]])
except Exception:r['error']=traceback.format_exc();unreal.log_error(r['error'])
(out/'copper.json').write_text(json.dumps(r,ensure_ascii=False,indent=2),encoding='utf8');unreal.SystemLibrary.quit_editor()
