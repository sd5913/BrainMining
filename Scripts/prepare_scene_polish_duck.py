"""Build a self-contained rubber duck; the imported mesh lacks its original face materials."""
import unreal,json,sys,traceback,shutil
from pathlib import Path
ROOT=Path(unreal.Paths.project_dir());sys.path.insert(0,str(ROOT/'Scripts'))
from scene_polish_materials import mat,node,prop,finish
EA=unreal.EditorAssetLibrary;levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);api=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
report={'passed':False}
try:
 materials=[]
 for name,color in [('Yellow',(1,.65,.015)),('Orange',(.85,.16,.008)),('Eyes',(.008,.012,.016)),('Glint',(.9,.94,1)),('Wing',(.9,.47,.008))]:
  m=mat('/Game/MainGame/Materials/ScenePolish/M_RubberDuck'+name)
  prop(node(m,unreal.MaterialExpressionConstant3Vector,constant=unreal.LinearColor(*color,1)),unreal.MaterialProperty.MP_BASE_COLOR)
  prop(node(m,unreal.MaterialExpressionConstant,r=.36),unreal.MaterialProperty.MP_ROUGHNESS);finish(m);materials.append(m)
 source=unreal.load_asset('/Game/HeavenPreview/Meshes/SM_DarkDuck');geo=unreal.DynamicMesh()
 def oval(p,r,material):
  unreal.GeometryScript_Primitives.append_sphere_lat_long(geo,unreal.GeometryScriptPrimitiveOptions(material_id=material),unreal.Transform(location=unreal.Vector(*p),scale=unreal.Vector(*r)),radius=1,steps_phi=16,steps_theta=32)
 oval((0,8,29),(43,50,29),0);oval((0,-25,69),(26,26,29),0)
 oval((0,-51,61),(19,22,6),1);oval((0,47,39),(16,24,13),0)
 for sign in [-1,1]:
  oval((sign*39,10,34),(7,27,16),4)
  oval((sign*18,-43,79),(4.2,4.2,4.5),2)
  oval((sign*19,-46,80.5),(1.2,1.2,1.3),3)
 path='/Game/MainGame/Meshes/SM_HeavenRubberDuck'
 duck=unreal.load_asset(path) if EA.does_asset_exist(path) else EA.duplicate_asset(source.get_path_name(),path)
 _,result=unreal.GeometryScript_AssetUtils.copy_mesh_to_static_mesh(geo,duck,unreal.GeometryScriptCopyMeshToAssetOptions(enable_recompute_normals=False,enable_recompute_tangents=True),unreal.GeometryScriptMeshWriteLOD())
 slots=[]
 for m in materials:
  slot=unreal.StaticMaterial();slot.set_editor_property('material_interface',m);slots.append(slot)
 duck.set_editor_property('static_materials',slots)
 assert EA.save_loaded_asset(duck,False)
 assert levels.load_level('/Game/MainGame/Maps/Lvl_MainGame')
 for a in api.get_all_level_actors():
  if isinstance(a,unreal.HeavenToilet):a.spray_duck.set_static_mesh(duck);a.set_actor_label('天堂马桶 · 持续喷水与小黄鸭')
 assert levels.save_current_level()
 report.update(passed=True,bounds=str(duck.get_bounding_box()),result=str(result))
except Exception:report['error']=traceback.format_exc()
(ROOT/'Saved/Validation/ScenePolish/duck.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
levels.load_level('/Engine/Maps/Entry');unreal.SystemLibrary.quit_editor()
