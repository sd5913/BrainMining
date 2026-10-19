import unreal, json
MAP='/Game/CollectionPrototype/HeavenToilet/Maps/Lvl_HeavenToilet'
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
levels.load_level(MAP)
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
rows=[]
for a in actors:
    comps=a.get_components_by_class(unreal.NiagaraComponent)
    if comps:
        rows.append({'label':a.get_actor_label(),'path':a.get_path_name(),'actor_hidden':a.is_hidden_ed(),'scale':str(a.get_actor_scale3d()),'components':[{'name':c.get_name(),'asset':c.get_editor_property('asset').get_path_name() if c.get_editor_property('asset') else None,'auto':c.get_editor_property('auto_activate'),'visible':c.is_visible(),'active':c.is_active()} for c in comps]})
print('HEAVEN_NIAGARA_ACTORS',json.dumps(rows,ensure_ascii=False))
