import unreal
import json
from pathlib import Path
report=[]
for path in ['/Game/CitySampleCrowd/Character/Male/NormalWeight/Materials/MI_m_nrw_jeans', '/Game/CitySampleCrowd/Character/Female/NormalWeight/Materials/MI_f_nrw_jeans']:
    material=unreal.load_asset(path)
    row={'path':path,'vectors':{},'scalars':{}}
    for n in unreal.MaterialEditingLibrary.get_vector_parameter_names(material):
        row['vectors'][str(n)]=str(unreal.MaterialEditingLibrary.get_material_instance_vector_parameter_value(material,n))
    for n in unreal.MaterialEditingLibrary.get_scalar_parameter_names(material):
        row['scalars'][str(n)]=unreal.MaterialEditingLibrary.get_material_instance_scalar_parameter_value(material,n)
    report.append(row)
Path(unreal.Paths.project_saved_dir(),'Validation','EndingCrowdMaterials.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
unreal.log('ENDING_CROWD_MATERIALS_INSPECTED')
