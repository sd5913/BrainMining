"""Formal MainGame PIE: starter pickup, short-click excavation and visible upgrades."""
import unreal,json,time,traceback
from pathlib import Path
ROOT=Path(unreal.Paths.project_dir()).resolve();OUT=ROOT/'Saved/Validation/PickaxeFeel';OUT.mkdir(parents=True,exist_ok=True)
ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem);levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);helper=unreal.MiningGUIEditorLibrary
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
s=dict(phase=0,start=time.monotonic(),next=0,rows=[],trial=0,checks=[],upgrades=[])
def actors(cls):return unreal.GameplayStatics.get_all_actors_of_class(s['w'],cls)
def pose(eye,target):
    p=s['pawn'];camera=p.get_component_by_class(unreal.CameraComponent)
    p.set_actor_location(eye-(camera.get_world_location()-p.get_actor_location()),False,True)
    s['pc'].set_control_rotation(unreal.MathLibrary.find_look_at_rotation(eye,target))
def finish(error=''):
    unreal.unregister_slate_post_tick_callback(handle)
    (OUT/'runtime.json').write_text(json.dumps(dict(passed=not error,error=error,checks=s['checks'],rows=s['rows'],upgrades=s['upgrades'],seconds=time.monotonic()-s['start']),indent=2),encoding='utf-8')
    levels.editor_request_end_play();s['cleanup']=time.monotonic()+2
    def cleanup(dt):
        if time.monotonic()<s['cleanup'] or ed.get_game_world():return
        unreal.unregister_slate_post_tick_callback(s['cleanup_handle']);s.clear();levels.load_level('/Engine/Maps/Entry');unreal.SystemLibrary.quit_editor()
    s['cleanup_handle']=unreal.register_slate_post_tick_callback(cleanup)
def tick(dt):
    if s.get('busy'):return
    s['busy']=True
    try:
        now=time.monotonic()
        assert now-s['start']<220,'Probe timed out'
        if now<s['next']:return
        ph=s['phase']
        if ph==0:
            assert levels.load_level('/Game/MainGame/Maps/Lvl_MainGame');helper.start_floating_preview();s.update(phase=1,next=now+4);return
        if ph==1:
            w=ed.get_game_world()
            if not w:return
            s['w']=w;h=actors(unreal.MiningExhibitHall)[0]
            if not h.ready:return
            for o in actors(unreal.MiningOpening):o.start_game()
            s.update(hall=h,pc=unreal.GameplayStatics.get_player_controller(w,0),pawn=unreal.GameplayStatics.get_player_pawn(w,0),phase=2,next=now+4);return
        if ph==2:
            for o in actors(unreal.MiningOpening):
                if not o.finished:o.start_game();s['next']=now+.2;return
            p=s['pawn'];p.get_component_by_class(unreal.CharacterMovementComponent).disable_movement()
            s['mine']=p.get_component_by_class(unreal.MiningComponent)
            s['room']=actors(unreal.MiningStartRoom)[0]
            pick=next(a for a in actors(unreal.MiningStarterPickup) if a.equipment==unreal.MiningStarterEquipment.PICKAXE)
            pos=pick.get_actor_location();pose(pos+unreal.Vector(-100,0,45),pos)
            s.update(phase=3,next=now+.4);return
        if ph==3:
            assert s['room'].interact(s['pc']),'Starter pickup failed: '+str(s['room'].get_target(s['pc']))
            s['rock']=next(a for a in actors(unreal.DiggableRock) if a.actor_has_tag('BM_StartRoom_ExitBlocker'))
            s['rows'].append(dict(rock=s['rock'].get_path_name(),pos=list(s['rock'].get_actor_location().to_tuple()),size=list(s['rock'].rock_size.to_tuple()),hardness=s['rock'].hardness_level,reach=s['mine'].reach))
            s.update(phase=4);return
        if ph==4:
            r=s['rock']
            if s['trial']%4==0 or s['trial']>=36:r.reset_rock()
            center=r.get_actor_location()
            # Each trial starts from intact terrain. Aim at centre and near its edges.
            lateral=[0,-65,65,0,0,-100,100,0,0][(s['trial']//4)%9]
            up=[0,0,0,-65,65,0,0,-100,100][(s['trial']//4)%9]
            target=center+unreal.Vector(0,lateral,up)
            if s['trial']>=36:target=center+unreal.Vector(0,109,0)
            pose(target+unreal.Vector(-250,0,0),target)
            s.update(phase=5,next=now+.4);return
        if ph==5:
            m=s['mine'];r=s['rock'];cam=s['pawn'].get_component_by_class(unreal.CameraComponent);eye=cam.get_world_location();rot=cam.get_world_rotation()
            hit=unreal.SystemLibrary.line_trace_single(s['w'],eye,eye+unreal.MathLibrary.get_forward_vector(rot)*m.reach,unreal.TraceTypeQuery.ECC_VISIBILITY,True,[s['pawn']],unreal.DrawDebugTrace.NONE,True)
            hit=hit.to_tuple() if hit else None
            s['row']=dict(trial=s['trial'],target=m.target_rock.get_name() if m.target_rock else None,hit=hit[9].get_name() if hit and hit[9] else None,point=list(hit[5].to_tuple()) if hit else None,normal=list(hit[7].to_tuple()) if hit else None,before=r.successful_cuts)
            if s['trial']>=36:assert m.target_rock==r,'Edge test must start aimed at dirt'
            helper.send_preview_key(s['pc'],'LeftMouseButton',True);helper.send_preview_key(s['pc'],'LeftMouseButton',False)
            if s['trial']>=36:s['pc'].set_control_rotation(unreal.Rotator(yaw=2 if s['trial']==36 else 15))
            s.update(phase=6,next=now+.55);return
        if ph==6:
            r=s['rock'];row=s['row'];row.update(after=s['rock'].successful_cuts,status=s['mine'].status,rock_status=s['rock'].last_status);s['rows'].append(row)
            s['trial']+=1
            if row['target']==r.get_name() and row['trial']!=37:
                assert row['after']==row['before']+1,str(row)
                s['checks'].append('Real short click excavates trial '+str(row['trial']))
            if row['trial']==37:
                assert row['after']==row['before'],'Turning away should miss'
                s['checks'].append('Large deliberate turn still misses')
            if s['trial']>=38:s.update(phase=7);return
            s.update(phase=4)
        if ph==7:
            s['progress']=s['pawn'].get_component_by_class(unreal.MiningProgressionComponent)
            wallet=s['progress'].get_wallet()
            for material in ['2','4','6','9']:assert wallet.add_mineral(material,100)
            s['effects']=s['pawn'].get_component_by_class(unreal.MiningToolEffectsComponent)
            s.update(level=1,phase=8);return
        if ph==8:
            level=s['level'];e=s['effects'];before=time.perf_counter()
            assert s['progress'].purchase(0,level),'Upgrade purchase failed'
            elapsed=(time.perf_counter()-before)*1000
            expected=['','copper','silver','gold','max'][level]
            mesh=e.pickaxe_mesh.get_editor_property('static_mesh')
            assert mesh and mesh.get_name()=='SM_Pickaxe_'+expected,'New model must be assigned immediately'
            assert e.pickaxe_upgrade_remaining>.6,'Upgrade animation starts immediately'
            s['upgrades'].append(dict(level=level,mesh=mesh.get_name(),purchase_ms=elapsed))
            s['checks'].append('Level '+str(level)+' model swaps immediately')
            s.update(phase=9,next=now+.12,upgrade_started=now);return
        if ph==9:
            e=s['effects'];assert e.pickaxe_mesh.is_visible() and e.pickaxe_mesh.get_editor_property('static_mesh'),'Tool disappeared'
            assert e.pickaxe_upgrade_remaining>0 and e.pickaxe_pivot.get_editor_property('relative_location').length()>1,'Upgrade visibly lifts tool'
            if '-nullrhi' not in unreal.SystemLibrary.get_command_line().lower():
                assert helper.capture_preview(s['pc'],str(OUT/('upgrade_'+str(s['level'])+'.png')))
            s['checks'].append('Level '+str(s['level'])+' visible during animation')
            s['progress'].apply_upgrades()
            s.update(phase=10,next=now+.8);return
        if ph==10:
            if s['effects'].pickaxe_upgrade_remaining>0 and now-s['upgrade_started']<5:return
            e=s['effects'];assert e.pickaxe_upgrade_remaining==0 and e.pickaxe_pivot.get_editor_property('relative_location').length()<.01,'Animation must settle'
            s['checks'].append('Level '+str(s['level'])+' settles without replay')
            if s['level']==4:finish();return
            s['level']+=1;s.update(phase=8)
    except Exception:finish(traceback.format_exc())
    finally:s['busy']=False
handle=unreal.register_slate_post_tick_callback(tick)
