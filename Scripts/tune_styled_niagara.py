"""Style the duplicated source-pack systems through discovered UEMCP schemas."""
import json
from pathlib import Path
from wensi_editor_tool import call
TS='NiagaraToolsets.NiagaraToolset_System'
def invoke(name,args):
 r=json.loads(call(TS,name,args)['result']['content'][0]['text'])
 if r.get('returnValue') is False:raise RuntimeError((name,args,r))
 return r.get('returnValue')
rows=json.loads(Path('Saved/Validation/IntroP2Office/styled_inputs.json').read_text())
quiet={'ArcImpact':{'smoke','spark'},'Ignition':{'smoke','refr','sparks'},'Finale':{'spark','spark_l','parts','smoke_after','refr','project','main_ground','puff'},'Spark':{'spark','spark_burst','smoke','lightning','light','glow'},'Muzzle':{'sparks','smoke','muzzleflash_x','muzzleflash_y'}}
seen=set();changes=[]
for row in rows:
 effect=row['effect'];ref=row['ref'];emitter=ref['emitterName'];module=ref['moduleName'];key=(effect,emitter)
 if key not in seen:
  seen.add(key)
  invoke('SetEmitterData',{'emitter':ref,'emitterData':{'propertyValues':json.dumps({'bIsEnabled':emitter not in quiet[effect]})}})
 if emitter in quiet[effect]:continue
 warm=effect in ['Ignition','Spark'] or 'spark' in emitter
 color=(1.,.42,.09) if warm else (.06,.65,1.)
 for i in row['inputs']:
  name=i['name'];typ=None;value=None
  if name=='Color' and module=='InitializeParticle':typ='/Script/CoreUObject.LinearColor';value=dict(zip(['r','g','b','a'],(*color,1.)))
  elif name=='Color' and module=='Color':
   typ='/Script/NiagaraEditor.NiagaraExt_StackInputData_HlslExpression';value={'hlslExpression':'float4(%s,%s,%s,pow(saturate(1-Particles.NormalizedAge),1.35))'%color}
  elif name=='Scale Color':typ='/Script/CoreUObject.Vector3f';value=dict(x=12.,y=12.,z=12.)
  elif name=='Lifetime' and module=='InitializeParticle':typ='/Script/Niagara.NiagaraFloat';value={'value':1.15 if effect=='Finale' else .9}
  elif name=='Spawn Count':
   typ='/Script/Niagara.NiagaraInt32';value={'value':1 if emitter in ['glow','impact','front','shockwave'] else 8}
  if typ:
   invoke('SetStackInputData',{'stackInputRef':dict(ref,inputNameStack=[name]),'inputData':{'struct':{'refPath':typ},'value':value}})
   changes.append([effect,emitter,module,name,value])
Path('Saved/Validation/IntroP2Office/styled_changes.json').write_text(json.dumps(changes,indent=2),encoding='utf8')
print('Styled Niagara edits:',len(changes))
