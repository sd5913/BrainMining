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
 start_eye=opening.get_editor_property('SeatEye');path=[]
 while not opening.finished:
  eye=pc.player_camera_manager.get_camera_location();path.append([eye.x,eye.y,eye.z]);yield .1
 r['opening_path']=path
 assert ((pc.get_controlled_pawn().get_actor_location()-opening.get_editor_property('StandingLocation'))*unreal.Vector(1,1,0)).length()<10,'Opening ends at the restored landing beside the bed'
 hall=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.MiningExhibitHall)[0];hall.set_actor_tick_enabled(False)
 pawn=pc.get_controlled_pawn();room=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.MiningStartRoom)[0]
 assert H.write_preview_state(room,'bHeadlampUnlocked','True')
 for c in pawn.get_components_by_class(unreal.LightComponent):
  if c.get_name() in ['MiningLight','MiningFillLight']:c.set_visibility(True);c.set_intensity(1.4 if c.get_name()=='MiningLight' else .22)
 field=next(a for a in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.MiningCollectionField) if a.use_authored_placements)
 assert H.write_preview_state(field,'Inventory','(1,1,1,1,1,1,1,1,1,1,1,1)')
 field.toggle_catalogue();yield .7;field.catalogue_widget.select_collection_item(2);yield .3
 H.capture_preview(pc,str(OUT/'02_collection_centered.png'))
 button=unreal.CollectionUMGEditorLibrary.find_inspect_control(field.catalogue_widget,'ItemButton1')
 pc.set_mouse_location(350,620);yield .5
 H.capture_preview(pc,str(OUT/'02b_collection_hover.png'));r['collection_button_hovered']=button.is_hovered()
 field.catalogue_widget.select_collection_item(10);yield .4
 H.capture_preview(pc,str(OUT/'02d_firefly_glow.png'))
 preview=field.get_editor_property('Preview');halo=preview.get_editor_property('Model').get_material(10)
 assert halo.get_scalar_parameter_value('GlowGain')>2,'Inspection boosts the firefly halo'
 field.catalogue_widget.close_collection()
 gi=unreal.GameplayStatics.get_game_instance(w)
 session=next(o for o in unreal.ObjectIterator(unreal.MiningExperienceSubsystem) if o.get_outer()==gi)
 assert session.toggle_pause_menu();yield .4;H.capture_preview(pc,str(OUT/'02c_pause_focus.png'));session.toggle_pause_menu()
 # Verify current copper surfaces in the main campaign, not only source transforms.
 import json
 data=json.loads((root/'Saved/Validation/Playtest17/copper.json').read_text(encoding='utf8'))
 for move in data['moves']:
  surface=unreal.Vector(*move['after']['surface'][0]);normal=move['after']['surface'][1]
  pose(surface+unreal.Vector(-150,240,130),surface);yield 1
  H.capture_preview(pc,str(OUT/('03_copper_'+str(move['index'])+'.png')))
 feathers=unreal.GameplayStatics.get_all_actors_with_tag(w,'CollectionFeather')
 assert len(feathers)>=21,'The bird has denser feather clues'
 feather=feathers[0];target=feather.get_actor_location();pose(target+unreal.Vector(-150,0,30),target);yield .5
 H.capture_preview(pc,str(OUT/'04_white_feather.png'))
 # Isolated cut fixture, retaining the real campaign field and collection input.
 loc=unreal.Vector(0,0,50000)
 # At 70%, the object must pop out before becoming a pickup target.
 rock=unreal.GameplayStatics.get_all_actors_with_tag(w,'Playtest17CutFixture')[0]
 rock.set_editor_property('rock_size',unreal.Vector(500,500,500));rock.reset_rock()
 item=H.spawn_preview_collectible(field,2,loc);assert item
 H.write_preview_state(item,'HostRock',rock.get_path_name());assert H.write_preview_state(item,'bReleased','False');assert H.write_preview_state(item,'Exposure','0')
 item.get_editor_property('Body').set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
 pose(loc+unreal.Vector(-340,0,0),loc);yield .4
 assert field.target!=item,'Cannot collect through intact soil'
 r['exposures']=[]
 for radius in range(220,231):
  rock.mine_sphere(loc+unreal.Vector(-220,0,0),radius);yield .3
  r['exposures'].append([radius,item.exposure,item.released])
  if item.exposure<.7:assert field.target!=item,'Below 70% must not be collectible'
  if item.released:break
 assert .7<=item.exposure<.95,r['exposures']
 assert item.released,'70% exposure must release the object first'
 item.get_editor_property('Body').set_simulate_physics(False)
 pose(loc+unreal.Vector(-340,0,0),item.get_actor_location());yield .3
 assert field.target==item,'Exposed model supports the normal pickup trace'
 hud=pc.get_editor_property('InteractionHUD')
 assert hud.interaction_action=='化学试剂','Released collectible uses the shared name and E-key prompt'
 H.capture_preview(pc,str(OUT/'05_partial_pickup.png'))
 old=field.inventory[2];field.collect_target();assert item.collected and field.inventory[2]==old+1
 r['released_pickup_at_70_percent']=True
 # The real office session owns its cursor, click feedback and restoration.
 assert H.prepare_collection_preview_area(pc,1,False);hall.set_actor_tick_enabled(False);yield .4
 office=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.PopupCloseStage)[0]
 device=next(d for d in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.PopupOfficeDevice) if d.get_editor_property('DeviceIndex')==0)
 frame=device.get_actor_transform()
 pose(unreal.MathLibrary.transform_location(frame,unreal.Vector(-190,0,160)),unreal.MathLibrary.transform_location(frame,unreal.Vector(0,0,124)));yield .5
 H.send_preview_key(pc,'E',True);H.send_preview_key(pc,'E',False);yield 10
 status=office.validate_office();assert status.startswith('PASS'),status
 r['office_cursor_click']=status
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
 assert 'reward=0' in puzzle.mirror_status()
 assert hall.get_museum_skip_progress()==0,'Releasing P clears the skip bar'
 H.send_preview_key(pc,'P',True);yield 2
 assert 'reward=0' in puzzle.mirror_status()
 assert .5<hall.get_museum_skip_progress()<.9,'The skip bar tracks the same three-second hold timer'
 H.capture_preview(pc,str(OUT/'06a_museum_skip_progress.png'))
 yield 1.2;H.send_preview_key(pc,'P',False)
 assert 'reward=1' in puzzle.mirror_status()
 H.capture_preview(pc,str(OUT/'06_museum_p_skip.png'));r['hold_p_three_seconds']=True
 r['passed']=True;finish()
'''
exec(compile(head+run+source[source.index('def finish('):],'playtest17_main','exec'))
