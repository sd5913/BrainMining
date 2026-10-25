import unreal,sys,json,traceback
from pathlib import Path
root=Path(unreal.Paths.project_dir());sys.path.insert(0,str(root/'Scripts'))
from scene_polish_materials import mat,node,wire,prop,finish,custom
report={'passed':False}
try:
 m=mat('/Game/CollectionPrototype/Materials/M_BranchClueFlat');m.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT)
 tint=node(m,unreal.MaterialExpressionVectorParameter,parameter_name='Tint',default_value=unreal.LinearColor(.03,.03,.03,1));prop(tint,unreal.MaterialProperty.MP_EMISSIVE_COLOR,'RGB');finish(m)
 m=mat('/Game/CollectionPrototype/Materials/M_BranchAroma');m.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT);m.set_editor_property('blend_mode',unreal.BlendMode.BLEND_TRANSLUCENT);m.set_editor_property('two_sided',True)
 uv=node(m,unreal.MaterialExpressionTextureCoordinate);t=node(m,unreal.MaterialExpressionTime)
 prop(node(m,unreal.MaterialExpressionConstant,r=0.),unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
 steam="""float v=UV.y; float alpha=0; for(int i=0;i<3;i++){float phase=i*2.3; float center=.5+v*(.15*sin(v*10-Time*1.1)+.07*sin(v*17-Time*.8+phase)); float width=.018+v*.075; float x=(UV.x-center)/width; alpha+=exp(-x*x*1.7)*(.12+.025*sin(v*12-Time*.75+phase));} float base=smoothstep(0,.035,v); float top=1-smoothstep(.32,.94,v); return saturate(alpha*base*top);"""
 opacity=custom(m,steam,{'UV':(uv,''),'Time':(t,'')},unreal.CustomMaterialOutputType.CMOT_FLOAT1)
 depth=node(m,unreal.MaterialExpressionDepthFade,fade_distance_default=65.)
 wire(opacity,depth,'Opacity');prop(depth,unreal.MaterialProperty.MP_OPACITY)
 prop(node(m,unreal.MaterialExpressionConstant3Vector,constant=unreal.LinearColor(.32,.29,.24,1)),unreal.MaterialProperty.MP_EMISSIVE_COLOR);finish(m)
 m=mat('/Game/CollectionPrototype/Materials/M_BranchInkClue');m.set_editor_property('material_domain',unreal.MaterialDomain.MD_DEFERRED_DECAL);m.set_editor_property('blend_mode',unreal.BlendMode.BLEND_TRANSLUCENT)
 uv=node(m,unreal.MaterialExpressionTextureCoordinate)
 # Eight trigrams, arranged clockwise from Qian. Leave the center completely empty.
 code="""float2 p=(UV-.5)*2; float angle=atan2(p.x,-p.y); float sector=floor(angle/.785398163+ .5); float a=sector*.785398163; float2 n=float2(sin(a),-cos(a)); float r=dot(p,n),x=abs(dot(p,float2(n.y,-n.x))); int index=((int)sector+8)%8; int masks[8]={7,6,2,4,0,1,5,3}; float paint=0; for(int row=0;row<3;row++){float stroke=(1-smoothstep(.022,.029,abs(r-(.51+row*.145))))*(1-smoothstep(.145,.153,x)); if((masks[index] & (1<<row))==0)stroke*=smoothstep(.026,.033,x); paint=max(paint,stroke);} return paint*.91;"""
 prop(custom(m,code,{'UV':(uv,'')},unreal.CustomMaterialOutputType.CMOT_FLOAT1),unreal.MaterialProperty.MP_OPACITY)
 prop(node(m,unreal.MaterialExpressionConstant3Vector,constant=unreal.LinearColor(.012,.016,.022,1)),unreal.MaterialProperty.MP_BASE_COLOR)
 prop(node(m,unreal.MaterialExpressionConstant,r=.97),unreal.MaterialProperty.MP_ROUGHNESS);finish(m)
 m=mat('/Game/CollectionPrototype/Materials/M_BranchVent');m.set_editor_property('material_domain',unreal.MaterialDomain.MD_DEFERRED_DECAL);m.set_editor_property('blend_mode',unreal.BlendMode.BLEND_TRANSLUCENT)
 uv=node(m,unreal.MaterialExpressionTextureCoordinate)
 prop(custom(m,'float2 p=(UV-.5)*2; float a=atan2(p.y,p.x); float r=length(p); float edge=.65+.06*sin(a*5)+.035*sin(a*9+1);return (1-smoothstep(edge-.13,edge+.08,r))*.94;',{'UV':(uv,'')},unreal.CustomMaterialOutputType.CMOT_FLOAT1),unreal.MaterialProperty.MP_OPACITY)
 prop(node(m,unreal.MaterialExpressionConstant3Vector,constant=unreal.LinearColor(.012,.007,.003,1)),unreal.MaterialProperty.MP_BASE_COLOR)
 prop(node(m,unreal.MaterialExpressionConstant,r=1.),unreal.MaterialProperty.MP_ROUGHNESS);finish(m)
 report['passed']=True
except Exception:report['error']=traceback.format_exc();unreal.log_error(report['error'])
finally:
 (root/'Saved/Validation/BranchClues/materials.json').write_text(json.dumps(report,indent=2),encoding='utf8');unreal.SystemLibrary.quit_editor()
