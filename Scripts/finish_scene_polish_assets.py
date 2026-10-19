import unreal,json,shutil,traceback
from pathlib import Path
ROOT=Path(unreal.Paths.project_dir());OUT=ROOT/'Saved/Validation/ScenePolish'
report={'passed':False,'pipe_materials':[]};EA=unreal.EditorAssetLibrary
try:
 material=unreal.load_asset('/Game/CollectionPrototype/HeavenToilet/Materials/M_HeavenPipe');assert material
 for suffix in ['57CD01F4','D798609C']:
  path='/Game/CollectionPrototype/HeavenToilet/Maps/_GENERATED/Lenovo/modular_pipes_junct_oly_and_hard_surface_Submesh_'+suffix
  mesh=unreal.load_asset(path);assert mesh
  rel=Path('Content')/(path.removeprefix('/Game/')+'.uasset');backup=ROOT/'Saved/Backups/ScenePolishPull'/rel
  backup.parent.mkdir(parents=True,exist_ok=True)
  if not backup.exists():shutil.copy2(ROOT/rel,backup)
  for i,slot in enumerate(mesh.static_materials):
   if not slot.material_interface:mesh.set_material(i,material);report['pipe_materials'].append(path+':'+str(i))
  assert EA.save_loaded_asset(mesh,False)
 duck=unreal.load_asset('/Game/HeavenPreview/Meshes/SM_DarkDuck')
 report['duck']={'bounds':str(duck.get_bounding_box()),'materials':[str(s.material_interface) for s in duck.static_materials]}
 report['passed']=True
except Exception:report['error']=traceback.format_exc()
(OUT/'asset_finish.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
unreal.SystemLibrary.quit_editor()
