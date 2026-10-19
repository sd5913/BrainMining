import unreal
unreal.EditorLoadingAndSavingUtils.load_map('/Game/EndingEmergence/Maps/Lvl_EndingEmergence_Demo1')
d=next(a for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors() if isinstance(a,unreal.EndingEmergence))
print('ENDING_PROP_NAMES',[x for x in dir(d) if any(k in x.lower() for k in ('scen','build','city','crowd'))])

