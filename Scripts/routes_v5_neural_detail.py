"""Export the existing neural generator's fine branches before cave simplification."""
from pathlib import Path

def export_detail(objects, out):
    from mathutils import Vector
    import bmesh
    records = []
    for number, ob in enumerate(objects):
        bm = bmesh.new()
        bm.from_mesh(ob.data)
        bmesh.ops.triangulate(bm, faces=list(bm.faces))
        assert all(e.is_manifold for e in bm.edges), ob.name
        bm.verts.ensure_lookup_table()
        bm.verts.index_update()
        # A small outward offset avoids coplanar faces with the coarse cave core.
        points = [ob.matrix_world @ v.co for v in bm.verts]
        center = sum(points, Vector()) / len(points)
        points = [center + (p-center)*1.035 + Vector((397.5,-117.5,139)) for p in points]
        target = Path(out) / f'NeuralDetail_{number}.obj'
        lines = ['# Existing neural scenery, world Blender metres']
        lines += ['v %.8f %.8f %.8f' % tuple(p) for p in points]
        lines += ['f %d %d %d' % tuple(v.index+1 for v in f.verts) for f in bm.faces]
        target.write_text('\n'.join(lines)+'\n', encoding='utf-8')
        records.append({'file':str(target),'triangles':len(bm.faces)})
        bm.free()
    return records

if __name__ == '__main__':
    import bpy, json
    root = Path(__file__).resolve().parents[1]
    folder = root / 'Art/MainGame/RoutesV5/TunnelStalagmiteV3'
    bpy.ops.wm.open_mainfile(filepath=str(folder/'TunnelStalagmiteV3_RoutesV5.blend'))
    objects = sorted([o for o in bpy.data.objects if o.name.startswith('REFERENCE_Scenery_1_')], key=lambda o:o.name)
    assert len(objects)==2, [o.name for o in bpy.data.objects if o.name.startswith('REFERENCE')]
    print(json.dumps(export_detail(objects, folder)))
