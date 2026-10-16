import unreal,json,traceback
from pathlib import Path
ROOT=Path(unreal.Paths.project_dir()).resolve();OUT=ROOT/'Saved/Validation/MainGameRoutesV5';OUT.mkdir(parents=True,exist_ok=True)
ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem);levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
report={}
try:
 assert levels.load_level('/Game/MainGame/Maps/Lvl_MainGame')
 aa=list(actors.get_all_level_actors());hall=next(a for a in aa if isinstance(a,unreal.MiningExhibitHall))
 report['areas']=[dict(tag=str(a.tag),entry=list(a.entry.translation.to_tuple()),exit=list(a.exit.to_tuple()),suppressed=[str(x) for x in a.suppressed_source_actors]) for a in hall.areas]
 report['museum']=[dict(path=a.get_path_name(),label=a.get_actor_label(),class_name=a.get_class().get_name(),position=list(a.get_actor_location().to_tuple()),bounds=[list(x.to_tuple()) for x in a.get_actor_bounds(False)],components=[dict(name=c.get_name(),material=str(c.get_material(0))) for c in a.get_components_by_class(unreal.StaticMeshComponent)]) for a in aa if 'Museum' in a.get_path_name() or 'Ceiling' in a.get_actor_label()]
 report['regions']=[dict(path=a.get_path_name(),origin=list(a.get_actor_location().to_tuple()),cutter_origin=list((a.get_actor_location()+a.cutters[0].local_transform.translation).to_tuple()),size=list(a.rock_size.to_tuple()),material=str(a.rock_material)) for a in aa if isinstance(a,unreal.MiningExternalRegion)]
 bp=unreal.load_asset('/Game/MainGame/Blueprints/BP_MainGameCharacter');cdo=unreal.get_default_object(bp.generated_class());bomb=cdo.get_component_by_class(unreal.MiningBombToolComponent);fx=cdo.get_component_by_class(unreal.MiningToolEffectsComponent)
 report['bomb']=dict(capacity=bomb.max_bombs,settings=str(bomb.settings));report['fx']={p:str(fx.get_editor_property(p)) for p in ['bEnabled','DebrisClass','DebrisMeshAssets','DebrisChunkCount']}
 report['lights']=[dict(path=a.get_path_name(),label=a.get_actor_label(),tags=[str(t) for t in a.tags]) for a in aa if isinstance(a,unreal.Light)]
except Exception:report['error']=traceback.format_exc()
finally:
 (OUT/'inspection.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8');unreal.SystemLibrary.execute_console_command(ed.get_editor_world(),'QUIT_EDITOR')
