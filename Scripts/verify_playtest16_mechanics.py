from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir());base=(root/'Scripts/capture_feedback11_main.py').read_text(encoding='utf8');head=base[:base.index('def run():')].replace('Saved/Validation/Feedback11/Main','Saved/Validation/Playtest16/Mechanics')
run='''def run():
 assert L.load_level('/Game/MainGame/Maps/Lvl_MainGame');H.start_floating_preview()
 while not E.get_game_world():yield .2
 w=E.get_game_world();pc=unreal.GameplayStatics.get_player_controller(w,0);s['pc']=pc;yield 7
 H.resize_preview_viewport(pc,1280,720)
 opening=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.MiningOpening)[0];opening.start_game()
 while not opening.finished:yield .2
 pawn=pc.get_controlled_pawn();hall=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.MiningExhibitHall)[0];hall.set_actor_tick_enabled(False)
 room=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.MiningStartRoom)[0]
 pear=unreal.GameplayStatics.get_all_actors_with_tag(w,'StartRoomView_Pear')[0]
 target=pear.get_actor_bounds(False)[0];pose(unreal.Vector(-34580,230,11230),target);yield .6
 assert pear.static_mesh_component.get_editor_property('render_custom_depth');assert room.interact(pc);yield .3
 H.capture_preview(pc,str(OUT/'pear_final.png'))
 pick=next(a for a in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.MiningStarterPickup) if a.equipment==unreal.MiningStarterEquipment.PICKAXE)
 target=pick.get_actor_location();pose(target+unreal.Vector(-100,0,70),target);yield .3;assert room.interact(pc)
 target=opening.get_editor_property('door_title_location');pose(target-unreal.Vector(180,0,0),target);yield .1
 H.capture_preview(pc,str(OUT/'title_early.png'));yield 1
 H.capture_preview(pc,str(OUT/'title_middle.png'));yield 1.5
 H.capture_preview(pc,str(OUT/'title_full.png'))
 decal=opening.get_components_by_class(unreal.DecalComponent)[0];assert abs(decal.get_editor_property('fade_in_duration')-2.4)<.01
 # Use actual character movement across alternating excavation-like ridges.
 actors=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.StaticMeshActor)
 for i,(x,z,sx,sz) in enumerate([(300,49975,30,.5),(150,50018,.5,.36),(280,50027,.5,.54),(420,50015,.5,.30)]):
  a=actors[i];c=a.static_mesh_component;c.set_mobility(unreal.ComponentMobility.MOVABLE);c.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Cube'));c.set_collision_profile_name('BlockAll');c.set_visibility(True)
  a.set_actor_hidden_in_game(False);a.set_actor_enable_collision(True);a.set_actor_location(unreal.Vector(x,0,z),False,True);a.set_actor_rotation(unreal.Rotator(),False);a.set_actor_scale3d(unreal.Vector(sx,12,sz))
 pawn.set_actor_location(unreal.Vector(0,0,50100),False,True);move=pawn.get_component_by_class(unreal.CharacterMovementComponent);move.set_movement_mode(unreal.MovementMode.MOVE_WALKING);yield .6
 end=unreal.GameplayStatics.get_time_seconds(w)+3
 while unreal.GameplayStatics.get_time_seconds(w)<end:
  pawn.add_movement_input(unreal.Vector(1,0,0),1,True);yield 0
 move.stop_movement_immediately();r['walk_end']=pawn.get_actor_location().to_tuple();assert pawn.get_actor_location().x>550,'Feet caught on small ridges'
 marks=[c for c in unreal.ObjectIterator(unreal.DecalComponent) if unreal.SystemLibrary.is_valid(c) and c.component_has_tag('MiningFootprint')]
 assert len(marks)>3
 for m in marks:assert abs(m.get_editor_property('fade_start_delay')-3)<.01 and abs(m.get_editor_property('fade_duration')-12)<.01
 r['footprint_count']=len(marks);r['hold_seconds']=3;r['fade_seconds']=12
 pose(unreal.Vector(650,120,50300),unreal.Vector(360,0,50000));yield .1
 for name,delay in [('fresh',0),('fading',5),('late',6)]:
  yield delay;H.capture_preview(pc,str(OUT/('footprints_'+name+'.png')))
 yield 4
 assert not [c for c in unreal.ObjectIterator(unreal.DecalComponent) if unreal.SystemLibrary.is_valid(c) and c.component_has_tag('MiningFootprint')]
 # Live collection widget and its actual button styles.
 field=next(f for f in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.MiningCollectionField) if f.use_authored_placements)
 assert H.write_preview_state(field,'Inventory','(1,1,1,1,1,1,1,1,1,1,1,1)')
 H.send_preview_key(pc,'C',True);H.send_preview_key(pc,'C',False);yield 1
 assert field.catalogue_open
 widget=field.catalogue_widget;widget.select_collection_item(2);yield .5
 ui=unreal.CollectionUMGEditorLibrary
 assert str(ui.find_inspect_control(widget,'CollectionTitle').get_text())=='藏品'
 for name in ['PreviousButton','NextButton']:
  b=ui.find_inspect_control(widget,name);style=b.widget_style
  assert style.normal.tint_color.specified_color!=style.hovered.tint_color.specified_color and style.hovered.tint_color.specified_color!=style.pressed.tint_color.specified_color
 H.capture_preview(pc,str(OUT/'collection.png'));widget.close_collection()
 # Entry must trigger immediately even while looking away from the computers.
 H.prepare_collection_preview_area(pc,1,False);hall.set_actor_tick_enabled(False);yield .3
 office=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.PopupCloseStage)[0]
 pose(unreal.Vector(-16550,-400,6550),unreal.Vector(-16800,-400,6550));yield .5
 r['office_status']=office.get_exit_status()
 director=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.MainGameSubtitleDirector)[0]
 assert director.has_triggered('电脑弹窗玩法1'),'Office entrance narration should already have triggered'
 H.capture_preview(pc,str(OUT/'office_side_sealed.png'))
 # Exercise the normal timed reward and collection path.
 office.start_office_round();unreal.GameplayStatics.set_global_time_dilation(w,5)
 deadline=time.monotonic()+55
 while 'bomb_visible=1' not in office.get_exit_status():
  assert time.monotonic()<deadline;yield .3
 unreal.GameplayStatics.set_global_time_dilation(w,1)
 pickup=next(c for a in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.Actor) for c in a.get_components_by_class(unreal.StaticMeshComponent) if c.get_name()=='OfficeBombSkillPickup')
 target=pickup.get_world_location();pose(target+unreal.Vector(-120,-120,90),target);yield .4
 H.send_preview_key(pc,'E',True);H.send_preview_key(pc,'E',False);yield .6
 r['office_after_pickup']=office.get_exit_status();assert 'skill_unlocked=1' in r['office_after_pickup']
 pose(unreal.Vector(-15160,-12,6560),unreal.Vector(-14750,0,6545));yield .4
 H.capture_preview(pc,str(OUT/'office_bomb_exit_guide.png'))
 mining=pawn.get_component_by_class(unreal.MiningComponent);mining.unlock_drill()
 H.send_preview_key(pc,'V',True);H.send_preview_key(pc,'V',False);yield 1
 H.capture_preview(pc,str(OUT/'drill_first_person.png'))
 r['passed']=True;finish()
'''
exec(compile(head+run+base[base.index('def finish('):].replace('<160','<260'),'playtest16_mechanics_runtime','exec'))
