"""Blender: rasterize only the four casing screws in the existing, unchanged UV atlas."""
import bpy,json
from pathlib import Path
root=Path(__file__).resolve().parents[1];out=root/'Saved/DrillStylized/SilverScrews';out.mkdir(parents=True,exist_ok=True)
mesh=bpy.data.objects['SM_DrillStylized'];data=mesh.data
assert len(data.vertices)==12016
assert all(-.044<v.co.x<-.017 for v in list(data.vertices)[:468])
def emission(name,value):
 m=bpy.data.materials.new(name);m.use_nodes=True;n=m.node_tree.nodes;n.clear();e=n.new('ShaderNodeEmission');e.inputs['Color'].default_value=(value,value,value,1);o=n.new('ShaderNodeOutputMaterial');m.node_tree.links.new(e.outputs[0],o.inputs[0]);return m
black=emission('MaskBlack',0);white=emission('ScrewMask',1)
data.materials.clear();data.materials.append(black);data.materials.append(white)
faces=[]
for p in data.polygons:
 p.material_index=int(all(v<468 for v in p.vertices))
 if p.material_index:faces.append(p.index)
assert len(faces)==884,len(faces)
for o in bpy.context.scene.objects:o.select_set(False)
mesh.select_set(True);bpy.context.view_layer.objects.active=mesh
scene=bpy.context.scene;scene.render.engine='CYCLES';scene.cycles.samples=1;scene.render.bake.margin=8
for size in [2048,1024]:
 image=bpy.data.images.new('ScrewMask'+str(size),size,size,alpha=False);image.colorspace_settings.name='Non-Color'
 for m in [black,white]:
  tex=m.node_tree.nodes.new('ShaderNodeTexImage');tex.image=image;m.node_tree.nodes.active=tex
 bpy.ops.object.bake(type='EMIT')
 image.filepath_raw=str(out/('mask-'+str(size)+'.png'));image.file_format='PNG';image.save()
(out/'mask-report.json').write_text(json.dumps({'screws':4,'faces':len(faces),'geometry_changed':False,'uv_changed':False},indent=2))
print('SCREW_MASKS_READY')
