"""PIE-only visual check for the heavy side of the museum balance."""
import unreal
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
assert world
for exhibit in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.WenSiMirrorExhibit):
    if exhibit.get_editor_property('kind')==6:
        exhibit.set_editor_property('word',unreal.WenSiMirrorWord.HEAVY)
        unreal.log('WENSI_BALANCE_HEAVY_PREVIEW')
        break
