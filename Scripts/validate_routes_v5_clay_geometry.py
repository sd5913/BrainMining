import bpy,json,runpy
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];folder=ROOT/'Art/MainGame/RoutesV5/ClayRelief'
helpers=runpy.run_path(str(ROOT/'Art/Mining/TunnelCutter/validate_tunnel.py'),run_name='validation_helpers')
report={'passed':True,'meshes':[]}
for path in sorted(folder.glob('*.obj')):
    vertices=[];triangles=[]
    for line in path.read_text().splitlines():
        p=line.split()
        if p[0]=='v':vertices.append(tuple(float(x) for x in p[1:]))
        elif p[0]=='f':triangles.append(tuple(int(x)-1 for x in p[1:]))
    mesh=bpy.data.meshes.new(path.stem);mesh.from_pydata(vertices,[],triangles);mesh.update()
    result=helpers['topology'](mesh);result.update(helpers['intersections'](mesh));result['file']=path.name
    result['passed']=result['signed_volume_m3']<0 and all(result[k]==0 for k in ['boundary_edges','nonmanifold_edges','inconsistent_winding_edges','degenerate_faces','duplicate_faces','self_intersection_pairs'])
    report['passed'] &= result['passed'];report['meshes'].append(result);bpy.data.meshes.remove(mesh)
    print(path.name,result['passed'],result['self_intersection_pairs'],flush=True)
(folder/'geometry_validation.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
assert report['passed']
