# Close1 office art

Edited level: `/Game/PopupClose/Maps/Lvl_PopupClose1`. Other office maps retain their default layout until their **Office Art** options are enabled.

The outliner folder `PopupOffice/OfficeArt` contains individually editable furniture and desktop props. Source meshes and packed PBR maps were extracted from the supplied `office-assets` FBX files, mainly table1, table2, table3 and printer1. Original source files and user-placed clock / notebook / chair actors are retained.

`PopupCloseOffice` exposes surface overrides, carpet, ceiling tiles and the option to replace its perimeter blockout. `PopupCloseStage` exposes indoor light fill and central desk replacement. **Refresh Office Art** applies the configuration in the editor; the same configuration is applied at BeginPlay, so saved art settings survive a restart.

Both existing computer / printer actors retain their device indices, interaction targets, timers, popup bookkeeping and rewards. Imported visual shells use **Replacement Model** and **Replacement Transform**. Computer screen widgets remain live; their aim and camera follow the screen component height. Computer shells ignore visibility rays to keep UI clicking unobstructed.

Printer output was measured from printer1's upper side tray: original horizontal surface height 0.948 m, model scale 1.12. In device-local UE coordinates the paper stack center is `(6, -36, 106.45)` cm, output direction `(0, -1, 0)`, paper yaw 90 degrees. **Paper Output Position**, **Paper Output Direction** and **Paper Output Yaw** can be adjusted on each printer actor. Both ordinary output and the office-wide eruption read these overrides. The six perimeter monitors tagged `OfficePopupEmitter` supply the decorative eruption positions.

Popup artwork lives under `/Game/PopupClose/PopupArt`; source images are retained in `Art/Office/Popups`. UI backgrounds only trim transparent padding. Physical copies bake the previous message onto the first three blank skins, keeping the pile readable without hundreds of extra widgets. Message / Loading / Robot keep their supplied wording. Click areas follow OK, Cancel on Loading, and the Robot checkbox. The desktop, airborne physics slabs and instanced floor piles all use the same six skins. Round-robin skin selection balances their counts without changing the physics / recycling budgets.

Packaging explicitly includes the popup art folder because native code loads these skins by path. Source preparation / importing / dressing scripts are in `Scripts/*office*.py`. These import only task-owned assets; they do not save unrelated user assets or replace the existing gameplay actors.

Backups and runtime checks are in `Saved/OfficeArt`. The textured office temporarily enables temporal AA during its active game session, then restores the previous method on leaving / EndPlay; the mining project's default renderer remains unchanged.

Pending-device warnings use the supplied red exclamation texture `PopupArt/T_PendingWarning`. They are screen-space billboards: always face the camera, remain readable without scene lighting, and clamp to the viewport when approached. The currently focused computer places its marker near the upper right instead of above the camera. Pending counts, warning removal, rewards and locking still follow the existing bookkeeping.

The nine ceiling lights are saved native subobjects. The office Actor exposes Interior Light Lumens, Corner Light Lumens and Interior Light Temperature; these are reapplied in its art mode at BeginPlay, preventing a map reload from reverting to the old unitless lighting. Default art values are 1500 / 800 lumens and 4700 K. During computer tasks the pending markers use separate rows on the right.
