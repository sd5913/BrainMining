"""Rebuild the 64 fitted plugs, retaining route intervals and open terminal pockets."""
import bpy,bmesh,json,math,runpy,traceback,time,sys
import numpy as np
from pathlib import Path
from mathutils import Vector,noise
from mathutils.bvhtree import BVHTree
ROOT=Path(__file__).resolve().parents[3];BASE=ROOT/'Art/MainGame';DEST=BASE/'BlockersRockV2'
STAGES=[('TunnelV1','MainGame_Cutter_Validated.blend','SM_MainGame_Tunnel_Cutter'),
 ('TunnelCavernV2','MainGame_Cavern_Cutter_Validated.blend','SM_MainGame_Cavern_Cutter'),
 ('TunnelStalagmiteV3','MainGame_Stalagmite_Cutter_Validated.blend','SM_MainGame_Stalagmite_Cutter'),
 ('TunnelNeuralV4','MainGame_Neural_Cutter_Validated.blend','SM_MainGame_Neural_Cutter')]
DEST.mkdir(parents=True,exist_ok=True);summary={'passed':False,'stages':[]};started=time.monotonic()
def report(**values):
    summary.update(values);(DEST/'progress.json').write_text(json.dumps(summary,indent=2),encoding='utf-8');print(json.dumps(values),flush=True)
validators=runpy.run_path(str(ROOT/'Art/Mining/TunnelCutter/validate_tunnel.py'),run_name='validators')
def remove(o):
    mesh=o.data;bpy.data.objects.remove(o,do_unlink=True)
    if mesh.users==0:bpy.data.meshes.remove(mesh)
def mesh_tree(o):
    o.data.calc_loop_triangles();return BVHTree.FromPolygons([v.co[:] for v in o.data.vertices],[tuple(t.vertices) for t in o.data.loop_triangles],all_triangles=True)
def smoothstep(a,b,x):
    f=max(0,min(1,(x-a)/(b-a)));return f*f*(3-2*f)

def envelope(record,factor,voxel_size=.16):
    path=ns['paths'][record['path_index']];a,b=ns['interval'](record,factor);length=b-a
    samples=np.linspace(a,b,max(4,math.ceil(length/.8)+1));sides=64;verts=[];faces=[]
    recess=min(.45,length*.09);amplitude=min(.58,length*.13);seed=record['seed']*.003
    def profile(x,y,aa,bb,end):
        radius=math.sqrt((x/aa)**2+(y/bb)**2);angle=math.atan2(y/bb,x/aa)
        rim=smoothstep(.57,.96,radius)
        lobes=.5+.5*math.sin(angle*5+seed+math.sin(angle*3)*1.7)
        macro=noise.noise(Vector((x/1.15,y/1.15,seed+end*7)))
        detail=noise.noise(Vector((x/.48,y/.48,seed+end*11)))
        # Fingers at the wall lead the cap; the center recedes to form a broken rock face.
        distance=rim*(.08+.30*lobes)-(1-rim)*recess+amplitude*(macro*.9+detail*.24)*(1-rim*.75)
        distance=max(-recess-amplitude,min(.40,distance))
        return (1 if end else -1)*distance
    for k,s in enumerate(samples):
        center,u,v,t=ns['frame'](path,s);wa=float(np.interp(s,path['arc'],path['half_width']));ha=float(np.interp(s,path['arc'],path['half_height']))
        for j in range(sides):
            theta=j*2*math.pi/sides;x=(wa+1.25)*math.cos(theta);y=(ha+1.25)*math.sin(theta)
            dz=profile(x,y,wa,ha,k==len(samples)-1) if k in [0,len(samples)-1] else 0
            verts.append(tuple(center+x*u+y*v+dz*t))
    for k in range(len(samples)-1):
        for j in range(sides):
            p=k*sides+j;q=k*sides+(j+1)%sides;faces.append((p,q,q+sides,p+sides))
    for end,k,s in [(0,0,a),(1,len(samples)-1,b)]:
        center,u,v,t=ns['frame'](path,s);outer=list(range(k*sides,(k+1)*sides))
        wa=float(np.interp(s,path['arc'],path['half_width']));ha=float(np.interp(s,path['arc'],path['half_height']))
        for frac in (.90,.80,.68,.56,.44,.32,.20,.09):
            inner=[]
            for j in range(sides):
                theta=j*2*math.pi/sides;x=(wa+1.25)*frac*math.cos(theta);y=(ha+1.25)*frac*math.sin(theta)
                inner.append(len(verts));verts.append(tuple(center+x*u+y*v+profile(x,y,wa,ha,end)*t))
            for j in range(sides):
                n=(j+1)%sides;faces.extend([(outer[j],outer[n],inner[n]),(outer[j],inner[n],inner[j])])
            outer=inner
        c=len(verts);verts.append(tuple(center+profile(0,0,wa,ha,end)*t))
        for j in range(sides):faces.append((outer[j],outer[(j+1)%sides],c))
    mesh=bpy.data.meshes.new(record['name']+'_RockEnvelope');mesh.from_pydata(verts,[],faces);mesh.update()
    bm=bmesh.new();bm.from_mesh(mesh);bmesh.ops.triangulate(bm,faces=list(bm.faces));bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces));bm.to_mesh(mesh);bm.free()
    obj=bpy.data.objects.new(record['name'],mesh);bpy.context.scene.collection.objects.link(obj)
    # Broad envelopes can fold across themselves at tight turns. Voxel-union the envelope
    # before the exact cavity intersection; the final contact wall still comes from the cutter.
    bpy.context.view_layer.objects.active=obj;obj.select_set(True)
    remesh=obj.modifiers.new('Closed_Rock_Envelope','REMESH');remesh.mode='VOXEL';remesh.voxel_size=voxel_size
    bpy.ops.object.modifier_apply(modifier=remesh.name)
    dec=obj.modifiers.new('Lowpoly_Envelope','DECIMATE');dec.ratio=.26;dec.use_collapse_triangulate=True
    bpy.ops.object.modifier_apply(modifier=dec.name)
    return obj

def clean(mesh):
    bm=bmesh.new();bm.from_mesh(mesh)
    bmesh.ops.remove_doubles(bm,verts=list(bm.verts),dist=4e-5)
    for _ in range(8):
        bmesh.ops.triangulate(bm,faces=list(bm.faces));bmesh.ops.dissolve_degenerate(bm,dist=4e-5,edges=list(bm.edges))
        tiny=[f for f in bm.faces if f.calc_area()<1e-10]
        if not tiny:break
        bmesh.ops.collapse(bm,edges=list(set(min(f.edges,key=lambda e:e.calc_length()) for f in tiny)))
    bmesh.ops.triangulate(bm,faces=list(bm.faces));bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces));bm.to_mesh(mesh);bm.free()

def preview(stage,tunnel,block,record,folder):
    scene=bpy.context.scene
    for o in scene.objects:o.hide_render=True
    block.hide_render=False
    path=ns['paths'][record['path_index']];a,b=ns['interval'](record,factor);center,u,v,t=ns['frame'](path,a)
    camera_data=bpy.data.cameras.new('RoughGuideCamera');camera=bpy.data.objects.new('RoughGuideCamera',camera_data);scene.collection.objects.link(camera)
    eye=center-t*max(9,float(np.interp(a,path['arc'],path['half_width']))*2.4)+v*.35
    camera.location=eye;camera.rotation_euler=(center-eye).to_track_quat('-Z','Y').to_euler();camera_data.lens=29;camera_data.clip_end=1000;scene.camera=camera
    scene.render.engine='BLENDER_WORKBENCH';scene.render.resolution_x=1100;scene.render.resolution_y=850;scene.render.resolution_percentage=100
    scene.render.image_settings.file_format='PNG';scene.render.filepath=str(folder/'RoughGuide_Front.png')
    shading=scene.display.shading;shading.light='STUDIO';shading.color_type='MATERIAL';shading.show_shadows=True;shading.show_cavity=True;shading.cavity_type='BOTH';shading.background_type='WORLD'
    scene.world=bpy.data.worlds.new('RockPreview');scene.world.color=(.03,.035,.04)
    bpy.ops.render.render(write_still=True)
    bpy.data.objects.remove(camera,do_unlink=True);bpy.data.cameras.remove(camera_data);scene.camera=None
    for o in scene.objects:o.hide_render=o==tunnel

try:
    requested=sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else []
    for stage,file,name in STAGES:
        if requested and stage not in requested:continue
        bpy.ops.wm.read_factory_settings(use_empty=True);folder=DEST/stage;folder.mkdir(parents=True,exist_ok=True)
        with bpy.data.libraries.load(str(BASE/stage/file),link=False) as (source,target):
            assert name in source.objects,(name,source.objects);target.objects=[name]
        tunnel=target.objects[0];bpy.context.scene.collection.objects.link(tunnel);tunnel.hide_render=True;tunnel.display_type='WIRE'
        helper=(ROOT/'Art/Mining/TunnelFill50V3/build_fill50.py').read_text().split('\ntry:\n    OUT.mkdir')[0]
        ns={'__file__':str(ROOT/'Art/Mining/TunnelFill50V3/build_fill50.py'),'__name__':'rough_guide_helpers'};exec(compile(helper,'fitted_guide_helpers','exec'),ns)
        ns.update(OUT=folder,SOURCE=BASE/stage,paths=json.loads((BASE/stage/'centerlines.json').read_text()));ns['prepare_paths']()
        plan=json.loads((BASE/stage/'placement_plan.json').read_text());factor=plan['factor'];records=plan['objects'];blocks=[];envelopes=[];reports=[]
        palette=ns['old']['materials']();tree=mesh_tree(tunnel);total=validators['topology'](tunnel.data)['signed_volume_m3']
        report(phase='building',stage=stage,completed=0)
        for i,r in enumerate(records):
            for voxel_size in (.16,.14,.18,.13,.17):
                obj=envelope(r,factor,voxel_size);env=bpy.data.objects.new('SealEnvelope',obj.data.copy());bpy.context.scene.collection.objects.link(env);env.hide_render=True
                bpy.context.view_layer.objects.active=obj;obj.select_set(True)
                boolean=obj.modifiers.new('Exact_Fit_To_Existing_Cavity','BOOLEAN');boolean.operation='INTERSECT';boolean.solver='EXACT';boolean.object=tunnel
                bpy.ops.object.modifier_apply(modifier=boolean.name);clean(obj.data)
                top=validators['topology'](obj.data);top.update(validators['intersections'](obj.data))
                if top['connected_components']==1 and all(top[k]==0 for k in ['boundary_edges','nonmanifold_edges','inconsistent_winding_edges','degenerate_faces','duplicate_faces','self_intersection_pairs']):
                    envelopes.append(env);top['envelope_voxel_m']=voxel_size;break
                remove(obj);remove(env)
            else:raise AssertionError((r['name'],'Unable to make a clean exact intersection',top))
            assert top['connected_components']==1 and top['signed_volume_m3']>0,(obj.name,top)
            assert all(top[k]==0 for k in ['boundary_edges','nonmanifold_edges','inconsistent_winding_edges','degenerate_faces','duplicate_faces','self_intersection_pairs']),(obj.name,top)
            a,b=ns['interval'](r,factor);btree=mesh_tree(obj);path=ns['paths'][r['path_index']];error=0
            # Interior full sections must seal against the original cavity, leaving no bypass.
            for s in np.linspace(a+(b-a)*.39,b-(b-a)*.39,4):
                center,u,v,t=ns['frame'](path,s)
                for angle in np.linspace(0,2*math.pi,32,endpoint=False):
                    direction=math.cos(angle)*u+math.sin(angle)*v;actual=tree.ray_cast(center,direction,30);plug=btree.ray_cast(center,direction,30)
                    assert actual[0] is not None and plug[0] is not None,(obj.name,'missing section')
                    d=(actual[0]-plug[0]).length;error=max(error,d);assert d<.003,(obj.name,'wall gap',s,d)
            obj.data.materials.clear()
            for m in palette:obj.data.materials.append(m)
            for p in obj.data.polygons:p.use_smooth=False;p.material_index=max(0,min(4,int((noise.noise(p.center/1.5)+1)*2.5)))
            obj['purpose']='Closed full-section diggable stone; lobed wall fingers and rough recessed end faces'
            obj['start_arc_m']=a;obj['end_arc_m']=b;obj['path_name']=r['path_name'];obj['shared_origin_with_tunnel']=True
            top.update(name=obj.name,max_wall_fit_error_m=error);reports.append(top);blocks.append(obj);report(stage=stage,completed=i+1)
        blocked=sum(r['signed_volume_m3'] for r in reports);fraction=blocked/total
        assert .45<fraction<.55,(stage,'fill fraction',fraction)
        # Full Boolean seal test on envelopes avoids coincident wall surfaces in the real plugs.
        residual=bpy.data.objects.new('Residual',tunnel.data.copy());bpy.context.scene.collection.objects.link(residual);bpy.context.view_layer.objects.active=residual
        for envelope_object in envelopes:
            boolean=residual.modifiers.new('Verify_Air_Pockets','BOOLEAN');boolean.operation='DIFFERENCE';boolean.solver='EXACT';boolean.object=envelope_object;bpy.ops.object.modifier_apply(modifier=boolean.name)
        residual_top=validators['topology'](residual.data);assert residual_top['connected_components']==17,(stage,residual_top)
        remove(residual)
        for envelope_object in envelopes:remove(envelope_object)
        trees=[mesh_tree(o) for o in blocks];terminals=[]
        for p in ns['paths']:
            nearest=min(t.find_nearest(Vector(p['points'][-1]))[3] for t in trees);assert nearest>(5 if p['name']=='Main' else 2.5),(stage,p['name'],'terminal covered',nearest)
            terminals.append(dict(path=p['name'],nearest_blocker_distance_m=nearest))
        for i,a_tree in enumerate(trees):
            for j,b_tree in enumerate(trees[:i]):
                if not a_tree.overlap(b_tree):continue
                # BVH overlap only reports bounding-box candidates, not triangle crossings.
                aa=blocks[i].data;bb=blocks[j].data;aa.calc_loop_triangles();bb.calc_loop_triangles()
                check=bpy.data.meshes.new('PairIntersectionCheck');offset=len(aa.vertices)
                check.from_pydata([v.co[:] for v in aa.vertices]+[v.co[:] for v in bb.vertices],[],
                    [tuple(t.vertices) for t in aa.loop_triangles]+[tuple(k+offset for k in t.vertices) for t in bb.loop_triangles])
                crossings=validators['intersections'](check)['self_intersection_pairs'];bpy.data.meshes.remove(check)
                assert crossings==0,(stage,'blocker triangle crossings',i,j,crossings)
        bpy.ops.object.select_all(action='DESELECT')
        for o in blocks:o.select_set(True)
        bpy.context.view_layer.objects.active=blocks[0]
        bpy.ops.export_scene.fbx(filepath=str(folder/'Tunnel_Blockers.fbx'),use_selection=True,object_types={'MESH'},global_scale=1,
            apply_unit_scale=True,apply_scale_options='FBX_SCALE_UNITS',axis_forward='-Y',axis_up='Z',mesh_smooth_type='FACE',use_custom_props=True,bake_anim=False)
        preview(stage,tunnel,blocks[1],records[1],folder)
        bpy.ops.wm.save_as_mainfile(filepath=str(folder/'Blockers_RockV2.blend'))
        result=dict(stage=stage,passed=True,count=len(blocks),fill_fraction=fraction,total_triangles=sum(r['faces'] for r in reports),
            blockers=reports,terminals=terminals,residual_air_components=17,fbx=str(folder/'Tunnel_Blockers.fbx'))
        (folder/'validation.json').write_text(json.dumps(result,indent=2),encoding='utf-8');summary['stages'].append(result);report(phase='stage_complete',stage=stage)
    report(passed=True,phase='complete',elapsed_seconds=time.monotonic()-started)
except Exception:report(phase='failed',error=traceback.format_exc());raise
