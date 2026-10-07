import runpy
from pathlib import Path

import unreal


world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
generator = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningTunnelGenerator)[0]
spaces = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningLayoutSpace)
assert len(spaces) == 3
assert all(space.get_editor_property("preview").get_collision_enabled() == unreal.CollisionEnabled.NO_COLLISION for space in spaces)
assert all(space.get_editor_property("preview").get_editor_property("hidden_in_game") for space in spaces)
targets = [
    (unreal.Vector(500, 0, 0), None),
    (unreal.Vector(4500, 0, 0), None),
    (unreal.Vector(4500, 2400, 0), None),
    (unreal.Vector(4500, 0, 0), None),
    (unreal.Vector(6800, 0, 0), None),
    (unreal.Vector(6800, -1900, -100), None),
    (unreal.Vector(6800, 0, 0), None),
    (unreal.Vector(8500, 0, 0), None),
    (unreal.Vector(9000, 0, 0), 1),
]
for route in list(generator.routes)[1:]:
    targets.extend((route.path.get_location_at_spline_point(index, unreal.SplineCoordinateSpace.WORLD), None) for index in range(2))
    targets.append((route.to_room.get_actor_location(), route.to_room.connection_order))
runpy.run_path(str(Path(__file__).with_name("verify_direct_faces_runtime.py")), init_globals={"targets_override": targets, "space_checkpoints": {2: 0, 5: 0}, "runtime_output": "space_traversal.json"})
