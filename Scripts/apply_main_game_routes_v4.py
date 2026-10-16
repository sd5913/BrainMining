"""Install angular surfaces without translating any room or route endpoint."""
import unreal,json,sys,math,hashlib,traceback
from pathlib import Path
ROOT=Path(unreal.Paths.project_dir()).resolve();sys.path.insert(0,str(ROOT/'Scripts'))
import main_game_room_entrances_geometry as entrance
OUT=ROOT/'Saved/Validation/MainGameRoutesV4';ART=ROOT/'Art/MainGame/RoutesV4'
BEFORE=json.loads((OUT/'before.json').read_text(encoding='utf-8'))
STAGES=['TunnelV1','TunnelCavernV2','TunnelStalagmiteV3','TunnelNeuralV4']
I=unreal.Transform();NEVER=unreal.PropertyAccessChangeNotifyMode.NEVER
ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem);levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
state_path=OUT/'authoring.json';state=json.loads(state_path.read_text()) if state_path.exists() else dict(passed=False,completed=[],backup=BEFORE['backup'])
def log(**kw):
 state.update(kw);state_path.write_text(json.dumps(state,indent=2),encoding='utf-8');unreal.log('LOWPOLY_ROUTES '+json.dumps(kw))
def V(p):return unreal.Vector(*p)
def tag(a,tags):a.set_editor_property('tags',list(a.tags)+[t for t in tags if not a.actor_has_tag(t)],notify_mode=NEVER)
def cut_box(shell,center,size):
 c=shell.get_component_by_class(unreal.DynamicMeshComponent);mesh=c.get_dynamic_mesh();tool=unreal.DynamicMesh();local=unreal.MathLibrary.inverse_transform_location(shell.get_actor_transform(),center)
 unreal.GeometryScript_Primitives.append_box(tool,unreal.GeometryScriptPrimitiveOptions(),unreal.Transform(location=local),dimension_x=size.x,dimension_y=size.y,dimension_z=size.z,origin=unreal.GeometryScriptPrimitiveOriginMode.CENTER)
 unreal.GeometryScript_MeshBooleans.apply_mesh_boolean(mesh,I,tool,I,unreal.GeometryScriptBooleanOperation.SUBTRACT,unreal.GeometryScriptMeshBooleanOptions(fill_holes=True,simplify_output=False))
 assert unreal.GeometryScript_MeshQueries.get_is_closed_mesh(mesh);c.update_collision(False)
try:
 assert not ed.get_game_world();assert levels.set_current_level_by_name('Lvl_MainGame');assert not state['passed']
 for stage in STAGES:assert json.loads((ART/stage/'progress.json').read_text())['passed'],stage
 existing=list(actors.get_all_level_actors());hall=next(a for a in existing if isinstance(a,unreal.MiningExhibitHall))
 regions=[next(a for a in existing if isinstance(a,unreal.MiningExternalRegion) and a.actor_has_tag('BM_Campaign_MainGame%02d'%(i+1))) for i in range(4)]
 cfg=json.loads((ROOT/'Config/MainGameRooms.json').read_text(encoding='utf-8'))
 helper=(ROOT/'Scripts/create_main_game.py').read_text(encoding='utf-8').split('\ntry:\n')[0];ns={'__name__':'route_import_helpers'};exec(compile(helper,'route_import_helpers','exec'),ns)
 for i,stage in enumerate(STAGES):
  if stage in state['completed']:continue
  r=regions[i];origin=V(BEFORE['regions'][i]['cutter_origin']);ns['ART']=ART/stage
  mesh=ns['import_fbx']('Tunnel_Cutter.fbx','/Game/MainGame/Models/RoutesV4/'+stage+'/Tunnel',1)[0]
  plugs=ns['import_fbx']('Tunnel_Blockers.fbx','/Game/MainGame/Models/RoutesV4/'+stage+'/Blockers',8)
  # Existing bounds, origin, route centerlines and all gameplay interfaces stay fixed.
  r.set_editor_property('cutters',[unreal.MiningExternalCutter(mesh=mesh,local_transform=unreal.Transform(location=origin-r.get_actor_location()))],notify_mode=NEVER)
  r.set_editor_property('guide_layers',[unreal.MiningExternalGuideLayer(mesh=m) for m in plugs],notify_mode=NEVER)
  success=False
  for chunk in [r.chunk_length,1800.,1700.,1900.,1600.]:
   r.set_editor_property('chunk_length',chunk,notify_mode=NEVER)
   if r.generate_region():success=True;break
  assert success,str(r.build_status);assert r.generate_guide_layers(),str(r.guide_build_status)
  assert len(r.generated_guide_layers)==8
  for a in [r,r.generated_rock,r.generated_shell]+list(r.generated_guide_layers):tag(a,['BM_Campaign_MainGame%02d'%(i+1),'BM_MainGame_LowpolyRoutesV4'])
  r.apply_hardness_settings()
  for spec in cfg['generated_connections']:
   if spec['mine']!=i:continue
   mouth=origin.x+(-6500 if spec['side']=='entry' else 6500);floor=min(entrance.floor_at(spec,mouth-400),entrance.floor_at(spec,mouth+400))
   cut_box(r.generated_shell,V([mouth,spec['a'][1],floor+500]),unreal.Vector(800,1100,1500))
  if i==0:cut_box(r.generated_shell,origin+unreal.Vector(-6345,0,6240),unreal.Vector(390,640,481))
  assert levels.save_current_level();state['completed'].append(stage);log(phase='saved_'+stage)
 config=json.loads((ROOT/'Config/MainGameRoutes.json').read_text());config.update(revision=4,source_root='Art/MainGame/RoutesV4',surface_style='angular-lowpoly',guide_slots=[8]*4,active_guide_indices=[list(range(8)) for i in range(4)])
 (ROOT/'Config/MainGameRoutes.json').write_text(json.dumps(config,indent=2),encoding='utf-8')
 for old,new in zip(BEFORE['areas'],hall.areas):
  assert new.entry.translation.distance(V(old['entry']))<.01 and new.exit.distance(V(old['exit']))<.01
 for p,digest in BEFORE['source_hashes'].items():assert hashlib.sha256((ROOT/'Content'/(p[6:]+'.umap')).read_bytes()).hexdigest()==digest,p
 assert levels.save_current_level();log(passed=True,phase='geometry_saved',all_room_interfaces_unchanged=True)
except Exception:log(passed=False,error=traceback.format_exc());raise
