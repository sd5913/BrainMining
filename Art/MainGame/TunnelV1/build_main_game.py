"""Fresh MainGame cutter + 16 fitted plugs, using the previously verified construction methods."""
import bpy,bmesh,json,runpy,math,traceback
from pathlib import Path
from mathutils import Vector,noise
OUT=Path(__file__).parent
MINING=OUT.parents[1]/'Mining'
paths=json.loads((OUT/'centerlines.json').read_text())
validation=runpy.run_path(str(MINING/'TunnelCutter/validate_tunnel.py'),run_name='validators')
wide=runpy.run_path(str(MINING/'TunnelWideV2/build_wide.py'),run_name='tube_helpers')
bpy.ops.wm.read_factory_settings(use_empty=True)
scene=bpy.context.scene;scene.name='MainGame_Tunnel_V1';scene.unit_settings.system='METRIC';scene.unit_settings.scale_length=1

try:
    parts=[wide['tube'](p) for p in paths]
    bpy.ops.object.select_all(action='DESELECT')
    for obj in parts:obj.select_set(True)
    bpy.context.view_layer.objects.active=parts[0];bpy.ops.object.join();tunnel=bpy.context.object;tunnel.name='SM_MainGame_Tunnel_Cutter'
    remesh=tunnel.modifiers.new('Union_and_Seal','REMESH');remesh.mode='VOXEL';remesh.voxel_size=.22
    bpy.ops.object.modifier_apply(modifier=remesh.name)
    smooth=tunnel.modifiers.new('Natural_Junctions','SMOOTH');smooth.factor=.65;smooth.iterations=4
    bpy.ops.object.modifier_apply(modifier=smooth.name);tunnel.data.update()
    normals=[v.normal.copy() for v in tunnel.data.vertices]
    for v,n in zip(tunnel.data.vertices,normals):
        amount=.15*noise.noise(v.co/2.1+Vector((3.1,7.2,1.4)))+.04*noise.noise(v.co/.8)
        v.co+=n*max(-.12,min(.12,amount))
    dec=tunnel.modifiers.new('Lowpoly_Rock','DECIMATE');dec.ratio=.085;dec.use_collapse_triangulate=True
    bpy.ops.object.modifier_apply(modifier=dec.name)
    bm=bmesh.new();bm.from_mesh(tunnel.data);bmesh.ops.triangulate(bm,faces=list(bm.faces));bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces));bm.to_mesh(tunnel.data);bm.free()
    wide['materials'](tunnel)
    topology=validation['topology'](tunnel.data);topology.update(validation['intersections'](tunnel.data))
    assert topology['connected_components']==1 and topology['euler_characteristic']==2
    assert all(topology[k]==0 for k in ['boundary_edges','nonmanifold_edges','inconsistent_winding_edges','degenerate_faces','duplicate_faces','self_intersection_pairs']),topology
    topology.update(passed=True,**json.loads((OUT/'plan_summary.json').read_text()),dimensions_m=list(tunnel.dimensions))
    (OUT/'tunnel_validation.json').write_text(json.dumps(topology,indent=2));print('MAIN_GAME_TUNNEL '+json.dumps(topology),flush=True)
    bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'MainGame_Cutter_Validated.blend'))

    # Load the validated helpers without executing the old build/export entrypoint.
    code=(MINING/'TunnelFill50V3/build_fill50.py').read_text().split('\ntry:\n    OUT.mkdir')[0]
    replacements={"SOURCE=OUT.parent/'TunnelWideV2'":"SOURCE=OUT", "SCENE='Tunnel_Fill50_V3'":"SCENE='MainGame_Tunnel_V1'",
        "NAME='SM_DescendingTunnel_Wide_Lowpoly'":"NAME='SM_MainGame_Tunnel_Cutter'",'SEED=20261011':'SEED=20261013',
        "OUT.parent/'TunnelCutter/validate_tunnel.py'":"OUT.parents[1]/'Mining/TunnelCutter/validate_tunnel.py'",
        "OUT.parent/'TunnelBlockers/generate_blockers.py'":"OUT.parents[1]/'Mining/TunnelBlockers/generate_blockers.py'",
        'return .5*noise.noise':'return .22*noise.noise','==21':'==17','count=20':'count=16','residual_connected_components=21':'residual_connected_components=17',
        "assert nearest>8.,":"assert nearest>(6. if path['name']=='Main' else 3.),",'data.ortho_scale=345':'data.ortho_scale=185',
        "OUT/'Tunnel_Fill50_Placement.png'":"OUT/'MainGame_Blockers_Placement.png'"}
    for a,b in replacements.items():assert a in code,a;code=code.replace(a,b)
    clean_line='bmesh.ops.triangulate(bm,faces=list(bm.faces));bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces));bm.to_mesh(obj.data);bm.free()'
    assert clean_line in code
    code=code.replace(clean_line,'for repair_pass in range(4):\n            bmesh.ops.triangulate(bm,faces=list(bm.faces))\n            bmesh.ops.dissolve_degenerate(bm,dist=4e-5,edges=list(bm.edges))\n        '+clean_line)
    ns={'__file__':str(__file__),'__name__':'main_game_helpers'};exec(compile(code,str(MINING/'TunnelFill50V3/build_fill50.py'),'exec'),ns)
    ns.update(scene=scene,tunnel=tunnel,palette=ns['old']['materials']());ns['prepare_paths']()
    main=ns['paths'][0];roots=sorted(p['root_arc'] for p in ns['paths'][1:]);length=float(main['arc'][-1])
    bounds=[(8.,roots[0]-4.)]+[(a+4.,b-4.) for a,b in zip(roots[:-1],roots[1:])]+[(roots[-1]+4.,length-10.)]
    while len(bounds)<12:
        choices=[(ns['volume_estimate'](main,a,b),i) for i,(a,b) in enumerate(bounds) if b-a>9]
        _,i=max(choices);a,b=bounds.pop(i);mid=(a+b)*.5;bounds[i:i]=[(a,mid-1.25),(mid+1.25,b)]
    records=[dict(path_index=0,path_name='Main',allowed_start_m=a,allowed_end_m=b) for a,b in bounds]
    for i,path in enumerate(ns['paths'][1:],1):records.append(dict(path_index=i,path_name=path['name'],allowed_start_m=8.,allowed_end_m=float(path['arc'][-1])-5.))
    records.sort(key=lambda r:(r['path_index'],r['allowed_start_m']))
    assert len(records)==16 and all(r['allowed_end_m']>r['allowed_start_m']+2 for r in records)
    for i,r in enumerate(records,1):r.update(name=f'SM_MainGame_Blocker_{i:02}',seed=20261013+i*191)
    total=topology['signed_volume_m3']
    def estimate(f):return sum(ns['volume_estimate'](ns['paths'][r['path_index']],*ns['interval'](r,f)) for r in records)
    print('MAIN_GAME_BLOCK_CAPACITY '+str(estimate(1)/total),flush=True)
    lo,hi=.4,1.
    for _ in range(25):
        mid=(lo+hi)*.5
        if estimate(mid)<total*.5:lo=mid
        else:hi=mid
    factor=(lo+hi)*.5
    for attempt in range(4):
        blocks=ns['generate'](records,factor)
        fill=sum(validation['topology'](ob.data)['signed_volume_m3'] for ob in blocks)/total
        ns['checkpoint'](phase='volume_check',attempt=attempt,fill_fraction=fill,factor=factor)
        if abs(fill-.5)<.012 or (factor>.999 and .45<=fill<=.55):break
        for ob in blocks:ns['remove'](ob)
        factor=min(1.,factor*.5/fill)
    assert .45<=fill<=.55,fill
    report=ns['validate'](blocks,records,total,factor)
    (OUT/'placement_plan.json').write_text(json.dumps(dict(seed=20261013,factor=factor,objects=records),indent=2))
    def export(objects,filename):
        bpy.ops.object.select_all(action='DESELECT')
        for ob in objects:ob.select_set(True)
        bpy.context.view_layer.objects.active=objects[0]
        bpy.ops.export_scene.fbx(filepath=str(OUT/filename),use_selection=True,object_types={'MESH'},global_scale=1,
            apply_unit_scale=True,apply_scale_options='FBX_SCALE_UNITS',axis_forward='-Y',axis_up='Z',mesh_smooth_type='FACE',use_custom_props=True,bake_anim=False)
    export([tunnel],'Tunnel_Cutter.fbx');export(blocks,'Tunnel_Blockers.fbx')
    ns['preview'](blocks)
    cutter_collection=bpy.data.collections.new('CUTTER');scene.collection.children.link(cutter_collection)
    blocker_collection=bpy.data.collections.new('BLOCKERS');scene.collection.children.link(blocker_collection)
    for ob,collection in [(tunnel,cutter_collection)]+[(ob,blocker_collection) for ob in blocks]:
        for old_collection in list(ob.users_collection):old_collection.objects.unlink(ob)
        collection.objects.link(ob)
    tunnel.hide_render=False;tunnel.display_type='WIRE'
    bpy.ops.object.select_all(action='DESELECT')
    for ob in blocks:ob.select_set(True)
    bpy.context.view_layer.objects.active=blocks[0]
    for area in bpy.context.screen.areas:
        if area.type=='VIEW_3D':
            view=area.spaces.active;view.shading.type='SOLID';view.shading.color_type='MATERIAL'
            view.region_3d.view_location=Vector((0,0,37));view.region_3d.view_distance=140
            view.region_3d.view_rotation=Vector((.6,-1,.6)).to_track_quat('Z','Y');view.region_3d.view_perspective='ORTHO'
    bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'MainGame_Tunnel_V1.blend'))
    ns['checkpoint'](phase='complete',passed=True,fill_fraction=report['fill_fraction'],triangles=report['total_triangles'])
except Exception:
    (OUT/'build_error.txt').write_text(traceback.format_exc());raise
