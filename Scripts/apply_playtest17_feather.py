"""Keep white clue feathers legible in the mine's purple ambient lighting."""
import unreal, sys
from pathlib import Path
root=Path(unreal.Paths.project_dir());sys.path.insert(0,str(root/'Scripts'))
from scene_polish_materials import mat,node,wire,prop,finish
path='/Game/CollectionPrototype/Materials/M_CollectionFeatherFade'
m=mat(path)
m.set_editor_property('blend_mode',unreal.BlendMode.BLEND_TRANSLUCENT)
m.set_editor_property('two_sided',True)
tint=node(m,unreal.MaterialExpressionVectorParameter,parameter_name='Tint',default_value=unreal.LinearColor(1,1,1,1))
prop(tint,unreal.MaterialProperty.MP_BASE_COLOR,'RGB')
fill=node(m,unreal.MaterialExpressionMultiply,const_b=.6)
wire(tint,fill,'A','RGB');prop(fill,unreal.MaterialProperty.MP_EMISSIVE_COLOR)
prop(node(m,unreal.MaterialExpressionScalarParameter,parameter_name='Opacity',default_value=1.),unreal.MaterialProperty.MP_OPACITY)
prop(node(m,unreal.MaterialExpressionConstant,r=.9),unreal.MaterialProperty.MP_ROUGHNESS)
finish(m)
unreal.SystemLibrary.quit_editor()
