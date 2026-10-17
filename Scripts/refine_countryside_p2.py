import unreal, math, json
from pathlib import Path
A=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);L=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ML=unreal.MaterialEditingLibrary;AT=unreal.AssetToolsHelpers.get_asset_tools();base='/Game/Passages/Echo/ArtP2/Materials/'
def build(name,code,core=False):
 m=unreal.load_asset(base+name)
 if not m:m=AT.create_asset(name,base.rstrip('/'),unreal.Material,unreal.MaterialFactoryNew())
 ML.delete_all_material_expressions(m);m.set_editor_property('blend_mode',unreal.BlendMode.BLEND_ADDITIVE);m.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT);m.set_editor_property('two_sided',True)
 def expr(cls):return ML.create_material_expression(m,cls)
 c=expr(unreal.MaterialExpressionVectorParameter);c.set_editor_property('parameter_name','Color');c.set_editor_property('default_value',unreal.LinearColor(.12,1,2,1))
 gain=expr(unreal.MaterialExpressionConstant);gain.set_editor_property('r',160. if core else 30.)
 emission=expr(unreal.MaterialExpressionMultiply);ML.connect_material_expressions(c,'',emission,'A');ML.connect_material_expressions(gain,'',emission,'B');ML.connect_material_property(emission,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
 uv=expr(unreal.MaterialExpressionTextureCoordinate);mask=expr(unreal.MaterialExpressionCustom)
 ins=[]
 for n in ['UV','T']:
  i=unreal.CustomInput();i.set_editor_property('input_name',n);ins.append(i)
 mask.set_editor_property('inputs',ins);mask.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT1);mask.set_editor_property('code',code)
 ML.connect_material_expressions(uv,'',mask,'UV');t=expr(unreal.MaterialExpressionTime);ML.connect_material_expressions(t,'',mask,'T')
 alpha=expr(unreal.MaterialExpressionScalarParameter);alpha.set_editor_property('parameter_name','Opacity');alpha.set_editor_property('default_value',1.)
 mult=expr(unreal.MaterialExpressionMultiply);ML.connect_material_expressions(mask,'',mult,'A');ML.connect_material_expressions(alpha,'',mult,'B');ML.connect_material_property(mult,'',unreal.MaterialProperty.MP_OPACITY)
 ML.recompile_material(m);unreal.EditorAssetLibrary.save_loaded_asset(m);return m
build('M_P2RuneRing','float2 p=(UV-.5)*2;float r=length(p);float a=atan2(p.y,p.x);float ring=1-smoothstep(.012,.035,abs(r-.82));float outer=(1-smoothstep(.007,.018,abs(r-.96)))*step(.35,frac(a*6.366));float glyph=(1-smoothstep(.014,.03,abs(r-.66)))*step(.65,frac(a*3.183));return saturate(ring+outer+glyph)*(1-smoothstep(.99,1,r));')
build('M_P2EnergyCore','float a=sin(UV.y*65+sin(UV.x*29+T*3)*2-T*6);float b=sin(UV.x*58+UV.y*21+T*7);return .18+.5*pow(saturate(a*b),3);',True)
# Avoid using an atlas material on a procedural lamp. Give the retained articulated fixture teal enamel.
m=unreal.load_asset(base+'M_P2LampEnamel')
if not m:
 m=AT.create_asset('M_P2LampEnamel',base.rstrip('/'),unreal.Material,unreal.MaterialFactoryNew())
 c=ML.create_material_expression(m,unreal.MaterialExpressionConstant3Vector);c.set_editor_property('constant',unreal.LinearColor(.055,.19,.17,1));ML.connect_material_property(c,'',unreal.MaterialProperty.MP_BASE_COLOR)
 for prop,value in [(unreal.MaterialProperty.MP_ROUGHNESS,.33),(unreal.MaterialProperty.MP_METALLIC,.48)]:
  c=ML.create_material_expression(m,unreal.MaterialExpressionConstant);c.set_editor_property('r',value);ML.connect_material_property(c,'',prop)
 ML.recompile_material(m);unreal.EditorAssetLibrary.save_loaded_asset(m)
for a in A.get_all_level_actors():
 if isinstance(a,unreal.StaticMeshActor) and a.get_actor_label().startswith('Study V10 / Lamp ') and not any(s in a.get_actor_label() for s in ['diffuser','cord']):a.static_mesh_component.set_material(0,m)
 # The pile's old tape still provides the gameplay-readable carton cue.
 if isinstance(a,unreal.StaticMeshActor) and 'Library/Bound volume' in a.get_actor_label():
  # Turn fore edges away: all spines face toward the player/entry.
  r=a.get_actor_rotation();a.set_actor_rotation(unreal.Rotator(pitch=r.pitch,yaw=180,roll=r.roll),False)
stage=next(a for a in A.get_all_level_actors() if isinstance(a,unreal.CardRoomStage))
for c in stage.get_components_by_class(unreal.SceneComponent):
 if c.get_name()=='Opponent':
  c.set_relative_location(stage.get_editor_property('opponent_base_local'),False,False);s=stage.get_editor_property('opponent_scale');c.set_relative_scale3d(unreal.Vector(s,s,s));c.set_visibility(True,True)
assert L.save_current_level()
unreal.log('P2_ART_REFINED_AND_VFX_MATERIALS_READY')
