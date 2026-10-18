"""Refresh ending pigment and create the animated soil cross-section material."""
from pathlib import Path
import json
import unreal

project = Path(unreal.Paths.project_dir())
old = unreal.load_asset('/Game/EndingEmergence/Materials/M_EE_ComicRock')
audit = {'shading_model': str(old.get_editor_property('shading_model'))}
getter = getattr(unreal.MaterialEditingLibrary, 'get_material_property_input_node', None)
if getter:
    node = getter(old, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    audit['emissive_node'] = node.get_class().get_name() if node else None
    if isinstance(node, unreal.MaterialExpressionCustom):
        audit['previous_shader'] = node.get_editor_property('code')
(project/'Saved/Validation/ending-rock-material-before.json').write_text(json.dumps(audit, indent=2), encoding='utf-8')

exec(compile((project/'Scripts/apply_ending_white_exit.py').read_text(encoding='utf-8-sig'), 'white_exit', 'exec'))
graphic('PaperCutaway', r'''
float2 p=float2((UV.x-.5)*2500,(.5-UV.y)*2000);
float progress=saturate(ApertureRadius), nearest=10000;
float2 a=float2(0,-1050);
[loop] for(int i=0;i<64;++i) {
    float t=min(progress,(i+1)/64.0);
    float2 b=float2(650*sin(t*3*3.141593)+120*sin(t*7*3.141593),-1050+2200*t);
    float2 ab=b-a;
    float along=saturate(dot(p-a,ab)/max(dot(ab,ab),.01));
    nearest=min(nearest,length(p-a-ab*along));
    a=b;
    if(t>=progress)break;
}
float band=floor((p.y+sin(p.x*.004)*27+sin(p.x*.011)*11)/125);
float layer=frac(sin(band*17.37)*43758.5453);
float grain=frac(sin(dot(floor(p*.18),float2(12.9898,78.233)))*43758.5453);
float grit=.91+.09*grain;
float3 soil=lerp(float3(.34,.13,.045),float3(.78,.43,.15),layer)*grit;
float lamina=smoothstep(.82,.97,frac((p.y+sin(p.x*.004)*27)/125));
soil*=1-lamina*.24;
float width=56+5*sin(p.y*.033);
float edge=1-smoothstep(width-4,width+5,nearest);
float depth=saturate(nearest/width);
float3 cavity=lerp(float3(.018,.009,.005),float3(.20,.078,.025),depth*depth);
float3 color=lerp(soil,cavity,edge*step(.001,progress));
float bevel=smoothstep(.94,1,max(abs(p.x)/1250,abs(p.y)/1000));
color*=1-bevel*.28;
float cut=step(abs(UV.x-.5)+abs(UV.y-.5),.94);
return float4(color*1.9,cut);
''', True)
unreal.log('ENDING_LIVING_CUTAWAY_READY')
