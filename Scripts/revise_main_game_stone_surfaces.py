"""Smooth differentiated ordinary stone, remove auxiliary cave lights, keep terrain intact."""
import unreal,json,traceback,runpy
from pathlib import Path
ROOT=Path(unreal.Paths.project_dir());lib=unreal.MaterialEditingLibrary
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world.get_path_name()=='/Game/MainGame/Maps/Lvl_MainGame.Lvl_MainGame'
recipe=json.loads((ROOT/'Config/MainGameRockThemes.json').read_text())
parent=unreal.load_asset(recipe['folder']+'/'+recipe['master'])
custom=lib.get_material_property_input_node(parent,unreal.MaterialProperty.MP_EMISSIVE_COLOR)
assert isinstance(custom,unreal.MaterialExpressionCustom)
custom.set_editor_property('code',(ROOT/'Art/MainGame/Materials/StageRockPattern.hlsl').read_text())
assert not lib.recompile_material(parent)
assert unreal.EditorAssetLibrary.save_loaded_asset(parent,only_if_is_dirty=False)
runpy.run_path(str(ROOT/'Scripts/update_main_game_rock_themes.py'),run_name='__main__')
assert json.loads((ROOT/'Saved/Validation/main_game_rock_themes.json').read_text())['passed']
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);removed=[]
for actor in actors.get_all_level_actors():
    if actor.get_actor_label().startswith(('MainGame_V2_','MainGame_V3_','MainGame_V4_')) and actor.get_components_by_class(unreal.LightComponent):
        removed.append(actor.get_actor_label());assert actors.destroy_actor(actor)
assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
(ROOT/'Saved/Validation/main_game_smooth_stone_revision.json').write_text(json.dumps(dict(passed=True,removed_lights=removed,
    themes=[t['label'] for t in recipe['themes']],emission_disabled=True,saved=True),ensure_ascii=False,indent=2),encoding='utf-8')
unreal.log('SMOOTH_STONE_REVISION '+str(removed))
