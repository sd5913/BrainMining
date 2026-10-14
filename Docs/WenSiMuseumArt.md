# Museum copies

Art dressing is saved in `Lvl_WenSi_MirrorMuseum1` and
`Lvl_WenSi_ScreenBridge1`, under `/Game/WenSi/Maps`. The original maps are
unchanged. Imported `/Game/Museum` meshes and materials supply the classical
columns, panel mouldings, parquet, ceiling, paintings, statues,
plinths, windows and benches. Original Museum assets are not edited.

Warm exhibit lighting combines with soft neutral gallery fill and passage light.
The ceiling uses room-sized modular panels; no oversized or
flattened arches project down into the rooms. Column height and floor/ceiling
coverage follow the existing room dimensions.
Fixed exposure, modest vignette and low atmospheric fog retain visibility of
the puzzles while keeping the edges of the rooms subdued. The second Screen
Bridge gallery keeps its lower ceiling and all existing circuit mechanisms.

New actors live in `MysteryMuseum/Architecture`, `Exhibitions` and `Lighting`
and carry `MysteryMuseumArt`. New decoration uses no collision. Old blockout
decoration is hidden through its mesh components, preserving its original
collision and making the old visual reversible. The gameplay actors, original
transforms and collider profiles were compared before and after dressing.
Puzzle objects, lifts, hopper, reward shutter, reset buttons, area routing,
game modes and camera/pawn logic are retained.

Native WenSi objects now expose an optional `ArtModel`, `ArtRotation` and
`bUniformArtScale`, plus the editor's `RefreshArtModel` button. The copied maps
use `/Game/Fab/Stone/stone1` for the rock and pressure block, Museum `SM_Hebe`
for the statue and `SM_Philosopher` for the small mirror-gallery exhibit.
Models keep their original materials and fit the existing gameplay envelope;
uniform sculptures stand on the envelope's bottom rather than hovering.
The old proxy parts keep collision/visibility flags for existing rule and
contact checks, but stop rendering into color, depth, shadows and ray tracing.
`ArtVisual` itself has no collision. It follows its actor through floating,
big/small changes and resets, and receives the existing highlight/ghost state.
Unconfigured actors retain the original presentation, including original maps.

The reproduction script `Scripts/dress_wensi_museum_copies.py` only accepts the
two suffixed maps and refuses PIE or unsaved packages. It removes only actors
with its own tag before regenerating the dressing. Backups of both originals
and both user-made copies are in `Saved/MuseumArt/Before_20261014_015537`.

PIE checks: Mirror Museum passes 35 mechanism checks; Screen Bridge passes
45 circuit/progression/contact/input checks. Both thought widgets were tested
with actual Slate word dragging. Game-rendered captures and validation reports
are under `Saved/MuseumArt`; the verification script exercises real E/G inputs
and captures the thought flight at several times. The whole card recedes,
turns and flies to its side together in 0.42 seconds. Arrival
is fixed in screen space with no idle float or overshoot.
