# Museum exhibit refresh

Imported under `/Game/WenSi/Museum/Refresh`. The actual campaign room is the
`Lvl_WenSi_MirrorMuseum1` sublevel loaded by MainGame.

- **Balloon** — plaggy, [CC0 - Balloon on Fab](https://www.fab.com/listings/44753a13-5c8b-4e16-a50b-3ada3adb6a4b). Fab distributes this listing under [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/); credit the author despite the CC0 wording in its title. Model and PBR maps retained; display size adjusted and an independent animated tether retained.
- **Concrete Cat Statue** — [Poly Haven](https://polyhaven.com/a/concrete_cat_statue), CC0. Used for the small ornament. Scaled to the original gameplay envelope.
- **Horse Statue 01** — [Poly Haven](https://polyhaven.com/a/horse_statue_01), CC0. Replaces the balance weight, with the in-game porcelain material and noun “雕塑”.
- **Rock 09** — [Poly Haven](https://polyhaven.com/a/rock_09), CC0. Fitted to the original stone puzzle envelope; original colliders retained.
- **Metal 032** — [ambientCG](https://ambientcg.com/view?id=Metal032), CC0. 2K PBR maps applied to the two existing functional metal covers.
- **Porcelain** — authored from the project's existing `M_MuseumSurface` parent, using a white tint, low roughness and zero metallic. Used by the sphere and horse; no additional external porcelain asset is required.
- **Niagara Examples Pack** — [Epic Games on Fab](https://www.fab.com/listings/0e188eca-4e54-4fb2-a9ed-d8b8a565e600), Fab Standard License. Acquired with the user's explicit Fab EULA acceptance. UE 5.7 content imported into UE 5.8. `NS_Smoke_Plume` copied to `NS_MuseumSmoke`, shortened for the indoor exhibit, with outdoor wind disabled. Its 15 required original dependencies remain in `/Game/NiagaraExamples`; unrelated examples are archived locally in `Saved/Validation/MuseumAssetRefresh/UnusedNiagaraExamples`.

Poly Haven source file URLs, sizes and MD5 values are recorded in `downloads.json`.
ORM textures were packed from the source AO/roughness/metallic maps; normal maps
are imported as DirectX normal maps with sRGB disabled.

Existing exhibit proxy components continue to control thinking, word swapping,
balance movement and passage blocking. The decorative bronze shark uses its
own triangle collision so the removed empty plinth leaves no invisible box.

## Museum follow-up

- **Bronze Shark Statue** — Tina, [Poly Haven](https://polyhaven.com/a/bronze_shark_statue), CC0. Full scanned statue with its own base replaces the empty first-room plinth. 2K diffuse, DirectX normal and packed ARM maps.
- **Folding Screen (Imperial China Palace and Garden)** — 3D Assets, [asset page](https://3dassets.dev/assets/imperial-china-palace-and-garden-folding-screen-5a8651b3), CC0 1.0. Contributor reports AI-assisted authorship. Imported as a static four-panel screen; recolored using project material instances, fitted to the original screen selection/collision proxy.
- **Hanging balance scale, 1.0 m (Blacksmith Forge and Armoury)** — 3D Assets, [asset page](https://3dassets.dev/assets/blacksmith-forge-and-armoury-blacksmith-forge-and-armo-c279a8b3), CC0 1.0. Contributor reports AI-assisted authorship. Beam, hinge and pan meshes extracted and adapted to the existing balance. Original scale column, suspension and counterweight remain; the imported beam and pans attach to the original gameplay-driven actors. The original rigid tilt clip is not used because it rocks the pans with the beam rather than keeping them level.
- **Display plinths** — reused the Museum pack's existing `SM_PillarStand02`; no additional third-party pack.
- **Conductive apparatus** — matte enamel instance of the project's `M_MuseumSurface`, metallic=0. Conductive block uses Metal032, with a project-authored beveled block mesh.

Direct download URLs and hashes for follow-up sources are in
`downloads_followup.json`. Preparation/import/application scripts are under
`Scripts/*museum*followup.py` and `Scripts/export_museum_balance.py`.
