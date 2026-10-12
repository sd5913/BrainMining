"""New C4 mesh and materials; retain all physics, blast settings and artist mounts."""
import unreal,json
from pathlib import Path
root=Path(unreal.Paths.project_dir());source=root/'SourceAssets/Bomb/StickyCountdown'
tools=unreal.AssetToolsHelpers.get_asset_tools();lib=unreal.MaterialEditingLibrary;assets=unreal.EditorAssetLibrary
folder='/Game/Bomb/StickyCountdown'
def imp(filename,name,path,options=None):
 t=unreal.AssetImportTask();t.filename=str(filename);t.destination_path=path;t.destination_name=name;t.automated=True;t.replace_existing=True;t.save=True
 if options:t.options=options;t.factory=unreal.FbxFactory()
 tools.import_asset_tasks([t]);a=unreal.load_asset(path+'/'+name);assert a,name;return a
textures={}
for channel in ['BaseColor','Roughness','Metallic']:
 t=imp(source/('T_StickyBomb_'+channel+'.png'),'T_StickyBomb_'+channel,folder)
 t.set_editor_property('srgb',channel=='BaseColor');t.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_DEFAULT if channel=='BaseColor' else unreal.TextureCompressionSettings.TC_MASKS)
 assert assets.save_loaded_asset(t,False);textures[channel]=t
def material(name):
 m=unreal.load_asset(folder+'/'+name) or tools.create_asset(name,folder,unreal.Material,unreal.MaterialFactoryNew());lib.delete_all_material_expressions(m);return m
def node(m,cls,**values):
 n=lib.create_material_expression(m,cls)
 for k,v in values.items():n.set_editor_property(k,v)
 return n
def output(m,n,prop,out=''):assert lib.connect_material_property(n,out,prop)
def wire(a,out,b,socket):assert lib.connect_material_expressions(a,out,b,socket)
reuse_materials=all(assets.does_asset_exist(folder+'/'+n) for n in ['M_StickyBombBody','M_BombFuseLamp','M_BombCountdownDisplay'])
if not reuse_materials:
 body=material('M_StickyBombBody')
 for channel,prop in [('BaseColor',unreal.MaterialProperty.MP_BASE_COLOR),('Roughness',unreal.MaterialProperty.MP_ROUGHNESS),('Metallic',unreal.MaterialProperty.MP_METALLIC)]:
  t=node(body,unreal.MaterialExpressionTextureSampleParameter2D,parameter_name=channel,texture=textures[channel]);t.set_editor_property('sampler_type',unreal.MaterialSamplerType.SAMPLERTYPE_COLOR if channel=='BaseColor' else unreal.MaterialSamplerType.SAMPLERTYPE_MASKS);output(body,t,prop,'RGB' if channel=='BaseColor' else 'R')
 lamp=material('M_BombFuseLamp');lamp.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
 color=node(lamp,unreal.MaterialExpressionVectorParameter,parameter_name='LampColor',default_value=unreal.LinearColor(1,.004,.001,1))
 base=node(lamp,unreal.MaterialExpressionConstant3Vector,constant=unreal.LinearColor(.32,.008,.003,1));output(lamp,base,unreal.MaterialProperty.MP_BASE_COLOR)
 strength=node(lamp,unreal.MaterialExpressionScalarParameter,parameter_name='BlinkStrength',default_value=.04)
 mult=node(lamp,unreal.MaterialExpressionMultiply);wire(color,'RGB',mult,'A');wire(strength,'',mult,'B');output(lamp,mult,unreal.MaterialProperty.MP_EMISSIVE_COLOR)
 rough=node(lamp,unreal.MaterialExpressionConstant,r=.3);output(lamp,rough,unreal.MaterialProperty.MP_ROUGHNESS)
 display=material('M_BombCountdownDisplay');display.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT)
 uv=node(display,unreal.MaterialExpressionTextureCoordinate,coordinate_index=0)
 remaining=node(display,unreal.MaterialExpressionScalarParameter,parameter_name='RemainingSeconds',default_value=3)
 code=r'''
 float2 q=float2(frac(UV.x*5.0),UV.y);
 int ch=clamp((int)floor(UV.x*5.0),0,4);
 int secs=clamp((int)ceil(max(RemainingSeconds-0.00001,0)),0,99);
 int digit=ch==3?secs/10:(ch==4?secs%10:0);
 int masks[10]={63,6,91,79,102,109,125,7,127,111};
 int bits=masks[digit];float mask=0;float aa=max(fwidth(q.x),fwidth(q.y))*0.8;
 if(ch==2){
  float d=min(length((q-float2(.5,.36))*float2(.8,1)),length((q-float2(.5,.64))*float2(.8,1)))-.045;
  mask=1-smoothstep(-aa,aa,d);
 }else{
  float2 centers[7]={float2(.5,.16),float2(.80,.33),float2(.80,.67),float2(.5,.84),float2(.20,.67),float2(.20,.33),float2(.5,.5)};
  for(int i=0;i<7;i++){
   if((bits&(1<<i))!=0){
    float2 halfsize=(i==0||i==3||i==6)?float2(.235,.030):float2(.039,.124);
    float2 d=abs(q-centers[i])-halfsize;
    float sd=length(max(d,0))+min(max(d.x,d.y),0);
    mask=max(mask,1-smoothstep(-aa,aa,sd));
   }
  }
 }
 return float3(.002,.006,.007)+float3(2.2,.63,.085)*mask;
 '''
 custom=node(display,unreal.MaterialExpressionCustom,code=code,output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT3)
 inputs=[]
 for name in ['UV','RemainingSeconds']:
  i=unreal.CustomInput();i.set_editor_property('input_name',name);inputs.append(i)
 custom.set_editor_property('inputs',inputs);wire(uv,'',custom,'UV');wire(remaining,'',custom,'RemainingSeconds');output(display,custom,unreal.MaterialProperty.MP_EMISSIVE_COLOR)
 for m in [body,lamp,display]:lib.recompile_material(m);assert assets.save_loaded_asset(m,False)
else:
 body=unreal.load_asset(folder+'/M_StickyBombBody');lamp=unreal.load_asset(folder+'/M_BombFuseLamp');display=unreal.load_asset(folder+'/M_BombCountdownDisplay')
unreal.SystemLibrary.execute_console_command(None,'Interchange.FeatureFlags.Import.FBX 0')
opt=unreal.FbxImportUI();opt.automated_import_should_detect_type=False;opt.mesh_type_to_import=unreal.FBXImportType.FBXIT_STATIC_MESH;opt.original_import_type=unreal.FBXImportType.FBXIT_STATIC_MESH
opt.import_mesh=True;opt.import_materials=False;opt.import_textures=False;opt.import_as_skeletal=False;opt.import_animations=False
opt.static_mesh_import_data.combine_meshes=True;opt.static_mesh_import_data.auto_generate_collision=False;opt.static_mesh_import_data.normal_import_method=unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS_AND_TANGENTS
mesh=imp(source/'StickyBomb.fbx','SM_StickyBomb','/Game/Bomb',opt)
mapping={'BombBody':body,'BombFuseLamp':lamp,'BombCountdownDisplay':display}
slots=list(mesh.static_materials)
for i,s in enumerate(slots):
 name=str(s.material_slot_name);assert name in mapping,(name,list(mapping));s.material_interface=mapping[name];slots[i]=s
mesh.set_editor_property('static_materials',slots);assert assets.save_loaded_asset(mesh,False)
players=[]
for path in ['/Game/Showcase/Blueprints/BP_SharedMiningCharacter','/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter','/Game/ExternalMining/Blueprints/BP_ExternalMiningTestCharacter']:
 bp=unreal.load_asset(path)
 if not bp:continue
 cdo=unreal.get_default_object(bp.generated_class());model=next((c for c in cdo.get_components_by_class(unreal.StaticMeshComponent) if c.get_name()=='BombModel'),None)
 if not model:continue
 old=model.static_mesh;position=model.get_editor_property('relative_location')
 legacy=old or unreal.load_asset('/Game/Bomb/SM_Bomb')
 delta=legacy.get_bounds().origin-mesh.get_bounds().origin if legacy and legacy!=mesh else unreal.Vector()
 scale=model.get_editor_property('relative_scale3d');delta=unreal.Vector(delta.x*scale.x,delta.y*scale.y,delta.z*scale.z)
 delta=unreal.MathLibrary.transform_direction(unreal.Transform(rotation=model.get_editor_property('relative_rotation')),delta)
 model.set_static_mesh(mesh);model.set_editor_property('override_materials',[]);model.set_relative_location(position+delta,False,False)
 unreal.BlueprintEditorLibrary.compile_blueprint(bp);assert assets.save_loaded_asset(bp,False)
 players.append({'blueprint':path,'old_mesh':old.get_path_name() if old else None,'pivot_compensation':str(delta)})
report={'mesh':mesh.get_path_name(),'triangles':mesh.get_num_triangles(0),'materials':[str(s.material_slot_name) for s in mesh.static_materials],'players':players,'bounds':str(mesh.get_bounds())}
(root/'Saved/BombCountdown/import.json').write_text(json.dumps(report,indent=2),encoding='utf-8');unreal.log('BOMB_COUNTDOWN_IMPORTED '+json.dumps(report))
