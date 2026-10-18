"""Read the authored geometry before changing the nine gameplay feedback items."""
import unreal,json,traceback
from pathlib import Path
ROOT=Path(unreal.Paths.project_dir()).resolve();OUT=ROOT/'Saved/Validation/GameplayFeedback'
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
lib=unreal.MiningDesignerLibrary
def vec(v):return list(v.to_tuple())
def row(a):
    c,e=a.get_actor_bounds(False)
    d=dict(name=a.get_name(),label=a.get_actor_label(),type=a.get_class().get_name(),level=a.get_level().get_path_name(),location=vec(a.get_actor_location()),scale=vec(a.get_actor_scale3d()),bounds=[vec(c-e),vec(c+e)],tags=[str(t) for t in a.tags],components=[])
    for comp in a.get_components_by_class(unreal.StaticMeshComponent):
        mesh=comp.static_mesh
        d['components'].append(dict(name=comp.get_name(),mesh=mesh.get_path_name() if mesh else None,collision=str(comp.get_collision_enabled()),profile=str(comp.get_collision_profile_name()),materials=[m.get_path_name() if m else None for m in comp.get_materials()]))
    if isinstance(a,unreal.GeneratedMiningRock):
        source=lib.get_campaign_rock_source(a)
        pts=unreal.GeometryScript_List.convert_vector_list_to_array(unreal.GeometryScript_MeshQueries.get_all_vertex_positions(source,False)[1])
        pts=[unreal.MathLibrary.transform_location(a.get_actor_transform(),p).to_tuple() for p in pts]
        d['source_bounds']=[[min(p[i] for p in pts) for i in range(3)],[max(p[i] for p in pts) for i in range(3)]]
        d['triangles']=source.get_triangle_count()
        for k in ['bomb_only','surface_material','mining_material']:
            try:d[k]=str(a.get_editor_property(k))
            except Exception:pass
    if isinstance(a,unreal.DiggableRock):d['rock_size']=vec(a.get_editor_property('rock_size'));d['rock_material']=str(a.get_editor_property('rock_material'))
    return d
try:
    exec(compile((ROOT/'Scripts/create_gameplay_feedback_materials.py').read_text(encoding='utf-8'),'feedback_materials','exec'),{'__name__':'__main__'})
    assert levels.load_level('/Game/MainGame/Maps/Lvl_MainGame')
    aa=list(actors.get_all_level_actors())
    report={'main':[],'office':[],'museum':[]}
    for a in aa:
        if isinstance(a,unreal.DiggableRock) or a.actor_has_tag('BM_ParkourPlatform') or a.actor_has_tag('BM_MainGame_ParkourTerminalPlatform'):report['main'].append(row(a))
        if 'Lvl_PopupClose1' in a.get_level().get_path_name():report['office'].append(row(a))
        if any(t in a.get_actor_label() for t in ['Reward','Balance']) or isinstance(a,unreal.WenSiMirrorExhibit):report['museum'].append(row(a))
    (OUT/'inspection.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    unreal.log('GAMEPLAY_FEEDBACK_INSPECTED')
finally:unreal.SystemLibrary.quit_editor()
