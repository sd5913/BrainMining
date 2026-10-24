import unreal,json,traceback
from pathlib import Path
root=Path(unreal.Paths.project_dir());out=root/'Saved/Validation/BranchClues';L=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);A=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
try:
 assert L.load_level('/Game/MainGame/Maps/Lvl_MainGame')
 aa=list(A.get_all_level_actors());report={'regions':[],'boundaries':[]}
 for a in aa:
  if isinstance(a,unreal.MiningExternalRegion):
   report['regions'].append({'name':a.get_name(),'label':a.get_actor_label(),'location':a.get_actor_location().to_tuple(),'size':a.rock_size.to_tuple(),'shell':a.generated_shell.get_name() if a.generated_shell else None,'shellmat':a.shell_material.get_path_name() if a.shell_material else None,'cutters':[{'mesh':c.mesh.get_path_name(),'transform':c.local_transform.export_text()} for c in a.cutters if c.mesh and c.enabled]})
  label=a.get_actor_label()
  if any(x in label.lower() for x in ['bedrock','uppershellseal','不可挖','outerunmineable']):report['boundaries'].append({'name':a.get_name(),'label':label,'class':a.get_class().get_name(),'bounds':[v.to_tuple() for v in a.get_actor_bounds(False)],'tags':[str(t) for t in a.tags]})
 (out/'boundary-inventory.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
except Exception:unreal.log_error(traceback.format_exc())
finally:unreal.SystemLibrary.quit_editor()
