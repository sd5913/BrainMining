"""Author rounded, sewn plush parts in centimeters; compatible with native cat animation."""
import bpy, math
from pathlib import Path
from mathutils import Vector
OUT=Path(__file__).resolve().parents[1]/'SourceAssets/CountrysideP2/Cat'
OUT.mkdir(parents=True,exist_ok=True)
bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
bpy.context.scene.unit_settings.system='METRIC';bpy.context.scene.unit_settings.scale_length=.01
materials=[]
for name,col in [('Fur',(.64,.33,.12,1)),('Seam',(.2,.08,.025,1)),('InnerEar',(.58,.22,.17,1))]:
 m=bpy.data.materials.new(name);m.diffuse_color=col;materials.append(m)
def smooth(o):
 for p in o.data.polygons:p.use_smooth=True
 return o
def sphere(name,scale=(50,50,50),pos=(0,0,0),slot=0):
 bpy.ops.mesh.primitive_uv_sphere_add(segments=64,ring_count=40,radius=1,location=pos)
 o=bpy.context.object;o.name=name;o.scale=scale;bpy.ops.object.transform_apply(location=False,rotation=False,scale=True);o.data.materials.append(materials[slot]);return smooth(o)
def curve(name,points,radius=.32,slot=1):
 c=bpy.data.curves.new(name,'CURVE');c.dimensions='3D';c.bevel_depth=radius;c.bevel_resolution=3
 s=c.splines.new('POLY');s.points.add(len(points)-1)
 for p,co in zip(s.points,points):p.co=(*co,1)
 o=bpy.data.objects.new(name,c);bpy.context.collection.objects.link(o);o.data.materials.append(materials[slot]);return o
def stitches(orientation='body'):
 # Small thread bars straddle the side seam. Visible at miniature player scale.
 obs=[]
 for i in range(31):
  a=-1.2+i*2.4/30
  y=50*math.cos(a);z=50*math.sin(a)
  obs.append(curve('Thread',[(x,y*1.008,z*1.008) for x in [-1.1,0,1.1]],.22))
 return obs
def export(name,obs):
 bpy.ops.object.select_all(action='DESELECT')
 for o in obs:o.select_set(True)
 bpy.context.view_layer.objects.active=obs[0]
 for o in list(obs):
  if o.type=='CURVE':bpy.context.view_layer.objects.active=o;bpy.ops.object.convert(target='MESH')
 bpy.context.view_layer.objects.active=obs[0];bpy.ops.object.join();o=bpy.context.object;o.name=name
 # Guarantee the same compact unit bounds used by native component transforms.
 bpy.ops.object.transform_apply(location=False,rotation=False,scale=True)
 bpy.ops.export_scene.fbx(filepath=str(OUT/(name+'.fbx')),use_selection=True,global_scale=1,apply_unit_scale=True,apply_scale_options='FBX_SCALE_NONE',axis_forward='-Y',axis_up='Z',object_types={'MESH'},mesh_smooth_type='FACE',use_mesh_modifiers=True,add_leaf_bones=False,bake_anim=False)
 bpy.data.objects.remove(o,do_unlink=True)
body=sphere('PearBody')
for v in body.data.vertices:
 z=v.co.z/50;f=1-.16*z-.06*z*z;v.co.x*=f;v.co.y*=f
export('SM_PlushBody',[body]+stitches())
head=sphere('SculptedHead')
for v in head.data.vertices:
 z=v.co.z/50;v.co.y*=1+.08*max(0,-z);v.co.x*=1+.04*max(0,-z)
export('SM_PlushHead',[head]+stitches('head'))
# Ear is a thick rounded triangular pouch rather than a cone.
verts=[(-24,-45,-45),(24,-45,-45),(-20,45,-45),(20,45,-45),(-4,8,52),(4,8,52)]
faces=[(0,1,3,2),(0,4,5,1),(2,3,5,4),(0,2,4),(1,5,3)]
me=bpy.data.meshes.new('Ear');me.from_pydata(verts,[],faces);me.update();ear=bpy.data.objects.new('Ear',me);bpy.context.collection.objects.link(ear);ear.data.materials.append(materials[0]);bpy.context.view_layer.objects.active=ear;ear.select_set(True)
bevel=ear.modifiers.new('Soft sewn edge','BEVEL');bevel.width=9;bevel.segments=5
bpy.ops.object.modifier_apply(modifier=bevel.name);smooth(ear)
inner=sphere('InnerVelvet',(3.2,26,30),(-22,1,-4),2)
edge=curve('EarSeam',[(-22,-33,-33),(-19,-13,4),(-9,5,39),(-18,23,4),(-20,33,-33)],.65)
export('SM_PlushEar',[ear,inner,edge])
paw=sphere('Mitten')
export('SM_PlushPaw',[paw]+[curve('Finger seam',[(x,y,math.sqrt(max(0,2500-x*x-y*y))+.2) for x in range(-22,23,2)],.45) for y in [-13,13]])
export('SM_PlushMuzzle',[sphere('Muzzle')]+stitches())
# Ribbon shaped bow with folded cloth surfaces, centered on the original bow anchor.
left=sphere('BowLeft',(42,48,48),(0,-28,0));right=sphere('BowRight',(42,48,48),(0,28,0));knot=sphere('BowKnot',(49,17,30),(-2,0,0))
export('SM_PlushBow',[left,right,knot])
eye=sphere('GlossButton')
export('SM_PlushButton',[eye])
print('COUNTRYSIDE_PLUSH_FBX_READY',OUT)
