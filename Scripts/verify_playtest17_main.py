"""PIE validation of real pickup, hold-to-skip and lamp recovery paths, plus visual checks."""
from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir());source=(root/'Scripts/capture_feedback11_main.py').read_text(encoding='utf8')
head=source[:source.index('def run():')].replace('Saved/Validation/Feedback11/Main','Saved/Validation/Playtest17/Main')
run='''def run():
 assert L.load_level('/Game/MainGame/Maps/Lvl_MainGame')
 fixture=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).spawn_actor_from_class(unreal.DiggableRock,unreal.Vector(0,0,50000))
 fixture.tags=['Playtest17CutFixture'];fixture.set_editor_property('rock_size',unreal.Vector(500,500,500));fixture.reset_rock()
 H.start_floating_preview()
 while not E.get_game_world():yield .2
 w=E.get_game_world();pc=unreal.GameplayStatics.get_player_controller(w,0);s['pc']=pc
 yield 8;H.resize_preview_viewport(pc,1280,720)
 opening=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.MiningOpening)[0]
 H.capture_preview(pc,str(OUT/'01_opening.png'));r['opening_eye']=str(opening.get_editor_property('SeatEye'))
 opening.start_game()
 while not opening.finished:yield .2
 hall=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.MiningExhibitHall)[0];hall.set_actor_tick_enabled(False)
 pawn=pc.get_controlled_pawn();room=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.MiningStartRoom)[0]
 assert H.write_preview_state(room,'bHeadlampUnlocked','True')
 field=next(a for a in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.MiningCollectionField) if a.use_authored_placements)
 assert H.write_preview_state(field,'Inventory','(1,1,1,1,1,1,1,1,1,1,1,1)')
 field.toggle_catalogue();yield .7;field.catalogue_widget.select_collection_item(2);yield .3
 H.capture_preview(pc,str(OUT/'02_collection_centered.png'));field.catalogue_widget.close_collection()
 # Verify current copper surfaces in the main campaign, not only source transforms.
 import json
 data=json.loads((root/'Saved/Validation/Playtest17/copper.json').read_text(encoding='utf8'))
 for move in data['moves']:
  surface=unreal.Vector(*move['after']['surface'][0]);normal=move['after']['surface'][1]
  pose(surface+unreal.Vector(-150,240,130),surface);yield 1
  H.capture_preview(pc,str(OUT/('03_copper_'+str(move['index'])+'.png')))
 feathers=unreal.GameplayStatics.get_all_actors_with_tag(w,'CollectionFeather')
 assert feathers
 feather=feathers[0];target=feather.get_actor_location();pose(target+unreal.Vector(-150,0,30),target);yield .5
 H.capture_preview(pc,str(OUT/'04_white_feather.png'))
 # Isolated cut fixture, retaining the real campaign field and collection input.
 loc=unreal.Vector(0,0,50000)
 rock=unreal.GameplayStatics.get_all_actors_with_tag(w,'Playtest17CutFixture')[0]
 rock.set_editor_property('rock_size',unreal.Vector(500,500,500));rock.reset_rock()
 item=H.spawn_preview_collectible(field,2,loc);assert item
 H.write_preview_state(item,'HostRock',rock.get_path_name());assert H.write_preview_state(item,'bReleased','False');assert H.write_preview_state(item,'Exposure','0')
 item.body.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
 pose(loc+unreal.Vector(-340,0,0),loc);yield .4
 assert field.target!=item,'Cannot collect through intact soil'
 r['exposures']=[]
 for radius in range(200,341,10):
  rock.mine_sphere(loc+unreal.Vector(-220,0,0),radius);yield .3
  r['exposures'].append([radius,item.exposure,item.released])
  if .5<item.exposure<.95 and not item.released:break
 assert .5<item.exposure<.95 and not item.released,r['exposures']
 assert not item.body.is_simulating_physics()
 pose(loc+unreal.Vector(-340,0,0),loc);yield .3
 assert field.target==item,'Exposed model supports the normal pickup trace'
 H.capture_preview(pc,str(OUT/'05_partial_pickup.png'))
 old=field.inventory[2];field.collect_target();assert item.collected and field.inventory[2]==old+1
 r['partial_pickup']=True
 # Reproduce a late encounter release that turns the miner lamp off in mine 3.
 assert H.prepare_collection_preview_area(pc,4,False)
 pawn.set_actor_location(unreal.Vector(34000,11750,20000),False,True);hall.set_actor_tick_enabled(True);yield .4
 lights=[c for c in pawn.get_components_by_class(unreal.LightComponent) if c.get_name() in ['MiningLight','MiningFillLight']]
 for c in lights:c.set_visibility(False);c.set_intensity(0)
 yield .3
 assert all(c.is_visible() and c.intensity>0 for c in lights),'Third mine restores lamps after stale policy state'
 r['lamp_recovery']=True
 # A short P press must not skip; releasing it resets the three-second timer.
 assert H.prepare_collection_preview_area(pc,5,False);yield 1
 puzzle=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.WenSiMirrorPuzzle)[0]
 H.send_preview_key(pc,'P',True);yield 1;H.send_preview_key(pc,'P',False);yield .2
 assert H.read_preview_state(puzzle,'bRewardUnlocked')=='False'
 H.send_preview_key(pc,'P',True);yield 2
 assert H.read_preview_state(puzzle,'bRewardUnlocked')=='False'
 yield 1.2;H.send_preview_key(pc,'P',False)
 assert H.read_preview_state(puzzle,'bRewardUnlocked')=='True'
 H.capture_preview(pc,str(OUT/'06_museum_p_skip.png'));r['hold_p_three_seconds']=True
 r['passed']=True;finish()
'''
exec(compile(head+run+source[source.index('def finish('):],'playtest17_main','exec'))
