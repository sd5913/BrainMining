import unreal,json,shutil,hashlib
from pathlib import Path
from datetime import datetime
ROOT=Path(unreal.Paths.project_dir()).resolve();OUT=ROOT/'Saved/Validation/MainGameRoomEntrances';OUT.mkdir(exist_ok=True,parents=True)
ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem);levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert not ed.get_game_world();assert levels.save_current_level()
assert levels.load_level('/Game/MainGame/Maps/Lvl_MainGame')
backup=ROOT/'Saved/Backups'/('MainGame_RoomEntrances_'+datetime.now().strftime('%Y%m%d-%H%M%S'));backup.mkdir()
shutil.copy2(ROOT/'Content/MainGame/Maps/Lvl_MainGame.umap',backup/'Lvl_MainGame.umap');shutil.copy2(ROOT/'Config/MainGameRooms.json',backup/'MainGameRooms.json')
actors=list(unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors());hall=next(a for a in actors if isinstance(a,unreal.MiningExhibitHall));data={'backup':str(backup),'post_processes':[],'lights':[]}
for a in actors:
    if isinstance(a,unreal.PostProcessVolume):
        s=a.settings
        data['post_processes'].append({'label':a.get_actor_label(),'name':a.get_name(),'level':a.get_level().get_path_name(),'unbound':a.unbound,'enabled':a.enabled,'priority':a.priority,'blend_radius':a.blend_radius,'position':list(a.get_actor_location().to_tuple()),'settings':{k:str(s.get_editor_property(k)) for k in ['auto_exposure_method','auto_exposure_bias','auto_exposure_min_brightness','auto_exposure_max_brightness','auto_exposure_apply_physical_camera_exposure','override_auto_exposure_method','override_auto_exposure_bias','override_auto_exposure_min_brightness','override_auto_exposure_max_brightness','override_auto_exposure_apply_physical_camera_exposure']}})
data['geometry_api']={n:{k:str(getattr(getattr(unreal,n),k).__doc__) for k in dir(getattr(unreal,n)) if 'volume' in k} for n in dir(unreal) if n.startswith('GeometryScript_') and any('volume' in k for k in dir(getattr(unreal,n)))}
data['volume_bounds']={a.get_name():[list(v.to_tuple()) for v in a.get_actor_bounds(False)] for a in actors if isinstance(a,unreal.PostProcessVolume)}
data['source_hashes']={s:hashlib.sha256((ROOT/'Content'/(s[6:]+'.umap')).read_bytes()).hexdigest() for s in json.loads((ROOT/'Saved/Validation/MainGameRooms/before.json').read_text())['source_hashes']}
(OUT/'before.json').write_text(json.dumps(data,indent=2),encoding='utf-8');unreal.log('ROOM_ENTRANCES_INSPECTED '+str(backup))
