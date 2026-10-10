"""Export the twenty plugs, save the editable scene, and render previews."""
import bpy
import json
import runpy
from pathlib import Path
from mathutils import Vector

OUTPUT = Path('H:/PolyU/Spotlight/BrainMining/Art/Mining/TunnelBlockers')
ns = runpy.run_path(str(OUTPUT/'generate_blockers.py'), run_name='blocker_helpers')
scene = bpy.context.scene
assert scene.name == ns['SCENE']
objects = ns['blockers']()
assert len(objects) == 20 and all(obj.type == 'MESH' for obj in objects)
guide = bpy.data.objects[ns['REFERENCE']]

def select_blockers():
    bpy.ops.object.select_all(action='DESELECT')
    for obj in objects:
        obj.select_set(True)
    bpy.context.view_layer.objects.active = objects[0]

select_blockers()
bpy.ops.export_scene.fbx(
    filepath=str(OUTPUT/'Tunnel_Blockers_20.fbx'),
    use_selection=True, object_types={'MESH'}, global_scale=1.0,
    apply_unit_scale=True, apply_scale_options='FBX_SCALE_UNITS',
    axis_forward='-Y', axis_up='Z', use_mesh_modifiers=True,
    mesh_smooth_type='FACE', use_custom_props=True, bake_anim=False)
bpy.ops.wm.save_as_mainfile(filepath=str(OUTPUT/'Tunnel_Blockers_20.blend'))

bounds = [guide.matrix_world @ Vector(c) for c in guide.bound_box]
center = sum(bounds, Vector())/8
direction = Vector((1,-.4,.4)).normalized()
rotation = direction.to_track_quat('Z','Y')
cam_data = bpy.data.cameras.new('Temporary_Blocker_Preview_Camera')
cam = bpy.data.objects.new(cam_data.name,cam_data)
scene.collection.objects.link(cam)
ns['enum_value'](cam_data,'type','ORTHO')
cam_data.clip_end = 1000
cam_data.clip_start = .1
wire = bpy.data.objects.new('Temporary_Tunnel_Wire_Preview',guide.data.copy())
wire.matrix_world = guide.matrix_world.copy()
scene.collection.objects.link(wire)
wire.data.materials.clear()
wire_mat = bpy.data.materials.new('Temporary_Tunnel_Wire_Material')
wire_mat.diffuse_color = (.11,.23,.30,1)
wire.data.materials.append(wire_mat)
for poly in wire.data.polygons:
    poly.material_index = 0
mod = wire.modifiers.new('Preview_Wire','WIREFRAME')
mod.thickness = .024
mod.use_replace = True
world = bpy.data.worlds.new('Temporary_Blocker_Preview_World')
world.color = (.027,.034,.046)
old_camera, old_engine, old_world = scene.camera,scene.render.engine,scene.world

def frame_camera(points, eye, width, height):
    camera_center = sum(points,Vector())/len(points)
    camera_rotation = eye.to_track_quat('Z','Y')
    local = [camera_rotation.inverted()@(p-camera_center) for p in points]
    horizontal = max(p.x for p in local)-min(p.x for p in local)
    vertical = max(p.y for p in local)-min(p.y for p in local)
    cam_data.ortho_scale = max(horizontal,vertical*width/height)*1.16
    cam.location = camera_center+eye*400
    cam.rotation_euler = camera_rotation.to_euler()
    scene.render.resolution_x,scene.render.resolution_y = width,height
    return camera_center,camera_rotation,cam_data.ortho_scale

try:
    scene.camera,scene.world = cam,world
    try:
        scene.render.engine = 'BLENDER_WORKBENCH'
    except TypeError as error:
        raise RuntimeError(f'Workbench preview unavailable: {error}')
    scene.render.resolution_percentage = 100
    scene.render.film_transparent = False
    ns['enum_value'](scene.render.image_settings,'file_format','PNG')
    shading = scene.display.shading
    for prop,value in [('light','STUDIO'),('color_type','MATERIAL'),('cavity_type','BOTH'),('background_type','WORLD')]:
        ns['enum_value'](shading,prop,value)
    shading.show_shadows = True
    shading.show_cavity = True
    shading.curvature_ridge_factor = .8
    shading.curvature_valley_factor = .8
    full_center,full_rotation,full_scale = frame_camera(bounds,direction,1600,1280)
    scene.render.filepath = str(OUTPUT/'Tunnel_Blockers_Placement.png')
    bpy.ops.render.render(write_still=True)
    wire.hide_render = True
    chosen = objects[3]
    plan = json.loads((OUTPUT/'placement_plan.json').read_text())['objects'][3]
    paths = json.loads((ns['SOURCE']/'centerlines.json').read_text())
    _,u,v,tangent = ns['frame'](paths[plan['path_index']],plan['arc_position_m'])
    eye = (-tangent+.35*u+.25*v).normalized()
    detail_bounds = [chosen.matrix_world@Vector(c) for c in chosen.bound_box]
    for obj in objects:
        obj.hide_render = obj != chosen
    frame_camera(detail_bounds,eye,1200,1000)
    scene.render.filepath = str(OUTPUT/'Tunnel_Blocker_Detail.png')
    bpy.ops.render.render(write_still=True)
finally:
    scene.camera,scene.render.engine,scene.world = old_camera,old_engine,old_world
    for obj in objects:
        obj.hide_render = False
    wire_mesh = wire.data
    bpy.data.objects.remove(wire,do_unlink=True)
    bpy.data.meshes.remove(wire_mesh)
    bpy.data.materials.remove(wire_mat)
    bpy.data.objects.remove(cam,do_unlink=True)
    bpy.data.cameras.remove(cam_data)
    bpy.data.worlds.remove(world)

select_blockers()
for area in bpy.context.screen.areas:
    if area.type == 'VIEW_3D':
        space = area.spaces.active
        space.region_3d.view_location = full_center
        space.region_3d.view_rotation = full_rotation
        space.region_3d.view_distance = full_scale*.9
        ns['enum_value'](space.region_3d,'view_perspective','ORTHO')
bpy.ops.wm.save_as_mainfile(filepath=str(OUTPUT/'Tunnel_Blockers_20.blend'))
print(json.dumps({'fbx':str(OUTPUT/'Tunnel_Blockers_20.fbx'),
                  'blend':str(OUTPUT/'Tunnel_Blockers_20.blend'),
                  'exported_objects':[obj.name for obj in objects],
                  'triangles':sum(len(obj.data.polygons) for obj in objects),
                  'scene_objects':len(scene.objects)}))
