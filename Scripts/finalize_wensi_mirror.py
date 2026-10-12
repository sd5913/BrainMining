"""Finish the newly created museum map only; never edits the previous WenSi maps."""
import unreal

world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world.get_path_name().startswith('/Game/WenSi/Maps/Lvl_WenSi_MirrorMuseum.'),world.get_path_name()
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
for actor in actors.get_all_level_actors():
    if isinstance(actor,unreal.WenSiMirrorExhibit):
        kind=actor.get_editor_property('kind')
        if kind==12:
            actor.set_actor_location(unreal.Vector(1110,480,104),False,False)
        if kind==7:
            for rim in actor.get_components_by_class(unreal.StaticMeshComponent):
                if rim.get_name()=='Part7':
                    rim.set_visibility(False)
                    rim.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
assert unreal.EditorLoadingAndSavingUtils.save_map(world,'/Game/WenSi/Maps/Lvl_WenSi_MirrorMuseum')
unreal.log('WENSI_MIRROR_FINALIZED')
