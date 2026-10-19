"""Reconcile collectible feedback in the saved campaign, without regenerating terrain."""
import unreal,json,math,traceback,shutil
from pathlib import Path
from datetime import datetime
ROOT=Path(unreal.Paths.project_dir());OUT=ROOT/'Saved/Validation/CollectionFeedback';OUT.mkdir(parents=True,exist_ok=True)
ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem);levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);api=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
ML=unreal.MaterialEditingLibrary;EA=unreal.EditorAssetLibrary;DL=unreal.MiningDesignerLibrary
report={'passed':False,'placements':[],'branches':[]};sources={}
def V(v):return unreal.Vector(*v)
def xyz(v):return list(v.to_tuple())
def setp(a,k,v):a.set_editor_property(k,v,notify_mode=unreal.PropertyAccessChangeNotifyMode.NEVER)
def save_report(): (OUT/'authoring.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
def trace(a,b,ignore=None):
 h=unreal.SystemLibrary.line_trace_single(ed.get_editor_world(),a,b,unreal.TraceTypeQuery.ECC_VISIBILITY,True,ignore or [],unreal.DrawDebugTrace.NONE,True)
 return h.to_tuple() if h and h.to_tuple()[0] else None
def material(name,code,parameters,alpha=False):
 path='/Game/CollectionPrototype/Materials/'+name
 m=unreal.load_asset(path) if EA.does_asset_exist(path) else unreal.AssetToolsHelpers.get_asset_tools().create_asset(name,'/Game/CollectionPrototype/Materials',unreal.Material,unreal.MaterialFactoryNew())
 ML.delete_all_material_expressions(m);m.set_editor_property('two_sided',True)
 if name=='M_CollectionStainOverlay':m.set_editor_property('material_domain',unreal.MaterialDomain.MD_DEFERRED_DECAL)
 if alpha:m.set_editor_property('blend_mode',unreal.BlendMode.BLEND_TRANSLUCENT)
 else:m.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT)
 c=ML.create_material_expression(m,unreal.MaterialExpressionCustom);c.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT4 if alpha else unreal.CustomMaterialOutputType.CMOT_FLOAT3)
 names=['P']+list(parameters);ins=[]
 for name in names:
  i=unreal.CustomInput();i.set_editor_property('input_name',name);ins.append(i)
 c.set_editor_property('inputs',ins);c.set_editor_property('code',code)
 pos=ML.create_material_expression(m,unreal.MaterialExpressionWorldPosition);ML.connect_material_expressions(pos,'XYZ',c,'P')
 for name,val in parameters.items():
  n=ML.create_material_expression(m,unreal.MaterialExpressionVectorParameter);n.set_editor_property('parameter_name',name);n.set_editor_property('default_value',unreal.LinearColor(*val,1));ML.connect_material_expressions(n,'RGB',c,name)
 if alpha:
  mask=ML.create_material_expression(m,unreal.MaterialExpressionComponentMask);mask.set_editor_property('r',True);mask.set_editor_property('g',True);mask.set_editor_property('b',True);assert ML.connect_material_expressions(c,'',mask,'');ML.connect_material_property(mask,'',unreal.MaterialProperty.MP_BASE_COLOR)
  opacity=ML.create_material_expression(m,unreal.MaterialExpressionComponentMask);opacity.set_editor_property('r',False);opacity.set_editor_property('g',False);opacity.set_editor_property('b',False);opacity.set_editor_property('a',True);assert ML.connect_material_expressions(c,'',opacity,'');ML.connect_material_property(opacity,'',unreal.MaterialProperty.MP_OPACITY)
 else:ML.connect_material_property(c,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
 ML.recompile_material(m);assert EA.save_loaded_asset(m);return m
def owned(a,label,area):
 a.set_actor_label(label);setp(a,'tags',['BM_MainGame_Collections','BM_CollectionFeedback',area]);a.set_folder_path('MainGame/Collections');return a
def cube(label,p,size,mat,area,rotation=None):
 a=owned(api.spawn_actor_from_class(unreal.StaticMeshActor,p,rotation or unreal.Rotator()),label,area);a.static_mesh_component.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Cube'));a.static_mesh_component.set_material(0,mat);a.set_actor_scale3d(size/100);return a
def soil(label,p,size,mat,area,rotation=None,hardness=0):
 a=owned(api.spawn_actor_from_class(unreal.DiggableRock,p,rotation or unreal.Rotator()),label,area);setp(a,'rock_size',size);setp(a,'rock_material',mat);setp(a,'hardness_level',hardness);a.reset_rock();return a
def cut(actor,p,size,cylinder=False):
 if actor not in sources:sources[actor]=DL.get_campaign_rock_source(actor)
 tool=unreal.DynamicMesh();t=unreal.Transform(location=p)
 if cylinder:unreal.GeometryScript_Primitives.append_cylinder(tool,unreal.GeometryScriptPrimitiveOptions(),t,radius=size.x/2,height=size.z,radial_steps=24,origin=unreal.GeometryScriptPrimitiveOriginMode.CENTER)
 else:unreal.GeometryScript_Primitives.append_box(tool,unreal.GeometryScriptPrimitiveOptions(),t,dimension_x=size.x,dimension_y=size.y,dimension_z=size.z,origin=unreal.GeometryScriptPrimitiveOriginMode.CENTER)
 unreal.GeometryScript_MeshBooleans.apply_mesh_boolean(sources[actor],actor.get_actor_transform(),tool,unreal.Transform(),unreal.GeometryScriptBooleanOperation.SUBTRACT,unreal.GeometryScriptMeshBooleanOptions(fill_holes=True,simplify_output=False))
try:
 assert levels.load_level('/Game/MainGame/Maps/Lvl_MainGame')
 aa=list(api.get_all_level_actors());hall=next(a for a in aa if isinstance(a,unreal.MiningExhibitHall));field=next(a for a in aa if isinstance(a,unreal.MiningCollectionField) and a.authored_placements)
 assert not any(a.actor_has_tag('BM_CollectionFeedback') for a in aa),'Already applied: inspect saved feedback report instead of carving twice.'
 backup=ROOT/'Saved/Backups'/('FeedbackMap_'+datetime.now().strftime('%Y%m%d-%H%M%S')+'.umap');backup.parent.mkdir(exist_ok=True,parents=True);shutil.copy2(ROOT/'Content/MainGame/Maps/Lvl_MainGame.umap',backup)
 report['backup']=str(backup)
 material('M_CollectionStainOverlay','''
float3 q=P-Origin;float depth=dot(q,Forward);
float2 trail=float2(dot(q,Right),q.z);
float corridor=(1-smoothstep(100,145,length(trail*float2(1,.95))))*step(-20,depth)*(1-smoothstep(850,950,depth));
float3 color=0;float alpha=0;float layer=floor(depth/210);
[unroll] for(int i=0;i<16;++i){
 float seed=i+layer*23;float3 r=frac(sin(float3(seed*17.13+4.1,seed*37.21+9.4,seed*93.71+2.7))*43758.5453);
 float2 d=trail-(r.xy-.5)*float2(205,175);float radius=4+r.z*r.z*25;float angle=atan2(d.y,d.x);
 float border=radius*(1+.19*sin(angle*5+r.x*17)+.09*sin(angle*11+r.y*21));
 float patch=1-smoothstep(border-.7,border+.7,length(d*float2(1,.85+r.x*.5)));
 float runLength=radius+15+r.y*40;
 float drip=(1-smoothstep(1,2.6,abs(d.x+sin(d.y*.07)*1.2)))*step(d.y,0)*smoothstep(-runLength,-runLength+8,d.y)*step(.55,r.z);
 float a=max(patch,drip)*corridor*(.62+r.z*.25);
 float3 pigment=r.x>.54?float3(.045,.24,.14):float3(.25,.034,.17);
 color=lerp(color,pigment,a);alpha=max(alpha,a);
}return float4(color/max(alpha,.001),alpha);
''',{'Origin':(0,0,0),'Forward':(1,0,0),'Right':(0,1,0)},True)
 # A smoothly transparent ant shell; one instance per ant is shared by all body parts.
 ant=material('M_CollectionAntFade','return float4(.028,.014,.008,Fade.x);',{'Fade':(1,1,1)},True)
 # Continuous spherical world direction avoids face-by-face sky coordinates.
 dome=material('M_CollectionHeavenSky','''
float3 d=normalize(P-Origin);float h=saturate(d.z);
float3 sky=lerp(float3(.55,.79,1.0),float3(.12,.37,.76),pow(h,.55));
float cloud=sin(d.x*13+d.y*5)*sin(d.y*17-d.z*8)+.35*sin(d.x*31+d.z*19);
cloud=smoothstep(.42,.9,cloud)*(1-smoothstep(.6,.95,h));
return lerp(sky,float3(.98,.97,.91),cloud*.82)*7.;
''',{'Origin':(36007.500305,15215.952185,18110.792084)})
 placements=list(field.authored_placements)
 # Three rewards at existing branch ends, reached through the existing soil guides.
 branches={1:('TunnelV1',2),4:('TunnelV1',4),10:('TunnelStalagmiteV3',4)}
 depths={1:480,4:480,8:460,10:430}
 for p in placements:
  front=p.clue_transform.translation;normal=unreal.MathLibrary.get_forward_vector(p.clue_transform.rotation.rotator())*-1
  approach=front+normal*220+unreal.Vector(0,0,30)
  if p.item_id in branches:
   stage,bi=branches[p.item_id];region=next(a for a in aa if isinstance(a,unreal.MiningExternalRegion) and a.actor_has_tag(p.area_tag))
   paths=json.loads((ROOT/'Art/MainGame/RoutesV6'/stage/'centerlines.json').read_text());path=paths[bi]
   origin=region.get_actor_location()+region.cutters[0].local_transform.translation
   raw=lambda q:origin+unreal.Vector(q[0]*100,-q[1]*100,q[2]*100)
   candidates=[]
   for k in range(len(path['points'])-3,max(1,len(path['points'])-20),-2):
    base=raw(path['points'][k]);floor=trace(base,base-unreal.Vector(0,0,1500))
    if not floor or floor[1]:continue
    start=unreal.Vector(base.x,base.y,floor[5].z+160)
    heading=(raw(path['points'][-1])-raw(path['points'][-8])).normal()
    for direction in [heading,unreal.Vector(0,1,0),unreal.Vector(0,-1,0)]:
     h=trace(start,start+direction*1300)
     if h and not h[1] and isinstance(h[9],unreal.DiggableRock) and h[9]==region.generated_rock and abs(h[7].z)<.65 and h[5].distance(start)>70:candidates.append((h,start,floor[5]));break
    if candidates:break
   assert candidates,'No safe terminal branch site for '+str(p.item_id)
   hit,start,ground=candidates[0];front=hit[5];normal=hit[7];p.host_rock=hit[9];p.clue_transform=unreal.Transform(location=front,rotation=unreal.MathLibrary.make_rot_from_x(normal*-1))
   approach=unreal.Vector(start.x,start.y,ground.z+100)
   report['branches'].append(dict(item_id=p.item_id,path=path['name'],region=stage,approach=xyz(approach)))
  if p.item_id in depths:
   t=p.transform;t.translation=front-normal*depths[p.item_id]
   if p.item_id==1:t.translation-=unreal.Vector(0,0,80)
   if p.item_id==8:t.translation=front-unreal.Vector(0,0,depths[8])
   p.transform=t
  if p.item_id in [8,9]:approach=front+unreal.Vector(-180,40,110)
  elif p.item_id in [6,7]:approach=p.transform.translation+unreal.Vector(0,210,90)
  elif p.item_id==5:approach=p.transform.translation+unreal.Vector(60,150,80)
  elif p.item_id==0:approach=front+unreal.Vector(0,-200,0)
  p.approach_transform=unreal.Transform(location=approach,rotation=unreal.MathLibrary.find_look_at_rotation(approach,front))
  if p.item_id==8:
   pos=p.transform.translation;cut(p.host_rock,pos+unreal.Vector(0,0,4),unreal.Vector(64,64,106))
   cut(p.host_rock,unreal.Vector(pos.x-6,pos.y,(pos.z+front.z)/2+45),unreal.Vector(28,28,front.z-pos.z+90),True)
  report['placements'].append(dict(item_id=p.item_id,area_tag=str(p.area_tag),position=xyz(p.transform.translation),clue=xyz(front),normal=xyz(normal),approach=xyz(approach),host=p.host_rock.get_name() if p.host_rock else None))
 setp(field,'authored_placements',placements)
 for rock,source in sources.items():assert DL.set_campaign_rock_source(rock,source)
 red=unreal.load_asset('/Game/MainGame/Materials/RoutesV5/M_DarkRed');purple=unreal.load_asset('/Game/MainGame/Materials/RoutesV5/M_BlueViolet');clay=unreal.load_asset('/Game/MainGame/Materials/DirtTransition/MI_ClayBlockage')
 water=next(a for a in aa if isinstance(a,unreal.HeavenToiletWater));frame=water.spill_coordinate_frame
 world=lambda p:unreal.MathLibrary.transform_location(frame,V(p))
 for a in aa:
  if a.get_actor_label().startswith('COL_Heaven_Branch') and isinstance(a,unreal.StaticMeshActor):a.static_mesh_component.set_material(0,red)
  if 'Heaven hemisphere' in a.get_actor_label() and isinstance(a,unreal.StaticMeshActor):a.static_mesh_component.set_material(0,dome)
 plug=soil('COL_Heaven_FaucetSoil',world((90,0,225)),unreal.Vector(730,660,450),red,'BM_Campaign_MainGame03',frame.rotation.rotator(),2)
 setp(water,'retaining_soil',list(water.retaining_soil)+[plug])
 setp(water,'outer_x',-300.)
 for rock in water.retaining_soil:
  setp(rock,'rock_material',red)
  for mesh in rock.get_components_by_class(unreal.DynamicMeshComponent):mesh.set_material(0,red)
 # The upper approach is enclosed; the original trap floor and drop trigger remain intact.
 echo=next(a for a in aa if isinstance(a,unreal.EchoPassageStage));ef=echo.get_actor_transform();ep=lambda p:unreal.MathLibrary.transform_location(ef,V(p));area='BM_Campaign_MainGameJourney'
 cube('COL_Echo_EntryCeiling',ep((-270,0,1225)),unreal.Vector(1080,540,50),purple,area)
 cube('COL_Echo_EntryFrontRock',ep((390,0,1045)),unreal.Vector(180,540,360),purple,area)
 for side in [-1,1]:cube('COL_Echo_EntryFrontRim',ep((245,side*240,1045)),unreal.Vector(150,60,360),purple,area)
 soil('COL_Echo_EntryTemptationSoil',ep((245,0,1050)),unreal.Vector(110,420,300),clay,area,hardness=1)
 # Parameters remain scoped to the existing cell/vessel post process material.
 oil=unreal.load_asset('/Game/MouseMuseum/MI_EndingOilPaint')
 for key,value in {'SampleRadius':6.,'StrokeSpreadNear':8.,'StrokeSpreadFar':5.,'DetailProtectionNear':.35,'DetailProtectionFar':.3,'CanvasDistortionIntensity':3.}.items():ML.set_material_instance_scalar_parameter_value(oil,key,value)
 ML.update_material_instance(oil);assert EA.save_loaded_asset(oil)
 for a in aa:
  if isinstance(a,unreal.EndingEmergence):setp(a,'cell_paint_weight',1.)
 assert unreal.EditorLoadingAndSavingUtils.save_map(ed.get_editor_world(),'/Game/MainGame/Maps/Lvl_MainGame')
 report['passed']=True
except Exception:report['error']=traceback.format_exc();unreal.log_error(report['error'])
finally:save_report();unreal.SystemLibrary.quit_editor()
