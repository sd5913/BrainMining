"""PIE-only check of V input, real contact effects, editable pose and timed cooldown."""
import json,time,traceback
from pathlib import Path
import unreal
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
root=Path(unreal.Paths.project_dir());out=root/'Saved/Validation/DrillRevision';out.mkdir(parents=True,exist_ok=True)
editor=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
helper=unreal.MiningGUIEditorLibrary
settings=unreal.get_default_object(unreal.load_class(None,'/Script/UnrealEd.EditorPerformanceSettings'))
old=settings.get_editor_property('bThrottleCPUWhenNotForeground');settings.set_editor_property('bThrottleCPUWhenNotForeground',False)
report={'passed':False,'checks':[]};s={'phase':'warmup','started':time.monotonic(),'ready':None,'next':0}
def check(v,label):
    assert v,label
    report['checks'].append(label)
bp=unreal.load_asset('/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter')
cdo=unreal.get_default_object(bp.generated_class())
camera=cdo.get_component_by_class(unreal.CameraComponent)
for stem in ['Pickaxe','DrillRight','DrillLeft']:
    mount=cdo.get_editor_property(stem+'Mount');motion=cdo.get_editor_property(stem+'Motion');model=cdo.get_editor_property(stem+'Model')
    check(mount.get_attach_parent()==camera and motion.get_attach_parent()==mount and model.get_attach_parent()==motion,stem+' has editable camera mount with separate animation child')
    check(model.static_mesh is not None and model.get_editor_property('visible') and not model.get_editor_property('only_owner_see'),stem+' model is visible in editor without starting the game')
    check(model.get_editor_property('hidden_in_game'),stem+' editor preview starts hidden until gameplay selects a tool')
def key(n,v):helper.send_preview_key(s['pc'],n,v)
def shot(n):check(helper.capture_preview(s['pc'],str(out/(n+'.png'))),'Captured '+n)
def actors(cls):return unreal.GameplayStatics.get_all_actors_of_class(s['world'],cls)
def aim():
    p,e=s['rock'].get_actor_bounds(False)
    s['pawn'].set_actor_location(unreal.Vector(p.x-e.x-130,p.y,p.z),False,True)
    s['pc'].set_control_rotation(unreal.Rotator())
def volume():return abs(unreal.GeometryScript_MeshQueries.get_mesh_volume_area(s['rock'].rock_mesh.get_dynamic_mesh())[1])
def finish(ok):
    unreal.unregister_slate_post_tick_callback(callback)
    settings.set_editor_property('bThrottleCPUWhenNotForeground',old)
    report['passed']=ok;report['elapsed']=time.monotonic()-s['started']
    (out/'runtime.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    unreal.log('DRILL_REVISION_RESULT '+json.dumps(report));levels.editor_request_end_play()
    s.update(phase='exit',next=time.monotonic()+2)
def tick(delta):
    try:
        if time.monotonic()-s['started']>240:raise RuntimeError('Timeout at '+s['phase'])
        phase=s['phase']
        if phase=='warmup':
            world=editor.get_game_world()
            if not world:return
            if s['ready'] is None:s['ready']=time.monotonic()
            if time.monotonic()-s['ready']<20:return
            pawn=unreal.GameplayStatics.get_player_character(world,0)
            if not pawn:return
            pc=unreal.GameplayStatics.get_player_controller(world,0)
            m=pawn.get_component_by_class(unreal.MiningComponent);e=pawn.get_component_by_class(unreal.MiningToolEffectsComponent)
            s.update(world=world,pawn=pawn,pc=pc,m=m,e=e,rock=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.DiggableRock)[0])
            # Isolate the new contact dust from the lab's existing green chamber fog.
            # This changes only the temporary PIE instance and never saves the map.
            s['rock'].fog_chamber.set_editor_property('bEnabled',False)
            pawn.get_component_by_class(unreal.CharacterMovementComponent).set_movement_mode(unreal.MovementMode.MOVE_NONE)
            m.interval=.3;m.radius=160;m.reset_field();aim()
            check(e.left_drill_mesh and e.drill_mesh,'Both drills present on existing character')
            check(e.drill_mesh.static_mesh.get_num_triangles(0)>0,'Editable drill mesh has valid geometry')
            check(e.drill_mesh.get_material(0).get_path_name().endswith('M_Drill_Body.M_Drill_Body') and e.drill_mesh.get_material(1).get_path_name().endswith('M_Drill.M_Drill'),'Both new FBX materials assigned to correct mesh slots')
            check(not str(pc.mining_gui.coin_text.get_text()),'Currency display remains blank')
            check(not m.get_editor_property('bDrillMode'),'Pickaxe is initial tool')
            key('Q',True);s.update(phase='q_ignored',next=unreal.GameplayStatics.get_time_seconds(world)+.12);return
        now=unreal.GameplayStatics.get_time_seconds(s['world'])
        if now<s['next']:return
        m,e,pc=s['m'],s['e'],s['pc'];v=pc.mining_gui.ability_v
        if phase=='q_ignored':
            key('Q',False);check(not m.get_editor_property('bDrillMode'),'Q no longer switches weapons')
            s['volume']=volume();key('LeftMouseButton',True);s.update(phase='pickaxe',next=now+.23)
        elif phase=='pickaxe':
            key('LeftMouseButton',False)
            check(s['rock'].successful_cuts==1 and e.swing_count>0,'Original pickaxe still mines and swings')
            s['pickaxe_removed']=s['volume']-volume()
            check(not actors(unreal.MiningDrillDustBurst),'Pickaxe effect unchanged')
            m.reset_field();aim();key('V',True);s.update(phase='equipped',next=now+.35)
        elif phase=='equipped':
            key('V',False);check(m.get_editor_property('bDrillMode'),'V equips drill through actual input')
            check(e.drill_mesh.get_editor_property('visible') and e.left_drill_mesh.get_editor_property('visible') and not e.pickaxe_mesh.get_editor_property('visible'),'Dual drill pose replaces pickaxe while active')
            check(not v.get_editor_property('bUsed') and not str(v.cooldown_text.get_text()),'Active drill has normal icon and no cooldown numbers')
            s['use_end']=now+m.get_drill_use_remaining();s['remaining']=m.get_drill_use_remaining()
            shot('dual_drills');s['mount']=e.drill_pivot.get_attach_parent()
            loc=s['mount'].get_editor_property('relative_location');s['pose']=unreal.Vector(loc.x,loc.y,loc.z)
            scale=e.drill_mesh.get_editor_property('relative_scale3d');s['scale']=unreal.Vector(scale.x,scale.y,scale.z)
            s['mount'].set_editor_property('relative_location',unreal.Vector(loc.x,loc.y,-6))
            e.drill_mesh.set_editor_property('relative_scale3d',unreal.Vector(.65,.65,.65))
            s['model_asset']=e.drill_mesh.static_mesh
            e.drill_mesh.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Cube'))
            e.drill_mesh.set_editor_property('relative_rotation',unreal.Rotator(pitch=3,yaw=4,roll=5))
            s.update(phase='editable',next=now+.1)
        elif phase=='editable':
            check(abs(e.drill_mesh.get_editor_property('relative_scale3d').x-.65)<.01 and abs(s['mount'].get_editor_property('relative_location').z+6)<.1,'Direct component position and scale edits survive animation ticks')
            rot=e.drill_mesh.get_editor_property('relative_rotation')
            check(e.drill_mesh.static_mesh.get_name()=='Cube' and max(abs(rot.pitch-3),abs(rot.yaw-4),abs(rot.roll-5))<.01,'Direct component mesh replacement and orientation survive runtime updates')
            e.drill_mesh.set_static_mesh(s['model_asset']);e.drill_mesh.set_editor_property('relative_rotation',unreal.Rotator())
            e.drill_mesh.set_editor_property('relative_scale3d',s['scale']);s['mount'].set_editor_property('relative_location',s['pose'])
            key('V',True);s.update(phase='no_restart',next=now+.12)
        elif phase=='no_restart':
            key('V',False);check(abs(now+m.get_drill_use_remaining()-s['use_end'])<.04,'Repeated V cannot extend the ten-second session')
            s['angle']=e.drill_spin_degrees;s['swings']=e.swing_count
            key('LeftMouseButton',True);s.update(phase='contact',next=now+.16)
        elif phase=='contact':
            check(e.get_editor_property('bDrillRunning') and abs(e.drill_spin_degrees-s['angle'])>1,'Drill spins while mouse is held')
            check(e.swing_count==s['swings'],'Pickaxe animation not played in drill mode')
            chunks=actors(unreal.MiningDebrisBurst);clouds=actors(unreal.MiningDrillDustBurst)
            check(chunks and all(50<=x.chunk_count<=62 for x in chunks),'More randomized physical fragments per drill hit')
            check(s['volume']-volume()>s['pickaxe_removed']*1.2,'Drill removes more actual soil')
            check(clouds and all(max(1,int(e.drill_dust_particle_count*.8-.5))<=x.sprites.get_instance_count()<=min(64,int(e.drill_dust_particle_count*1.2+.5)) and max(1,int(e.drill_smoke_particle_count*.8-.5))<=x.smoke_sprites.get_instance_count()<=min(24,int(e.drill_smoke_particle_count*1.2+.5)) for x in clouds),'Real contact follows editable dust and smoke counts')
            check(all(x.source_material==s['rock'].rock_mesh.get_material(0) for x in clouds),'Dust follows actual contacted soil color')
            s['spark_seen']=any(x.spark_sprites.get_instance_count()>0 for x in clouds)
            report['contact_counts']=[{'dust':x.sprites.get_instance_count(),'smoke':x.smoke_sprites.get_instance_count(),'sparks':x.spark_sprites.get_instance_count()} for x in clouds]
            report['pose']={'right':str(s['mount'].get_editor_property('relative_rotation')),'left':str(e.left_drill_pivot.get_attach_parent().get_editor_property('relative_rotation')),'location':str(s['mount'].get_editor_property('relative_location')),'scale':str(e.drill_mesh.get_editor_property('relative_scale3d'))}
            shot('drilling_contact');s.update(phase='more_contact',next=now+.65)
        elif phase=='more_contact':
            clouds=actors(unreal.MiningDrillDustBurst)
            check(s['spark_seen'] or any(x.spark_sprites.get_instance_count()>0 for x in clouds),'Visible sparks are emitted at real impacts')
            check(len(clouds)<=12 and len(actors(unreal.MiningDebrisBurst))<=6,'Effects and physical debris have bounded accumulation')
            key('LeftMouseButton',False);key('Tab',True);s.update(phase='menu',next=now+.3)
        elif phase=='menu':
            check(pc.show_mouse_cursor and pc.is_look_input_ignored() and not m.is_mining_active(),'Tab still exposes pointer and suspends drill/look')
            check(helper.send_preview_focused_key(pc,'Tab',False),'Focused UI accepts Tab release')
            s.update(phase='closed',next=now+.2)
        elif phase=='closed':
            check(not pc.show_mouse_cursor and not pc.is_look_input_ignored(),'Tab release restores camera and hides cursor')
            check(not m.is_mining_active(),'Menu release cannot restart a stale mining click')
            pc.set_control_rotation(unreal.Rotator(pitch=90,yaw=180));s.update(phase='miss_ready',next=now+.1)
        elif phase=='miss_ready':
            s['bursts']=e.debris_burst_count;key('LeftMouseButton',True);s.update(phase='miss',next=now+1.8)
        elif phase=='miss':
            check(e.debris_burst_count==s['bursts'] and not actors(unreal.MiningDrillDustBurst),'No dust or fragments are produced in empty space')
            key('LeftMouseButton',False);s['angle']=e.drill_spin_degrees;s.update(phase='expiry',next=s['use_end']+.15)
        elif phase=='expiry':
            check(not m.get_editor_property('bDrillMode') and e.pickaxe_mesh.get_editor_property('visible'),'At ten seconds drill automatically returns to pickaxe')
            check(not e.drill_mesh.get_editor_property('visible') and not e.left_drill_mesh.get_editor_property('visible'),'Both drills hide at session end')
            check(9.5<m.get_drill_cooldown_remaining()<=10 and v.get_editor_property('bUsed'),'Cooldown makes V icon red for ten seconds')
            check(str(v.cooldown_text.get_text())=='10','Countdown number overlays red icon')
            shot('cooldown_10');key('V',True);s.update(phase='blocked',next=now+.2)
        elif phase=='blocked':
            key('V',False);check(not m.get_editor_property('bDrillMode'),'V is rejected during cooldown')
            s['cooldown_end']=now+m.get_drill_cooldown_remaining();s.update(phase='countdown',next=now+2.2)
        elif phase=='countdown':
            number=int(str(v.cooldown_text.get_text()))
            check(6<=number<=8 and number==int(__import__('math').ceil(m.get_drill_cooldown_remaining())),'Cooldown display decrements with gameplay timer')
            shot('cooldown_counting');s.update(phase='ready',next=s['cooldown_end']+.15)
        elif phase=='ready':
            check(m.get_drill_cooldown_remaining()==0 and not v.get_editor_property('bUsed') and not str(v.cooldown_text.get_text()),'Cooldown expiry restores normal V icon and hides digits')
            key('V',True);s.update(phase='again',next=now+.2)
        elif phase=='again':
            key('V',False);check(m.get_editor_property('bDrillMode'),'V can activate again after cooldown')
            m.reset_field();check(not actors(unreal.MiningDrillDustBurst),'Reset clears all contact effects')
            key('G',True);s.update(phase='g',next=now+.1)
        elif phase=='g':
            key('G',False);check(pc.mining_gui.ability_g.get_editor_property('bUsed'),'Existing G red slash interaction preserved')
            check(not str(pc.mining_gui.coin_text.get_text()),'Coin value remains empty after all interactions')
            finish(True)
    except Exception:
        report['error']=traceback.format_exc();finish(False)
check(levels.load_level('/Game/MiningPrototype/Maps/Lvl_MiningLab'),'Existing mining lab loads')
helper.start_floating_preview()
busy=False
def guarded(delta):
    global busy
    if busy:return
    busy=True
    try:tick(delta)
    finally:busy=False
callback=unreal.register_slate_post_tick_callback(guarded)
def quit_after(delta):
    if s['phase']=='exit' and time.monotonic()>s['next'] and editor.get_game_world() is None:
        unreal.unregister_slate_post_tick_callback(exit_callback)
        unreal.SystemLibrary.execute_console_command(editor.get_editor_world(),'QUIT_EDITOR')
exit_callback=unreal.register_slate_post_tick_callback(quit_after)
