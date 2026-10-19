import unreal,sys,json,shutil,traceback
from pathlib import Path
ROOT=Path(unreal.Paths.project_dir());sys.path.insert(0,str(ROOT/'Scripts'))
from scene_polish_materials import create_materials
out=ROOT/'Saved/Validation/ScenePolish';levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);api=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
report={'passed':False,'changed':[]}
try:
 mats=create_materials(lambda p:None)
 assert levels.load_level('/Game/MainGame/Maps/Lvl_MainGame')
 shutil.copy2(ROOT/'Content/MainGame/Maps/Lvl_MainGame.umap',ROOT/'Saved/Backups/ScenePolishPull/before-surface-finish.umap')
 for a in api.get_all_level_actors():
  if not a.actor_has_tag('BM_Campaign_MainGame01'):continue
  if isinstance(a,(unreal.DiggableRock,unreal.MiningExternalRegion)):
   m=a.get_editor_property('rock_material');key=m.get_path_name().split('.')[0] if m else ''
   if key in mats:
    a.set_editor_property('rock_material',mats[key],notify_mode=unreal.PropertyAccessChangeNotifyMode.NEVER)
    for c in a.get_components_by_class(unreal.DynamicMeshComponent):c.set_material(0,mats[key])
    report['changed'].append(a.get_actor_label())
  for c in a.get_components_by_class(unreal.DynamicMeshComponent)+a.get_components_by_class(unreal.StaticMeshComponent):
   for i,m in enumerate(c.get_materials()):
    key=m.get_path_name().split('.')[0] if m else ''
    if key in mats:c.set_material(i,mats[key]);report['changed'].append(a.get_actor_label()+':'+c.get_name())
 assert levels.save_current_level();report['passed']=True
except Exception:report['error']=traceback.format_exc()
(out/'surfaces.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
levels.load_level('/Engine/Maps/Entry');unreal.SystemLibrary.quit_editor()
