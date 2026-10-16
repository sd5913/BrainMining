"""Build ending-only merged VAT walkers; source Fab assets remain untouched."""
import unreal
import shutil
from datetime import datetime
from pathlib import Path

ROOT='/Game/EndingEmergence/Crowd/Lite'
MAP='/Game/EndingEmergence/Maps/Lvl_EndingEmergence_Demo'
lib=unreal.MaterialEditingLibrary
assets=unreal.AssetToolsHelpers.get_asset_tools()
unreal.EditorAssetLibrary.make_directory(ROOT)
source=unreal.load_asset('/Game/EndingEmergence/Crowd/DA_EndingCrowd')
assert source
people=list(source.get_editor_property('people'))
# The bowed Old Man Walk is deliberately excluded from every baked character.
people[3].set_editor_property('walk',unreal.load_asset('/Game/EndingEmergence/Crowd/EE_CityMaleWalk'))

def create(name,cls,factory):
    p=ROOT+'/'+name
    return unreal.load_asset(p) if unreal.EditorAssetLibrary.does_asset_exist(p) else assets.create_asset(name,ROOT,cls,factory())

master=create('M_CrowdMotion',unreal.Material,unreal.MaterialFactoryNew)
lib.delete_all_material_expressions(master)
master.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT)
lib.set_material_usage(master,unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES)

def node(cls,**props):
    result=lib.create_material_expression(master,cls)
    for k,v in props.items():result.set_editor_property(k,v)
    return result

colors=node(unreal.MaterialExpressionVertexColor)
exposure=node(unreal.MaterialExpressionMultiply,const_b=.16)
assert lib.connect_material_expressions(colors,'',exposure,'A')
assert lib.connect_material_property(exposure,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
tex=node(unreal.MaterialExpressionTextureObjectParameter,parameter_name='Motion')
uv=node(unreal.MaterialExpressionTextureCoordinate)
time=node(unreal.MaterialExpressionTime)
phase=node(unreal.MaterialExpressionPerInstanceCustomData,data_index=0,const_default_value=0)
rate=node(unreal.MaterialExpressionPerInstanceCustomData,data_index=1,const_default_value=1)
period=node(unreal.MaterialExpressionScalarParameter,parameter_name='Period',default_value=1)
frames=node(unreal.MaterialExpressionScalarParameter,parameter_name='Frames',default_value=16)
custom=node(unreal.MaterialExpressionCustom,output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT3)
bindings=[('Motion',tex),('UV',uv),('Time',time),('Phase',phase),('Rate',rate),('Period',period),('Frames',frames)]
inputs=[]
for name,_ in bindings:
    item=unreal.CustomInput();item.set_editor_property('input_name',name);inputs.append(item)
custom.set_editor_property('inputs',inputs)
custom.set_editor_property('code','''
float f=frac(Time*Rate/max(Period,.01)+Phase)*Frames;
float a=floor(f),b=fmod(a+1,Frames);
float3 p=Texture2DSampleLevel(Motion,MotionSampler,float2(UV.x,(UV.y+a)/Frames),0).xyz;
float3 q=Texture2DSampleLevel(Motion,MotionSampler,float2(UV.x,(UV.y+b)/Frames),0).xyz;
return lerp(p,q,frac(f));
''')
for name,n in bindings: assert lib.connect_material_expressions(n,'',custom,name)
transform=node(unreal.MaterialExpressionTransform,
    transform_source_type=unreal.MaterialVectorCoordTransformSource.TRANSFORMSOURCE_INSTANCE,
    transform_type=unreal.MaterialVectorCoordTransform.TRANSFORM_WORLD)
assert lib.connect_material_expressions(custom,'',transform,'')
assert lib.connect_material_property(transform,'',unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)

meshes=[]
palette=[unreal.LinearColor(.22,.30,.34,1),unreal.LinearColor(.35,.24,.18,1),unreal.LinearColor(.14,.18,.27,1),unreal.LinearColor(.27,.18,.21,1)]
for index in range(9):
    idle=index==8
    person=people[0 if idle else index%4]
    name='SM_Hero' if idle else 'SM_Walker_'+str(index)
    path=ROOT+'/'+name
    # Reuse completed baking across interrupted runs; material graph is safe to refresh.
    mesh=unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else unreal.EndingCrowdBaker.bake_walker(person,path,1 if idle else 2 if index<4 else 3,idle,palette[0 if idle else index%4])
    assert mesh, 'Failed to bake '+path
    motion=unreal.load_asset(path+'_Motion');assert motion
    if index==0:tex.set_editor_property('texture',motion)
    material=create('MI_'+name,unreal.MaterialInstanceConstant,unreal.MaterialInstanceConstantFactoryNew)
    lib.set_material_instance_parent(material,master)
    lib.set_material_instance_texture_parameter_value(material,'Motion',motion)
    clip=person.get_editor_property('idle' if idle else 'walk')
    lib.set_material_instance_scalar_parameter_value(material,'Period',unreal.AnimationLibrary.get_sequence_length(clip))
    lib.set_material_instance_scalar_parameter_value(material,'Frames',1 if idle else 16)
    mesh.set_material(0,material)
    assert unreal.EditorAssetLibrary.save_loaded_asset(material)
    assert unreal.EditorAssetLibrary.save_loaded_asset(mesh)
    meshes.append(mesh)
lib.recompile_material(master)
assert unreal.EditorAssetLibrary.save_loaded_asset(master)
data_path=ROOT+'/DA_EndingCrowdLite'
if unreal.EditorAssetLibrary.does_asset_exist(data_path):data=unreal.load_asset(data_path)
else:
    factory=unreal.DataAssetFactory();factory.set_editor_property('data_asset_class',unreal.EndingCrowdAssets)
    data=assets.create_asset('DA_EndingCrowdLite',ROOT,unreal.EndingCrowdAssets,factory)
data.set_editor_property('people',[])
data.set_editor_property('baked_walkers',meshes[:8]);data.set_editor_property('baked_hero',meshes[8])
assert unreal.EditorAssetLibrary.save_loaded_asset(data)
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);assert levels.load_level(MAP)
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
demo=next(a for a in actors.get_all_level_actors() if isinstance(a,unreal.EndingEmergence))
backup=Path(unreal.Paths.project_saved_dir(),'Backups','EndingCrowdLite-'+datetime.now().strftime('%Y%m%d-%H%M%S')+'.umap')
backup.parent.mkdir(parents=True,exist_ok=True)
shutil.copy2(Path(unreal.Paths.project_content_dir(),'EndingEmergence/Maps/Lvl_EndingEmergence_Demo.umap'),backup)
demo.set_editor_property('crowd_assets',data)
assert levels.save_current_level()
unreal.log('ENDING_CROWD_LITE_APPLIED: 1201 people, 9 merged batches, no runtime skeletal meshes or source skin/clothing materials')
