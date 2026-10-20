"""PIE regression of production MainGame flows; all fixtures stay in the PIE world."""
import json,time,traceback
from pathlib import Path
import unreal

out=Path(unreal.Paths.project_dir())/'Saved/Validation/GamePolish'
out.mkdir(parents=True,exist_ok=True)
ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
helper=unreal.MiningGUIEditorLibrary
ui=unreal.CollectionUMGEditorLibrary
s={'phase':'start','next':time.monotonic()+2,'start':time.monotonic(),'busy':False}
report={'passed':False,'checks':[]}
performance=unreal.get_default_object(unreal.load_class(None,'/Script/UnrealEd.EditorPerformanceSettings'))
old_throttle=performance.get_editor_property('bThrottleCPUWhenNotForeground')
performance.set_editor_property('bThrottleCPUWhenNotForeground',False)
def check(ok,label):
    assert ok,label
    report['checks'].append(label)
def actors(cls):return unreal.GameplayStatics.get_all_actors_of_class(s['world'],cls)
def key(name):
    helper.send_preview_key(s['pc'],name,True);helper.send_preview_key(s['pc'],name,False)
def control(name):return ui.find_inspect_control(s['widget'],name)
def shot(name):check(helper.capture_preview(s['pc'],str(out/(name+'.png'))),'Screenshot '+name)
def pose(local,target):
    p=unreal.MathLibrary.transform_location(s['room'].get_actor_transform(),unreal.Vector(*local))
    pawn=s['pawn'];move=pawn.get_component_by_class(unreal.CharacterMovementComponent)
    move.stop_movement_immediately();move.disable_movement();pawn.set_actor_location(p,False,True)
    eye=pawn.get_actor_eyes_view_point()[0]
    s['pc'].set_control_rotation(unreal.MathLibrary.find_look_at_rotation(eye,target))
def subtitle():
    instance=unreal.GameplayStatics.get_game_instance(s['world'])
    return next(o for o in unreal.ObjectIterator(unreal.SubtitleSubsystem) if o.get_outer()==instance)
def state(obj,name,value):
    text='('+','.join(str(v) for v in value)+')' if isinstance(value,list) else str(value)
    check(helper.write_preview_state(obj,name,text),'PIE fixture '+name)
def active_id():
    segment=helper.read_preview_object(subtitle(),'ActiveSegment')
    return str(segment.segment_id) if segment else ''
def save_report():
    (out/'validation.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
def finish(error=None):
    report.update(passed=error is None,error=error);save_report()
    if ed.get_game_world():levels.editor_request_end_play()
    s.update(phase='cleanup',next=time.monotonic()+2)

def tick(dt):
    if s['busy'] or time.monotonic()<s['next']:return
    s['busy']=True
    try:
        now=time.monotonic();phase=s['phase']
        if phase=='cleanup':
            if ed.get_game_world():return
            levels.load_level('/Game/MainGame/Maps/Lvl_MainGame')
            performance.set_editor_property('bThrottleCPUWhenNotForeground',old_throttle)
            unreal.unregister_slate_post_tick_callback(handle)
            unreal.EditorPythonScripting.set_keep_python_script_alive(False)
            return
        assert now-s['start']<240,'Timeout in '+phase
        if phase=='start':
            assert levels.load_level('/Game/MainGame/Maps/Lvl_MainGame')
            helper.start_floating_preview();s.update(phase='menu',next=now+6);return
        if phase=='ending_start':
            if ed.get_game_world():return
            assert levels.load_level('/Game/EndingEmergence/Maps/Lvl_EndingEmergence_Demo')
            helper.start_floating_preview();s.update(phase='ending',next=now+5);return
        world=ed.get_game_world()
        if not world:return
        s['world']=world
        if phase=='menu':
            pc=unreal.GameplayStatics.get_player_controller(world,0)
            if not pc or not pc.get_controlled_pawn():return
            openings=actors(unreal.MiningOpening)
            menus=[helper.read_preview_object(o,"Menu") for o in openings if helper.read_preview_object(o,"Menu")]
            if not openings or not menus:return
            room=actors(unreal.MiningStartRoom)[0]
            s.update(pc=pc,pawn=pc.get_controlled_pawn(),opening=openings[0],menu=menus[0],room=room)
            check(len(menus)==1,'One opening menu')
            check(not room.pickaxe_unlocked,'Pickaxe starts locked')
            shot('01_main_menu')
            helper.simulate_preview_button(helper.read_preview_object(menus[0],'StartButton'),True);helper.simulate_preview_button(helper.read_preview_object(menus[0],'StartButton'),False);s.update(phase='fade',next=now+.3);return
        if phase=='fade':
            alpha=helper.read_preview_object(s['menu'],'TitleImage').get_render_opacity()
            check(0<alpha<1,'Start title fades smoothly')
            s.update(phase='props',prop_index=0,next=now+4);return
        if phase=='props':
            check(s['opening'].finished,'Opening returns control')
            check(not s['room'].headlamp_unlocked,'Headlamp remains off before digging')
            lamp=next(c for c in s['pawn'].get_components_by_class(unreal.LightComponent) if c.get_name()=='MiningLight')
            check(not lamp.is_visible(),'Headlamp component is hidden')
            panel=helper.read_preview_object(s['pc'].mining_gui,'MineralPanel')
            check(panel and panel.get_visibility()==unreal.SlateVisibility.HIDDEN,'Corner mineral HUD hidden before pickaxe')
            s.update(phase='aim_prop');return
        if phase=='aim_prop':
            tags=['StartRoomView_Robot','StartRoomView_Computer','StartRoomView_Book']
            tag=tags[s['prop_index']]
            actor=next(a for a in actors(unreal.Actor) if a.actor_has_tag(tag))
            center=actor.get_actor_bounds(False)[0]
            report.setdefault('prop_bounds',[]).append({'tag':tag,'center':str(center),'actor':actor.get_name()})
            subtitle().stop_all();pose([(-235,0,98),(-200,110,98),(-170,210,98)][s['prop_index']],center)
            s.update(phase='interact_prop',expected=tag,next=now+.35,aim_deadline=now+3);return
        if phase=='interact_prop':
            if s['pc'].interaction_hud.interaction_action!='查看' and now<s['aim_deadline']:return
            if s['pc'].interaction_hud.interaction_action!='查看':shot('debug_prop_aim')
            check(s['pc'].interaction_hud.interaction_action=='查看','Shared 查看 prompt for '+s['expected'])
            key('E');check(active_id()==s['expected'],'E plays '+s['expected'])
            s['prop_index']+=1
            if s['prop_index']<3:s.update(phase='aim_prop',next=now+.2);return
            pickup=next(a for a in actors(unreal.MiningStarterPickup) if a.equipment==unreal.MiningStarterEquipment.PICKAXE)
            subtitle().stop_all();pose((210,-87,98),pickup.get_actor_location())
            s.update(phase='pickup',next=now+.3);return
        if phase=='pickup':
            check(s['pc'].interaction_hud.interaction_action=='镐子','Starter prompt contains only the item name')
            key('E');check(s['room'].pickaxe_unlocked,'E collects and unlocks pickaxe')
            s.update(phase='wall',next=now+1.2);return
        if phase=='wall':
            panel=helper.read_preview_object(s['pc'].mining_gui,'MineralPanel')
            check(panel.get_visibility()==unreal.SlateVisibility.HIT_TEST_INVISIBLE,'Corner mineral HUD revealed after pickaxe')
            decals=s['opening'].get_components_by_class(unreal.DecalComponent)
            check(len(decals)==1 and decals[0].get_editor_property('fade_in_duration')>.5,'Wall title fades in as a noncolliding decal')
            pose((250,0,98),s['opening'].get_editor_property('door_title_location'))
            s.update(phase='wall_capture',next=now+.3);return
        if phase=='wall_capture':
            shot('02_pickaxe_wall_title')
            rock=next(a for a in actors(unreal.DiggableRock) if a.actor_has_tag('BM_StartRoom_ExitBlocker'))
            rock.mine_sphere(unreal.MathLibrary.transform_location(s['room'].get_actor_transform(),unreal.Vector(525,0,95)),150)
            s.update(phase='breach',next=now+.5);return
        if phase=='breach':
            check(s['room'].headlamp_unlocked,'Digging open the entrance unlocks headlamp')
            lamp=next(c for c in s['pawn'].get_components_by_class(unreal.LightComponent) if c.get_name()=='MiningLight')
            check(lamp.is_visible(),'Headlamp enabled after breach')
            field=next(f for f in actors(unreal.MiningCollectionField) if f.use_authored_placements)
            field.set_editor_property('area_managed',False)
            target=unreal.MathLibrary.transform_location(s['room'].get_actor_transform(),unreal.Vector(160,-100,135))
            item=helper.spawn_preview_collectible(field,2,target)
            check(item is not None,'PIE-only collectible fixture')
            subtitle().stop_all();pose((20,-100,98),target)
            s.update(phase='collect',field=field,item=item,next=now+.4);return
        if phase=='collect':
            s['field'].collect_target();check(s['item'].collected,'Shared pickup path collects the first collectible')
            check(active_id().startswith('收集物拾取_'),'Pickup subtitle starts first')
            s.update(phase='collection_hint',next=now+.2);return
        if phase=='collection_hint':
            if active_id()!='首次收藏提示':return
            check(True,'Collection hint follows completed pickup subtitle')
            check(str(helper.read_preview_object(subtitle(),'ActiveSegment').lines[0].text)=='Collection……按C键打开界面吧','First collection hint exact text')
            s['field'].set_editor_property('authored_placements',[])
            inv=[0]*12;inv[0]=1;inv[2]=1;inv[10]=1;state(s['field'],'Inventory',inv)
            subtitle().stop_all();key('C');check(s['field'].catalogue_open,'C opens collection')
            s['widget']=s['field'].catalogue_widget;s.update(phase='catalogue',next=now+.8);return
        if phase=='catalogue':
            widget=s['widget'];widget.select_collection_item(0)
            check(control('PreviousButton').get_visibility()==unreal.SlateVisibility.HIDDEN,'First page hides previous arrow')
            check(not control('ItemButton1').get_is_enabled(),'Locked collectible is disabled')
            widget.select_collection_item(1);check(s['field'].selected_id==0,'Programmatic selection cannot bypass locked item')
            widget.turn_collection_page(1)
            check(s['field'].selected_id==-1 and str(control('ItemDescription').get_text())=='','Empty page has no selected item or locked description')
            widget.turn_collection_page(1)
            check(s['field'].selected_id==10,'Last page selects its first owned item')
            check(control('NextButton').get_visibility()==unreal.SlateVisibility.HIDDEN,'Last page hides next arrow')
            widget.turn_collection_page(1);check(s['field'].selected_id==10,'Page does not wrap past the right boundary')
            check(str(control('ItemName').get_text())=='罐子里的萤火虫','Firefly collectible renamed')
            s.update(phase='firefly_shot',next=now+.5);return
        if phase=='firefly_shot':
            widget=s['widget'];shot('03_firefly_collection')
            widget.close_collection();key('C');check(s['field'].catalogue_open,'C reopens collection')
            check(str(control('ItemName').get_text())=='罐子里的萤火虫','Reopened model and selected page agree')
            widget.close_collection();key('F8');s.update(phase='settings',next=now+.5);return
        if phase=='settings':
            shot('04_settings_sensitivity');key('F8')
            bomb=s['pawn'].get_component_by_class(unreal.MiningBombToolComponent);bomb.unlock_skill();bomb.set_editor_property('use_inventory',True);state(bomb,'BombCount',0);state(bomb,'RechargeProgress',.45);bomb.set_component_tick_enabled(False)
            s.update(phase='bomb',next=now+.4);return
        if phase=='bomb':
            ability=s['pc'].mining_gui.ability_g
            check(not ability.recharge_bar,'Bomb cooldown has no separate bar')
            mat=ability.ability_icon.get_dynamic_material()
            check(mat.get_scalar_parameter_value('RechargeActive')==1,'Bomb alpha material receives recharge state')
            shot('05_bomb_internal_cooldown');levels.editor_request_end_play();s.update(phase='ending_start',next=now+2);return
        if phase=='ending':
            pc=unreal.GameplayStatics.get_player_controller(world,0)
            if not pc or not pc.get_controlled_pawn():return
            s.update(pc=pc,pawn=pc.get_controlled_pawn())
            sub=subtitle();sub.play_segment('StartRoomView_Robot');sub.play_status_line('Ending subtitles must stay hidden',3)
            check(not helper.read_preview_object(sub,'ActiveSegment'),'Ending suppresses narrative and status subtitles')
            director=actors(unreal.EndingEmergence)[0];director.set_actor_tick_enabled(False);state(director,'GateIndex',2);state(director,'Shot',-1)
            s.update(phase='ending_capture',next=now+.4);return
        if phase=='ending_capture':
            shot('06_ending_jump_hint');finish();return
        save_report()
    except Exception:
        finish(traceback.format_exc())
    finally:
        s['busy']=False
        report['phase']=s['phase'];save_report()

unreal.EditorPythonScripting.set_keep_python_script_alive(True)
handle=unreal.register_slate_post_tick_callback(tick)









