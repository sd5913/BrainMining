"""Exaggerated but bounded upward gush and an independent exposed-pipe drip."""
from wensi_mcp import rpc
import json
TOOLSET='NiagaraToolsets.NiagaraToolset_System'
def call(name,args):
 r=rpc('tools/call',{'name':'call_tool','arguments':{'toolset_name':TOOLSET,'tool_name':name,'arguments':args}})['result']
 if r.get('isError'):raise RuntimeError(r)
 return json.loads(r['content'][0]['text']).get('returnValue')
def ref(system,e='',s='',m='',path=None):return {'system':{'refPath':system+'.'+system.rsplit('/',1)[-1]},'emitterName':e,'scriptName':s,'moduleName':m,'rendererIndex':-1,'inputNameStack':path or []}
def value(sys,e,s,m,path,kind,v):return call('SetStackInputData',{'stackInputRef':ref(sys,e,s,m,path),'inputData':{'struct':{'refPath':kind},'value':v}})
def renderer(sys,e,v):
 r=ref(sys,e);r['rendererIndex']=0;return call('SetRendererData',{'renderer':r,'rendererData':{'propertyValues':json.dumps(v)}})
water='/Game/HeavenPreview/VFX/NS_HeavenToiletWater';drip='/Game/HeavenPreview/VFX/NS_HeavenPipeDrip'
for sys,dripping in [(water,False),(drip,True)]:
 for e,rate,spread,lo,hi in [('main',9 if dripping else 320,3 if dripping else 28,-35 if dripping else 790,-15 if dripping else 880),('flow',0 if dripping else 240,70,640,820),('flowdummy',0 if dripping else 25,70,640,820),('flow_ref',0 if dripping else 45,80,650,820)]:
  value(sys,e,'EmitterUpdateScript','SpawnRate',['SpawnRate'],'/Script/Niagara.NiagaraFloat',{'value':rate})
  for n,z,sign in [('Minimum',lo,-1),('Maximum',hi,1)]:value(sys,e,'ParticleSpawnScript','AddVelocity',['Velocity',n],'/Script/CoreUObject.Vector3f',{'x':sign*spread,'y':sign*spread,'z':z})
  for n,v in [('Minimum',.55 if dripping else 1.7),('Maximum',.7 if dripping else 2.1)]:value(sys,e,'ParticleSpawnScript','InitializeParticle',['Lifetime',n],'/Script/Niagara.NiagaraFloat',{'value':v})
 value(sys,'main','ParticleUpdateScript','AccelerationForce',['Acceleration'],'/Script/CoreUObject.Vector3f',{'x':0,'y':0,'z':-980})
 value(sys,'main','ParticleSpawnScript','SystemLocation',['Offset'],'/Script/CoreUObject.Vector3f',{'x':0,'y':0,'z':0})
 for n,v in [('Minimum',2.2 if dripping else 5),('Maximum',3. if dripping else 8)]:value(sys,'main','ParticleSpawnScript','InitializeParticle',['Sprite Size','Value',n],'/Script/Niagara.NiagaraFloat',{'value':v})
 value(sys,'main','ParticleUpdateScript','ScaleSpriteSize',['Scale Factor'],'/Script/CoreUObject.Vector2f',{'x':1,'y':1})
 renderer(sys,'main',{'PivotInUVSpace':{'x':.5,'y':.5},'bCastShadows':False,'Alignment':'Unaligned'})
 for e in ['flow','flow_ref']:
  for n,v in [('Minimum',3),('Maximum',5)]:value(sys,e,'ParticleSpawnScript','InitializeParticle',['Sprite Size',n],'/Script/CoreUObject.Vector2f',{'x':v,'y':v})
  renderer(sys,e,{'bCastShadows':False})
summary=call('GetEmitterSummary',{'emitterRef':ref(water,'main')})
mesh_index=next((i for i,c in enumerate(summary['rendererClasses']) if c['refPath']=='/Script/Niagara.NiagaraMeshRendererProperties'),None)
if mesh_index is None:mesh_index=call('AddRenderer',{'newRendererLocation':ref(water,'main'),'rendererClass':{'refPath':'/Script/Niagara.NiagaraMeshRendererProperties'}})['rendererIndex']
r=ref(water,'main');r['rendererIndex']=mesh_index
call('SetRendererData',{'renderer':r,'rendererData':{'propertyValues':json.dumps({'Meshes':[{'Mesh':{'refPath':'/Engine/BasicShapes/Sphere.Sphere'},'Scale':{'x':.06,'y':.06,'z':.15}}],'bOverrideMaterials':True,'OverrideMaterials':[{'ExplicitMat':{'refPath':'/Game/HeavenPreview/Materials/MI_HeavenGushCore.MI_HeavenGushCore'}}],'bCastShadows':False})}})
renderer(water,'main',{'bIsEnabled':False})
print('EXAGGERATED_GUSH_AND_PIPE_DRIP_TUNED')
