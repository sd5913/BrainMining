import json
import time
from pathlib import Path
import unreal

assert '-SpatialDragUIValidation' in unreal.SystemLibrary.get_command_line()
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
assert editor.get_game_world() is None
drag_world = unreal.EditorLoadingAndSavingUtils.new_map_from_template('/Game/MiningPrototype/Maps/Lvl_MiningSpatial_Demo', False)
assert drag_world and not drag_world.get_path_name().startswith('/Game/MiningPrototype/Maps/Lvl_MiningSpatial_Demo.')
drag_actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
drag_generator = next(actor for actor in drag_actors.get_all_level_actors() if isinstance(actor, unreal.MiningTunnelGenerator))
drag_region = next(actor for actor in drag_actors.get_all_level_actors() if isinstance(actor, unreal.MiningSpatialRegion))
drag_main = drag_region.main_tunnel
drag_output = Path(unreal.Paths.project_saved_dir()) / 'Validation'
drag_full_build_ms = []
for sample in range(3):
    started = time.perf_counter()
    assert drag_generator.rebuild_tunnels(), drag_generator.last_build_status
    drag_full_build_ms.append((time.perf_counter() - started) * 1000)
drag_baseline = {
    'knots': [knot.export_text() for knot in drag_main.knots],
    'full_build_ms': drag_full_build_ms,
    'chunks': sum(len(segment.chunks) for segment in drag_generator.generated_segments),
    'budget': drag_generator.segment_triangle_budget,
    'revision': drag_generator.build_revision,
    'map': drag_world.get_path_name(),
}
(drag_output / 'spatial_drag_user_baseline.json').write_text(json.dumps(drag_baseline, ensure_ascii=False, indent=2), encoding='utf-8')
drag_actors.set_selected_level_actors([drag_main])
unreal.MiningDesignerLibrary.open_spatial_editor()
drag_recording = False
drag_frame_samples = []
drag_metrics_samples = []
drag_last_tick = time.perf_counter()

def drag_tick(delta_seconds):
    global drag_last_tick
    now = time.perf_counter()
    elapsed = (now - drag_last_tick) * 1000
    drag_last_tick = now
    if drag_recording:
        drag_frame_samples.append(elapsed)
        metrics = json.loads(unreal.MiningDesignerLibrary.spatial_drag_performance())
        if not drag_metrics_samples or metrics != drag_metrics_samples[-1]:
            drag_metrics_samples.append(metrics)

drag_tick_handle = unreal.register_slate_post_tick_callback(drag_tick)

def drag_record_start():
    global drag_recording, drag_frame_samples, drag_metrics_samples
    drag_frame_samples = []
    drag_metrics_samples = []
    drag_recording = True

def drag_record_stop(name):
    global drag_recording
    drag_recording = False
    values = sorted(drag_frame_samples)
    report = {
        'name': name,
        'metrics': json.loads(unreal.MiningDesignerLibrary.spatial_drag_performance()),
        'frame_samples_ms': values,
        'frames': len(values),
        'frame_p50_ms': values[len(values) // 2] if values else None,
        'frame_p95_ms': values[min(len(values) - 1, int(len(values) * .95))] if values else None,
        'frame_max_ms': values[-1] if values else None,
        'metrics_samples': drag_metrics_samples,
        'knots': [knot.export_text() for knot in drag_main.knots],
        'revision': drag_generator.build_revision,
        'applied': drag_generator.layout_applied,
        'status': unreal.MiningDesignerLibrary.spatial_editing_status(),
        'rebuilt_chunks': sum(segment.last_rebuilt_editor_chunks for segment in drag_generator.generated_segments),
        'reused_chunks': sum(segment.last_reused_editor_chunks for segment in drag_generator.generated_segments),
    }
    (drag_output / ('spatial_drag_ui_' + name + '.json')).write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    unreal.log('SPATIAL_DRAG_UI ' + json.dumps({key: value for key, value in report.items() if key not in ('knots', 'metrics_samples', 'frame_samples_ms')}, ensure_ascii=False))

unreal.log('SPATIAL_DRAG_UI_READY ' + json.dumps(drag_baseline, ensure_ascii=False))
