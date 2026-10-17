import unreal,json
from pathlib import Path
root=Path(unreal.Paths.project_dir());A=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);L=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
AT=unreal.AssetToolsHelpers.get_asset_tools();ML=unreal.MaterialEditingLibrary;EA=unreal.EditorAssetLibrary
task=unreal.AssetImportTask();task.filename=str(root/'SourceAssets/Opening/TitleTransparent.png');task.destination_path='/Game/StartRoom/UI';task.destination_name='T_OpeningTitle';task.automated=True;task.save=True;task.replace_existing=True;AT.import_asset_tasks([task])
t=unreal.load_asset('/Game/StartRoom/UI/T_OpeningTitle');assert t
t.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_EDITOR_ICON);t.set_editor_property('lod_group',unreal.TextureGroup.TEXTUREGROUP_UI);EA.save_loaded_asset(t)
base='/Game/Passages/Echo/ArtP2/StyledVFX'
EA.make_directory(base)
for n in ['ArcImpact','Ignition','Finale','Spark','Muzzle']:
 dst=base+'/NS_Styled_'+n
 s=unreal.load_asset(dst) or EA.duplicate_asset('/Game/Passages/Echo/ArtP2/VFX/NS_P2_'+n,dst)
 assert s;EA.save_loaded_asset(s)
m=unreal.load_asset(base+'/M_SoftEnergy') or AT.create_asset('M_SoftEnergy',base,unreal.Material,unreal.MaterialFactoryNew())
ML.delete_all_material_expressions(m);m.set_editor_property('blend_mode',unreal.BlendMode.BLEND_ADDITIVE);m.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT);m.set_editor_property('two_sided',True)
def node(c):return ML.create_material_expression(m,c)
color=node(unreal.MaterialExpressionVectorParameter);color.set_editor_property('parameter_name','Color');color.set_editor_property('default_value',unreal.LinearColor(.06,.62,1,1))
gain=node(unreal.MaterialExpressionConstant);gain.set_editor_property('r',7.)
mul=node(unreal.MaterialExpressionMultiply);ML.connect_material_expressions(color,'',mul,'A');ML.connect_material_expressions(gain,'',mul,'B');ML.connect_material_property(mul,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
f=node(unreal.MaterialExpressionFresnel);f.set_editor_property('exponent',2.5);f.set_editor_property('base_reflect_fraction',.04)
inv=node(unreal.MaterialExpressionOneMinus);assert ML.connect_material_expressions(f,'',inv,'')
alpha=node(unreal.MaterialExpressionScalarParameter);alpha.set_editor_property('parameter_name','Opacity');alpha.set_editor_property('default_value',.7)
mult=node(unreal.MaterialExpressionMultiply);ML.connect_material_expressions(inv,'',mult,'A');ML.connect_material_expressions(alpha,'',mult,'B');ML.connect_material_property(mult,'',unreal.MaterialProperty.MP_OPACITY)
ML.recompile_material(m);EA.save_loaded_asset(m)
# Retain the central interactive actor and remove the duplicated index-0 copy.
assert L.load_level('/Game/PopupClose/Maps/Lvl_PopupClose1')
devices=[a for a in A.get_all_level_actors() if isinstance(a,unreal.PopupOfficeDevice)]
dupe=next((a for a in devices if a.get_name()=='PopupOfficeDevice_4'),None)
if dupe:
 assert dupe.get_editor_property('device_index')==0 and any(a.get_name()=='PopupOfficeDevice_0' for a in devices)
 assert A.destroy_actor(dupe)
assert len([a for a in A.get_all_level_actors() if isinstance(a,unreal.PopupOfficeDevice) and a.get_editor_property('device_index')==0])==1
assert L.save_current_level()
unreal.log('OPENING_STYLE_ASSETS_PREPARED')
