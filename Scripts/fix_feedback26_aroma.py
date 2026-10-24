"""Update only the authored food steam and paw-shaped vent materials."""
import unreal,json
from pathlib import Path
m=unreal.load_asset('/Game/CollectionPrototype/Materials/M_BranchAroma')
color=steam=0
for n in unreal.ObjectIterator(unreal.MaterialExpression):
 if n.get_outer()!=m:continue
 if isinstance(n,unreal.MaterialExpressionConstant3Vector):
  n.set_editor_property('constant',unreal.LinearColor(.85,.78,.65,1));color+=1
 if isinstance(n,unreal.MaterialExpressionCustom):
  n.set_editor_property('code',"float v=UV.y; float alpha=0; for(int i=0;i<3;i++){float phase=i*2.3; float center=.5+v*(.15*sin(v*10-Time*1.1)+.07*sin(v*17-Time*.8+phase)); float width=.027+v*.105; float x=(UV.x-center)/width; alpha+=exp(-x*x*1.7)*(.19+.025*sin(v*12-Time*.75+phase));} float base=smoothstep(0,.035,v); float top=1-smoothstep(.4,.98,v); return saturate(alpha*base*top);");steam+=1
assert color==steam==1
unreal.MaterialEditingLibrary.recompile_material(m)
assert unreal.EditorAssetLibrary.save_loaded_asset(m)
vent=unreal.load_asset('/Game/CollectionPrototype/Materials/M_BranchVent')
code="""float2 p=(UV-.5)*2;
// Rounded central pad with four separate oval toes. No noisy torn rim.
float2 c[3]={float2(0,.22),float2(-.24,.43),float2(.24,.43)};
float2 r[3]={float2(.34,.36),float2(.31,.23),float2(.31,.23)};
float pad=10;
for(int i=0;i<3;i++){float d=(length((p-c[i])/r[i])-1)*min(r[i].x,r[i].y);float h=saturate(.5+.5*(pad-d)/.10);pad=lerp(pad,d,h)-.10*h*(1-h);}
float2 toes[4]={float2(-.64,-.04),float2(-.26,-.48),float2(.26,-.48),float2(.64,-.04)};
float toe=10;
for(int j=0;j<4;j++){float2 q=p-toes[j];float a=(j<2?1:-1)*(j==0||j==3?.28:.12);q=float2(q.x*cos(a)-q.y*sin(a),q.x*sin(a)+q.y*cos(a));float2 s=(j==0||j==3)?float2(.16,.22):float2(.185,.275);toe=min(toe,(length(q/s)-1)*.16);}
return (1-smoothstep(-.008,.012,min(pad,toe)))*.96;
"""
count=0
for n in unreal.ObjectIterator(unreal.MaterialExpressionCustom):
 if n.get_outer()==vent:n.set_editor_property('code',code);count+=1
assert count==1
unreal.MaterialEditingLibrary.recompile_material(vent)
assert unreal.EditorAssetLibrary.save_loaded_asset(vent)
Path(unreal.Paths.project_dir(),'Saved/Validation/PlayerFeedback26/aroma.json').write_text(json.dumps({'passed':True,'steam_nodes':steam,'paw_nodes':count}),encoding='utf8')
unreal.SystemLibrary.quit_editor()
