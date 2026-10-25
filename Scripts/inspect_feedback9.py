import unreal,json
from pathlib import Path
root=Path(unreal.Paths.project_dir());out=root/'Saved/Validation/Feedback9';out.mkdir(parents=True,exist_ok=True)
L=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);A=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
assert L.load_level('/Game/MainGame/Maps/Lvl_MainGame')
r={'minerals':[],'collections':[],'walls':[],'subtitles':[],'drill':[]}
for a in A.get_all_level_actors():
 if isinstance(a,unreal.MiningTreasureField):
  for i,p in enumerate(a.get_editor_property('authored_placements')):
   if p.surface.translation.x < -25000:r['minerals'].append(dict(index=i,id=str(p.treasure_id),surface=p.surface.translation.to_tuple(),core=p.transform.translation.to_tuple(),normal=unreal.MathLibrary.get_forward_vector(p.surface.rotation.rotator()).to_tuple()))
 if isinstance(a,unreal.MiningCollectionField):
  for i,p in enumerate(a.get_editor_property('authored_placements')):
   r['collections'].append(dict(index=i,id=p.item_id,pos=p.transform.translation.to_tuple(),rotation=p.transform.rotation.rotator().to_tuple(),clue=p.clue_transform.translation.to_tuple(),normal=unreal.MathLibrary.get_forward_vector(p.clue_transform.rotation.rotator()).to_tuple(),rock=p.host_rock.get_path_name() if p.host_rock else None))
 if a.actor_has_tag('BM_BombOnly') or a.actor_has_tag('BM_Polish32_BombSign'):
  d=dict(name=a.get_name(),label=a.get_actor_label(),pos=a.get_actor_location().to_tuple(),rot=a.get_actor_rotation().to_tuple())
  d['decals']=[dict(pos=c.get_world_location().to_tuple(),rot=c.get_world_rotation().to_tuple(),size=c.get_editor_property('decal_size').to_tuple()) for c in a.get_components_by_class(unreal.DecalComponent)]
  r['walls'].append(d)
for i in range(23):
 path=f'/Game/Subtitles/Collectibles/DA_CollectiblePickup_{i:02d}';a=unreal.load_asset(path)
 r['subtitles'].append(dict(id=i,path=path,lines=[str(x.text) for x in a.get_editor_property('lines')]))
for path in ['/Game/MainGame/Blueprints/BP_MainGameCharacter','/Game/Showcase/Blueprints/BP_SharedMiningCharacter']:
 c=unreal.get_default_object(unreal.EditorAssetLibrary.load_blueprint_class(path)).get_component_by_class(unreal.MiningComponent)
 r['drill'].append(dict(path=path,capacity=c.get_editor_property('max_drill_energy'),energy=c.get_editor_property('drill_energy'),recovery=c.get_editor_property('drill_recovery_rate')))
(out/'inspection.json').write_text(json.dumps(r,ensure_ascii=False,indent=2),encoding='utf8')
unreal.SystemLibrary.quit_editor()
