"""Rock-facet variation of the watertight solid tunnel cutter in Blender."""
import bpy
import bmesh
import json
from pathlib import Path
from mathutils import Vector, noise

OUTPUT = Path('H:/PolyU/Spotlight/BrainMining/Art/Mining/TunnelCutter')


def enum_value(owner, prop, value):
    valid = [i.identifier for i in owner.bl_rna.properties[prop].enum_items]
    if value not in valid:
        raise ValueError((prop, value, valid))
    setattr(owner, prop, value)


def generate():
    source = bpy.data.objects['SM_DescendingTunnel_Cutter']
    scene = bpy.data.scenes.new('Descending_Tunnel_Lowpoly')
    scene.unit_settings.system = source.users_scene[0].unit_settings.system
    scene.unit_settings.scale_length = 1.0
    obj = bpy.data.objects.new('SM_DescendingTunnel_Cutter_Lowpoly', source.data.copy())
    obj.matrix_world = source.matrix_world.copy()
    scene.collection.objects.link(obj)
    bpy.context.window.scene = scene
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    obj.data.update()
    deviations = []
    normals = [v.normal.copy() for v in obj.data.vertices]
    for v, normal in zip(obj.data.vertices, normals):
        p = v.co.copy()
        # Broad rock undulations plus smaller chips, with continuous shared vertices.
        # Displacement changes the volume itself, so the boolean wall inherits it.
        broad = noise.noise(p / 3.8 + Vector((5.19, -8.73, 1.37)))
        detail = noise.noise(p / 1.35 + Vector((-6.42, 4.21, 9.56)))
        delta = .68 * broad + .20 * detail
        delta = max(-.55, min(.55, delta))
        v.co += normal * delta
        deviations.append(delta)
    obj.data.update()
    mod = obj.modifiers.new('Large_Rock_Facets', 'DECIMATE')
    enum_value(mod, 'decimate_type', 'COLLAPSE')
    mod.ratio = .058
    mod.use_collapse_triangulate = True
    bpy.ops.object.modifier_apply(modifier=mod.name)
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    bmesh.ops.recalc_face_normals(bm, faces=list(bm.faces))
    bm.to_mesh(obj.data)
    bm.free()
    obj.data.name = 'SM_DescendingTunnel_Cutter_Lowpoly_Mesh'
    obj.data.materials.clear()
    palette = [(.31,.265,.215), (.365,.31,.245), (.42,.355,.28),
               (.475,.40,.315), (.53,.445,.35)]
    for i, color in enumerate(palette):
        mat = bpy.data.materials.new(f'Rock_Facet_Preview_{i+1:02}')
        mat.diffuse_color = (*color, 1)
        mat.use_nodes = True
        node = next(n for n in mat.node_tree.nodes if n.type == 'BSDF_PRINCIPLED')
        node.inputs['Base Color'].default_value = (*color, 1)
        node.inputs['Roughness'].default_value = 1.0
        obj.data.materials.append(mat)
    for face in obj.data.polygons:
        face.use_smooth = False
        tone = noise.noise(face.center / 4.6 + Vector((2.4, -3.7, 5.3)))
        face.material_index = max(0, min(len(palette)-1, int((tone + 1) * 2.5)))
    for key in source.keys():
        obj[key] = source[key]
    obj['style'] = 'Irregular low-poly rock facets, flat face normals, geometric relief'
    obj['rock_displacement_limit_m'] = .55
    obj['original_smooth_mesh'] = source.name
    obj['generator'] = str(OUTPUT / 'stylize_lowpoly.py')
    bounds = [obj.matrix_world @ Vector(c) for c in obj.bound_box]
    center = sum(bounds, Vector()) / 8
    for area in bpy.context.screen.areas:
        if area.type == 'VIEW_3D':
            space = area.spaces.active
            enum_value(space.shading, 'type', 'SOLID')
            enum_value(space.shading, 'color_type', 'MATERIAL')
            enum_value(space.shading, 'light', 'STUDIO')
            space.shading.show_cavity = True
            enum_value(space.shading, 'cavity_type', 'BOTH')
            space.overlay.show_floor = False
            space.overlay.show_axis_x = False
            space.overlay.show_axis_y = False
            space.overlay.show_extras = False
            space.region_3d.view_location = center
            space.region_3d.view_rotation = Vector((1,-.4,.4)).to_track_quat('Z','Y')
            space.region_3d.view_distance = 185
            enum_value(space.region_3d, 'view_perspective', 'ORTHO')
    print(json.dumps({'object':obj.name,'vertices':len(obj.data.vertices),
                      'triangles':len(obj.data.polygons),
                      'displacement_min_m':min(deviations),'displacement_max_m':max(deviations),
                      'dimensions_m':list(obj.dimensions)}))


if __name__ == '__main__':
    generate()
