"""Build the independent Echo Passage P1 assets/map; never replace an existing map."""
import unreal, os
ROOT='/Game/Passages/Echo'
MAP=ROOT+'/Maps/Lvl_EchoPassage_P1'
assets=unreal.AssetToolsHelpers.get_asset_tools()
lib=unreal.MaterialEditingLibrary
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
if unreal.EditorAssetLibrary.does_asset_exist(MAP):raise RuntimeError('Existing Echo map preserved; refusing overwrite')
for folder in ['Maps','Materials','Audio']:unreal.EditorAssetLibrary.make_directory(ROOT+'/'+folder)
def base(name):
    path=ROOT+'/Materials/'+name
    if unreal.EditorAssetLibrary.does_asset_exist(path):return unreal.load_asset(path)
    return assets.create_asset(name,ROOT+'/Materials',unreal.Material,unreal.MaterialFactoryNew())
def flat(name,rgb):
    m=base(name);lib.delete_all_material_expressions(m)
    m.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT)
    c=lib.create_material_expression(m,unreal.MaterialExpressionConstant3Vector)
    c.set_editor_property('constant',unreal.LinearColor(*rgb,1))
    lib.connect_material_property(c,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    lib.recompile_material(m);unreal.EditorAssetLibrary.save_loaded_asset(m)
flat('M_WarmWood',(.20,.095,.033))
flat('M_WarmGlow',(1.8,.74,.21))
flat('M_PickWood',(.075,.065,.045))
flat('M_PickIron',(.19,.22,.24))
import runpy
runpy.run_path(os.path.join(os.path.dirname(__file__),'update_echo_passage_material.py'))
for name in ['S_Knock','S_Landing','S_Crumble','S_Exit']:
    filename=os.path.join(unreal.Paths.project_saved_dir(),'EchoPassageSourceAudio',name+'.wav')
    task=unreal.AssetImportTask();task.set_editor_property('filename',filename);task.set_editor_property('destination_path',ROOT+'/Audio');task.set_editor_property('automated',True);task.set_editor_property('save',True);assets.import_asset_tasks([task])
if not levels.new_level(MAP):raise RuntimeError('Could not create Echo map')
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
world.get_world_settings().set_editor_property('default_game_mode',unreal.load_class(None,'/Script/BrainMining.MiningEncounterGameMode'))
stage=actors.spawn_actor_from_class(unreal.load_class(None,'/Script/EchoPassage.EchoPassageStage'),unreal.Vector())
stage.set_actor_label('EchoPassage_IndependentEncounter')
start=actors.spawn_actor_from_class(unreal.PlayerStart,unreal.Vector(-480,0,998),unreal.Rotator())
start.set_actor_label('EchoPassage_Departure')
post=actors.spawn_actor_from_class(unreal.PostProcessVolume,unreal.Vector());post.set_editor_property('unbound',True)
s=post.get_editor_property('settings')
for name,value in [('override_auto_exposure_method',True),('auto_exposure_method',unreal.AutoExposureMethod.AEM_MANUAL),('override_auto_exposure_bias',True),('auto_exposure_bias',0.0),('override_auto_exposure_apply_physical_camera_exposure',True),('auto_exposure_apply_physical_camera_exposure',False),('override_bloom_intensity',True),('bloom_intensity',.12),('override_motion_blur_amount',True),('motion_blur_amount',0.0)]:s.set_editor_property(name,value)
post.set_editor_property('settings',s)
unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).set_level_viewport_camera_info(unreal.Vector(110,-60,185),unreal.Rotator(pitch=-8,yaw=0,roll=0))
if not levels.save_current_level():raise RuntimeError('Echo map save failed')
unreal.log('ECHO_PASSAGE_V01_CREATED: '+MAP)
