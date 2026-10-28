"""Match the two terminal insulators to the surrounding gold apparatus."""
import unreal,json
from pathlib import Path
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert levels.load_level('/Game/WenSi/Maps/Lvl_WenSi_MirrorMuseum1')
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
gold=unreal.load_asset('/Game/WenSi/Museum/Materials/MI_MuseumRoughGold');assert gold
changed=[]
for a in actors.get_all_level_actors():
 if a.get_actor_label().startswith('ContactInsulator_'):
  a.modify()
  if not a.actor_has_tag("MuseumGoldContactBase"):a.tags=list(a.tags)+[unreal.Name("MuseumGoldContactBase")]
  for mesh in a.get_components_by_class(unreal.StaticMeshComponent):mesh.set_material(0,gold)
  changed.append(a.get_actor_label())
assert len(changed)==2,changed
assert levels.save_current_level()
out=Path(unreal.Paths.project_dir())/'Saved/Validation/Feedback28';out.mkdir(parents=True,exist_ok=True)
(out/'gold-bases.json').write_text(json.dumps(dict(changed=changed,material=gold.get_path_name()),indent=2),encoding='utf8')
unreal.SystemLibrary.quit_editor()
