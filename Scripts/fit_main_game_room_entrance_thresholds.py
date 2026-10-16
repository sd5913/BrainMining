import unreal,json,sys
from pathlib import Path
ROOT=Path(unreal.Paths.project_dir()).resolve();sys.path.insert(0,str(ROOT/'Scripts'));import main_game_room_entrances_geometry as geometry
OUT=ROOT/'Saved/Validation/MainGameRoomEntrances';cfg=json.loads((ROOT/'Config/MainGameRooms.json').read_text());ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem);assert not ed.get_game_world()
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);all_actors=list(actors.get_all_level_actors());hall=next(a for a in all_actors if isinstance(a,unreal.MiningExhibitHall));areas=list(hall.areas)
for i,x in [(0,-16720),(2,-700),(4,46685)]:
    area=areas[i];exit=area.exit;exit.x=x;area.set_editor_property('exit',exit);route=list(area.map_route);route[-1]=exit;area.set_editor_property('map_route',route)
hall.set_editor_property('areas',areas,notify_mode=unreal.PropertyAccessChangeNotifyMode.NEVER)
for i in [1,5]:
    s=cfg['generated_connections'][i];s['plug_x']=[0,-14180,0,0,0,50400][i];a=next(a for a in all_actors if a.get_actor_label()=='LINK_'+s['id']+'_DiggableSoil')
    a.set_actor_location(unreal.Vector(s['plug_x'],s['a'][1],s['a'][2]+180),False,True);path=OUT/('LINK_'+s['id']+'_DiggableSoil.obj');geometry.write_obj(path,*geometry.soil(s));assert unreal.MiningDesignerLibrary.import_campaign_rock(a,str(path));a.reset_rock()
cfg['room_activation_x']=[-16720,-700,46685];(ROOT/'Config/MainGameRooms.json').write_text(json.dumps(cfg,ensure_ascii=False,indent=2),encoding='utf-8')
assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
unreal.log('ROOM_ENTRANCE_THRESHOLDS_SAVED')
