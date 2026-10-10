"""Generate a descending, watertight tunnel boolean cutter in live Blender.

Run with Blender's Python API. Distances are metres. No third-party assets.
"""
import bpy
import bmesh
import json
import math
from pathlib import Path

import numpy as np
from mathutils import Vector

OUTPUT = Path('H:/PolyU/Spotlight/BrainMining/Art/Mining/TunnelCutter')
OUTPUT.mkdir(parents=True, exist_ok=True)


def main_path():
    xy = np.array([
        [-60, -8], [-47, 5], [-30, 22], [-40, 40], [-25, 57],
        [0, 49], [16, 30], [5, 8], [22, -13], [49, -4],
        [38, 18], [63, 35]], dtype=float)
    padded = np.vstack([2 * xy[0] - xy[1], xy, 2 * xy[-1] - xy[-2]])
    samples = []
    for k in range(len(xy) - 1):
        a, b, c, d = padded[k:k + 4]
        # Cardinal interpolation for XY; linear Z prevents spline overshoot.
        for t in np.linspace(0, 1, 120, endpoint=False):
            p = .5 * ((2 * b) + (-a + c) * t +
                      (2 * a - 5 * b + 4 * c - d) * t * t +
                      (-a + 3 * b - 3 * c + d) * t ** 3)
            samples.append([p[0], p[1], 40 - 88 * (k + t) / (len(xy) - 1)])
    samples.append([*xy[-1], -48])
    dense = np.array(samples)
    arc = np.r_[0, np.cumsum(np.linalg.norm(np.diff(dense, axis=0), axis=1))]
    s = np.linspace(0, arc[-1], math.ceil(arc[-1] / .28) + 1)
    p = np.column_stack([np.interp(s, arc, dense[:, j]) for j in range(3)])
    u = s / s[-1]
    r = 2.55 + .22 * np.sin(2 * np.pi * u * 2.3) + .10 * np.sin(2 * np.pi * u * 4.2 + .4)
    return p, r, s


def distance_matrix(a, b):
    return np.linalg.norm(a[:, None, :] - b[None, :, :], axis=2)


def branch_path(root, tangent, length, angle, bend, drop):
    forward = tangent[:2] / np.linalg.norm(tangent[:2])
    side = np.array([-forward[1], forward[0]])
    direction = math.cos(angle) * forward + math.sin(angle) * side
    cross = np.array([-direction[1], direction[0]])
    controls = np.array([
        root,
        root + np.r_[direction * (length * .30), -drop * .26],
        root + np.r_[direction * (length * .73) + cross * bend, -drop * .67],
        root + np.r_[direction * length + cross * bend * .5, -drop],
    ])
    t = np.linspace(0, 1, 220)
    p = ((1 - t)[:, None] ** 3 * controls[0] +
         3 * ((1 - t) ** 2 * t)[:, None] * controls[1] +
         3 * ((1 - t) * t ** 2)[:, None] * controls[2] +
         t[:, None] ** 3 * controls[3])
    arc = np.r_[0, np.cumsum(np.linalg.norm(np.diff(p, axis=0), axis=1))]
    s = np.linspace(0, arc[-1], math.ceil(arc[-1] / .28) + 1)
    p = np.column_stack([np.interp(s, arc, p[:, j]) for j in range(3)])
    u = s / s[-1]
    # Same order of diameter as the trunk, gradually narrowing toward the cap.
    r = 2.38 + .16 * np.sin(np.pi * u * 1.4) - .38 * u ** 2
    return p, r, s, controls


def plan():
    p, r, s = main_path()
    paths = [dict(name='Main', points=p, radii=r, arc=s)]
    minima = []
    for n, fraction in enumerate([.115, .24, .365, .49, .615, .74, .875]):
        i = int(np.argmin(abs(s / s[-1] - fraction)))
        tangent = p[min(i + 2, len(p) - 1)] - p[max(i - 2, 0)]
        length = [32, 40, 28, 38, 33, 43, 30][n]
        drop = [15, 21, 13, 19, 17, 23, 16][n]
        candidates = []
        for sign in (-1, 1):
            for degrees in (60, 75, 90, 105, 120):
                for bend in (-9, 0, 9):
                    bp, br, bs, controls = branch_path(p[i], tangent, length,
                                                       math.radians(degrees * sign), bend, drop)
                    # Junction overlap is intentional only within 9 m of the root.
                    far_main = abs(s - s[i]) > 9
                    clearance_a = np.min(distance_matrix(bp[::3], p[far_main][::3]) -
                                         br[::3, None] - r[far_main][None, ::3])
                    detached = bs > 9
                    clearance_b = np.min(distance_matrix(bp[detached][::3], p[::3]) -
                                         br[detached][::3, None] - r[None, ::3])
                    gap = min(clearance_a, clearance_b)
                    for old in paths[1:]:
                        gap = min(gap, np.min(distance_matrix(bp[::3], old['points'][::3]) -
                                             br[::3, None] - old['radii'][None, ::3]))
                    # Prefer a well-separated branch over the first plausible side.
                    score = gap + .004 * np.linalg.norm(bp[-1, :2])
                    candidates.append((score, gap, bp, br, bs, controls))
        chosen = max(candidates, key=lambda x: x[0])
        _, gap, bp, br, bs, controls = chosen
        if gap < 1.2:
            raise RuntimeError(f'Branch {n + 1} insufficient separation: {gap:.3f} m')
        paths.append(dict(name=f'Branch_{n + 1:02}', points=bp, radii=br,
                          arc=bs, controls=controls, root_arc=float(s[i])))
        minima.append(float(gap))
    for path in paths:
        assert np.all(np.diff(path['points'][:, 2]) < 0), path['name']
        # Nonlocal tube portions must not fold back into themselves.
        dist = distance_matrix(path['points'][::3], path['points'][::3])
        arcd = abs(path['arc'][::3, None] - path['arc'][None, ::3])
        mask = arcd > 11
        rr = path['radii'][::3, None] + path['radii'][None, ::3]
        assert np.min((dist - rr)[mask]) > 1.2, path['name']
    return paths, minima


def tube_mesh(path):
    pts = path['points']
    radii = path['radii']
    tangents = np.gradient(pts, axis=0)
    tangents /= np.linalg.norm(tangents, axis=1)[:, None]
    rings = []
    # Parallel-transport frames keep circular sections stable through bends.
    t0 = Vector(tangents[0])
    axis = Vector((0, 0, 1))
    normal = t0.cross(axis).normalized()
    frames = []
    previous = t0
    for tangent in tangents:
        current = Vector(tangent)
        normal = previous.rotation_difference(current) @ normal
        normal = (normal - current * normal.dot(current)).normalized()
        frames.append((normal.copy(), current.cross(normal).normalized()))
        previous = current
    sides = 64

    def ring(center, radius, frame):
        radius = float(radius)
        normal, binormal = frame
        return [tuple(Vector(center) + radius *
                      (math.cos(2 * math.pi * j / sides) * normal +
                       math.sin(2 * math.pi * j / sides) * binormal)) for j in range(sides)]

    # Rounded solid caps, avoiding a flat abrupt plug at any dead end.
    for theta in np.linspace(-math.pi / 2, 0, 13)[1:]:
        center = pts[0] + tangents[0] * radii[0] * math.sin(theta)
        rings.append(ring(center, radii[0] * math.cos(theta), frames[0]))
    for index in range(1, len(pts)):
        rings.append(ring(pts[index], radii[index], frames[index]))
    for theta in np.linspace(0, math.pi / 2, 13)[1:-1]:
        center = pts[-1] + tangents[-1] * radii[-1] * math.sin(theta)
        rings.append(ring(center, radii[-1] * math.cos(theta), frames[-1]))
    vertices = [v for r in rings for v in r]
    faces = []
    for k in range(len(rings) - 1):
        for j in range(sides):
            a, b = k * sides + j, k * sides + (j + 1) % sides
            faces.append((a, b, b + sides, a + sides))
    vertices.append(tuple(pts[0] - tangents[0] * radii[0]))
    start = len(vertices) - 1
    vertices.append(tuple(pts[-1] + tangents[-1] * radii[-1]))
    end = len(vertices) - 1
    for j in range(sides):
        faces.append((start, (j + 1) % sides, j))
        offset = (len(rings) - 1) * sides
        faces.append((end, offset + j, offset + (j + 1) % sides))
    return vertices, faces


def generate():
    paths, clearances = plan()
    serial = []
    for path in paths:
        item = {k: (v.tolist() if isinstance(v, np.ndarray) else v) for k, v in path.items()}
        serial.append(item)
    (OUTPUT / 'centerlines.json').write_text(json.dumps(serial, indent=2), encoding='utf-8')
    scene = bpy.data.scenes.new('Descending_Tunnel_Cutter')
    bpy.context.window.scene = scene
    scene.unit_settings.system = 'METRIC'
    scene.unit_settings.scale_length = 1.0
    objects = []
    for path in paths:
        vertices, faces = tube_mesh(path)
        mesh = bpy.data.meshes.new(path['name'] + '_temporary')
        mesh.from_pydata(vertices, [], faces)
        mesh.update()
        bm = bmesh.new()
        bm.from_mesh(mesh)
        bmesh.ops.recalc_face_normals(bm, faces=list(bm.faces))
        bm.to_mesh(mesh)
        bm.free()
        obj = bpy.data.objects.new(path['name'], mesh)
        scene.collection.objects.link(obj)
        objects.append(obj)
    bpy.ops.object.select_all(action='DESELECT')
    for obj in objects:
        obj.select_set(True)
    bpy.context.view_layer.objects.active = objects[0]
    bpy.ops.object.join()
    obj = bpy.context.object
    obj.name = 'SM_DescendingTunnel_Cutter'
    remesh = obj.modifiers.new('FuseBranches_Watertight', 'REMESH')
    remesh.mode = 'VOXEL'
    remesh.voxel_size = .16
    remesh.use_smooth_shade = True
    bpy.ops.object.modifier_apply(modifier=remesh.name)
    smooth = obj.modifiers.new('SmoothJunctions', 'SMOOTH')
    smooth.factor = .85
    smooth.iterations = 14
    bpy.ops.object.modifier_apply(modifier=smooth.name)
    optimize = obj.modifiers.new('BooleanMesh_Optimize', 'DECIMATE')
    optimize.ratio = .16
    optimize.use_collapse_triangulate = True
    bpy.ops.object.modifier_apply(modifier=optimize.name)
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    bmesh.ops.recalc_face_normals(bm, faces=list(bm.faces))
    bm.to_mesh(obj.data)
    bm.free()
    for polygon in obj.data.polygons:
        polygon.use_smooth = True
    mat = bpy.data.materials.new('Tunnel_Cutter_Clay')
    mat.diffuse_color = (.20, .46, .56, 1)
    mat.use_nodes = True
    principled = next(n for n in mat.node_tree.nodes if n.type == 'BSDF_PRINCIPLED')
    principled.inputs['Base Color'].default_value = (.12, .40, .49, 1)
    principled.inputs['Roughness'].default_value = .46
    obj.data.materials.append(mat)
    obj['purpose'] = 'Solid, closed negative volume for tunnel boolean subtraction'
    obj['units'] = 'metres; FBX exports unit metadata for Unreal centimetres'
    obj['branch_count'] = 7
    obj['centerlines_strictly_descending'] = True
    obj['remesh_voxel_metres'] = .16
    obj['sampled_minimum_branch_clearance_metres'] = min(clearances)
    obj['generator'] = str(OUTPUT / 'generate_tunnel.py')
    # A single visible mesh: no wall shells, rock, rooms, blockers or render props.
    for area in bpy.context.screen.areas:
        if area.type == 'VIEW_3D':
            space = area.spaces.active
            space.shading.type = 'SOLID'
            space.shading.color_type = 'MATERIAL'
            space.shading.light = 'STUDIO'
            space.shading.show_shadows = True
            space.overlay.show_floor = False
            space.overlay.show_axis_x = False
            space.overlay.show_axis_y = False
            space.region_3d.view_perspective = 'ORTHO'
            space.region_3d.view_location = obj.location
    print(json.dumps({'object': obj.name, 'branches': 7,
                      'vertices': len(obj.data.vertices), 'faces': len(obj.data.polygons),
                      'dimensions_m': list(obj.dimensions),
                      'min_sampled_clearance_m': min(clearances),
                      'scene': scene.name}, ensure_ascii=False))


if __name__ == '__main__':
    generate()
