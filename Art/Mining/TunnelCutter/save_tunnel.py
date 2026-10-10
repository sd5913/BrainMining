"""Export the finished single-object cutter and render an isolated preview."""
import bpy
import json
from pathlib import Path
from mathutils import Vector

OUTPUT = Path('H:/PolyU/Spotlight/BrainMining/Art/Mining/TunnelCutter')
scene = bpy.context.scene
obj = bpy.data.objects['SM_DescendingTunnel_Cutter']
obj.data.name = 'SM_DescendingTunnel_Cutter_Mesh'
material = obj.data.materials[0]
material.diffuse_color = (.32, .65, .72, 1)
principled = next(n for n in material.node_tree.nodes if n.type == 'BSDF_PRINCIPLED')
principled.inputs['Base Color'].default_value = (.32, .65, .72, 1)

bpy.ops.object.select_all(action='DESELECT')
obj.select_set(True)
bpy.context.view_layer.objects.active = obj
bounds = [obj.matrix_world @ Vector(c) for c in obj.bound_box]
center = sum(bounds, Vector()) / 8
direction = Vector((1, -.4, .4)).normalized()
rotation = direction.to_track_quat('Z', 'Y')
for area in bpy.context.screen.areas:
    if area.type == 'VIEW_3D':
        space = area.spaces.active
        space.region_3d.view_rotation = rotation
        space.region_3d.view_location = center
        space.region_3d.view_distance = 185
        space.region_3d.view_perspective = 'ORTHO'
        space.shading.show_cavity = True
        space.shading.cavity_type = 'BOTH'
        space.overlay.show_extras = False

bpy.ops.export_scene.fbx(
    filepath=str(OUTPUT / 'SM_DescendingTunnel_Cutter.fbx'),
    use_selection=True, object_types={'MESH'},
    global_scale=1.0, apply_unit_scale=True, apply_scale_options='FBX_SCALE_UNITS',
    axis_forward='-Y', axis_up='Z',
    use_mesh_modifiers=True, mesh_smooth_type='FACE',
    use_custom_props=True, bake_anim=False)

# A temporary Workbench camera adds no geometry to the deliverable.
camera_data = bpy.data.cameras.new('Temporary_Preview_Camera')
camera = bpy.data.objects.new('Temporary_Preview_Camera', camera_data)
scene.collection.objects.link(camera)
camera.rotation_euler = rotation.to_euler()
camera.location = center + direction * 400
camera_data.type = 'ORTHO'
camera_data.clip_end = 1000
local_bounds = [rotation.inverted() @ (p - center) for p in bounds]
aspect = 1.25
width = max(p.x for p in local_bounds) - min(p.x for p in local_bounds)
height = max(p.y for p in local_bounds) - min(p.y for p in local_bounds)
camera_data.ortho_scale = max(width, height * aspect) * 1.13
old_camera = scene.camera
old_engine = scene.render.engine
try:
    scene.camera = camera
    scene.render.engine = 'BLENDER_WORKBENCH'
    scene.render.resolution_x = 1600
    scene.render.resolution_y = 1280
    scene.render.resolution_percentage = 100
    scene.render.film_transparent = False
    scene.render.image_settings.file_format = 'PNG'
    scene.render.filepath = str(OUTPUT / 'Tunnel_Cutter_Preview.png')
    shading = scene.display.shading
    shading.light = 'STUDIO'
    shading.color_type = 'MATERIAL'
    shading.show_shadows = True
    shading.show_cavity = True
    shading.cavity_type = 'BOTH'
    shading.curvature_ridge_factor = 1.25
    shading.curvature_valley_factor = 1.1
    shading.background_type = 'WORLD'
    scene.world = bpy.data.worlds.new('Tunnel_Preview_Background')
    scene.world.color = (.035, .045, .060)
    bpy.ops.render.render(write_still=True)
finally:
    scene.camera = old_camera
    scene.render.engine = old_engine
    bpy.data.objects.remove(camera, do_unlink=True)
    bpy.data.cameras.remove(camera_data)

for mesh in list(bpy.data.meshes):
    if mesh.users == 0 and (mesh.name.endswith('_temporary') or mesh.name.startswith('Temporary_Boolean_Verification')):
        bpy.data.meshes.remove(mesh)
bpy.ops.wm.save_as_mainfile(filepath=str(OUTPUT / 'Descending_Tunnel_Cutter.blend'))
print(json.dumps({'saved_blend': str(OUTPUT / 'Descending_Tunnel_Cutter.blend'),
                  'saved_fbx': str(OUTPUT / 'SM_DescendingTunnel_Cutter.fbx'),
                  'preview': str(OUTPUT / 'Tunnel_Cutter_Preview.png'),
                  'visible_scene_objects': [o.name for o in scene.objects]}, ensure_ascii=False))
