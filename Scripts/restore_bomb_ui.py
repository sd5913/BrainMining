"""Restore the original bomb artwork immediately beside drill V; no cooldown artwork."""
import json
from pathlib import Path
import unreal

assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
texture = unreal.load_asset('/Game/UI/MiningGUI/Art/T_BombAbility')
assert texture
bp = unreal.load_asset('/Game/UI/MiningGUI/WBP_AbilityG')
defaults = unreal.get_default_object(bp.generated_class())
defaults.set_editor_property('normal_texture', texture)
defaults.set_editor_property('used_texture', texture)
defaults.set_editor_property('bAvailable', True)
defaults.set_editor_property('bUsed', False)
widgets = [obj for obj in unreal.ObjectIterator(unreal.Widget)
           if obj.get_path_name().startswith(bp.get_path_name()+':WidgetTree.')]
icon = next(obj for obj in widgets if obj.get_name() == 'AbilityIcon')
content = next(obj for obj in widgets if obj.get_name() == 'AbilityContent')
# The supplied PNG includes its own G key. Scale the complete image to match
# the octagon height of the adjacent drill, with the key at the same baseline.
icon.set_brush_from_texture(texture, False)
icon.slot.set_position(unreal.Vector2D(-33., -14.))
icon.slot.set_size(unreal.Vector2D(166., 166.))
for widget in widgets:
    if widget.get_parent() == content and widget != icon:
        widget.set_visibility(unreal.SlateVisibility.COLLAPSED)
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
assert assets.save_loaded_asset(bp, False)
hud = unreal.load_asset('/Game/UI/MiningGUI/WBP_MiningHUD')
g = next(obj for obj in unreal.ObjectIterator(unreal.MiningGUIAbilityWidget)
         if obj.get_path_name() == hud.get_path_name()+':WidgetTree.AbilityG')
g.set_available(True)
g.set_used(False)
g.set_editor_property('normal_texture', texture)
g.set_editor_property('used_texture', texture)
unreal.BlueprintEditorLibrary.compile_blueprint(hud)
assert assets.save_loaded_asset(hud, False)
unreal.log('BOMB_G_ICON_RESTORED')
