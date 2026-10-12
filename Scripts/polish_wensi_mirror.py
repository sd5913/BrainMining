"""Apply the requested frosted mirrors and open statue display to the new map."""
import unreal

MAP='/Game/WenSi/Maps/Lvl_WenSi_MirrorMuseum'
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
if not world.get_path_name().startswith(MAP+'.'):
    assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level(MAP)
    world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
retired=[]
for actor in actors.get_all_level_actors():
    label=actor.get_actor_label()
    if label.startswith(('StatueGlass_','StatueGlassBack_','StatueGlassFront_','StatueShowcaseTop_','ShowcaseFrame_')):
        retired.append(actor)
for actor in retired:
    assert actors.destroy_actor(actor)

path='/Game/WenSi/Mirror/Materials'
material=unreal.load_asset(path+'/M_WenSiMirror')
assert material
unreal.MaterialEditingLibrary.delete_all_material_expressions(material)
material.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT)
make=lambda cls: unreal.MaterialEditingLibrary.create_material_expression(material,cls)
texture=make(unreal.MaterialExpressionTextureSampleParameter2D)
texture.set_editor_property('parameter_name','MirrorTexture')
texture.set_editor_property('texture',unreal.load_asset(path+'/RT_WenSiMirrorDefault'))
texture.set_editor_property('sampler_type',unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
uv=make(unreal.MaterialExpressionTextureCoordinate)
u=make(unreal.MaterialExpressionComponentMask);u.set_editor_property('r',True);u.set_editor_property('g',False)
v=make(unreal.MaterialExpressionComponentMask);v.set_editor_property('r',False);v.set_editor_property('g',True)
flip=make(unreal.MaterialExpressionOneMinus)
append=make(unreal.MaterialExpressionAppendVector)
gain=make(unreal.MaterialExpressionScalarParameter);gain.set_editor_property('parameter_name','ReflectionStrength');gain.set_editor_property('default_value',.38)
tint=make(unreal.MaterialExpressionVectorParameter);tint.set_editor_property('parameter_name','FrostTint');tint.set_editor_property('default_value',unreal.LinearColor(.24,.28,.31,1))
multiply=make(unreal.MaterialExpressionMultiply)
add=make(unreal.MaterialExpressionAdd)
def connect(source,output,target,pin):
    assert unreal.MaterialEditingLibrary.connect_material_expressions(source,output,target,pin),(source.get_name(),target.get_name(),pin)
connect(uv,'',u,'');connect(uv,'',v,'');connect(u,'',flip,'')
connect(flip,'',append,'A');connect(v,'',append,'B');connect(append,'',texture,'UVs')
connect(texture,'RGB',multiply,'A');connect(gain,'',multiply,'B')
connect(multiply,'',add,'A');connect(tint,'RGB',add,'B')
assert unreal.MaterialEditingLibrary.connect_material_property(add,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
unreal.MaterialEditingLibrary.recompile_material(material)
assert unreal.EditorAssetLibrary.save_loaded_asset(material,False)
assert unreal.EditorLoadingAndSavingUtils.save_map(world,MAP)
unreal.log('WENSI_POLISH_SAVED: removed_statue_enclosure_parts='+str(len(retired)))
