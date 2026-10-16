"""Restore real clay relief around the V5 plugs without changing the cave routes."""
import bpy,bmesh,json,math,random,bisect,sys
from pathlib import Path
from mathutils import Vector,noise
from mathutils.bvhtree import BVHTree
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'Art/MainGame/RoutesV5/ClayRelief';OUT.mkdir(parents=True,exist_ok=True)
STAGES=['TunnelV1','TunnelCavernV2','TunnelStalagmiteV3']
ORIGINS=[(-238,0,59),(-77,0,4),(397.5,-117.5,139)]
rng=random.Random(2026101617)

def sample(path,s):
    i=max(1,min(len(path['arc'])-1,bisect.bisect_left(path['arc'],s)))
    f=max(0,min(1,(s-path['arc'][i-1])/(path['arc'][i]-path['arc'][i-1])))
    return Vector(path['points'][i-1]).lerp(Vector(path['points'][i]),f)

def tree(ob):
    ob.data.calc_loop_triangles()
    return BVHTree.FromPolygons([ob.matrix_world@v.co for v in ob.data.vertices],[tuple(t.vertices) for t in ob.data.loop_triangles],all_triangles=True)

def write(ob,path,origin):
    bm=bmesh.new();bm.from_mesh(ob.data);bmesh.ops.triangulate(bm,faces=list(bm.faces))
    if '_Guide_' in path.name:
        # Keep the existing guide's chunked runtime path. The native importer
        # selects chunking above 12k triangles; planar face splits do not alter
        # the silhouette, seal, or faceted normals.
        count=max(0,(12002-len(bm.faces)+1)//2)
        for face in sorted(list(bm.faces),key=lambda f:f.calc_area(),reverse=True)[:count]:
            vertices=list(face.verts);center=bm.verts.new(sum((v.co for v in vertices),Vector())/3)
            bm.faces.remove(face)
            for k in range(3):bm.faces.new((vertices[k],vertices[(k+1)%3],center))
        bm.verts.ensure_lookup_table();bm.verts.index_update()
    bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces));bm.verts.ensure_lookup_table();bm.verts.index_update()
    assert all(e.is_manifold for e in bm.edges),ob.name
    lines=['# V5 clay relief: world Blender metres']
    lines+=['v %.8f %.8f %.8f'%tuple(ob.matrix_world@v.co+origin) for v in bm.verts]
    # ImportCampaignRock reflects Y and reverses indices. UE GeometryCore uses
    # clockwise outward faces (opposite Blender), so export clockwise here too.
    lines+=['f %d %d %d'%(f.verts[0].index+1,f.verts[2].index+1,f.verts[1].index+1) for f in bm.faces]
    path.write_text('\n'.join(lines)+'\n',encoding='utf-8');count=len(bm.faces);bm.free();return count

report={'passed':False,'regions':[]}
for i,stage in enumerate(STAGES):
    folder=ROOT/'Art/MainGame/RoutesV5'/stage
    bpy.ops.wm.open_mainfile(filepath=str(folder/(stage+'_RoutesV5.blend')))
    paths=json.loads((folder/'centerlines.json').read_text());specs=json.loads((folder/'placement_plan.json').read_text())['objects']
    cavern=tree(bpy.data.objects[f'SM_RoutesV5_{stage}_Cutter'])
    clods=[];records=[];plugs=[];accepted=[]
    for j,spec in enumerate(specs):
        if (i==1 and j in [2,6]) or (i==2 and j in [1,3,5]):continue
        ob=bpy.data.objects[spec['name']];path=paths[spec['path_index']]
        start=sample(path,spec['start_arc_m']);end=sample(path,spec['end_arc_m']);axis=(end-start).normalized()
        # Broad tiers plus smaller clumps on both exposed faces. Move outward
        # only, retaining the original sealing rim and buried mineral hosts.
        moved=0
        for v in ob.data.vertices:
            p=ob.matrix_world@v.co
            d=min((p-start).length,(p-end).length)
            facing=v.normal.dot(axis)
            if abs(facing)<.6:continue
            amount=.08+.28*(.5+.5*noise.noise(p*1.4))+ .10*(.5+.5*noise.noise(p*4.7))
            amount*=min(1,(abs(facing)-.6)/.25)
            v.co+=axis*(amount if facing>0 else -amount);moved+=1
        plugs.append({'index':j,'file':f'{stage}_Guide_{j}.obj','moved_vertices':moved,'triangles':write(ob,OUT/f'{stage}_Guide_{j}.obj',Vector(ORIGINS[i]))})
        for arc,sign in [(spec['start_arc_m'],-1),(spec['end_arc_m'],1)]:
            # Continuous axial/azimuthal sampling: no rings or angular slots.
            # Random patches leave bare wall between denser, asymmetric streaks.
            reach=rng.uniform(3.0,4.1)
            clusters=[(rng.uniform(0,math.tau),rng.uniform(.22,.65)) for _ in range(rng.randint(2,4))]
            for attempt in range(rng.randint(63,83)):
                    offset=.08+rng.expovariate(1.30)
                    if offset>reach:continue
                    s=arc+sign*offset
                    if s<.05 or s>path['arc'][-1]-.05:continue
                    p=sample(path,s);t=(sample(path,s+.1)-sample(path,s-.1)).normalized()
                    u=t.cross(Vector((0,0,1)))
                    if u.length<.1:u=t.cross(Vector((0,1,0)))
                    u.normalize();v=t.cross(u).normalized()
                    if rng.random()<.72:
                        center,spread=rng.choice(clusters);a=rng.gauss(center,spread)
                    else:a=rng.uniform(0,math.tau)
                    direction=u*math.cos(a)+v*math.sin(a)
                    hit,n,_,distance=cavern.ray_cast(p,direction,15)
                    if hit is None:continue
                    if n.dot(direction)>0:n=-n
                    taper=1-.40*min(1,offset/reach)
                    width=rng.uniform(.16,.49)*taper;depth=rng.uniform(.11,.30)*taper
                    if n.z>.55:depth=min(depth,.07)
                    if any((hit-q).length<width+w+.025 for q,w in accepted):continue
                    bpy.ops.mesh.primitive_uv_sphere_add(segments=rng.choice([6,7,8]),ring_count=4,radius=1,location=hit+n*depth*.08)
                    clod=bpy.context.object;clod.name='Clay_Clod';clod.rotation_euler=n.to_track_quat('Z','Y').to_euler();clod.scale=(width,width*rng.uniform(.58,1.12),depth)
                    # Unequal vertices break the repeated pebble silhouette.
                    for vert in clod.data.vertices:vert.co*=rng.uniform(.82,1.12)
                    clods.append(clod);accepted.append((hit,width))
                    records.append({'position':list(hit+Vector(ORIGINS[i])),'normal':list(n),'depth_m':depth,'guide':j,'distance_from_plug_m':offset,'side':sign,'width_m':width})
    groups=[];count=0
    for j in sorted(set(r['guide'] for r in records)):
        group=[ob for ob,r in zip(clods,records) if r['guide']==j]
        bpy.ops.object.select_all(action='DESELECT')
        for ob in group:ob.select_set(True)
        bpy.context.view_layer.objects.active=group[0];bpy.ops.object.join();joined=bpy.context.object
        name=f'{stage}_WallClods_{j}.obj';triangles=write(joined,OUT/name,Vector(ORIGINS[i]));count+=triangles
        groups.append({'file':name,'guide':j,'clods':len(group),'triangles':triangles})
    old=OUT/f'{stage}_WallClods.obj'
    if old.exists():old.unlink()
    report['regions'].append({'stage':stage,'plugs':plugs,'clods':len(records),'triangles':count,'clod_meshes':groups,'samples':records})
    print('CLAY_RELIEF',stage,len(records),flush=True)
report['density_bands']=[sum(lo<=s['distance_from_plug_m']<hi for r in report['regions'] for s in r['samples']) for lo,hi in [(0,1),(1,2),(2,3),(3,5)]]
assert all(a>b for a,b in zip(report['density_bands'],report['density_bands'][1:])),report['density_bands']
report['passed']=True;(OUT/'manifest.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
