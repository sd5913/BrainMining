"""Close the temporary Python bridge after the completed, saved editor checks."""
import unreal
ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
assert not ed.get_game_world()
assert ed.get_editor_world().get_path_name()=='/Game/MainGame/Maps/Lvl_MainGame.Lvl_MainGame'
settings=unreal.get_default_object(unreal.load_class(None,'/Script/PythonScriptPlugin.PythonScriptPluginSettings'))
settings.set_editor_property('bRemoteExecution',False)
assert not settings.get_editor_property('bRemoteExecution')
unreal.log('LOWPOLY_EDITOR_READY_REMOTE_PYTHON_CLOSED')
