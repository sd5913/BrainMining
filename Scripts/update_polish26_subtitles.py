"""Move the soil discovery line out of the room-exit subtitle asset."""
import unreal

segment = unreal.load_asset('/Game/Subtitles/DA_LeaveStartRoom')
assert segment
lines = [line for line in segment.get_editor_property('lines') if '软软的泥土' not in str(line.text)]
assert lines, 'Preserve the authored room-exit line'
segment.set_editor_property('lines', lines)
assert unreal.EditorAssetLibrary.save_loaded_asset(segment, False)
unreal.log('POLISH26 room exit lines: ' + repr([str(line.text) for line in lines]))
unreal.SystemLibrary.quit_editor()
