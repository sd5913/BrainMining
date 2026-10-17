import json
from wensi_editor_tool import call
TS='NiagaraToolsets.NiagaraToolset_System'
for e in ['fire','fire_source']:
 ref=dict(system={'refPath':'/Game/Passages/Echo/ArtP2/StyledVFX/NS_Styled_Ignition.NS_Styled_Ignition'},emitterName=e,scriptName='',moduleName='',rendererIndex=0,inputNameStack=[])
 d={'Material':{'refPath':'/Game/Passages/Echo/ArtP2/StyledVFX/M_StyledFire.M_StyledFire'},'bCastShadows':False}
 r=json.loads(call(TS,'SetRendererData',{'renderer':ref,'rendererData':{'propertyValues':json.dumps(d)}})['result']['content'][0]['text'])
 assert r.get('returnValue') is not False,r
 print('Updated styled fire renderer:',e)
