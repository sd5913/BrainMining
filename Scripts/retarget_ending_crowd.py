"""Retarget acquired Basic Walk clips into ending-only assets; never edit the packs."""
import unreal
import json
from pathlib import Path

ROOT='/Game/EndingEmergence/Crowd'
assets=unreal.AssetToolsHelpers.get_asset_tools()
unreal.EditorAssetLibrary.make_directory(ROOT)
def create(name, cls, factory):
    path=ROOT+'/'+name
    return unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else assets.create_asset(name,ROOT,cls,factory())

source=unreal.load_asset('/Game/Walk_cycle_AnimsSet/Demo/Meshes/SKM_Quinn')
source_rig=create('IK_EE_BasicWalk',unreal.IKRigDefinition,unreal.IKRigDefinitionFactory)
controller=unreal.IKRigController.get_controller(source_rig)
controller.set_skeletal_mesh(source)
assert controller.apply_auto_generated_retarget_definition()
controller.apply_auto_fbik()
unreal.EditorAssetLibrary.save_loaded_asset(source_rig)

for gender,prefix,clip in [('Female','f','Anim_FemaleWalk'),('Male','m','Anim_OldManWalk')]:
    target=unreal.load_asset(f'/Game/CitySampleCrowd/Character/{gender}/NormalWeight/Meshes/{prefix}_tal_nrw_base')
    target_rig=create('IK_EE_'+gender,unreal.IKRigDefinition,unreal.IKRigDefinitionFactory)
    controller=unreal.IKRigController.get_controller(target_rig)
    controller.set_skeletal_mesh(target)
    assert controller.apply_auto_generated_retarget_definition()
    controller.apply_auto_fbik()
    rt=create('RTG_EE_'+gender,unreal.IKRetargeter,unreal.IKRetargetFactory)
    rc=unreal.IKRetargeterController.get_controller(rt)
    rc.set_ik_rig(unreal.RetargetSourceOrTarget.SOURCE,source_rig)
    rc.set_ik_rig(unreal.RetargetSourceOrTarget.TARGET,target_rig)
    rc.remove_all_ops()
    rc.add_default_ops()
    rc.auto_map_chains(unreal.AutoMapChainType.EXACT,True)
    rc.auto_align_all_bones(unreal.RetargetSourceOrTarget.TARGET)
    rc.reset_retarget_pose(rc.get_current_retarget_pose_name(unreal.RetargetSourceOrTarget.TARGET),['foot_l','foot_r','ball_l','ball_r'],unreal.RetargetSourceOrTarget.TARGET)
    for a in [target_rig,rt]: unreal.EditorAssetLibrary.save_loaded_asset(a)
    inputs=unreal.IKRetargetBatchOperationInputs()
    inputs.assets_to_retarget=[unreal.EditorAssetLibrary.find_asset_data('/Game/Walk_cycle_AnimsSet/Animation/'+clip)]
    inputs.source_mesh=source
    inputs.target_mesh=target
    inputs.ik_retarget_asset=rt
    inputs.target_path=ROOT
    inputs.prefix='EE_'
    inputs.include_referenced_assets=False
    inputs.overwrite_existing_files=True
    result=unreal.IKRetargetBatchOperation.run_batch_retarget(inputs)
    assert len(result)==1, str(result)
    anim=result[0].get_asset()
    unreal.AnimationLibrary.set_is_root_motion_lock_forced(anim,True)
    unreal.AnimationLibrary.set_root_motion_enabled(anim,False)
    unreal.AnimationLibrary.set_root_motion_lock_type(anim,unreal.RootMotionRootLock.REF_POSE)
    assert unreal.EditorAssetLibrary.save_loaded_asset(anim)
    unreal.log('ENDING_WALK_RETARGETED '+anim.get_path_name())

report=[]
for path in [
 '/Game/CitySampleCrowd/Character/Male/m_001/Face/m_001_nrw_FaceMesh',
 '/Game/CitySampleCrowd/Character/Female/f_001/Face/f_001_nrw_FaceMesh',
 '/Game/CitySampleCrowd/Character/Male/m_001/Hair/Helmets/Hair_S_AfroFade_Helmet_LOD5',
 '/Game/CitySampleCrowd/Character/Female/f_001/Hair/Helmets/Hair_S_Coil_Helmet_LOD5']:
    a=unreal.load_asset(path)
    row={'path':path,'class':a.get_class().get_name()}
    if isinstance(a,unreal.SkeletalMesh):
        row['materials']=[m.material_interface.get_path_name() if m.material_interface else None for m in a.get_editor_property('materials')]
    else:
        row['bounds']=str(a.get_bounding_box())
    report.append(row)
Path(unreal.Paths.project_saved_dir(),'Validation','EndingCrowdHeads.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
unreal.log('ENDING_CROWD_RETARGET_COMPLETE')
