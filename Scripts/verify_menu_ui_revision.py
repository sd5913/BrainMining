"""Production PIE checks and screenshots for menus, title and the ending card."""
import unreal,json,time,traceback,gc
from pathlib import Path
out=Path(unreal.Paths.project_dir())/'Saved/Validation/UIRevision';out.mkdir(parents=True,exist_ok=True)
mode=(out/'preview_mode.txt').read_text().strip() if (out/'preview_mode.txt').exists() else 'main'
ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem);levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
helper=unreal.MiningGUIEditorLibrary;ui=unreal.CollectionUMGEditorLibrary
report={'passed':False,'mode':mode,'checks':[]};s={'phase':'start','start':time.monotonic(),'next':0,'busy':False}
performance=unreal.get_default_object(unreal.load_class(None,'/Script/UnrealEd.EditorPerformanceSettings'))
old_throttle=performance.get_editor_property('bThrottleCPUWhenNotForeground');performance.set_editor_property('bThrottleCPUWhenNotForeground',False)
def check(ok,name):
    assert ok,name
    report['checks'].append(name)
def key(name,focused=True):
    fn=helper.send_preview_focused_key if focused else helper.send_preview_key
    fn(s['pc'],name,True);fn(s['pc'],name,False)
def shot(name):check(helper.capture_preview(s['pc'],str(out/(name+'.png'))),'Screenshot '+name)
def actors(cls):return unreal.GameplayStatics.get_all_actors_of_class(ed.get_game_world(),cls)
def finish(error=None):
    report.update(passed=not error,error=error)
    (out/(mode+'_checks.json')).write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    if ed.get_game_world():levels.editor_request_end_play()
    s.clear();s.update(phase='cleanup',next=time.monotonic()+2,busy=False)
def tick(dt):
    if s.get('busy') or time.monotonic()<s.get('next',0):return
    s['busy']=True
    try:
        now=time.monotonic();phase=s['phase']
        if phase=='cleanup':
            if ed.get_game_world():return
            gc.collect();levels.load_level('/Game/MainGame/Maps/Lvl_MainGame')
            performance.set_editor_property('bThrottleCPUWhenNotForeground',old_throttle)
            unreal.unregister_slate_post_tick_callback(handle);unreal.EditorPythonScripting.set_keep_python_script_alive(False)
            (out/(mode+'_done.txt')).write_text('done');return
        assert now-s['start']<240,'Timeout in '+phase
        if phase=='start':
            path='/Game/MainGame/Maps/Lvl_MainGame' if mode=='main' else '/Game/EndingEmergence/Maps/Lvl_EndingEmergence_Demo'
            assert levels.load_level(path);helper.start_floating_preview();s.update(phase='ready',next=now+7);return
        w=ed.get_game_world()
        if not w:return
        pc=unreal.GameplayStatics.get_player_controller(w,0)
        if not pc or not pc.get_controlled_pawn():return
        s['pc']=pc
        if phase=='ready':
            instance=unreal.GameplayStatics.get_game_instance(w)
            s['session']=next(o for o in unreal.ObjectIterator(unreal.MiningExperienceSubsystem) if o.get_outer()==instance)
            if mode=='ending':
                check(helper.prepare_ending_credits_preview(pc,[1]*12),'Credits preview uses shared ending pipeline')
                s.update(phase='ending_black',next=now+.45);return
            openings=actors(unreal.MiningOpening)
            if not openings:return
            opening=openings[0];menu=helper.read_preview_object(opening,'Menu')
            if not menu:return
            check(helper.read_preview_object(menu,'TitleImage').get_editor_property('brush').resource_object.get_path_name().startswith('/Game/StartRoom/UI/T_OpeningTitle'),'New title uses shared title asset')
            start=helper.read_preview_object(menu,'StartButton');exit=helper.read_preview_object(menu,'ExitButton')
            check(start.get_editor_property('render_transform').angle<0 and exit.get_editor_property('render_transform').angle>0,'Opening buttons angle towards the room as in the reference')
            shot('opening')
            helper.simulate_preview_button(start,True);helper.simulate_preview_button(start,False)
            s.update(phase='settings_open',next=now+4.5);return
        if phase=='settings_open':
            key('F8',False);s.update(phase='settings_close',next=now+.5);return
        if phase=='settings_close':
            check(unreal.GameplayStatics.is_game_paused(w),'Settings pause the game')
            shot('settings')
            key('Escape');check(not unreal.GameplayStatics.is_game_paused(w),'First focused Escape closes settings and resumes gameplay')
            check(not pc.show_mouse_cursor,'Closing settings restores captured game cursor')
            s.update(phase='pause_open',next=now+.25);return
        if phase=='pause_open':
            key('Escape');s.update(phase='pause_check',next=now+.4);return
        if phase=='pause_check':
            check(unreal.GameplayStatics.is_game_paused(w),'Next Escape opens the pause menu')
            shot('pause')
            s['session'].request_quit_confirmation();check(s['session'].quit_confirmation,'Pause owns the confirmation rather than a hidden settings panel')
            s.update(phase='confirm',next=now+.3);return
        if phase=='confirm':
            shot('quit_confirmation');key('Escape')
            check(not s['session'].quit_confirmation and unreal.GameplayStatics.is_game_paused(w),'Escape cancels quit and stays paused')
            key('Escape');check(not unreal.GameplayStatics.is_game_paused(w),'Second Escape returns to gameplay without resurrecting settings')
            field=next(f for f in actors(unreal.MiningCollectionField) if f.use_authored_placements)
            field.set_editor_property('area_managed',False);helper.write_preview_state(field,'Inventory','('+','.join(['1']*12)+')')
            field.toggle_catalogue();s.update(phase='collection',field=field,next=now+.7);return
        if phase=='collection':
            widget=s['field'].catalogue_widget
            help=ui.find_inspect_control(widget,'PreviewHelp')
            check(help.get_visibility()==unreal.SlateVisibility.COLLAPSED,'Collection help line is fully removed')
            for name in ['PreviousButton','NextButton','CloseButton','ItemButton0','ItemButton1','ItemButton2','ItemButton3']:
                style=ui.find_inspect_control(widget,name).get_editor_property('widget_style')
                n,p=style.normal,style.pressed
                color=lambda b:tuple(getattr(b.tint_color.specified_color,k) for k in ['r','g','b','a'])
                pad=lambda m:tuple(getattr(m,k) for k in ['left','top','right','bottom'])
                check(n.resource_object==p.resource_object and color(n)==color(p) and n.draw_as==p.draw_as and pad(style.normal_padding)==pad(style.pressed_padding),'No pressed highlight or movement on '+name)
            shot('collection');widget.close_collection();finish();return
        if phase=='ending_black':
            shot('credits_black_fade');s.update(phase='ending_text',next=now+3);return
        if phase=='ending_text':
            shot('credits_text');s.update(phase='ending_wait',next=now+3);return
        if phase=='ending_wait':
            shot('credits_before_escape');s.update(phase='ending_prompt',next=now+5.15);return
        if phase=='ending_prompt':
            shot('credits_escape_bright');s.update(phase='ending_breathe',next=now+1.8);return
        if phase=='ending_breathe':
            shot('credits_escape_dim');key('Escape');s.update(phase='ending_pause',next=now+.4);return
        if phase=='ending_pause':
            check(unreal.GameplayStatics.is_game_paused(w),'Credits Escape opens the styled pause menu')
            s['session'].open_pause_collection()
            fields=actors(unreal.MiningCollectionField)
            check(any(f.catalogue_open for f in fields),'Ending pause can still open collection')
            for f in fields:
                if f.catalogue_open:f.toggle_catalogue()
            finish();return
    except Exception:finish(traceback.format_exc())
    finally:
        s['busy']=False
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
handle=unreal.register_slate_post_tick_callback(tick)
