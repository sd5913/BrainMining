import unreal, json
from pathlib import Path
e=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
w=e.get_game_world() or e.get_editor_world()
rows=[]
for a in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.Actor):
    cl=a.get_class().get_name()
    meshes=[]
    for c in a.get_components_by_class(unreal.StaticMeshComponent):
        if c.static_mesh:
            mp=c.static_mesh.get_path_name()
            if any(s in (mp+' '+a.get_actor_label()).lower() for s in ['tele','tv','crt']):
                meshes.append(dict(mesh=mp,component=c.get_path_name(),transform=str(c.get_world_transform()),materials=[str(c.get_material(i)) for i in range(c.get_num_materials())]))
    if meshes or cl in ['MiningRoomEscape','MiningGiantRoomEntrance','CardRoomStage','WorldSettings'] or 'Controller' in cl:
        props={}
        for p in ['card_stage','entrance','area_managed','build_study_layout','phase','water','water_z','television_screen','default_game_mode','spell_presentation_scale']:
            try: props[p]=str(a.get_editor_property(p))
            except Exception: pass
        rows.append(dict(name=a.get_name(),label=a.get_actor_label(),cls=cl,transform=str(a.get_actor_transform()),props=props,meshes=meshes,tick=a.is_actor_tick_enabled()))
p=Path(unreal.Paths.project_dir())/'Saved/Validation/P2Repair'
p.mkdir(parents=True,exist_ok=True)
(p/('runtime.json' if e.get_game_world() else 'editor.json')).write_text(json.dumps(rows,indent=2),encoding='utf8')
