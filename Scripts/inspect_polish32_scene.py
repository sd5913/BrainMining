import unreal,json
from pathlib import Path
out=Path(unreal.Paths.project_dir())/'Saved/Validation/Polish32';out.mkdir(parents=True,exist_ok=True)
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);api=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
assert levels.load_level('/Game/MainGame/Maps/Lvl_MainGame')
r={'walls':[],'collections':[],'heaven':[]}
for a in api.get_all_level_actors():
 if a.actor_has_tag('BM_BombOnly'):
  r['walls'].append(dict(path=a.get_path_name(),label=a.get_actor_label(),transform=a.get_actor_transform().export_text(),bounds=[list(v.to_tuple()) for v in a.get_actor_bounds(False)],tags=[str(t) for t in a.tags]))
 if isinstance(a,unreal.MiningCollectionField):
  for p in a.authored_placements:r['collections'].append(dict(id=p.item_id,clue=p.clue_transform.export_text(),body=p.transform.export_text(),approach=p.approach_transform.export_text(),host=p.host_rock.get_path_name() if p.host_rock else None))
 if 'heaven' in a.get_actor_label().lower():r['heaven'].append(dict(path=a.get_path_name(),label=a.get_actor_label(),transform=a.get_actor_transform().export_text(),bounds=[list(v.to_tuple()) for v in a.get_actor_bounds(False)]))
system=unreal.load_asset('/Game/Passages/Echo/ArtP2/VFX/NS_P2_Poison')
r['poison_stack']=unreal.MiningNiagaraEditorLibrary.inspect_stack(system)
m=unreal.load_asset('/Game/MainGame/Materials/M_BuriedMineralGlow')
r['glow_custom']=[o.get_editor_property('code') for o in unreal.ObjectIterator(unreal.MaterialExpressionCustom) if o.get_outer()==m]
(out/'scene-audit.json').write_text(json.dumps(r,ensure_ascii=False,indent=2),encoding='utf8')
unreal.SystemLibrary.quit_editor()
