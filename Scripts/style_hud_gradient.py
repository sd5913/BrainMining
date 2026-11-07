"""Mirror a fading black HUD plate with bevelled outside corners and a fine rim."""
import datetime,json,shutil
from pathlib import Path
import unreal

root=Path(unreal.Paths.project_dir()).resolve()
out=root/'Saved/Validation/HUDGradient';out.mkdir(parents=True,exist_ok=True)
assert not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
material=unreal.load_asset('/Game/UI/MiningGUI/Immersive/M_InsetBackplate');assert material
backup=out/('before_'+datetime.datetime.now().strftime('%Y%m%d_%H%M%S'))
backup.mkdir()
shutil.copy2(root/'Content/UI/MiningGUI/Immersive/M_InsetBackplate.uasset',backup/'M_InsetBackplate.uasset')
lib=unreal.MaterialEditingLibrary
shader=next(x for x in unreal.ObjectIterator(unreal.MaterialExpressionCustom) if x.get_outer()==material)

def scalar(name,value):
    existing=next((x for x in unreal.ObjectIterator(unreal.MaterialExpressionScalarParameter)
                   if x.get_outer()==material and str(x.get_editor_property('parameter_name'))==name),None)
    node=existing or lib.create_material_expression(material,unreal.MaterialExpressionScalarParameter)
    node.set_editor_property('parameter_name',name);node.set_editor_property('default_value',value)
    return node

parameters=[('ChamferPixels',10.),('BorderPixels',1.2),('BorderOpacity',.68),('FadePower',.55),
            ('TopTaperPixels',2.),('BottomTaperPixels',2.)]
pins=list(shader.get_editor_property('inputs'))
for name,value in parameters:
    if not any(str(pin.get_editor_property('input_name'))==name for pin in pins):
        pin=unreal.CustomInput();pin.set_editor_property('input_name',name);pins.append(pin)
shader.set_editor_property('inputs',pins)
for name,value in parameters:assert lib.connect_material_expressions(scalar(name,value),'',shader,name)
shader.set_editor_property('description','Mirrored outer bevel and fine bone-colour rim; black fades inward')
shader.set_editor_property('code','''
float2 size=max(PanelSize.xy,float2(1,1));
// x increases towards the outside edge on both sides of the screen.
float outward=OuterSide>0.5?UV.x:1-UV.x;
float x=outward*size.x;
float y=UV.y*size.y;
// Independent caps let stacked cards taper only at the group's outer edges.
// The two facing edges stay straight and keep their constant spacing.
// Defaults preserve the existing two-sided taper on right-hand panels.
float top=1.0+(1-outward)*TopTaperPixels;
float bottom=size.y-1.0-(1-outward)*BottomTaperPixels;
float outer=size.x-1.0-x;
float topDistance=y-top;
float bottomDistance=bottom-y;
float bevel=min(ChamferPixels,min(size.x,size.y)*.18);
float d=min(min(outer,topDistance),bottomDistance);
d=min(d,(outer+topDistance-bevel)*.70710678);
d=min(d,(outer+bottomDistance-bevel)*.70710678);
float shape=smoothstep(-.65,.65,d);
float edge=1-smoothstep(max(.1,BorderPixels-.35),BorderPixels+.65,d);
float fade=smoothstep(0,.035,outward)*pow(max(outward,.00001),max(.1,FadePower));
float fillAlpha=BackdropOpacity*fade;
float rimAlpha=BorderOpacity*pow(saturate(outward),.8);
float3 ink=float3(.009,.012,.013);
float3 rim=float3(.76,.79,.69);
return float4(lerp(ink,rim,edge),lerp(fillAlpha,rimAlpha,edge)*shape);
''')
lib.recompile_material(material)
assert unreal.EditorAssetLibrary.save_loaded_asset(material,only_if_is_dirty=False)
(out/'authoring.json').write_text(json.dumps({'material':material.get_path_name(),'backup':str(backup),
    'parameters':dict(parameters),'left_outer_side':0,'right_outer_side':1,'map_saved':False},indent=2),encoding='utf-8')
unreal.log('HUD_GRADIENT_SAVED')
