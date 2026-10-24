import unreal,json
from pathlib import Path
root=Path(unreal.Paths.project_dir());out=root/'Saved/Validation/Collection23';out.mkdir(parents=True,exist_ok=True)
assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level('/Game/MainGame/Maps/Lvl_MainGame')
actors=list(unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors());r=[]
for region in actors:
 if not isinstance(region,unreal.MiningExternalRegion):continue
 row=dict(name=region.get_actor_label(),tags=[str(t) for t in region.tags],layers=[])
 for a in [region.generated_rock,region.generated_shell]+list(region.generated_guide_layers):
  if not a:continue
  center,extent=a.get_actor_bounds(False)
  cs=a.get_components_by_class(unreal.PrimitiveComponent)
  row['layers'].append(dict(name=a.get_actor_label(),path=a.get_path_name(),bounds=[list(center.to_tuple()),list(extent.to_tuple())],tags=[str(t) for t in a.tags],materials=[c.get_material(0).get_path_name() if c.get_material(0) else '' for c in cs]))
 r.append(row)
(out/'scene-audit.json').write_text(json.dumps(r,ensure_ascii=False,indent=2),encoding='utf8');unreal.SystemLibrary.quit_editor()
