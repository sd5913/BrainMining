import unreal,json
from pathlib import Path
E=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
A=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
assert not E.get_game_world()
es=[a for a in A.get_all_level_actors() if isinstance(a,unreal.MiningRoomEscape)]
assert len(es)==1
e=es[0];entry=e.get_editor_property('computer_entry')
pos=entry+unreal.Vector(-2750,-650,280)
E.set_level_viewport_camera_info(pos,unreal.MathLibrary.find_look_at_rotation(pos,entry+unreal.Vector(0,100,250)))
A.clear_actor_selection_set()
assert unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True,True)
out=Path(unreal.Paths.project_dir())/'Saved/Validation/IntroP2Office'
parts=e.get_components_by_class(unreal.ActorComponent)
(out/'tv_editor_reloaded.json').write_text(json.dumps(dict(actor=e.get_path_name(),preview_parts=sum('TelevisionEditorPreview' in [str(t) for t in c.component_tags] for c in parts)),indent=2),encoding='utf8')
