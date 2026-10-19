"""Turn the mesh around its handle, preserving the authored grip and swing pivots."""
import unreal,json,shutil,traceback
from pathlib import Path
root=Path(unreal.Paths.project_dir());result={'passed':False}
try:
 path='Content/FirstPerson/Blueprints/BP_FirstPersonCharacter.uasset'
 backup=root/'Saved/Backups/GameplayFollowup20261018/Assets'/path
 if not backup.exists():backup.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(root/path,backup)
 bp=unreal.load_asset('/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter')
 subsystem=unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem);lib=unreal.SubobjectDataBlueprintFunctionLibrary
 model=None
 for h in subsystem.k2_gather_subobject_data_for_blueprint(bp):
  obj=lib.get_object_for_blueprint(lib.get_data(h),bp)
  if obj and obj.get_name()=='PickaxeModel':model=obj;break
 assert model
 result['before']=str(model.get_relative_transform())
 if unreal.EditorAssetLibrary.get_metadata_tag(bp,'MiningStrikingHeadForward')!='1':
  rotation=model.get_editor_property('relative_rotation').quaternion()*unreal.Rotator(yaw=180).quaternion()
  model.set_editor_property('relative_rotation',rotation.rotator())
  unreal.EditorAssetLibrary.set_metadata_tag(bp,'MiningStrikingHeadForward','1')
  unreal.BlueprintEditorLibrary.compile_blueprint(bp)
  assert unreal.EditorAssetLibrary.save_loaded_asset(bp,False)
 result['after']=str(model.get_relative_transform());result['passed']=True
except Exception:result['error']=traceback.format_exc();unreal.log_error(result['error'])
(root/'Saved/Validation/GameplayFollowup/pickaxe-alignment.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
unreal.SystemLibrary.quit_editor()
