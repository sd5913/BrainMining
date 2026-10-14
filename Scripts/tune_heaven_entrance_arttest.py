"""Adapt the pack-derived droplets to horizontal basin outflow, without sprite stretching."""
from wensi_mcp import rpc
import json
SYSTEM='/Game/HeavenPreview/VFX/NS_HeavenEntranceFlow'
def ref(e='',s='',m='',path=None):
 return {'system':{'refPath':SYSTEM+'.NS_HeavenEntranceFlow'},'emitterName':e,'scriptName':s,'moduleName':m,'rendererIndex':-1,'inputNameStack':path or []}
def call(name,args):
 r=rpc('tools/call',{'name':'call_tool','arguments':{'toolset_name':'NiagaraToolsets.NiagaraToolset_System','tool_name':name,'arguments':args}})['result']
 if r.get('isError'):raise RuntimeError(r)
 return json.loads(r['content'][0]['text']).get('returnValue')
def value(e,s,m,path,kind,v):return call('SetStackInputData',{'stackInputRef':ref(e,s,m,path),'inputData':{'struct':{'refPath':kind},'value':v}})
for e,rate in [('main',100),('flow',150),('flowdummy',40),('flow_ref',60)]:
 for n,x,y,z in [('Minimum',-250,-20,10),('Maximum',-180,20,45)]:value(e,'ParticleSpawnScript','AddVelocity',['Velocity',n],'/Script/CoreUObject.Vector3f',{'x':x,'y':y,'z':z})
 value(e,'EmitterUpdateScript','SpawnRate',['SpawnRate'],'/Script/Niagara.NiagaraFloat',{'value':rate})
 for n,v in [('Minimum',.7),('Maximum',1.1)]:value(e,'ParticleSpawnScript','InitializeParticle',['Lifetime',n],'/Script/Niagara.NiagaraFloat',{'value':v})
value('main','ParticleUpdateScript','AccelerationForce',['Acceleration'],'/Script/CoreUObject.Vector3f',{'x':0,'y':0,'z':-980})
print('ENTRANCE_FLOW_TUNED')
