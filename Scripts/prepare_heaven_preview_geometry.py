"""Small deterministic meshes for the isolated heaven preview, in centimeters."""
from pathlib import Path
import math,json
root=Path(__file__).resolve().parents[1]/'Art/HeavenPreview';root.mkdir(parents=True,exist_ok=True)
vertices=[];faces=[];uvs=[];normals=[]
# Thin torus, normal along X. OBJ import orientation is measured before placement.
major=80.;minor=1.25;steps=96;tube=8
for i in range(steps):
 a=2*math.pi*i/steps
 for j in range(tube):
  b=2*math.pi*j/tube;r=major+minor*math.cos(b);vertices.append((minor*math.sin(b),r*math.cos(a),r*math.sin(a)));uvs.append((i/steps,j/tube));normals.append((math.sin(b),math.cos(b)*math.cos(a),math.cos(b)*math.sin(a)))
for i in range(steps):
 for j in range(tube):faces.append((i*tube+j,(i+1)%steps*tube+j,(i+1)%steps*tube+(j+1)%tube,i*tube+(j+1)%tube))
with (root/'Halo.obj').open('w',encoding='utf8') as f:
 f.write('o Halo\ns 1\n')
 for v in vertices:f.write('v %.6f %.6f %.6f\n'%v)
 for uv in uvs:f.write('vt %.6f %.6f\n'%uv)
 for n in normals:f.write('vn %.6f %.6f %.6f\n'%n)
 for p in faces:
  for tri in [(p[0],p[1],p[2]),(p[0],p[2],p[3])]:f.write('f '+' '.join(f'{i+1}/{i+1}/{i+1}' for i in tri)+'\n')
(root/'manifest.json').write_text(json.dumps({'units':'centimeters','halo_radius':major,'halo_vertices':len(vertices),'halo_faces':len(faces)},indent=2),encoding='utf8')
print('HEAVEN_GEOMETRY',len(vertices),len(faces))
