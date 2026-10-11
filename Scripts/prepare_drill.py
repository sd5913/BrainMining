"""Blender background: extract the rigid FBX spin and prepare a camera-ready mesh.

Usage: blender -b -P Scripts/prepare_drill.py
Original FBX is retained; only SourceAssets/Drill generated files are written.
"""
import bpy
import json
import math
from pathlib import Path
from mathutils import Matrix, Vector

root = Path(__file__).resolve().parents[1]
output = root / 'SourceAssets/Drill'
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=str(output / 'drill.fbx'), use_image_search=False)
meshes = [obj for obj in bpy.context.scene.objects if obj.type == 'MESH']

def action_curves(action):
    return [curve for layer in action.layers for strip in layer.strips
            for bag in strip.channelbags for curve in bag.fcurves]

# The exported file also contains a previous pickaxe swing take. Select the
# authored drilling take whose Y angle advances by more than a full revolution.
candidates = []
for action in bpy.data.actions:
    curve = next((curve for curve in action_curves(action)
                  if curve.data_path == 'rotation_euler' and curve.array_index == 1), None)
    if curve and len(curve.keyframe_points) > 1:
        span = curve.keyframe_points[-1].co.y - curve.keyframe_points[0].co.y
        if span > math.tau:
            candidates.append((action, curve))
assert candidates, 'FBX has no continuous drill rotation take'
action, spin = sorted(candidates, key=lambda pair: pair[0].name)[0]
fps = bpy.context.scene.render.fps / bpy.context.scene.render.fps_base
first, last = action.frame_range
start_angle = spin.evaluate(first)
keys = [[(frame-first)/fps, math.degrees(spin.evaluate(frame)-start_angle)]
        for frame in range(int(first), int(last)+1)]

textures, materials = {}, []
for index, material in enumerate(sorted({slot.material for obj in meshes for slot in obj.material_slots}, key=lambda m:m.name)):
    bsdf = next(n for n in material.node_tree.nodes if n.type == 'BSDF_PRINCIPLED')
    settings = {'name': 'M_Drill' if index == 0 else 'M_Drill_Body',
                'source_name': material.name, 'textures': {}, 'mapping': {},
                'base_color': list(bsdf.inputs['Base Color'].default_value),
                'metallic': bsdf.inputs['Metallic'].default_value,
                'roughness': bsdf.inputs['Roughness'].default_value}
    for role, socket in [('BaseColor','Base Color'),('Normal','Normal'),('Roughness','Roughness'),('Metallic','Metallic')]:
        if not bsdf.inputs[socket].links: continue
        node = bsdf.inputs[socket].links[0].from_node
        if node.type == 'NORMAL_MAP': node = node.inputs['Color'].links[0].from_node
        assert node.type == 'TEX_IMAGE' and node.image and node.image.packed_file
        image = node.image
        name = 'T_Drill_' + str(index) + '_' + role
        filename = name + Path(image.filepath).suffix
        (output / filename).write_bytes(image.packed_file.data)
        textures[name] = {'role':role, 'file':filename}
        settings['textures'][role] = name
        vector = node.inputs['Vector']
        if vector.links and vector.links[0].from_node.type == 'MAPPING':
            mapping = vector.links[0].from_node
            settings['mapping'][role] = {'scale':list(mapping.inputs['Scale'].default_value), 'location':list(mapping.inputs['Location'].default_value)}
    materials.append(settings)

# Bake the source nonuniform scale into geometry while retaining its shaft pivot.
# Rotate the resulting upright shaft onto local +X in geometry. The FBX
# object's Euler-Y spin (after its -90 degree base tilt) then becomes local-X spin.
prepared_objects = []
for obj in meshes:
    alignment = Matrix.Rotation(math.pi/2, 4, 'Y') @ Matrix.Rotation(math.pi, 4, 'Z') @ Matrix.Diagonal(Vector((*obj.scale, 1)))
    prepared = bpy.data.objects.new(obj.name, obj.data.copy())
    prepared.data.transform(alignment)
    prepared.data.update()
    bpy.context.collection.objects.link(prepared)
    prepared_objects.append(prepared)
# The new export uses a smaller object scale; keep the original 58.6 cm presentation length.
xs = [v.co.x for obj in prepared_objects for v in obj.data.vertices]
normalization = .58603 / (max(xs)-min(xs))
for obj in prepared_objects: obj.data.transform(Matrix.Scale(normalization,4))
bpy.ops.object.select_all(action='DESELECT')
for obj in prepared_objects: obj.select_set(True)
bpy.context.view_layer.objects.active = prepared_objects[0]
bpy.ops.object.join()
prepared = bpy.context.view_layer.objects.active
prepared.name = 'SM_Drill'
slot_materials = [slot.material.name for slot in prepared.material_slots]
bpy.ops.export_scene.fbx(filepath=str(output / 'SM_Drill.fbx'), use_selection=True,
                         object_types={'MESH'}, axis_forward='-Y', axis_up='Z',
                         use_mesh_modifiers=True, bake_anim=False, add_leaf_bones=False,
                         path_mode='STRIP', mesh_smooth_type='FACE')

report = {'source': 'drill.fbx', 'source_action': action.name, 'fps': fps,
          'source_frame_range': [first, last], 'duration': (last-first)/fps,
          'rotation_degrees': keys, 'cycle_delta_degrees': keys[-1][1],
          'source_scale_baked': [list(obj.scale) for obj in meshes], 'size_normalization': normalization,
          'textures': textures, 'materials': materials, 'slot_materials':slot_materials,
          'animation_axis': 'Unreal local +X',
          'loop_method': 'Add each completed cycle angle to retain continuous rotation'}
(output / 'drill_animation.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
print('DRILL_PREPARED ' + json.dumps(report, ensure_ascii=False))
