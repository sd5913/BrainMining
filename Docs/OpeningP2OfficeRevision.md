# Opening, office and P2 revision

The MainGame persistent map owns `OPENING / Bed to doorway` (`AMiningOpening`).
Its editable world anchors define the seated eye, standing capsule position and
door title placement. The supplied transparent title is imported as
`/Game/StartRoom/UI/T_OpeningTitle`. Start and Quit are native UMG buttons. Start
locks input during a 3.2-second rise, carries the same title into the doorway,
then restores the pawn camera, collision, look, movement and mining HUD. The
source StartRoom map is not rebuilt.

Close1 had two devices with index 0. `PopupOfficeDevice_4` was the duplicate.
The original `PopupOfficeDevice_0` remains interactive. `GatherDevices` now
preserves authored device transforms; only missing devices use fallback spawn
positions. Other desks and both printers retain their existing roles.

`AMiningEncounterGameMode::StartPlay` recognizes both study maps. P1 and P2 use
the same `AMiningRoomEscape`, delegates, water timings, reward trigger, floating
route, physical television screen, 14 tunnel sections, checkpoints and retries.
The P1 map is not rewritten. P2 now contains an authored `AMiningRoomEscape`
named `P1 Television / Flood + Dig Escape`, linked to its card stage and giant
room entrance. The cabinet, rounded bezel, knobs, aerials and shelf use the
same `BuildTelevisionModel` implementation in editor preview and gameplay.
Preview components are tagged and removed before BeginPlay; the original
diggable screen and tunnel are built once by the shared escape implementation.
This makes the television visible in the editor while preserving P1 gameplay.
The P2 regression checks persistence after map load and excludes duplicate
runtime escape actors or editor preview geometry.

P2 uses four Niagara systems under `ArtP2/StyledVFX`. They derive
from the supplied realistic starter pack, with cyan energy and warm fire,
shorter lifetimes, controlled particle counts and disabled metal sparks,
smoke/debris/ground projection emitters. The drilling-like Spark system is no
longer assigned or played. Poison separately references the original
`ArtP2/VFX/NS_P2_Poison`; its asset, emitters and parameters are not retuned.
The original source available at the start of this revision lacked the earlier
P2 runtime integration, so rebuilding had exposed legacy sprite gas. The new
integration reconnects the existing Niagara resource, keeps it across the deal
transition and stops it when the rules ignite poison or restart the encounter.
`poison_sha256_before.txt` and `presentation_runtime.json` record asset integrity.
Smooth energy
cores replace faceted orbs, and a narrow pulse replaces the crowded sprite beam.
Seventeen trajectories converge for the last hand. Soft expanding impact rings,
brief local lighting and longer fire/energy lifetimes make hits readable at the
giant room's scale. Cast components stop between hands and after victory.
Persistent mesh/material maps on the stage reapply P2's authored plush art after
loading and BeginPlay; native component overrides alone had reverted on reload.

Reproduction scripts:

- `audit_intro_p2_office.py`: save and back up maps, collect initial actor audit.
- `prepare_opening_style_assets.py`: import title, duplicate FX, create energy
  material and remove the known duplicate office actor.
- `inspect_styled_niagara.py` and `tune_styled_niagara.py`: inspect and edit the
  copied Niagara systems through the local UEMCP endpoint.
- `install_opening_p2.py`: assign P2 art properties and author MainGame anchors.
- `install_p2_television.py`: save current editing work, back up P2, and author
  only the shared television escape actor and its two gameplay references.
- `review_p2_television.py`: inspect the live CRT and cut its screen through
  the player's actual `MiningComponent.TryMine` trace, recording the result.
- `refine_p2_impact_materials.py`: after initial preparation, build the bright
  soft energy and irregular soft ring materials used by the current art pass.

Verification targets:

- `BrainMining.Opening.BedToDoor`
- `BrainMining.Office.PlacedCentralComputer`
- `BrainMining.Encounters.PIE.CardEscapeCheckpoints`
- `BrainMining.Encounters.PIE.P2CardEscapeCheckpoints`
- `BrainMining.ReviewP2` in a P2 PIE session captures the actual game viewport
  at charge, flight, impact and aftermath phases. This editor review alone uses
  half speed for reliable frame capture and restores normal speed afterward.
- `validate_p2_presentation.py`: records active systems, material provenance and
  poison asset hash while running that review.

Evidence and rendered frames are written to `Saved/Validation/IntroP2Office`.
The final television build succeeded, and `tv_tests_final.json` records both
P1 and P2 escape regressions passing with no warnings or errors. The authored
P2 actor reloads with 63 preview components (including the original local
television lamp). `tv_real_mining.json` additionally records a successful
player mining trace against the live screen, with one escape actor and 15
diggable sections. The review temporarily extends its own PIE timers for
photographs; these values are never saved to the map.
The pre-edit maps are in `Saved/Backups/IntroP2Office_20261016_0401*` (use the
actual timestamped folder). Build through `E:/BrainMining_CodexBuild`, the existing
junction to this project. Editor SARIF output is disabled for MSVC's non-ASCII
path issue. An existing catalogue widget's missing `CatalogueArtwork` material
array was restored to allow the project to compile.
