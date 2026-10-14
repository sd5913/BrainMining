"""Idempotent native Niagara MCP edits for the toilet's original water art.

Run with the project editor and its MCP endpoint open. Source pack systems stay
untouched; all simulation edits target the two HeavenPreview copies.
"""
from wensi_mcp import rpc
import json,pathlib
TOOLSET='NiagaraToolsets.NiagaraToolset_System'
def call(name,args):
 result=rpc('tools/call',{'name':'call_tool','arguments':{'toolset_name':TOOLSET,'tool_name':name,'arguments':args}})['result']
 if result.get('isError'):raise RuntimeError(result)
 return json.loads(result['content'][0]['text']).get('returnValue')
def ref(system,emitter='',script='',module='',inputs=None):
 return {'system':{'refPath':system+'.'+system.rsplit('/',1)[-1]},'emitterName':emitter,'scriptName':script,'moduleName':module,'rendererIndex':-1,'inputNameStack':inputs or []}
def set_value(system,e,s,m,path,kind,value):
 return call('SetStackInputData',{'stackInputRef':ref(system,e,s,m,path),'inputData':{'struct':{'refPath':kind},'value':value}})
water='/Game/HeavenPreview/VFX/NS_HeavenToiletWater';burst='/Game/HeavenPreview/VFX/NS_HeavenToiletOpeningSplash'
velocity={'refPath':'/Niagara/Modules/Spawn/Velocity/AddVelocity.AddVelocity'}
for e,spread,lo,hi,rate in [('main',30,380,460,110),('flow',55,350,460,180),('flowdummy',55,350,460,50),('flow_ref',40,390,470,80)]:
 topo=call('GetEmitterTopology',{'emitterRef':ref(water,e)})
 if not any(m['moduleName']=='AddVelocity' for m in topo['particleSpawnScript']['modules']):call('AddModule',{'moduleLocationRef':ref(water,e,'ParticleSpawnScript'),'moduleAsset':velocity})
 set_value(water,e,'ParticleSpawnScript','AddVelocity',['Velocity'],'/Script/NiagaraEditor.NiagaraExt_StackInputData_DynamicInput',{'dynamicInputAsset':{'refPath':'/Niagara/DynamicInputs/UniformRange/UniformRangedVector.UniformRangedVector'}})
 for name,z,sign in [('Minimum',lo,-1),('Maximum',hi,1)]:set_value(water,e,'ParticleSpawnScript','AddVelocity',['Velocity',name],'/Script/CoreUObject.Vector3f',{'x':sign*spread,'y':sign*spread,'z':z})
 set_value(water,e,'EmitterUpdateScript','SpawnRate',['SpawnRate'],'/Script/Niagara.NiagaraFloat',{'value':rate})
 for name,v in [('Minimum',2),('Maximum',2.7)]:set_value(water,e,'ParticleSpawnScript','InitializeParticle',['Lifetime',name],'/Script/Niagara.NiagaraFloat',{'value':v})
set_value(water,'main','ParticleUpdateScript','AccelerationForce',['Acceleration'],'/Script/CoreUObject.Vector3f',{'x':0,'y':0,'z':-980})
for name,v in [('Minimum',8),('Maximum',14)]:set_value(water,'main','ParticleSpawnScript','InitializeParticle',['Sprite Size','Value',name],'/Script/Niagara.NiagaraFloat',{'value':v})

def renderer(system,e,values):
 r=ref(system,e);r['rendererIndex']=0
 return call('SetRendererData',{'renderer':r,'rendererData':{'propertyValues':json.dumps(values)}})

# Leak source used metre-long, independently randomised strip sprites. Bound both
# axes so even the worst random combination stays below a 1.5:1 aspect ratio.
for e in ['flow','flow_ref']:
 for name,size in [('Minimum',4),('Maximum',6)]:
  set_value(water,e,'ParticleSpawnScript','InitializeParticle',['Sprite Size',name],'/Script/CoreUObject.Vector2f',{'x':size,'y':size})
 set_value(water,e,'ParticleUpdateScript','ScaleSpriteSize',['Scale Factor'],'/Script/CoreUObject.Vector2f',{'x':1,'y':1})
 renderer(water,e,{'Material':{'refPath':'/Game/Realistic_Starter_VFX_Pack_Niagara/Materials/M_WaterDrop.M_WaterDrop'},'PivotInUVSpace':{'x':.5,'y':.5},'SubImageSize':{'x':1,'y':1},'Alignment':'Unaligned'})
# The CPU dummy still supplies collision/death events, but must not render dirt.
renderer(water,'flowdummy',{'bIsEnabled':False})
topo=call('GetEmitterTopology',{'emitterRef':ref(water,'main')})
if not any(m['moduleName']=='Collision' for m in topo['particleUpdateScript']['modules']):call('AddModule',{'moduleLocationRef':ref(water,'main','ParticleUpdateScript'),'moduleAsset':{'refPath':'/Niagara/Modules/Collision/Collision.Collision'}})
summary=call('GetSystemSummary',{'system':ref(burst)['system']})
for e in ['dirt_main','dirt','dirt002']:
 if any(x['emitterName']==e for x in summary['emitters']):call('RemoveEmitter',{'emitterToRemove':ref(burst,e)})
set_value(burst,'','SystemUpdateScript','SystemState',['Loop Behavior'],'/Script/NiagaraEditor.NiagaraExt_StackInputData_Enum',{'enum':{'refPath':'/Niagara/Enums/ENiagara_EmitterStateOptions.ENiagara_EmitterStateOptions'},'enumName':'ENiagara_EmitterStateOptions::NewEnumerator1','displayName':'Once'})
for e in ['drops','drops_dummy','main','drops001']:
 for name,z,sign in [('Minimum',400,-1),('Maximum',500,1)]:set_value(burst,e,'ParticleSpawnScript','AddVelocity',['Velocity',name],'/Script/CoreUObject.Vector3f',{'x':sign*65,'y':sign*65,'z':z})
set_value(burst,'main','ParticleUpdateScript','AccelerationForce',['Acceleration'],'/Script/CoreUObject.Vector3f',{'x':0,'y':0,'z':-980})
for e,lo,hi in [('drops',4,6),('drops001',10,15)]:
 for name,size in [('Minimum',lo),('Maximum',hi)]:
  set_value(burst,e,'ParticleSpawnScript','InitializeParticle',['Sprite Size',name],'/Script/CoreUObject.Vector2f',{'x':size,'y':size})
 topology=call('GetEmitterTopology',{'emitterRef':ref(burst,e)})
 if any(m['moduleName']=='ScaleSpriteSize' for m in topology['particleUpdateScript']['modules']):
  set_value(burst,e,'ParticleUpdateScript','ScaleSpriteSize',['Scale Factor'],'/Script/CoreUObject.Vector2f',{'x':1,'y':1})
 renderer(burst,e,{'PivotInUVSpace':{'x':.5,'y':.5},'Alignment':'Unaligned'})
for name,size in [('Minimum',10),('Maximum',18)]:
 set_value(burst,'main','ParticleSpawnScript','InitializeParticle',['Sprite Size','Value',name],'/Script/Niagara.NiagaraFloat',{'value':size})
renderer(burst,'drops_dummy',{'bIsEnabled':False})
print('TOILET_WATER_TUNED: save both Niagara assets in the editor after compile.')
