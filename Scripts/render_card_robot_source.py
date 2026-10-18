import bpy
from pathlib import Path
from mathutils import Vector
root=Path(__file__).resolve().parents[1];out=root/'Saved/Validation/CardRobot'
bpy.ops.wm.open_mainfile(filepath=str(out/'inspect.blend'))
scene=bpy.context.scene;scene.render.engine='BLENDER_WORKBENCH';scene.display.shading.light='STUDIO';scene.display.shading.color_type='MATERIAL'
scene.display.shading.show_shadows=True;scene.display.shading.show_cavity=True
scene.render.resolution_x=720;scene.render.resolution_y=720;scene.render.resolution_percentage=100
scene.world=bpy.data.worlds.new('PreviewWorld');scene.world.color=(.1,.1,.1)
bpy.ops.object.camera_add();camera=bpy.context.object;scene.camera=camera;camera.data.type='ORTHO';camera.data.ortho_scale=5
for side in (-1,1):
    camera.location=(-.65,2.2+side*10,2.2);camera.rotation_euler=(Vector((-.65,2.2,1.8))-camera.location).to_track_quat('-Z','Y').to_euler()
    scene.render.filepath=str(out/('source-front.png' if side==-1 else 'source-back.png'));bpy.ops.render.render(write_still=True)
