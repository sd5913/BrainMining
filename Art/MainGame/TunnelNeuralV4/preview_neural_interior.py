"""Render real cavity surfaces and branching stones; hide plugs for morphology inspection."""
import bpy,bmesh,json,math
from pathlib import Path
from mathutils import Vector
from mathutils.bvhtree import BVHTree
import numpy as np
OUT=Path(__file__).parent
bpy.ops.wm.open_mainfile(filepath=str(OUT/'MainGame_Tunnel_Neural_V4.blend'))
scene=bpy.context.scene;tunnel=bpy.data.objects['SM_MainGame_Neural_Cutter'];tunnel.hide_render=True
for ob in bpy.data.objects:
    if ob.name.startswith('REFERENCE_') or 'Connector' in ob.name:ob.hide_render=True
for ob in bpy.data.collections['BLOCKERS'].objects:ob.hide_render=True
inside=bpy.data.objects.new('Preview_Actual_Neural_Cavity',tunnel.data.copy());scene.collection.objects.link(inside)
bm=bmesh.new();bm.from_mesh(inside.data)
for f in bm.faces:f.normal_flip()
bm.to_mesh(inside.data);bm.free()
path=json.loads((OUT/'centerlines.json').read_text())[0];center=json.loads((OUT/'plan_summary.json').read_text())['cavern_centers_arc_m'][0]
def point(s):return Vector([float(np.interp(s,path['arc'],np.array(path['points'])[:,i])) for i in range(3)])
tunnel.data.calc_loop_triangles()
tree=BVHTree.FromPolygons([v.co[:] for v in tunnel.data.vertices],[tuple(t.vertices) for t in tunnel.data.loop_triangles],all_triangles=True)
eye=point(center-6);floor=tree.ray_cast(eye,Vector((0,0,-1)),25)[0];assert floor is not None
eye.z=floor.z+2.;target=point(center+2);target.z=eye.z+.1
data=bpy.data.cameras.new('Neural_Interior_Camera');camera=bpy.data.objects.new('Neural_Interior_Camera',data);scene.collection.objects.link(camera)
camera.location=eye;camera.rotation_euler=(target-eye).to_track_quat('-Z','Y').to_euler();data.lens=18.;data.clip_start=.08;data.clip_end=500;scene.camera=camera
scene.render.engine='BLENDER_WORKBENCH';scene.render.resolution_x=1440;scene.render.resolution_y=960;scene.render.resolution_percentage=100
scene.render.image_settings.file_format='PNG';scene.render.filepath=str(OUT/'MainGame_Neural_Interior.png')
shade=scene.display.shading;shade.light='STUDIO';shade.color_type='MATERIAL';shade.show_shadows=True;shade.show_cavity=True;shade.cavity_type='BOTH';shade.show_backface_culling=True
scene.world.color=(.025,.03,.04);bpy.ops.render.render(write_still=True)
forward=(target-eye).normalized()
(OUT/'preview_camera.json').write_text(json.dumps(dict(location_m=list(eye),ue_yaw_deg=math.degrees(math.atan2(-forward.y,forward.x)),
    ue_pitch_deg=math.degrees(math.asin(forward.z)),blockers_hidden_in_preview=True),indent=2))
print('NEURAL_PREVIEW_COMPLETE',flush=True)
