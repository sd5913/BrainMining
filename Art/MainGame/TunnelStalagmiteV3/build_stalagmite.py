"""Subtract blind ground-anchored cones from the cutter; resulting UE rock contains stalagmites."""
from pathlib import Path
OUT=Path(__file__).parent
code=(OUT.parent/'TunnelCavernV2/build_cavern.py').read_text()
code=code.replace('exclusions=[(r-3.5,r+3.5)', 'exclusions=[(r-4.5,r+4.5)')
for a,b in {
    'MainGame_Tunnel_Cavern_V2':'MainGame_Tunnel_Stalagmite_V3',
    'SM_MainGame_Cavern_Cutter':'SM_MainGame_Stalagmite_Cutter',
    'MainGame_Cavern_Cutter_Validated.blend':'MainGame_Stalagmite_Cutter_Validated.blend',
    'MainGame_Cavern_Placement.png':'MainGame_Stalagmite_Placement.png',
    'SM_MainGame_Cavern_Blocker_':'SM_MainGame_Stalagmite_Blocker_',
    '20261014':'20261015',
    "== -10":"==2", "==-10":"==2",
    "OUT.parent/'TunnelV1/centerlines.json'":"OUT.parent/'TunnelCavernV2/centerlines.json'",
    'REFERENCE_ROCK_COLUMNS_NOT_EXPORTED':'REFERENCE_STALAGMITES_NOT_EXPORTED',
}.items():code=code.replace(a,b)
start=code.index("pillar_step='''");end=code.index("source=source.replace(\"    topology",start)
stone_step=r'''
    from mathutils.bvhtree import BVHTree
    pillar_specs=json.loads((OUT/'stalagmites.json').read_text());pillar_objects=[]
    original=bpy.data.objects.new('TEMP_Original_Cavity',tunnel.data.copy());scene.collection.objects.link(original)
    original.data.calc_loop_triangles()
    terrain=BVHTree.FromPolygons([v.co[:] for v in original.data.vertices],[tuple(t.vertices) for t in original.data.loop_triangles],all_triangles=True)
    for number,spec in enumerate(pillar_specs):
        sample=Vector(spec['center_m']);floor=terrain.ray_cast(sample,Vector((0,0,-1)),30)
        ceiling=terrain.ray_cast(sample,Vector((0,0,1)),30)
        assert floor[0] is not None and ceiling[0] is not None,spec['name']
        base=floor[0].copy();height=min(spec['height_m'],ceiling[0].z-base.z-1.5)
        assert height>=1.,spec['name']
        verts=[];faces=[];sides=12;rings=9;embedded=.8
        for k in range(rings):
            t=k/rings;z=base.z-embedded+(height+embedded)*t
            radius=spec['radius_m']*(1-t)**.83*(1+.12*math.sin(t*math.pi*3))
            for j in range(sides):
                angle=j*2*math.pi/sides
                r=radius*(1+.08*noise.noise(Vector((j*.8,t*3,number*.91))))
                verts.append((base.x+r*math.cos(angle)+.12*t*t,base.y+r*math.sin(angle)+.07*math.sin(t*math.pi),z))
        tip=len(verts);verts.append((base.x+.12,base.y,base.z+height))
        for k in range(rings-1):
            for j in range(sides):
                a=k*sides+j;b=k*sides+(j+1)%sides;faces.append((a,b,b+sides,a+sides))
        faces.append(tuple(reversed(range(sides))))
        for j in range(sides):faces.append(((rings-1)*sides+j,(rings-1)*sides+(j+1)%sides,tip))
        mesh=bpy.data.meshes.new(spec['name']);mesh.from_pydata(verts,[],faces);mesh.update()
        bm=bmesh.new();bm.from_mesh(mesh);bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces));bm.to_mesh(mesh);bm.free()
        ob=bpy.data.objects.new('REFERENCE_'+spec['name'],mesh);scene.collection.objects.link(ob);ob.data.materials.append(tunnel.data.materials[1])
        bpy.context.view_layer.objects.active=tunnel
        boolean=tunnel.modifiers.new('Ground_Stalagmite_'+spec['name'],'BOOLEAN');boolean.operation='DIFFERENCE';boolean.solver='EXACT';boolean.object=ob
        bpy.ops.object.modifier_apply(modifier=boolean.name)
        # Clip the reference to the actual visible part so previews have no underground protrusions.
        bpy.context.view_layer.objects.active=ob
        clip=ob.modifiers.new('Visible_Above_Floor','BOOLEAN');clip.operation='INTERSECT';clip.solver='EXACT';clip.object=original
        bpy.ops.object.modifier_apply(modifier=clip.name);pillar_objects.append(ob)
        probe=base+Vector((.02,0,height*.32))
        spec.update(base_m=list(base),tip_m=[base.x+.12,base.y,base.z+height],probe_m=list(probe),actual_height_m=height,
            ceiling_clearance_m=ceiling[0].z-base.z-height,embedded_m=embedded,ground_attached=True)
    mesh=original.data;bpy.data.objects.remove(original,do_unlink=True);bpy.data.meshes.remove(mesh)
    bm=bmesh.new();bm.from_mesh(tunnel.data)
    for _ in range(4):
        bmesh.ops.triangulate(bm,faces=list(bm.faces));bmesh.ops.dissolve_degenerate(bm,dist=4e-5,edges=list(bm.edges))
    bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces));bm.to_mesh(tunnel.data);bm.free()
    tunnel.data.calc_loop_triangles()
    tree=BVHTree.FromPolygons([v.co[:] for v in tunnel.data.vertices],[tuple(t.vertices) for t in tunnel.data.loop_triangles],all_triangles=True)
    for spec in pillar_specs:
        probe=Vector(spec['probe_m']);hit=tree.ray_cast(probe,Vector((1,0,0)),2)
        assert hit[0] is not None and hit[3]<1.2,(spec['name'],'Missing actual stalagmite surface')
        assert spec['ceiling_clearance_m']>=1.5
    (OUT/'stalagmites.json').write_text(json.dumps(pillar_specs,indent=2))
'''
code=code[:start]+"pillar_step="+repr(stone_step)+"\n"+code[end:]
exec(compile(code,str(OUT/'build_stalagmite_generated.py'),'exec'),{'__file__':str(__file__),'__name__':'__main__'})
