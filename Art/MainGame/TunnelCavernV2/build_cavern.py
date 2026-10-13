"""Reuse validated closed-tube/plug construction, with subtractive through-columns."""
import bpy,bmesh,json,math,runpy
from pathlib import Path
from mathutils import Vector,noise
import numpy as np
OUT=Path(__file__).parent
source=(OUT.parent/'TunnelV1/build_main_game.py').read_text()
source=source.replace('import bpy,bmesh,json,runpy,math,traceback','import bpy,bmesh,json,runpy,math,traceback\nimport numpy as np')
for a,b in {
    'MainGame_Tunnel_V1':'MainGame_Tunnel_Cavern_V2',
    'SM_MainGame_Tunnel_Cutter':'SM_MainGame_Cavern_Cutter',
    'MainGame_Cutter_Validated.blend':'MainGame_Cavern_Cutter_Validated.blend',
    'MainGame_Blockers_Placement.png':'MainGame_Cavern_Placement.png',
    'SM_MainGame_Blocker_':'SM_MainGame_Cavern_Blocker_',
    '20261013':'20261014',
    "topology['euler_characteristic']==2":"topology['euler_characteristic']==-10",
}.items():source=source.replace(a,b)

pillar_step='''
    pillar_specs=json.loads((OUT/'pillars.json').read_text())
    pillar_objects=[]
    for spec in pillar_specs:
        verts=[];faces=[];sides=16;rings=13;center=Vector(spec['center_m'])
        for k in range(rings):
            t=k/(rings-1);z=(t-.5)*spec['height_m']
            radius=spec['radius_m']*(1+.32*abs(2*t-1)**3+.14*math.sin(t*math.pi*3))
            for j in range(sides):
                angle=j*2*math.pi/sides
                r=radius*(1+.08*noise.noise(Vector((j*.65,t*4,spec['arc_m']))))
                verts.append(tuple(center+Vector((r*math.cos(angle)+.13*math.sin(t*math.pi),r*math.sin(angle),z))))
        for k in range(rings-1):
            for j in range(sides):
                a=k*sides+j;b=k*sides+(j+1)%sides;faces.append((a,b,b+sides,a+sides))
        faces.extend([tuple(reversed(range(sides))),tuple(range((rings-1)*sides,rings*sides))])
        mesh=bpy.data.meshes.new(spec['name']);mesh.from_pydata(verts,[],faces);mesh.update()
        bm=bmesh.new();bm.from_mesh(mesh);bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces));bm.to_mesh(mesh);bm.free()
        obj=bpy.data.objects.new('REFERENCE_'+spec['name'],mesh);scene.collection.objects.link(obj)
        obj.data.materials.append(tunnel.data.materials[1]);obj['purpose']='Column solid reference only; real column is the hole through the cutter'
        pillar_objects.append(obj)
        bpy.context.view_layer.objects.active=tunnel
        boolean=tunnel.modifiers.new('Preserve_'+spec['name'],'BOOLEAN');boolean.operation='DIFFERENCE';boolean.solver='EXACT';boolean.object=obj
        bpy.ops.object.modifier_apply(modifier=boolean.name)
    bm=bmesh.new();bm.from_mesh(tunnel.data)
    for _ in range(4):
        bmesh.ops.triangulate(bm,faces=list(bm.faces));bmesh.ops.dissolve_degenerate(bm,dist=4e-5,edges=list(bm.edges))
    bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces));bm.to_mesh(tunnel.data);bm.free()
    # A ray through each missing column meets an inner cavity surface on either side.
    from mathutils.bvhtree import BVHTree
    tunnel.data.calc_loop_triangles()
    tree=BVHTree.FromPolygons([v.co[:] for v in tunnel.data.vertices],[tuple(t.vertices) for t in tunnel.data.loop_triangles],all_triangles=True)
    for spec in pillar_specs:
        center=Vector(spec['center_m'])
        hit=tree.ray_cast(center,Vector((1,0,0)),3)
        assert hit[0] is not None and hit[3]<2.0,(spec['name'],'Column not cut out')
'''
source=source.replace("    topology=validation['topology'](tunnel.data);",pillar_step+"\n    topology=validation['topology'](tunnel.data);")
start=source.index('    bounds=[(8.,roots[0]-4.)]')
end=source.index('    records=[dict(',start)
source=source[:start]+'''    exclusions=[(r-3.5,r+3.5) for r in roots]
    exclusions.extend((s-3.7,s+3.7) for s in topology['cavern_centers_arc_m'])
    bounds=[(6.,length-10.)]
    for lo,hi in sorted(exclusions):
        updated=[]
        for a,b in bounds:
            if hi<=a or lo>=b:updated.append((a,b));continue
            if lo>a+2.5:updated.append((a,lo))
            if hi<b-2.5:updated.append((hi,b))
        bounds=updated
    while len(bounds)<12:
        choices=[(ns['volume_estimate'](main,a,b),i) for i,(a,b) in enumerate(bounds) if b-a>8.]
        assert choices,('Not enough plug windows',bounds)
        _,i=max(choices);a,b=bounds.pop(i);mid=(a+b)*.5;bounds[i:i]=[(a,mid-.75),(mid+.75,b)]
'''+source[end:]

# Build the future-room connector as a separate auxiliary cutter, excluded from plug volume.
connector_step='''
    old=json.loads((OUT.parent/'TunnelV1/centerlines.json').read_text())[0]
    main=json.loads((OUT/'centerlines.json').read_text())[0]
    coords=[];aa=[];bb=[]
    for i,s in enumerate(old['arc']):
        if s<old['arc'][-1]-6:continue
        p=old['points'][i];coords.append([p[0]-148,p[1],p[2]+56.8]);aa.append(old['half_width'][i]);bb.append(old['half_height'][i])
    for t in np.linspace(0,1,65)[1:-1]:
        # 18 m between old end (-83,66.8) and next start (-65,65).
        blend=math.sin(math.pi*t)**2
        a=(1-t)*old['half_width'][-1]+t*main['half_width'][0]
        b=(1-t)*old['half_height'][-1]+t*main['half_height'][0]
        width=a*(1-blend)+6.*blend;height=b*(1-blend)+4.*blend
        base=66.8-1.8*t
        coords.append([-83+18*t,0,base+height-b]);aa.append(width);bb.append(height)
    for i,s in enumerate(main['arc']):
        if s>6:break
        coords.append(main['points'][i]);aa.append(main['half_width'][i]);bb.append(main['half_height'][i])
    connector=wide['tube'](dict(name='FutureRoom_Connector',points=coords,half_width=aa,half_height=bb))
    connector.name='SM_MainGame_FutureRoom_Connector'
    bpy.context.view_layer.objects.active=connector
    remesh=connector.modifiers.new('Connector_Closed_Union','REMESH');remesh.mode='VOXEL';remesh.voxel_size=.18
    bpy.ops.object.modifier_apply(modifier=remesh.name)
    smooth=connector.modifiers.new('Connector_Smooth_Transitions','SMOOTH');smooth.factor=.5;smooth.iterations=3
    bpy.ops.object.modifier_apply(modifier=smooth.name)
    dec=connector.modifiers.new('Connector_Lowpoly','DECIMATE');dec.ratio=.14;dec.use_collapse_triangulate=True
    bpy.ops.object.modifier_apply(modifier=dec.name)
    bm=bmesh.new();bm.from_mesh(connector.data)
    for _ in range(4):
        bmesh.ops.triangulate(bm,faces=list(bm.faces));bmesh.ops.dissolve_degenerate(bm,dist=4e-5,edges=list(bm.edges))
    bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces));bm.to_mesh(connector.data);bm.free()
    wide['materials'](connector)
    top=validation['topology'](connector.data);top.update(validation['intersections'](connector.data))
    assert top['boundary_edges']==0 and top['nonmanifold_edges']==0 and top['self_intersection_pairs']==0,top
    (OUT/'connector_validation.json').write_text(json.dumps(top,indent=2))
    export([connector],'FutureRoom_Connector.fbx');connector.hide_render=True;connector.display_type='WIRE'
    reserve=bpy.data.collections.new('FUTURE_ROOM_CONNECTOR');scene.collection.children.link(reserve)
    for c in list(connector.users_collection):c.objects.unlink(connector)
    reserve.objects.link(connector)
    references=bpy.data.collections.new('REFERENCE_ROCK_COLUMNS_NOT_EXPORTED');scene.collection.children.link(references)
    for ob in pillar_objects:
        for c in list(ob.users_collection):c.objects.unlink(ob)
        references.objects.link(ob)
'''
source=source.replace("allowed_start_m=8.,allowed_end_m=float(path['arc'][-1])-5.","allowed_start_m=6.5,allowed_end_m=float(path['arc'][-1])-5.")
source=source.replace("    ns['preview'](blocks)",connector_step+"\n    ns['preview'](blocks)")
exec(compile(source,str(OUT/'build_cavern_generated.py'),'exec'),{'__file__':str(__file__),'__name__':'__main__'})
