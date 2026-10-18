"""Real starter pickup, V equip, held drilling and stow in MainGame PIE."""
import unreal,json,time,traceback,math
from pathlib import Path
ROOT=Path(unreal.Paths.project_dir()).resolve();OUT=ROOT/'Saved/Validation/MainGameDrill';OUT.mkdir(parents=True,exist_ok=True)
ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem);levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);helper=unreal.MiningGUIEditorLibrary
perf=unreal.get_default_object(unreal.load_class(None,'/Script/UnrealEd.EditorPerformanceSettings'));old=perf.get_editor_property('bThrottleCPUWhenNotForeground');perf.set_editor_property('bThrottleCPUWhenNotForeground',False)
s={'phase':'setup','next':0,'start':time.monotonic(),'busy':False,'pickup':0};report={'passed':False,'checks':[],'frames':{}}
def check(ok,msg):assert ok,msg;report['checks'].append(msg)
def key(n,down):helper.send_preview_key(s['pc'],n,down)
def press(n):key(n,True);key(n,False)
def frame(label):
    items=[]
    for c in s['pawn'].get_components_by_class(unreal.StaticMeshComponent):
        if c.get_name() not in ['PickaxeModel','DrillRightModel','DrillLeftModel']:continue
        chain=[];p=c
        while p:
            chain.append(dict(name=p.get_name(),hidden=p.get_editor_property('hidden_in_game'),visible=p.get_editor_property('visible')));p=p.get_attach_parent()
        box=c.static_mesh.get_bounding_box();corners=[]
        camera=s['pawn'].get_component_by_class(unreal.CameraComponent)
        for x in [box.min.x,box.max.x]:
            for y in [box.min.y,box.max.y]:
                for z in [box.min.z,box.max.z]:
                    w=unreal.MathLibrary.transform_location(c.get_world_transform(),unreal.Vector(x,y,z));corners.append(list(unreal.MathLibrary.inverse_transform_location(camera.get_world_transform(),w).to_tuple()))
        flags={}
        for flag in ['render_in_main_pass','render_in_depth_pass','only_owner_see','owner_no_see','first_person_primitive_type','visible_in_scene_capture_only','hidden_in_scene_capture','bounds_scale']:
            try:flags[flag]=str(c.get_editor_property(flag))
            except Exception:pass
        items.append(dict(name=c.get_name(),mesh=c.static_mesh.get_path_name() if c.static_mesh else None,visible=c.is_visible(),hidden=c.get_editor_property('hidden_in_game'),world=list(c.get_world_location().to_tuple()),scale=list(c.get_world_scale().to_tuple()),owner_no_see=c.get_editor_property('owner_no_see'),chain=chain,flags=flags,camera_space_corners=corners,triangles=c.static_mesh.get_num_triangles(0),materials=[c.get_material(i).get_path_name() if c.get_material(i) else None for i in range(c.get_num_materials())]))
    report['frames'][label]=dict(models=items,drill=s['mining'].get_editor_property('bDrillMode'),effects_enabled=s['effects'].get_editor_property('bEnabled'))
    helper.capture_preview(s['pc'],str(OUT/(label+'.png')))
    if label in ['drill_equipped','drill_running']:
        width,height=s['pc'].get_viewport_size();tan_h=math.tan(math.radians(s['pc'].player_camera_manager.get_fov_angle())*.5);tan_v=tan_h*height/width
        for item in items:
            if not item['name'].startswith('Drill'):continue
            check(item['visible'] and not item['hidden'] and item['triangles']>0 and not item['owner_no_see'],label+' '+item['name']+' has visible physical mesh')
            inside=sum(x>10 and abs(y)<x*tan_h and abs(z)<x*tan_v for x,y,z in item['camera_space_corners']);item['corners_in_view']=inside
            check(inside>=4,label+' '+item['name']+' occupies the first-person viewing frustum')
        check(not next(i for i in items if i['name']=='PickaxeModel')['visible'],'Pickaxe is stowed while drilling')
def finish(err=None):
    if 'pc' in s:
        for n in ['E','V','LeftMouseButton']:key(n,False)
    report.update(passed=err is None,elapsed=time.monotonic()-s['start'])
    if err:report['error']=err
    (OUT/'runtime.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8');s['phase']='done';perf.set_editor_property('bThrottleCPUWhenNotForeground',old);unreal.unregister_slate_post_tick_callback(callback);levels.editor_request_end_play()
def tick(dt):
    if s['busy'] or s['phase']=='done':return
    s['busy']=True
    try:
        now=time.monotonic();assert now-s['start']<100,'Drill audit timed out'
        if now<s['next']:return
        world=ed.get_game_world()
        if not world:return
        phase=s['phase']
        if phase=='setup':
            for opening in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.MiningOpening):
                if not opening.finished:opening.start_game();s['next']=now+1;return
            pc=unreal.GameplayStatics.get_player_controller(world,0);pawn=unreal.GameplayStatics.get_player_pawn(world,0);halls=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.MiningExhibitHall)
            if not pawn or not pc or not pc.mining_gui or not halls or not halls[0].get_editor_property('bReady'):return
            s.update(world=world,pc=pc,pawn=pawn,hall=halls[0],mining=pawn.get_component_by_class(unreal.MiningComponent),effects=pawn.get_component_by_class(unreal.MiningToolEffectsComponent),move=pawn.get_component_by_class(unreal.MiningCharacterMovementComponent),pickups=list(unreal.GameplayStatics.get_all_actors_of_class(world,unreal.MiningStarterPickup)),room=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.MiningStartRoom)[0],phase='aim')
        elif phase=='aim':
            p=s['pickups'][s['pickup']].get_actor_location();s['pawn'].set_actor_location(unreal.Vector(p.x-125,p.y,s['room'].get_actor_location().z+102),False,True);s['move'].set_movement_mode(unreal.MovementMode.MOVE_FLYING);s['pc'].set_control_rotation(unreal.MathLibrary.find_look_at_rotation(s['pawn'].get_component_by_class(unreal.CameraComponent).get_world_location(),p));s.update(phase='pickup',next=now+.3)
        elif phase=='pickup':
            press('E');check(s['pickups'][s['pickup']].collected,'Real E acquires starter pickup '+str(s['pickup']));s['pickup']+=1;s.update(phase='aim' if s['pickup']<2 else 'equip',next=now+.4)
        elif phase=='equip':
            check(not s['mining'].is_drill_unlocked(),'Starter pickups do not unlock the card-room drill')
            check(not s['pawn'].get_component_by_class(unreal.MiningBombToolComponent).is_skill_unlocked(),'Starter pickups do not unlock the office bomb')
            press('V');press('G')
            check(not s['mining'].get_editor_property('bDrillMode') and not s['pawn'].get_component_by_class(unreal.MiningBombToolComponent).get_editor_property('bEquipped'),'V and G cannot equip locked campaign rewards')
            s.update(phase='grant',next=now+.3)
        elif phase=='grant':
            # Drill pickup now belongs to the card room; grant it for this pose fixture.
            s['mining'].unlock_drill()
            s['pc'].set_control_rotation(unreal.Rotator());frame('pickaxe');press('V');s.update(phase='equipped',next=now+.8)
        elif phase=='equipped':
            frame('drill_equipped');check(s['mining'].get_editor_property('bDrillMode'),'Real V equips the drill');key('LeftMouseButton',True);s.update(phase='running',next=now+.7)
        elif phase=='running':
            frame('drill_running');check(s['mining'].is_mining_active(),'Real held left mouse starts drilling');check(s['effects'].get_editor_property('bDrillRunning'),'Drill spin animation runs with held input');key('LeftMouseButton',False);press('V');s.update(phase='stowed',next=now+.6)
        elif phase=='stowed':
            frame('stowed');check(not s['mining'].get_editor_property('bDrillMode'),'Real V returns to pickaxe');models=report['frames']['stowed']['models'];check(next(c for c in models if c['name']=='PickaxeModel')['visible'] and not any(c['visible'] for c in models if c['name'].startswith('Drill')),'Stowing hides both drills and restores pickaxe');check(all(c.get_editor_property('hidden_in_game') for c in s['pawn'].get_components_by_class(unreal.SkeletalMeshComponent)),'Player body remains hidden');finish()
    except Exception:finish(traceback.format_exc())
    finally:s['busy']=False
assert not ed.get_game_world();assert levels.load_level('/Game/MainGame/Maps/Lvl_MainGame')
unreal.EditorPythonScripting.set_keep_python_script_alive(True);callback=unreal.register_slate_post_tick_callback(tick);helper.start_floating_preview()
