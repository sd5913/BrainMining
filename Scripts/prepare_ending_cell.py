"""Blender 3.2: create ending-only reduced meshes and a shared diffuse atlas.
Source files in Art/Ending/CellModels are read-only; outputs go to SourceAssets.+"""
import bpy, math, json
import numpy as np
from pathlib import Path
from mathutils import Vector, Matrix
root=Path(__file__).resolve().parents[1]
source=root/'Art/Ending/CellModels'
out=root/'SourceAssets/EndingEmergence/Cell';out.mkdir(parents=True,exist_ok=True)
bpy.ops.wm.open_mainfile(filepath=str(next(source.glob('*.blend'))))
objects=[o for o in bpy.data.objects if o.type=='MESH']
materials=list({m.name:m for o in objects for m in o.data.materials if m}.values())
tile=512;grid=math.ceil(math.sqrt(len(materials)));size=tile*grid
pixels=np.ones((size,size,4),dtype=np.float32)
for i,m in enumerate(materials):
    bs=next((n for n in m.node_tree.nodes if n.type=='BSDF_PRINCIPLED'),None) if m.use_nodes else None
    color=tuple(bs.inputs['Base Color'].default_value) if bs else tuple(m.diffuse_color)
    tex=next((n.image for n in m.node_tree.nodes if n.type=='TEX_IMAGE' and n.image and 'Base_Color' in n.image.name),None) if m.use_nodes else None
    patch=np.empty((tile,tile,4),dtype=np.float32);patch[:]=color
    if tex:
        path=source/'tex'/tex.name
        if path.exists():
            im=bpy.data.images.load(str(path),check_existing=False);im.scale(tile-8,tile-8)
            data=np.array(im.pixels[:],dtype=np.float32).reshape(tile-8,tile-8,4)
            patch=np.pad(data,((4,4),(4,4),(0,0)),mode='edge');bpy.data.images.remove(im)
    patch[:,:,3]=1
    x=(i%grid)*tile;y=(i//grid)*tile;pixels[y:y+tile,x:x+tile]=patch
atlas=bpy.data.images.new('T_CellAtlas',size,size,alpha=False)
atlas.pixels.foreach_set(pixels.reshape(-1));atlas.filepath_raw=str(out/'T_CellAtlas.png');atlas.file_format='PNG';atlas.save()
shared=bpy.data.materials.new('CellAtlas');shared.use_nodes=True
tex=bpy.data.textures.new('CellAtlas','IMAGE');tex.image=atlas
node=shared.node_tree.nodes.new('ShaderNodeTexImage');node.image=atlas
shared.node_tree.links.new(node.outputs['Color'],shared.node_tree.nodes.get('Principled BSDF').inputs['Base Color'])
for o in objects:
    uv=o.data.uv_layers.active or o.data.uv_layers.new()
    for p in o.data.polygons:
        m=o.data.materials[p.material_index];i=materials.index(m)
        for k in p.loop_indices:
            v=uv.data[k].uv.copy()
            uv.data[k].uv=((i%grid+(4+v.x*(tile-8))/tile)/grid,(i//grid+(4+v.y*(tile-8))/tile)/grid)
        p.material_index=0
    o.data.materials.clear();o.data.materials.append(shared)
stats={}
def export(name,chosen,target,normalize=True):
    bpy.ops.object.select_all(action='DESELECT');copies=[]
    for o in chosen:
        c=o.copy();c.data=o.data.copy();bpy.context.collection.objects.link(c);copies.append(c);c.select_set(True)
    bpy.context.view_layer.objects.active=copies[0];bpy.ops.object.join();o=bpy.context.object;o.name=name
    bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
    tris=sum(len(p.vertices)-2 for p in o.data.polygons)
    dec=o.modifiers.new('EndingReduction','DECIMATE');dec.ratio=min(1,target/tris)
    bpy.ops.object.modifier_apply(modifier=dec.name)
    # Center and normalize to a 100-unit diameter; open cutaway faces -X.
    box=[Vector(v) for v in o.bound_box];lo=Vector([min(v[i] for v in box) for i in range(3)]);hi=Vector([max(v[i] for v in box) for i in range(3)])
    center=(lo+hi)/2;scale=100/max(hi-lo)
    rot=Matrix.Rotation(-math.pi/2,4,'Z')
    for v in o.data.vertices:v.co=rot@((v.co-center)*scale)
    o.data.update()
    bpy.ops.export_scene.fbx(filepath=str(out/(name+'.fbx')),use_selection=True,object_types={'MESH'},add_leaf_bones=False,bake_anim=False,axis_forward='-Y',axis_up='Z',global_scale=1,apply_unit_scale=True,bake_space_transform=False,path_mode='STRIP')
    stats[name]=dict(triangles=sum(len(p.vertices)-2 for p in o.data.polygons),materials=len(o.data.materials))
    bpy.data.objects.remove(o,do_unlink=True)
export('SM_CellHero',objects,65000)
export('SM_CellDistant',objects,4500)
export('SM_CellMitochondria',[o for o in objects if o.name=='33'],2500)
export('SM_CellReticulum',[o for o in objects if o.name in ['14','25']],6500)
export('SM_CellNucleus',[o for o in objects if o.name in ['110','154','286','297','308','99','242']],7000)
(out/'preparation.json').write_text(json.dumps(stats,indent=2),encoding='utf8')
print('ENDING_CELL_PREPARED',stats)
