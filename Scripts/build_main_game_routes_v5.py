"""Background Blender authoring; reuse the project's validated solid/plug pipeline."""
import sys,copy,json,textwrap
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'Scripts'))
import main_game_routes_v5_plan as planner
code=(ROOT/'Scripts/build_main_game_routes_v3.py').read_text(encoding='utf-8').split('wanted=next(')[0]
code=code.replace("source=source[:source.index(\"if __name__=='__main__':\")]", "source=source[:source.index(\"if __name__=='__main__':\")]\nsource=transform_source(source)")
def transform_source(source):
 source=source.replace("OUTROOT=BASE/'RoutesV3'","OUTROOT=BASE/'RoutesV5'").replace('SM_RoutesV2_','SM_RoutesV5_').replace('_RoutesV2.blend','_RoutesV5.blend')
 source=source.replace('seed=20261015+index*100+i*191)', 'seed=20261015+index*100+i*191,bomb_only=(index==1 and i in [3,7]) or (index==2 and i in [2,4,6]))')
 source=source.replace("half=(2.8 if r['path_index']==0 else 2.2)/(.88*2)","half=(1.2 if r.get('bomb_only') else 2.8 if r['path_index']==0 else 2.2)/(.88*2)")
 source=source.replace('smooth.factor=.65;smooth.iterations=4','smooth.factor=.3;smooth.iterations=1').replace('.10*noise.noise(v.co/2.1','(.42 if abs(v.co.x)<55 else .08)*noise.noise(v.co/2.1').replace('17000/len(tunnel.data.polygons)','6500/len(tunnel.data.polygons)')
 a=source.index('    if index:\n        if index==1:step=');b=source.index('    clean(tunnel);before_faces',a)
 source=source[:a]+"    add_scenery(tunnel,paths,spec,out,index)\n"+source[b:]
 # Put the two region-2 soil plugs in the vertical chamber exits.
 marker="    records += [dict(path_index=i,path_name=p['name']"
 a=source.index(marker)
 source=source[:a]+"    if index==1:\n        for j,target in enumerate(spec['upward_gates']):\n            arc=float(main['arc'][np.argmin(np.linalg.norm(np.array(main['points'])-target,axis=1))]);records[j].update(allowed_start_m=arc-2,allowed_end_m=arc+2)\n        records.sort(key=lambda r:r['allowed_start_m'])\n"+source[a:]
 return source
ns={'__file__':str(ROOT/'Scripts/build_main_game_routes_v3.py'),'__name__':'v5_helpers','transform_source':transform_source}
exec(compile(code,'v5_helpers','exec'),ns);scope=ns['scope'];scope['plan']=planner.plan
bpy=scope['bpy'];np=scope['np'];Vector=scope['Vector'];bmesh=scope['bmesh']
tube_code=(ROOT/'Art/Mining/TunnelWideV2/build_wide.py').read_text().split('def materials(')[0].replace('sides=40','sides=9')
tube_ns=dict(scope);exec(compile(tube_code,'angular_tube','exec'),tube_ns);original_tube=scope['wide']['tube']
def irregular_tube(path):
 if 'arc' not in path:return original_tube(path)
 p=copy.deepcopy(path);arc=np.array(p['arc']);s=np.unique(np.r_[np.arange(0,arc[-1],1.15),arc[-1]])
 for key in ['points','half_width','half_height']:
  data=np.array(p[key]);p[key]=np.column_stack([np.interp(s,arc,data[:,j]) for j in range(3)]) if data.ndim==2 else np.interp(s,arc,data)
 obj=tube_ns['tube'](p)
 # Independent broad wall bulges make asymmetry survive voxel union/decimation.
 for v in obj.data.vertices:
  if abs(v.co.x)>55:continue
  nearest=int(np.argmin(np.linalg.norm(p['points']-np.array(v.co),axis=1)));center=Vector(p['points'][nearest]);delta=v.co-center
  v.co=center+delta*(1+.11*scope['noise'].noise(v.co/3.2+Vector((2,9,4))))
 return obj
scope['wide']['tube']=irregular_tube
def add_scenery(tunnel,paths,features,out,index):
 scene=bpy.context.scene;platforms=[];spires=[]
 tunnel.data.calc_loop_triangles()
 from mathutils.bvhtree import BVHTree
 tree=BVHTree.FromPolygons([v.co[:] for v in tunnel.data.vertices],[tuple(t.vertices) for t in tunnel.data.loop_triangles],all_triangles=True)
 for n,spec in enumerate(features['spires']):
  direction=Vector((0,0,1 if spec['ceiling'] else -1));hit=tree.ray_cast(Vector(spec['center_m']),direction,12)
  assert hit[0] is not None,spec
  height=spec['height_m'];center=hit[0]-direction*(height*.5-.22)
  bpy.ops.mesh.primitive_cone_add(vertices=7,radius1=0 if spec['ceiling'] else spec['radius_m'],radius2=spec['radius_m'] if spec['ceiling'] else 0,depth=height,location=center)
  ob=bpy.context.object;ob.name=f'Stalactite_{n:02}' if spec['ceiling'] else f'Stalagmite_{n:02}'
  bpy.context.view_layer.objects.active=tunnel;mod=tunnel.modifiers.new('Grounded_Cave_Spire','BOOLEAN');mod.operation='DIFFERENCE';mod.solver='EXACT';mod.object=ob;bpy.ops.object.modifier_apply(modifier=mod.name)
  spec['base_m']=list(hit[0]);spires.append(ob)
 for spec in features['platforms']:
  top=spec['top_m'];base=spec['base_z_m'];height=top[2]-base
  bpy.ops.mesh.primitive_cone_add(vertices=7,radius1=spec['radius_m']*1.4,radius2=spec['radius_m'],depth=height,location=(top[0],top[1],(top[2]+base)/2))
  ob=bpy.context.object;ob.name=spec['name'];bpy.ops.object.transform_apply(location=True,rotation=False,scale=True);platforms.append(ob)
 # Reuse the original cavern and neural geometry generators, with new sites.
 for key,filename,var in [('pillars','TunnelCavernV2/build_cavern.py','pillar_step'),('neural','TunnelNeuralV4/build_neural.py','neural_step')]:
  if not features[key]:continue
  datafile='pillars.json' if key=='pillars' else 'neural_rocks.json';scope['save'](out/datafile,features[key])
  step=scope['literal'](var,ROOT/'Art/MainGame'/filename)
  local=dict(OUT=out,tunnel=tunnel,wide=scope['wide'],validation=scope['validation'],bpy=bpy,bmesh=bmesh,json=json,math=scope['math'],np=np,Vector=Vector,noise=scope['noise'],scene=scene)
  exec(compile(textwrap.dedent(step),'reused_landscape','exec'),local)
  if key=='neural':
   from routes_v5_neural_detail import export_detail
   export_detail(local['pillar_objects'],out)
  for ob in local['pillar_objects']:ob.hide_render=True;ob.hide_set(True)
 if platforms:scope['export'](platforms,out/'Platforms.fbx')
 for ob in platforms+spires:ob.hide_render=True;ob.hide_set(True)
 scope['save'](out/'features.json',features)
scope['add_scenery']=add_scenery
original_seal=scope['seal_final']
def seal_lowpoly(obj):
 original_seal(obj)
 if len(obj.data.polygons)<=24000:return
 previous=obj.data.copy();bpy.context.view_layer.objects.active=obj
 mod=obj.modifiers.new('Broad_Final_Facets','DECIMATE');mod.ratio=24000/len(obj.data.polygons);mod.use_collapse_triangulate=True;bpy.ops.object.modifier_apply(modifier=mod.name);scope['clean'](obj)
 top=scope['validation']['topology'](obj.data);top.update(scope['validation']['intersections'](obj.data))
 if any(top[k] for k in ['boundary_edges','nonmanifold_edges','duplicate_faces','degenerate_faces','self_intersection_pairs']):obj.data=previous
scope['seal_final']=seal_lowpoly
wanted=next((a.split('=',1)[1] for a in sys.argv if a.startswith('--stage=')),None)
for index,stage in enumerate(planner.STAGES):
 if wanted and stage!=wanted:continue
 scope['build'](stage,index)
