"""Expose the jar halo intensity for its HDR collection inspection render."""
import unreal,sys
from pathlib import Path
root=Path(unreal.Paths.project_dir());sys.path.insert(0,str(root/'Scripts'))
from scene_polish_materials import mat,node,wire,prop,finish
m=mat('/Game/CollectionPrototype/Materials/M_CollectionFireflyHalo')
m.set_editor_property('blend_mode',unreal.BlendMode.BLEND_ADDITIVE)
m.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT)
color=node(m,unreal.MaterialExpressionConstant3Vector,constant=unreal.LinearColor(.3,1,.06))
gain=node(m,unreal.MaterialExpressionScalarParameter,parameter_name='GlowGain',default_value=1.)
emissive=node(m,unreal.MaterialExpressionMultiply);wire(color,emissive,'A');wire(gain,emissive,'B')
prop(emissive,unreal.MaterialProperty.MP_EMISSIVE_COLOR)
edge=node(m,unreal.MaterialExpressionFresnel,exponent=1.4,base_reflect_fraction=0.)
inverse=node(m,unreal.MaterialExpressionOneMinus);wire(edge,inverse,'')
soft=node(m,unreal.MaterialExpressionPower,const_exponent=3.);wire(inverse,soft,'Base')
opacity=node(m,unreal.MaterialExpressionScalarParameter,parameter_name='GlowOpacity',default_value=.18)
mask=node(m,unreal.MaterialExpressionMultiply);wire(soft,mask,'A');wire(opacity,mask,'B')
prop(mask,unreal.MaterialProperty.MP_OPACITY);finish(m)
unreal.SystemLibrary.quit_editor()
