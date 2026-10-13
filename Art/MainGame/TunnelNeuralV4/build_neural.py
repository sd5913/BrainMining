"""Lowpoly neuronal stone arbors, grounded and subtracted from the tunnel cutter."""
from pathlib import Path
OUT=Path(__file__).parent
code=(OUT.parent/'TunnelCavernV2/build_cavern.py').read_text()
code=code.replace('exclusions=[(r-3.5,r+3.5)', 'exclusions=[(r-4.5,r+4.5)')
for a,b in {
    'MainGame_Tunnel_Cavern_V2':'MainGame_Tunnel_Neural_V4',
    'SM_MainGame_Cavern_Cutter':'SM_MainGame_Neural_Cutter',
    'MainGame_Cavern_Cutter_Validated.blend':'MainGame_Neural_Cutter_Validated.blend',
    'MainGame_Cavern_Placement.png':'MainGame_Neural_Placement.png',
    'SM_MainGame_Cavern_Blocker_':'SM_MainGame_Neural_Blocker_',
    '20261014':'20261016',
    '==-10':'<=2',
    "OUT.parent/'TunnelV1/centerlines.json'":"OUT.parent/'TunnelStalagmiteV3/centerlines.json'",
    'REFERENCE_ROCK_COLUMNS_NOT_EXPORTED':'REFERENCE_NEURAL_ROCKS_NOT_EXPORTED',
}.items():code=code.replace(a,b)
start=code.index("pillar_step='''");end=code.index("source=source.replace(\"    topology",start)
neural_step=r'''
    from mathutils.bvhtree import BVHTree
    pillar_specs=json.loads((OUT/'neural_rocks.json').read_text());pillar_objects=[]
    original=bpy.data.objects.new('TEMP_Uncut_Cavity',tunnel.data.copy());scene.collection.objects.link(original);original.data.calc_loop_triangles()
    terrain=BVHTree.FromPolygons([v.co[:] for v in original.data.vertices],[tuple(t.vertices) for t in original.data.loop_triangles],all_triangles=True)
    def filament(name,points,radii):
        return wide['tube'](dict(name=name,points=[list(p) for p in points],half_width=radii,half_height=radii))
    for number,spec in enumerate(pillar_specs):
        sample=Vector(spec['center_m']);floor=terrain.ray_cast(sample,Vector((0,0,-1)),30);ceiling=terrain.ray_cast(sample,Vector((0,0,1)),30)
        assert floor[0] is not None and ceiling[0] is not None,spec['name']
        base=floor[0].copy();t=Vector(spec['tangent']);u=Vector(spec['outward']);z=Vector((0,0,1))
        soma=base+t*.10+u*.08+z*2.15
        parts=[filament('Neural_Root',[base-z*.85-t*.08,base+z*.35,base+z*1.15+t*.16,soma],[.47,.44,.37,.32])]
        bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=2,radius=1,location=soma)
        body=bpy.context.object;body.scale=(.82,.72,.70);bpy.ops.object.transform_apply(location=False,rotation=False,scale=True);parts.append(body)
        for v in body.data.vertices:v.co*=1+.055*noise.noise(v.co*4+Vector((number*.9,2.,7.)))
        endpoints=[(-1.7,.15,.95),(1.8,.12,1.0),(-1.45,.42,-.35),(1.55,.40,-.18),(-.32,.35,1.75),(.50,.90,1.18)]
        for j,(along,out,up) in enumerate(endpoints):
            endpoint=soma+t*along+u*out+z*up
            middle=soma+t*(along*.53)+u*(out*.5+.10)+z*(up*.40+.12)
            parts.append(filament(f'Dendrite_{j}',[soma,middle,endpoint],[.265,.19,.105]))
            # Two fine twigs branch away from each primary, with rounded sealed tips.
            for k,sign in enumerate([-1,1]):
                root=soma.lerp(middle,.75) if k==0 else middle.lerp(endpoint,.35)
                target=root+t*(sign*.54)+u*(.25+.10*k)+z*(.42 if j%2==0 else -.38)
                bend=root.lerp(target,.55)+u*.10+z*.08
                parts.append(filament(f'Twig_{j}_{k}',[root,bend,target],[.16,.12,.085]))
        bpy.ops.object.select_all(action='DESELECT')
        for ob in parts:ob.select_set(True)
        bpy.context.view_layer.objects.active=parts[0];bpy.ops.object.join();ob=bpy.context.object;ob.name='REFERENCE_'+spec['name']
        remesh=ob.modifiers.new('Neural_Branch_Union','REMESH');remesh.mode='VOXEL';remesh.voxel_size=.085;bpy.ops.object.modifier_apply(modifier=remesh.name)
        smooth=ob.modifiers.new('Neural_Soft_Junctions','SMOOTH');smooth.factor=.6;smooth.iterations=2;bpy.ops.object.modifier_apply(modifier=smooth.name)
        dec=ob.modifiers.new('Neural_Lowpoly','DECIMATE');dec.ratio=.24;dec.use_collapse_triangulate=True;bpy.ops.object.modifier_apply(modifier=dec.name)
        bm=bmesh.new();bm.from_mesh(ob.data);bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces));bm.to_mesh(ob.data);bm.free()
        rock_top=validation['topology'](ob.data);assert rock_top['connected_components']==1 and rock_top['boundary_edges']==0,rock_top
        ob.data.materials.clear();ob.data.materials.append(tunnel.data.materials[1])
        bpy.context.view_layer.objects.active=tunnel
        boolean=tunnel.modifiers.new('Neural_Rock_Indent_'+spec['name'],'BOOLEAN');boolean.operation='DIFFERENCE';boolean.solver='EXACT';boolean.object=ob
        bpy.ops.object.modifier_apply(modifier=boolean.name)
        bpy.context.view_layer.objects.active=ob
        clip=ob.modifiers.new('Visible_Grounded_Rock','BOOLEAN');clip.operation='INTERSECT';clip.solver='EXACT';clip.object=original
        bpy.ops.object.modifier_apply(modifier=clip.name);pillar_objects.append(ob)
        spec.update(base_m=list(base),probe_m=list(soma),tip_m=list(soma+z*1.95),ground_attached=True,
            ceiling_clearance_m=ceiling[0].z-(soma.z+1.95),visible_triangles=len(ob.data.polygons))
        assert spec['ceiling_clearance_m']>1.5,spec
    mesh=original.data;bpy.data.objects.remove(original,do_unlink=True);bpy.data.meshes.remove(mesh)
    bm=bmesh.new();bm.from_mesh(tunnel.data)
    for _ in range(5):
        bmesh.ops.triangulate(bm,faces=list(bm.faces));bmesh.ops.dissolve_degenerate(bm,dist=4e-5,edges=list(bm.edges))
    bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces));bm.to_mesh(tunnel.data);bm.free()
    tunnel.data.calc_loop_triangles()
    tree=BVHTree.FromPolygons([v.co[:] for v in tunnel.data.vertices],[tuple(t.vertices) for t in tunnel.data.loop_triangles],all_triangles=True)
    for spec in pillar_specs:
        hit=tree.ray_cast(Vector(spec['probe_m']),Vector((1,0,0)),2)
        assert hit[0] is not None and hit[3]<1.2,(spec['name'],'Missing neural stone body')
    (OUT/'neural_rocks.json').write_text(json.dumps(pillar_specs,indent=2))
'''
code=code[:start]+"pillar_step="+repr(neural_step)+"\n"+code[end:]
repair_step="""    for ob in blocks:
        for repair_pass in range(8):
            bm=bmesh.new();bm.from_mesh(ob.data)
            bad=[f for f in bm.faces if f.calc_area()<1e-10]
            if not bad:bm.free();break
            edges=list(set(min(f.edges,key=lambda e:e.calc_length()) for f in bad))
            bmesh.ops.collapse(bm,edges=edges)
            bmesh.ops.triangulate(bm,faces=list(bm.faces));bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces));bm.to_mesh(ob.data);bm.free()
        assert validation['topology'](ob.data)['degenerate_faces']==0,ob.name
    report=ns['validate'](blocks,records,total,factor)
"""
code=code.replace('exec(compile(source,',"source=source.replace("+repr("    report=ns['validate'](blocks,records,total,factor)")+", "+repr(repair_step)+")\nexec(compile(source,")
exec(compile(code,str(OUT/'build_neural_generated.py'),'exec'),{'__file__':str(__file__),'__name__':'__main__'})
