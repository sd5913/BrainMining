import json
from pathlib import Path
from wensi_editor_tool import call
TS='NiagaraToolsets.NiagaraToolset_System'
def invoke(name,args):return json.loads(call(TS,name,args)['result']['content'][0]['text'])['returnValue']
out=[]
for n in ['ArcImpact','Ignition','Finale','Spark','Muzzle']:
 system={'refPath':f'/Game/Passages/Echo/ArtP2/StyledVFX/NS_Styled_{n}.NS_Styled_{n}'}
 summary=invoke('GetSystemSummary',{'system':system})
 for e in summary['emitters']:
  ref=dict(system=system,emitterName=e['emitterName'],scriptName='',moduleName='',rendererIndex=0,inputNameStack=[])
  top=invoke('GetEmitterTopology',{'emitterRef':ref})
  for key in ['emitterUpdateScript','particleSpawnScript','particleUpdateScript']:
   for m in top.get(key,{}).get('modules',[]):
    inputs=[i for i in m['inputs'] if i.get('bIsEditable') and any(s in i['name'].lower() for s in ['color','spawn count','spawn rate','lifetime'])]
    if inputs:
     rr=dict(ref,scriptName=top[key]['scriptName'],moduleName=m['moduleName'])
     values=invoke('GetModuleInputValues',{'moduleRef':rr})
     out.append(dict(effect=n,ref=rr,inputs=inputs,values=values))
Path('Saved/Validation/IntroP2Office/styled_inputs.json').write_text(json.dumps(out,indent=2),encoding='utf-8')
print('Found',len(out),'modules')
