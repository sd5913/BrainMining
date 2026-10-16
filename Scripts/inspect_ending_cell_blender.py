import bpy, json
from pathlib import Path
from mathutils import Vector
root=Path(__file__).resolve().parents[1]
source=next((root/'Art/Ending/CellModels').glob('*.blend'))
bpy.ops.wm.open_mainfile(filepath=str(source))
rows=[]
for o in bpy.data.objects:
    if o.type!='MESH': continue
    box=[o.matrix_world@Vector(v) for v in o.bound_box]
    rows.append(dict(name=o.name,verts=len(o.data.vertices),faces=len(o.data.polygons),
        min=[min(v[i] for v in box) for i in range(3)],max=[max(v[i] for v in box) for i in range(3)],
        materials=[m.name if m else '' for m in o.data.materials],hidden=o.hide_render,
        modifiers=[m.type for m in o.modifiers]))
out=root/'Saved/CellModelInspection.json';out.parent.mkdir(exist_ok=True)
out.write_text(json.dumps(dict(objects=rows,images=[dict(name=i.name,path=i.filepath) for i in bpy.data.images]),ensure_ascii=False,indent=2),encoding='utf8')
print('CELL_INSPECTION',out)
