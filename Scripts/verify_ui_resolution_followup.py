"""Check real PIE menus/end-card actions while resizing the game viewport."""
import unreal,json,time,traceback,gc
from pathlib import Path
out=Path(unreal.Paths.project_dir())/'Saved/Validation/UIResponsive';out.mkdir(parents=True,exist_ok=True)
mode=(out/'mode.txt').read_text().strip() if (out/'mode.txt').exists() else 'main'
ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem);levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
helper=unreal.MiningGUIEditorLibrary
resolutions=[(1280,720),(1920,1080),(1024,768),(2560,1080)]
s={'phase':'start','start':time.monotonic(),'next':0,'busy':False};report={'passed':False,'mode':mode,'checks':[],'captures':[]}
performance=unreal.get_default_object(unreal.load_class(None,'/Script/UnrealEd.EditorPerformanceSettings'))
old_throttle=performance.get_editor_property('bThrottleCPUWhenNotForeground');performance.set_editor_property('bThrottleCPUWhenNotForeground',False)
def check(ok,name):
    assert ok,name
    report['checks'].append(name)
def key(name,focused=True):
    fn=helper.send_preview_focused_key if focused else helper.send_preview_key
    fn(s['pc'],name,True);fn(s['pc'],name,False)
def begin_sizes(screen,after):s.update(phase='size',screen=screen,after=after,index=0,next=time.monotonic()+.2)
def finish(error=None):
    report.update(passed=not error,error=error)
    (out/(mode+'.json')).write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    if ed.get_game_world():levels.editor_request_end_play()
    s.clear();s.update(phase='cleanup',next=time.monotonic()+2,busy=False)
def actors(cls):return unreal.GameplayStatics.get_all_actors_of_class(ed.get_game_world(),cls)
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
        assert now-s['start']<300,'Timeout in '+phase
        if phase=='start':
            path='/Game/MainGame/Maps/Lvl_MainGame' if mode=='main' else '/Game/EndingEmergence/Maps/Lvl_EndingEmergence_Demo'
            assert levels.load_level(path);helper.start_floating_preview();s.update(phase='ready',next=now+6);return
        w=ed.get_game_world()
        if phase=='exit_check':
            if w:return
            check(True,'Esc on the end card exits PIE directly without opening pause');finish();return
        if not w:return
        pc=unreal.GameplayStatics.get_player_controller(w,0)
        if not pc or not pc.get_controlled_pawn():return
        s['pc']=pc
        if phase=='ready':
            instance=unreal.GameplayStatics.get_game_instance(w)
            s['session']=next(o for o in unreal.ObjectIterator(unreal.MiningExperienceSubsystem) if o.get_outer()==instance)
            if mode=='ending':
                if unreal.GameplayStatics.get_time_seconds(w)<5:return
                check(helper.prepare_ending_credits_preview(pc,[1]*12),'End card runs through the existing ending controller')
                s.update(phase='credits_ready',next=now+9);return
            opening=actors(unreal.MiningOpening)
            if not opening:return
            menu=helper.read_preview_object(opening[0],'Menu')
            if not menu:return
            s['start_button']=helper.read_preview_object(menu,'StartButton')
            begin_sizes('opening','start_game');return
        if phase=='size':
            width,height=resolutions[s['index']]
            check(helper.resize_preview_viewport(pc,width,height),'Resize '+s['screen']+' '+str(width)+'x'+str(height))
            s.update(phase='capture',next=now+.65);return
        if phase=='capture':
            width,height=resolutions[s['index']];name=s['screen']+'_'+str(width)+'x'+str(height)+'.png'
            check(helper.capture_preview(pc,str(out/name)),'Capture '+name);report['captures'].append(name)
            s['index']+=1
            s.update(phase='size' if s['index']<len(resolutions) else s['after'],next=now+.15);return
        if phase=='start_game':
            button=s.pop('start_button');helper.simulate_preview_button(button,True);helper.simulate_preview_button(button,False)
            s.update(phase='pause_open',next=now+4.5);return
        if phase=='pause_open':
            key('Escape');check(unreal.GameplayStatics.is_game_paused(w),'Escape opens the regular pause menu')
            begin_sizes('pause','confirm_open');return
        if phase=='confirm_open':
            s['session'].request_quit_confirmation();check(s['session'].quit_confirmation,'Quit confirmation opens')
            begin_sizes('confirmation','settings_open');return
        if phase=='settings_open':
            key('Escape');check(not s['session'].quit_confirmation,'Escape cancels confirmation')
            key('Escape');check(not unreal.GameplayStatics.is_game_paused(w),'Escape returns from pause')
            key('F8',False);check(unreal.GameplayStatics.is_game_paused(w),'Settings open')
            begin_sizes('settings','collection_open');return
        if phase=='collection_open':
            key('Escape');check(not unreal.GameplayStatics.is_game_paused(w),'Settings Escape closes the actual settings panel')
            field=next(f for f in actors(unreal.MiningCollectionField) if f.use_authored_placements)
            field.set_editor_property('area_managed',False);helper.write_preview_state(field,'Inventory','('+','.join(['1']*12)+')')
            field.toggle_catalogue();s['field']=field
            begin_sizes('collection','subtitle_open');return
        if phase=='subtitle_open':
            s.pop('field').toggle_catalogue()
            instance=unreal.GameplayStatics.get_game_instance(w)
            sub=next(o for o in unreal.ObjectIterator(unreal.SubtitleSubsystem) if o.get_outer()==instance)
            sub.play_status_line('电脑屏幕弹出越来越多的弹窗 办公室里的打印机开始喷出越来越多的纸',30)
            begin_sizes('subtitle','main_finish');return
        if phase=='main_finish':finish();return
        if phase=='credits_ready':begin_sizes('credits','credits_collect');return
        if phase=='credits_collect':
            key('C');check(any(f.catalogue_open for f in actors(unreal.MiningCollectionField)),'C opens collection despite ending input locks')
            begin_sizes('credits_collection','credits_return');return
        if phase=='credits_return':
            key('Escape');check(not any(f.catalogue_open for f in actors(unreal.MiningCollectionField)),'Collection Escape returns to credits')
            s.update(phase='credits_quit',next=now+.4);return
        if phase=='credits_quit':key('Escape');s.update(phase='exit_check',next=now+.5);return
    except Exception:finish(traceback.format_exc())
    finally:s['busy']=False
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
handle=unreal.register_slate_post_tick_callback(tick)
