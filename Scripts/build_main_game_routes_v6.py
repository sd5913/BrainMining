"""Preserve V5 sources; derive taller chambers and grounded mushroom platforms."""
import bpy,bmesh,json,sys,math,copy,shutil
from pathlib import Path
from mathutils import Vector
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'Scripts'))
from main_game_routes_v6_shape import point,weights
SRC=ROOT/'Art/MainGame/RoutesV5';OUT=ROOT/'Art/MainGame/RoutesV6';OUT.mkdir(parents=True,exist_ok=True)
STAGES=['TunnelV1','TunnelCavernV2','TunnelStalagmiteV3']
for stage in STAGES:
    folder=OUT/stage;folder.mkdir(exist_ok=True)
    for name in ['centerlines.json','features.json','placement_plan.json','plan_summary.json','neural_rocks.json']:
        src=SRC/stage/name
        if src.exists():shutil.copy2(src,folder/src.name)
stage=STAGES[1];folder=OUT/stage
paths=json.loads((SRC/stage/'centerlines.json').read_text());old=copy.deepcopy(paths)
for p in paths:
    p['points']=[point(q) for q in p['points']];arc=[0.]
    for a,b in zip(p['points'],p['points'][1:]):arc.append(arc[-1]+math.dist(a,b))
    p['arc']=arc
    p['half_height']=[h*(1+.7*sum(weights(q[0]))) for h,q in zip(p['half_height'],p['points'])]
    p['radii']=[max(a,b)+.45 for a,b in zip(p['half_width'],p['half_height'])]
import bisect
def arc(index,s):
    a=old[index]['arc'];b=paths[index]['arc'];k=max(1,min(len(a)-1,bisect.bisect_left(a,s)));f=(s-a[k-1])/(a[k]-a[k-1]);return b[k-1]+f*(b[k]-b[k-1])
for p in paths[1:]:p['root_arc']=arc(0,p['root_arc'])
(folder/'centerlines.json').write_text(json.dumps(paths,indent=2))
plan=json.loads((SRC/stage/'placement_plan.json').read_text())
for spec in plan['objects']:
    for k in ['start_arc_m','end_arc_m','allowed_start_m','allowed_end_m','arc_m']:
        if k in spec:spec[k]=arc(spec['path_index'],spec[k])
    for k in ['center_m','start_m','end_m']:
        if k in spec:spec[k]=point(spec[k])
(folder/'placement_plan.json').write_text(json.dumps(plan,indent=2))
features=json.loads((SRC/stage/'features.json').read_text())
for p in features['platforms']:
    ci=p['chamber'];n=int(p['name'].rsplit('_',1)[1])-1;center=76 if ci==0 else 100
    if ci==1 and n==4:p['top_m'][0]=19.5;p['top_m'][1]=2.3
    p['top_m'][2]=center-5+n*2.4
    p['base_z_m']=point([p['top_m'][0],p['top_m'][1],p['base_z_m']])[2]
    p['top_m']=point(p['top_m']);p.update(radius_m=1.75,stem_radius_m=.55,cap_depth_m=1.2)
features['upward_gates']=[point(p) for p in features['upward_gates']]
(folder/'features.json').write_text(json.dumps(features,indent=2))
summary=json.loads((folder/'plan_summary.json').read_text());summary.update(style='tall-mushroom-jetpack-chambers',main_length_m=paths[0]['arc'][-1],cavern_centers_arc_m=[arc(0,s) for s in summary['cavern_centers_arc_m']]);(folder/'plan_summary.json').write_text(json.dumps(summary,indent=2))
bpy.ops.wm.open_mainfile(filepath=str(SRC/stage/(stage+'_RoutesV5.blend')))
# Deform the original cutter/soil source as well as the runtime-derived geometry.
for ob in list(bpy.data.objects):
    if ob.type!='MESH':continue
    for v in ob.data.vertices:v.co=ob.matrix_world.inverted()@Vector(point(ob.matrix_world@v.co))
platforms=[]
for spec in features['platforms']:
    existing=bpy.data.objects.get(spec['name'])
    if existing:bpy.data.objects.remove(existing,do_unlink=True)
    x,y,z=spec['top_m'];base=spec['base_z_m'];r=spec['radius_m'];stem=spec['stem_radius_m']
    # One watertight faceted mushroom: narrow grounded stem, flared shoulder,
    # wide level cap. No overlapping primitive cones or hidden internal faces.
    rings=[(base,stem*1.3),(z-2,stem),(z-1.2,r*.78),(z-.45,r),(z,r*.97)]
    verts=[];faces=[];sides=9
    for h,radius in rings:
        verts.extend((x+radius*math.cos(k*math.tau/sides),y+radius*math.sin(k*math.tau/sides),h) for k in range(sides))
    faces.append(tuple(reversed(range(sides))))
    for j in range(len(rings)-1):
        for k in range(sides):faces.append((j*sides+k,j*sides+(k+1)%sides,(j+1)*sides+(k+1)%sides,(j+1)*sides+k))
    faces.append(tuple((len(rings)-1)*sides+k for k in range(sides)))
    mesh=bpy.data.meshes.new(spec['name']);mesh.from_pydata(verts,[],faces);mesh.update()
    ob=bpy.data.objects.new(spec['name'],mesh);bpy.context.collection.objects.link(ob);platforms.append(ob)
    bm=bmesh.new();bm.from_mesh(mesh);assert all(e.is_manifold for e in bm.edges);bm.free()
bpy.ops.object.select_all(action='DESELECT')
for ob in platforms:ob.hide_set(False);ob.hide_render=False;ob.select_set(True)
bpy.context.view_layer.objects.active=platforms[0]
bpy.ops.export_scene.fbx(filepath=str(folder/'Platforms.fbx'),use_selection=True,object_types={'MESH'},apply_unit_scale=True,apply_scale_options='FBX_SCALE_UNITS',axis_forward='-Y',axis_up='Z',mesh_smooth_type='FACE',add_leaf_bones=False,bake_anim=False)
bpy.ops.object.select_all(action='DESELECT')
cutter=bpy.data.objects['SM_RoutesV5_'+stage+'_Cutter'];cutter.hide_set(False);cutter.select_set(True);bpy.context.view_layer.objects.active=cutter
bpy.ops.export_scene.fbx(filepath=str(folder/'Tunnel_Cutter.fbx'),use_selection=True,object_types={'MESH'},apply_unit_scale=True,apply_scale_options='FBX_SCALE_UNITS',axis_forward='-Y',axis_up='Z',mesh_smooth_type='FACE',add_leaf_bones=False,bake_anim=False)
bpy.ops.wm.save_as_mainfile(filepath=str(folder/(stage+'_RoutesV6.blend')))
report={'passed':True,'platforms':features['platforms'],'adjacent_rises_m':[features['platforms'][i+1]['top_m'][2]-features['platforms'][i]['top_m'][2] for i in [0,1,2,3,5,6,7,8]]}
assert all(3.9<h<4.3 for h in report['adjacent_rises_m']),report
(OUT/'authoring.json').write_text(json.dumps(report,indent=2));print('ROUTES_V6_ART_READY',flush=True)
