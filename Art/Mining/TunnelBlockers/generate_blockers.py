"""Create twenty full-section, tunnel-fitted Lowpoly rock plugs in Blender.

The tunnel remains a separate reference; only the plugs are exported to FBX.
"""
import bpy
import bmesh
import json
import math
import random
import runpy
from pathlib import Path

import numpy as np
from mathutils import Vector, noise
from mathutils.bvhtree import BVHTree

OUTPUT = Path('H:/PolyU/Spotlight/BrainMining/Art/Mining/TunnelBlockers')
SOURCE = Path('H:/PolyU/Spotlight/BrainMining/Art/Mining/TunnelCutter')
SEED = 20261010
TUNNEL = 'SM_DescendingTunnel_Cutter_Lowpoly'
SCENE = 'Tunnel_Blockers_20'
REFERENCE = 'REFERENCE_Tunnel_Fit_Only'
COUNTS = [10, 1, 2, 1, 2, 1, 2, 1]


def enum_value(owner, prop, value):
    choices = [item.identifier for item in owner.bl_rna.properties[prop].enum_items]
    assert value in choices, (prop, value, choices)
    setattr(owner, prop, value)


def frame(path, arc_position):
    s = np.array(path['arc'])
    p = np.array(path['points'])
    center = Vector([float(np.interp(arc_position, s, p[:, i])) for i in range(3)])
    before = Vector([float(np.interp(arc_position-.6, s, p[:, i])) for i in range(3)])
    after = Vector([float(np.interp(arc_position+.6, s, p[:, i])) for i in range(3)])
    tangent = (after-before).normalized()
    u = tangent.cross(Vector((0,0,1))).normalized()
    v = tangent.cross(u).normalized()
    return center, u, v, tangent


def placement_plan():
    paths = json.loads((SOURCE/'centerlines.json').read_text(encoding='utf-8'))
    rng = random.Random(SEED)
    plan = []
    roots = [path['root_arc'] for path in paths[1:]]
    for index, (path, count) in enumerate(zip(paths,COUNTS)):
        selected = []
        for attempt in range(12000):
            position = rng.uniform(9, path['arc'][-1]-7)
            if index == 0 and any(abs(position-root)<10 for root in roots):
                continue
            if any(abs(position-old)<10 for old in selected):
                continue
            center, _, _, _ = frame(path,position)
            if any((center-Vector(old['center_m'])).length<8.8 for old in plan):
                continue
            selected.append(position)
            plan.append(dict(path_index=index,path_name=path['name'],arc_position_m=position,
                             center_m=list(center),half_thickness_m=rng.uniform(1.05,1.7),
                             relief_seed=rng.randint(1,1000000)))
            if len(selected)==count:
                break
        if len(selected)!=count:
            raise RuntimeError(f'Cannot place {count} separated blockers on {path["name"]}')
    plan.sort(key=lambda p:(p['path_index'],p['arc_position_m']))
    for number, record in enumerate(plan,1):
        record['name']=f'SM_TunnelBlocker_{number:02}'
    return paths, plan


def initialize():
    OUTPUT.mkdir(parents=True,exist_ok=True)
    paths, plan = placement_plan()
    assert len(plan)==20
    (OUTPUT/'placement_plan.json').write_text(json.dumps({'seed':SEED,'objects':plan},indent=2),encoding='utf-8')
    assert bpy.data.scenes.get(SCENE) is None, 'Existing blocker scene must be preserved.'
    source=bpy.data.objects[TUNNEL]
    scene=bpy.data.scenes.new(SCENE)
    enum_value(scene.unit_settings,'system','METRIC')
    scene.unit_settings.scale_length=1
    bpy.context.window.scene=scene
    guide=bpy.data.objects.new(REFERENCE,source.data)
    guide.matrix_world=source.matrix_world.copy()
    scene.collection.objects.link(guide)
    guide.hide_render=True
    enum_value(guide,'display_type','WIRE')
    guide.hide_select=True
    for area in bpy.context.screen.areas:
        if area.type=='VIEW_3D':
            space=area.spaces.active
            enum_value(space.shading,'type','SOLID')
            enum_value(space.shading,'color_type','MATERIAL')
            space.overlay.show_floor=False
            space.overlay.show_axis_x=False
            space.overlay.show_axis_y=False
            space.overlay.show_extras=False
    print(json.dumps({'scene':scene.name,'planned_objects':20,'per_path':COUNTS,'seed':SEED}))


def rough_slab(record,path):
    center,u,v,tangent=frame(path,record['arc_position_m'])
    rng=random.Random(record['relief_seed'])
    grid=12
    span=7
    step=2*span/grid
    offset=Vector((rng.uniform(-30,30),rng.uniform(-30,30),rng.uniform(-30,30)))
    coordinates=[]
    for j in range(grid+1):
        for i in range(grid+1):
            x,y=-span+i*step,-span+j*step
            if 0<i<grid and 0<j<grid:
                x+=rng.uniform(-.22,.22)*step
                y+=rng.uniform(-.22,.22)*step
            coordinates.append((x,y))
    vertices=[]
    ring=(grid+1)**2
    for cap in (-1,1):
        for x,y in coordinates:
            p=Vector((x,y,cap*5))+offset
            relief=.42*noise.noise(p/1.6)+.16*noise.noise(p/.7)
            relief=max(-.55,min(.55,relief))
            z=cap*record['half_thickness_m']+relief
            vertices.append(tuple(center+x*u+y*v+z*tangent))
    faces=[]
    for cap in range(2):
        for j in range(grid):
            for i in range(grid):
                a=cap*ring+j*(grid+1)+i
                b,c,d=a+1,a+grid+2,a+grid+1
                if (i+j)%2:
                    faces.extend([(a,b,d),(b,c,d)])
                else:
                    faces.extend([(a,b,c),(a,c,d)])
    perimeter=(list(range(grid+1))+[j*(grid+1)+grid for j in range(1,grid+1)]+
               [grid*(grid+1)+i for i in range(grid-1,-1,-1)]+
               [j*(grid+1) for j in range(grid-1,0,-1)])
    for i,a in enumerate(perimeter):
        b=perimeter[(i+1)%len(perimeter)]
        faces.append((a,b,b+ring,a+ring))
    mesh=bpy.data.meshes.new(record['name']+'_Mesh')
    mesh.from_pydata(vertices,[],faces)
    mesh.update()
    bm=bmesh.new()
    bm.from_mesh(mesh)
    bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces))
    bm.to_mesh(mesh)
    bm.free()
    obj=bpy.data.objects.new(record['name'],mesh)
    bpy.context.scene.collection.objects.link(obj)
    return obj,center,u,v,tangent


def materials():
    palette=[(.31,.155,.075),(.39,.205,.095),(.47,.26,.13),(.55,.32,.17),(.61,.39,.23)]
    result=[]
    for i,color in enumerate(palette):
        name=f'Tunnel_Blocker_Rock_{i+1:02}'
        mat=bpy.data.materials.get(name)
        if mat is None:
            mat=bpy.data.materials.new(name)
            mat.diffuse_color=(*color,1)
            mat.use_nodes=True
            node=next(n for n in mat.node_tree.nodes if n.type=='BSDF_PRINCIPLED')
            node.inputs['Base Color'].default_value=(*color,1)
            node.inputs['Roughness'].default_value=1
        result.append(mat)
    return result


def generate_range(start,end):
    bpy.context.window.scene=bpy.data.scenes[SCENE]
    paths=json.loads((SOURCE/'centerlines.json').read_text(encoding='utf-8'))
    records=json.loads((OUTPUT/'placement_plan.json').read_text(encoding='utf-8'))['objects']
    cutter=bpy.data.objects[REFERENCE]
    rock_materials=materials()
    reports=[]
    for record in records[start:end]:
        assert bpy.data.objects.get(record['name']) is None, record['name']
        obj,center,u,v,tangent=rough_slab(record,paths[record['path_index']])
        bpy.ops.object.select_all(action='DESELECT')
        obj.select_set(True)
        bpy.context.view_layer.objects.active=obj
        modifier=obj.modifiers.new('Exact_Tunnel_Interior_Fit','BOOLEAN')
        enum_value(modifier,'operation','INTERSECT')
        enum_value(modifier,'solver','EXACT')
        modifier.object=cutter
        bpy.ops.object.modifier_apply(modifier=modifier.name)
        bm=bmesh.new()
        bm.from_mesh(obj.data)
        bmesh.ops.triangulate(bm,faces=list(bm.faces))
        bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces))
        bm.to_mesh(obj.data)
        bm.free()
        obj.data.materials.clear()
        for mat in rock_materials:
            obj.data.materials.append(mat)
        for polygon in obj.data.polygons:
            polygon.use_smooth=False
            tone=noise.noise(polygon.center/2.3+Vector((3.5,-1.2,7.9)))
            polygon.material_index=max(0,min(4,int((tone+1)*2.5)))
        obj['purpose']='Full-section diggable rock blockage; sides match tunnel interior'
        obj['path_name']=record['path_name']
        obj['path_arc_position_m']=record['arc_position_m']
        obj['center_m']=record['center_m']
        obj['random_seed']=record['relief_seed']
        obj['nominal_thickness_m']=2*record['half_thickness_m']
        obj['shared_origin_with_tunnel']=True
        reports.append({'name':obj.name,'triangles':len(obj.data.polygons),'dimensions_m':list(obj.dimensions)})
    print(json.dumps({'generated':reports}))


def blockers():
    return [bpy.data.objects[f'SM_TunnelBlocker_{i:02}'] for i in range(1,21)]


def mesh_bvh(obj):
    return BVHTree.FromPolygons([obj.matrix_world@v.co for v in obj.data.vertices],
                               [tuple(p.vertices) for p in obj.data.polygons],all_triangles=True)


def validate():
    validation=runpy.run_path(str(SOURCE/'validate_tunnel.py'),run_name='blocker_validation')
    paths=json.loads((SOURCE/'centerlines.json').read_text(encoding='utf-8'))
    records=json.loads((OUTPUT/'placement_plan.json').read_text(encoding='utf-8'))['objects']
    tunnel=bpy.data.objects[REFERENCE]
    original_bvh=mesh_bvh(tunnel)
    reports=[]
    for obj,record in zip(blockers(),records):
        report=validation['topology'](obj.data)
        report.update(validation['intersections'](obj.data))
        center,u,v,tangent=frame(paths[record['path_index']],record['arc_position_m'])
        bvh=mesh_bvh(obj)
        errors=[]
        distances=[]
        for i in range(120):
            angle=2*math.pi*i/120
            ray=math.cos(angle)*u+math.sin(angle)*v
            original=original_bvh.ray_cast(center,ray,7)
            plug=bvh.ray_cast(center,ray,7)
            assert original[0] is not None and plug[0] is not None, obj.name
            errors.append(abs(original[3]-plug[3]))
            distances.append(original[3])
        report['max_cross_section_fit_error_m']=max(errors)
        report['min_center_section_radius_m']=min(distances)
        report['centerline_blocked']=bvh.ray_cast(center-tangent*5,tangent,10)[0] is not None
        report['name']=obj.name
        report['path_name']=record['path_name']
        report['passed']=(report['connected_components']==1 and report['euler_characteristic']==2 and
                          report['signed_volume_m3']>0 and report['max_cross_section_fit_error_m']<.0002 and
                          report['centerline_blocked'] and all(report[k]==0 for k in
                            ('nonmanifold_edges','boundary_edges','inconsistent_winding_edges','degenerate_faces',
                             'duplicate_faces','self_intersection_pairs')))
        reports.append(report)
        assert report['passed'],report
    def verification_mesh(source,record):
        mesh=source.data.copy()
        center=Vector(record['center_m'])
        # Exact contact is deliberately preserved in the deliverable. For this
        # auxiliary boolean only, extend each plug by 2% to avoid coplanar
        # coincidence artifacts in the difference solver.
        for vertex in mesh.vertices:
            vertex.co=center+(vertex.co-center)*1.02
        mesh.update()
        return mesh
    combined=bpy.data.objects.new('Temporary_Combined_Blockers',verification_mesh(blockers()[0],records[0]))
    bpy.context.scene.collection.objects.link(combined)
    # Joining temporary copies preserves twenty individually exportable objects.
    copies=[combined]
    for source,record in zip(blockers()[1:],records[1:]):
        temp=bpy.data.objects.new('Temporary_Blocker_Copy',verification_mesh(source,record))
        bpy.context.scene.collection.objects.link(temp)
        copies.append(temp)
    bpy.ops.object.select_all(action='DESELECT')
    for copy in copies:copy.select_set(True)
    bpy.context.view_layer.objects.active=combined
    bpy.ops.object.join()
    residual=bpy.data.objects.new('Temporary_Unblocked_Tunnel',tunnel.data.copy())
    residual.matrix_world=tunnel.matrix_world.copy()
    bpy.context.scene.collection.objects.link(residual)
    try:
        bpy.ops.object.select_all(action='DESELECT')
        residual.select_set(True)
        bpy.context.view_layer.objects.active=residual
        modifier=residual.modifiers.new('Verify_All_Twenty_Seals','BOOLEAN')
        enum_value(modifier,'operation','DIFFERENCE')
        enum_value(modifier,'solver','EXACT')
        modifier.object=combined
        bpy.ops.object.modifier_apply(modifier=modifier.name)
        remaining=validation['topology'](residual.data)
        # Cutting 20 separate interior edges of a tree must create 21 regions.
        assert remaining['connected_components']==21,remaining
        assert remaining['nonmanifold_edges']==0 and remaining['boundary_edges']==0,remaining
    finally:
        for obj in (combined,residual):
            mesh=obj.data
            bpy.data.objects.remove(obj,do_unlink=True)
            if mesh.users==0:bpy.data.meshes.remove(mesh)
        for mesh in list(bpy.data.meshes):
            if mesh.users==0 and mesh.name.startswith('SM_TunnelBlocker_'):
                bpy.data.meshes.remove(mesh)
    report=dict(seed=SEED,object_count=len(reports),total_triangles=sum(r['faces'] for r in reports),
                all_passed=all(r['passed'] for r in reports),blockers=reports,
                blocked_tunnel_connected_components=remaining['connected_components'],
                boolean_test_only_plug_scale=1.02,
                max_fit_error_m=max(r['max_cross_section_fit_error_m'] for r in reports))
    (OUTPUT/'validation.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    print(json.dumps({k:v for k,v in report.items() if k!='blockers'}))
    return report


if __name__=='__main__':
    initialize()
    generate_range(0,20)
    validate()
