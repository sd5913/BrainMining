"""Rebuild the displaced heaven-wall vein in the new chamber's thicker corner."""
import unreal,json,sys,traceback
from pathlib import Path
ROOT=Path(unreal.Paths.project_dir());sys.path.insert(0,str(ROOT/'Scripts'))
import campaign_minerals_api as minerals
L=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);A=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);E=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
def repair(field,hall,shell,indices):
 placements=list(field.get_editor_property('authored_placements'));result={}
 stage=minerals._stage(hall,A,field)
 try:
  for index in indices:
   p=placements[index];center=shell.get_actor_location();start=center+unreal.Vector(0,0,630);end=start+unreal.Vector(2100,-2100,0)
   ignored=[a for a in A.get_all_level_actors() if a!=shell]
   h=unreal.SystemLibrary.line_trace_single(E.get_editor_world(),start,end,unreal.TraceTypeQuery.ECC_VISIBILITY,True,ignored,unreal.DrawDebugTrace.NONE,True).to_tuple()
   assert h[0] and h[9]==shell
   t=unreal.Transform(location=h[5],rotation=unreal.MathLibrary.make_rot_from_x(h[7]));p.surface=t;p.transform=t;p.hosts=[shell];p.fragments=[];p.fragment_supports=[]
   stage.set_editor_property('authored_placements',[p]);assert unreal.MiningDesignerLibrary.prepare_campaign_minerals(stage)
   check=minerals.validate(stage,hall);assert check['ok'],check['errors']
   placements[index]=list(stage.get_editor_property('authored_placements'))[0];result[str(index)]=check
  field.set_editor_property('authored_placements',placements)
 finally:A.destroy_actor(stage)
 return result
if __name__=='__main__':
 report={'passed':False}
 try:
  assert L.load_level('/Game/MainGame/Maps/Lvl_MainGame');aa=list(A.get_all_level_actors())
  hall=next(a for a in aa if isinstance(a,unreal.MiningExhibitHall));field=minerals.get_field(hall);shell=next(a for a in aa if a.get_actor_label()=='COL_Heaven_DiggableDomeShell')
  report['validation']=repair(field,hall,shell,[64]);assert L.save_current_level();report['passed']=True
 except Exception:report['error']=traceback.format_exc()
 (ROOT/'Saved/Validation/ScenePolish/heaven_mineral.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
 L.load_level('/Engine/Maps/Entry');unreal.SystemLibrary.quit_editor()
