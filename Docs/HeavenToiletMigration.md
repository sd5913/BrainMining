The existing `/Game/CollectionPrototype/HeavenToilet/Maps/Lvl_HeavenToilet`
contains the adapted HeavenPreview sky, edge clouds and proximity toilet at
the authored three-step dais, centered at (2580,0). The platform's top remains
90cm; the original entrance, pipe route, hollow collider and 48cm water remain.

The collection placement optionally references an authored `PresentationActor`.
Its original collectible keeps the inventory ID and interaction target; hits
on the authored model resolve through its owner. Collecting removes the model,
lid and spray together. Placements without a presentation keep prior behavior.
The catalogue uses the same toilet model for this placement.

The supplied combined pipe model is split with welded vertices and small bolts
grouped with their nearest pipe body. The selected straight/curved assemblies
keep their UVs and original metal materials. Long spans tile the straight model,
and curved ports trim the adjacent spans at the six original route corners.
Original pipe actors and supplied demo submeshes are hidden, retained in the map.
Meshes live under `/Game/HeavenPreview/Meshes/Pipes`; the separated Blender source
and all exported variants remain under `Saved/HeavenPreview/Pipes`.

`NS_HeavenEntranceFlow` adapts the existing pack-derived water droplets: negative
X velocities, small lateral spread, bounded sprite sizes, -980cm/s² gravity.
The existing soil connectivity check activates it only after a low connected
breach. Higher tunnels and blind cuts do not trigger it. Sealing the entrance
stops emission and lets remaining droplets finish. The connected water sheet
and basin remain in place; this is a visual flow, not a fluid simulation.

Use `Scripts/install_heaven_pipes_arttest.py` for route fitting and collection
binding, `Scripts/tune_heaven_entrance_arttest.py` for Niagara settings, and
`Scripts/verify_heaven_migration_arttest.py` for real E interaction and water
state checks. Reports and rendered captures are in `Saved/HeavenPreview`.
