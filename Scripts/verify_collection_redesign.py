"""Validate the production UMG catalogue, paused animation, paging and editable geometry."""
import json,time,traceback
from pathlib import Path
import unreal

root=Path(unreal.Paths.project_dir()).resolve();out=root/'Saved/Validation/CollectionRedesign'
out.mkdir(parents=True,exist_ok=True)
ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
helper=unreal.MiningGUIEditorLibrary;ui=unreal.CollectionUMGEditorLibrary
state={'phase':'start','next':0,'start':time.monotonic(),'busy':False}
report={'passed':False,'checks':[]}
def check(ok,label):
    assert ok,label
    report['checks'].append(label)
def control(name):return ui.find_inspect_control(state['widget'],name)
def shot(name):check(helper.capture_preview(state['pc'],str(out/(name+'.png'))),'Capture '+name)
def click(name):
    button=control(name)
    check(helper.simulate_preview_button(button,True),'Hit test '+name)
    check(helper.simulate_preview_button(button,False),'Click '+name)
def key(name):
    helper.send_preview_focused_key(state['pc'],name,True)
    helper.send_preview_focused_key(state['pc'],name,False)
def finish(error=None):
    report.update(passed=error is None,error=error)
    (out/'validation.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    levels.editor_request_end_play();state.update(phase='done',next=time.monotonic()+2)
def tick(dt):
    if state['busy'] or time.monotonic()<state['next']:return
    state['busy']=True
    try:
        now=time.monotonic();phase=state['phase']
        if phase=='done':
            if not ed.get_game_world():
                unreal.unregister_slate_post_tick_callback(handle)
                levels.load_level('/Game/MainGame/Maps/Lvl_MainGame')
                unreal.get_editor_subsystem(unreal.AssetEditorSubsystem).open_editor_for_assets([unreal.load_asset('/Game/CollectionPrototype/UI/WBP_CollectionInspect')])
                unreal.EditorPythonScripting.set_keep_python_script_alive(False)
            return
        assert now-state['start']<180,'Collection UI validation timed out'
        if phase=='start':
            assert levels.load_level('/Game/EndingEmergence/Maps/Lvl_EndingEmergence_Demo')
            helper.start_floating_preview();state.update(phase='wait',next=now+6);return
        if phase=='main_start':
            if ed.get_game_world():return
            assert levels.load_level('/Game/MainGame/Maps/Lvl_MainGame')
            helper.start_floating_preview();state.update(phase='main_wait',next=now+6);return
        world=ed.get_game_world()
        if not world:return
        if phase=='main_wait':
            pc=unreal.GameplayStatics.get_player_controller(world,0)
            if not pc or not pc.get_controlled_pawn():return
            openings=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.MiningOpening)
            for opening in openings:
                if not opening.finished:
                    opening.start_game();state['next']=now+.5;return
            fields=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.MiningCollectionField)
            field=next((f for f in fields if f.use_authored_placements and f.catalogue_widget),None)
            if not field:return
            field.toggle_catalogue()
            check(field.catalogue_open,'MainGame authored collection opens the new UMG')
            state.update(pc=pc,field=field,widget=field.catalogue_widget,phase='main_open',next=now+.8);return
        if phase=='main_open':
            shot('04_main_game')
            click('CloseButton');check(not state['field'].catalogue_open,'MainGame return button closes the new UI')
            finish();return
        if phase=='wait':
            pc=unreal.GameplayStatics.get_player_controller(world,0)
            if not pc or not pc.get_controlled_pawn() or unreal.GameplayStatics.get_time_seconds(world)<4:return
            ending=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.EndingEmergence)[0]
            ending.set_actor_tick_enabled(False)
            check(helper.prepare_ending_credits_preview(pc,[0,1,1,0,0,1,0,0,0,1,0,0]),'Seed shared campaign inventory')
            instance=unreal.GameplayStatics.get_game_instance(world)
            menu=next(x for x in unreal.ObjectIterator(unreal.MiningExperienceSubsystem) if x.get_outer()==instance)
            check(menu.toggle_pause_menu(),'Ending pause menu opens')
            menu.open_pause_collection()
            field=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.MiningCollectionField)[0]
            widget=field.catalogue_widget
            check(widget is not None and widget.get_class().get_name()=='WBP_CollectionInspect_C','Runtime uses the editable Widget Blueprint')
            state.update(pc=pc,field=field,widget=widget,world=world)
            widget.select_collection_item(2)
            widget.set_editor_property('panel_slide_duration',.8)
            check(control('InfoPanel').get_editor_property('render_transform').translation.x>500,'Info panel starts outside the screen')
            state.update(phase='sliding',next=now+.18);return
        if phase=='sliding':
            offset=control('InfoPanel').get_editor_property('render_transform').translation.x
            check(0<offset<600,'Panel moves left while the game is paused')
            check(unreal.GameplayStatics.is_game_paused(world),'World stays paused during slide')
            shot('01_slide');state.update(phase='owned',next=now+1);return
        if phase=='owned':
            check(abs(control('InfoPanel').get_editor_property('render_transform').translation.x)<.1,'Panel settles at its designer position')
            check(abs(control('SceneBlur').get_editor_property('blur_strength')-12)<.1,'Gaussian blur reaches configured strength')
            check(abs(control('SceneDim').get_render_opacity()-.48)<.01,'Black overlay reaches configured opacity')
            check(str(control('ItemName').get_text())=='化学试剂','Selected name uses current collectible text')
            check(str(control('ItemDescription').get_text())=='加点这个……再加点那个……','Selected description uses current collectible text')
            check(control('PreviewImage').get_visibility()==unreal.SlateVisibility.HIT_TEST_INVISIBLE,'Owned item preview is visible')
            for name in ['PreviousButton','NextButton','CloseButton']:
                style=control(name).get_editor_property('widget_style')
                check(style.normal.tint_color.specified_color!=style.pressed.tint_color.specified_color,name+' has a distinct pressed highlight')
            shot('02_owned')
            state['rotation']=str(state['field'].preview.get_actor_rotation())
            panel=control('PreviewPanel');state['position']=panel.slot.get_position()
            panel.slot.set_position(state['position']+unreal.Vector2D(55,15))
            state.update(phase='drag',next=now+.3);return
        if phase=='drag':
            check(ui.exercise_inspect_pointer(state['widget'],False),'Dragging works after moving the preview control')
            check(str(state['field'].preview.get_actor_rotation())!=state['rotation'],'Dragging rotates the actual model')
            check(ui.exercise_inspect_pointer(state['widget'],True),'Wheel zoom handles the moved preview geometry')
            control('PreviewPanel').slot.set_position(state['position'])
            click('NextButton');check(state['field'].selected_id==6,'Next button advances one four-item page')
            click('PreviousButton');check(state['field'].selected_id==2,'Previous button returns to the first page')
            click('ItemButton0');check(state['field'].selected_id==0,'Card click selects its actual item')
            state.update(phase='unknown',next=now+.3);return
        if phase=='unknown':
            check(str(control('ItemName').get_text())=='尚未发现','Unknown item retains its locked description')
            check(control('PreviewImage').get_visibility()==unreal.SlateVisibility.HIDDEN,'Unknown item hides the model')
            shot('03_unknown');click('CloseButton')
            check(not state['field'].catalogue_open and not unreal.GameplayStatics.is_game_paused(world),'Return button resumes the ending')
            state['field'].toggle_catalogue();state.update(phase='reopen',next=now+.9);return
        if phase=='reopen':
            key('Tab');check(not state['field'].catalogue_open,'Tab closes the focused catalogue')
            state['field'].toggle_catalogue();state.update(phase='escape',next=now+.9);return
        if phase=='escape':
            key('Escape');check(not state['field'].catalogue_open,'Escape closes reopened collection')
            check(not state['pc'].is_move_input_ignored() and not state['pc'].is_look_input_ignored(),'Returning restores movement and look')
            check(list(state['field'].inventory)==[0,1,1,0,0,1,0,0,0,1,0,0],'UI interactions preserve collection progress')
            levels.editor_request_end_play();state.update(phase='main_start',next=now+2)
    except Exception:finish(traceback.format_exc())
    finally:state['busy']=False
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
unreal.get_default_object(unreal.load_class(None,'/Script/UnrealEd.EditorPerformanceSettings')).set_editor_property('bThrottleCPUWhenNotForeground',False)
handle=unreal.register_slate_post_tick_callback(tick)
