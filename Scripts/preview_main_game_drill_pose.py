"""Try the existing shared-character drill pose in a disposable PIE copy."""
import unreal
from pathlib import Path
root=Path(unreal.Paths.project_dir()).resolve()
code=(root/'Scripts/verify_main_game_drill.py').read_text(encoding='utf-8').replace("OUT=ROOT/'Saved/Validation/MainGameDrill'","OUT=ROOT/'Saved/Validation/MainGameDrill/Preview'")
code=code.replace("frame('pickaxe');press('V')", "frame('pickaxe');source=unreal.get_default_object(unreal.load_asset('/Game/Showcase/Blueprints/BP_SharedMiningCharacter').generated_class());parts={c.get_name():c for c in s['pawn'].get_components_by_class(unreal.SceneComponent)}\n            for stem in ['DrillRightMount','DrillLeftMount','DrillRightModel','DrillLeftModel']:\n                original=source.get_editor_property(stem)\n                for prop in ['relative_location','relative_rotation','relative_scale3d']:parts[stem].set_editor_property(prop,original.get_editor_property(prop))\n            press('V')")
exec(compile(code,'drill_pose_preview','exec'),{'__name__':'__main__'})
