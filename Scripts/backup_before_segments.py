import json
from pathlib import Path
import unreal

output = Path(unreal.Paths.project_saved_dir()) / "RoomDrivenBackup" / "Segments_2026-10-05_15-16-00"
output.mkdir(parents=True, exist_ok=True)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
world = editor.get_game_world()
report = {"rocks": []}

def vector(value):
    return [value.x, value.y, value.z]

if world:
    pawn = unreal.GameplayStatics.get_player_pawn(world, 0)
    if pawn:
        report["player_position"] = vector(pawn.get_actor_location())
        report["player_rotation"] = str(pawn.get_control_rotation())
    for rock in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.DiggableRock):
        mesh = rock.rock_mesh.get_dynamic_mesh()
        positions = unreal.GeometryScript_MeshQueries.get_all_vertex_positions(mesh, False)[1]
        triangles = unreal.GeometryScript_MeshQueries.get_all_triangle_indices(mesh, True)[1]
        vertices = unreal.GeometryScript_List.convert_vector_list_to_array(positions)
        indices = unreal.GeometryScript_List.convert_triangle_list_to_array(triangles)
        report["rocks"].append({"name": rock.get_name(), "location": vector(rock.get_actor_location()), "scale": vector(rock.get_actor_scale3d()), "cuts": rock.successful_cuts, "triangle_count": rock.triangle_count, "vertices": [vector(value) for value in vertices], "triangles": [vector(value) for value in indices]})
    report["rooms"] = [{"order": room.connection_order, "location": vector(room.get_actor_location()), "size": vector(room.room_size), "scale": vector(room.get_actor_scale3d())} for room in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningLayoutRoom)]
    (output / "live_excavation.json").write_text(json.dumps(report), encoding="utf-8")
    unreal.log("SEGMENT_BACKUP_OK " + str([(rock["cuts"], rock["triangle_count"]) for rock in report["rocks"]]))
