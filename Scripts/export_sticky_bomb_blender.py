"""Bake the artist's live C4 variant and export three independently controlled sections."""
import bpy,json
from pathlib import Path
from mathutils import Vector
ROOT=Path('E:/涌现taptap2026/BrainMining');OUT=ROOT/'SourceAssets/Bomb/StickyCountdown';OUT.mkdir(parents=True,exist_ok=True)
items=json.loads((ROOT/'Saved/BombCountdown/live_objects.json').read_text(encoding='utf-8'))
names={i['name'] for i in items};offset=Vector((-.4667259752750397,-.1589272916316986,.7286515235900879));scale=.5
lamp_names={'SB_Detail_28','SB_Detail_28.001'};assert lamp_names<=names
lamps=bpy.data.materials.new('BombFuseLamp');lamps.use_nodes=True
lp=next(n for n in lamps.node_tree.nodes if n.type=='BSDF_PRINCIPLED');lp.inputs['Base Color'].default_value=(.35,.015,.006,1)
objects=[]
dg=bpy.context.evaluated_depsgraph_get()
for name in sorted(names):
 if name=='SB_Timer':continue
 ob=bpy.data.objects[name];ev=ob.evaluated_get(dg);me=bpy.data.meshes.new_from_object(ev)
 copy=bpy.data.objects.new('Export_'+name,me);bpy.context.scene.collection.objects.link(copy)
 for v in me.vertices:v.co=(ob.matrix_world@v.co-offset)*scale
 if name in lamp_names:
  me.materials.clear();me.materials.append(lamps)
  for p in me.polygons:p.material_index=0
 objects.append(copy)
for ob in list(bpy.data.objects):
 if ob not in objects:bpy.data.objects.remove(ob,do_unlink=True)
for ob in objects:ob.select_set(True)
bpy.context.view_layer.objects.active=objects[0];bpy.ops.object.join();mesh=objects[0];mesh.name='SM_Bomb'
while len(mesh.data.uv_layers)>1:mesh.data.uv_layers.remove(mesh.data.uv_layers[-1])
if not mesh.data.uv_layers:mesh.data.uv_layers.new(name='UVMap')
mesh.data.uv_layers[0].name='UVMap';mesh.data.uv_layers.active_index=0
bpy.ops.object.mode_set(mode='EDIT');bpy.ops.mesh.select_all(action='SELECT');bpy.ops.uv.smart_project(angle_limit=1.15,island_margin=.008);bpy.ops.object.mode_set(mode='OBJECT')
sc=bpy.context.scene;sc.render.engine='CYCLES';sc.cycles.samples=1;sc.render.bake.margin=6
materials=set(m for m in mesh.data.materials if m)
outputs={}
for channel,socket in [('BaseColor','Base Color'),('Roughness','Roughness'),('Metallic','Metallic')]:
 image=bpy.data.images.new('T_StickyBomb_'+channel,1024,1024,alpha=False);image.colorspace_settings.name='sRGB' if channel=='BaseColor' else 'Non-Color'
 saved=[]
 for mat in materials:
  nt=mat.node_tree;n=nt.nodes;l=nt.links;out=next(x for x in n if x.type=='OUTPUT_MATERIAL');p=next(x for x in n if x.type=='BSDF_PRINCIPLED')
  original=out.inputs['Surface'].links[0].from_socket
  em=n.new('ShaderNodeEmission');source=p.inputs[socket]
  if source.is_linked:l.new(source.links[0].from_socket,em.inputs['Color'])
  else:
   value=source.default_value;em.inputs['Color'].default_value=tuple(value) if socket=='Base Color' else (value,value,value,1)
  l.new(em.outputs[0],out.inputs['Surface']);tex=n.new('ShaderNodeTexImage');tex.image=image;n.active=tex;saved.append((mat,out,original,em,tex))
 bpy.ops.object.bake(type='EMIT')
 image.filepath_raw=str(OUT/('T_StickyBomb_'+channel+'.png'));image.file_format='PNG';image.save();outputs[channel]=image
 for mat,out,original,em,tex in saved:
  mat.node_tree.links.new(original,out.inputs['Surface']);mat.node_tree.nodes.remove(em);mat.node_tree.nodes.remove(tex)
body=bpy.data.materials.new('BombBody');body.use_nodes=True;p=next(n for n in body.node_tree.nodes if n.type=='BSDF_PRINCIPLED')
for channel,socket in [('BaseColor','Base Color'),('Roughness','Roughness'),('Metallic','Metallic')]:
 t=body.node_tree.nodes.new('ShaderNodeTexImage');t.image=outputs[channel];body.node_tree.links.new(t.outputs['Color'],p.inputs[socket])
is_lamp=[mesh.data.materials[p.material_index]==lamps for p in mesh.data.polygons]
mesh.data.materials.clear();mesh.data.materials.append(body);mesh.data.materials.append(lamps)
for p,lamp in zip(mesh.data.polygons,is_lamp):p.material_index=int(lamp)
# Replace the baked static font with a flush, UV-normalized digital display plane.
cx=.0232889354*scale;cy=.1341120899*scale;z=.04585*scale;w=.125*scale;h=.029*scale
me=bpy.data.meshes.new('CountdownDisplay');me.from_pydata([(cx-w/2,cy-h/2,z),(cx+w/2,cy-h/2,z),(cx+w/2,cy+h/2,z),(cx-w/2,cy+h/2,z)],[],[(0,1,2,3)]);me.update()
uv=me.uv_layers.new(name='UVMap');coords=[(0,0),(1,0),(1,1),(0,1)]
for li,co in zip(me.polygons[0].loop_indices,coords):uv.data[li].uv=co
display=bpy.data.materials.new('BombCountdownDisplay');me.materials.append(display)
ob=bpy.data.objects.new('Display',me);sc.collection.objects.link(ob);ob.select_set(True);mesh.select_set(True);bpy.context.view_layer.objects.active=mesh;bpy.ops.object.join()
mesh.data.calc_loop_triangles()
bpy.ops.export_scene.fbx(filepath=str(OUT/'StickyBomb.fbx'),use_selection=True,object_types={'MESH'},axis_forward='-Y',axis_up='Z',apply_unit_scale=True,global_scale=1.,use_mesh_modifiers=True,mesh_smooth_type='FACE',add_leaf_bones=False,bake_anim=False,path_mode='COPY',embed_textures=False)
report={'scale':scale,'triangles':len(mesh.data.loop_triangles),'materials':[m.name for m in mesh.data.materials],'lamp_objects':sorted(lamp_names),'display_center_cm':[cx*100,cy*100,z*100]}
(OUT/'Export.json').write_text(json.dumps(report,indent=2));print('C4_EXPORTED',report)
