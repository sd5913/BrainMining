"""Build and independently validate the widened Lowpoly tunnel in background Blender."""
import bpy,bmesh,json,math,runpy
from pathlib import Path
import numpy as np
from mathutils import Vector,noise

OUTPUT=Path(__file__).parent
NAME='SM_DescendingTunnel_Wide_Lowpoly'
SCENE='Descending_Tunnel_Wide_V2'

def enum_value(owner,prop,value):
    choices=[i.identifier for i in owner.bl_rna.properties[prop].enum_items]
    assert value in choices,(prop,value,choices)
    setattr(owner,prop,value)

def tube(path):
    p=np.array(path['points']);a=np.array(path['half_width']);b=np.array(path['half_height'])
    t=np.gradient(p,axis=0);t/=np.linalg.norm(t,axis=1)[:,None]
    previous=Vector(t[0]);normal=previous.cross(Vector((0,0,1))).normalized();frames=[]
    for tangent in t:
        current=Vector(tangent);normal=previous.rotation_difference(current)@normal
        normal=(normal-current*normal.dot(current)).normalized()
        frames.append((normal.copy(),current.cross(normal).normalized()));previous=current
    sides=40;rings=[]
    def ring(center,aa,bb,f):
        aa,bb=float(aa),float(bb)
        return [tuple(Vector(center)+aa*math.cos(2*math.pi*j/sides)*f[0]+bb*math.sin(2*math.pi*j/sides)*f[1]) for j in range(sides)]
    cap0=min(a[0],b[0]);cap1=min(a[-1],b[-1])
    for theta in np.linspace(-math.pi/2,0,9)[1:]:
        rings.append(ring(p[0]+t[0]*cap0*math.sin(theta),a[0]*math.cos(theta),b[0]*math.cos(theta),frames[0]))
    for i in range(1,len(p)):rings.append(ring(p[i],a[i],b[i],frames[i]))
    for theta in np.linspace(0,math.pi/2,9)[1:-1]:
        rings.append(ring(p[-1]+t[-1]*cap1*math.sin(theta),a[-1]*math.cos(theta),b[-1]*math.cos(theta),frames[-1]))
    vertices=[v for r in rings for v in r];faces=[]
    for k in range(len(rings)-1):
        for j in range(sides):
            i=k*sides+j;n=k*sides+(j+1)%sides;faces.append((i,n,n+sides,i+sides))
    vertices.extend([tuple(p[0]-t[0]*cap0),tuple(p[-1]+t[-1]*cap1)])
    for j in range(sides):
        faces.append((len(vertices)-2,(j+1)%sides,j));i=(len(rings)-1)*sides
        faces.append((len(vertices)-1,i+j,i+(j+1)%sides))
    mesh=bpy.data.meshes.new(path['name']);mesh.from_pydata(vertices,[],faces);mesh.update()
    bm=bmesh.new();bm.from_mesh(mesh);bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces));bm.to_mesh(mesh);bm.free()
    obj=bpy.data.objects.new(path['name'],mesh);bpy.context.scene.collection.objects.link(obj);return obj

def materials(obj):
    obj.data.materials.clear()
    for i,color in enumerate([(.31,.265,.215),(.365,.31,.245),(.42,.355,.28),(.475,.40,.315),(.53,.445,.35)]):
        mat=bpy.data.materials.new(f'Wide_Rock_Facet_{i+1:02}');mat.diffuse_color=(*color,1);mat.use_nodes=True
        node=next(n for n in mat.node_tree.nodes if n.type=='BSDF_PRINCIPLED')
        node.inputs['Base Color'].default_value=(*color,1);node.inputs['Roughness'].default_value=1
        obj.data.materials.append(mat)
    for poly in obj.data.polygons:
        poly.use_smooth=False;tone=noise.noise(poly.center/5+Vector((2.4,-3.7,5.3)))
        poly.material_index=max(0,min(4,int((tone+1)*2.5)))

def build():
    paths=json.loads((OUTPUT/'centerlines.json').read_text())
    scene=bpy.data.scenes.new(SCENE);bpy.context.window.scene=scene
    enum_value(scene.unit_settings,'system','METRIC');scene.unit_settings.scale_length=1
    objects=[tube(path) for path in paths]
    bpy.ops.object.select_all(action='DESELECT')
    for obj in objects:obj.select_set(True)
    bpy.context.view_layer.objects.active=objects[0];bpy.ops.object.join();obj=bpy.context.object;obj.name=NAME
    remesh=obj.modifiers.new('Union_and_Seal_Branches','REMESH');enum_value(remesh,'mode','VOXEL');remesh.voxel_size=.38
    bpy.ops.object.modifier_apply(modifier=remesh.name)
    print('STAGE voxel union '+str(len(obj.data.polygons)),flush=True)
    smooth=obj.modifiers.new('Continuous_Junctions','SMOOTH');smooth.factor=.8;smooth.iterations=6
    bpy.ops.object.modifier_apply(modifier=smooth.name);obj.data.update()
    normals=[v.normal.copy() for v in obj.data.vertices]
    for v,n in zip(obj.data.vertices,normals):
        delta=.48*noise.noise(v.co/4.2+Vector((5.19,-8.73,1.37)))+.14*noise.noise(v.co/1.7+Vector((-6.42,4.21,9.56)))
        v.co+=n*max(-.4,min(.4,delta))
    obj.data.update();mod=obj.modifiers.new('Irregular_Lowpoly_Rock','DECIMATE')
    enum_value(mod,'decimate_type','COLLAPSE');mod.ratio=.065;mod.use_collapse_triangulate=True
    bpy.ops.object.modifier_apply(modifier=mod.name)
    bm=bmesh.new();bm.from_mesh(obj.data);bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces));bm.to_mesh(obj.data);bm.free()
    obj.data.name=NAME+'_Mesh';materials(obj)
    obj['purpose']='Closed solid negative volume for tunnel subtraction; no wall shell'
    obj['branch_count']=7;obj['centerlines_strictly_descending']=True
    obj['nominal_old_width_m']=5.1;obj['front_view']='Looking along +Y from -Y; X right, Z up'
    obj['shared_export_origin']=[0.,0.,0.]
    helpers=runpy.run_path(str(OUTPUT.parent/'TunnelCutter/validate_tunnel.py'),run_name='helpers')
    report=helpers['topology'](obj.data);report.update(helpers['intersections'](obj.data))
    report.update(json.loads((OUTPUT/'plan_summary.json').read_text()))
    report['dimensions_m']=list(obj.dimensions);report['all_centerlines_strictly_descending']=all(np.all(np.diff(np.array(p['points'])[:,2])<0) for p in paths)
    report['passed']=report['connected_components']==1 and report['euler_characteristic']==2 and report['signed_volume_m3']>0 and report['all_centerlines_strictly_descending'] and all(report[k]==0 for k in ('boundary_edges','nonmanifold_edges','inconsistent_winding_edges','degenerate_faces','duplicate_faces','self_intersection_pairs'))
    (OUTPUT/'tunnel_validation.json').write_text(json.dumps(report,indent=2));print(json.dumps(report),flush=True);assert report['passed'],report
    bpy.ops.wm.save_as_mainfile(filepath=str(OUTPUT/'Tunnel_Wide_V2.blend'))

if __name__=='__main__':build()
