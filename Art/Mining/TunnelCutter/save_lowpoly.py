"""Export and preview the Lowpoly solid cutter; no preview props are saved."""
import bpy
import json
from pathlib import Path
from mathutils import Vector

OUTPUT = Path('H:/PolyU/Spotlight/BrainMining/Art/Mining/TunnelCutter')
scene = bpy.context.scene
obj = bpy.data.objects['SM_DescendingTunnel_Cutter_Lowpoly']
bpy.ops.object.select_all(action='DESELECT')
obj.select_set(True)
bpy.context.view_layer.objects.active = obj
bpy.ops.export_scene.fbx(
    filepath=str(OUTPUT / 'SM_DescendingTunnel_Cutter_Lowpoly.fbx'),
    use_selection=True, object_types={'MESH'}, global_scale=1.0,
    apply_unit_scale=True, apply_scale_options='FBX_SCALE_UNITS',
    axis_forward='-Y', axis_up='Z', use_mesh_modifiers=True,
    mesh_smooth_type='FACE', use_custom_props=True, bake_anim=False)

bounds = [obj.matrix_world @ Vector(c) for c in obj.bound_box]
center = sum(bounds, Vector()) / 8
direction = Vector((1,-.4,.4)).normalized()
rotation = direction.to_track_quat('Z','Y')
cam_data = bpy.data.cameras.new('Temporary_Rock_Preview_Camera')
cam = bpy.data.objects.new('Temporary_Rock_Preview_Camera',cam_data)
scene.collection.objects.link(cam)
cam.rotation_euler = rotation.to_euler()
cam_data.type = 'ORTHO'
cam_data.clip_end = 1000
old_camera, old_engine = scene.camera, scene.render.engine
try:
    scene.camera = cam
    # Engine enum choices are checked by Blender upon assignment.
    try:
        scene.render.engine = 'BLENDER_WORKBENCH'
    except TypeError as error:
        raise RuntimeError(f'Workbench preview unavailable: {error}')
    scene.render.resolution_percentage = 100
    scene.render.film_transparent = False
    formats = [i.identifier for i in scene.render.image_settings.bl_rna.properties['file_format'].enum_items]
    assert 'PNG' in formats
    scene.render.image_settings.file_format = 'PNG'
    shading = scene.display.shading
    for prop, value in [('light','STUDIO'),('color_type','MATERIAL'),('cavity_type','BOTH'),('background_type','WORLD')]:
        assert value in [i.identifier for i in shading.bl_rna.properties[prop].enum_items]
        setattr(shading,prop,value)
    shading.show_shadows = True
    shading.show_cavity = True
    shading.curvature_ridge_factor = .9
    shading.curvature_valley_factor = .8
    scene.world = bpy.data.worlds.new('Lowpoly_Preview_Background')
    scene.world.color = (.035,.045,.060)
    local = [rotation.inverted() @ (p-center) for p in bounds]
    cam_data.ortho_scale = max(max(p.x for p in local)-min(p.x for p in local),
                               (max(p.y for p in local)-min(p.y for p in local))*1.25)*1.13
    cam.location = center + direction*400
    scene.render.resolution_x, scene.render.resolution_y = 1600,1280
    scene.render.filepath = str(OUTPUT / 'Tunnel_Lowpoly_Preview.png')
    bpy.ops.render.render(write_still=True)
    data = json.loads((OUTPUT / 'centerlines.json').read_text())
    detail_center = Vector(data[4]['points'][0])
    cam.location = detail_center + direction*120
    cam_data.ortho_scale = 23
    scene.render.resolution_x,scene.render.resolution_y = 1200,1000
    scene.render.filepath = str(OUTPUT / 'Tunnel_Lowpoly_Detail.png')
    bpy.ops.render.render(write_still=True)
finally:
    scene.camera, scene.render.engine = old_camera, old_engine
    bpy.data.objects.remove(cam,do_unlink=True)
    bpy.data.cameras.remove(cam_data)
bpy.ops.wm.save_as_mainfile(filepath=str(OUTPUT / 'Descending_Tunnel_Cutter_Lowpoly.blend'))
print(json.dumps({'blend':str(OUTPUT / 'Descending_Tunnel_Cutter_Lowpoly.blend'),
                  'fbx':str(OUTPUT / 'SM_DescendingTunnel_Cutter_Lowpoly.fbx'),
                  'active_scene_objects':[o.name for o in scene.objects]}))
