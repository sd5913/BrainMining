"""Exercise real G/LMB input, Chaos contacts, a three-second fuse and radial knockback in PIE."""
import json
import time
import traceback
from pathlib import Path
import unreal

unreal.EditorPythonScripting.set_keep_python_script_alive(True)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
helper = unreal.MiningGUIEditorLibrary
perf = unreal.get_default_object(unreal.load_class(None, '/Script/UnrealEd.EditorPerformanceSettings'))
old_throttle = perf.get_editor_property('bThrottleCPUWhenNotForeground')
perf.set_editor_property('bThrottleCPUWhenNotForeground', False)
out = Path(unreal.Paths.project_saved_dir())/'Validation/Bomb'
out.mkdir(parents=True, exist_ok=True)
report = {'passed': False, 'checks': [], 'samples': {}}
state = {'phase': 'setup', 'started': time.monotonic(), 'busy': False}
base = unreal.Vector(0., -20000., 6000.)

def vec(value): return [value.x, value.y, value.z]
def valid(value): return value is not None and unreal.SystemLibrary.is_valid(value)
def check(value, label):
    assert value, label
    report['checks'].append(label)
def key(name, down): helper.send_preview_key(state['pc'], name, down)
def press(name): key(name, True); key(name, False)
def shot(name):
    check(helper.capture_preview(state['pc'], str(out/(name+'.png'))), 'Captured '+name)
def spawn(cls, tag, location, scale=unreal.Vector(1., 1., 1.)):
    # The development console spawns directly in the PIE world; the editor
    # level and its saved actor list are never changed.
    before = set(unreal.GameplayStatics.get_all_actors_of_class(state['world'], cls))
    unreal.SystemLibrary.execute_console_command(state['world'], 'Summon '+cls.static_class().get_path_name(), state['pc'])
    created = [a for a in unreal.GameplayStatics.get_all_actors_of_class(state['world'], cls) if a not in before]
    assert len(created) == 1, 'PIE console spawn failed: '+str(cls)
    actor = created[0]
    actor.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    actor.set_actor_location(location, False, True)
    actor.set_actor_rotation(unreal.Rotator(0., 0., 0.), True)
    actor.set_actor_scale3d(scale)
    actor.set_editor_property('tags', [tag, 'BM_PlayerEffect'])
    return actor
def cube(tag, location, scale):
    actor = spawn(unreal.StaticMeshActor, tag, location, scale)
    component = actor.static_mesh_component
    component.set_mobility(unreal.ComponentMobility.MOVABLE)
    component.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Cube'))
    component.set_collision_profile_name('BlockAll')
    return actor
def move_player(location):
    state['pawn'].set_actor_location(location, False, True)
    state['movement'].stop_movement_immediately()
    state['pc'].set_control_rotation(unreal.Rotator(0., 0., 0.))
def finish(passed, reason=''):
    unreal.unregister_slate_post_tick_callback(callback)
    if editor.get_game_world() and 'pc' in state:
        for name in ['G', 'V', 'LeftMouseButton', 'SpaceBar']: key(name, False)
    perf.set_editor_property('bThrottleCPUWhenNotForeground', old_throttle)
    report.update(passed=passed, reason=reason, elapsed=time.monotonic()-state['started'])
    (out/'runtime.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    unreal.log('BOMB_VERIFICATION '+json.dumps(report))
    if editor.get_game_world(): levels.editor_request_end_play()

def tick(delta):
    if state['busy']: return
    state['busy'] = True
    try:
        if time.monotonic()-state['started'] > 150: raise RuntimeError('Bomb test timeout: '+state['phase'])
        phase = state['phase']
        if phase == 'setup':
            world = editor.get_game_world()
            if not world: return
            pc = unreal.GameplayStatics.get_player_controller(world, 0)
            pawn = unreal.GameplayStatics.get_player_pawn(world, 0)
            if not pc or not pc.mining_gui or not pawn: return
            halls = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningExhibitHall)
            if any(not h.get_editor_property('bReady') for h in halls): return
            tool = pawn.get_component_by_class(unreal.MiningBombToolComponent)
            movement = pawn.get_component_by_class(unreal.MiningCharacterMovementComponent)
            mining = pawn.get_component_by_class(unreal.MiningComponent)
            check(tool is not None, 'Latest player blueprint inherits the bomb tool')
            state.update(world=world, pc=pc, pawn=pawn, tool=tool, movement=movement, mining=mining, gui=pc.mining_gui)
            report['character_class'] = pawn.get_class().get_path_name()
            check(pc.mining_gui.ability_g.get_editor_property('bAvailable'), 'Latest main level shows restored bomb G beside drill V')
            check(not tool.get_editor_property('bEquipped') and tool.settings.fuse_seconds == 3., 'Bomb starts stowed with a three-second throw fuse')
            check(all(c.green_segment4 for c in [pc.mining_gui.pickaxe_upgrade, pc.mining_gui.vortex_upgrade, pc.mining_gui.blast_upgrade]), 'Four-level upgrade rings are preserved')
            cube('BombValidationFloor', base-unreal.Vector(0., 0., 20.), unreal.Vector(100., 100., .4))
            state['wall'] = cube('BombValidationWall', base+unreal.Vector(850., 0., 150.), unreal.Vector(.3, 10., 3.))
            state['body'] = cube('BombValidationBody', base+unreal.Vector(-4000., 0., 20.), unreal.Vector(.3, .3, .3))
            light = spawn(unreal.PointLight, 'BombValidationLight', base+unreal.Vector(600., 0., 500.))
            lamp = light.get_component_by_class(unreal.PointLightComponent)
            lamp.set_intensity(4500.)
            lamp.set_attenuation_radius(4000.)
            height = pawn.get_component_by_class(unreal.CapsuleComponent).get_scaled_capsule_half_height()
            state['height'] = height
            move_player(base+unreal.Vector(0., 0., height+2.))
            state.update(phase='settle', at=unreal.GameplayStatics.get_time_seconds(world))
            return
        now = unreal.GameplayStatics.get_time_seconds(state['world'])
        tool = state['tool']; pawn = state['pawn']; mining = state['mining']
        if phase == 'settle' and now-state['at'] >= 2.:
            press('G'); state.update(phase='equipped', at=now)
        elif phase == 'equipped' and now-state['at'] >= .15:
            check(tool.get_editor_property('bEquipped') and not mining.get_editor_property('bDrillMode'), 'Real G key equips bomb without mining or drilling')
            models = pawn.get_components_by_class(unreal.StaticMeshComponent)
            held = next(m for m in models if m.get_name() == 'BombModel')
            check(held.is_visible() and held.static_mesh is not None, 'Artist bomb is visible in the camera mount')
            check(not next(m for m in models if m.get_name() == 'PickaxeModel').is_visible(), 'Equipping bomb stows the pickaxe model')
            shot('equipped')
            key('LeftMouseButton', True)
            state.update(phase='thrown', at=now)
        elif phase == 'thrown' and now-state['at'] >= .1:
            bomb = tool.last_thrown_bomb
            check(valid(bomb), 'Actual left click launches a physical bomb')
            check(not tool.get_editor_property('bEquipped') and not mining.is_mining_active(), 'Throw returns to pickaxe and does not carry the click into mining')
            check(bomb.collision.is_simulating_physics() and bomb.collision.is_gravity_enabled(), 'Thrown bomb uses engine rigid-body physics and gravity')
            check(not bomb.get_editor_property('bHidden'), 'Area visibility rules leave the active thrown bomb visible')
            check(bomb.collision.get_collision_enabled() == unreal.CollisionEnabled.QUERY_AND_PHYSICS, 'Bomb has query and physical collision')
            state.update(bomb=bomb, launch=bomb.launched_at, velocity=bomb.get_velocity(), position=bomb.get_actor_location(), at=now, phase='gravity')
        elif phase == 'gravity' and now-state['at'] >= .15:
            bomb = state['bomb']
            check(bomb.get_velocity().z < state['velocity'].z-80., 'Gravity decreases vertical velocity during flight')
            check(bomb.get_actor_location().x > state['position'].x+80., 'Bomb travels forward rather than teleporting to a target')
            check(len(unreal.GameplayStatics.get_all_actors_of_class(state['world'], unreal.MiningBombActor)) == 1, 'Holding left mouse does not spawn extra bombs')
            key('LeftMouseButton', False)
            shot('gravity-flight')
            state['phase'] = 'contact'
        elif phase == 'contact' and now-state['launch'] >= 2.:
            bomb = state['bomb']
            check(valid(bomb) and bomb.contact_count > 0, 'Bomb contacts the wall or floor and remains intact before its fuse expires')
            check(bomb.get_actor_location().z >= base.z+8., 'Collision keeps the physical bomb above the floor')
            pos = bomb.get_actor_location()
            move_player(unreal.Vector(pos.x-220., pos.y, base.z+state['height']+1.))
            state['body'].set_actor_location(unreal.Vector(pos.x, pos.y+190., base.z+20.), False, True)
            body_component = state['body'].static_mesh_component
            body_component.set_collision_profile_name('PhysicsActor')
            body_component.set_simulate_physics(True)
            state.update(phase='await-blast', nearby=pawn.get_actor_location(), body_before=state['body'].get_actor_location())
        elif phase == 'await-blast' and tool.explosion_count > 0:
            check(abs(tool.last_explosion_time-state['launch']-3.) < .15, 'Fuse detonates three seconds after launch, independent of collisions')
            check(not valid(tool.last_thrown_bomb), 'Detonation removes the bomb actor and model')
            effects = unreal.GameplayStatics.get_all_actors_of_class(state['world'], unreal.MiningBombExplosion)
            check(len(effects) == 1 and effects[0].fire.get_instance_count() > 0 and effects[0].smoke.get_instance_count() > 0, 'Stylized fire and smoke survive independently of the destroyed bomb')
            check(not effects[0].get_editor_property('bHidden'), 'Area visibility rules leave the independent explosion visible')
            report['samples']['blast'] = {'fuse': tool.last_explosion_time-state['launch'], 'location': vec(tool.last_explosion_location), 'player_before': vec(state['nearby'])}
            state.update(phase='flash', detonation=tool.last_explosion_time, effect=effects[0].fire)
        elif phase == 'flash' and now-state['detonation'] >= .08:
            check(state['effect'].get_instance_count() > 0 and state['effect'].is_visible(), 'Faceted blast presentation is visible promptly')
            report['samples']['blast']['activation_latency'] = now-state['detonation']
            direction = (state['nearby']-tool.last_explosion_location).normal()
            check(pawn.get_velocity().dot(direction) > 100., 'Nearby player receives outward explosion velocity through CharacterMovement')
            shot('blast-flash')
            state['phase'] = 'smoke'
        elif phase == 'smoke' and now-state['detonation'] >= .6:
            check(pawn.get_actor_location().distance(state['nearby']) > 80., 'Player physically moves away after the shockwave')
            check(state['body'].get_actor_location().distance(state['body_before']) > 40., 'Shockwave also pushes nearby simulated debris')
            report['samples']['knockback'] = {'player_after':vec(pawn.get_actor_location()), 'velocity':vec(pawn.get_velocity()), 'displacement':pawn.get_actor_location().distance(state['nearby'])}
            check(state['gui'].ability_g.cooldown_remaining == 0., 'Bomb artwork never enters a cooldown state')
            shot('blast-smoke')
            move_player(base+unreal.Vector(0., 0., state['height']+1.))
            state.update(phase='second-equip', at=now)
        elif phase == 'second-equip' and now-state['at'] >= .2:
            press('G'); state.update(phase='second-throw', at=now)
        elif phase == 'second-throw' and now-state['at'] >= .15:
            check(tool.get_editor_property('bEquipped'), 'Bomb can be equipped again immediately without cooldown')
            press('LeftMouseButton'); state.update(phase='move-far', at=now)
        elif phase == 'move-far' and now-state['at'] >= .1:
            check(valid(tool.last_thrown_bomb), 'Second independent fuse starts on the next throw')
            state['second_launch'] = tool.last_thrown_bomb.launched_at
            move_player(base+unreal.Vector(-1800., 0., state['height']+1.))
            state.update(phase='far', far_position=pawn.get_actor_location())
        elif phase == 'far' and tool.explosion_count == 2 and now-tool.last_explosion_time >= .12:
            check(pawn.get_actor_location().distance(state['far_position']) < 8., 'Player beyond the explosion radius is not knocked back')
            move_player(base+unreal.Vector(0., 0., state['height']+1.))
            press('G'); state.update(phase='occlusion-equip', at=now)
        elif phase == 'occlusion-equip' and now-state['at'] >= .15:
            press('LeftMouseButton'); state.update(phase='occlusion-place', at=now)
        elif phase == 'occlusion-place' and now-state['at'] >= .1:
            bomb=tool.last_thrown_bomb
            check(valid(bomb), 'Third bomb supports a wall-occlusion check')
            # Keep the bomb physically resting on the floor for a deterministic
            # blocked blast; the real three-second timer remains untouched.
            bomb.set_actor_location(base+unreal.Vector(600., 0., 12.), False, True)
            bomb.collision.set_physics_linear_velocity(unreal.Vector())
            bomb.collision.set_physics_angular_velocity_in_degrees(unreal.Vector())
            state['wall'].set_actor_location(base+unreal.Vector(400., 0., 150.), False, True)
            move_player(base+unreal.Vector(200., 0., state['height']+1.))
            state.update(phase='occlusion', blocked_position=pawn.get_actor_location())
        elif phase == 'occlusion' and tool.explosion_count == 3 and now-tool.last_explosion_time >= .12:
            check(pawn.get_actor_location().distance(state['blocked_position']) < 8., 'Solid wall blocks the nearby bomb shockwave')
            press('G'); state.update(phase='switch-drill', at=now)
        elif phase == 'switch-drill' and now-state['at'] >= .15:
            check(tool.get_editor_property('bEquipped'), 'G can select bomb after detonation')
            press('V'); state.update(phase='drill', at=now)
        elif phase == 'drill' and now-state['at'] >= .15:
            check(mining.get_editor_property('bDrillMode') and not tool.get_editor_property('bEquipped'), 'V switches cleanly from bomb to the existing drill')
            press('G'); state.update(phase='from-drill', at=now)
        elif phase == 'from-drill' and now-state['at'] >= .15:
            check(tool.get_editor_property('bEquipped') and not mining.get_editor_property('bDrillMode'), 'G stows the drill and selects bomb')
            check(mining.get_drill_cooldown_remaining() > 0., 'Bomb selection preserves the existing drill cooldown deadline')
            press('G'); state.update(phase='tab-open', at=now)
        elif phase == 'tab-open' and now-state['at'] >= .15:
            check(not tool.get_editor_property('bEquipped'), 'Pressing G again returns to pickaxe')
            key('Tab', True); state.update(phase='tab-block', at=now)
        elif phase == 'tab-block' and now-state['at'] >= .35:
            press('G'); state.update(phase='tab-check', at=now)
        elif phase == 'tab-check' and now-state['at'] >= .15:
            check(not tool.get_editor_property('bEquipped') and state['pc'].show_mouse_cursor, 'Upgrade UI blocks bomb equip behind the pointer')
            shot('ui-four-upgrades')
            helper.send_preview_focused_key(state['pc'], 'Tab', False)
            check(not state['pc'].show_mouse_cursor and not state['pc'].is_look_input_ignored(), 'Held Tab still restores mouse look on release')
            finish(True)
    except Exception:
        finish(False, traceback.format_exc())
    finally:
        state['busy'] = False

assert not editor.get_game_world(), 'Do not interrupt a user preview'
callback = unreal.register_slate_post_tick_callback(tick)
helper.start_floating_preview()
