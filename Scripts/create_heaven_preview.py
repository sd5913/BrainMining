"""Create an isolated, small heaven preview. Never writes any existing gameplay map."""
import unreal,math,json,pathlib,datetime,shutil,os

def create_heaven():
 root=pathlib.Path(unreal.Paths.project_dir());out=root/'Saved/HeavenPreview';out.mkdir(parents=True,exist_ok=True)
 folder='/Game/HeavenPreview';map_path=folder+'/Maps/Lvl_HeavenPreview';lib=unreal.MaterialEditingLibrary;assets=unreal.AssetToolsHelpers.get_asset_tools();api=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
 if unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages() or unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages():raise RuntimeError('Save existing work first; use a clean dedicated preview editor.')
 resume=os.environ.get('HEAVEN_RESUME_FAILED_BUILD')=='1' and not (out/'creation.json').exists()
 exists=unreal.EditorAssetLibrary.does_asset_exist(map_path)
 if exists and not resume:raise RuntimeError('Preview map exists; retain its authored edits and use the update script instead.')
 if exists:
  backup=out/('FailedBuild_'+datetime.datetime.now().strftime('%Y%m%d_%H%M%S'));backup.mkdir();shutil.copy2(root/'Content/HeavenPreview/Maps/Lvl_HeavenPreview.umap',backup/'Lvl_HeavenPreview.umap')
 for sub in ['Materials','Meshes','VFX','Maps']:unreal.EditorAssetLibrary.make_directory(folder+'/'+sub)
 noise='''struct FCloud {float h(float2 p){return frac(sin(dot(p,float2(127.1,311.7)))*43758.5453);} float n(float2 p){float2 i=floor(p),f=frac(p);f=f*f*(3-2*f);return lerp(lerp(h(i),h(i+float2(1,0)),f.x),lerp(h(i+float2(0,1)),h(i+1),f.x),f.y);} float f(float2 p){return .55*n(p)+.27*n(p*2.03)+.13*n(p*4.11)+.05*n(p*8.2);}}; FCloud c;'''
 made=[]
 def material(name,code,alpha=None,lit=False,color=(1,1,1)):
  m=assets.create_asset('M_'+name,folder+'/Materials',unreal.Material,unreal.MaterialFactoryNew());assert m
  m.set_editor_property('two_sided',True);m.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_DEFAULT_LIT if lit else unreal.MaterialShadingModel.MSM_UNLIT)
  if alpha is not None:m.set_editor_property('blend_mode',unreal.BlendMode.BLEND_TRANSLUCENT)
  def custom(text,output):
   node=lib.create_material_expression(m,unreal.MaterialExpressionCustom);node.set_editor_property('output_type',output)
   inputs=[]
   for key,cls,output_pin in [('P',unreal.MaterialExpressionWorldPosition,'XYZ'),('N',unreal.MaterialExpressionPixelNormalWS,''),('V',unreal.MaterialExpressionCameraVectorWS,''),('UV',unreal.MaterialExpressionTextureCoordinate,''),('T',unreal.MaterialExpressionTime,'')]:
    inp=unreal.CustomInput();inp.set_editor_property('input_name',key);inputs.append(inp)
   node.set_editor_property('inputs',inputs);node.set_editor_property('code',text)
   for key,cls,output_pin in [('P',unreal.MaterialExpressionWorldPosition,'XYZ'),('N',unreal.MaterialExpressionPixelNormalWS,''),('V',unreal.MaterialExpressionCameraVectorWS,''),('UV',unreal.MaterialExpressionTextureCoordinate,''),('T',unreal.MaterialExpressionTime,'')]:lib.connect_material_expressions(lib.create_material_expression(m,cls),output_pin,node,key)
   return node
  rgb=custom(code,unreal.CustomMaterialOutputType.CMOT_FLOAT3);lib.connect_material_property(rgb,'',unreal.MaterialProperty.MP_BASE_COLOR if lit else unreal.MaterialProperty.MP_EMISSIVE_COLOR)
  if alpha is not None:lib.connect_material_property(custom(alpha,unreal.CustomMaterialOutputType.CMOT_FLOAT1),'',unreal.MaterialProperty.MP_OPACITY)
  if lit:
   rough=lib.create_material_expression(m,unreal.MaterialExpressionConstant);rough.set_editor_property('r',.28);lib.connect_material_property(rough,'',unreal.MaterialProperty.MP_ROUGHNESS)
  lib.recompile_material(m);made.append(m);return m
 sky=material('HeavenSky',noise+'''
float3 d=-normalize(V);float e=saturate(d.z*.5+.5);float3 col=lerp(float3(.92,.92,.99),float3(.46,.68,.95),smoothstep(.38,.96,e));
float2 q=d.xy/max(.28,d.z+.5)*3.4+float2(T*.002,-T*.0012);float f=c.f(q);float cloud=smoothstep(.44,.65,f)*smoothstep(-.12,.32,d.z);
col=lerp(col,float3(.99,.985,.96),cloud);float haze=1-smoothstep(-.2,.22,d.z);col=lerp(col,float3(.97,.96,.98),haze);
float sun=pow(saturate(dot(d,normalize(float3(.3,.35,.88)))),80);return (col+float3(.3,.19,.06)*sun)*3.;
''')
 sky.set_editor_property('is_sky',True);lib.recompile_material(sky)
 cloud=material('CloudDeck',noise+'''float f=c.f(P.xy*.005+float2(T*.002,0));return 3.*lerp(float3(.72,.79,.91),float3(.98,.97,.96),smoothstep(.16,.8,f));''',alpha='''float r=length(P.xy);return .99*(1-smoothstep(350,625,r));''')
 puff=material('CloudPuff',noise+'''float f=c.f(P.xy*.004+float2(T*.003,0));float l=saturate(N.z*.5+.5);return 3.*lerp(float3(.68,.76,.88),float3(.99,.98,.96),.45+.38*l+.16*f);''',alpha='''return .82*pow(saturate(dot(normalize(N),normalize(V))),.8);''')
 puff.set_editor_property('two_sided',False);lib.recompile_material(puff)
 porcelain=material('AltarPorcelain','''float g=.985+.015*sin(P.x*.035)*sin(P.y*.024);return float3(.94,.935,.9)*g;''',lit=True)
 gold=material('SacredGold','''return float3(1,.63,.17)*3.5;''')
 # Import only the small generated halo. The user's original FBX / Fab assets remain untouched.
 task=unreal.AssetImportTask();task.filename=str((root/'Art/HeavenPreview/Halo.obj').resolve());task.destination_path=folder+'/Meshes';task.destination_name='SM_Halo';task.automated=True;task.save=True;task.replace_existing=resume
 options=unreal.FbxImportUI();options.import_mesh=True;options.import_materials=False;options.import_textures=False;options.import_animations=False;options.set_editor_property('automated_import_should_detect_type',False);options.set_editor_property('mesh_type_to_import',unreal.FBXImportType.FBXIT_STATIC_MESH);task.options=options
 unreal.SystemLibrary.execute_console_command(None,'Interchange.FeatureFlags.Import.FBX 0');assets.import_asset_tasks([task]);halo=unreal.load_asset(folder+'/Meshes/SM_Halo');assert halo
 registry=unreal.AssetRegistryHelpers.get_asset_registry();toilet_records=registry.get_assets_by_path('/Game/Fab/Toilet',recursive=True);toilet=next((r.get_asset() for r in toilet_records if str(r.asset_class_path.asset_name)=='StaticMesh'),None);assert toilet,'Imported Fab toilet mesh required'
 assert (levels.load_level(map_path) if exists else levels.new_level(map_path))
 world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world();ws=world.get_world_settings();ws.set_editor_property('default_game_mode',unreal.load_class(None,'/Script/Engine.GameModeBase'))
 def mesh(label,m,p,s=(1,1,1),mat=None,rot=None,collision=False,hidden=False):
  a=api.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(*p));a.set_actor_label(label);a.set_folder_path('HeavenPreview/'+('Clouds' if '云' in label else 'Set'));a.set_editor_property('tags',['HeavenPreview']);c=a.static_mesh_component;c.set_static_mesh(m);c.set_mobility(unreal.ComponentMobility.STATIC);c.set_collision_profile_name('BlockAll' if collision else 'NoCollision');c.set_cast_shadow(collision)
  if mat:
   for i in range(c.get_num_materials()):c.set_material(i,mat)
  a.set_actor_scale3d(unreal.Vector(*s))
  if rot:a.set_actor_rotation(rot,True)
  if hidden:a.set_actor_hidden_in_game(True);c.set_visibility(False)
  return a
 cube=unreal.load_asset('/Engine/BasicShapes/Cube');plane=unreal.load_asset('/Engine/BasicShapes/Plane');sphere=unreal.load_asset('/Engine/BasicShapes/Sphere');cylinder=unreal.load_asset('/Engine/BasicShapes/Cylinder')
 mesh('连续天空 · 12米包围空间',cube,(0,0,300),(13,13,11),sky)
 mesh('隐藏地面碰撞',cube,(0,0,-12),(12,12,.2),collision=True,hidden=True)
 for x,y in [(-600,0),(600,0),(0,-600),(0,600)]:mesh('隐藏边界碰撞',cube,(x,y,400),(.2,12,8) if x else (12,.2,8),collision=True,hidden=True)
 mesh('云海地面',plane,(0,0,-1),(13,13,1),cloud)
 for i in range(24):
  angle=math.tau*i/24;r=505+40*math.sin(i*3.7);scale=2.1+.6*math.sin(i*2.3)
  mesh(f'边界白云_{i:02d}',sphere,(math.cos(angle)*r,math.sin(angle)*r,-45-22*math.sin(i)),(scale*1.45,scale*1.15,scale*.55),puff)
 mesh('圣坛 · 下层',cylinder,(0,0,3),(3.8,3.8,.06),porcelain,collision=True)
 mesh('圣坛 · 主台',cylinder,(0,0,13),(2.8,2.8,.2),porcelain,collision=True)
 mesh('圣坛 · 金色薄边',halo,(0,0,23.2),(1.72,1.72,1.72),gold,rot=unreal.Rotator(pitch=90,yaw=0,roll=0))
 bounds=toilet.get_bounds();height=bounds.box_extent.z*2;scale=105/max(height,.1);base=(bounds.origin.z-bounds.box_extent.z)*scale
 subject=api.spawn_actor_from_class(unreal.load_class(None,'/Script/BrainMining.HeavenToilet'),unreal.Vector(0,0,24-base));subject.set_actor_label('神圣马桶 · 靠近开盖喷水');subject.set_actor_rotation(unreal.Rotator(pitch=0,yaw=90,roll=0),True);subject.set_actor_scale3d(unreal.Vector(scale,scale,scale));b=halo.get_bounds();ext=[b.box_extent.x,b.box_extent.y,b.box_extent.z];factor=80/max(ext);axis=min(range(3),key=lambda i:ext[i]);rot=unreal.Rotator(pitch=90 if axis==2 else 0,yaw=90 if axis==1 else 0,roll=0)
 mesh('神圣光环',halo,(85,0,162),(factor,factor,factor),gold,rot=rot)
 # Warm primary light and cool soft fill; fixed exposure preserves the bright cloud detail.
 sun=api.spawn_actor_from_class(unreal.DirectionalLight,unreal.Vector(0,0,600));sun.set_actor_label('神圣暖光');sun.set_actor_rotation(unreal.Rotator(pitch=-60,yaw=-25,roll=0),True);sun_comp=sun.get_components_by_class(unreal.DirectionalLightComponent)[0];sun_comp.set_intensity(1);sun_comp.set_light_color(unreal.LinearColor(1,.89,.7,1))
 skylight=api.spawn_actor_from_class(unreal.SkyLight,unreal.Vector(0,0,350));skylight.set_actor_label('天空柔光');skylight.get_components_by_class(unreal.SkyLightComponent)[0].set_intensity(.8)
 key=api.spawn_actor_from_class(unreal.RectLight,unreal.Vector(-180,-150,250));key.set_actor_label('马桶柔光');key.set_actor_rotation(unreal.Rotator(pitch=-35,yaw=40,roll=0),True);key_comp=key.get_components_by_class(unreal.RectLightComponent)[0];key_comp.set_editor_property('intensity_units',unreal.LightUnits.LUMENS);key_comp.set_intensity(200);key_comp.set_source_width(150);key_comp.set_source_height(150);key_comp.set_light_color(unreal.LinearColor(1,.96,.86,1))
 fog=api.spawn_actor_from_class(unreal.ExponentialHeightFog,unreal.Vector(0,0,20));fog.set_actor_label('低层白雾');fc=fog.get_components_by_class(unreal.ExponentialHeightFogComponent)[0];fc.set_fog_density(.0005);fc.set_fog_height_falloff(.35);fc.set_fog_inscattering_color(unreal.LinearColor(.93,.95,.99,1));fc.set_fog_max_opacity(.18);fc.set_start_distance(450)
 pp=api.spawn_actor_from_class(unreal.PostProcessVolume,unreal.Vector(0,0,0));pp.set_actor_label('天堂曝光');pp.set_editor_property('unbound',True);settings=pp.get_editor_property('settings')
 for key,value in [('override_auto_exposure_method',True),('auto_exposure_method',unreal.AutoExposureMethod.AEM_HISTOGRAM),('override_auto_exposure_min_brightness',True),('auto_exposure_min_brightness',0.),('override_auto_exposure_max_brightness',True),('auto_exposure_max_brightness',0.),('override_auto_exposure_bias',True),('auto_exposure_bias',-1.5),('override_bloom_intensity',True),('bloom_intensity',.28),('override_vignette_intensity',True),('vignette_intensity',.08)]:settings.set_editor_property(key,value)
 pp.set_editor_property('settings',settings)
 start=api.spawn_actor_from_class(unreal.PlayerStart,unreal.Vector(-360,0,145));start.set_actor_rotation(unreal.Rotator(pitch=-6,yaw=0,roll=0),True)
 unreal.SystemLibrary.execute_console_command(None,'r.AntiAliasingMethod 2');unreal.EditorLevelLibrary.set_level_viewport_camera_info(unreal.Vector(-390,-160,160),unreal.Rotator(pitch=-7,yaw=23,roll=0))
 for actor in api.get_all_level_actors():
  for component in actor.get_components_by_class(unreal.LightComponent):component.set_mobility(unreal.ComponentMobility.MOVABLE)
 unreal.EditorAssetLibrary.save_loaded_assets(made+[halo]);assert unreal.EditorLoadingAndSavingUtils.save_map(world,map_path)
 report={'map':map_path,'toilet':toilet.get_path_name(),'toilet_scale':scale,'halo_extent':ext,'water_system':'/Game/HeavenPreview/VFX/NS_HeavenToiletWater','actors':len(api.get_all_level_actors()),'actual_space_cm':[1200,1200,800]};(out/'creation.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8');print('HEAVEN_PREVIEW_CREATED',json.dumps(report,ensure_ascii=False))

create_heaven()
