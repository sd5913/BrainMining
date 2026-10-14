"""Export selected reusable office meshes and embedded PBR textures; leave source FBX untouched."""
import bpy,json,pathlib,re
from mathutils import Vector,Matrix
from io_scene_fbx import import_fbx
import_fbx.blen_read_light=lambda *args,**kwargs:bpy.data.lights.new('ImportLight',type='POINT')
ROOT=pathlib.Path(__file__).resolve().parents[1];SRC=ROOT.parent/'office-assets';OUT=ROOT/'Art/Office/Backrooms';OUT.mkdir(parents=True,exist_ok=True)
specs={'table1':{'SM_CubicleDesk':None},'table2':{'SM_OfficeDesk':None},'printer1':{'SM_Copier':[lambda n:not n.startswith('Small Stack')]},'guizi1':{'SM_FilingCabinet':None},'table3':{
 'SM_Calculator':['calculator.001'],'SM_CoffeeCup':['coffee_cup.001'],'SM_Folder':['folder'],
 'SM_Keyboard':['keyboard.001'],'SM_Mouse':['mouse.001'],'SM_Notebook':['notebook'],'SM_PaperTray':['paper_tray'],'SM_DeskMonitor':['monitor_27'],
 'SM_Paper':['paper_A4'],'SM_PaperStack':['stack_paper'],'SM_DeskChair':['ikea_markus.001'],
 'SM_PenCup':['mesh_pencil_cup','cutter.001','highlighter.001','marker.001','pen_1.001','pen_1.002','pen_1.003','pen_2.003','pen_2.004','pencil_1.001','pencil_1.002','pencil_2.001','pencil_2.002','pencil_3.001','pencil_3.002','pencil_4.001','pencil_4.002','pencil_4.003','ruler.001','scissors.001']}}
report={'meshes':[],'materials':{}}
def safe(n):return re.sub('[^A-Za-z0-9_]+','_',n).strip('_')
for source,groups in specs.items():
 bpy.ops.wm.read_factory_settings(use_empty=True);bpy.ops.import_scene.fbx(filepath=str(SRC/(source+'.fbx')));bpy.context.view_layer.update()
 originals=[o for o in bpy.data.objects if o.type=='MESH']
 for name,selector in groups.items():
  chosen=originals if selector is None else [o for o in originals if (selector[0](o.name) if callable(selector[0]) else o.name in selector)]
  bpy.ops.object.select_all(action='DESELECT');copies=[]
  for o in chosen:
   cp=o.copy();cp.data=o.data.copy();bpy.context.scene.collection.objects.link(cp);cp.matrix_world=o.matrix_world.copy();cp.select_set(True);copies.append(cp)
  bpy.context.view_layer.objects.active=copies[0];bpy.ops.object.join();mesh=bpy.context.object;bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
  points=[v.co for v in mesh.data.vertices];lo=Vector([min(p[i] for p in points) for i in range(3)]);hi=Vector([max(p[i] for p in points) for i in range(3)])
  pivot=Vector(((lo.x+hi.x)/2,(lo.y+hi.y)/2,lo.z))
  for v in mesh.data.vertices:v.co-=pivot
  mesh.matrix_world=Matrix.Identity(4);bpy.context.view_layer.update()
  mesh.name=name;mesh.data.name=name
  for i,m in enumerate(mesh.data.materials):
   if m is None:
    filler=bpy.data.materials.get('Office_Laminate') or bpy.data.materials.new('Office_Laminate');filler.diffuse_color=(.46,.41,.31,1);mesh.data.materials[i]=filler
  for m in mesh.data.materials:
   if not m:continue
   key=m.name if m.name.startswith(source+'_') else safe(source+'_'+m.name);m.name=key
   if key in report['materials']:continue
   bs=next((n for n in m.node_tree.nodes if n.type=='BSDF_PRINCIPLED'),None) if m.node_tree else None
   mat={'color':list(bs.inputs['Base Color'].default_value) if bs else list(m.diffuse_color),'roughness':bs.inputs['Roughness'].default_value if bs else .7,'metallic':bs.inputs['Metallic'].default_value if bs else 0,'textures':{}}
   for n in m.node_tree.nodes if m.node_tree else []:
    if n.type!='TEX_IMAGE' or not n.image:continue
    im=n.image;label=im.name.lower();role=next((r for r in ['BaseColor','Normal','Roughness','Metallic','Opacity'] if r.lower() in label),None)
    if role is None:
     if 'color' in label or label=='image':role='BaseColor'
     elif 'display' in label:role='BaseColor'
    if not role:continue
    dest=OUT/(key+'_'+role+'.png')
    # Copy decoded pixels rather than retaining paths into temporary FBX .fbm directories.
    im.filepath_raw=str(dest);im.file_format='PNG';im.save();mat['textures'][role]=dest.name
   # Keep older office cubicles beige, using original textures for metal / cloth.
   if source=='table1' and not mat['textures']:
    mat['color']=[.46,.41,.31,1] if 'Desk_board' in key else mat['color'];mat['roughness']=.78
   report['materials'][key]=mat
  bpy.ops.export_scene.fbx(filepath=str(OUT/(name+'.fbx')),use_selection=True,object_types={'MESH'},axis_forward='-Y',axis_up='Z',bake_anim=False,add_leaf_bones=False,path_mode='STRIP',mesh_smooth_type='FACE')
  report['meshes'].append({'name':name,'source':source,'size_m':list(hi-lo),'materials':[m.name for m in mesh.data.materials]})
  bpy.data.objects.remove(mesh,do_unlink=True)
(OUT/'Manifest.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8');print('EXPORTED',len(report['meshes']),len(report['materials']))
