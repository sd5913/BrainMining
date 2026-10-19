"""Private scene-polish materials. Call from the guarded authoring pass."""
import unreal
EA=unreal.EditorAssetLibrary;ML=unreal.MaterialEditingLibrary
FOLDER='/Game/MainGame/Materials/ScenePolish'
def mat(path):
 m=unreal.load_asset(path) if EA.does_asset_exist(path) else unreal.AssetToolsHelpers.get_asset_tools().create_asset(path.rsplit('/',1)[1],path.rsplit('/',1)[0],unreal.Material,unreal.MaterialFactoryNew())
 ML.delete_all_material_expressions(m);return m
def node(m,cls,**kw):
 n=ML.create_material_expression(m,cls)
 for k,v in kw.items():n.set_editor_property(k,v)
 return n
def wire(a,b,key,output=''):assert ML.connect_material_expressions(a,output,b,key)
def prop(n,p,output=''):assert ML.connect_material_property(n,output,p)
def custom(m,code,inputs,kind=unreal.CustomMaterialOutputType.CMOT_FLOAT3):
 n=node(m,unreal.MaterialExpressionCustom,code=code,output_type=kind)
 pins=[]
 for k in inputs:
  pin=unreal.CustomInput();pin.set_editor_property('input_name',k);pins.append(pin)
 n.set_editor_property('inputs',pins)
 for k,(v,o) in inputs.items():wire(v,n,k,o)
 return n
def finish(m):ML.recompile_material(m);assert EA.save_loaded_asset(m,False);return m
def create_materials(backup):
 path='/Game/Rooms/CardDuel/Materials/M_PlayedCardFace';backup(path)
 m=mat(path);m.set_editor_property('two_sided',True);m.set_editor_property('blend_mode',unreal.BlendMode.BLEND_MASKED)
 uv=node(m,unreal.MaterialExpressionTextureCoordinate)
 rotated=custom(m,'return float2(UV.y,1-UV.x);',{'UV':(uv,'')},unreal.CustomMaterialOutputType.CMOT_FLOAT2)
 tex=node(m,unreal.MaterialExpressionTextureSampleParameter2D,parameter_name='CardArt',texture=unreal.load_asset('/Game/Rooms/CardDuel/UI/T_Card_Laser'))
 wire(rotated,tex,'UVs');prop(tex,unreal.MaterialProperty.MP_BASE_COLOR,'RGB');prop(tex,unreal.MaterialProperty.MP_OPACITY_MASK,'A')
 glow=node(m,unreal.MaterialExpressionMultiply,const_b=.2);wire(tex,glow,'A','RGB');prop(glow,unreal.MaterialProperty.MP_EMISSIVE_COLOR)
 prop(node(m,unreal.MaterialExpressionConstant,r=.8),unreal.MaterialProperty.MP_ROUGHNESS);finish(m)
 path=FOLDER+'/M_CollectibleDetail';backup(path);m=mat(path)
 tint=node(m,unreal.MaterialExpressionVectorParameter,parameter_name='Tint',default_value=unreal.LinearColor(.7,.7,.7,1));prop(tint,unreal.MaterialProperty.MP_BASE_COLOR,'RGB')
 for name,value,p in [('Roughness',.48,unreal.MaterialProperty.MP_ROUGHNESS),('Metallic',0.,unreal.MaterialProperty.MP_METALLIC)]:
  prop(node(m,unreal.MaterialExpressionScalarParameter,parameter_name=name,default_value=value),p)
 gain=node(m,unreal.MaterialExpressionScalarParameter,parameter_name='Glow',default_value=0.)
 glow=node(m,unreal.MaterialExpressionMultiply);wire(tint,glow,'A','RGB');wire(gain,glow,'B');prop(glow,unreal.MaterialProperty.MP_EMISSIVE_COLOR);finish(m)
 path=FOLDER+'/M_MuseumCeiling';backup(path);m=mat(path)
 p=node(m,unreal.MaterialExpressionWorldPosition)
 color=custom(m,'float2 tile=abs(frac(P.xy/120)-.5)*120; float seam=smoothstep(58.8,59.6,max(tile.x,tile.y)); float grain=frac(sin(dot(floor(P.xy*3),float2(12.9898,78.233)))*43758.5453); return lerp(float3(.68,.665,.63)*( .96+.04*grain),float3(.19,.20,.20),seam*.48);',{'P':(p,'XYZ')})
 prop(color,unreal.MaterialProperty.MP_BASE_COLOR);prop(node(m,unreal.MaterialExpressionConstant,r=.91),unreal.MaterialProperty.MP_ROUGHNESS);finish(m)
 # Preserve the original layer/soil color logic, add fine and medium rock relief.
 parents={}
 result={}
 for src in ['/Game/MainGame/Materials/DirtTransition/MI_DirtWall_StartRoom','/Game/MainGame/Materials/DirtTransition/MI_DirtWall_01','/Game/Campaign/Materials/M_CampaignLimestone','/Game/MainGame/Materials/RoutesV5/M_Earth']:
  original=unreal.load_asset(src);assert original,src
  parent=original
  while isinstance(parent,unreal.MaterialInstanceConstant):parent=parent.get_editor_property('parent')
  key=parent.get_path_name()
  if key not in parents:
   dst=FOLDER+'/'+parent.get_name()+'_Relief'
   m=unreal.load_asset(dst) if EA.does_asset_exist(dst) else EA.duplicate_asset(parent.get_path_name(),dst)
   backup(dst)
   if not any(n.get_editor_property('desc')=='ScenePolishRelief' for n in ML.get_material_expressions(m)):
    p=node(m,unreal.MaterialExpressionWorldPosition);normal=node(m,unreal.MaterialExpressionVertexNormalWS)
    n=custom(m,'float3 g=.25*cos(P*.17+sin(P.yzx*.09))+.09*cos(P*1.7); g-=N*dot(g,N); return normalize(N-g);',{'P':(p,'XYZ'),'N':(normal,'')});n.set_editor_property('desc','ScenePolishRelief')
    m.set_editor_property('tangent_space_normal',False);prop(n,unreal.MaterialProperty.MP_NORMAL)
    prop(node(m,unreal.MaterialExpressionConstant,r=.91),unreal.MaterialProperty.MP_ROUGHNESS)
    prop(node(m,unreal.MaterialExpressionConstant,r=.12),unreal.MaterialProperty.MP_SPECULAR)
    base=ML.get_material_property_input_node(m,unreal.MaterialProperty.MP_BASE_COLOR);channel=ML.get_material_property_input_node_output_name(m,unreal.MaterialProperty.MP_BASE_COLOR)
    if base:
     grain=custom(m,'float n=sin(P.x*.13+sin(P.y*.21))*sin(P.z*.19+P.y*.07); float grit=frac(sin(dot(floor(P*1.5),float3(12.9,78.2,37.7)))*43758.5);return .84+.13*n+.06*grit;',{'P':(p,'XYZ')},unreal.CustomMaterialOutputType.CMOT_FLOAT1)
     mul=node(m,unreal.MaterialExpressionMultiply);wire(base,mul,'A',channel);wire(grain,mul,'B');prop(mul,unreal.MaterialProperty.MP_BASE_COLOR)
    finish(m)
   parents[key]=m
  m=parents[key]
  if isinstance(original,unreal.MaterialInstanceConstant):
   dst=FOLDER+'/'+original.get_name()+'_Relief';copy=unreal.load_asset(dst) if EA.does_asset_exist(dst) else EA.duplicate_asset(src,dst)
   ML.set_material_instance_parent(copy,m);ML.update_material_instance(copy);EA.save_loaded_asset(copy,False);result[src]=copy
  else:result[src]=m
 # Pigment remains part of the existing bounded decal and excavated rock surface.
 for path in ['/Game/CollectionPrototype/Materials/M_CollectionStainOverlay','/Game/CollectionPrototype/Materials/M_CollectionStainedEarth','/Game/Campaign/Materials/M_CampaignStainedEarth']:
  backup(path);m=unreal.load_asset(path)
  if not m:continue
  for n in ML.get_material_expressions(m):
   if isinstance(n,unreal.MaterialExpressionCustom):
    code=n.get_editor_property('code')
    code=code.replace('float3(.045,.24,.14)','float3(.008,.30,.045)').replace('float3(.25,.034,.17)','float3(.32,.006,.17)').replace('.62+r.z*.25','.92+r.z*.08')
    n.set_editor_property('code',code)
  finish(m)
 return result
