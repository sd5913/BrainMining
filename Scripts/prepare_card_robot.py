"""Preserve the supplied rigid toy geometry; give each mechanical part its own pivot."""
import bpy,json,bmesh
from pathlib import Path
from mathutils import Vector
root=Path(__file__).resolve().parents[1];out=root/'SourceAssets/CardRobot/Prepared';out.mkdir(parents=True,exist_ok=True)
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=str(root/'SourceAssets/CardRobot/character.fbx'))
meshes={o.name:o for o in bpy.data.objects if o.type=='MESH'}
scale=1.4/(3.425699+.241)
# Canonical UE coordinates: feet at Z=0, face toward -X. Blender's Y is mirrored by FBX import.
def canonical(p):return Vector(((p.y-2.2)*scale,-(p.x+.65)*scale,(p.z+.241)*scale))
parts={
 'Body':(['Toy wooden doll','Toy wooden doll.004'],(0,0,.65)),
 'Head':(['Toy wooden doll.001'],(0,0,.9)),
 'ArmL':(['Toy wooden doll.002'],(0,.365,.62)),
 'ArmR':(['Toy wooden doll.003'],(0,-.365,.62)),
 'FootL':(['Toy wooden doll.006'],(0,.125,.30)),
 'FootR':(['Toy wooden doll.005'],(0,-.125,.30)),
}
brows=sorted(n for n in meshes if not n.startswith('Toy wooden'))
parts['BrowL']=([brows[0]],None);parts['BrowR']=([brows[1]],None)
# The two screw eyes are one source object. Split at its center without altering the screws.
eye=meshes['Toy wooden doll.007']
for name,sign in [('EyeL',1),('EyeR',-1)]:
    obj=eye.copy();obj.data=eye.data.copy();bpy.context.collection.objects.link(obj)
    bm=bmesh.new();bm.from_mesh(obj.data)
    bmesh.ops.delete(bm,geom=[v for v in bm.verts if sign*canonical(obj.matrix_world@v.co).y<0],context='VERTS')
    bm.to_mesh(obj.data);bm.free();meshes[name]=obj;parts[name]=([name],None)
report=[]
for name,(sources,pivot) in parts.items():
    vertices=[];faces=[];mats=[]
    for src in sources:
        obj=meshes[src];start=len(vertices)
        vertices.extend(canonical(obj.matrix_world@v.co) for v in obj.data.vertices)
        # Mirroring Blender Y changes handedness; reverse winding to retain outward faces.
        faces.extend(tuple(start+i for i in reversed(p.vertices)) for p in obj.data.polygons)
        mats.extend(p.material_index for p in obj.data.polygons)
    if pivot is None:pivot=tuple((min(v[i] for v in vertices)+max(v[i] for v in vertices))*.5 for i in range(3))
    pivot=Vector(pivot)
    # Geometry is relative to the mechanical pivot, in metres. UE import converts to cm.
    coords=[(v.x-pivot.x,-(v.y-pivot.y),v.z-pivot.z) for v in vertices]
    mesh=bpy.data.meshes.new('SM_Robot_'+name);mesh.from_pydata(coords,[],faces);mesh.update()
    obj=bpy.data.objects.new(mesh.name,mesh);bpy.context.collection.objects.link(obj)
    for matname,color in [('RobotIvory',(.69,.49,.26,1)),('RobotGraphite',(.026,.031,.037,1))]:
        m=bpy.data.materials.get(matname) or bpy.data.materials.new(matname);m.diffuse_color=color;mesh.materials.append(m)
    for p,index in zip(mesh.polygons,mats):p.material_index=1 if name.startswith(('Eye','Brow')) else min(index,1)
    bpy.ops.object.select_all(action='DESELECT');obj.select_set(True);bpy.context.view_layer.objects.active=obj
    bpy.ops.export_scene.fbx(filepath=str(out/(mesh.name+'.fbx')),use_selection=True,object_types={'MESH'},axis_forward='-Y',axis_up='Z',bake_anim=False,add_leaf_bones=False,path_mode='STRIP',mesh_smooth_type='FACE')
    report.append(dict(name=name,mesh=mesh.name,pivot=[round(x*100,4) for x in pivot],vertices=len(vertices)))
(out/'parts.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
