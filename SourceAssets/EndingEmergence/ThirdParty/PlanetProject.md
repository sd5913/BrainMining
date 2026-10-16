# Planet Project — ending integration

- Fab: https://www.fab.com/listings/fc591129-a147-4684-acbb-38a990e30c3a
- Product: [FREE] Planet Project for Unreal Engine 5
- Publisher: Arghanion's Puzzlebox
- Acquired and added to the project by the user, 2026-10-15.
- User confirmed eligibility for the free Personal tier. The Fab Standard License applies; this is not a public-domain asset pack.
- Source assets remain unchanged under `/Game/Planet_Project`.

The integration uses `T_8k_earth_daymap`, `T_8k_earth_nightmap`, and
`T_8k_earth_clouds` through ending-only materials, plus `SM_Earth_Planet`
for the planet and atmosphere silhouettes at the original cinematic size. The product description
attributes Earth/Moon texture sources to NASA; the creator's packaged project
and materials remain subject to the product license.

`Scripts/apply_ending_planet_project.py` adapts these textures to the existing
orbital camera only. The continental night view was restored to its original
procedural materials on 2026-10-15 at the user's request. It preserves the gameplay route and other
shots, removes the old procedural Earth light instances, and backs up modified
ending assets to `Saved/Backups/EndingPlanet-*`. It does not use the pack's
orbit/gravity Blueprint or its volumetric-cloud system.

Applied on 2026-10-15 after disk space was freed. The existing map remains the
only ending demo; no new playable map is created. A full procedural rebuild
via `create_ending_emergence.py` reapplies this adaptation automatically when
the source pack is present. After using the actor's manual Build Demo button,
rerun the adaptation script in a clean editor to restore the Earth-only
atmosphere assignment and remove placeholder light instances.

Validation: `BrainMining.EndingEmergence.PlayableRouteAndFilm` passed on
2026-10-15 at 17:04 HKT (1 success, 0 failures). Report:
`Saved/Validation/EndingPlanetProject/index.json`. Cloud-deck and Earth
screenshots were visually checked, including the longitude orientation.

The subsequent horizon revision keeps the same Earth imagery and smooth mesh,
thins the atmosphere shell, and changes the camera to an oblique climb looking
along the limb. The adapter no longer overwrites NightContinent or CloudDeck.
