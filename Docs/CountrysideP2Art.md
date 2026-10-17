# Countryside P2 scene art

Map: `/Game/Passages/Echo/Maps/Lvl_EchoMazeToCards_P2`.

The P2 study uses `Countryside_Interior` furniture, bound books, ceramic objects,
carpets, plants, wooden window frames and pendant fixtures. The room keeps its
giant scale. Wood and fabric texture detail, teal enamel and warm practical lights
are balanced with cooler window light.

All 837 original actors retain their location, rotation and scale. Original
blocking meshes remain as collision geometry; their primitive-component visibility
is disabled where new art covers them. New decorative static meshes have no
collision. Entry excavation, spiral ramps, table height, card landing, reward and
opponent flight anchors are retained. P1's map is unchanged.

Both the floor plush and animated opponent use smooth authored FBX parts with
sewn details, padded ears, fabric normals, a teal bow and ginger/cream materials.
The opponent's head aim, breathing, eyes, paw gestures and flight use the original
animation logic. The `CountrysideP2Art` actor tag enables its material/model skin;
untagged encounters retain their original presentation.

P2 card effects use seven duplicated Niagara systems in
`/Game/Passages/Echo/ArtP2/VFX`, with 42 source-pack dependencies imported without
overwriting existing pack assets. Played card types drive the presentation:
laser uses a narrow cyan beam and electric impact; poison lobs eight green spores
into a lingering stun cloud; fire sends orange embers alongside the laser and
ignites the poison left by the previous hand. Seventeen cards each emit their own
laser, spore or ember before merging into a three-colour comet and oversized final
impact. Looping source systems are explicitly stopped between casts; only poison
persists until ignition. Damage and round timing remain in the original card rules.
Inactive Niagara actors under `FXAssetBank`
retain explicit map references for cooking.

## Reproduction and review

- `Scripts/dress_countryside_p2.py`: initial art pass, run on the open P2 map.
- `Scripts/build_countryside_cat_meshes.py`: Blender mesh authoring source.
- `Scripts/refine_countryside_p2.py`: enamel, effect materials and editor preview.
- `Scripts/polish_p2_energy_materials.py`: magic emission matched to the scene exposure.
- `Scripts/finish_countryside_floor_cat.py`: pictured floor plush and local exposure.
- `Scripts/prepare_countryside_vfx.py`: selective Niagara dependency import.
- `Scripts/capture_countryside_p2.py`: anchor validation and art screenshots.
- `Scripts/verify_p2_runtime.py`: PIE card-flow verification.

The editor-only command `CardRoom.P2AutoPlay` exercises actual matching,
card-staging and confirmation APIs in a P2 PIE world. It writes four-round results
and charge, flight and impact screenshots into `Saved/Automation/CountrysideP2`.
`skill_vfx_validation.json` checks the four distinct active system sets and detects
systems leaking from an earlier cast.
The verification world uses 0.35 time dilation to capture the short charge and
flight phases; game defaults are unchanged. The P2 stage prepares Niagara systems
before interaction, including waiting for editor shader compilation, so a first
cast cannot be lost while the source system compiles.

Original map backup: `Saved/ArtBackups/CountrysideP2_20261015`.
Anchor results: `Saved/Automation/CountrysideP2/anchor_validation.json`.
Runtime results: `Saved/Automation/CountrysideP2/runtime_validation.json` and
`runtime-vfx.csv`. Check the recorded result rather than assuming a successful run.

The module was built through the ASCII junction `E:/BrainMining_CodexBuild` to
avoid MSVC's handling of the project's Chinese path. Editor SARIF diagnostics are
disabled in the editor target because their output path also failed compilation.
