"""Run in an editor launched with -ExecutePythonScript. Only changes the PIE world."""
import json
import time
import traceback
from pathlib import Path
import unreal

unreal.EditorPythonScripting.set_keep_python_script_alive(True)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
output = Path(unreal.Paths.project_saved_dir()) / 'Validation' / 'MiningToolEffects'
output.mkdir(parents=True, exist_ok=True)
report = {'passed': False, 'checks': [], 'scope': 'Existing mining with test109 pickaxe, Concrete Debris shapes and matching surface material; PIE only'}
state = {'phase': 'start', 'started': time.monotonic(), 'next': 0}
performance = unreal.get_default_object(unreal.load_class(None, '/Script/UnrealEd.EditorPerformanceSettings'))
old_throttle = performance.get_editor_property('bThrottleCPUWhenNotForeground')
performance.set_editor_property('bThrottleCPUWhenNotForeground', False)

def check(value, label):
    assert value, label
    report['checks'].append(label)

def finish(passed, reason):
    unreal.unregister_slate_post_tick_callback(callback)
    performance.set_editor_property('bThrottleCPUWhenNotForeground', old_throttle)
    report.update(passed=passed, reason=reason, elapsed=time.monotonic()-state['started'])
    (output / 'runtime.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    unreal.log('MINING_TOOL_EFFECTS_RESULT ' + json.dumps(report))
    if editor.get_game_world():
        levels.editor_request_end_play()

def aim_at_rock():
    origin, extent = state['rock'].get_actor_bounds(False)
    state['pawn'].set_actor_location(unreal.Vector(origin.x-extent.x-130, origin.y, origin.z), False, True)
    state['controller'].set_control_rotation(unreal.Rotator(pitch=0, yaw=0, roll=0))

def bursts():
    return unreal.GameplayStatics.get_all_actors_of_class(state['world'], state['burst_class'])

def tick(delta):
    try:
        now = time.monotonic()
        if now-state['started'] > 180:
            raise RuntimeError('PIE validation timed out: ' + state['phase'])
        if now < state['next']:
            return
        phase = state['phase']
        if phase == 'start':
            check(levels.load_level('/Game/MiningPrototype/Maps/Lvl_MiningLab'), 'Existing mining lab loads')
            levels.editor_request_begin_play()
            state.update(phase='setup', next=now+3)
        elif phase == 'setup':
            world = editor.get_game_world()
            if not world:
                return
            pawn = unreal.GameplayStatics.get_player_character(world, 0)
            if not pawn:
                return
            mining = pawn.get_component_by_class(unreal.MiningComponent)
            effects = pawn.get_component_by_class(unreal.MiningToolEffectsComponent)
            check(mining and effects and effects.pickaxe_mesh and effects.pickaxe_pivot, 'Existing player has mining and camera-mounted pickaxe')
            check(effects.pickaxe_mesh.static_mesh.get_path_name() == '/Game/Pickaxe/SM_Pickaxe.SM_Pickaxe', 'Original test109 pickaxe mesh is used')
            check(effects.pickaxe_mesh.get_collision_profile_name() == 'NoCollision', 'Pickaxe cannot obstruct mining or movement')
            check(effects.pickaxe_pivot.get_attach_parent() == pawn.get_component_by_class(unreal.CameraComponent), 'Pickaxe follows the first-person camera')
            curve = unreal.load_asset('/Game/Pickaxe/CF_PickaxeSwing')
            check(abs(curve.get_float_value(0.2)-75.595901) < 0.001, 'Original swing curve amplitude is preserved')
            rock_list = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.DiggableRock)
            check(bool(rock_list), 'Existing diggable rock is present')
            state.update(world=world, pawn=pawn, mining=mining, effects=effects, controller=unreal.GameplayStatics.get_player_controller(world, 0), rock=rock_list[0], burst_class=unreal.MiningDebrisBurst)
            mining.reset_field()
            pawn.get_component_by_class(unreal.CharacterMovementComponent).set_movement_mode(unreal.MovementMode.MOVE_NONE)
            mining.interval = 1.0
            aim_at_rock()
            state.update(phase='hit', next=now+0.3)
        elif phase == 'hit':
            state.update(swings=state['effects'].swing_count, particles=state['effects'].debris_burst_count, cuts=state['rock'].successful_cuts, material=state['rock'].rock_mesh.get_material(0))
            check(state['mining'].try_mine(), 'Actual TryMine successfully removes rock')
            check(state['rock'].successful_cuts == state['cuts']+1, 'Presentation does not add extra cuts')
            check(state['effects'].swing_count == state['swings']+1, 'Accepted mining attempt starts one swing')
            check(not state['mining'].try_mine(), 'Existing cooldown rejects immediate repeat')
            check(state['effects'].swing_count == state['swings']+1, 'Cooldown does not produce duplicate swings')
            state.update(phase='swing', next=now+0.10)
        elif phase == 'swing':
            check(state['effects'].get_editor_property('bSwinging') and state['effects'].pickaxe_pivot.relative_rotation.pitch < -1, 'Pickaxe visibly follows the swing curve during play')
            state.update(phase='particles', next=now+0.14)
        elif phase == 'particles':
            check(state['effects'].debris_burst_count == state['particles']+1, 'One successful dig creates one debris burst at the swing impact')
            active = bursts()
            check(len(active) == 1, 'Soil debris burst spawns in the PIE world')
            chips = active[0].get_components_by_class(unreal.StaticMeshComponent)
            check(30 <= len(chips) <= 42, 'Each hit produces 30 to 42 soil chunks instead of eight tiny chips')
            check(all(c.static_mesh.get_path_name().startswith('/Game/Debris/Meshes/') for c in chips), 'Concrete Debris pack supplies the irregular chunk shapes')
            check(all(c.get_material(0) == state['material'] for c in chips), 'Every chunk uses the actual mined surface material instead of grey concrete')
            sizes = []
            for c in chips:
                ext = c.static_mesh.get_bounds().box_extent
                sizes.append(2*max(ext.x,ext.y,ext.z)*c.get_editor_property('relative_scale3d').x)
            report['chunk_sizes_cm'] = sizes
            report['matched_material'] = state['material'].get_path_name()
            state['first_signature'] = [(c.static_mesh.get_path_name(), c.get_editor_property('relative_scale3d').x) for c in chips]
            check(sum(s>=14 for s in sizes)>=8 and sum(s<7 for s in sizes)>=8, 'Large clods and small crumbs are mixed in one spray')
            check(all(c.is_simulating_physics() for c in chips), 'All soil chunks simulate physics')
            # ECollisionResponse::ECR_Ignore is 0; avoid Python enum aliases varying between engine versions.
            check(all(c.get_collision_response_to_channel(unreal.CollisionChannel.ECC_VISIBILITY).value == 0 for c in chips), 'Debris cannot intercept the next mining trace')
            check(all(c.get_collision_response_to_channel(unreal.CollisionChannel.ECC_PAWN).value == 0 for c in chips), 'Debris cannot push or block the player')
            report['chip_velocities'] = [str(c.get_physics_linear_velocity()) for c in chips]
            check(all(c.get_physics_linear_velocity().length() > 1 for c in chips), 'All chunks spray outward under physics')
            unreal.SystemLibrary.execute_console_command(state['world'], 'Shot SHOWUI filename="'+str(output/'mining_hit.png')+'" -nosuffix', state['controller'])
            state.update(phase='reset', next=now+0.1)
        elif phase == 'reset':
            state['mining'].reset_field()
            check(not bursts() and not state['effects'].get_editor_property('bSwinging'), 'Reset removes debris and returns the pickaxe to rest')
            aim_at_rock()
            state.update(phase='pending_reset', next=now+0.3)
        elif phase == 'pending_reset':
            check(state['mining'].try_mine(), 'Existing mining still works after reset')
            state['particles'] = state['effects'].debris_burst_count
            state['mining'].reset_field()
            state.update(phase='check_reset', next=now+0.35)
        elif phase == 'check_reset':
            check(state['effects'].debris_burst_count == state['particles'] and not bursts(), 'Reset cancels pending impact timers')
            aim_at_rock()
            state['controller'].set_control_rotation(unreal.Rotator(pitch=90, yaw=180, roll=0))
            state.update(phase='miss', next=now+0.3)
        elif phase == 'miss':
            check(not state['mining'].try_mine(), 'Aiming away from rock does not cut geometry')
            state.update(phase='check_miss', next=now+0.3)
        elif phase == 'check_miss':
            check(state['effects'].debris_burst_count == state['particles'], 'Misses do not spawn fake mining debris')
            state['mining'].reset_field()
            state['rock'].rock_material = unreal.load_asset('/Game/MiningPrototype/Materials/DirectRooms/MI_Diggable_Warm')
            state['mining'].reset_field()
            aim_at_rock()
            state.update(phase='warm_hit',next=now+0.3)
        elif phase == 'warm_hit':
            state['material'] = state['rock'].rock_mesh.get_material(0)
            check(state['mining'].try_mine(), 'Mining works with a different warm soil surface')
            state.update(phase='warm_check',next=now+0.26)
        elif phase == 'warm_check':
            if not bursts() and now-state['next'] < 3:
                return
            check(all(c.get_material(0)==state['material'] for a in bursts() for c in a.get_components_by_class(unreal.StaticMeshComponent)) and bool(bursts()), 'Warm soil tint is inherited automatically')
            signature = [(c.static_mesh.get_path_name(), c.get_editor_property('relative_scale3d').x) for a in bursts() for c in a.get_components_by_class(unreal.StaticMeshComponent)]
            check(signature != state['first_signature'], 'Successive digs use different randomized debris shapes and sizes')
            state['rock'].rock_material = unreal.load_asset('/Game/MiningPrototype/Materials/HollowRegion/MI_HollowRegionRock')
            state['mining'].reset_field()
            aim_at_rock()
            state.update(phase='blue_hit',next=now+0.3)
        elif phase == 'blue_hit':
            state['material'] = state['rock'].rock_mesh.get_material(0)
            check(state['mining'].try_mine(), 'Mining works with the hollow-region soil surface')
            state.update(phase='blue_check',next=now+0.26)
        elif phase == 'blue_check':
            if not bursts() and now-state['next'] < 3:
                return
            report['hollow_expected_material'] = state['material'].get_path_name()
            report['hollow_chunk_materials'] = list(set(c.get_material(0).get_path_name() for a in bursts() for c in a.get_components_by_class(unreal.StaticMeshComponent)))
            check(all(c.get_material(0)==state['material'] for a in bursts() for c in a.get_components_by_class(unreal.StaticMeshComponent)) and bool(bursts()), 'Hollow-region soil material is inherited automatically')
            state.update(phase='expiry',next=now,expiry_at=unreal.GameplayStatics.get_time_seconds(state['world'])+2.3)
        elif phase == 'expiry':
            # Lifespans use simulation time, which can lag wall time while new shaders compile.
            if unreal.GameplayStatics.get_time_seconds(state['world']) < state['expiry_at']:
                return
            check(not bursts(), 'Physical chunks clean themselves up after their lifetime')
            state['mining'].reset_field()
            report['swings'] = state['effects'].swing_count
            report['debris_bursts'] = state['effects'].debris_burst_count
            finish(True, 'Dense mixed-size soil debris matches each mined material; physics, cooldown, miss, reset and cleanup pass')
    except Exception:
        finish(False, traceback.format_exc())

callback = unreal.register_slate_post_tick_callback(tick)
unreal.log('MINING_TOOL_EFFECTS_VALIDATION_ARMED')
