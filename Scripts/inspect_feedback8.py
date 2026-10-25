import unreal,json
from pathlib import Path
root=Path(unreal.Paths.project_dir());out=root/'Saved/Validation/Feedback8';out.mkdir(parents=True,exist_ok=True)
L=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);A=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
assert L.load_level('/Game/MainGame/Maps/Lvl_MainGame')
r={'entrance':[],'minerals':[],'collections':[],'music':[]}
for a in A.get_all_level_actors():
 name=a.get_actor_label()
 if name.startswith('STARTROOM_') or a.get_name()=='DynamicMeshActor_6':
  comps=a.get_components_by_class(unreal.MeshComponent)
  r['entrance'].append(dict(name=name,path=a.get_path_name(),position=a.get_actor_location().to_tuple(),bounds=[v.to_tuple() for v in a.get_actor_bounds(False)],materials=[c.get_material(i).get_path_name() for c in comps for i in range(c.get_num_materials()) if c.get_material(i)]))
 if isinstance(a,unreal.MiningTreasureField):
  for i,p in enumerate(a.get_editor_property('authored_placements')):
   r['minerals'].append(dict(field=name,index=i,id=str(p.treasure_id),surface=p.surface.translation.to_tuple(),core=p.transform.translation.to_tuple()))
 if isinstance(a,unreal.MiningCollectionField):
  r['collections'].append(dict(name=name,placements=[dict(id=p.item_id,clue=p.clue_transform.translation.to_tuple()) for p in a.get_editor_property('authored_placements')]))
for path in ['/Game/Audio/Pilot/M_DreamyFlashback_Loop','/Game/Audio/Mix/SC_Music','/Game/Audio/Mix/SM_Player']:
 a=unreal.load_asset(path);d={'path':path}
 for k in ['priority','volume','looping','virtualization_mode','override_concurrency','concurrency_set','sound_class_object','properties','sound_class_effects','duration','fade_in_time','initial_delay']:
  try:d[k]=str(a.get_editor_property(k))
  except:pass
 r['music'].append(d)
(out/'inspection.json').write_text(json.dumps(r,ensure_ascii=False,indent=2),encoding='utf8')
unreal.SystemLibrary.quit_editor()
