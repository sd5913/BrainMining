"""Tune a separate copy of Epic's continuous smoke plume via Unreal MCP."""
import json,pathlib
from wensi_editor_tool import call
T='NiagaraToolsets.NiagaraToolset_System'
system={'refPath':'/Game/WenSi/Museum/Refresh/VFX/NS_MuseumSmoke.NS_MuseumSmoke'}
def ref(script,module,inputs=[]):
 return dict(system=system,emitterName='SmokePlume',scriptName=script,moduleName=module,rendererIndex=-1,inputNameStack=inputs)
changes=[('ParticleSpawnScript','InitializeParticle','Lifetime Min','/Script/Niagara.NiagaraFloat',{'value':2.2}),('ParticleSpawnScript','InitializeParticle','Lifetime Max','/Script/Niagara.NiagaraFloat',{'value':4.2}),('ParticleUpdateScript','LinearForce','Force','/Script/CoreUObject.Vector3f',{'x':0,'y':0,'z':7})]
report=[]
for script,module,name,typ,val in changes:
 r=call(T,'SetStackInputData',{'stackInputRef':ref(script,module,[name]),'inputData':{'struct':{'refPath':typ},'value':val}})
 report.append(r)
 print(module,name,json.dumps(r),flush=True)
# The gallery has no outdoor wind; keep the plume inside its exhibit envelope.
r=call(T,'SetModuleEnabled',{'moduleRef':ref('ParticleUpdateScript','WindForce'),'bEnabled':False});report.append(r);print('WindForce',json.dumps(r),flush=True)
pathlib.Path('Saved/Validation/MuseumAssetRefresh/smoke_tuning.json').write_text(json.dumps(report))
