"""Install RoutesV5 into MainGame only; source puzzle/office maps remain untouched."""
import unreal,json,sys,math,traceback,shutil,hashlib
from pathlib import Path
ROOT=Path(unreal.Paths.project_dir()).resolve();sys.path.insert(0,str(ROOT/'Scripts'))
import campaign_minerals_api as api
import main_game_room_entrances_geometry as entrances
STAGES=['TunnelV1','TunnelCavernV2','TunnelStalagmiteV3'];ART=ROOT/'Art/MainGame/RoutesV5';OUT=ROOT/'Saved/Validation/MainGameRoutesV5'
ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem);levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
NEVER=unreal.PropertyAccessChangeNotifyMode.NEVER;I=unreal.Transform();report={'passed':False,'completed':[]}
def log(**kw):
 report.update(kw);(OUT/'authoring.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8');unreal.log('ROUTES_V5 '+json.dumps(kw,ensure_ascii=False))
def setp(a,k,v):
 if isinstance(a,unreal.Object):a.set_editor_property(k,v,notify_mode=NEVER)
 else:a.set_editor_property(k,v)
def tag(a,*tags):setp(a,'tags',list(a.tags)+[t for t in tags if not a.actor_has_tag(t)])
def raw(p):return unreal.Vector(p[0]*100,-p[1]*100,p[2]*100)
def material(name,color):
 dest='/Game/MainGame/Materials/RoutesV5';path=dest+'/'+name
 if unreal.EditorAssetLibrary.does_asset_exist(path):return unreal.load_asset(path)
 mat=unreal.AssetToolsHelpers.get_asset_tools().create_asset(name,dest,unreal.Material,unreal.MaterialFactoryNew())
 lib=unreal.MaterialEditingLibrary
 node=lib.create_material_expression(mat,unreal.MaterialExpressionConstant3Vector);node.set_editor_property('constant',unreal.LinearColor(*color,1));lib.connect_material_property(node,'',unreal.MaterialProperty.MP_BASE_COLOR)
 for prop,value in [(unreal.MaterialProperty.MP_ROUGHNESS,1.),(unreal.MaterialProperty.MP_SPECULAR,0.),(unreal.MaterialProperty.MP_METALLIC,0.)]:
  node=lib.create_material_expression(mat,unreal.MaterialExpressionConstant);node.set_editor_property('r',value);lib.connect_material_property(node,'',prop)
 lib.recompile_material(mat);assert unreal.EditorAssetLibrary.save_loaded_asset(mat,False);return mat
def box(label,center,size,mat,area):
 a=actors.spawn_actor_from_class(unreal.StaticMeshActor,center);a.set_actor_label(label);a.static_mesh_component.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Cube'));a.set_actor_scale3d(size/100);a.static_mesh_component.set_material(0,mat);a.static_mesh_component.set_collision_profile_name('BlockAll');tag(a,area,'BM_RoutesV5');return a
def cut_box(shell,center,size):
 c=shell.get_component_by_class(unreal.DynamicMeshComponent);mesh=c.get_dynamic_mesh();tool=unreal.DynamicMesh();local=unreal.MathLibrary.inverse_transform_location(shell.get_actor_transform(),center)
 unreal.GeometryScript_Primitives.append_box(tool,unreal.GeometryScriptPrimitiveOptions(),unreal.Transform(location=local),dimension_x=size.x,dimension_y=size.y,dimension_z=size.z,origin=unreal.GeometryScriptPrimitiveOriginMode.CENTER)
 unreal.GeometryScript_MeshBooleans.apply_mesh_boolean(mesh,I,tool,I,unreal.GeometryScriptBooleanOperation.SUBTRACT,unreal.GeometryScriptMeshBooleanOptions(fill_holes=True,simplify_output=False));assert unreal.GeometryScript_MeshQueries.get_is_closed_mesh(mesh);c.update_collision(False)
def assign_rock_material(a,mat):
 setp(a,'rock_material',mat)
 for c in a.get_components_by_class(unreal.DynamicMeshComponent):c.set_material(0,mat)

try:
 assert not ed.get_game_world();assert levels.load_level('/Game/MainGame/Maps/Lvl_MainGame');assert levels.set_current_level_by_name('Lvl_MainGame')
 all_actors=list(actors.get_all_level_actors());hall=next(a for a in all_actors if isinstance(a,unreal.MiningExhibitHall));field=api.get_field(hall)
 assert not hall.actor_has_tag('BM_RoutesV5'),'Already authored: use verification or restore the recorded pre-change backup.'
 cfg=json.loads((ROOT/'Config/MainGameRooms.json').read_text(encoding='utf-8'))
 source_hashes={r['source']:hashlib.sha256((ROOT/'Content'/(r['source'][6:]+'.umap')).read_bytes()).hexdigest() for r in cfg['rooms']}
 before=json.loads((OUT/'inspection.json').read_text(encoding='utf-8'))
 backup=next((ROOT/'Saved/Backups').glob('MainGameRoutesV5_*'));bpfile=ROOT/'Content/MainGame/Blueprints/BP_MainGameCharacter.uasset'
 if not (backup/bpfile.name).exists():shutil.copy2(bpfile,backup/bpfile.name)
 retained=[dict(id=str(p.treasure_id),value=p.value,tag=str(p.area_tag),progress=p.route_progress,guide=bool(p.guide_layer),ceiling=p.surface.rotation.rotator().pitch<-30) for p in field.get_editor_property('authored_placements')]
 (OUT/'mineral_recipe_before.json').write_text(json.dumps(retained,indent=2),encoding='utf-8')
 mats=[material('M_Earth',(.23,.14,.085)),material('M_BlueViolet',(.115,.09,.29)),material('M_DarkRed',(.18,.024,.028))]
 clay=material('M_DiggableClay',(.29,.16,.075));hard=material('M_BlastOnlyBasalt',(.065,.085,.115));platformmat=material('M_ParkourBasalt',(.075,.055,.14))
 helper=(ROOT/'Scripts/create_main_game.py').read_text(encoding='utf-8').split('\ntry:\n')[0];ns={'__name__':'import_helpers'};exec(compile(helper,'import_helpers','exec'),ns)
 regions=[];areas=list(hall.areas);origins=[]
 for i,stage in enumerate(STAGES):
  assert json.loads((ART/stage/'progress.json').read_text())['passed']
  region=next(a for a in all_actors if isinstance(a,unreal.MiningExternalRegion) and a.actor_has_tag(f'BM_Campaign_MainGame{i+1:02}'));regions.append(region)
  origin=unreal.Vector(*before['regions'][i]['cutter_origin']);origins.append(origin);ns['ART']=ART/stage
  mesh=ns['import_fbx']('Tunnel_Cutter.fbx','/Game/MainGame/Models/RoutesV5/'+stage+'/Tunnel',1)[0]
  plugs=ns['import_fbx']('Tunnel_Blockers.fbx','/Game/MainGame/Models/RoutesV5/'+stage+'/Blockers',8)
  bounds=mesh.get_bounding_box();xmin=-6250.;xmax=6250. if i<2 else 7850.
  center=origin+unreal.Vector((xmin+xmax)/2,(bounds.min.y+bounds.max.y)/2,(bounds.min.z+bounds.max.z)/2)
  size=unreal.Vector(xmax-xmin,math.ceil((bounds.max.y-bounds.min.y+800)/100)*100,math.ceil((bounds.max.z-bounds.min.z+800)/100)*100)
  region.set_actor_location(center,False,True);setp(region,'rock_size',size);setp(region,'rock_material',mats[i]);setp(region,'shell_material',mats[i]);setp(region,'guide_material',clay)
  cutters=[unreal.MiningExternalCutter(mesh=mesh,local_transform=unreal.Transform(location=origin-center))]
  if i==2:
   # Subtract the museum's existing interior from both host and outer shell.
   cutters.append(unreal.MiningExternalCutter(mesh=unreal.load_asset('/Engine/BasicShapes/Cube'),local_transform=unreal.Transform(location=unreal.Vector(48400,11750,14450)-center,scale=unreal.Vector(45,22,8))))
  setp(region,'cutters',cutters);setp(region,'guide_layers',[unreal.MiningExternalGuideLayer(mesh=m) for m in plugs])
  success=False
  for chunk in [1800.,1700.,1900.,1600.,2000.]:
   setp(region,'chunk_length',chunk)
   if region.generate_region():success=True;break
  assert success,str(region.build_status);assert region.generate_guide_layers(),str(region.guide_build_status)
  for a in [region,region.generated_rock,region.generated_shell,*region.generated_guide_layers]:tag(a,f'BM_Campaign_MainGame{i+1:02}','BM_RoutesV5')
  region.apply_hardness_settings()
  # Preserve existing horizontal mouths except the retired museum connector.
  for spec in cfg['generated_connections']:
   if spec['mine']!=i or (i==2 and spec['side']=='exit'):continue
   mouth=origin.x+(-6500 if spec['side']=='entry' else 6500);floor=min(entrances.floor_at(spec,mouth-400),entrances.floor_at(spec,mouth+400))
   cut_box(region.generated_shell,unreal.Vector(mouth,spec['a'][1],floor+500),unreal.Vector(800,1100,1500))
  if i==0:cut_box(region.generated_shell,origin+unreal.Vector(-6345,0,6240),unreal.Vector(390,640,481))
  if i>=1:
   for j in ([2,6] if i==1 else [1,3,5]):
    gate=region.generated_guide_layers[j];tag(gate,'BM_BombOnly');assign_rock_material(gate,hard);gate.set_actor_label(f'MAIN_GAME_BombOnly_{i+1}_{j+1}')
  paths=json.loads((ART/stage/'centerlines.json').read_text());route=[origin+raw(p) for p in paths[0]['points'][::4]];route.append(origin+raw(paths[0]['points'][-1]))
  area=areas[i*2];setp(area,'map_route',route);setp(area,'mining_bounds',unreal.Box(min=center-size/2,max=center+size/2));setp(area,'activation_bounds',unreal.Box(min=center-size/2,max=center+size/2));areas[i*2]=area
  report['completed'].append(stage);log(phase='generated_'+stage)
 # Indestructible, grounded flat-topped pillars in both ascent chambers.
 ns['ART']=ART/STAGES[1];platforms=ns['import_fbx']('Platforms.fbx','/Game/MainGame/Models/RoutesV5/Platforms',10)
 for mesh in platforms:
  body=mesh.get_editor_property('body_setup');body.set_editor_property('collision_trace_flag',unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE);assert unreal.EditorAssetLibrary.save_loaded_asset(mesh,False)
  a=actors.spawn_actor_from_class(unreal.StaticMeshActor,origins[1]);a.set_actor_label('MAIN_GAME_'+mesh.get_name());a.static_mesh_component.set_static_mesh(mesh);a.static_mesh_component.set_material(0,platformmat);a.static_mesh_component.set_collision_profile_name('BlockAll');tag(a,'BM_Campaign_MainGame02','BM_RoutesV5','BM_ParkourPlatform')
 path=json.loads((ART/STAGES[1]/'centerlines.json').read_text())[0]
 for n in range(5,len(path['points'])-5,38):
  p=origins[1]+raw(path['points'][n])+unreal.Vector(0,0,80);a=actors.spawn_actor_from_class(unreal.PointLight,p);a.set_actor_label(f'MAIN_GAME_CrystalLight_{n:03}');tag(a,'BM_Campaign_MainGame02','BM_RoutesV5')
  c=a.point_light_component;c.set_mobility(unreal.ComponentMobility.MOVABLE);c.set_light_color(unreal.LinearColor(.42,.38,.8) if n%3 else unreal.LinearColor(.24,.55,.7));c.set_intensity(180.);c.set_attenuation_radius(1500.);c.set_cast_shadows(False)
 # Reuse the project's floating dust-mote Niagara system in the two lit chambers.
 dust=unreal.load_asset('/Game/Variant_Horror/Blueprints/Light/Assets/NS_DustMote')
 for n,p in enumerate([[-28,0,76],[14,0,100]]):
  a=actors.spawn_actor_from_class(unreal.NiagaraActor,origins[1]+raw(p));a.set_actor_label(f'MAIN_GAME_CrystalMotes_{n+1}');a.set_actor_scale3d(unreal.Vector(3,3,3));a.get_component_by_class(unreal.NiagaraComponent).set_asset(dust);tag(a,'BM_Campaign_MainGame02','BM_RoutesV5')
 # The first bomb gate is the existing fitted plug immediately outside the office.
 for spec in cfg['generated_connections']:
  if spec['mine']==2 and spec['side']=='exit':continue
  rock=unreal.load_object(None,spec['rock']);shell=unreal.load_object(None,spec['shell']);assign_rock_material(rock,mats[spec['mine']]);shell.get_component_by_class(unreal.DynamicMeshComponent).set_material(0,mats[spec['mine']])
 first=next(c for c in cfg['generated_connections'] if c['mine']==1 and c['side']=='entry')
 gate=unreal.load_object(None,first['plug']);assert gate;tag(gate,'BM_BombOnly');assign_rock_material(gate,hard);gate.set_actor_label('MAIN_GAME_OfficeExit_BombOnly')
 # Remove the former sideways museum tunnel; close its former doorway.
 for a in list(actors.get_all_level_actors()):
  if a.get_level()==hall.get_level() and ('Mine03_Museum' in a.get_actor_label() or a.get_actor_label().startswith('LINK_BM_Campaign_MainGame03_46250_Collar_')):actors.destroy_actor(a)
 limestone=unreal.load_asset('/Game/WenSi/Museum/Materials/MI_Limestone')
 box('MAIN_GAME_Museum_OldEntranceSealed',unreal.Vector(46670,11750,14610),unreal.Vector(45,440,420),limestone,'BM_Campaign_MainGameMuseum')
 # Suppress only the roof pieces crossed by the opening, then replace their rims.
 museum=areas[-1];suppressed=list(museum.suppressed_source_actors);opening=(46980.,47420.,11530.,11970.);roof_records=[]
 for a in all_actors:
  if '/WenSi/Maps/Lvl_WenSi_MirrorMuseum1.' not in a.get_path_name() or 'Ceiling' not in a.get_actor_label():continue
  center,extent=a.get_actor_bounds(False);x0,x1=center.x-extent.x,center.x+extent.x;y0,y1=center.y-extent.y,center.y+extent.y
  if not (14800<=center.z<=14850 and x0<opening[1] and x1>opening[0] and y0<opening[3] and y1>opening[2]):continue
  suppressed.append(a.get_name());roof_records.append(a.get_name());comp=a.get_component_by_class(unreal.StaticMeshComponent);mat=comp.get_material(0) if comp else limestone
  rectangles=[(x0,min(x1,opening[0]),y0,y1),(max(x0,opening[1]),x1,y0,y1),(max(x0,opening[0]),min(x1,opening[1]),y0,min(y1,opening[2])),(max(x0,opening[0]),min(x1,opening[1]),max(y0,opening[3]),y1)]
  for j,(xa,xb,ya,yb) in enumerate(rectangles):
   if xb>xa+.1 and yb>ya+.1:box('MAIN_GAME_RoofRim_'+a.get_name()+str(j),unreal.Vector((xa+xb)/2,(ya+yb)/2,center.z),unreal.Vector(xb-xa,yb-ya,extent.z*2),mat,'BM_Campaign_MainGameMuseum')
  a.set_actor_enable_collision(False);a.set_is_temporarily_hidden_in_editor(True)
 assert roof_records;setp(museum,'suppressed_source_actors',suppressed);setp(museum,'entry',unreal.Transform(location=unreal.Vector(47200,11750,14510)));areas[-1]=museum
 area=areas[4];setp(area,'exit',unreal.Vector(47200,11750,14765));setp(area,'exit_extent',unreal.Vector(230,230,250));setp(area,'instructions','沿螺旋通路向下探索；在尽头向下挖开土层，落入博物馆。');areas[4]=area
 gate=actors.spawn_actor_from_class(unreal.DiggableRock,unreal.Vector(47200,11750,15000));gate.set_actor_label('MAIN_GAME_Museum_DownwardSoil');setp(gate,'rock_size',unreal.Vector(620,620,140));setp(gate,'hardness_level',2);setp(gate,'rock_material',clay);gate.reset_rock();tag(gate,'BM_Campaign_MainGame03','BM_RoutesV5','BM_MuseumDownwardSoil')
 setp(hall,'areas',areas);tag(hall,'BM_MuseumCeilingDrop','BM_RoutesV5')
 from polish_main_game_routes_v5 import apply as polish_routes
 polish_routes()
 # Shrink both gameplay radii and visual explosion; capacity is captured by progression.
 bp=unreal.load_asset('/Game/MainGame/Blueprints/BP_MainGameCharacter');cdo=unreal.get_default_object(bp.generated_class());bomb=cdo.get_component_by_class(unreal.MiningBombToolComponent)
 settings=bomb.settings;setp(settings,'blast_radius',350.);setp(settings,'excavation_radius',260.);setp(settings,'explosion_scale',.625);setp(settings,'debris_count',36);setp(bomb,'settings',settings);setp(bomb,'max_bombs',1)
 fx=cdo.get_component_by_class(unreal.MiningToolEffectsComponent);setp(fx,'enabled',True);setp(fx,'debris_class',unreal.MiningDebrisBurst.static_class());setp(fx,'debris_chunk_count',36)
 unreal.BlueprintEditorLibrary.compile_blueprint(bp);assert unreal.EditorAssetLibrary.save_loaded_asset(bp,False)
 # Re-site the existing four-mineral recipe on new walls without changing counts.
 log(phase='rehosting_minerals');candidates=list(unreal.MiningDesignerLibrary.find_campaign_mineral_sites(hall));cache=[dict(p=p,tag=str(p.area_tag),progress=p.route_progress,guide=bool(p.guide_layer),position=p.surface.translation,ceiling=p.surface.rotation.rotator().pitch<-30) for p in candidates]
 selected=[];used=set();positions=[]
 for r in retained:
  choices=[(j,c) for j,c in enumerate(cache) if j not in used and c['tag']==r['tag'] and all(c['position'].distance(pos)>135 for pos in positions)]
  assert choices,('No mineral site',r)
  j,c=min(choices,key=lambda jc:abs(jc[1]['progress']-r['progress'])+(.3 if jc[1]['guide']!=r['guide'] else 0)+(.12 if jc[1]['ceiling']!=r['ceiling'] else 0))
  p=c['p'];setp(p,'treasure_id',r['id']);setp(p,'value',r['value']);selected.append(p);used.add(j);positions.append(c['position'])
 stage=api._stage(hall,actors,field);setp(stage,'authored_placements',selected);assert unreal.MiningDesignerLibrary.prepare_campaign_minerals(stage),'Mineral burial failed';valid=api.validate(stage,hall);assert valid['ok'],valid.get('errors');api._commit(stage,field,actors)
 assert levels.save_current_level()
 for source,digest in source_hashes.items():assert hashlib.sha256((ROOT/'Content'/(source[6:]+'.umap')).read_bytes()).hexdigest()==digest
 cfg['generated_connections']=[c for c in cfg['generated_connections'] if not(c['mine']==2 and c['side']=='exit')];cfg['connections']=[c for c in cfg['connections'] if not(c['mine']==2 and c['side']=='exit')]
 cfg['museum_ceiling_drop']=dict(center=[47200,11750,15000],opening=[440,440],transition_z=14765,suppressed_roof=roof_records);cfg['regional_routes_revision']=5
 next(r for r in cfg['rooms'] if r['id']=='Museum')['entry']=[47200,11750,14510]
 cfg['room_activation_x']=cfg['room_activation_x'][:2]
 (ROOT/'Config/MainGameRooms.json').write_text(json.dumps(cfg,ensure_ascii=False,indent=2),encoding='utf-8')
 routes=json.loads((ROOT/'Config/MainGameRoutes.json').read_text());routes.update(revision=5,source_root='Art/MainGame/RoutesV5',styles=['narrow-spires','ascending-jetpack','tight-descending-coils'],surface_style='irregular-asymmetric-lowpoly',guide_slots=[8]*3)
 (ROOT/'Config/MainGameRoutes.json').write_text(json.dumps(routes,indent=2),encoding='utf-8')
 log(passed=True,phase='saved',minerals=valid,roof_suppressed=roof_records,source_maps_unchanged=True)
except Exception:log(error=traceback.format_exc());unreal.log_error(report['error'])
finally:unreal.SystemLibrary.execute_console_command(ed.get_editor_world(),'QUIT_EDITOR')
