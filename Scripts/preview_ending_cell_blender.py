import bpy, math
from pathlib import Path
from mathutils import Vector
root=Path(__file__).resolve().parents[1]
bpy.ops.wm.open_mainfile(filepath=str(next((root/'Art/Ending/CellModels').glob('*.blend'))))
scene=bpy.context.scene
scene.render.engine='BLENDER_WORKBENCH'
scene.display.shading.light='STUDIO'
scene.display.shading.color_type='MATERIAL'
scene.display.shading.show_shadows=True
scene.display.shading.show_cavity=True
scene.display.shading.background_type='WORLD'
scene.world.color=(.025,.025,.025)
scene.render.resolution_x=800;scene.render.resolution_y=800;scene.render.resolution_percentage=100
for m in bpy.data.materials:
    if m.use_nodes:
        bs=next((n for n in m.node_tree.nodes if n.type=='BSDF_PRINCIPLED'),None)
        if bs:m.diffuse_color=bs.inputs['Base Color'].default_value
cam=bpy.data.objects.new('PreviewCamera',bpy.data.cameras.new('PreviewCamera'))
scene.collection.objects.link(cam);scene.camera=cam
cam.data.type='ORTHO';cam.data.ortho_scale=46
center=Vector((2,-1,-3))
for i,d in enumerate([(-1,-1,.75),(1,-1,.75),(1,1,.75),(-1,1,.75)]):
    cam.location=center+Vector(d).normalized()*75
    cam.rotation_euler=(center-cam.location).to_track_quat('-Z','Y').to_euler()
    scene.render.filepath=str(root/f'Saved/cell-source-{i}.png')
    bpy.ops.render.render(write_still=True)
