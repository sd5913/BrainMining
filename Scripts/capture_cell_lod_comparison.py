import json
import time
import traceback
from pathlib import Path

import unreal


output = Path(unreal.Paths.project_dir()) / "Saved/Validation/SizeOptimization/CellVisual"
output.mkdir(parents=True, exist_ok=True)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
helper = unreal.MiningGUIEditorLibrary
state = {"phase": 0, "next": 0, "started": time.monotonic(), "busy": False, "images": []}


def finish(error=None):
    state["done"] = True
    unreal.unregister_slate_post_tick_callback(callback)
    (output / "result.json").write_text(json.dumps({"passed": error is None, "error": error, "images": state["images"]}, indent=2), encoding="utf-8")
    levels.editor_request_end_play()
    cleanup = {"after": time.monotonic() + 3}

    def close(delta):
        if editor.get_game_world() or time.monotonic() < cleanup["after"]:
            return
        unreal.unregister_slate_post_tick_callback(cleanup["callback"])
        unreal.SystemLibrary.quit_editor()

    cleanup["callback"] = unreal.register_slate_post_tick_callback(close)


def tick(delta):
    if state["busy"] or state.get("done") or time.monotonic() < state["next"]:
        return
    state["busy"] = True
    try:
        assert time.monotonic() - state["started"] < 180, "Cell capture timed out"
        if state["phase"] == 0:
            assert levels.load_level("/Engine/Maps/Entry")
            mesh = unreal.load_asset("/Game/Fab/Cell/cell/StaticMeshes/cell")
            bounds = mesh.get_bounds()
            origin = bounds.origin
            radius = bounds.sphere_radius
            mesh_actor = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector())
            mesh_actor.set_actor_label("SizeAuditCell")
            component = mesh_actor.get_component_by_class(unreal.StaticMeshComponent)
            component.set_static_mesh(mesh)
            component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
            camera_location = origin + unreal.Vector(radius * 2.4, -radius * 1.2, radius * 0.9)
            camera = actors.spawn_actor_from_class(unreal.CameraActor, camera_location)
            camera.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(camera_location, origin), False)
            light = actors.spawn_actor_from_class(unreal.DirectionalLight, camera_location)
            light.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(camera_location, origin), False)
            light.get_component_by_class(unreal.DirectionalLightComponent).set_intensity(6)
            unreal.SystemLibrary.execute_console_command(editor.get_editor_world(), "mining.SaveEnabled 0")
            helper.start_floating_preview()
            state.update(phase=1, next=time.monotonic() + 4)
        elif state["phase"] == 1:
            world = editor.get_game_world()
            if not world:
                return
            controller = unreal.GameplayStatics.get_player_controller(world, 0)
            if not controller:
                return
            camera = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.CameraActor)[0]
            controller.set_view_target_with_blend(camera, 0)
            helper.resize_preview_viewport(controller, 960, 720)
            components = [actor.get_component_by_class(unreal.StaticMeshComponent) for actor in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.StaticMeshActor)]
            component = next(component for component in components if component.static_mesh and component.static_mesh.get_name() == "cell")
            component.set_forced_lod_model(1)
            state.update(controller=controller, component=component, phase=2, next=time.monotonic() + 4)
        elif state["phase"] == 2:
            assert helper.capture_preview(state["controller"], str(output / "desktop_lod0.png"))
            state["images"].append("desktop_lod0.png")
            state["component"].set_forced_lod_model(2)
            state.update(phase=3, next=time.monotonic() + 3)
        elif state["phase"] == 3:
            assert helper.capture_preview(state["controller"], str(output / "android_lod1_desktop_preview.png"))
            state["images"].append("android_lod1_desktop_preview.png")
            finish()
    except Exception:
        finish(traceback.format_exc())
    finally:
        state["busy"] = False


callback = unreal.register_slate_post_tick_callback(tick)
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
