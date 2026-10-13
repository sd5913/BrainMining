# Small heaven preview

Map: `/Game/HeavenPreview/Maps/Lvl_HeavenPreview`.

The playable area is approximately 12 × 12 meters. A continuous sky material
samples the camera ray direction rather than the wall UVs, so the enclosing
cube's corners do not create visible background seams and the distant clouds
have no nearby-wall parallax. Slow procedural clouds, a low cloud deck and soft
cloud masses obscure the floor/wall transition and the physical boundaries.

The center is deliberately sparse: white stepped altar, thin golden halo,
existing Fab toilet model, and realistic Niagara water adapted from the specified
`Realistic_Starter_VFX_Pack_Niagara` pack. Soft warm lighting
and fixed exposure retain cloud detail while framing the ordinary toilet as a
solemn centerpiece. The sky and all new art live under `/Game/HeavenPreview`.

This is an independent visual preview with the engine's flying DefaultPawn.
Play in the editor and use WASD / mouse to inspect the illusion. No campaign
entry, existing character, office map or project startup map is changed.

When transferring the effect to a room in a shared map, keep the sky shell and
cloud layers local to that room, use an inward-facing shell, and make the post
process volume bounded. Adjust its blend radius at the entrance so the bright
exposure transitions smoothly from the surrounding level. Keep the toilet and
its existing gameplay actor separate from the environment art.

Generation: `Scripts/prepare_heaven_preview_geometry.py`, then
`Scripts/create_heaven_preview.py` in a clean dedicated Unreal editor. The
creation script refuses to overwrite an existing preview map or unsaved work.
Local validation files are stored in `Saved/HeavenPreview`.

The `HeavenToilet` actor starts with a closed lid. Coming within 220 cm of its
outlet opens the lid over 0.7 seconds; water starts after it is mostly open.
Leaving beyond 280 cm stops emission and closes the lid over one second.
This distance hysteresis avoids flicker at the activation boundary.

The original Fab toilet is untouched. Separate body/lid meshes preserve its UVs
and material, with the lid origin moved to the real hinge. The body uses its
triangle surface for collision so water is not blocked by a convex hull spanning
the empty bowl. The outlet is inside the rim at local `(0,60,260)` cm.

`NS_HeavenToiletWater` adapts the pack's Leak 2 materials, water sprite animation,
normal/refraction detail, droplet collision and event-driven splashes. Its stream
and droplets now start with upward velocities (roughly 350–470 cm/s) and evolve
under `-980 cm/s²` gravity. The spray has a small randomized lateral spread and
the stream has collision. `NS_HeavenToiletOpeningSplash` adapts Water Explosion,
removes the dirt emitters, limits the upward burst and runs once per approach.
The pack's source systems remain unchanged.

The leak's original strip sprites (up to 200 cm long) are replaced with compact
4–6 cm droplets with centered pivots and bounded scale. Even independently
randomized width/height cannot exceed a 1.5:1 aspect ratio. The main animated
water sprites are 8–14 cm; the opening burst uses 4–18 cm sprites rather than
the original 250 cm sheets. Visible flow uses the pack's water material.
CPU dummy renderers are hidden while preserving their collision/death events.

Import/reference preparation: `Scripts/import_heaven_water_assets.py`.
Mesh preparation: export the Fab mesh to `Saved/HeavenPreview/toilet_source.fbx`,
inspect it with the local Blender helper, then run
`Scripts/prepare_heaven_toilet_parts.py` and `Scripts/import_heaven_toilet_parts.py`.
Install in the preview with `Scripts/install_heaven_toilet.py`.
`Scripts/verify_heaven_toilet.py` checks its full proximity lifecycle in PIE.
`Scripts/tune_heaven_water.py` reapplies the documented simulation settings via
native Niagara MCP tools; save both preview Niagara systems after they compile.
