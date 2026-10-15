import unreal,json
from pathlib import Path
ROOT=Path(unreal.Paths.project_dir()).resolve()
reference=json.loads((ROOT/'Saved/Validation/main_game_fulljourney_reference.json').read_text())
source=unreal.load_class(None,reference['game_mode']);assert source
defaults=unreal.get_default_object(source)
bp=unreal.load_asset('/Game/MainGame/Blueprints/BP_MainGameGameMode')
unreal.BlueprintEditorLibrary.reparent_blueprint(bp,source)
mode=unreal.get_default_object(bp.generated_class())
mode.set_editor_property('default_pawn_class',unreal.load_class(None,'/Game/MainGame/Blueprints/BP_MainGameCharacter.BP_MainGameCharacter_C'))
mode.set_editor_property('player_controller_class',defaults.player_controller_class)
mode.set_editor_property('hud_class',defaults.hud_class)
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
assert unreal.EditorAssetLibrary.save_loaded_asset(bp,only_if_is_dirty=False)
assert isinstance(unreal.get_default_object(bp.generated_class()),unreal.MiningExhibitGameMode)
report=json.loads((ROOT/'Saved/Validation/main_game_system_integration.json').read_text())
report['game_mode_parent']=source.get_path_name()
(ROOT/'Saved/Validation/main_game_system_integration.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
