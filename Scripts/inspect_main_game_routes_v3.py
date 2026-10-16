"""Save a complete recovery point before changing the regional route identities."""
import unreal,json,sys,shutil,hashlib,time
from pathlib import Path
ROOT=Path(unreal.Paths.project_dir()).resolve();OUT=ROOT/'Saved/Validation/MainGameRoutesV3';OUT.mkdir(exist_ok=True)
sys.path.insert(0,str(ROOT/'Scripts'));import campaign_minerals_api as api
ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem);levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
assert not ed.get_game_world();assert ed.get_editor_world().get_path_name()=='/Game/MainGame/Maps/Lvl_MainGame.Lvl_MainGame';assert levels.set_current_level_by_name('Lvl_MainGame');assert levels.save_current_level()
target=OUT/'before.json';assert not target.exists(),'Recovery snapshot already exists'
backup=ROOT/'Saved/Backups'/('MainGame_RegionalRoutesV3_'+time.strftime('%Y%m%d-%H%M%S'));backup.mkdir()
shutil.copy2(ROOT/'Content/MainGame/Maps/Lvl_MainGame.umap',backup/'Lvl_MainGame.umap');shutil.copytree(ROOT/'Content/MainGame/Materials/DirtTransition',backup/'DirtTransition')
for name in ['MainGameRooms.json','MainGameRoutes.json','MainGameTuning.json','DefaultGame.ini']:shutil.copy2(ROOT/'Config'/name,backup/name)
all_actors=list(actors.get_all_level_actors());hall=next(a for a in all_actors if isinstance(a,unreal.MiningExhibitHall));valid=api.validate(api.get_field(hall),hall);assert valid['ok'];cfg=json.loads((ROOT/'Config/MainGameRooms.json').read_text(encoding='utf-8'))
report={'backup':str(backup),'regions':[],'areas':[],'source_hashes':{},'minerals':valid,'rewards':[{k:p[k] for k in ['id','area','value','guide']} for p in api._layout(api.get_field(hall))]}
for i in range(4):
 r=next(a for a in all_actors if isinstance(a,unreal.MiningExternalRegion) and a.actor_has_tag('BM_Campaign_MainGame%02d'%(i+1)));report['regions'].append(dict(actor=r.get_path_name(),origin=list(r.get_actor_location().to_tuple()),size=list(r.rock_size.to_tuple()),cutter_origin=list((r.get_actor_location()+r.cutters[0].local_transform.translation).to_tuple()),rock=r.generated_rock.get_path_name(),triangles=r.generated_rock.triangle_count,normal_hardness=r.rock_hardness,soil_hardness=r.guide_hardness,guides=len(r.generated_guide_layers)))
for a in hall.areas:report['areas'].append(dict(tag=str(a.tag),origin=list(a.origin.to_tuple()),entry=list(a.entry.translation.to_tuple()),exit=list(a.exit.to_tuple()),route_start=list(a.map_route[0].to_tuple()) if a.map_route else None,route_end=list(a.map_route[-1].to_tuple()) if a.map_route else None))
for src in [r['source'] for r in cfg['rooms']]+['/Game/StartRoom/Maps/Lvl_StartRoom','/Game/Campaign/Maps/Lvl_FullJourney']:report['source_hashes'][src]=hashlib.sha256((ROOT/'Content'/(src[6:]+'.umap')).read_bytes()).hexdigest()
target.write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8');unreal.log('REGIONAL_ROUTES_BACKUP_READY '+str(backup))
