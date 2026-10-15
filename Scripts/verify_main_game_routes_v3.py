"""Run the established physical route audit against the regional V3 sources."""
import unreal,json
from pathlib import Path
ROOT=Path(unreal.Paths.project_dir()).resolve();code=(ROOT/'Scripts/verify_main_game_routes_v2.py').read_text(encoding='utf-8')
code=code.replace("OUT=ROOT/'Saved/Validation/MainGameRoutesV2';ART=ROOT/'Art/MainGame/RoutesV2'","OUT=ROOT/'Saved/Validation/MainGameRoutesV3';ART=ROOT/'Art/MainGame/RoutesV3'")
code=code.replace("['areas'][i]","['areas'][i*2]")
a=code.index('            delta=V(ROOMS');b=code.index('            check(len(r.generated_guide_layers)',a)
code=code[:a]+'''            rise=unreal.Vector(0,0,10000 if i>=2 else 0)
            check(area.map_route[0].distance(V(before['route_start'])+rise)<.01,stage+' fixed inlet mouth follows the current source-level placement')
            expected_exit=V(before['exit'])+unreal.Vector(0,0,10000 if i>=1 else 0)
            expected_route_end=expected_exit if i<3 else V(before['route_end'])+rise
            check(area.exit.distance(expected_exit)<.01 and area.map_route[-1].distance(expected_route_end)<.01,stage+' outlet and map centerline retain their intended connection or final-exit offsets')
            check(r.rock_hardness==i and r.guide_hardness==i+1 and r.generated_rock.hardness_level==i and all(g.hardness_level==i+1 for g in r.generated_guide_layers),stage+' normal and soil hardness survive runtime initialization')
'''+code[b:]
code=code.replace("state.update(phase='region',next=now+.2)","state['hall'].set_actor_tick_enabled(False)\n            escape=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.MiningRoomEscape)[0]\n            check(abs(escape.get_editor_property('ComputerEntry').z-16700)<.1,'Translated TV chamber is created at its new source level height')\n            check(abs(escape.get_editor_property('WaterStartZ')-10080)<.1,'Translated flood starts at the matching room height')\n            state.update(phase='region',next=now+.2)",1)
code=code.replace("levels.editor_request_end_play()","perf.set_editor_property('bThrottleCPUWhenNotForeground',old_throttle);levels.editor_request_end_play()")
code=code.replace("            move(photo_floor[5]+unreal.Vector(0,0,170),target)","            hall=state['hall'];hall.set_editor_property('linear_campaign',False)\n            try:check(hall.enter_area(i*2),'Activate the actual mining area for its player lighting')\n            finally:hall.set_editor_property('linear_campaign',True)\n            move(photo_floor[5]+unreal.Vector(0,0,170),target)")
prefix="perf=unreal.get_default_object(unreal.load_class(None,'/Script/UnrealEd.EditorPerformanceSettings'));old_throttle=perf.get_editor_property('bThrottleCPUWhenNotForeground');perf.set_editor_property('bThrottleCPUWhenNotForeground',False)\n"
exec(compile(prefix+code,'regional_route_runtime_audit','exec'),{'__name__':'__main__','unreal':unreal})
