"""Reauthor the dedicated hall water only; preserve player edits and hidden state."""
import unreal,pathlib,shutil,datetime,json
ROOT='/Game/HeavenPreview'
DEST='/Game/CollectionPrototype/HeavenToilet/Maps/Lvl_HeavenToilet'
lib=unreal.MaterialEditingLibrary

def run():
 assert not unreal.EditorLevelLibrary.get_pie_worlds(False)
 api=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
 assert unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world().get_path_name().split('.')[0]==DEST
 actors=api.get_all_level_actors();water=next(a for a in actors if isinstance(a,unreal.HeavenToiletWater))
 hidden_editor=water.is_temporarily_hidden_in_editor()
 hidden_game=water.get_editor_property('hidden')
 visible=[(c,c.is_visible(),c.get_editor_property('hidden_in_game')) for c in water.get_components_by_class(unreal.MeshComponent)]
 root=pathlib.Path(unreal.Paths.project_dir());backup=root/'Saved/HeavenPreview'/('BeforeWaterRefinement_'+datetime.datetime.now().strftime('%Y%m%d_%H%M%S'));backup.mkdir(parents=True)
 src=root/('Content/'+DEST.removeprefix('/Game/')+'.umap');shutil.copy2(src,backup/src.name)
 assetlib=unreal.EditorAssetLibrary;assets=unreal.AssetToolsHelpers.get_asset_tools()
 tex=unreal.load_asset(ROOT+'/Textures/T_HeavenWaterRipples')
 if not tex:tex=assets.duplicate_asset('T_HeavenWaterRipples',ROOT+'/Textures',unreal.load_asset('/Engine/Functions/Engine_MaterialFunctions02/ExampleContent/Textures/water_n'))
 assert tex;assetlib.save_loaded_asset(tex)
 path=ROOT+'/Materials/M_HeavenClearWater';m=unreal.load_asset(path)
 if not m:m=assets.create_asset('M_HeavenClearWater',ROOT+'/Materials',unreal.Material,unreal.MaterialFactoryNew())
 else:lib.delete_all_material_expressions(m)
 m.set_editor_property('blend_mode',unreal.BlendMode.BLEND_OPAQUE)
 m.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_SINGLE_LAYER_WATER)
 m.set_editor_property('two_sided',True);m.set_editor_property('tangent_space_normal',False)
 m.set_editor_property('used_with_niagara_mesh_particles',True)
 def scalar(value,name=None):
  n=lib.create_material_expression(m,unreal.MaterialExpressionScalarParameter if name else unreal.MaterialExpressionConstant)
  if name:n.set_editor_property('parameter_name',name);n.set_editor_property('default_value',value)
  else:n.set_editor_property('r',value)
  return n
 def vector(value,name=None):
  n=lib.create_material_expression(m,unreal.MaterialExpressionVectorParameter if name else unreal.MaterialExpressionConstant3Vector)
  if name:n.set_editor_property('parameter_name',name)
  n.set_editor_property('default_value' if name else 'constant',unreal.LinearColor(*value,1));return n
 def prop(n,p):assert lib.connect_material_property(n,'',p)
 def custom(code,typ,inputs):
  n=lib.create_material_expression(m,unreal.MaterialExpressionCustom);n.set_editor_property('code',code);n.set_editor_property('output_type',typ)
  items=[]
  for key in inputs:
   i=unreal.CustomInput();i.set_editor_property('input_name',key);items.append(i)
  n.set_editor_property('inputs',items)
  for key,(node,pin) in inputs.items():assert lib.connect_material_expressions(node,pin,n,key)
  return n
 p=lib.create_material_expression(m,unreal.MaterialExpressionWorldPosition);t=lib.create_material_expression(m,unreal.MaterialExpressionTime)
 flow=scalar(0,'FlowStrength');wake=scalar(0,'WakeStrength');wader=vector((0,0,0),'Wader')
 normals=[]
 for index,code in enumerate(['return P.xy/650.0+float2(.012*sin(T*.37),.009*cos(T*.29))+T*Flow*float2(.035,0);','return float2(-P.y,P.x)/245.0+float2(.008*cos(T*.41),.011*sin(T*.31))+T*Flow*float2(0,-.045);']):
  uv=custom(code,unreal.CustomMaterialOutputType.CMOT_FLOAT2,{'P':(p,'XYZ'),'T':(t,''),'Flow':(flow,'')})
  n=lib.create_material_expression(m,unreal.MaterialExpressionTextureSampleParameter2D);n.set_editor_property('parameter_name','WaterNormal');n.set_editor_property('texture',tex);n.set_editor_property('sampler_type',unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
  assert lib.connect_material_expressions(uv,'',n,'UVs');normals.append(n)
 vertex=lib.create_material_expression(m,unreal.MaterialExpressionVertexNormalWS)
 face=lib.create_material_expression(m,unreal.MaterialExpressionTwoSidedSign)
 strength=scalar(.075,'RippleStrength');amplitude=scalar(1,'WaveAmplitude')
 normal=custom('''
float2 d0=float2(.94,.342),d1=float2(-.38,.925),d2=float2(.72,-.694);
float3 k=float3(.013963,.024166,.048332);
float3 phase=float3(dot(P.xy,d0)*k.x-T*sqrt(980*k.x),dot(P.xy,d1)*k.y-T*sqrt(980*k.y)+1.7,dot(P.xy,d2)*k.z-T*sqrt(980*k.z)+3.2);
float3 deriv=float3(.7,.4,.18)*k*cos(phase)*Amplitude;
float2 slope=A.xy*Strength+B.xy*Strength*.42-(d0*deriv.x+d1*deriv.y+d2*deriv.z);
float2 d=P.xy-Wader.xy;float r=length(d);
slope+=Wake*normalize(d+float2(.01,.01))*.035*sin(r*.105-T*5.0)*exp(-r*.018);
float3 n=normalize(VN*Face);
// Face sign compensates the engine's two-sided flip for world-space normals.
return normalize(n+float3(slope,0))*Face;
''',unreal.CustomMaterialOutputType.CMOT_FLOAT3,{'A':(normals[0],'RGB'),'B':(normals[1],'RGB'),'Strength':(strength,''),'Amplitude':(amplitude,''),'VN':(vertex,''),'Face':(face,''),'P':(p,'XYZ'),'T':(t,''),'Wake':(wake,''),'Wader':(wader,'')})
 prop(normal,unreal.MaterialProperty.MP_NORMAL)
 for n,target in [(vector((.002,.006,.008),'SurfaceTint'),unreal.MaterialProperty.MP_BASE_COLOR),(scalar(.13,'Roughness'),unreal.MaterialProperty.MP_ROUGHNESS),(scalar(.25,'Specular'),unreal.MaterialProperty.MP_SPECULAR),(scalar(.012,'SurfaceOpacity'),unreal.MaterialProperty.MP_OPACITY)]:prop(n,target)
 displacement=custom('''
float2 d0=float2(.94,.342),d1=float2(-.38,.925),d2=float2(.72,-.694);
float3 k=float3(.013963,.024166,.048332);
float3 phase=float3(dot(P.xy,d0)*k.x-T*sqrt(980*k.x),dot(P.xy,d1)*k.y-T*sqrt(980*k.y)+1.7,dot(P.xy,d2)*k.z-T*sqrt(980*k.z)+3.2);
float3 a=float3(.7,.4,.18)*Amplitude;
float2 drift=(d0*a.x*cos(phase.x)+d1*a.y*cos(phase.y)+d2*a.z*cos(phase.z))*.18;
return float3(drift,dot(a,sin(phase)));
''',unreal.CustomMaterialOutputType.CMOT_FLOAT3,{'P':(p,'XYZ'),'T':(t,''),'Amplitude':(amplitude,'')})
 prop(displacement,unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
 output=lib.create_material_expression(m,unreal.MaterialExpressionSingleLayerWaterMaterialOutput)
 for key,node in [('ScatteringCoefficients',vector((.000015,.000028,.000035),'Scattering')),('AbsorptionCoefficients',vector((.00045,.00012,.00006),'Absorption')),('PhaseG',scalar(.1)),('ColorScaleBehindWater',vector((1,1,1)))]:assert lib.connect_material_expressions(node,'',output,key)
 lib.recompile_material(m);assert assetlib.save_loaded_asset(m)
 def instance(name,flow_value):
  mi=unreal.load_asset(ROOT+'/Materials/'+name)
  if not mi:mi=assets.create_asset(name,ROOT+'/Materials',unreal.MaterialInstanceConstant,unreal.MaterialInstanceConstantFactoryNew())
  lib.set_material_instance_parent(mi,m)
  for k,v in [('FlowStrength',flow_value),('RippleStrength',.22 if flow_value else .18),('Roughness',.12 if flow_value else .10),('WaveAmplitude',.22 if flow_value else 2.2)]:lib.set_material_instance_scalar_parameter_value(mi,k,v)
  lib.update_material_instance(mi);assetlib.save_loaded_asset(mi);return mi
 basin=instance('MI_HeavenClearBasin',0);spill=instance('MI_HeavenClearOutflow',1)
 core=instance('MI_HeavenGushCore',0)
 for key,v in [('WaveAmplitude',0),('RippleStrength',0),('Roughness',.07),('SurfaceOpacity',.018)]:lib.set_material_instance_scalar_parameter_value(core,key,v)
 lib.update_material_instance(core);assetlib.save_loaded_asset(core)
 water.set_editor_property('water_material',basin);water.set_editor_property('spill_material',spill)
 for c,v,h in visible:c.set_material(0,basin if c.get_name()=='ShallowWaterSurface' else spill);c.set_visibility(v);c.set_hidden_in_game(h)
 water.set_actor_hidden_in_game(hidden_game);water.set_is_temporarily_hidden_in_editor(hidden_editor)
 # The translucent cloud plane was just 2cm under the surface; remove this
 # milky overlay while retaining the perimeter clouds and the user's sky setup.
 cloud=next((a for a in actors if a.get_actor_label()=='天堂云海 · 水面下方'),None)
 if cloud:
  cloud.set_actor_hidden_in_game(True);cloud.static_mesh_component.set_visibility(False)
 # The old small point fill lights produced repeated saturated white patches.
 # Keep their diffuse illumination and reduce only their specular contribution.
 for a in actors:
  if isinstance(a,unreal.PointLight) and a.get_actor_label().startswith(('Heaven central','Heaven dome wash','Heaven zenith')):
   a.get_component_by_class(unreal.PointLightComponent).set_editor_property('specular_scale',.35)
  elif isinstance(a,unreal.PostProcessVolume) and a.get_actor_label()=='Heaven fixed exposure':
   settings=a.get_editor_property('settings');settings.set_editor_property('override_reflection_method',True);settings.set_editor_property('reflection_method',unreal.ReflectionMethod.SCREEN_SPACE);settings.set_editor_property('override_screen_space_reflection_quality',True);settings.set_editor_property('screen_space_reflection_quality',75.);a.set_editor_property('settings',settings)
  elif isinstance(a,unreal.SkyLight) and a.get_actor_label()=='天堂天空反射柔光':
   a.get_component_by_class(unreal.SkyLightComponent).set_editor_property('real_time_capture',False)
 assert unreal.EditorLoadingAndSavingUtils.save_current_level()
 report={'water_editor_hidden':hidden_editor,'water_game_hidden':hidden_game,'material':basin.get_path_name(),'backup':str(backup),'cloud_underlay_hidden':bool(cloud)}
 (root/'Saved/HeavenPreview/water_refinement.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8');print('WATER_REFINED',json.dumps(report,ensure_ascii=False))
run()

