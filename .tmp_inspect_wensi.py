import json,sys
sys.path.insert(0,'Scripts')
from wensi_editor_tool import call
base='/Game/WenSi/Maps/Lvl_WenSi_MirrorMuseum1.Lvl_WenSi_MirrorMuseum1:PersistentLevel.'
for i in list(range(12)):
 ref=base+f'WenSiMirrorExhibit_{i}'
 try:
  r=call('editor_toolset.toolsets.object.ObjectTools','get_properties',{'instance':{'refPath':ref},'properties':['kind','word','displayNoun']})
  print(i,r['result']['content'][0]['text'])
 except Exception as e: print(i,'ERR',e)
