import bpy,runpy,json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'Saved/Validation/MainGameRoutesV6'
helpers=runpy.run_path(str(ROOT/'Art/Mining/TunnelCutter/validate_tunnel.py'),run_name='helpers')
report=[]
for path in OUT.glob('failed-*.obj'):
    verts=[];faces=[]
    for line in path.read_text().splitlines():
        p=line.split()
        if p[0]=='v':verts.append(tuple(map(float,p[1:])))
        if p[0]=='f':faces.append(tuple(int(x)-1 for x in p[1:]))
    mesh=bpy.data.meshes.new(path.stem);mesh.from_pydata(verts,[],faces);mesh.update()
    item=helpers['topology'](mesh);item.update(helpers['intersections'](mesh));item['file']=path.name;report.append(item)
    print(path.name,json.dumps(item),flush=True)
(OUT/'source-diagnosis.json').write_text(json.dumps(report,indent=2))
