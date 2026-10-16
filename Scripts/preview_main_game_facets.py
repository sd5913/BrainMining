"""Offline mesh QA; viewport lights are not installed in MainGame."""
import bpy,json,math
from pathlib import Path
from mathutils import Vector
from mathutils.bvhtree import BVHTree
ROOT=Path(__file__).resolve().parents[1];out=ROOT/'Saved/Validation/MainGameRoutesV4/GeometryPreview';out.mkdir(parents=True,exist_ok=True)
files={'TunnelV1':'TunnelV1_RoutesV2.blend','TunnelCavernV2':'Cavern_Faceted_Validated.blend','TunnelStalagmiteV3':'TunnelStalagmiteV3_RoutesV2.blend','TunnelNeuralV4':'Neural_Faceted_Validated.blend'}
for stage,file in files.items():
 folder=ROOT/'Art/MainGame/RoutesV4'/stage;bpy.ops.wm.open_mainfile(filepath=str(folder/file));scene=bpy.context.scene
 tunnel=next(o for o in bpy.data.objects if o.type=='MESH' and 'Cutter' in o.name)
 for ob in scene.objects:ob.hide_render=ob!=tunnel
 path=json.loads((folder/'centerlines.json').read_text())[0];k=int(len(path['points'])*.23);p=Vector(path['points'][k]);tree=BVHTree.FromObject(tunnel,bpy.context.evaluated_depsgraph_get());hit=tree.ray_cast(p,Vector((0,0,-1)),30)
 camera=bpy.data.cameras.new('QA_Camera');ob=bpy.data.objects.new('QA_Camera',camera);scene.collection.objects.link(ob);ob.location=p.copy()
 if hit[0]:ob.location.z=hit[0].z+1.7
 target=Vector(path['points'][min(k+20,len(path['points'])-1)]);ob.rotation_euler=(target-ob.location).to_track_quat('-Z','Y').to_euler();camera.lens=19;camera.clip_start=.03;camera.clip_end=1000;scene.camera=ob
 scene.render.engine='BLENDER_WORKBENCH';scene.display.shading.light='STUDIO';scene.display.shading.color_type='SINGLE';scene.display.shading.single_color=(.34,.29,.23);scene.display.shading.show_cavity=True;scene.display.shading.cavity_type='BOTH';scene.display.shading.show_shadows=False;scene.render.resolution_x=1280;scene.render.resolution_y=720;scene.render.resolution_percentage=100
 scene.render.filepath=str(out/(stage+'.png'));bpy.ops.render.render(write_still=True)
