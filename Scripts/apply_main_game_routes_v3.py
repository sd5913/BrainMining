"""Install regional identities and translate downstream linked rooms, never source maps."""
import unreal,json,sys,math,hashlib,traceback
from pathlib import Path
ROOT=Path(unreal.Paths.project_dir()).resolve();sys.path.insert(0,str(ROOT/'Scripts'));import main_game_room_entrances_geometry as entrance
OUT=ROOT/'Saved/Validation/MainGameRoutesV3';ART=ROOT/'Art/MainGame/RoutesV3';BEFORE=json.loads((OUT/'before.json').read_text(encoding='utf-8'));STAGES=['TunnelV1','TunnelCavernV2','TunnelStalagmiteV3','TunnelNeuralV4'];D=unreal.Vector(0,0,10000);I=unreal.Transform();NEVER=unreal.PropertyAccessChangeNotifyMode.NEVER
ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem);levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
state_path=OUT/'authoring.json';state=json.loads(state_path.read_text(encoding='utf-8')) if state_path.exists() else {'passed':False,'completed':[],'backup':BEFORE['backup']}
def log(**kw):state.update(kw);state_path.write_text(json.dumps(state,ensure_ascii=False,indent=2),encoding='utf-8');unreal.log('REGIONAL_ROUTES '+json.dumps(kw,ensure_ascii=False))
def V(p):return unreal.Vector(*p)
def raw(p):return unreal.Vector(p[0]*100,-p[1]*100,p[2]*100)
def shift_t(t):t=t.copy();t.translation+=D;return t
def shift_box(b):return unreal.Box(min=b.min+D,max=b.max+D)
def union(a,b):return unreal.Box(min=V([min(x,y) for x,y in zip(a.min.to_tuple(),b.min.to_tuple())]),max=V([max(x,y) for x,y in zip(a.max.to_tuple(),b.max.to_tuple())]))
def tag(a,tags):a.set_editor_property('tags',list(a.tags)+[t for t in tags if not a.actor_has_tag(t)],notify_mode=NEVER)
def cut_box(shell,center,size):
 c=shell.get_component_by_class(unreal.DynamicMeshComponent);mesh=c.get_dynamic_mesh();tool=unreal.DynamicMesh();local=unreal.MathLibrary.inverse_transform_location(shell.get_actor_transform(),center)
 unreal.GeometryScript_Primitives.append_box(tool,unreal.GeometryScriptPrimitiveOptions(),unreal.Transform(location=local),dimension_x=size.x,dimension_y=size.y,dimension_z=size.z,origin=unreal.GeometryScriptPrimitiveOriginMode.CENTER)
 unreal.GeometryScript_MeshBooleans.apply_mesh_boolean(mesh,I,tool,I,unreal.GeometryScriptBooleanOperation.SUBTRACT,unreal.GeometryScriptMeshBooleanOptions(fill_holes=True,simplify_output=False));assert unreal.GeometryScript_MeshQueries.get_is_closed_mesh(mesh);c.update_collision(False)
try:
 assert not ed.get_game_world();assert ed.get_editor_world().get_path_name()=='/Game/MainGame/Maps/Lvl_MainGame.Lvl_MainGame';assert levels.set_current_level_by_name('Lvl_MainGame');assert not state['passed']
 for stage in STAGES[:3]:assert json.loads((ART/stage/'progress.json').read_text())['passed'],stage
 existing=list(actors.get_all_level_actors());hall=next(a for a in existing if isinstance(a,unreal.MiningExhibitHall));regions=[next(a for a in existing if isinstance(a,unreal.MiningExternalRegion) and a.actor_has_tag('BM_Campaign_MainGame%02d'%(i+1))) for i in range(4)];cfg=json.loads((ROOT/'Config/MainGameRooms.json').read_text(encoding='utf-8'))
 if not state.get('moved'):
  # Native generated children inherit the parent motion. Move only the highest
  # selected parent so no attached source or collision mesh receives it twice.
  tags=['BM_Campaign_MainGame03','BM_Campaign_MainGame04','BM_Campaign_MainGameJourney','BM_Campaign_MainGameMuseum'];move=[]
  for a in existing:
   if a.get_level()!=hall.get_level() or a==hall:continue
   label=a.get_actor_label();outlet=label.startswith('LINK_Mine02_Journey') or (label.startswith('LINK_BM_Campaign_MainGame02_') and a.get_actor_location().x>regions[1].get_actor_location().x)
   if outlet or any(a.actor_has_tag(t) for t in tags):move.append(a)
  for a in move:
   p=a.get_attach_parent_actor();covered=False
   while p:
    if p in move:covered=True;break
    p=p.get_attach_parent_actor()
   if not covered:a.set_actor_transform(shift_t(a.get_actor_transform()),False,True)
  areas=list(hall.areas)
  for i,a in enumerate(areas):
   if i>=3:
    for k in ['origin','exit']:a.set_editor_property(k,a.get_editor_property(k)+D)
    a.set_editor_property('entry',shift_t(a.entry));a.set_editor_property('map_route',[p+D for p in a.map_route]);a.set_editor_property('mining_bounds',shift_box(a.mining_bounds));a.set_editor_property('activation_bounds',shift_box(a.activation_bounds))
    if a.source_level:a.source_level.set_editor_property('level_transform',shift_t(a.source_level.get_editor_property('level_transform')),notify_mode=NEVER)
   elif i==2:
    a.set_editor_property('exit',a.exit+D);route=list(a.map_route);route[-1]=a.exit;a.set_editor_property('map_route',route)
  hall.set_editor_property('areas',areas,notify_mode=NEVER)
  for i in [2,3]:
   for k in ['mine_origins','mine_deltas','mine_entry_positions']:cfg[k][i][2]+=D.z
  for r in cfg['rooms'][1:]:
   for k in ['origin','entry','exit']:r[k][2]+=D.z
   for b in r['bounds']:b[2]+=D.z
  for key in ['connections','generated_connections']:
   for s in cfg[key][2:]:
    for k in ['a','b']:s[k][2]+=D.z
    if 'floor_samples' in s:
     for p in s['floor_samples']:p[1]+=D.z
  # Existing fourth-region clay remains physically identical after translation.
  mat=regions[3].rock_material;values=list(mat.get_editor_property('vector_parameter_values'))
  for v in values:
   if str(v.parameter_info.name).startswith(('MudStart','MudEnd')):
    c=v.parameter_value;v.set_editor_property('parameter_value',unreal.LinearColor(c.r,c.g,c.b+D.z,c.a))
  mat.set_editor_property('vector_parameter_values',values,notify_mode=NEVER);unreal.MaterialEditingLibrary.update_material_instance(mat);assert unreal.EditorAssetLibrary.save_loaded_asset(mat,False)
  cfg.update(regional_routes_revision=3,downstream_rise_cm=10000);(ROOT/'Config/MainGameRooms.json').write_text(json.dumps(cfg,ensure_ascii=False,indent=2),encoding='utf-8');tag(hall,['BM_MainGame_RegionalRoutesV3_Shifted']);assert levels.save_current_level();log(moved=True,phase='downstream_shift_saved')
  assert levels.load_level('/Game/MainGame/Maps/Lvl_MainGame');existing=list(actors.get_all_level_actors());hall=next(a for a in existing if isinstance(a,unreal.MiningExhibitHall));regions=[next(a for a in existing if isinstance(a,unreal.MiningExternalRegion) and a.actor_has_tag('BM_Campaign_MainGame%02d'%(i+1))) for i in range(4)]
 helper=(ROOT/'Scripts/create_main_game.py').read_text(encoding='utf-8').split('\ntry:\n')[0];ns={'__name__':'route_import_helpers'};exec(compile(helper,'route_import_helpers','exec'),ns)
 for i,stage in enumerate(STAGES[:3]):
  if stage in state['completed']:continue
  r=regions[i];origin=V(BEFORE['regions'][i]['cutter_origin'])+(D if i>=2 else unreal.Vector());ns['ART']=ART/stage;mesh=ns['import_fbx']('Tunnel_Cutter.fbx','/Game/MainGame/Models/RoutesV3/'+stage+'/Tunnel',1)[0];plugs=ns['import_fbx']('Tunnel_Blockers.fbx','/Game/MainGame/Models/RoutesV3/'+stage+'/Blockers',8)
  bounds=mesh.get_bounding_box();lo=origin+bounds.min;hi=origin+bounds.max;center=V([origin.x,(lo.y+hi.y)/2,(lo.z+hi.z)/2]);size=V([12500,math.ceil((hi.y-lo.y+900)/100)*100,math.ceil((hi.z-lo.z+800)/100)*100]);r.set_actor_location(center,False,True);r.set_editor_property('rock_size',size,notify_mode=NEVER);r.set_editor_property('cutters',[unreal.MiningExternalCutter(mesh=mesh,local_transform=unreal.Transform(location=origin-center))],notify_mode=NEVER);r.set_editor_property('guide_layers',[unreal.MiningExternalGuideLayer(mesh=m) for m in plugs],notify_mode=NEVER)
  success=False
  for chunk in [1800.,1700.,1900.,1600.]:
   r.set_editor_property('chunk_length',chunk,notify_mode=NEVER)
   if r.generate_region():success=True;break
  assert success,str(r.build_status);assert r.generate_guide_layers(),str(r.guide_build_status);assert len(r.generated_guide_layers)==8
  area_tag='BM_Campaign_MainGame%02d'%(i+1)
  for a in [r,r.generated_rock,r.generated_shell]+list(r.generated_guide_layers):tag(a,[area_tag,'BM_MainGame_RegionalRoutesV3'])
  r.apply_hardness_settings();paths=json.loads((ART/stage/'centerlines.json').read_text());areas=list(hall.areas);area=areas[i*2];route=[origin+raw(p) for p in paths[0]['points']];route.append(area.exit);area.set_editor_property('map_route',route);area.set_editor_property('origin',center);box=unreal.Box(min=center-size*.5-unreal.Vector(250,250,250),max=center+size*.5+unreal.Vector(250,250,250));area.set_editor_property('mining_bounds',union(area.mining_bounds,box));area.set_editor_property('activation_bounds',union(area.activation_bounds,box));hall.set_editor_property('areas',areas,notify_mode=NEVER)
  # Reapply the natural-room apertures on the newly generated bedrock.
  for s in cfg['generated_connections']:
   if s['mine']!=i:continue
   mouth=origin.x+(-6500 if s['side']=='entry' else 6500);floor=min(entrance.floor_at(s,mouth-400),entrance.floor_at(s,mouth+400));cut_box(r.generated_shell,V([mouth,s['a'][1],floor+500]),unreal.Vector(800,1100,1500))
  if i==0:cut_box(r.generated_shell,origin+unreal.Vector(-6345,0,6240),unreal.Vector(390,640,481))
  assert levels.save_current_level();state['completed'].append(stage);log(phase='saved_'+stage)
 (ROOT/'Config/MainGameRoutes.json').write_text(json.dumps(dict(revision=3,source_root='Art/MainGame/RoutesV3',stages=STAGES,styles=['narrow-branches','ascending-caverns','descending-spiral','mixed-expedition'],guide_slots=[8,8,8,16],active_guide_indices=[list(range(8))]*3+[[0,2,4,6,8,10,12,14]],downstream_rise_cm=10000),indent=2),encoding='utf-8')
 for p,digest in BEFORE['source_hashes'].items():assert hashlib.sha256((ROOT/'Content'/(p[6:]+'.umap')).read_bytes()).hexdigest()==digest,p
 assert regions[3].generated_rock.triangle_count==BEFORE['regions'][3]['triangles'];assert levels.save_current_level();log(passed=True,phase='geometry_saved',fourth_region_geometry_retained=True,error=None)
except Exception:log(passed=False,error=traceback.format_exc());raise
