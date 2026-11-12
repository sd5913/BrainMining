"""Record production collectible reveals in isolated MainGame PIE.

Set EMERGENCE_REVIEW_ITEM=pigeon or subway. The pigeon uses ordinary pickaxe
actions and swept pawn traversal. The train uses small explicit soil-cut fixtures
to stop at 40% exposure, then observes the unmodified production animation clock.
No map, player save, Steam achievement, or browser is modified.
"""
import json
import os
import time
import traceback
from datetime import datetime
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
MODE = os.environ.get('EMERGENCE_REVIEW_ITEM', 'pigeon')
assert MODE in ('pigeon', 'subway')
assert all(flag in unreal.SystemLibrary.get_command_line().lower()
           for flag in ('-nosteam', '-miningqa', '-userdir='))
OUT = ROOT / 'Saved/Review/EmergenceFix' / (MODE + '-' + datetime.now().strftime('%Y%m%d-%H%M%S'))
(OUT / 'frames').mkdir(parents=True)
(OUT.parent / ('latest-' + MODE + '.txt')).write_text(str(OUT), encoding='utf8')
ed = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
H = unreal.MiningGUIEditorLibrary
s = dict(phase='load', next=0, started=time.monotonic(), busy=False,
         frames=[], samples=[], checks=[], mine_attempts=0, stage='approach')
unreal.EditorPythonScripting.set_keep_python_script_alive(True)


def check(ok, label):
    assert ok, label
    s['checks'].append(label)


def actors(cls):
    return unreal.GameplayStatics.get_all_actors_of_class(s['world'], cls)


def vec(v):
    return list(v.to_tuple())


def eye():
    return s['pawn'].get_component_by_class(unreal.CameraComponent).get_world_location()


def pose(location, target):
    offset = eye() - s['pawn'].get_actor_location()
    s['pawn'].set_actor_location(location - offset, False, True)
    s['pc'].set_control_rotation(unreal.MathLibrary.find_look_at_rotation(location, target))


def press(key):
    H.send_preview_key(s['pc'], key, True)
    H.send_preview_key(s['pc'], key, False)


def stage(name, delay=0):
    s.update(stage=name, phase=name, next=time.monotonic() + delay)


def save_report(error=None):
    report = dict(passed=error is None, error=error, item=MODE,
                  checks=s['checks'], samples=s['samples'], frames=s['frames'],
                  mine_attempts=s['mine_attempts'],
                  first_flock=dict(mine_attempts=s.get('flock_attempts'), successful_cuts=s.get('flock_cuts'),
                                   accepted_strikes=s.get('flock_accepted_strikes')) if MODE=='pigeon' else None,
                  source_map='/Game/MainGame/Maps/Lvl_MainGame',
                  scope='Isolated campaign checkpoint; actual rendered production animation. '
                        + ('Mineral-inventory fixture purchases the area-appropriate level-3 pickaxe through normal progression; '
                           'ordinary pickaxe actions and swept traversal; soil hardness is unchanged.' if MODE == 'pigeon' else
                           'Incremental cuts open a visible passage from the real outer surface inward and stop at partial exposure; '
                           'no further cuts during pickup delay.'))
    (OUT / 'recording.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf8')


def finish(error=None):
    if s.get('done'):
        return
    s['done'] = True
    if s.get('mining'):
        s['mining'].cancel_mining()
    if s.get('audio'):
        try:
            unreal.AudioMixerLibrary.stop_recording_output(s['world'], unreal.AudioRecordingExportType.WAV_FILE,
                                                           'live-audio', str(OUT))
        except Exception:
            pass
    save_report(error)
    unreal.unregister_slate_post_tick_callback(handle)
    levels.editor_request_end_play()
    cleanup = dict(at=time.monotonic() + 3)

    def close(dt):
        if ed.get_game_world() or time.monotonic() < cleanup['at']:
            return
        if not cleanup.get('unloaded'):
            s.clear()
            s['done'] = True
            levels.load_level('/Engine/Maps/Entry')
            cleanup.update(unloaded=True, at=time.monotonic() + 2)
            return
        unreal.unregister_slate_post_tick_callback(cleanup['handle'])
        unreal.SystemLibrary.quit_editor()
    cleanup['handle'] = unreal.register_slate_post_tick_callback(close)
    unreal.log('EMERGENCE_REVIEW_FINISHED ' + str(OUT) + ' passed=' + str(error is None))


def record(now):
    item = s['item']
    world_time = unreal.GameplayStatics.get_time_seconds(s['world'])
    sample = dict(seconds=now-s['record_started'], world_seconds=world_time-s['world_start'],
                  stage=s['stage'], exposure=float(item.get_editor_property('Exposure')), released=bool(item.get_editor_property('bReleased')),
                  pickup_ready=bool(item.is_collectible()), position=vec(item.get_actor_location()),
                  cuts=int(s['host'].get_editor_property('SuccessfulCuts')), eye=vec(eye()),
                  flock_bursts=int(s['field'].get_editor_property('FlockBursts')), physics=bool(s['body_component'].is_simulating_physics()))
    if MODE == 'subway':
        sample['crowd_heads'] = sum(part.is_visible() for part in s['crowd_heads'])
        # Each passenger has three parts whose visibility is driven identically.
        sample['crowd_parts'] = sample['crowd_heads'] * 3
    else:
        birds = unreal.GameplayStatics.get_all_actors_with_tag(s['world'], 'CollectionFlyingBird')
        sample['birds'] = [dict(name=b.get_name(), position=vec(b.get_actor_location()),
                                forward=vec(b.get_actor_forward_vector()), hidden=bool(b.get_editor_property('hidden')))
                           for b in birds]
    s['samples'].append(sample)
    if now >= s.get('next_live_state', 0):
        mining = s['mining']
        target_rock = mining.get_editor_property('TargetRock')
        live = dict(sample, phase=s['phase'], pawn=vec(s['pawn'].get_actor_location()),
                    route_index=s.get('route_index'), mine_attempts=s['mine_attempts'],
                    clearance_mode=s.get('clearance_mode', False),
                    last_mine_result=s.get('last_mine_result'),
                    mining_status=str(mining.get_editor_property('Status')),
                    mining_target=target_rock.get_name() if target_rock else None,
                    mining_active=bool(mining.is_mining_active()),
                    mining_reach=float(mining.get_editor_property('Reach')))
        if MODE == 'pigeon':
            live['waypoint'] = vec(s['route'][s['route_index']])
            # Report both the mining line and the production exit clearance.
            # A line hit is diagnostic; the production gate additionally uses a 14cm sweep.
            traces = {}
            origin = s['field'].get_bird_emergence_origin()
            for name, start, end, ignored in (
                    ('mining', eye(), eye()+(origin-eye()).normal()*live['mining_reach'], [s['pawn']]),
                    ('exit', origin, eye(), [s['pawn'], item])):
                hit = unreal.SystemLibrary.line_trace_single(s['world'], start, end,
                          unreal.TraceTypeQuery.ECC_VISIBILITY, True, ignored, unreal.DrawDebugTrace.NONE, True)
                values = hit.to_tuple() if hit else None
                traces[name] = dict(actor=values[9].get_name() if values and values[9] else None,
                                    impact=vec(values[5]) if values else None)
            live['trace'] = traces
        (OUT/'live-state.json').write_text(json.dumps(live, ensure_ascii=False, indent=2), encoding='utf8')
        s['next_live_state'] = now + 2
    if now >= s.get('next_frame', 0):
        name = 'frame-%06d.jpg' % len(s['frames'])
        check(H.capture_preview_video_frame(s['pc'], str(OUT/'frames'/name), 90), 'Rendered frame ' + name)
        s['checks'].pop()  # Keep the report focused on behavior, not each successful frame.
        s['frames'].append(dict(file=name, seconds=sample['seconds'], world_seconds=sample['world_seconds'], stage=s['stage']))
        s['next_frame'] = now + 1/30
    return sample


def prepare_route():
    placement = s['placement']
    front = placement.clue_transform.translation
    body = s['item'].get_actor_location()
    s.update(front=front, body=body, original=body, cuts_before=s['host'].get_editor_property('SuccessfulCuts'))
    if MODE == 'pigeon':
        # The short bank is mined toward the resident's actual exit origin.
        # The authored tunnel centerline is not an eye-height aiming path.
        report_path = Path(os.environ.get('PIGEON_DEPTH_REPORT', str(ROOT/'Saved/Validation/PigeonDeepSoil/authoring.json')))
        spec = json.loads(report_path.read_text(encoding='utf8'))
        check(spec.get('passed'), 'Deep-soil authoring report passed')
        route = [s['field'].get_bird_emergence_origin()]
        s.update(route=route, route_index=0, next_strike=0, flock_at=None,
                 progress_probe=None, clearance_mode=False, accepted_strikes=0)
        approach = placement.approach_transform.translation
        s['pawn'].set_actor_location(approach, False, True)
        s['pc'].set_control_rotation(unreal.MathLibrary.find_look_at_rotation(eye(), route[0]))
        check((body-front).length() > 50, 'Resident pigeon is authored inside the actual soil bank')
    else:
        spec = next(x for x in json.loads((ROOT/'Config/BranchCollectiblePlacements.json').read_text()) if x['id'] == 21)
        outward = unreal.Vector(*spec['normal'])
        s.update(outward=outward, next_cut=0, radius=35, first_crowd=None, ready_at=None,
                 cut_center=front-outward*10)
        # Observe beside and above the real sign, preserving every scene prop.
        # The outward offset keeps the viewpoint outside the uncut soil bank.
        side = unreal.Vector(-outward.y, outward.x, 0).normal()
        pose(front + outward*250 + side*220 + unreal.Vector(0,0,80), body)
    check(not s['item'].get_editor_property('bReleased'), 'Collectible begins buried')
    check(s['item'].get_editor_property('Exposure') < .05, 'Collectible model initially has negligible exposure')


def pigeon_step(now, sample):
    item = s['item']
    check((item.get_actor_location()-s['original']).length() < .1, 'Resident remains at its authored buried position')
    s['checks'].pop()
    if s['phase'] == 'approach':
        stage('digging', 2)
        return
    if s['phase'] == 'flock':
        if now-s['flock_at'] >= 4:
            stage('final_dig')
        return
    if s['phase'] == 'revealed':
        if now-s['revealed_at'] < 3:
            return
        check(s['field'].get_editor_property('FlockBursts') == 1, 'Exactly one flock burst occurred')
        check(2 <= s['flock_attempts'] <= 4, 'Flock emerges after about three ordinary pickaxe attempts')
        check(not s['body_component'].is_simulating_physics(), 'Resident has no physics pop-out')
        check(item.is_collectible(), 'Resident becomes collectible after actual excavation')
        directional = []
        for sample in s['samples']:
            for bird in sample.get('birds', []):
                if bird['hidden']:
                    continue
                delta = [sample['eye'][i]-bird['position'][i] for i in range(3)]
                distance = sum(v*v for v in delta)**.5
                if 100 < distance < 900:
                    directional.append(sum(-bird['forward'][i]*delta[i]/distance for i in range(3)))
        check(directional and max(directional) > .9, 'Flying pigeon beaks face the player along the exit flight')
        finish()
        return
    if sample['flock_bursts'] and s['flock_at'] is None:
        s.update(flock_at=now, flock_cuts=sample['cuts']-s['cuts_before'],
                 flock_attempts=s['mine_attempts'], flock_accepted_strikes=s['accepted_strikes'])
        s['mining'].cancel_mining()
        stage('flock')
        return
    if item.is_collectible() and s['flock_at'] is not None:
        s['mining'].cancel_mining()
        s['pc'].set_control_rotation(unreal.MathLibrary.find_look_at_rotation(eye(), s['body']))
        s['revealed_at'] = now
        stage('revealed')
        return
    route = s['route']
    index = s['route_index']
    waypoint = route[index]
    current = eye()
    while index < len(route)-1 and (waypoint-current).length() < 220:
        index += 1
        waypoint = route[index]
    s['route_index'] = index
    pawn_position = s['pawn'].get_actor_location()
    probe = s['progress_probe']
    if probe is None:
        s['progress_probe'] = dict(position=pawn_position, at=now, attempts=s['mine_attempts'])
        s['last_forward_progress'] = now
    elif now-probe['at'] >= 2:
        moved = (pawn_position-probe['position']).length()
        if moved >= 5:
            s.update(clearance_mode=False, last_forward_progress=now)
        s['progress_probe'] = dict(position=pawn_position, at=now, attempts=s['mine_attempts'])
    check(now-s['last_forward_progress'] < 45,
          'Ordinary excavation must advance the pawn within 45 seconds; inspect live-state.json')
    s['checks'].pop()
    target = waypoint
    s['pc'].set_control_rotation(unreal.MathLibrary.find_look_at_rotation(current, target))
    if now >= s['next_strike']:
        s['last_mine_result'] = bool(s['mining'].try_mine())
        s['accepted_strikes'] += int(s['last_mine_result'])
        s['mine_attempts'] += 1
        s['next_strike'] = now + .65
    distance = (waypoint-current).length()
    if distance > 180:
        direction = (waypoint-current).normal()
        target = s['pawn'].get_actor_location() + direction*min(7, distance-180)
        s['pawn'].set_actor_location(target, True, False)


def subway_step(now, sample):
    item = s['item']
    if s['phase'] not in ('subway_ready', 'subway_support', 'subway_fall', 'subway_collect', 'subway_collected'):
        check((item.get_actor_location()-s['original']).length() < .1 and not s['body_component'].is_simulating_physics(),
              'Train remains fixed during the three-second crowd presentation')
        s['checks'].pop()
    if s['phase'] == 'approach':
        stage('subway_dig', 2)
        return
    if s['phase'] == 'subway_dig':
        if sample['exposure'] >= .4:
            check(sample['exposure'] < .7, 'Reveal reached between 40 and 70 percent exposure')
            check(not sample['pickup_ready'], 'Pickup is still locked when doors start opening')
            s['reveal_cuts'] = sample['cuts']
            stage('crowd_wait')
        elif now >= s['next_cut']:
            # Start at the outer soil surface and expand inward. Cutting at
            # the buried carriage instead would create an invisible sealed cavity.
            s['host'].mine_sphere(s['cut_center'], s['radius'])
            s['radius'] += 10
            check(s['radius'] <= 275, 'Outer-surface partial-reveal cut remains bounded')
            s['next_cut'] = now + .6
            s['mine_attempts'] += 1
    elif s['phase'] == 'crowd_wait':
        if sample['crowd_parts'] and s['first_crowd'] is None:
            s['first_crowd'] = sample['world_seconds']
            press('E')
            check(not item.get_editor_property('bCollected'), 'An early E press cannot skip the crowd presentation')
        if sample['pickup_ready']:
            check(s['first_crowd'] is not None, 'Crowd emerges before pickup is enabled')
            elapsed = sample['world_seconds'] - s['first_crowd']
            check(2.8 <= elapsed <= 3.25, 'Pickup unlocks after about three seconds of crowd animation')
            check(sample['cuts'] == s['reveal_cuts'], 'No additional excavation is needed for pickup')
            s['ready_at'] = now
            stage('subway_ready')
    elif s['phase'] == 'subway_ready' and now-s['ready_at'] >= 3:
        check(item.is_collectible() and item.get_editor_property('Exposure') < .7, 'Partial train remains collectible without further excavation')
        s.update(before_fall=item.get_actor_location(), fall_at=now)
        stage('subway_support')
    elif s['phase'] == 'subway_support':
        p = item.get_actor_location()
        hit = unreal.SystemLibrary.line_trace_single(s['world'], p, p-unreal.Vector(0,0,750),
                    unreal.TraceTypeQuery.ECC_VISIBILITY, True, [item, s['pawn']], unreal.DrawDebugTrace.NONE, True)
        check(hit is not None, 'A real supporting surface is present below the train')
        values = hit.to_tuple()
        check(values[0] and isinstance(values[9], unreal.DiggableRock), 'Actual support below train is mineable soil')
        check(values[9].mine_sphere(values[5]+s['outward']*80-unreal.Vector(0,0,40), 260),
              'Excavating real lower support from the visible cavity succeeds')
        s.update(fall_at=now, support_actor=values[9].get_name())
        stage('subway_fall')
    elif s['phase'] == 'subway_fall':
        position = item.get_actor_location()
        s['pc'].set_control_rotation(unreal.MathLibrary.find_look_at_rotation(eye(), position))
        if now-s['fall_at'] < 5:
            return
        check(position.z < s['before_fall'].z-5, 'Removing support lets train settle downward instead of hanging in air')
        drift = position-s['before_fall']
        check(abs(drift.x)<.5 and abs(drift.y)<.5, 'Train settles vertically without an outward launch')
        check(item.is_collectible(), 'Settled train remains collectible')
        pose(position+s['outward']*210+unreal.Vector(0,0,65), position)
        stage('subway_collect', .6)
    elif s['phase'] == 'subway_collect':
        press('E')
        stage('subway_collected', 2)
    elif s['phase'] == 'subway_collected':
        check(item.get_editor_property('bCollected'), 'Normal E interaction collects the settled train')
        finish()


def tick(dt):
    if s.get('done') or s['busy']:
        return
    s['busy'] = True
    try:
        now = time.monotonic()
        assert now-s['started'] < 150, 'Review timeout at ' + s['phase']
        if s['phase'] == 'load':
            check(not ed.get_game_world(), 'No existing PIE session')
            check(levels.load_level('/Game/MainGame/Maps/Lvl_MainGame'), 'Production MainGame loaded')
            unreal.SystemLibrary.execute_console_command(ed.get_editor_world(), 'mining.SaveEnabled 0')
            H.start_floating_preview()
            s.update(phase='ready', next=now+5)
            return
        world = ed.get_game_world()
        if not world:
            return
        s['world'] = world
        pc = unreal.GameplayStatics.get_player_controller(world, 0)
        if not pc or not pc.get_controlled_pawn():
            return
        s.update(pc=pc, pawn=pc.get_controlled_pawn())
        if s.get('record_started'):
            sample = record(now)
        if now < s['next']:
            return
        if s['phase'] == 'ready':
            halls = actors(unreal.MiningExhibitHall)
            openings = actors(unreal.MiningOpening)
            if not halls or not halls[0].get_editor_property('bReady') or not openings:
                return
            if not openings[0].get_editor_property('bFinished'):
                openings[0].start_game()
                s['next'] = now+2
                return
            s['hall'] = halls[0]
            s['movement'] = s['pawn'].get_component_by_class(unreal.CharacterMovementComponent)
            s['movement'].disable_movement()
            pickup = next(a for a in actors(unreal.MiningStarterPickup) if a.equipment == unreal.MiningStarterEquipment.PICKAXE)
            pose(pickup.get_actor_location()+unreal.Vector(-100,0,60), pickup.get_actor_location())
            s.update(phase='equip', next=now+.5)
        elif s['phase'] == 'equip':
            room = actors(unreal.MiningStartRoom)[0]
            check(room.interact(pc), 'Pickaxe is obtained through its actual starter interaction')
            field = next(f for f in actors(unreal.MiningCollectionField) if f.get_editor_property('AuthoredPlacements'))
            placement = next(p for p in field.get_editor_property('AuthoredPlacements') if p.item_id == (3 if MODE == 'pigeon' else 21))
            area = next(i for i,a in enumerate(s['hall'].get_editor_property('Areas')) if str(a.tag) == str(placement.area_tag))
            check(H.prepare_collection_preview_area(pc, area, False), 'Correct campaign area activated')
            s.update(field=field, placement=placement, phase='prepare', next=now+2)
        elif s['phase'] == 'prepare':
            s['movement'].disable_movement()
            s['mining'] = s['pawn'].get_component_by_class(unreal.MiningComponent)
            s['mining'].select_pickaxe()
            if MODE == 'pigeon':
                progression = s['pawn'].get_component_by_class(unreal.MiningProgressionComponent)
                wallet = progression.get_wallet()
                for mineral in ('2', '4', '6', '9'):
                    wallet.add_mineral(mineral, 100)
                for level in range(progression.get_level(0)+1, 4):
                    check(progression.purchase(0, level), 'Isolated material fixture purchases pickaxe level '+str(level))
                check(progression.get_level(0) == 3, 'Review uses area-appropriate level-3 pickaxe without modifying soil hardness')
            s['item'] = next(i for i in s['field'].get_editor_property('Items') if i.get_editor_property('ItemId') == (3 if MODE == 'pigeon' else 21))
            s['host'] = s['placement'].host_rock
            s['body_component'] = s['item'].get_editor_property('Body')
            if MODE == 'subway':
                # Production creates unnamed components: only the 36 passenger
                # heads use Sphere; the 72 body/leg and eight door parts use Cube.
                # Inspect registered components instead of protected EmergenceParts.
                s['crowd_heads'] = [part for part in s['item'].get_components_by_class(unreal.StaticMeshComponent)
                                    if part.static_mesh and part.static_mesh.get_path_name() == '/Engine/BasicShapes/Sphere.Sphere']
                check(len(s['crowd_heads']) == 36, 'All 36 production passenger heads are identified by their Sphere mesh')
            prepare_route()
            H.resize_preview_viewport(pc, 1280, 720)
            unreal.SystemLibrary.execute_console_command(world, 'DisableAllScreenMessages')
            s.update(phase='begin', next=now+3)
        elif s['phase'] == 'begin':
            s.update(record_started=now, world_start=unreal.GameplayStatics.get_time_seconds(world))
            try:
                unreal.AudioMixerLibrary.start_recording_output(world, 160)
                s['audio'] = True
            except Exception:
                pass
            stage('approach')
        elif MODE == 'pigeon':
            pigeon_step(now, sample)
        else:
            subway_step(now, sample)
    except Exception:
        finish(traceback.format_exc())
    finally:
        s['busy'] = False


handle = unreal.register_slate_post_tick_callback(tick)
