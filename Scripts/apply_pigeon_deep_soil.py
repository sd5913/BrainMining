"""Thicken only the authored pigeon soil plug into Branch_03's dead end.

Run via Run_MainGameCollectionScript.ps1 after the Editor build has finished.
The map is backed up before changes. Re-running validates the tagged result
without stretching it again. No gameplay/save data is loaded or modified.
"""
import bisect
import json
import math
import shutil
import traceback
from datetime import datetime
from pathlib import Path

import unreal


ROOT = Path(unreal.Paths.project_dir()).resolve()
OUT = ROOT / "Saved/Validation/PigeonDeepSoil"
OUT.mkdir(parents=True, exist_ok=True)
TAG = "BM_PigeonThreeStrikeSoil_20261108"
SUPERSEDED_TAG = "BM_PigeonDeepSoil_20261108"
# Current campaign settings: base depth 80 cm; level 3 versus hardness 2 gives
# DepthFactor 1.5, so one complete ordinary strike reaches at most 120 cm.
# The actual faceted cutter/impact normal changes that slightly: aim for the
# third strike (about 2-4 strikes in normal play), not a long excavation tunnel.
DEPTH = 450.0
BODY_DEPTH = 270.0
PRESERVE_FRONT = 30.0
report = {"passed": False}


def xyz(p):
    return list(p.to_tuple())


def setp(obj, key, value):
    obj.set_editor_property(key, value, notify_mode=unreal.PropertyAccessChangeNotifyMode.NEVER)


def triangles(mesh, transform):
    result = []
    for index in range(mesh.get_triangle_count()):
        valid, *vertices = unreal.GeometryScript_MeshQueries.get_triangle_positions(mesh, index)
        if valid:
            result.append([transform.transform_location(v) for v in vertices])
    return result


def ray_hits(tris, origin, direction):
    hits = []
    for a, b, c in tris:
        edge, other = b - a, c - a
        h = direction.cross(other)
        determinant = edge.dot(h)
        if abs(determinant) < 1e-8:
            continue
        delta = origin - a
        u = delta.dot(h) / determinant
        if not 0 <= u <= 1:
            continue
        cross = delta.cross(edge)
        v = direction.dot(cross) / determinant
        if v < 0 or u + v > 1:
            continue
        distance = other.dot(cross) / determinant
        if distance > .001:
            hits.append(distance)
    result = []
    for distance in sorted(hits):
        if not result or abs(distance - result[-1]) > .05:
            result.append(distance)
    return result


def inside(tris, point):
    # A generic non-axis direction avoids the triangulation seams of planar faces.
    direction = unreal.Vector(.317, .671, .431).normal()
    return len(ray_hits(tris, point, direction)) % 2 == 1


def volume(tris):
    origin = tris[0][0]
    return abs(sum((a - origin).dot((b - origin).cross(c - origin)) for a, b, c in tris)) / 6


try:
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    assert levels.load_level("/Game/MainGame/Maps/Lvl_MainGame")
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
    field = next(a for a in actors if isinstance(a, unreal.MiningCollectionField) and a.authored_placements)
    placements = list(field.authored_placements)
    bird = next(p for p in placements if p.item_id == 3)
    host = bird.host_rock
    assert host and host.actor_has_tag("BM_Collection23_PigeonSoil")
    assert not host.actor_has_tag(SUPERSEDED_TAG), (
        "This is the superseded 12 m trial. Restore only the verified task map "
        "baseline from Saved/Backups/PigeonDeepSoil-20261108-183648-760084 "
        "after closing the Editor; do not compound the old deformation.")
    # Preserve the authored difficulty. Later campaign tuning may have changed
    # this from the old zero-hardness pigeon soil without changing its identity.
    original_hardness = host.hardness_level
    report["hardness_level"] = original_hardness
    assert not host.actor_has_tag("BM_BombOnly"), "The pigeon soil must support normal excavation"
    lib = unreal.MiningDesignerLibrary
    mesh = lib.get_campaign_rock_source(host)
    assert mesh and unreal.GeometryScript_MeshQueries.get_is_closed_mesh(mesh)
    frame = host.get_actor_transform()
    before = triangles(mesh, frame)
    clue = bird.clue_transform.translation
    approach = bird.approach_transform.translation
    eye = approach + unreal.Vector(0, 0, 64)
    # This is the actual main-route side installed by apply_pigeon_surface.py.
    inward = unreal.MathLibrary.transform_direction(bird.clue_transform, unreal.Vector(1, 0, 0)).normal()
    assert inward.dot(clue - eye) > 100, "Clue direction must point from the player approach into the soil"
    routes = ROOT / "Art/MainGame/RoutesV6/TunnelCavernV2"
    plan = json.loads((routes / "placement_plan.json").read_text(encoding="utf-8"))["objects"][6]
    assert plan["path_name"] == "Branch_03"
    route = json.loads((routes / "centerlines.json").read_text(encoding="utf-8"))[plan["path_index"]]
    region = next(a for a in actors if isinstance(a, unreal.MiningExternalRegion) and a.actor_has_tag(bird.area_tag))
    route_origin = region.get_actor_location() + region.cutters[0].local_transform.translation

    def point(arc):
        arc = min(max(arc, route["arc"][0]), route["arc"][-1])
        index = max(1, min(len(route["arc"]) - 1, bisect.bisect_left(route["arc"], arc)))
        t = (arc - route["arc"][index - 1]) / (route["arc"][index] - route["arc"][index - 1])
        q = [a + (b - a) * t for a, b in zip(route["points"][index - 1], route["points"][index])]
        return route_origin + unreal.Vector(q[0] * 100, -q[1] * 100, q[2] * 100)

    def tangent(arc):
        return (point(arc + .04) - point(arc - .04)).normal()

    # Keep the existing clue and outside soil face fixed. Bend the extra depth
    # with the real dead-end route so the collectible never enters a side wall.
    start_arc = plan["start_arc_m"]
    route_start = point(start_arc)
    route_axis = tangent(start_arc)
    route_side = route_axis.cross(unreal.Vector(0, 0, 1)).normal()
    route_up = route_side.cross(route_axis).normal()
    assert route_axis.dot(inward) > .9, "Branch_03 progression must match the exterior clue"
    assert start_arc + DEPTH / 100 < route["arc"][-1], "Depth would extend past the dead end"
    old_body = bird.transform.translation
    report.update(host=host.get_path_name(), clue=xyz(clue), approach=xyz(approach), eye=xyz(eye),
                  inward=xyz(inward), original_body=xyz(old_body), route_start=xyz(route_start),
                  depth_cm=DEPTH, body_depth_cm=BODY_DEPTH, already_applied=host.actor_has_tag(TAG),
                  desired_pickaxe_strikes=3, expected_strike_range=[2, 4],
                  tuning_basis={"pickaxe_level": 3, "base_depth_cm": 80, "hardness": 2,
                                "hardness_depth_factor": 1.5, "maximum_depth_per_strike_cm": 120,
                                "resolved_stages": 1})
    if not host.actor_has_tag(TAG):
        crossings = ray_hits(before, eye, inward)
        assert len(crossings) >= 2, "Exterior sightline must cross both original soil faces"
        report["old_sightline_thickness_cm"] = crossings[1] - crossings[0]
        assert 60 < report["old_sightline_thickness_cm"] < 400, "Unexpected source plug; refuse to stretch unrelated terrain"
        local_points = unreal.GeometryScript_List.convert_vector_list_to_array(
            unreal.GeometryScript_MeshQueries.get_all_vertex_positions(mesh, False)[1])
        world_points = [frame.transform_location(p) for p in local_points]
        depths = [(p - route_start).dot(route_axis) for p in world_points]
        far = max(depths)
        assert 60 < far < 400, far
        factor = (DEPTH - PRESERVE_FRONT) / (far - PRESERVE_FRONT)
        assert factor > 3
        backup = ROOT / "Saved/Backups" / ("PigeonDeepSoil-" + datetime.now().strftime("%Y%m%d-%H%M%S-%f"))
        backup.mkdir(parents=True)
        shutil.copy2(ROOT / "Content/MainGame/Maps/Lvl_MainGame.umap", backup / "Lvl_MainGame.umap")
        report.update(backup=str(backup), original_max_depth_cm=far, stretch_factor=factor)
        for index, (p, depth) in enumerate(zip(world_points, depths)):
            if depth <= PRESERVE_FRONT:
                continue
            new_depth = PRESERVE_FRONT + (depth - PRESERVE_FRONT) * factor
            old_arc = start_arc + depth / 100
            new_arc = start_arc + new_depth / 100
            old_tangent = tangent(old_arc)
            old_side = old_tangent.cross(unreal.Vector(0, 0, 1)).normal()
            old_up = old_side.cross(old_tangent).normal()
            new_tangent = tangent(new_arc)
            new_side = new_tangent.cross(unreal.Vector(0, 0, 1)).normal()
            new_up = new_side.cross(new_tangent).normal()
            # Correct the coarse axial estimate with the actual centerline,
            # preserving the cross section while turning it with the branch.
            residual = p - point(old_arc)
            q = point(new_arc) + new_side * residual.dot(old_side) + new_up * residual.dot(old_up) + new_tangent * residual.dot(old_tangent)
            _, valid = unreal.GeometryScript_MeshEdits.set_vertex_position(mesh, index, unreal.MathLibrary.inverse_transform_location(frame, q), True)
            assert valid, index
        assert unreal.GeometryScript_MeshQueries.get_is_closed_mesh(mesh)
        unreal.GeometryScript_Normals.set_per_face_normals(mesh)
        # Position relative to the route center, retaining the original height
        # and lateral offset so the body remains visibly embedded in the bank.
        old_arc = start_arc + (old_body - route_start).dot(route_axis) / 100
        old_tangent = tangent(old_arc)
        old_side = old_tangent.cross(unreal.Vector(0, 0, 1)).normal()
        old_up = old_side.cross(old_tangent).normal()
        offset = old_body - point(old_arc)
        body_arc = start_arc + BODY_DEPTH / 100
        body_tangent = tangent(body_arc)
        body_side = body_tangent.cross(unreal.Vector(0, 0, 1)).normal()
        body_up = body_side.cross(body_tangent).normal()
        body = point(body_arc) + body_side * offset.dot(old_side) + body_up * offset.dot(old_up)
        after = triangles(mesh, frame)
        samples = [body + unreal.Vector(x, y, z) for x in (-45, 0, 45) for y in (-45, 0, 45) for z in (-35, 0, 35)]
        assert all(inside(after, p) for p in samples), "The complete resident body must be covered by real soil"
        assert lib.set_campaign_rock_source(host, mesh)
        bird.transform = unreal.Transform(location=body, rotation=unreal.MathLibrary.make_rot_from_x(body_tangent), scale=bird.transform.scale3d)
        setp(field, "authored_placements", placements)
        setp(host, "tags", list(host.tags) + [TAG])
        assert host.hardness_level == original_hardness
        host.reset_rock()
        assert levels.save_current_level()
    else:
        after = before
        body = bird.transform.translation
        # Tune only the resident placement on an already-authored 4.5 m plug.
        # This keeps all source geometry and every unrelated map actor intact.
        prior_path = OUT / "authoring.json"
        prior = json.loads(prior_path.read_text(encoding="utf-8"))
        assert prior.get("passed") and abs(prior["depth_cm"] - DEPTH) < .01
        assert body.distance(unreal.Vector(*prior["body"])) < .1, "Resident changed since the last verified authoring report"
        previous_depth = prior["body_depth_cm"]
        if abs(previous_depth - BODY_DEPTH) > .01:
            old_arc = start_arc + previous_depth / 100
            old_tangent = tangent(old_arc)
            old_side = old_tangent.cross(unreal.Vector(0, 0, 1)).normal()
            old_up = old_side.cross(old_tangent).normal()
            offset = body - point(old_arc)
            body_arc = start_arc + BODY_DEPTH / 100
            body_tangent = tangent(body_arc)
            body_side = body_tangent.cross(unreal.Vector(0, 0, 1)).normal()
            body_up = body_side.cross(body_tangent).normal()
            body = point(body_arc) + body_side * offset.dot(old_side) + body_up * offset.dot(old_up)
            samples = [body + unreal.Vector(x, y, z) for x in (-45, 0, 45) for y in (-45, 0, 45) for z in (-35, 0, 35)]
            assert all(inside(after, p) for p in samples), "Tuned resident must remain entirely inside real soil"
            backup = ROOT / "Saved/Backups" / ("PigeonDepthTune-" + datetime.now().strftime("%Y%m%d-%H%M%S-%f"))
            backup.mkdir(parents=True)
            shutil.copy2(ROOT / "Content/MainGame/Maps/Lvl_MainGame.umap", backup / "Lvl_MainGame.umap")
            shutil.copy2(prior_path, backup / "authoring.json")
            report.update(backup=str(backup), previous_body_depth_cm=previous_depth, relocated_only=True)
            bird.transform = unreal.Transform(location=body, rotation=unreal.MathLibrary.make_rot_from_x(body_tangent), scale=bird.transform.scale3d)
            setp(field, "authored_placements", placements)
            assert levels.save_current_level()
    report.update(body=xyz(body), body_in_soil=inside(after, body), triangles=len(after),
                  clue_to_body_cm=clue.distance(body), route_end=xyz(point(route["arc"][-1])),
                  original_volume_cm3=volume(before), volume_cm3=volume(after))
    assert report["body_in_soil"]
    report["dig_path"] = [xyz(point(start_arc + distance / 100)) for distance in range(0, int(BODY_DEPTH) + 1, 50)]
    report["dig_path"][-1] = xyz(body)
    report["body_tangent"] = xyz(tangent(start_arc + BODY_DEPTH / 100))
    report["passed"] = True
except Exception:
    report["error"] = traceback.format_exc()
    unreal.log_error(report["error"])
finally:
    (OUT / "authoring.json").write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    unreal.SystemLibrary.quit_editor()
