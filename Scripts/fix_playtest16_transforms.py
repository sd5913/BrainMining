import unreal
L=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);A=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
assert L.load_level('/Game/StartRoom/Maps/Lvl_StartRoom')
p=next(a for a in A.get_all_level_actors() if a.actor_has_tag('StartRoomView_Pear'));p.set_actor_scale3d(unreal.Vector(1.45,1.45,1.45))
assert L.save_current_level()
assert L.load_level('/Game/MainGame/Maps/Lvl_MainGame')
for a in A.get_all_level_actors():
 if a.get_actor_label().startswith('MAIN_GAME_CrystalMotes_'):a.tags=['BM_Campaign_MainGame02','BM_RoutesV5']
assert L.save_current_level()
unreal.SystemLibrary.quit_editor()
