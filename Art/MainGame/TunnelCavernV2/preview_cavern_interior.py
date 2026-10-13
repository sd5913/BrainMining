"""Render the real inward cavity surfaces, including rock-column holes; no asset edits."""
import bpy,bmesh,json,math
from pathlib import Path
from mathutils import Vector
import numpy as np
OUT=Path(__file__).parent
bpy.ops.wm.open_mainfile(filepath=str(OUT/'MainGame_Tunnel_Cavern_V2.blend'))
scene=bpy.context.scene;tunnel=bpy.data.objects['SM_MainGame_Cavern_Cutter']
tunnel.hide_render=True
for ob in bpy.data.objects:
    if ob.name.startswith('REFERENCE_') or 'Connector' in ob.name:ob.hide_render=True
inside=bpy.data.objects.new('Preview_Real_Cavity_Surfaces',tunnel.data.copy());scene.collection.objects.link(inside)
bm=bmesh.new();bm.from_mesh(inside.data)
for f in bm.faces:f.normal_flip()
bm.to_mesh(inside.data);bm.free()
path=json.loads((OUT/'centerlines.json').read_text())[0];center=json.loads((OUT/'plan_summary.json').read_text())['cavern_centers_arc_m'][0]
def point(s):return Vector([float(np.interp(s,path['arc'],np.array(path['points'])[:,i])) for i in range(3)])
tangent=(point(center+.2)-point(center-.2)).normalized();side=tangent.cross(Vector((0,0,1))).normalized()
eye=point(center-2.7)-side*4.5+Vector((0,0,-1.6));target=point(center+.8)+Vector((0,0,-.8))
data=bpy.data.cameras.new('Interior_Camera');camera=bpy.data.objects.new('Interior_Camera',data);scene.collection.objects.link(camera)
camera.location=eye;camera.rotation_euler=(target-eye).to_track_quat('-Z','Y').to_euler();data.lens=19.;data.clip_start=.08;data.clip_end=500;scene.camera=camera
scene.render.engine='BLENDER_WORKBENCH';scene.render.resolution_x=1440;scene.render.resolution_y=960;scene.render.resolution_percentage=100
scene.render.image_settings.file_format='PNG';scene.render.filepath=str(OUT/'MainGame_Cavern_Interior.png')
shade=scene.display.shading;shade.light='STUDIO';shade.color_type='MATERIAL';shade.show_shadows=True;shade.show_cavity=True;shade.cavity_type='BOTH';shade.show_backface_culling=True
scene.world.color=(.025,.03,.04)
bpy.ops.render.render(write_still=True)
print('CAVERN_INTERIOR_RENDER_COMPLETE',flush=True)
