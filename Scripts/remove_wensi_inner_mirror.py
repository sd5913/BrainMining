"""Remove only the redundant mirror inside the second-phase cover."""
import unreal

MAP='/Game/WenSi/Maps/Lvl_WenSi_MirrorMuseum'
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
if not world.get_path_name().startswith(MAP+'.'):
    assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level(MAP)
    world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
removed=0
for actor in actors.get_all_level_actors():
    if isinstance(actor,unreal.WenSiMirrorExhibit) and actor.get_editor_property('kind')==11:
        assert actors.destroy_actor(actor)
        removed+=1
mirrors=[actor.get_editor_property('kind') for actor in actors.get_all_level_actors()
    if isinstance(actor,unreal.WenSiMirrorExhibit) and actor.get_editor_property('kind') in (10,11)]
assert mirrors==[10],mirrors
assert unreal.EditorLoadingAndSavingUtils.save_map(world,MAP)
unreal.log('WENSI_INNER_MIRROR_REMOVED: removed='+str(removed)+' remaining=exterior')
