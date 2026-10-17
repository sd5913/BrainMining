import unreal
from pathlib import Path
names=[(unreal.GeometryScript_MeshEdits,'delete_triangles_from_mesh'),(unreal.GeometryScript_List,'convert_array_to_index_list'),(unreal.GeometryScript_MeshRepair,'fill_all_mesh_holes'),(unreal.GeometryScript_MeshTransforms,'transform_mesh')]
s=[]
for c,n in names:
 s.append(n+'\n'+str(getattr(c,n,None).__doc__))
s.append(str([x for x in dir(unreal.GeometryScript_MeshEdits) if 'triangle' in x or 'delete' in x]))
Path(unreal.Paths.project_dir()+'Saved/Validation/P2Repair/geometry_api.txt').write_text('\n'.join(s))
