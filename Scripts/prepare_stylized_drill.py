"""Prepare the latest drill on the original X shaft and bake UE-ready PBR textures."""
import bpy,math,json
from pathlib import Path
from mathutils import Vector,Matrix
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'SourceAssets/Drill/Stylized';OUT.mkdir(parents=True,exist_ok=True)
bpy.ops.wm.read_factory_settings(use_empty=True);bpy.ops.import_scene.fbx(filepath='C:/Users/Lenovo/Desktop/drill.fbx',use_image_search=False)
objects=[o for o in bpy.context.scene.objects if o.type=='MESH' and o.location.x<-.3]
assert len(objects)==5,[o.name for o in objects]
front=next(o for o in objects if o.name=='drill_press_01_gear_b.001');axis_x,axis_z=front.location.x,front.location.z
world={o:[o.matrix_world@v.co for v in o.data.vertices] for o in objects}
lo=min(v.y for vs in world.values() for v in vs);hi=max(v.y for vs in world.values() for v in vs);factor=.58603/(hi-lo)
for o in objects:
 o.animation_data_clear()
 alignment=Matrix(((0,factor,0,-lo*factor-.392025),(-factor,0,0,axis_x*factor),(0,0,factor,-axis_z*factor),(0,0,0,1)))
 o.data.transform(alignment@o.matrix_world);o.data.update()
 o.location=(0,0,0);o.rotation_euler=(0,0,0);o.scale=(1,1,1)
for o in list(bpy.data.objects):
 if o not in objects:bpy.data.objects.remove(o,do_unlink=True)
def material(name,c,rough,metal,var):
 m=bpy.data.materials.new(name);m.use_nodes=True;m.diffuse_color=(*c,1);n=m.node_tree.nodes;l=m.node_tree.links;n.clear();p=n.new('ShaderNodeBsdfPrincipled');out=n.new('ShaderNodeOutputMaterial');l.new(p.outputs[0],out.inputs[0]);p.inputs['Metallic'].default_value=metal
 tc=n.new('ShaderNodeTexCoord');noise=n.new('ShaderNodeTexNoise');noise.inputs['Scale'].default_value=45;noise.inputs['Detail'].default_value=2.5;l.new(tc.outputs['Object'],noise.inputs['Vector'])
 ramp=n.new('ShaderNodeValToRGB');ramp.color_ramp.elements[0].position=.18;ramp.color_ramp.elements[0].color=(*(v*(1-var) for v in c),1);ramp.color_ramp.elements[1].position=.82;ramp.color_ramp.elements[1].color=(*(min(v*(1+var),1) for v in c),1);l.new(noise.outputs['Fac'],ramp.inputs[0]);l.new(ramp.outputs[0],p.inputs['Base Color'])
 fine=n.new('ShaderNodeTexNoise');fine.inputs['Scale'].default_value=1400;fine.inputs['Detail'].default_value=2;l.new(tc.outputs['Object'],fine.inputs['Vector'])
 rr=n.new('ShaderNodeMapRange');rr.inputs['To Min'].default_value=rough-.06;rr.inputs['To Max'].default_value=rough+.06;l.new(fine.outputs['Fac'],rr.inputs[0]);l.new(rr.outputs[0],p.inputs['Roughness'])
 bump=n.new('ShaderNodeBump');bump.inputs['Distance'].default_value=.00009;bump.inputs['Strength'].default_value=.12;l.new(fine.outputs['Fac'],bump.inputs['Height']);l.new(bump.outputs[0],p.inputs['Normal'])
 return m
paint=material('Paint_IndustrialYellow',(.55,.29,.025),.48,.18,.18)
dark=material('Trim_Charcoal',(.026,.040,.046),.52,.35,.15)
steel=material('Cutter_Steel',(.23,.28,.30),.38,.68,.12)
fastener=material('Fastener_Silver',(.42,.45,.48),.28,.9,.06)
edge=material('Worn_Edge',(.67,.43,.11),.48,.35,.1)
for o in objects:
 o.data.materials.clear()
 is_body=o.name=='drill_press_01_gear_b';is_ring=o.name in ['Bolt.001','柱体.001']
 o.data.materials.append(fastener if o.name=='Bolt.001' else paint if is_body else steel if o==front else dark)
 for p in o.data.polygons:p.material_index=0
 # Contrast fastening details and recesses against the yellow casing.
 if (is_body or is_ring) and o.name!='Bolt.001':
  o.data.materials.append(dark);o.data.materials.append(steel)
  for p in o.data.polygons:
   if p.normal.x>.85 or p.normal.x<-.85:p.material_index=1 if is_body else 2
 if is_body:
  o.data.materials.append(edge);be=o.modifiers.new('Soft paint edges','BEVEL');be.width=.0009;be.segments=2;be.limit_method='ANGLE';be.angle_limit=.65;be.material=3;be.harden_normals=True
  wn=o.modifiers.new('Weighted panel normals','WEIGHTED_NORMAL');wn.keep_sharp=True
dg=bpy.context.evaluated_depsgraph_get();copies=[]
for o in objects:
 me=bpy.data.meshes.new_from_object(o.evaluated_get(dg));ob=bpy.data.objects.new(o.name+'_Baked',me);bpy.context.collection.objects.link(ob);copies.append(ob)
for o in objects:bpy.data.objects.remove(o,do_unlink=True)
copy_names=[o.name for o in copies]
for o in copies:o.select_set(True)
bpy.context.view_layer.objects.active=copies[0];bpy.ops.object.join();mesh=copies[0];mesh.name='SM_DrillStylized'
for uv in list(mesh.data.uv_layers):mesh.data.uv_layers.remove(uv)
mesh.data.uv_layers.new(name='UVMap');bpy.ops.object.mode_set(mode='EDIT');bpy.ops.mesh.select_all(action='SELECT');bpy.ops.uv.smart_project(angle_limit=1.1,island_margin=.008);bpy.ops.object.mode_set(mode='OBJECT')
sc=bpy.context.scene;sc.render.engine='CYCLES';sc.cycles.samples=1;sc.render.bake.margin=8;outputs={}
materials=set(m for m in mesh.data.materials if m)
for role,socket in [('BaseColor','Base Color'),('Roughness','Roughness'),('Metallic','Metallic'),('Normal',None)]:
 im=bpy.data.images.new('T_DrillStylized_'+role,2048 if role=='BaseColor' else 1024,2048 if role=='BaseColor' else 1024,alpha=False);im.colorspace_settings.name='sRGB' if role=='BaseColor' else 'Non-Color';saved=[]
 for m in materials:
  n=m.node_tree.nodes;l=m.node_tree.links;out=next(x for x in n if x.type=='OUTPUT_MATERIAL');p=next(x for x in n if x.type=='BSDF_PRINCIPLED');original=out.inputs[0].links[0].from_socket;em=None
  if socket:
   em=n.new('ShaderNodeEmission');inp=p.inputs[socket]
   if inp.is_linked:l.new(inp.links[0].from_socket,em.inputs['Color'])
   else:
    v=inp.default_value;em.inputs['Color'].default_value=tuple(v) if role=='BaseColor' else (v,v,v,1)
   l.new(em.outputs[0],out.inputs[0])
  tex=n.new('ShaderNodeTexImage');tex.image=im;n.active=tex;saved.append((m,out,original,em,tex))
 bpy.ops.object.bake(type='NORMAL' if role=='Normal' else 'EMIT')
 im.filepath_raw=str(OUT/(im.name+'.png'));im.file_format='PNG';im.save();outputs[role]=im
 for m,out,original,em,tex in saved:
  if em:m.node_tree.links.new(original,out.inputs[0]);m.node_tree.nodes.remove(em)
  m.node_tree.nodes.remove(tex)
atlas=bpy.data.materials.new('DrillStylized');atlas.use_nodes=True;p=next(n for n in atlas.node_tree.nodes if n.type=='BSDF_PRINCIPLED')
for role,socket in [('BaseColor','Base Color'),('Roughness','Roughness'),('Metallic','Metallic'),('Normal','Normal')]:
 t=atlas.node_tree.nodes.new('ShaderNodeTexImage');t.image=outputs[role]
 if role=='Normal':
  norm=atlas.node_tree.nodes.new('ShaderNodeNormalMap');atlas.node_tree.links.new(t.outputs['Color'],norm.inputs['Color']);atlas.node_tree.links.new(norm.outputs[0],p.inputs[socket])
 else:atlas.node_tree.links.new(t.outputs['Color'],p.inputs[socket])
mesh.data.materials.clear();mesh.data.materials.append(atlas)
for f in mesh.data.polygons:f.material_index=0
bpy.ops.export_scene.fbx(filepath=str(OUT/'SM_DrillStylized.fbx'),use_selection=True,object_types={'MESH'},axis_forward='-Y',axis_up='Z',use_mesh_modifiers=True,bake_anim=False,add_leaf_bones=False,path_mode='STRIP',mesh_smooth_type='FACE')
mesh.data.calc_loop_triangles();report={'objects':copy_names,'triangles':len(mesh.data.loop_triangles),'shaft_axis':[1,0,0],'shaft_pivot':[0,0,0],'length_cm':58.603,'curve_policy':'Reuse existing CF_DrillSpin','source_actions':[a.name for a in bpy.data.actions]};(OUT/'Export.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
# Small standalone art preview of the actual baked material.
cam=bpy.data.objects.new('Camera',bpy.data.cameras.new('Camera'));sc.collection.objects.link(cam);cam.location=(.80,-1.1,.62);target=Vector((-.06,0,0));cam.rotation_euler=(target-cam.location).to_track_quat('-Z','Y').to_euler();cam.data.type='ORTHO';cam.data.ortho_scale=.82;sc.camera=cam
for name,co,power,color in [('Key',(.3,-.4,.7),22,(1,.90,.75)),('Fill',(-.3,.4,.4),12,(.65,.8,1))]:
 ld=bpy.data.lights.new(name,'AREA');ld.energy=power;ld.size=.6;ld.color=color;ob=bpy.data.objects.new(name,ld);sc.collection.objects.link(ob);ob.location=co;ob.rotation_euler=(target-ob.location).to_track_quat('-Z','Y').to_euler()
sc.world=bpy.data.worlds.new('Studio');sc.world.use_nodes=True;bg=next(n for n in sc.world.node_tree.nodes if n.type=='BACKGROUND');bg.inputs[0].default_value=(.05,.06,.08,1);bg.inputs[1].default_value=.3
sc.render.film_transparent=True;sc.render.resolution_x=1200;sc.render.resolution_y=700;sc.render.resolution_percentage=100;sc.cycles.samples=20;sc.cycles.use_denoising=True;sc.view_settings.view_transform='AgX'
art=ROOT/'Art/Drill/Stylized';art.mkdir(parents=True,exist_ok=True);sc.render.filepath=str(art/'Preview.png')
for m in list(bpy.data.materials):
 if m!=atlas:bpy.data.materials.remove(m,do_unlink=True)
bpy.data.orphans_purge(do_recursive=True)
for im in outputs.values():im.pack()
bpy.ops.wm.save_as_mainfile(filepath=str(art/'Drill_Stylized.blend'));bpy.ops.render.render(write_still=True);print('DRILL_STYLED',report)
