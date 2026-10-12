"""Assemble source levels into a new persistent campaign; never save source maps.

Run in the full editor with -ExecutePythonScript. Config/CampaignLayout.json is
the assembly recipe. Use -CampaignOutput=/Game/Campaign/Maps/Lvl_FullJourney_V02
to rebuild from updated sources while preserving the earlier assembly.
"""
import bisect
import hashlib
import json
import math
import re
import runpy
import shutil
from datetime import datetime
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir())
layout = json.loads((root / 'Config/CampaignLayout.json').read_text(encoding='utf-8'))
if layout.get('continuous'):
    runpy.run_path(str(root/'Scripts/create_continuous_campaign.py'),run_name='__main__')
    raise SystemExit(0)
override = re.search(r'-CampaignOutput=(/Game/[\w/]+)', unreal.SystemLibrary.get_command_line())
destination = override.group(1) if override else layout['map']
assets = unreal.EditorAssetLibrary
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
rebuild = '-CampaignRebuild' in unreal.SystemLibrary.get_command_line().split()
assert destination.startswith('/Game/Campaign/Maps/')
assert destination not in [s['source'] for s in layout['stages']]
if assets.does_asset_exist(destination):
    if not rebuild:
        raise RuntimeError('Assembly exists. Use -CampaignRebuild (backup first), or -CampaignOutput=/Game/Campaign/Maps/Lvl_FullJourney_V02.')
    old_file = root/'Content'/(destination[6:]+'.umap')
    backup = root/'Saved/CampaignBackups'/datetime.now().strftime('%Y%m%d-%H%M%S')
    backup.mkdir(parents=True,exist_ok=False)
    shutil.copy2(old_file,backup/old_file.name)
    shutil.copy2(root/'Config/CampaignLayout.json',backup/'CampaignLayout.json')
    previous_manifest=root/'Saved/FullJourneyManifest.json'
    if previous_manifest.exists():
        shutil.copy2(previous_manifest,backup/'PreviousManifest.json')
    unreal.log('CAMPAIGN_BACKUP ' + str(backup))
runpy.run_path(str(root/'Scripts/campaign_materials.py'))['ensure_blocker_materials']()
assert all(assets.does_asset_exist(s['source']) for s in layout['stages'])
assert sorted(c['item_id'] for s in layout['stages'] for c in s.get('collectibles', [])) == [0, 1, 2, 3]
assert unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
world.get_world_settings().set_editor_property('default_game_mode', unreal.MiningExhibitGameMode.static_class())
cube = unreal.load_asset('/Engine/BasicShapes/Cube')
sphere = unreal.load_asset('/Engine/BasicShapes/Sphere')
floor_mat = unreal.load_asset('/Game/Showcase/Materials/M_ContinuousFloor')
edge_mat = unreal.load_asset('/Game/Showcase/Materials/M_ContinuousEdge')
earth_mat = unreal.load_asset('/Game/CollectionPrototype/Materials/M_CollectionEarth')
route = json.loads((root / layout['mine_centerlines']).read_text())[0]
points = route['points']
arcs = [0.0]
for a, b in zip(points, points[1:]):
    arcs.append(arcs[-1] + math.dist(a, b))
blocks = [b for b in json.loads((root / layout['mine_blockers']).read_text())['objects'] if b['path_index'] == 0]
areas, placements, report = [], [], []

def v(values):
    return unreal.Vector(*values)

def tag_actor(actor, name, tag):
    actor.set_actor_label(name)
    actor.set_editor_property('tags', [tag])
    actor.set_folder_path('Campaign/' + str(tag))
    return actor

def box(name, position, size, tag, material=floor_mat, rotation=None):
    actor = actors.spawn_actor_from_class(unreal.StaticMeshActor, position, rotation or unreal.Rotator())
    tag_actor(actor, name, tag)
    actor.static_mesh_component.set_static_mesh(cube)
    actor.static_mesh_component.set_material(0, material)
    actor.set_actor_scale3d(v([n / 100 for n in size]))
    return actor

def label(text, position, rotation, tag, size=34):
    actor = actors.spawn_actor_from_class(unreal.TextRenderActor, position, rotation)
    tag_actor(actor, text, tag)
    actor.text_render.set_text(text)
    actor.text_render.set_world_size(size)
    actor.text_render.set_horizontal_alignment(unreal.HorizTextAligment.EHTA_CENTER)
    actor.text_render.set_text_render_color(unreal.Color(100, 240, 230, 255))

def lamp(position, tag, intensity=1500):
    actor = actors.spawn_actor_from_class(unreal.PointLight, position)
    tag_actor(actor, 'Campaign_PathLight', tag)
    actor.point_light_component.set_editor_property('use_inverse_squared_falloff', False)
    actor.point_light_component.set_editor_property('intensity_units', unreal.LightUnits.UNITLESS)
    actor.point_light_component.set_editor_property('light_falloff_exponent', 2.0)
    actor.point_light_component.set_editor_property('intensity', intensity / 3500.0 * .8)
    actor.point_light_component.set_editor_property('attenuation_radius', 1250.0)
    actor.point_light_component.set_editor_property('cast_shadows', False)

def single(group, cls):
    found = [a for a in group if isinstance(a, cls)]
    assert len(found) == 1, (str(cls), len(found))
    return found[0]

for index, spec in enumerate(layout['stages']):
    tag = 'BM_Campaign_' + spec['id']
    offset = v(spec['offset'])
    copied = list(unreal.MiningDesignerLibrary.copy_experience_actors(spec['source'], offset, tag))
    assert copied, spec['source']
    starts = [a for a in copied if isinstance(a, unreal.PlayerStart)]
    entry = starts[0].get_actor_transform() if starts else single(copied, unreal.PopupCloseStage).get_player_spawn_transform()
    for start in starts:
        actors.destroy_actor(start)
    if spec['kind'] in ('OFFICE', 'MUSEUM'):
        entry.translation = unreal.Vector(entry.translation.x, entry.translation.y, offset.z + 110)
    area = unreal.MiningExperienceArea()
    for name, value in [('title', spec['title']), ('instructions', spec['hint']), ('tag', tag), ('origin', offset), ('entry', entry), ('kind', getattr(unreal.MiningExperienceKind, spec['kind']))]:
        area.set_editor_property(name, value)
    row = dict(id=spec['id'], source=spec['source'], actors=len(copied)-len(starts), entry=[entry.translation.x,entry.translation.y,entry.translation.z], tag=tag)
    source_file = root / 'Content' / (spec['source'][6:] + '.umap')
    row['source_sha256'] = hashlib.sha256(source_file.read_bytes()).hexdigest()
    if spec['kind'] == 'MINING':
        region = single(copied, unreal.MiningExternalRegion)
        assert len(region.generated_guide_layers) == 20 and region.generated_rock and region.generated_shell
        cutter = region.cutters[0].local_transform
        def route_at(distance):
            j = max(1, min(len(points)-1, bisect.bisect_left(arcs, distance)))
            t = (distance-arcs[j-1])/(arcs[j]-arcs[j-1])
            p = [a+(b-a)*t for a,b in zip(points[j-1],points[j])]
            # Blender FBX (metres) -> UE (centimetres), shared source mesh axes.
            raw = unreal.Vector(p[0]*100, -p[1]*100, p[2]*100)
            return unreal.MathLibrary.transform_location(region.get_actor_transform(), unreal.MathLibrary.transform_location(cutter, raw))
        route_start, route_end = spec.get('start_arc_m',4), spec.get('end_arc_m',arcs[-1]-6)
        assert 0 < route_start < route_end < arcs[-1]
        start_center = route_at(route_start)
        start_forward = route_at(route_start+2)-start_center
        start_yaw = math.degrees(math.atan2(start_forward.y,start_forward.x))
        entry = unreal.Transform(location=start_center-unreal.Vector(0,0,90), rotation=unreal.Rotator(yaw=start_yaw))
        area.set_editor_property('entry', entry)
        row['entry'] = [entry.translation.x,entry.translation.y,entry.translation.z]
        box('Campaign_EntryLanding', start_center-unreal.Vector(0,0,220), (540,540,30), tag, floor_mat)
        lamp(start_center+unreal.Vector(0,0,180), tag, 2000)
        exit_pos = route_at(route_end)
        area.set_editor_property('map_route', [route_at(route_start + (route_end-route_start)*i/math.ceil((route_end-route_start)/2)) for i in range(math.ceil((route_end-route_start)/2)+1)])
        area.set_editor_property('exit', exit_pos)
        area.set_editor_property('exit_extent', unreal.Vector(340, 550, 650))
        row['exit'] = [exit_pos.x,exit_pos.y,exit_pos.z]
        row['route_length_cm'] = arcs[-1]*100
        row['playable_length_cm'] = (route_end-route_start)*100
        row['playable_arc_m'] = [route_start,route_end]
        # Bright guides follow the original winding descent, including blocked stretches.
        for step in range(math.ceil(route_start/12), math.floor(route_end/12)+1):
            pos = route_at(step*12)
            marker = actors.spawn_actor_from_class(unreal.StaticMeshActor, pos)
            tag_actor(marker, 'Campaign_DescentGuide_%02d' % step, tag)
            marker.static_mesh_component.set_static_mesh(sphere)
            marker.static_mesh_component.set_material(0, edge_mat)
            marker.static_mesh_component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
            marker.set_actor_scale3d(unreal.Vector(.18,.18,.18))
            if step % 2 == 0:
                lamp(pos, tag)
        forward = route_at(route_end+1)-exit_pos
        yaw = math.degrees(math.atan2(forward.y,forward.x))
        facing = unreal.Rotator(yaw=yaw+180)
        label('NEXT / ' + layout['stages'][index+1]['id'], exit_pos+unreal.Vector(0,0,240), facing, tag, 52)
        box('Campaign_ExitLanding', exit_pos-unreal.Vector(0,0,210), (700,800,30), tag, edge_mat)
        lamp(exit_pos, tag, 3500)
        for cache in spec.get('collectibles', []):
            n = cache['gap_after']
            distance = (blocks[n-1]['end_arc_m']+blocks[n]['start_arc_m'])/2
            assert route_start < distance < route_end, 'Collectible must be inside its playable mine section'
            center = route_at(distance)
            direction = route_at(distance+1)-center
            yaw = math.degrees(math.atan2(direction.y,direction.x))
            rot = unreal.Rotator(yaw=yaw)
            item_pos = center-unreal.Vector(0,0,100)
            placement = unreal.MiningCollectionPlacement()
            placement.set_editor_property('item_id', cache['item_id'])
            placement.set_editor_property('transform', unreal.Transform(location=item_pos, rotation=rot))
            placement.set_editor_property('area_tag', tag)
            placements.append(placement)
            box('Campaign_CacheBase_%d' % cache['item_id'], center-unreal.Vector(0,0,175), (340,340,30), tag)
            # A solid cover encloses the item; collection ray must hit excavated item first.
            rock = actors.spawn_actor_from_class(unreal.DiggableRock, item_pos, rot)
            tag_actor(rock, 'Campaign_CacheCover_%d' % cache['item_id'], tag)
            rock.set_editor_property('rock_size', unreal.Vector(240,240,220))
            rock.set_editor_property('rock_material', earth_mat)
            rock.reset_rock()
            label('CACHE / E', center+unreal.Vector(0,0,130), unreal.Rotator(yaw=yaw+180), tag, 28)
            lamp(center+unreal.Vector(0,0,230), tag, 2200)
            row.setdefault('collectibles', []).append(dict(item_id=cache['item_id'], arc_m=distance, position=[item_pos.x,item_pos.y,item_pos.z]))
    elif spec['kind'] == 'OFFICE':
        single(copied, unreal.PopupCloseStage).set_editor_property('area_managed', True)
    elif spec['kind'] == 'MUSEUM':
        museum = single(copied, unreal.WenSiPuzzle)
        museum.set_editor_property('area_managed', True)
        museum.set_editor_property('area_player_start', entry)
        assert museum.get_editor_property('circuit_puzzle')
    elif spec['kind'] == 'ENDING':
        ending = single(copied, unreal.MiningCellUniverse)
        ending.set_editor_property('area_managed', True)
        assert len(ending.gates) == 11 and all(ending.gates)
    elif spec['kind'] == 'JOURNEY':
        # Existing escape uses authored world positions; keep this source at the origin.
        assert spec['offset'] == [0,0,0]
        echo = single(copied, unreal.EchoPassageStage)
        echo.set_editor_property('room_in_same_level', True)
        echo.set_editor_property('destination_level', None)
        escapes = [a for a in copied if isinstance(a, unreal.MiningRoomEscape)]
        escape = escapes[0] if escapes else actors.spawn_actor_from_class(unreal.MiningRoomEscape, unreal.Vector())
        if not escapes:
            tag_actor(escape, 'Campaign_JourneyEscape', tag)
        escape.set_editor_property('area_managed', True)
        escape.set_editor_property('card_stage', single(copied, unreal.CardRoomStage))
        escape.set_editor_property('entrance', single(copied, unreal.MiningGiantRoomEntrance))
    areas.append(area)
    report.append(row)
    unreal.log('CAMPAIGN_STAGE ' + json.dumps(row,ensure_ascii=False))

collection = actors.spawn_actor_from_class(unreal.MiningCollectionField, unreal.Vector(0,-200000,0))
collection.set_actor_label('CAMPAIGN_GlobalCollection_2_1_1')
collection.set_editor_property('area_managed', True)
collection.set_editor_property('shared_abilities', True)
collection.set_editor_property('show_exploration_hud', False)
collection.set_editor_property('use_authored_placements', True)
collection.set_editor_property('authored_placements', placements)
key = unreal.Key()
key.set_editor_property('key_name', 'I')
collection.set_editor_property('catalogue_key', key)
hall = actors.spawn_actor_from_class(unreal.MiningExhibitHall, areas[0].entry.translation)
hall.set_actor_label('CAMPAIGN_LinearFlow_EditAreasHere')
hall.set_editor_property('linear_campaign', True)
hall.set_editor_property('areas', areas)
hall.set_editor_property('lobby_entry', areas[0].entry)
actors.spawn_actor_from_class(unreal.PlayerStart, areas[0].entry.translation, areas[0].entry.rotation.rotator())
assert unreal.EditorLoadingAndSavingUtils.save_map(world,destination)
manifest = root/'Saved/FullJourneyManifest.json'
manifest.write_text(json.dumps(dict(map=destination,stages=report,recipe=layout),ensure_ascii=False,indent=2),encoding='utf-8')
unreal.log('CAMPAIGN_SAVED ' + destination)
unreal.SystemLibrary.quit_editor()
