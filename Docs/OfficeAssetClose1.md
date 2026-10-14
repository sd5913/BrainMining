# Close1 imported Office architecture

`/Game/PopupClose/Maps/Lvl_PopupClose1` reuses meshes and materials from
`/Game/Office/Maps/Officeasset`. The reference map and its layout are unchanged.

The added actors are editable under `PopupOffice/OfficeAsset/Architecture`:
eight double windows with wood frames and blinds, infill/end walls preserving
both doorways, modular carpet and suspended ceilings, fluorescent fixtures and
vents. Existing authored pillars receive `Mi_Wall_01a` without changing poses.
The structural floor and room collision remain in the original stage actor.

`PopupCloseOffice.bUseImportedArchitecture` suppresses the old procedural
window, floor and ceiling details in both construction and BeginPlay. It is
enabled only on Close1, so other maps retain their own architecture.

The two existing `PopupOfficeDevice` computer actors use
`SM_Computer_02a_CRT_CustomScreen`. The central mesh pose was transferred from
the user's placed CRT, removing the overlapping old monitor/duplicate. Each
`DesktopSurface` is fitted to the CRT aperture and serves as the interaction
proxy. Its render target is displayed by `M_CRTScreen` on the model's original
curved glass, avoiding a flat rectangle penetrating the bezel. The screen camera
calculates its FOV from the actual widget dimensions. Native popup replies, accounting,
timers, warnings and printers are retained. The six decorative CRTs retain
`OfficePopupEmitter` and additionally use `OfficeCRTPopupEmitter` to move the
physical popup emission point to their glass face.

The imported glass material had an absent Megascans roughness dependency.
Close1 uses a repaired copy under `/Game/PopupClose/OfficeArt/Architecture`,
keeping the reference material separate.
The window-frame mesh is also duplicated with the repaired glass in its default
slot: an actor-level material override alone would still retain the absent
texture as a mesh dependency. Screen mouse traces hit the curved glass and map
its local X/Z coordinates back to desktop UVs; validation uses that same trace.

Apply script: `Scripts/dress_close1_from_officeasset.py` through editor Python.
The script operates only on Close1, preserves existing furniture and authored
props, and updates its own architecture actors in place. Backups, original
pose, geometry measurements and validation captures are in
`Saved/OfficeAsset` (local, unversioned).
