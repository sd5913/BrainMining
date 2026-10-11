"""20 curved, closed, low-poly plugs; actual combined volume targets half the original tunnel."""
import bpy,bmesh,json,math,runpy,time,traceback
import numpy as np
from pathlib import Path
from mathutils import Vector,noise
from mathutils.bvhtree import BVHTree

OUT=Path(__file__).parent
SOURCE=OUT.parent/'TunnelWideV2'
SCENE='Tunnel_Fill50_V3'
NAME='SM_DescendingTunnel_Wide_Lowpoly'
SEED=20261011
paths=json.loads((SOURCE/'centerlines.json').read_text())
validation=runpy.run_path(str(OUT.parent/'TunnelCutter/validate_tunnel.py'),run_name='validation_helpers')
old=runpy.run_path(str(OUT.parent/'TunnelBlockers/generate_blockers.py'),run_name='blocker_helpers')
status={'phase':'planning','passed':False}

def checkpoint(**kwargs):
    status.update(kwargs);(OUT/'progress.json').write_text(json.dumps(status,indent=2),encoding='utf-8')
    print(json.dumps(kwargs),flush=True)

def prepare_paths():
    for path in paths:
        p=np.array(path['points']);t=np.gradient(p,axis=0);t/=np.linalg.norm(t,axis=1)[:,None]
        previous=Vector(t[0]);normal=previous.cross(Vector((0,0,1))).normalized();frames=[]
        for tangent in t:
            current=Vector(tangent);normal=previous.rotation_difference(current)@normal
            normal=(normal-current*normal.dot(current)).normalized()
            frames.append((list(normal),list(current.cross(normal).normalized())));previous=current
        path['_frames']=np.array(frames);path['_tangents']=t
        path['_area']=math.pi*np.array(path['half_width'])*np.array(path['half_height'])

def frame(path,s):
    arcs=path['arc']
    interp=lambda arr:Vector([float(np.interp(s,arcs,np.array(arr)[:,i])) for i in range(3)])
    center=interp(path['points']);tangent=interp(path['_tangents']).normalized()
    normal=interp(path['_frames'][:,0,:]);normal=(normal-tangent*normal.dot(tangent)).normalized()
    return center,normal,tangent.cross(normal).normalized(),tangent

def volume_estimate(path,a,b):
    ss=np.linspace(a,b,max(3,math.ceil((b-a)/.35)))
    areas=np.interp(ss,path['arc'],path['_area'])
    return float(np.sum((areas[:-1]+areas[1:])*.5*np.diff(ss)))

def plan():
    main=paths[0];end=float(main['arc'][-1]);roots=sorted(p['root_arc'] for p in paths[1:])
    bounds=[(8,roots[0]-9)]
    bounds.extend((a+9,b-9) for a,b in zip(roots[:-1],roots[1:]))
    bounds.append((roots[-1]+9,end-16))
    # Keep a visible air pocket between every pair of plugs, including at junctions.
    while len(bounds)<13:
        candidates=[(volume_estimate(main,a,b),i) for i,(a,b) in enumerate(bounds) if b-a>30]
        _,i=max(candidates);a,b=bounds.pop(i);middle=(a+b)*.5
        bounds[i:i]=[(a,middle-3),(middle+3,b)]
    records=[dict(path_index=0,path_name=main['name'],allowed_start_m=float(a),allowed_end_m=float(b)) for a,b in bounds]
    for i,path in enumerate(paths[1:],1):
        records.append(dict(path_index=i,path_name=path['name'],allowed_start_m=18.,allowed_end_m=float(path['arc'][-1])-12.))
    records.sort(key=lambda r:(r['path_index'],r['allowed_start_m']))
    assert len(records)==20 and all(r['allowed_end_m']-r['allowed_start_m']>10 for r in records)
    for i,r in enumerate(records,1):r.update(name=f'SM_Fill50_Blocker_{i:02}',seed=SEED+i*191)
    return records

def interval(r,factor):
    mid=(r['allowed_start_m']+r['allowed_end_m'])*.5;half=(r['allowed_end_m']-r['allowed_start_m'])*factor*.5
    return mid-half,mid+half

def remove(obj):
    mesh=obj.data;bpy.data.objects.remove(obj,do_unlink=True)
    if mesh.users==0:bpy.data.meshes.remove(mesh)

def envelope(record,factor):
    path=paths[record['path_index']];a,b=interval(record,factor)
    samples=np.linspace(a,b,max(3,math.ceil((b-a)/1.2)+1));sides=40;verts=[];faces=[]
    def relief(x,y,end):
        return .5*noise.noise(Vector((x/2.8,y/2.8,end*5+record['seed']*.003)))
    for k,s in enumerate(samples):
        center,u,v,t=frame(path,s)
        aa=float(np.interp(s,path['arc'],path['half_width']))+1.25
        bb=float(np.interp(s,path['arc'],path['half_height']))+1.25
        for j in range(sides):
            theta=j*2*math.pi/sides;x=aa*math.cos(theta);y=bb*math.sin(theta)
            z=relief(x,y,k==len(samples)-1) if k in (0,len(samples)-1) else 0
            verts.append(tuple(center+x*u+y*v+z*t))
    for k in range(len(samples)-1):
        for j in range(sides):
            p=k*sides+j;q=k*sides+(j+1)%sides;faces.append((p,q,q+sides,p+sides))
    for end,k,s in [(0,0,a),(1,len(samples)-1,b)]:
        center,u,v,t=frame(path,s);outer=list(range(k*sides,(k+1)*sides))
        aa=float(np.interp(s,path['arc'],path['half_width']))+1.25
        bb=float(np.interp(s,path['arc'],path['half_height']))+1.25
        for frac in (.72,.43,.19):
            inner=[]
            for j in range(sides):
                theta=j*2*math.pi/sides;x=aa*frac*math.cos(theta);y=bb*frac*math.sin(theta)
                inner.append(len(verts));verts.append(tuple(center+x*u+y*v+relief(x,y,end)*t))
            for j in range(sides):
                n=(j+1)%sides;faces.extend([(outer[j],outer[n],inner[n]),(outer[j],inner[n],inner[j])])
            outer=inner
        c=len(verts);verts.append(tuple(center+relief(0,0,end)*t))
        for j in range(sides):faces.append((outer[j],outer[(j+1)%sides],c))
    mesh=bpy.data.meshes.new(record['name']+'_Envelope');mesh.from_pydata(verts,[],faces);mesh.update()
    bm=bmesh.new();bm.from_mesh(mesh);bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces));bm.to_mesh(mesh);bm.free()
    obj=bpy.data.objects.new(record['name'],mesh);bpy.context.scene.collection.objects.link(obj);return obj

def generate(records,factor):
    blocks=[]
    for number,r in enumerate(records,1):
        obj=envelope(r,factor);bpy.ops.object.select_all(action='DESELECT');obj.select_set(True);bpy.context.view_layer.objects.active=obj
        boolean=obj.modifiers.new('Exact_Tunnel_Interior_Fit','BOOLEAN');boolean.operation='INTERSECT';boolean.solver='EXACT';boolean.object=tunnel
        bpy.ops.object.modifier_apply(modifier=boolean.name)
        bm=bmesh.new();bm.from_mesh(obj.data)
        bmesh.ops.dissolve_degenerate(bm,dist=1e-5,edges=list(bm.edges))
        bmesh.ops.triangulate(bm,faces=list(bm.faces));bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces));bm.to_mesh(obj.data);bm.free()
        topology=validation['topology'](obj.data)
        assert topology['connected_components']==1 and topology['boundary_edges']==0 and topology['nonmanifold_edges']==0,(r['name'],topology)
        obj.data.materials.clear()
        for material in palette:obj.data.materials.append(material)
        for polygon in obj.data.polygons:
            polygon.use_smooth=False;polygon.material_index=max(0,min(4,int((noise.noise(polygon.center/3.2)+1)*2.5)))
        a,b=interval(r,factor);r.update(start_arc_m=a,end_arc_m=b,arc_length_m=b-a,center_m=list(frame(paths[r['path_index']],(a+b)*.5)[0]))
        for prop,value in [('purpose','Full-section curved diggable guide; ends lead to air pockets'),('path_name',r['path_name']),('start_arc_m',a),('end_arc_m',b),('shared_origin_with_tunnel',True)]:obj[prop]=value
        blocks.append(obj);checkpoint(phase='generating',completed=number,factor=factor)
    return blocks

def bvh(obj):
    obj.data.calc_loop_triangles();return BVHTree.FromPolygons([v.co[:] for v in obj.data.vertices],[tuple(t.vertices) for t in obj.data.loop_triangles],all_triangles=True)

def validate(blocks,records,total,factor):
    reports=[];trees=[bvh(ob) for ob in blocks];tree=bvh(tunnel)
    for ob,r,btree in zip(blocks,records,trees):
        top=validation['topology'](ob.data);top.update(validation['intersections'](ob.data))
        assert all(top[k]==0 for k in ('boundary_edges','nonmanifold_edges','inconsistent_winding_edges','degenerate_faces','duplicate_faces','self_intersection_pairs')),(ob.name,top)
        assert top['connected_components']==1 and top['signed_volume_m3']>0,(ob.name,top)
        path=paths[r['path_index']];max_error=0.
        # Sample the full length, not just a single midpoint, to prove no lateral bypass gaps.
        # At a curved/rough end the cut intentionally feathers into the air pocket;
        # test the full interior section away from that transition, plus the seal Boolean below.
        margin=min(r['arc_length_m']*.25, max(4.,max(path['half_width'])*.65))
        for s in np.linspace(r['start_arc_m']+margin,r['end_arc_m']-margin,9):
            center,u,v,_=frame(path,s)
            for angle in np.linspace(0,2*math.pi,32,endpoint=False):
                direction=math.cos(angle)*u+math.sin(angle)*v
                actual=tree.ray_cast(center,direction,30);plug=btree.ray_cast(center,direction,30)
                assert actual[0] is not None and plug[0] is not None,(ob.name,s,angle)
                error=(actual[0]-plug[0]).length;max_error=max(error,max_error)
                assert error<.003,(ob.name,s,error)
        top.update(name=ob.name,arc_length_m=r['arc_length_m'],volume_m3=top['signed_volume_m3'],max_wall_fit_error_m=max_error)
        reports.append(top);checkpoint(phase='validating',completed=len(reports))
    terminal=[]
    for path in paths:
        point=Vector(path['points'][-1]);nearest=min(t.find_nearest(point)[3] for t in trees)
        assert nearest>8.,(path['name'],'terminal blocked',nearest)
        terminal.append(dict(path_name=path['name'],free_centerline_tail_m=float(path['arc'][-1])-max(r['end_arc_m'] for r in records if r['path_name']==path['name']),nearest_blocker_distance_m=nearest))
    # T - (T intersect E) equals T - E. Subtract the original envelopes to avoid
    # coplanar tunnel-wall artifacts without inflating or changing the real plugs.
    clones=[envelope(record,factor) for record in records]
    bpy.ops.object.select_all(action='DESELECT')
    for ob in clones:ob.select_set(True)
    bpy.context.view_layer.objects.active=clones[0];bpy.ops.object.join();combined=bpy.context.object
    residual=bpy.data.objects.new('Residual_Seal_Test',tunnel.data.copy());scene.collection.objects.link(residual)
    bpy.context.view_layer.objects.active=residual
    boolean=residual.modifiers.new('Validate_20_Seals','BOOLEAN');boolean.operation='DIFFERENCE';boolean.solver='EXACT';boolean.object=combined
    bpy.ops.object.modifier_apply(modifier=boolean.name)
    residual_top=validation['topology'](residual.data);remove(combined);remove(residual)
    assert residual_top['connected_components']==21,residual_top
    report=dict(passed=True,count=20,tunnel_volume_m3=total,blocked_volume_m3=sum(r['volume_m3'] for r in reports),
                fill_fraction=sum(r['volume_m3'] for r in reports)/total,blockers=reports,terminals=terminal,
                residual_connected_components=21,total_triangles=sum(r['faces'] for r in reports),seal_test='Original envelopes; no expansion')
    (OUT/'validation.json').write_text(json.dumps(report,indent=2),encoding='utf-8');return report

def preview(blocks):
    scene.camera=None;tunnel.hide_render=True
    wire=bpy.data.objects.new('Preview_Tunnel_Wire',tunnel.data.copy());scene.collection.objects.link(wire)
    wire.data.materials.clear();mat=bpy.data.materials.new('Preview_Wire');mat.diffuse_color=(.12,.28,.36,1);wire.data.materials.append(mat)
    for face in wire.data.polygons:face.material_index=0
    dec=wire.modifiers.new('Wire_Density','DECIMATE');dec.ratio=.14
    wf=wire.modifiers.new('Tunnel_Wire','WIREFRAME');wf.thickness=.07;wf.use_replace=True
    data=bpy.data.cameras.new('Preview_Camera');camera=bpy.data.objects.new('Preview_Camera',data);scene.collection.objects.link(camera)
    bounds=[Vector(p) for p in tunnel.bound_box];center=sum(bounds,Vector())/8;eye=Vector((.8,-1,.6)).normalized()
    camera.location=center+eye*600;camera.rotation_euler=eye.to_track_quat('Z','Y').to_euler();data.type='ORTHO';data.ortho_scale=345;data.clip_end=2000;scene.camera=camera
    scene.render.engine='BLENDER_WORKBENCH';scene.render.resolution_x=1600;scene.render.resolution_y=1200;scene.render.resolution_percentage=100
    scene.render.image_settings.file_format='PNG';scene.render.filepath=str(OUT/'Tunnel_Fill50_Placement.png')
    shading=scene.display.shading;shading.light='STUDIO';shading.color_type='MATERIAL';shading.show_shadows=True;shading.show_cavity=True;shading.cavity_type='BOTH';shading.background_type='WORLD'
    scene.world=bpy.data.worlds.new('Fill50_Preview_World');scene.world.color=(.025,.035,.05)
    bpy.ops.render.render(write_still=True)
    remove(wire);bpy.data.objects.remove(camera,do_unlink=True);bpy.data.cameras.remove(data);scene.camera=None

try:
    OUT.mkdir(parents=True,exist_ok=True);prepare_paths();records=plan()
    source=bpy.data.objects[NAME]
    total=validation['topology'](source.data)['signed_volume_m3']
    scene=bpy.data.scenes.new(SCENE);scene.unit_settings.system='METRIC';scene.unit_settings.scale_length=1;bpy.context.window.scene=scene
    tunnel=bpy.data.objects.new('REFERENCE_Fill50_Tunnel',source.data);scene.collection.objects.link(tunnel);tunnel.hide_render=True;tunnel.hide_select=True;tunnel.display_type='WIRE'
    palette=old['materials']()
    def estimate(f):return sum(volume_estimate(paths[r['path_index']],*interval(r,f)) for r in records)
    checkpoint(estimated_capacity_fraction=estimate(1)/total)
    lo,hi=.2,1.
    for _ in range(28):
        mid=(lo+hi)*.5
        if estimate(mid)<total*.50:lo=mid
        else:hi=mid
    factor=(lo+hi)*.5
    for attempt in range(4):
        blocks=generate(records,factor);blocked=sum(validation['topology'](ob.data)['signed_volume_m3'] for ob in blocks)
        fraction=blocked/total;checkpoint(phase='volume_check',attempt=attempt,fill_fraction=fraction,factor=factor)
        if abs(fraction-.50)<.009:break
        assert not (factor>.999 and fraction<.485),'Insufficient interior space while keeping terminal pockets'
        for ob in blocks:remove(ob)
        factor=min(1.,factor*.50/fraction)
    assert abs(fraction-.5)<.015,fraction
    report=validate(blocks,records,total,factor)
    (OUT/'placement_plan.json').write_text(json.dumps(dict(seed=SEED,factor=factor,objects=records),indent=2),encoding='utf-8')
    bpy.ops.object.select_all(action='DESELECT')
    for ob in blocks:ob.select_set(True)
    bpy.context.view_layer.objects.active=blocks[0]
    bpy.ops.export_scene.fbx(filepath=str(OUT/'Tunnel_Fill50_Blockers_20.fbx'),use_selection=True,object_types={'MESH'},global_scale=1,
        apply_unit_scale=True,apply_scale_options='FBX_SCALE_UNITS',axis_forward='-Y',axis_up='Z',use_mesh_modifiers=True,mesh_smooth_type='FACE',use_custom_props=True,bake_anim=False)
    preview(blocks)
    bpy.ops.object.select_all(action='DESELECT')
    for ob in blocks:ob.select_set(True)
    bpy.context.view_layer.objects.active=blocks[0]
    for area in bpy.context.screen.areas:
        if area.type=='VIEW_3D':
            area.spaces.active.shading.type='SOLID';area.spaces.active.shading.color_type='MATERIAL';area.spaces.active.overlay.show_floor=False
            area.spaces.active.region_3d.view_location=sum((Vector(c) for c in tunnel.bound_box),Vector())/8
            area.spaces.active.region_3d.view_distance=300
            area.spaces.active.region_3d.view_rotation=Vector((.8,-1,.6)).to_track_quat('Z','Y');area.spaces.active.region_3d.view_perspective='ORTHO'
    bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'Tunnel_Fill50_V3.blend'))
    checkpoint(phase='complete',passed=True,fill_fraction=report['fill_fraction'],triangles=report['total_triangles'])
except Exception:
    checkpoint(phase='failed',error=traceback.format_exc());raise
