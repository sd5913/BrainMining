"""Preflight for legacy generators, before they mutate any level or materials."""
import unreal

MAP = '/Game/EndingEmergence/Maps/Lvl_EndingEmergence_Demo'

def assert_generation_allowed():
    if not unreal.EditorAssetLibrary.does_asset_exist(MAP):
        return
    if (unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
            or unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()):
        raise RuntimeError('Use a clean editor; preserve unsaved work before running a generator.')
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if not levels.load_level(MAP):
        raise RuntimeError('Cannot inspect the ending map; refusing to regenerate it.')
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
    if any((isinstance(a, unreal.EndingEmergence) and a.get_editor_property('editable_scenery'))
           or a.actor_has_tag('EndingEditableScenery') for a in actors):
        raise RuntimeError('ENDING_SCENERY_PROTECTED: this map is artist-authored. Edit EDIT_ actors in UE; legacy generation is disabled.')

assert_generation_allowed()
