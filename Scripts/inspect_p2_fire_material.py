import unreal,json
from pathlib import Path
m=unreal.load_asset('/Game/Realistic_Starter_VFX_Pack_Niagara/Materials/M_Fire_3');ML=unreal.MaterialEditingLibrary
d={'textures':[t.get_path_name() for t in ML.get_used_textures(m)],'nodes':[]}
for n in ML.get_material_expressions(m):
 v={'class':n.get_class().get_name(),'name':n.get_name()}
 for p in ['texture','constant','default_value','parameter_name','code','r']:
  try:v[p]=str(n.get_editor_property(p))
  except:pass
 d['nodes'].append(v)
Path(unreal.Paths.project_dir(),'Saved/Validation/IntroP2Office/fire_material.json').write_text(json.dumps(d,indent=2),encoding='utf8')
