"""Read-only diagnosis of pickup subtitles, terrain materials and heaven bounds."""
import json
from pathlib import Path
import unreal

root=Path(unreal.Paths.project_dir())
out=root/'Saved/Validation/TraversalFeedback'
out.mkdir(parents=True,exist_ok=True)
api=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
actors=list(api.get_all_level_actors())
report={'subtitles':[],'heaven':[],'terrain':[],'ui':[],'instancing_materials':[],'mineral_meshes':[]}
for p in unreal.EditorAssetLibrary.list_assets('/Game/Subtitles/Collectibles'):
    s=unreal.load_asset(p)
    if isinstance(s,unreal.SubtitleSegment):
        report['subtitles'].append({'path':p,'id':str(s.segment_id),'lines':[{'text':str(l.text),'audio':l.audio.get_path_name() if l.audio else None,'duration':l.audio.duration if l.audio else None} for l in s.lines]})
for a in actors:
    label=a.get_actor_label()
    if 'Heaven' in label:
        c,e=a.get_actor_bounds(False)
        report['heaven'].append({'label':label,'class':a.get_class().get_name(),'position':list(a.get_actor_location().to_tuple()),'center':list(c.to_tuple()),'extent':list(e.to_tuple())})
    if isinstance(a,unreal.DiggableRock):
        m=a.rock_material
        materials=[x.get_path_name() if x else None for x in a.rock_mesh.get_materials()]
        report['terrain'].append({'label':label,'material':m.get_path_name() if m else None,'slots':materials})
bp=unreal.load_asset('/Game/CollectionPrototype/UI/WBP_CollectionInspect')
if bp:
    report['collection_blueprint']=bp.get_path_name()
for path in ['/Game/UI/MiningGUI/WBP_AbilityG','/Game/UI/MiningGUI/WBP_MiningHUD']:
    unreal.load_asset(path)
for w in unreal.ObjectIterator(unreal.MiningGUIAbilityWidget):
    report['ui'].append({'object':w.get_path_name(),'normal':w.normal_texture.get_path_name() if w.normal_texture else None,'used':w.used_texture.get_path_name() if w.used_texture else None})
for base in ['/Game/MainGame/Materials','/Game/Campaign/Materials','/Game/Showcase/Treasure/Materials','/Game/Debris','/Game/CollectionPrototype/Materials']:
    for path in unreal.EditorAssetLibrary.list_assets(base):
        m=unreal.load_asset(path)
        if isinstance(m,unreal.Material) and m.material_domain==unreal.MaterialDomain.MD_SURFACE:
            report['instancing_materials'].append({'path':path,'instanced':m.get_editor_property('used_with_instanced_static_meshes'),'nanite':m.get_editor_property('used_with_nanite')})
for path in unreal.EditorAssetLibrary.list_assets('/Game/Showcase/Treasure/Meshes'):
    m=unreal.load_asset(path)
    if isinstance(m,unreal.StaticMesh):report['mineral_meshes'].append({'path':path,'slots':[x.material_interface.get_path_name() if x.material_interface else None for x in m.static_materials]})
textures=unreal.EditorAssetLibrary.list_assets('/Game/UI/MiningGUI/Art')
report['ability_art']=[p for p in textures if any(k in p.lower() for k in ['bomb','ability','skill','g_'])]
(out/'diagnosis.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
unreal.log('TRAVERSAL_DIAGNOSIS_COMPLETE')
