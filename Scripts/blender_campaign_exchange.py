"""Blender: build a complete spatial reference, or export selected terrain for UE.
Background: blender -b --python this.py -- build <exchange-directory>
Blender text editor: set MODE='export', ROOT below, run with selected editable rocks.
"""
import bpy, json, sys
from pathlib import Path
MODE='export'
ROOT=Path(__file__).resolve().parents[1]/'Art/Mining/FullJourneyExchange'
if '--' in sys.argv:
    args=sys.argv[sys.argv.index('--')+1:];MODE=args[0]
    if len(args)>1:ROOT=Path(args[1])

def read_obj(path):
    vertices=[];faces=[]
    for line in path.read_text(encoding='utf-8').splitlines():
        p=line.split()
        if p and p[0]=='v':vertices.append(tuple(float(x) for x in p[1:4]))
        elif p and p[0]=='f':faces.append(tuple(int(x)-1 for x in p[1:4]))
    return vertices,faces

if MODE=='build':
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene=bpy.context.scene;scene.unit_settings.system='METRIC'
    terrain=bpy.data.collections.new('EDIT_Terrain');scene.collection.children.link(terrain)
    reference=bpy.data.collections.new('REF_Rooms_and_Props');scene.collection.children.link(reference)
    reference.hide_select=True
    palette={}
    data=json.loads((ROOT/'Source/layout.json').read_text(encoding='utf-8'))
    # Group reference instances by source actor; thousands of office/cell props
    # do not need independent editable objects in a terrain workspace.
    batches={}
    for row in data['meshes']:
        key=row['id'] if row['editable_rock'] else row['actor']
        batches.setdefault(key,[]).append(row)
    for batch in batches.values():
        row=batch[0];v=[];f=[]
        for source in batch:
            points,faces=read_obj(ROOT/'Source'/source['file']);offset=len(v)
            v.extend(points);f.extend(tuple(i+offset for i in face) for face in faces)
        mesh=bpy.data.meshes.new(row['id']);mesh.from_pydata(v,[],f);mesh.update()
        obj=bpy.data.objects.new(row['label'],mesh)
        (terrain if row['editable_rock'] else reference).objects.link(obj)
        obj['ue_id']=row['id'];obj['ue_actor']=row['actor'];obj['ue_editable_rock']=row['editable_rock']
        key='Rock' if row['editable_rock'] else row['source_level']
        if key not in palette:
            mat=bpy.data.materials.new(key)
            mat.diffuse_color=(.21,.12,.065,1) if key=='Rock' else (.48,.58,.65,1)
            palette[key]=mat
        obj.data.materials.append(palette[key])
    # Open on the first mine and computer room; all other stages remain in the file.
    for screen in bpy.data.screens:
        for area in screen.areas:
            if area.type=='VIEW_3D':
                area.spaces.active.region_3d.view_location=(-120,0,38)
                area.spaces.active.region_3d.view_distance=110
                area.spaces.active.clip_end=10000
    bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'FullJourney_Editable.blend'),compress=True)
    print('CAMPAIGN_BLEND_SAVED',len(data['meshes']))
elif MODE=='export':
    output=ROOT/'Edited';output.mkdir(parents=True,exist_ok=True);rows=[]
    for obj in bpy.context.selected_objects:
        if not obj.get('ue_editable_rock',False):continue
        # Preserve closed solids. Do not weld separate diggable actors together.
        mesh=obj.data;mesh.calc_loop_triangles();verts=[obj.matrix_world@v.co for v in mesh.vertices]
        name=obj['ue_id']+'.obj'
        with (output/name).open('w',encoding='utf-8') as stream:
            for v in verts:stream.write('v %.8f %.8f %.8f\n'%tuple(v))
            mirrored=obj.matrix_world.to_3x3().determinant()<0
            for t in mesh.loop_triangles:
                indices=tuple(t.vertices)
                if mirrored:indices=(indices[0],indices[2],indices[1])
                stream.write('f %d %d %d\n'%tuple(i+1 for i in indices))
        rows.append(dict(actor=obj['ue_actor'],file=name))
    if not rows:raise RuntimeError('Select at least one object in EDIT_Terrain')
    (output/'edits.json').write_text(json.dumps(rows,indent=2),encoding='utf-8')
    print('CAMPAIGN_ROCKS_EXPORTED',len(rows))
