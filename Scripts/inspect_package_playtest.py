"""Read-only scene and runtime-asset inventory for the packaged-playtest fixes."""
import json
from pathlib import Path
import unreal

root=Path(unreal.Paths.project_dir()).resolve()
out=root/'Saved/Validation/PackagePlaytest'
out.mkdir(parents=True,exist_ok=True)
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
api=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
def vec(v):return list(v.to_tuple())
def actor(a):
    c,e=a.get_actor_bounds(False)
    parts=[]
    for m in (a.get_components_by_class(unreal.StaticMeshComponent) if any(k in a.get_class().get_name() for k in ['Popup','Escape','CardRoom','Heaven']) else []):
        if not m.static_mesh:continue
        parts.append(dict(name=m.get_name(),mesh=m.static_mesh.get_path_name(),position=vec(m.get_world_location()),scale=vec(m.get_world_scale()),materials=[x.get_path_name() if x else None for x in m.get_materials()]))
    return dict(name=a.get_name(),label=a.get_actor_label(),kind=a.get_class().get_name(),level=a.get_level().get_path_name(),position=vec(a.get_actor_location()),rotation=vec(a.get_actor_rotation()),scale=vec(a.get_actor_scale3d()),bounds=[vec(c),vec(e)],tags=[str(t) for t in a.tags],parts=parts)
assert levels.load_level('/Game/MainGame/Maps/Lvl_MainGame')
allactors=api.get_all_level_actors()
data={'actors':[],'minerals':[],'collections':[],'areas':[]}
for a in allactors:
    label=a.get_actor_label().lower()
    if True:
        data['actors'].append(actor(a))
    if isinstance(a,unreal.MiningTreasureField):
        for i,p in enumerate(a.get_editor_property('authored_placements')):
            data['minerals'].append(dict(index=i,id=str(p.treasure_id),area=str(p.area_tag),position=vec(p.transform.translation),surface=vec(p.surface.translation)))
    if isinstance(a,unreal.MiningCollectionField):
        for p in a.get_editor_property('authored_placements'):
            data['collections'].append(dict(id=p.item_id,position=vec(p.transform.translation),scale=vec(p.transform.scale3d),clue=vec(p.clue_transform.translation),approach=vec(p.approach_transform.translation),host=p.host_rock.get_name() if p.host_rock else None))
    if isinstance(a,unreal.MiningExhibitHall):
        for area in a.areas:data['areas'].append(dict(tag=str(area.tag),entry=vec(area.entry.translation),route=[vec(p) for p in area.map_route]))
(out/'scene.json').write_text(json.dumps(data,ensure_ascii=False,indent=2),encoding='utf-8')
materials=[]
for path in unreal.EditorAssetLibrary.list_assets('/Game/EndingEmergence/Materials',recursive=True):
    m=unreal.load_asset(path)
    if isinstance(m,unreal.Material):
        materials.append(dict(path=path,instancing=m.get_editor_property('used_with_instanced_static_meshes'),domain=str(m.material_domain)))
(out/'ending-materials.json').write_text(json.dumps(materials,indent=2),encoding='utf-8')
unreal.SystemLibrary.quit_editor()
