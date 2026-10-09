import hashlib
import json
import traceback
from pathlib import Path

import unreal


project = Path(unreal.Paths.project_dir()).resolve()
assert "saved/validation/" in project.as_posix().lower(), "Run only in an isolated validation project"
map_file = project / "Content/MiningPrototype/Maps/Lvl_MiningSpatial_Demo.umap"
original_hash = hashlib.sha256(map_file.read_bytes()).hexdigest()
report = {"passed": False}


def authoring_snapshot(world):
    result = {}
    for actor_class, properties in (
        (unreal.MiningLayoutRoom, ("room_size", "floor_anchored", "connection_order")),
        (unreal.MiningSpatialRegion, ("rock_size", "automatic_bedrock", "free_editing", "excavation_margin", "automatic_bedrock_thickness")),
        (unreal.MiningSpatialTunnel, ("room_size", "initial_up", "start_portal", "end_portal", "kind", "enabled")),
    ):
        for actor in unreal.GameplayStatics.get_all_actors_of_class(world, actor_class):
            item = {name: str(actor.get_editor_property(name)) for name in properties}
            item["transform"] = actor.get_actor_transform().export_text()
            if isinstance(actor, unreal.MiningSpatialTunnel):
                item["knots"] = [knot.export_text() for knot in actor.knots]
            result[actor.get_path_name()] = item
    return result


try:
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    assert levels.load_level("/Game/MiningPrototype/Maps/Lvl_MiningSpatial_Demo")
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    before = authoring_snapshot(world)
    regions = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningSpatialRegion)
    generators = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningTunnelGenerator)
    assert regions and generators
    report["regions"] = [{"name": region.get_actor_label(), "automatic": region.automatic_bedrock, "margin": region.excavation_margin} for region in regions]
    report["generators"] = []
    for generator in generators:
        accepted = unreal.MiningDesignerLibrary.check_spatial_edit(generator)
        entry = {"accepted": accepted, "applied": generator.layout_applied, "status": generator.last_build_status, "build_ms": generator.last_build_milliseconds}
        report["generators"].append(entry)
        assert accepted and generator.layout_applied, generator.last_build_status
        assert generator.segment_triangle_budget == 30000
        assert unreal.GeometryScript_MeshQueries.get_is_closed_mesh(generator.generated_bedrock.dynamic_mesh_component.get_dynamic_mesh())
        entry["segments"] = []
        for segment in generator.generated_segments:
            entry["segments"].append({"chunks": len(segment.chunks), "largest_chunk_triangles": segment.largest_chunk_triangles})
            assert segment.max_triangles == 30000 and len(segment.chunks) <= 128
            assert segment.largest_chunk_triangles <= 30000
            assert all(unreal.GeometryScript_MeshQueries.get_is_closed_mesh(chunk.mesh.get_dynamic_mesh()) for chunk in segment.chunks)
    report["authoring_unchanged"] = before == authoring_snapshot(world)
    assert report["authoring_unchanged"]
    main = regions[0].main_tunnel
    knots = main.knots
    knot_index = min(1, len(knots) - 1)
    knot = knots[knot_index]
    original_scale = unreal.Vector2D(knot.section_scale.x, knot.section_scale.y)
    knot.section_scale = unreal.Vector2D(0, 0)
    knots[knot_index] = knot
    main.set_editor_property("knots", knots)
    assert main.knots[knot_index].section_scale.x == 0
    assert unreal.MiningDesignerLibrary.check_spatial_edit(main)
    assert not generators[0].layout_applied
    expected = [knot.export_text() for knot in main.knots]
    draft_map = "/Game/QA/SpatialFreedomDraft"
    assert unreal.EditorLoadingAndSavingUtils.save_map(world, draft_map)
    assert levels.load_level("/Engine/Maps/Entry")
    assert levels.load_level(draft_map)
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    reloaded = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningSpatialRegion)[0].main_tunnel
    generator = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningTunnelGenerator)[0]
    report["invalid_draft_saved_and_reloaded"] = expected == [knot.export_text() for knot in reloaded.knots] and not generator.layout_applied
    assert report["invalid_draft_saved_and_reloaded"]
    knots = reloaded.knots
    knot = knots[knot_index]
    knot.section_scale = original_scale
    knots[knot_index] = knot
    reloaded.set_editor_property("knots", knots)
    assert unreal.MiningDesignerLibrary.check_spatial_edit(reloaded) and generator.layout_applied
    report["reloaded_draft_can_recover"] = True
    report["passed"] = True
except Exception:
    report["error"] = traceback.format_exc()
finally:
    report["map_file_unchanged"] = original_hash == hashlib.sha256(map_file.read_bytes()).hexdigest()
    report["passed"] = report["passed"] and report["map_file_unchanged"]
    output = project / "Saved/Validation/spatial_freedom_saved_map.json"
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    unreal.log("SPATIAL_FREEDOM_SAVED_MAP " + json.dumps(report, ensure_ascii=False))
