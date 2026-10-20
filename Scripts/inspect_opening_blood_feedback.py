import unreal,json
from pathlib import Path
rows={}
EA=unreal.EditorAssetLibrary; ML=unreal.MaterialEditingLibrary
for name in ['StartRoomRobot','StartRoomComputer','StartRoomBook']:
    a=unreal.load_asset('/Game/Subtitles/DA_'+name)
    rows[name]={'id':str(a.segment_id),'lines':[str(x.text) for x in a.lines],'tags':{str(k):str(v) for k,v in EA.get_metadata_tag_values(a).items()}}
for name,path in [('blood','/Game/BloodCirculation/Materials/MaterialInstance/Cell/MI_RedBloodCell'),('vessel','/Game/BloodCirculation/Materials/MaterialInstance/BloodVessel/MI_Vein')]:
    a=unreal.load_asset(path);chain=[]
    while a:
        entry={'path':a.get_path_name(),'class':a.get_class().get_name()}
        if isinstance(a,unreal.Material):
            entry.update(shading=str(a.get_editor_property('shading_model')),nodes=[])
            for n in ML.get_material_expressions(a):
                entry['nodes'].append({'name':n.get_name(),'class':n.get_class().get_name(),'description':n.get_editor_property('desc')})
            chain.append(entry);break
        for p in ['scalar_parameter_values','vector_parameter_values','texture_parameter_values','base_property_overrides']:
            entry[p]=str(a.get_editor_property(p))
        chain.append(entry);a=a.get_editor_property('parent')
    rows[name]=chain
out=Path(unreal.Paths.project_dir())/'Saved/Validation/TraversalFeedback/opening_blood_diagnosis.json'
out.write_text(json.dumps(rows,ensure_ascii=False,indent=2),encoding='utf-8')
unreal.log('OPENING_BLOOD_DIAGNOSIS_SAVED')
