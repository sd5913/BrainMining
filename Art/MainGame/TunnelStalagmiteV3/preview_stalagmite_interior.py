"""Render the actual inward cavity and ground stalagmites, without changing exported meshes."""
import bpy,bmesh,json
from pathlib import Path
from mathutils import Vector
from mathutils.bvhtree import BVHTree
import numpy as np
OUT=Path(__file__).parent
bpy.ops.wm.open_mainfile(filepath=str(OUT/'MainGame_Tunnel_Stalagmite_V3.blend'))
scene=bpy.context.scene;tunnel=bpy.data.objects['SM_MainGame_Stalagmite_Cutter'];tunnel.hide_render=True
for ob in bpy.data.objects:
    if ob.name.startswith('REFERENCE_') or 'Connector' in ob.name:ob.hide_render=True
inside=bpy.data.objects.new('Preview_Actual_Cavity',tunnel.data.copy());scene.collection.objects.link(inside)
bm=bmesh.new();bm.from_mesh(inside.data)
for f in bm.faces:f.normal_flip()
bm.to_mesh(inside.data);bm.free()
path=json.loads((OUT/'centerlines.json').read_text())[0];center=json.loads((OUT/'plan_summary.json').read_text())['cavern_centers_arc_m'][0]
def point(s):return Vector([float(np.interp(s,path['arc'],np.array(path['points'])[:,i])) for i in range(3)])
tunnel.data.calc_loop_triangles()
tree=BVHTree.FromPolygons([v.co[:] for v in tunnel.data.vertices],[tuple(t.vertices) for t in tunnel.data.loop_triangles],all_triangles=True)
eye=point(center-3.35);floor=tree.ray_cast(eye,Vector((0,0,-1)),25)[0];assert floor is not None
eye.z=floor.z+1.7;target=point(center+2);target.z=eye.z+.1
data=bpy.data.cameras.new('Stalagmite_Interior_Camera');camera=bpy.data.objects.new('Stalagmite_Interior_Camera',data);scene.collection.objects.link(camera)
camera.location=eye;camera.rotation_euler=(target-eye).to_track_quat('-Z','Y').to_euler();data.lens=18.;data.clip_start=.08;data.clip_end=500;scene.camera=camera
scene.render.engine='BLENDER_WORKBENCH';scene.render.resolution_x=1440;scene.render.resolution_y=960;scene.render.resolution_percentage=100
scene.render.image_settings.file_format='PNG';scene.render.filepath=str(OUT/'MainGame_Stalagmite_Interior.png')
shade=scene.display.shading;shade.light='STUDIO';shade.color_type='MATERIAL';shade.show_shadows=True;shade.show_cavity=True;shade.cavity_type='BOTH';shade.show_backface_culling=True
scene.world.color=(.025,.03,.04)
bpy.ops.render.render(write_still=True)
(OUT/'preview_camera.json').write_text(json.dumps(dict(location_m=list(eye),rotation_radians=list(camera.rotation_euler)),indent=2))
print('STALAGMITE_PREVIEW_COMPLETE',flush=True)
