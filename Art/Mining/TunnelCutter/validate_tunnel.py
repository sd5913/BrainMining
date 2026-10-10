"""Independent surface topology and triangle intersection checks in Blender."""
import bpy
import bmesh
import numpy as np
import json
from pathlib import Path
from mathutils.bvhtree import BVHTree

OUTPUT = Path('H:/PolyU/Spotlight/BrainMining/Art/Mining/TunnelCutter')


def topology(mesh):
    bm = bmesh.new()
    bm.from_mesh(mesh)
    bm.verts.ensure_lookup_table()
    seen = set()
    sizes = []
    for v in bm.verts:
        if v.index in seen:
            continue
        pending = [v]
        seen.add(v.index)
        count = 0
        while pending:
            current = pending.pop()
            count += 1
            for edge in current.link_edges:
                other = edge.other_vert(current)
                if other.index not in seen:
                    seen.add(other.index)
                    pending.append(other)
        sizes.append(count)
    faces = [tuple(sorted(v.index for v in f.verts)) for f in bm.faces]
    result = dict(vertices=len(bm.verts), edges=len(bm.edges), faces=len(bm.faces),
                  connected_components=len(sizes), boundary_edges=sum(e.is_boundary for e in bm.edges),
                  nonmanifold_edges=sum(not e.is_manifold for e in bm.edges),
                  inconsistent_winding_edges=sum(e.is_manifold and not e.is_contiguous for e in bm.edges),
                  degenerate_faces=sum(f.calc_area() < 1e-10 for f in bm.faces),
                  duplicate_faces=len(faces) - len(set(faces)),
                  euler_characteristic=len(bm.verts) - len(bm.edges) + len(bm.faces),
                  signed_volume_m3=bm.calc_volume(signed=True))
    bm.free()
    return result


def intersections(mesh):
    mesh.calc_loop_triangles()
    vertices = np.array([v.co[:] for v in mesh.vertices], dtype=float)
    triangles = np.array([t.vertices[:] for t in mesh.loop_triangles], dtype=np.int32)
    bvh = BVHTree.FromPolygons(vertices.tolist(), triangles.tolist(), all_triangles=True)
    candidates = []
    for a, b in bvh.overlap(bvh):
        if a < b and not set(triangles[a]).intersection(triangles[b]):
            candidates.append((a, b))
    epsilon = 1e-7
    crossing = []
    coplanar = []
    for offset in range(0, len(candidates), 20000):
        pairs = np.array(candidates[offset:offset + 20000])
        a = vertices[triangles[pairs[:, 0]]]
        b = vertices[triangles[pairs[:, 1]]]
        na = np.cross(a[:, 1] - a[:, 0], a[:, 2] - a[:, 0])
        nb = np.cross(b[:, 1] - b[:, 0], b[:, 2] - b[:, 0])
        na /= np.linalg.norm(na, axis=1)[:, None]
        nb /= np.linalg.norm(nb, axis=1)[:, None]
        da = np.einsum('nki,ni->nk', a - b[:, 0, None], nb)
        db = np.einsum('nki,ni->nk', b - a[:, 0, None], na)
        separated = ((da.min(axis=1) > epsilon) | (da.max(axis=1) < -epsilon) |
                     (db.min(axis=1) > epsilon) | (db.max(axis=1) < -epsilon))
        planar = ((np.abs(da).max(axis=1) < epsilon) & (np.abs(db).max(axis=1) < epsilon))
        coplanar.extend(pairs[planar].tolist())
        mask = ~separated & ~planar
        aa, bb, pp = a[mask], b[mask], pairs[mask]
        hits = np.zeros(len(pp), dtype=bool)
        for source, target in ((aa, bb), (bb, aa)):
            e1, e2 = target[:, 1] - target[:, 0], target[:, 2] - target[:, 0]
            for j in range(3):
                origin = source[:, j]
                direction = source[:, (j + 1) % 3] - origin
                h = np.cross(direction, e2)
                det = np.einsum('ni,ni->n', e1, h)
                valid = abs(det) > 1e-12
                inverse = np.zeros(len(det))
                inverse[valid] = 1 / det[valid]
                delta = origin - target[:, 0]
                u = inverse * np.einsum('ni,ni->n', delta, h)
                q = np.cross(delta, e1)
                v = inverse * np.einsum('ni,ni->n', direction, q)
                t = inverse * np.einsum('ni,ni->n', e2, q)
                hits |= valid & (u >= -epsilon) & (v >= -epsilon) & (u + v <= 1 + epsilon) & (t >= -epsilon) & (t <= 1 + epsilon)
        crossing.extend(pp[hits].tolist())
    # Coplanar pairs are unusual after voxel fusion. Check in their own 2-D plane.
    def orient(a, b, c):
        x, y = b - a, c - a
        return x[0] * y[1] - x[1] * y[0]

    def inside(p, tri):
        signs = [orient(tri[i], tri[(i + 1) % 3], p) for i in range(3)]
        return min(signs) >= -epsilon or max(signs) <= epsilon

    for ia, ib in coplanar:
        a, b = vertices[triangles[ia]], vertices[triangles[ib]]
        axis = np.argmax(abs(np.cross(a[1] - a[0], a[2] - a[0])))
        a, b = np.delete(a, axis, axis=1), np.delete(b, axis, axis=1)
        hit = any(inside(p, b) for p in a) or any(inside(p, a) for p in b)
        for i in range(3):
            for j in range(3):
                a1, a2, b1, b2 = a[i], a[(i + 1) % 3], b[j], b[(j + 1) % 3]
                if (orient(a1, a2, b1) * orient(a1, a2, b2) < 0 and
                        orient(b1, b2, a1) * orient(b1, b2, a2) < 0):
                    hit = True
        if hit:
            crossing.append([ia, ib])
    return dict(nonadjacent_bbox_pairs=len(candidates),
                self_intersection_pairs=len(crossing), examples=crossing[:10])


def validate(object_name='SM_DescendingTunnel_Cutter', report_name='validation.json'):
    obj = bpy.data.objects[object_name]
    report = topology(obj.data)
    report.update(intersections(obj.data))
    centers = json.loads((OUTPUT / 'centerlines.json').read_text(encoding='utf-8'))
    report['all_centerlines_strictly_descending'] = all(
        np.all(np.diff(np.array(p['points'])[:, 2]) < 0) for p in centers)
    report['branch_count'] = len(centers) - 1
    report['main_length_m'] = centers[0]['arc'][-1]
    report['main_vertical_drop_m'] = centers[0]['points'][0][2] - centers[0]['points'][-1][2]
    report['dimensions_m'] = list(obj.dimensions)
    report['passed'] = (report['connected_components'] == 1 and
                        report['euler_characteristic'] == 2 and
                        report['signed_volume_m3'] > 0 and
                        report['all_centerlines_strictly_descending'] and
                        all(report[k] == 0 for k in ('boundary_edges', 'nonmanifold_edges',
                                                    'inconsistent_winding_edges', 'degenerate_faces',
                                                    'duplicate_faces', 'self_intersection_pairs')))
    report['object_name'] = object_name
    (OUTPUT / report_name).write_text(json.dumps(report, indent=2), encoding='utf-8')
    print(json.dumps(report))
    assert report['passed'], report


if __name__ == '__main__':
    validate()
